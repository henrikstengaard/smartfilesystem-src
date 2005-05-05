#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <utility/tagitem.h>

#include "/packets.h"

static const char version[]={"\0$VER: Inhibit 1.0 " __AMIGADATE__ "\r\n"};

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="DEVICE=DRIVE/A,ON/S,OFF/S\n";

  struct {char *device;
          ULONG on;
          ULONG off;} arglist={NULL};

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
        LONG errorcode=0;

        msgport=dl->dol_Task;
        UnLockDosList(LDF_DEVICES|LDF_READ);

        if((errorcode=DoPkt(msgport,ACTION_INHIBIT,arglist.off!=0 ? DOSFALSE:DOSTRUE,0,0,0,0))==DOSFALSE) {
          PrintFault(IoErr(),"error while (un)locking the drive");
        }
      }
      else {
        VPrintf("Unknown device %s\n",&arglist.device);
        UnLockDosList(LDF_DEVICES|LDF_READ);
      }

      FreeArgs(readarg);
    }
    else {
      PutStr("wrong args!\n");
    }

    CloseLibrary((struct Library *)DOSBase);
  }
  return(0);
}
