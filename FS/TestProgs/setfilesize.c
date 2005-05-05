#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

/* SetFileSize test program

   Correct behaviour of SetFileSize function:

   After setting the size of the file the file ptr remains at the same location
   it was before calling SetFileSize(), unless this location would be located
   beyond the new EOF (which can happen only when truncating the file).  In this
   case the file ptr is set to the new EOF. */


main() {
  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    BPTR fh;

    if((fh=Open("setfilesize$testfile",MODE_NEWFILE))!=0) {
      LONG newsize,pos,errorcode;

      do {
        newsize=SetFileSize(fh,5000,OFFSET_END);
        if(newsize!=5000) {
          errorcode=IoErr();
          PrintFault(errorcode!=0 ? errorcode : -1,"setfilesize - failed to set file size from 0 to 5000 bytes");
          break;
        }

        pos=Seek(fh,0,OFFSET_CURRENT);
        if(pos==-1) {
          PrintFault(IoErr(),"setfilesize - error while seeking to current position");
          break;
        }

        if(pos!=0) {
          PutStr("setfilesize - fileptr was at 0 but didn't remain at 0 after extending the file from 0 to 5000 bytes.\n");
          if(Seek(fh,0,OFFSET_BEGINNING)==-1) {
            PrintFault(IoErr(),"setfilesize - error while seeking to 0");
            break;
          }
        }



        newsize=SetFileSize(fh,4000,OFFSET_BEGINNING);
        if(newsize!=4000) {
          PrintFault(IoErr(),"setfilesize - failed to set file size to from 5000 to 4000 bytes");
          break;
        }

        pos=Seek(fh,0,OFFSET_CURRENT);
        if(pos==-1) {
          PrintFault(IoErr(),"setfilesize - error while seeking to current position");
          break;
        }

        if(pos!=0) {
          PutStr("setfilesize - fileptr was at 0 but didn't remain at 0 after truncating the file from 5000 to 4000 bytes.\n");
          if(Seek(fh,0,OFFSET_BEGINNING)==-1) {
            PrintFault(IoErr(),"setfilesize - error while seeking to 0");
            break;
          }
        }



        if(Seek(fh,2000,OFFSET_BEGINNING)==-1) {
          PrintFault(IoErr(),"setfilesize - error while seeking to 2000");
          break;
        }



        newsize=SetFileSize(fh,3000,OFFSET_BEGINNING);
        if(newsize!=3000) {
          PrintFault(IoErr(),"setfilesize - failed to set file size from 4000 to 3000 bytes");
          break;
        }

        pos=Seek(fh,0,OFFSET_CURRENT);
        if(pos==-1) {
          PrintFault(IoErr(),"setfilesize - error while seeking to current position");
          break;
        }

        if(pos!=2000) {
          PutStr("setfilesize - fileptr was at 2000 but didn't remain at 2000 after truncating the file from 4000 to 3000 bytes.\n");
          if(Seek(fh,2000,OFFSET_BEGINNING)==-1) {
            PrintFault(IoErr(),"setfilesize - error while seeking to 2000");
            break;
          }
        }



        newsize=SetFileSize(fh,1000,OFFSET_BEGINNING);
        if(newsize!=1000) {
          PrintFault(IoErr(),"setfilesize - failed to set file size from 3000 to 1000 bytes");
          break;
        }

        pos=Seek(fh,0,OFFSET_CURRENT);
        if(pos==-1) {
          PrintFault(IoErr(),"setfilesize - error while seeking to current position");
          break;
        }

        if(pos!=1000) {
          PutStr("setfilesize - fileptr was at 2000 but wasn't set to 1000 after truncating the file from 3000 to 1000 bytes.\n");
          /*
          if(Seek(fh,2000,OFFSET_BEGINNING)==-1) {
            PrintFault(IoErr(),"setfilesize - error while seeking to 2000");
            break;
          }
          */
        }

        if(SetFileSize(fh,-5000,OFFSET_BEGINNING)!=-1) {
          PutStr("setfilesize - no error was returned when setting the file to -5000 bytes\n");
        }
      } while(0);

      Close(fh);
      DeleteFile("setfilesize$testfile");
    }
    else {
      PrintFault(IoErr(),"setfilesize - failed to create new file 'setfilesize$testfile'");
    }

    CloseLibrary((struct Library *)DOSBase);
  }
}


/* Odd things:

   In FFS when truncating the size of a file, then regardless of where the
   fileptr is, the fileptr will be set to the new file size.  When extending
   a file however the fileptr remains where it was.

   In the RAM-Handler when truncating the size of a file, then the fileptr is
   set to the new EOF if the current location is located beyond the new EOF.
   When extending a file the fileptr is always set to the new filesize.

   Aarrgh!!

*/

