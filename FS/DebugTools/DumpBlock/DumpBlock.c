#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <exec/errors.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "stdio.h"

#include "work:fs/deviceio.h"
#include "work:fs/fs.h"
#include "work:fs/blockstructure.h"

#include <devices/newstyle.h>      /* Doesn't include exec/types.h which is why it is placed here */

/* ASM prototypes */

extern ULONG __asm RANDOM(register __d0 ULONG);
extern LONG __asm STACKSWAP(void);
extern ULONG __asm CALCCHECKSUM(register __d0 ULONG,register __a0 ULONG *);
extern ULONG __asm MULU64(register __d0 ULONG,register __d1 ULONG,register __a0 ULONG *);
extern WORD __asm COMPRESSFROMZERO(register __a0 UWORD *,register __a1 UBYTE *,register __d0 ULONG);

void dumpblock(ULONG block, void *data, ULONG bytes);
LONG read(ULONG block);
LONG safedoio(UWORD action,UBYTE *buffer,ULONG blocks,ULONG blockoffset);
BOOL checkchecksum(void *buffer);
void setchecksum(void *buffer);

UBYTE string[200];

struct DosEnvec *dosenvec;

/* blocks_  = the number of blocks of something (blocks can contain 1 or more sectors)
   block_   = a block number of something (relative to start of partition)
   sectors_ = the number of sectors of something (1 or more sectors form a logical block)
   sector_  = a sector number (relative to the start of the disk)
   bytes_   = the number of bytes of something
   byte_    = a byte number of something
   shifts_  = the number of bytes written as 2^x of something
   mask_    = a mask for something */

ULONG sectors_total;              /* size of the partition in sectors */
UWORD sectors_block;              /* number of sectors in a block */
UWORD pad4;

ULONG sector_low;                 /* sector offset of our partition, needed for SCSI direct */
ULONG sector_high;                /* last sector plus one of our partition */

ULONG blocks_total;               /* size of the partition in blocks */
ULONG blocks_reserved_start;      /* number of blocks reserved at start (=reserved) */
ULONG blocks_reserved_end;        /* number of blocks reserved at end (=prealloc) */
ULONG blocks_used;                /* number of blocks in use excluding reserved blocks
                                     (=numblockused) */
ULONG blocks_useddiff;            /* number of blocks which became used in the last operation (may be negative) */
ULONG blocks_bitmap;              /* number of BTMP blocks for this partition */
ULONG blocks_inbitmap;            /* number of blocks a single bitmap block can contain info on */
ULONG blocks_admin;               /* the size of all AdminSpaces */
ULONG blocks_maxtransfer=1048576; /* max. blocks which may be transfered to the device at once (limits io_Length) */

ULONG block_root;                 /* the block offset of the root block */
ULONG block_bitmapbase;           /* the block offset of the first bitmap block */
ULONG block_extentbnoderoot;      /* the block offset of the root of the extent bnode tree */
ULONG block_adminspace;           /* the block offset of the first adminspacecontainer block */
ULONG block_rovingblockptr;       /* the roving block pointer! */

ULONG node_containers;            /* number of containers per ExtentIndexContainer */

ULONG byte_low;                   /* the byte offset of our partition on the disk */
ULONG byte_lowh;                  /* high 32 bits */
ULONG byte_high;                  /* the byte offset of the end of our partition (excluding) on the disk */
ULONG byte_highh;                 /* high 32 bits */

ULONG bytes_block;                /* size of a block in bytes */
ULONG bytes_sector;               /* size of a sector in bytes */

ULONG mask_block;
ULONG mask_block32;               /* masks the least significant bits of a BLCKf pointer */
ULONG mask_mask=0xFFFFFFFF;       /* mask as specified by mountlist */
ULONG mask_debug;

UWORD shifts_block;               /* shift count needed to convert a blockoffset<->byteoffset */
UWORD shifts_block32;             /* shift count needed to convert a blockoffset<->32byteoffset (only used by nodes.c!) */

ULONG disktype;                   /* the type of media inserted (same as id_DiskType in InfoData
                                     structure) */
