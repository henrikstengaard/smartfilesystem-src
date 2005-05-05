#include <dos/dos.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

/* Locks test program */

main() {
  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    BPTR lock;

    if((lock=CreateDir("locks$testdir"))!=0) {
      BPTR lock2;
      LONG errorcode;

      if((lock2=Lock("locks$testdir", SHARED_LOCK))!=0) {
        UnLock(lock2);
        errorcode=0;
      }
      else {
        errorcode=IoErr();
      }

      if(errorcode==ERROR_OBJECT_IN_USE) {
        if((lock2=Lock("locks$testdir", EXCLUSIVE_LOCK))!=0) {
          UnLock(lock2);
          errorcode=0;
        }
        else {
          errorcode=IoErr();
        }

        if(errorcode==ERROR_OBJECT_IN_USE) {
          BPTR fh;

          if((fh=Open("locks$testdir/testfile", MODE_NEWFILE))!=0) {
            printf("[info] locks - Creating a new file in an exclusively locked directory is allowed.\n");
            Close(fh);
            DeleteFile("locks$testdir/testfile");
          }
          else {
            PrintFault(IoErr(),"[info] locks - Creating a new file in an exclusively locked directory is not allowed");
          }
        }
        else {
          printf("locks - Locking an exclusively locked directory for exclusive access didn't result in 'Object is in use' but in error %ld.\n", errorcode);
        }
      }
      else {
        printf("locks - Locking an exclusively locked directory for shared access didn't result in 'Object is in use' but in error %ld.\n", errorcode);
      }

      UnLock(lock);
      DeleteFile("locks$testdir");
    }
    else {
      PrintFault(IoErr(), "locks - error while creating the directory 'locks$testdir'");
    }

    CloseLibrary((struct Library *)DOSBase);
  }
}
