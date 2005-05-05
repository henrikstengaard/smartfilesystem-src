#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>


char template[]="DEVICE/A";

struct {char *device;} arglist={NULL};

ULONG main() {
  struct RDArgs *readarg;
  struct InfoData id;
  BPTR lock;

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      if((lock=Lock(arglist.device,ACCESS_READ))!=0) {
        Info(lock,&id);

        printf("Device: %-5s  NumSoftErrors: %d  Unit: %d  Diskstate: %2d  NumBlocks: %d\nNumBlocksUsed: %d  BytesPerBlock: %d  DiskType: $%08x  InUse: $%08x\n",arglist.device,id.id_NumSoftErrors,id.id_UnitNumber,id.id_DiskState,id.id_NumBlocks,id.id_NumBlocksUsed,id.id_BytesPerBlock,id.id_DiskType,id.id_InUse);

        UnLock(lock);
      }
      FreeArgs(readarg);
    }
    CloseLibrary((struct Library *)DOSBase);
  }
  return(0);
}