UBYTE newstyledevice=FALSE;
UBYTE does64bit=FALSE;
UBYTE scsidirect=FALSE;
UBYTE is_casesensitive=FALSE;
UWORD cmdread=CMD_READ;
UWORD cmdwrite=CMD_WRITE;


struct MsgPort *msgport;
struct IOStdReq *ioreq;

UBYTE *buffer;

struct SCSICmd scsicmd;       /* for SCSIdirect */
struct SCSI10Cmd scsi10cmd;

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="DEVICE=DRIVE/A,BLOCK/N/A,BINARY/S\n";

  struct {char *device;
          ULONG *block;
          ULONG binary;} arglist={NULL};

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      struct MsgPort *msgport;
      struct DeviceNode *dn;
      UBYTE *devname=arglist.device;

      while(*devname!=0) {
        if(*devname==':') {
          *devname=0;
          break;
        }
        devname++;
      }

      dn=(struct DeviceNode *)LockDosList(LDF_DEVICES|LDF_READ);
      if((dn=(struct DeviceNode *)FindDosEntry((struct DosList *)dn,arglist.device,LDF_DEVICES))!=0) {
        struct FileSysStartupMsg *fssm;

        fssm=(struct FileSysStartupMsg *)BADDR(dn->dn_Startup);
        dosenvec=(struct DosEnvec *)BADDR(fssm->fssm_Environ);

        UnLockDosList(LDF_DEVICES|LDF_READ);

        if((msgport=CreateMsgPort())!=0) {
          if((ioreq=CreateIORequest(msgport,sizeof(struct IOStdReq)))!=0) {
            if(OpenDevice((UBYTE *)BADDR(fssm->fssm_Device)+1,fssm->fssm_Unit,(struct IORequest *)ioreq,fssm->fssm_Flags)==0) {
              {
                struct DosEnvec *de=dosenvec;
                UWORD bs;
                ULONG sectorspercilinder;

                bytes_sector=de->de_SizeBlock<<2;
                bytes_block=bytes_sector*de->de_SectorPerBlock;

                bs=bytes_block;

                shifts_block=0;
                while((bs>>=1)!=0) {
                  shifts_block++;
                }
//                shifts_block32=shifts_block-BLCKFACCURACY;

//                mask_block32=(1<<shifts_block32)-1;
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
                blocks_reserved_start=de->de_Reserved;
                blocks_reserved_end=de->de_PreAlloc;

                if(blocks_reserved_start<1) {
                  blocks_reserved_start=1;
                }

                if(blocks_reserved_end<1) {
                  blocks_reserved_end=1;
                }

                blocks_used=0;
//                blocks_inbitmap=(bytes_block-sizeof(struct fsBitmap))<<3;  /* must be a multiple of 32 !! */
//                blocks_bitmap=(blocks_total+blocks_inbitmap-1)/blocks_inbitmap;
//                blocks_admin=32;

                // block_adminspace=blocks_reserved_start;  /* The only 2 fixed blocks */
                // block_root=blocks_reserved_start+1;      /* on disk.                */
//                block_rovingblockptr=0;

//                printf("Partition start offset : 0x%08lx:%08lx   End offset  : 0x%08lx:%08lx\n",byte_lowh,byte_low,byte_highh,byte_high);
//                printf("Partition first sector : %-8ld              Last sector : %ld\n",sector_low,sector_high);
//                printf("sectors/block : %ld\n",de->de_SectorPerBlock);
//                printf("bytes/sector  : %ld\n",bytes_sector);
//                printf("bytes/block   : %ld\n",bytes_block);
//                printf("Total blocks in partition : %ld\n",blocks_total);
              }



              /* If the partition's high byte is beyond the 4 GB border we will
                 try and detect a 64-bit device. */

              if(byte_highh!=0) {
                struct NSDeviceQueryResult nsdqr;
                UWORD *cmdcheck;
                LONG errorcode;

                /* Checks for newstyle devices and 64-bit support */

                nsdqr.SizeAvailable=0;
                nsdqr.DevQueryFormat=0;

                ioreq->io_Command=NSCMD_DEVICEQUERY;
                ioreq->io_Length=sizeof(nsdqr);
                ioreq->io_Data=(APTR)&nsdqr;

                if(DoIO((struct IORequest *)ioreq)==0 && (ioreq->io_Actual >= 16) && (nsdqr.SizeAvailable == ioreq->io_Actual) && (nsdqr.DeviceType == NSDEVTYPE_TRACKDISK)) {
                  /* This must be a new style trackdisk device */

                  newstyledevice=TRUE;

                  printf("Device is a new style device (NSD)\n");

                  /* Is it safe to use 64 bits with this driver?  We can reject
                     bad mounts pretty easily via this check. */

                  for(cmdcheck=nsdqr.SupportedCommands; *cmdcheck; cmdcheck++) {
                    if(*cmdcheck == NSCMD_TD_READ64) {
                      /* This trackdisk style device supports the complete 64-bit
                         command set without returning IOERR_NOCMD! */

                      printf("Device supports 64-bit commands\n");

                      does64bit=TRUE;
                      cmdread=NSCMD_TD_READ64;
                      cmdwrite=NSCMD_TD_WRITE64;
                    }
                  }
                }
                else {
                  /* Checks for TD64 supporting devices */

                  ioreq->io_Command=24;  /* READ64 */
                  ioreq->io_Length=0;
                  ioreq->io_Actual=0;
                  ioreq->io_Offset=0;
                  ioreq->io_Data=0;

                  errorcode=DoIO((struct IORequest *)ioreq);
                  if(errorcode!=-1 && errorcode!=IOERR_NOCMD) {
                    printf("Device supports 64-bit commands\n");

                    does64bit=TRUE;
                    cmdread=24;
                    cmdwrite=25;
                  }
                }
              }

              if(scsidirect==TRUE) {
                cmdread=SCSICMD_READ10;
                cmdwrite=SCSICMD_WRITE10;
              }

              if((buffer=AllocVec(bytes_block,0))!=0) {
                read(*arglist.block);

                if(checkchecksum(buffer)!=DOSTRUE) {
                  printf("Warning: Checksum of block %ld is wrong.  It is 0x%08lx while it should be 0x%08lx.\n", *arglist.block, ((struct fsBlockHeader *)buffer)->ownblock, -CALCCHECKSUM(bytes_block, (ULONG *)buffer));
                }
                if(((struct fsBlockHeader *)buffer)->ownblock != *arglist.block) {
                  printf("Warning: Block %ld indicates that it is actually block %ld (ownblock is wrong!)\n", *arglist.block, ((struct fsBlockHeader *)buffer)->ownblock);
                }

                if(arglist.binary!=0) {
                  Write(Output(), buffer, bytes_block);
                }
                else {
                  dumpblock(*arglist.block,buffer,bytes_block);
                }

                FreeVec(buffer);
              }
              else {
                printf("Not enough memory\n");
              }

              CloseDevice((struct IORequest *)ioreq);
            }
            DeleteIORequest(ioreq);
          }
          DeleteMsgPort(msgport);
        }
      }
      else {
        VPrintf("Unknown device %s\n",&arglist.device);
        UnLockDosList(LDF_DEVICES|LDF_READ);
      }

      FreeArgs(readarg);
    }
    CloseLibrary((struct Library *)DOSBase);
  }
  return(0);
}



