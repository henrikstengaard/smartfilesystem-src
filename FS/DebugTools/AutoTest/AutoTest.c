#include <dos/dos.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "stdio.h"

/* ASM prototypes */

extern ULONG __asm RANDOM(register __d0 ULONG);
extern LONG __asm STACKSWAP(void);
extern ULONG __asm CALCCHECKSUM(register __d0 ULONG,register __a0 ULONG *);
extern ULONG __asm MULU64(register __d0 ULONG,register __d1 ULONG,register __a0 ULONG *);
extern WORD __asm COMPRESSFROMZERO(register __a0 UWORD *,register __a1 UBYTE *,register __d0 ULONG);

ULONG rangerandom(ULONG);
LONG create(LONG f);
LONG delete(LONG f);
LONG extend(LONG f);

UBYTE *buffer;

ULONG rndseed=1001;  // -> 3450
// ULONG rndseed=999;  // -> 4380
// ULONG rndseed=602;  // -> 5170
// ULONG rndseed=777;  // -> 4560
// ULONG rndseed=333;  // -> 6570
// ULONG rndseed=1000;  // -> 3710
// ULONG rndseed=45567;  // -> 7270
// ULONG rndseed=678;  // -> 3710
// ULONG rndseed=679;  // -> 3630
// ULONG rndseed=682;  // -> 3650
// ULONG rndseed=679;


/*
            else if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
              PutStr("\n***Break\n");
              break;
            }
*/

#define MAXFILES 600

struct {char *dir;
        LONG *debug;} arglist={NULL};

LONG debug=0x7FFFFFFF;
LONG ops=0;

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="DIR/A,DEBUG/N\n";
  UBYTE *files;

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      if(arglist.debug!=0) {
        debug=*arglist.debug;
      }

      if((files=AllocVec(MAXFILES,MEMF_CLEAR))!=0) {
        LONG errorcode;
        BPTR newdirlock;

        newdirlock=Lock(arglist.dir,SHARED_LOCK);
        newdirlock=CurrentDir(newdirlock);

        for(;;) {
          if(rangerandom(1000)==0) {
            Delay(200);
          }

          ops++;

          if(rangerandom(4)==0) {
            LONG f;

            /* Delete a file */

            f=rangerandom(MAXFILES);

            if(files[f]==TRUE) {
              if((errorcode=delete(f))!=0) {
                printf("\n");
                PrintFault(errorcode,"Error while deleting file");
                break;
              }
              files[f]=FALSE;
            }
          }
          else {
            LONG f;

            /* Create file */

            f=rangerandom(MAXFILES);

            if(files[f]==FALSE) {
              if((errorcode=create(f))!=0) {
                printf("\n");
                PrintFault(errorcode,"Error while creating file");
                break;
              }
              files[f]=TRUE;
            }
            else {
              if((errorcode=extend(f))!=0) {
                printf("\n");
                PrintFault(errorcode,"Error while extending file");
                break;
              }
            }
          }

          if((ops % 10)==0) {
            Printf("\rOperations %ld",ops);
          }

          if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            PutStr("\n***Break\n");
            break;
          }
        }

        newdirlock=CurrentDir(newdirlock);
        UnLock(newdirlock);

        FreeVec(files);
      }

      FreeArgs(readarg);
    }
    CloseLibrary((struct Library *)DOSBase);
  }
  return(0);
}


ULONG rangerandom(ULONG range) {
  rndseed=RANDOM(rndseed);
  return(rndseed%range);
}



LONG create(LONG f) {
  BPTR fh;
  UBYTE name[16];
  LONG errorcode=0;

  sprintf(name,"tf%06ld.tst",f);

  if(rangerandom(2)==0) {
    if(debug<=ops) printf("%6ld: Open MODE_NEWFILE\n", ops);

    if((fh=Open(name,MODE_NEWFILE))==0) {
      return(IoErr());
    }

    if(debug<=ops) printf("%6ld: Write 12 bytes\n", ops);

    if(Write(fh,name,12)!=12) {
      errorcode=IoErr();
    }
    Close(fh);
  }
  else {
    if(debug<=ops) printf("%6ld: Open MODE_NEWFILE temp\n", ops);

    if((fh=Open("temporary",MODE_NEWFILE))==0) {
      return(IoErr());
    }

    if(debug<=ops) printf("%6ld: Write 12 bytes to temp\n", ops);

    if(Write(fh,name,12)!=12) {
      errorcode=IoErr();
    }
    Close(fh);

    if(errorcode==0) {
      if(debug<=ops) printf("%6ld: Delete original\n", ops);

      DeleteFile(name);

      if(debug<=ops) printf("%6ld: Rename temp to original\n", ops);

      Rename("temporary",name);
    }
  }

  return(errorcode);
}



LONG extend(LONG f) {
  BPTR fh;
  UBYTE name[16];
  LONG errorcode=0;
  LONG size=rangerandom(16384);

  sprintf(name,"tf%06ld.tst",f);

  if(debug<=ops) printf("%6ld: Open MODE_OLDFILE\n", ops);

  if((fh=Open(name,MODE_OLDFILE))==0) {
    return(IoErr());
  }

  if(rangerandom(2)==0) {
    LONG bytes;

    while(size>0) {
      bytes=size;
      if(bytes>1024) {
        bytes=1024;
      }

      if(debug<=ops) printf("%6ld: Write %ld bytes\n", ops, bytes);

      if(Write(fh,name,bytes)!=bytes) {
        errorcode=IoErr();
        break;
      }

      size-=bytes;
    }
  }
  else {
    if(debug<=ops) printf("%6ld: SetFileSize OFFSET_END %ld\n", ops, size);

    SetFileSize(fh,size,OFFSET_END);
  }

  Close(fh);

  return(errorcode);
}



LONG delete(LONG f) {
  UBYTE name[16];

  sprintf(name,"tf%06ld.tst",f);

  if(debug<=ops) printf("%6ld: Delete %s\n", ops, name);

  if(DeleteFile(name)==0) {
    return(IoErr());
  }

  return(0);
}
