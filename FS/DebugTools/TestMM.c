#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <utility/tagitem.h>

static const char version[]={"\0$VER: TestMM 1.0 " __AMIGADATE__ "\r\n"};

#define SIZE (512*1024)

LONG writeandverify(ULONG fill);

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="FILE\n";

  struct {char *file;} arglist={NULL};

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      LONG errorcode;

      if((errorcode=writeandverify(0x00000000))!=0 || (errorcode=writeandverify(0xFFFFFFFF))!=0 || (errorcode=writeandverify(0xAAAAAAAA))!=0) {
        PrintFault(errorcode, "error while testing MaxTransfer");
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


LONG writeandverify(ULONG fill) {
  ULONG *buf;
  ULONG *p;
  LONG errorcode=0;
  LONG n;

  if((buf=AllocVec(SIZE, 0))!=0) {
    BPTR fh;

    p=buf;
    n=SIZE/4;

    while(--n>=0) {
      *p++=fill;
    }

    if((fh=Open("_test_file$000_", MODE_NEWFILE))!=0) {
      if(Write(fh, buf, SIZE)==SIZE) {
        if(Seek(fh, 0, OFFSET_BEGINNING)!=-1) {
          if(Read(fh, buf, SIZE)==SIZE) {
            p=buf;
            n=SIZE/4;

            while(--n>=0 && *p++==fill) {
            }

            if(n>=0) {
              PutStr("Your MaxTransfer value is TOO HIGH\nTry setting it to 0x1fffe or 0xfffe.\n");
            }
          }
          else {
            errorcode=IoErr();
          }
        }
        else {
          errorcode=IoErr();
        }
      }
      else {
        errorcode=IoErr();
      }

      Close(fh);
      DeleteFile("_test_file$000_");
    }
    else {
      errorcode=IoErr();
    }

    FreeVec(buf);
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}
