#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <dos/filehandler.h>
#include <exec/errors.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <proto/exec.h>

#include "deviceio2.h"
#include "deviceio2_protos.h"

#include "debug.h"
#include "support_protos.h"

#include <devices/newstyle.h>      /* Doesn't include exec/types.h which is why it is placed here */

extern struct DeviceNode *devnode;
extern struct IOStdReq *ioreq2;
extern struct FileSysStartupMsg *startupmsg;
extern ULONG mask_mask;             /* mask as specified by mountlist */

extern void starttimeout(void);
extern LONG request(UBYTE *,UBYTE *,UBYTE *,APTR, ... );
extern void dreq(UBYTE *fmt, ... );

extern LONG req(UBYTE *fmt, UBYTE *gads, ... );
extern LONG req_unusual(UBYTE *fmt, ... );

extern ULONG __asm MULU64(register __d0 ULONG,register __d1 ULONG,register __a0 ULONG *);

extern LONG getbuffer(UBYTE **tempbuffer, ULONG *maxblocks);

struct IOStdReq *ioreq=0;
struct MsgPort *msgport=0;
BYTE deviceopened=FALSE;
BYTE newstyledevice=FALSE;
BYTE does64bit=FALSE;
BYTE scsidirect=FALSE;

ULONG sectors_total;              /* size of the partition in sectors */
UWORD sectors_block;              /* number of sectors in a block */

UWORD shifts_block;               /* shift count needed to convert a blockoffset<->byteoffset */

UWORD cmdread=CMD_READ;
UWORD cmdwrite=CMD_WRITE;

ULONG sector_low;                 /* sector offset of our partition, needed for SCSI direct */
ULONG sector_high;                /* last sector plus one of our partition */

ULONG blocks_total;               /* size of the partition in blocks */
ULONG blocks_maxtransfer=1048576; /* max. blocks which may be transfered to the device at once (limits io_Length) */

ULONG byte_low;                   /* the byte offset of our partition on the disk */
ULONG byte_lowh;                  /* high 32 bits */
ULONG byte_high;                  /* the byte offset of the end of our partition (excluding) on the disk */
ULONG byte_highh;                 /* high 32 bits */

ULONG bytes_block;                /* size of a block in bytes */
ULONG bytes_sector;               /* size of a sector in bytes */

ULONG mask_block;
ULONG mask_mask=0xFFFFFFFF;       /* mask as specified by mountlist */

ULONG bufmemtype=MEMF_PUBLIC;   /* default value */


struct SCSI10Cmd {
  UBYTE Opcode;
  UBYTE Lun;
  ULONG LBA;
  UBYTE Reserved;
  UBYTE Length[2];
  UBYTE Control;
};

struct Read10Cmd {
  UBYTE Opcode;
  UBYTE Lun;
  ULONG LBA;
  UBYTE Reserved;
  UBYTE Length[2];
  UBYTE Control;
};

struct Write10Cmd {
  UBYTE Opcode;
  UBYTE Lun;
  ULONG LBA;
  UBYTE Reserved;
  UBYTE Length[2];
  UBYTE Control;
};

#define SCSICMD_READ10   0x28
#define SCSICMD_WRITE10  0x2a



/* Internal globals */

struct SCSICmd scsicmd;       /* for SCSIdirect */
struct SCSI10Cmd scsi10cmd;


/* The purpose of the deviceio code is to provide a layer which
   supports block I/O to devices:

   - Devices are accessed based on Logical Blocks (The partition
     offset and SectorsPerBlock are taken into account).

   - Devices can be acessed normally or using NSD, TD64 or SCSI
     direct transparently.

   - MaxTransfer and Mask is handled automatically.

   - Makes sure no accesses outside the partition are allowed. */


void update(void) {
  ioreq->io_Command=CMD_UPDATE;
  ioreq->io_Length=0;
  DoIO((struct IORequest *)ioreq);
}



void motoroff(void) {
  ioreq->io_Command=TD_MOTOR;
  ioreq->io_Length=0;
  DoIO((struct IORequest *)ioreq);
}