/*
            else if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
              PutStr("\n***Break\n");
              break;
            }
*/




LONG doio(void) {
//  LONG errorcode;

  return(DoIO((struct IORequest *)ioreq));

/*
  while((errorcode=DoIO((struct IORequest *)ioreq))!=0) {
    if(errorcode==TDERR_WriteProt) {
      if(request(PROGRAMNAME " request","%s\n"\
                                        "is write protected.\n",
                                        "Retry|Cancel",((UBYTE *)BADDR(devnode->dn_Name))+1)<=0) {
        return(errorcode);
      }
    }
    else if(errorcode==TDERR_DiskChanged) {
      if(request(PROGRAMNAME " request","You MUST insert volume %s\n"\
                                        "There still was some data which needs to be transfered!\n",
                                        "Retry|Cancel",((UBYTE *)BADDR(devnode->dn_Name))+1)<=0) {
        return(errorcode);
      }
    }
    else {
      if(request(PROGRAMNAME " request","%s\n"\
                                        "returned an error while volume %s was accessing it.\n\n"\
                                        "errorcode = %ld\n"\
                                        "io_Command = %ld\n"\
                                        "io_Offset = %ld\n"\
                                        "io_Length = %ld\n"\
                                        "io_Actual = %ld\n",
                                        "Retry|Cancel",(UBYTE *)BADDR(startupmsg->fssm_Device)+1,((UBYTE *)BADDR(devnode->dn_Name))+1,errorcode,(ULONG)ioreq->io_Command,ioreq->io_Offset,ioreq->io_Length,ioreq->io_Actual)<=0) {

        return(errorcode);
      }
    }
  } */
}



