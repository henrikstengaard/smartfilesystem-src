#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="FILENAME/A,SIZE/A/N\n";

  struct {char *filename;
          LONG *size;} arglist={NULL};

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      BPTR fh;

      if((fh=Open(arglist.filename, MODE_READWRITE))!=0) {
        if(SetFileSize(fh,*arglist.size,OFFSET_BEGINNING)==-1) {
          PrintFault(IoErr(),"Error while setting file size");
        }
        Close(fh);
      }
      else {
        PrintFault(IoErr(),"Error while opening file");
      }

      FreeArgs(readarg);
    }
    CloseLibrary((struct Library *)DOSBase);
  }
  return(0);
}
