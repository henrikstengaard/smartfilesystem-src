#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <utility/tagitem.h>
#include <stdio.h>
#include <libraries/iffparse.h>

#include "/packets.h"
#include "/query.h"

/*
   Simple defragmenting program.  It starts defragmenting the disk and prints
   out what it does.  Just press CTRL-C to stop it, and run it again to restart.
*/


// static const char version[]={"\0$VER: SFSquery 1.0 " __AMIGADATE__ "\r\n"};

struct DefragmentStep {
  ULONG id;       // id of the step ("MOVE", "DONE" or 0)
  ULONG length;   // length in longwords (can be 0)
  ULONG data[0];   // size of this array is determined by length.
};

struct DefragmentStep data[20];    // 80 bytes buffer.  SFS doesn't check this yet, so make sure it is large enough.

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="DEVICE/A\n";

  struct {char *name;} arglist={NULL};

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      struct MsgPort *msgport;
      struct DosList *dl;
      UBYTE *devname=arglist.name;

      while(*devname!=0) {
        if(*devname==':') {
          *devname=0;
          break;
        }
        devname++;
      }

      dl=LockDosList(LDF_DEVICES|LDF_READ);
      if((dl=FindDosEntry(dl,arglist.name,LDF_DEVICES))!=0) {
        LONG errorcode;

        msgport=dl->dol_Task;
        UnLockDosList(LDF_DEVICES|LDF_READ);


        DoPkt(msgport, ACTION_SFS_DEFRAGMENT_INIT, 0, 00, 0, 0, 0);

        while((errorcode=DoPkt(msgport, ACTION_SFS_DEFRAGMENT_STEP, (LONG)data, 20, 0, 0, 0))!=DOSFALSE) {
          if(data->id==MAKE_ID('M','O','V','E')) {
            printf("Moved %ld blocks from block %ld to %ld.\n", data->data[0], data->data[1], data->data[2]);
          }
          else if(data->id==MAKE_ID('D','O','N','E')) {
            break;
          }

//          data->id=0;   // This was included before SFS did this automatically.
        }

        printf("errorcode = %ld, 2nd error = %ld\n", errorcode, IoErr());
      }
      else {
        VPrintf("Couldn't find device '%s:'.\n",&arglist.name);
        UnLockDosList(LDF_DEVICES|LDF_READ);
      }

      FreeArgs(readarg);
    }
    else {
      PutStr("Wrong arguments!\n");
    }
    CloseLibrary((struct Library *)DOSBase);
  }
  return(0);
}