LONG read(ULONG block) {
  return(safedoio(DIO_READ,buffer,1,block));

//  return(readc(block,buffer,1));
}



LONG safedoio(UWORD action,UBYTE *buffer,ULONG blocks,ULONG blockoffset) {
  ULONG command;
  ULONG start,starthigh;
  ULONG end,endhigh;
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

  if(scsidirect==TRUE) {
    ioreq->io_Command=HD_SCSICMD;
    ioreq->io_Length=sizeof(struct SCSICmd);
    ioreq->io_Data=&scsicmd;

    scsicmd.scsi_Data=(UWORD *)buffer;
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
    ioreq->io_Data=buffer;
    ioreq->io_Command=command;
    ioreq->io_Length=blocks<<shifts_block;
    ioreq->io_Offset=start;
    ioreq->io_Actual=starthigh;
  }

  if(  ( starthigh > byte_lowh || (starthigh == byte_lowh && start >= byte_low) ) &&
       ( endhigh < byte_highh || (endhigh == byte_highh && end <= byte_high) )  ) {

    /* We're about to do a physical disk access.  (Re)set timeout.  Since
       the drive's motor will be turned off with the timeout as well we
       always (re)set the timeout even if doio() would return an error. */

    if((errorcode=doio())!=0) {
      return(errorcode);
    }
  }
  else {
//    DEBUG(("byte_low = 0x%08lx:%08lx, byte_high = 0x%08lx:%08lx\n",byte_lowh,byte_low,byte_highh,byte_high));
//    DEBUG(("start = 0x%08lx:%08lx, end = 0x%08lx:%08lx\n",starthigh,start,endhigh,end));

    return(88);
  }

  return(0);
}



void dumpblock(ULONG block, void *data, ULONG bytes) {
  ULONG *d=(ULONG *)data;
  UBYTE *d2=(UBYTE *)data;
  UWORD off=0;
  UBYTE s[40];

  if(bytes<bytes_block) {
    printf("Dump of first %ld bytes of block %ld.\n",bytes,block);
  }
  else {
    printf("Dump of block %ld.\n",block);
  }

  while(bytes>0) {
    WORD n;
    UBYTE c;
    UBYTE *s2;

    n=16;
    s2=s;

    while(--n>=0) {
      c=*d2++;

      if(c<32) {
        c+=64;
      }
      if(c>=127 && c<=160) {
        c='.';
      }

      *s2++=c;
    }
    *s2=0;

    printf("0x%04lx: %08lx %08lx %08lx %08lx %s\n",off,d[0],d[1],d[2],d[3],s);

    bytes-=16;
    d+=4;
    off+=16;
  }
}



BOOL checkchecksum(void *buffer) {
  if(CALCCHECKSUM(bytes_block, buffer)==0) {
    return(DOSTRUE);
  }
  return(DOSFALSE);
}



void setchecksum(void *buffer) {
  struct fsBlockHeader *bh=(struct fsBlockHeader *)buffer;

  bh->checksum=0;    /* Important! */
  bh->checksum=-CALCCHECKSUM(bytes_block, (ULONG *)buffer);
}
