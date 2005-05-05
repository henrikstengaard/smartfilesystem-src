#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "work:fs/debug.h"
#include "work:fs/packets.h"

LONG main() {
  ULONG debug=0;
  struct RDArgs *readarg;
  UBYTE template[]="DEVICE/A,ALL/S,CB=CACHEBUFFER/S,NODES/S,IO/S,SEEK/S,LOCK/S,BITMAP/S,OBJECTS/S\n";

  struct {char *name;
          ULONG all;
          ULONG cb;
          ULONG nodes;
          ULONG io;
          ULONG seek;
          ULONG lock;
          ULONG bitmap;
          ULONG objects;} arglist={NULL};

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      struct MsgPort *msgport;
      struct DosList *dl;

      if(arglist.cb!=0) {
        debug+=DEBUG_CACHEBUFFER;
      }
      if(arglist.nodes!=0) {
        debug+=DEBUG_NODES;
      }
      if(arglist.io!=0) {
        debug+=DEBUG_IO;
      }
      if(arglist.seek!=0) {
        debug+=DEBUG_SEEK;
      }
      if(arglist.lock!=0) {
        debug+=DEBUG_LOCK;
      }
      if(arglist.bitmap!=0) {
        debug+=DEBUG_BITMAP;
      }
      if(arglist.objects!=0) {
        debug+=DEBUG_OBJECTS;
      }

      if(arglist.all!=0) {
        debug^=0xFFFFFFFE;
      }


      dl=LockDosList(LDF_DEVICES|LDF_READ);
      if((dl=FindDosEntry(dl,arglist.name,LDF_DEVICES))!=0) {
        msgport=dl->dol_Task;
        UnLockDosList(LDF_DEVICES|LDF_READ);

        DoPkt(msgport,ACTION_SET_DEBUG,debug,0,0,0,0);
        PutStr("New debug levels set!\n");
      }
      else {
        UnLockDosList(LDF_DEVICES|LDF_READ);
      }

      FreeArgs(readarg);
    }
    CloseLibrary((struct Library *)DOSBase);
  }
  return(0);
}