UBYTE *errordescription(LONG errorcode) {
  if(errorcode==TDERR_NotSpecified || errorcode==TDERR_SeekError) {
    return("General device failure.\nThe disk is in an unreadable format.\n\n");
  }
  else if(errorcode==TDERR_BadSecPreamble || errorcode==TDERR_BadSecID || errorcode==TDERR_BadSecHdr || errorcode==TDERR_BadHdrSum || errorcode==TDERR_BadSecSum) {
    return("Invalid or bad sector.\nThe disk is in an unreadable format.\n\n");
  }
  else if(errorcode==TDERR_TooFewSecs || errorcode==TDERR_NoSecHdr) {
    return("Not enough sectors found.\nThe disk is in an unreadable format.\n\n");
  }
  else if(errorcode==TDERR_NoMem) {
    return("The device ran out of memory.\n\n");
  }
  else if(errorcode==HFERR_SelfUnit) {
    return("\n\n");
  }
  else if(errorcode==HFERR_DMA) {
    return("DMA error.\nThe transfering of data to/from the device failed.\n\n");
  }
  else if(errorcode==HFERR_Parity) {
    return("Parity error.\nThe transfering of data to/from the device failed.\n\n");
  }
  else if(errorcode==HFERR_SelTimeout) {
    return("Selection timeout.\nThe device doesn't respond.\n\n");
  }
  else if(errorcode==HFERR_BadStatus || errorcode==HFERR_Phase) {
    return("General device failure.\nThe device is in an invalid state.\n\n");
  }

  return("Unknown error.\n\n");
}



LONG writeprotection(struct IOStdReq *ioreq) {
  ioreq->io_Command=TD_PROTSTATUS;
  ioreq->io_Flags=IOF_QUICK;

  if(DoIO((struct IORequest *)ioreq)==0) {
    if(ioreq->io_Actual==0) {
      /* Not write protected */
      return(ID_VALIDATED);
    }
  }
  /* Write protected */
  return(ID_WRITE_PROTECTED);
}



UBYTE isdiskpresent(struct IOStdReq *ioreq) {
  LONG errorcode;
  ioreq->io_Command=TD_CHANGESTATE;
  ioreq->io_Flags=IOF_QUICK;

  if((errorcode=DoIO((struct IORequest *)ioreq))==0) {
    if(ioreq->io_Actual==0) {
      /* Disk present */
      return(TRUE);
    }
    else {
      /* No disk inserted */
      return(FALSE);
    }
  }

  req_unusual("Doesn't support disk change detection!\n"\
              "(errorcode = %ld).  SFS will now assume\n"\
              "that a disk IS present.", errorcode);

  /* If anything went wrong, we assume a disk is inserted. */
  return(TRUE);
}






LONG initdeviceio(UBYTE *devicename, ULONG unit, ULONG flags, struct DosEnvec *de) {
  if((msgport=CreateMsgPort())!=0) {
    if((ioreq=CreateIORequest(msgport, sizeof(struct IOStdReq)))!=0) {
      if(OpenDevice(devicename, unit, (struct IORequest *)ioreq, flags)==0) {
        deviceopened=TRUE;

        {
          UWORD bs;
          ULONG sectorspercilinder;

          bytes_sector=de->de_SizeBlock<<2;
          bytes_block=bytes_sector*de->de_SectorPerBlock;

          bs=bytes_block;

          shifts_block=0;
          while((bs>>=1)!=0) {
            shifts_block++;
          }

          mask_block=bytes_block-1;

          /* Absolute offset on the entire disk are expressed in Sectors;
             Offset relative to the start of the partition are expressed in Blocks */

          sectorspercilinder=de->de_Surfaces*de->de_BlocksPerTrack;            /* 32 bit */
          sectors_total=sectorspercilinder*(de->de_HighCyl+1-de->de_LowCyl);   /* 32 bit */

          sector_low=sectorspercilinder*de->de_LowCyl;             /* Needed for SCSI direct */
          sector_high=sectorspercilinder*(de->de_HighCyl+1);

          /* sectorspercilinder * bytes_sector cannot exceed 32-bit !! */

          byte_lowh  = MULU64(sectorspercilinder * bytes_sector, de->de_LowCyl,    &byte_low);
          byte_highh = MULU64(sectorspercilinder * bytes_sector, de->de_HighCyl+1, &byte_high);

          sectors_block=de->de_SectorPerBlock;

          blocks_total=sectors_total/de->de_SectorPerBlock;
        }

        /* If the partition's high byte is beyond the 4 GB border we will
           try and detect a 64-bit device. */

        if(scsidirect==TRUE) {
          cmdread=SCSICMD_READ10;
          cmdwrite=SCSICMD_WRITE10;
        }
        else if(byte_highh!=0) {  /* Check for newstyle devices and 64-bit support */
          struct NSDeviceQueryResult nsdqr;
          UWORD *cmdcheck;
          LONG errorcode;

          nsdqr.SizeAvailable=0;
          nsdqr.DevQueryFormat=0;

          ioreq->io_Command=NSCMD_DEVICEQUERY;
          ioreq->io_Length=sizeof(nsdqr);
          ioreq->io_Data=(APTR)&nsdqr;

          if(DoIO((struct IORequest *)ioreq)==0 && (ioreq->io_Actual >= 16) && (nsdqr.SizeAvailable == ioreq->io_Actual) && (nsdqr.DeviceType == NSDEVTYPE_TRACKDISK)) {
            /* This must be a new style trackdisk device */

            newstyledevice=TRUE;

            DEBUG(("Device is a new style device\n"));

            /* Is it safe to use 64 bits with this driver?  We can reject
               bad mounts pretty easily via this check. */

            for(cmdcheck=nsdqr.SupportedCommands; *cmdcheck; cmdcheck++) {
              if(*cmdcheck == NSCMD_TD_READ64) {
                /* This trackdisk style device supports the complete 64-bit
                   command set without returning IOERR_NOCMD! */

                DEBUG(("Device supports 64-bit commands\n"));

                does64bit=TRUE;
                cmdread=NSCMD_TD_READ64;
                cmdwrite=NSCMD_TD_WRITE64;
              }
            }
          }
          else {  /* Check for TD64 supporting devices */
            ioreq->io_Command=24;  /* READ64 */
            ioreq->io_Length=0;
            ioreq->io_Actual=0;
            ioreq->io_Offset=0;
            ioreq->io_Data=0;

            errorcode=DoIO((struct IORequest *)ioreq);
            if(errorcode!=-1 && errorcode!=IOERR_NOCMD) {
              does64bit=TRUE;
              cmdread=24;
              cmdwrite=25;
            }
          }
        }

        if(de->de_TableSize>=12) {
          bufmemtype=de->de_BufMemType;

          if(de->de_TableSize>=13) {
            blocks_maxtransfer=de->de_MaxTransfer>>shifts_block;

            if(blocks_maxtransfer==0) {
              blocks_maxtransfer=1;
            }

            if(de->de_TableSize>=14) {
              mask_mask=de->de_Mask;
            }
          }
        }

        return(0);
      }
    }
  }

  cleanupdeviceio();

  return(-1);
}



