#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <utility/tagitem.h>

static const char version[]={"\0$VER: Examine 1.0 " __AMIGADATE__ "\r\n"};

UBYTE mybuffer[1000]="";

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="DEVICE/A,FILE/A\n";

  struct {char *device;
          char *file;} arglist={NULL};

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
        struct FileInfoBlock *fib;
        BPTR lock;
        LONG errorcode;

        msgport=dl->dol_Task;
        UnLockDosList(LDF_DEVICES|LDF_READ);

        if((fib=AllocVec(512, 0))!=0) {
          if((lock=Lock(arglist.file, SHARED_LOCK))!=0) {

            if((errorcode=DoPkt(msgport,ACTION_EXAMINE_OBJECT, lock, MKBADDR(fib), 0, 0, 0))==DOSFALSE) {
              PrintFault(IoErr(), "while sending packet");
              ReadLink(msgport, lock, arglist.file, mybuffer, 1000);
            }

            printf("DirEntryType : %-8ld     EntryType : %ld\n", fib->fib_DirEntryType, fib->fib_EntryType);
            printf("Protection   : 0x%08lx   Size      : %ld bytes\n", fib->fib_Protection, fib->fib_Size);
            printf("DiskKey      : 0x%08lx   NumBlocks : %ld blocks\n", fib->fib_DiskKey, fib->fib_NumBlocks);
            printf("OwnerUID     : 0x%04lx       OwnerGID  : 0x%04lx\n", fib->fib_OwnerUID, fib->fib_OwnerGID);
            printf("FileName     : %s\n", fib->fib_FileName);
            printf("Comment      : %s\n", fib->fib_Comment);
            if(errorcode==DOSFALSE) {
              printf("SoftLink     : %s\n", mybuffer);
            }

            UnLock(lock);
          }
          else {
            PrintFault(IoErr(), "error locking file");
          }

          FreeVec(fib);
        }
        else {
          PutStr("Out of memory\n");
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
