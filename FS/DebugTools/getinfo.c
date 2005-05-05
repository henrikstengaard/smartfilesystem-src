#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "/packets.h"

LONG storeblock(struct MsgPort *msgport, ULONG block, ULONG blocksize, BPTR fh);

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="DEVICE=DRIVE/A,OUTPUTFILE/A\n";

  struct {char *device;
          char *name;} arglist={NULL};

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      struct MsgPort *msgport;
      struct DosList *dl;
      UBYTE *devname=arglist.device;

      while(*devname!=0) {
        if(*devname==':') {
          *devname=0;
          break;
        }
        devname++;
      }

      dl=LockDosList(LDF_DEVICES|LDF_READ);
      if((dl=FindDosEntry(dl,arglist.device,LDF_DEVICES))!=0) {
        ULONG blocks_total;
        ULONG bytes_block;
        LONG errorcode=0;

        msgport=dl->dol_Task;
        UnLockDosList(LDF_DEVICES|LDF_READ);

        if((blocks_total=DoPkt(msgport,ACTION_GET_TOTAL_BLOCKS, 0,0,0,0,0))!=0) {
          if((bytes_block=DoPkt(msgport,ACTION_GET_BLOCKSIZE, 0,0,0,0,0))!=0) {
            BPTR fh;

            if((fh=Open(arglist.name,MODE_NEWFILE))!=0) {

              for(;;) {
                if((errorcode=storeblock(msgport, 0, bytes_block, fh))!=0) {
                  break;
                }
                if((errorcode=storeblock(msgport, 1, bytes_block, fh))!=0) {
                  break;
                }
                if((errorcode=storeblock(msgport, 2, bytes_block, fh))!=0) {
                  break;
                }
                if((errorcode=storeblock(msgport, 3, bytes_block, fh))!=0) {
                  break;
                }
                if((errorcode=storeblock(msgport, 4, bytes_block, fh))!=0) {
                  break;
                }
                if((errorcode=storeblock(msgport, 5, bytes_block, fh))!=0) {
                  break;
                }
                if((errorcode=storeblock(msgport, 6, bytes_block, fh))!=0) {
                  break;
                }
                if((errorcode=storeblock(msgport, 7, bytes_block, fh))!=0) {
                  break;
                }
                if((errorcode=storeblock(msgport, blocks_total-1, bytes_block, fh))!=0) {
                  break;
                }
                break;
              }

              if(errorcode!=0) {
                PrintFault(errorcode,"error while getting information about the drive");
              }
              else {
                PutStr("All done!\n");
              }

              Close(fh);
            }
            else {
              PrintFault(IoErr(),"error while creating output file");
            }
          }
          else {
            PrintFault(IoErr(),"error while getting information about the drive");
          }
        }
        else {
          PrintFault(IoErr(),"error while getting information about the drive");
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


LONG storeblock(struct MsgPort *msgport, ULONG block, ULONG blocksize, BPTR fh) {
  LONG errorcode=0;
  UBYTE *buffer;

  if((buffer=(UBYTE *)DoPkt(msgport,ACTION_GET_BLOCKDATA, block, 0,0,0,0))!=0) {
    if((Write(fh,buffer,blocksize))!=blocksize) {
      errorcode=IoErr();
    }
    FreeVec(buffer);
  }
  else {
    errorcode=IoErr();
  }

  return(errorcode);
}
