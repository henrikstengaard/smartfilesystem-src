#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <utility/tagitem.h>
#include <stdio.h>

#include "/packets.h"
#include "/query.h"

static const char version[]={"\0$VER: DisplayStats 1.0 " __AMIGADATE__ "\r\n"};

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

        {
          struct TagItem tags[]={ASQ_CACHE_ACCESSES, 0,
                                 ASQ_CACHE_MISSES, 0,
                                 ASQ_OPERATIONS_DECODED, 0,
                                 ASQ_EMPTY_OPERATIONS_DECODED, 0};

          printf("Cache statistics for %s:\n", arglist.name);

          for(;;) {
            if((errorcode=DoPkt(msgport, ACTION_SFS_QUERY, (LONG)&tags, 0, 0, 0, 0))!=DOSFALSE) {
              ULONG perc;

              if(tags[0].ti_Data!=0) {
                perc=tags[1].ti_Data*100 / tags[0].ti_Data;
              }
              else {
                perc=0;
              }

              printf("Accesses: %-8ld OpsDecoded: %-8ld EmptyOpsDecoded: %-8ld Misses: %ld (%ld%%)   \r", tags[0].ti_Data, tags[2].ti_Data, tags[3].ti_Data, tags[1].ti_Data, perc);
              fflush(stdout);
              Delay(50);
            }
            else {
              break;
            }
          }
        }
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