void cleanupdeviceio() {
  if(deviceopened) {
    CloseDevice((struct IORequest *)ioreq);
  }

  if(ioreq!=0) {
    DeleteIORequest(ioreq);
  }

  if(msgport!=0) {
    DeleteMsgPort(msgport);
  }
}



LONG transfer(UWORD action, UBYTE *buffer, ULONG blockoffset, ULONG blocklength) {
  ULONG command, blocks;
  ULONG start, starthigh, end,endhigh;
  ULONG maxblocks=blocks_maxtransfer;
  UBYTE *destbuffer;
  UBYTE *tempbuffer=0;
  LONG errorcode;

  if(action==DIO_READ) {
    command=cmdread;
  }
  else if(action==DIO_WRITE) {
    command=cmdwrite;
  }
  else {
    return(-1);
  }

  if(((ULONG)buffer & ~mask_mask)!=0 && (errorcode=getbuffer(&tempbuffer, &maxblocks))!=0) {
    return(errorcode);
  }

  while(blocklength!=0) {
    blocks=blocklength;
    if(blocks>maxblocks) {
      blocks=maxblocks;
    }

    start=(blockoffset<<shifts_block)+byte_low;
    starthigh=(blockoffset>>(32-shifts_block))+byte_lowh;            /* High offset */
    if(start < byte_low) {
      starthigh+=1;     /* Add X bit :-) */
    }

    end=start+(blocks<<shifts_block);
    endhigh=starthigh;
    if(end < start) {
      endhigh+=1;       /* Add X bit */
    }

    if(tempbuffer!=0) {
      destbuffer=tempbuffer;
    }
    else {
      destbuffer=buffer;
    }

    if(scsidirect==TRUE) {
      ioreq->io_Command=HD_SCSICMD;
      ioreq->io_Length=sizeof(struct SCSICmd);
      ioreq->io_Data=&scsicmd;

      scsicmd.scsi_Data=(UWORD *)destbuffer;
      scsicmd.scsi_Length=blocks<<shifts_block;
      scsicmd.scsi_Command=(UBYTE *)&scsi10cmd;
      scsicmd.scsi_CmdLength=sizeof(struct SCSI10Cmd);

      if(command==SCSICMD_READ10) {
        scsicmd.scsi_Flags=SCSIF_READ;
      }
      else {
        scsicmd.scsi_Flags=0;
      }
      scsicmd.scsi_SenseData=NULL;
      scsicmd.scsi_SenseLength=0;
      scsicmd.scsi_SenseActual=0;

      scsi10cmd.Opcode=command;       // SCSICMD_READ10 or SCSICMD_WRITE10;
      scsi10cmd.Lun=0;
      scsi10cmd.LBA=sector_low+blockoffset*sectors_block;
      scsi10cmd.Reserved=0;
      scsi10cmd.Control=0;

      {
        UWORD *ptr=(UWORD *)&scsi10cmd.Length[0];

        //  scsi10cmd.Length=blocks*sectors_block;       /* Odd aligned... */
        *ptr=blocks*sectors_block;
      }
    }
    else {
      ioreq->io_Data=destbuffer;
      ioreq->io_Command=command;
      ioreq->io_Length=blocks<<shifts_block;
      ioreq->io_Offset=start;
      ioreq->io_Actual=starthigh;
    }

    if(tempbuffer!=0 && action==DIO_WRITE) {
      CopyMem(buffer,tempbuffer,blocks<<shifts_block);
    }

    if(  ( starthigh > byte_lowh || (starthigh == byte_lowh && start >= byte_low) ) &&
         ( endhigh < byte_highh || (endhigh == byte_highh && end <= byte_high) )  ) {

/*
      if(command==cmdread) {
        DEBUG(("safedoio: Reading %ld blocks from block %ld.\n",blocks,blockoffset));
      }
      else if(command==cmdwrite) {
        DEBUG(("safedoio: Writing %ld blocks to block %ld.\n",blocks,blockoffset));
      }
*/

      /* We're about to do a physical disk access.  (Re)set timeout.  Since
         the drive's motor will be turned off with the timeout as well we
         always (re)set the timeout even if doio() would return an error. */

      starttimeout();

      while((errorcode=DoIO((struct IORequest *)ioreq))!=0) {
        if(errorcode==TDERR_DiskChanged) {
          if(request(PROGRAMNAME " request","You MUST insert volume %s\n"\
                                            "There still was some data which needs to be transfered!",
                                            "Retry|Cancel",((UBYTE *)BADDR(devnode->dn_Name))+1)<=0) {
            break;
          }
        }
        else if(errorcode==TDERR_WriteProt || (action==DIO_WRITE && writeprotection(ioreq2)==ID_WRITE_PROTECTED)) {
          if(request(PROGRAMNAME " request","%s\n"\
                                            "is write protected.",
                                            "Retry|Cancel",((UBYTE *)BADDR(devnode->dn_Name))+1)<=0) {
            errorcode=ERROR_DISK_WRITE_PROTECTED;
            break;
          }
        }
        else {
          if(request(PROGRAMNAME " request","%s\n"\
                                            "returned an error while volume %s was accessing it.\n\n%s"\
                                            "errorcode = %ld\n"\
                                            "io_Command = %ld\n"\
                                            "io_Offset = %ld\n"\
                                            "io_Length = %ld\n"\
                                            "io_Actual = %ld\n",
                                            "Retry|Cancel",(UBYTE *)BADDR(startupmsg->fssm_Device)+1,((UBYTE *)BADDR(devnode->dn_Name))+1,errordescription(errorcode),errorcode,(ULONG)ioreq->io_Command,ioreq->io_Offset,ioreq->io_Length,ioreq->io_Actual)<=0) {

            break;
          }
        }
      }

      if(errorcode!=0) {
        return(errorcode);
      }
    }
    else {
      DEBUG(("byte_low = 0x%08lx:%08lx, byte_high = 0x%08lx:%08lx\n",byte_lowh,byte_low,byte_highh,byte_high));
      DEBUG(("start = 0x%08lx:%08lx, end = 0x%08lx:%08lx\n",starthigh,start,endhigh,end));

      request(PROGRAMNAME " request","%s\n"\
                                     "tried to access a block outside\n"\
                                     "of this volume's partition.\n"\
                                     "(blockoffset = %ld, blocks = %ld)",
                                     "Ok",((UBYTE *)BADDR(devnode->dn_Name))+1,blockoffset,blocks);
      return(INTERR_OUTSIDE_PARTITION);
    }

    if(tempbuffer!=0 && action==DIO_READ) {
      CopyMem(tempbuffer, buffer, blocks<<shifts_block);
    }

    blocklength-=blocks;
    blockoffset+=blocks;
    buffer+=blocks<<shifts_block;
  }

  return(0);
}
