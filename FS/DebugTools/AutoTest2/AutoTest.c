#include <dos/dos.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "stdio.h"

#define FILES        (15)
#define MAXSIZE      (524588)
#define MAXWRITESIZE (16384)

/* ASM prototypes */

extern ULONG __asm RANDOM(register __d0 ULONG);
extern LONG __asm STACKSWAP(void);
extern ULONG __asm CALCCHECKSUM(register __d0 ULONG,register __a0 ULONG *);
extern ULONG __asm MULU64(register __d0 ULONG,register __d1 ULONG,register __a0 ULONG *);
extern WORD __asm COMPRESSFROMZERO(register __a0 UWORD *,register __a1 UBYTE *,register __d0 ULONG);

ULONG rangerandom(ULONG);
LONG create(LONG file);
LONG delete(LONG f);
LONG extend(LONG f);
LONG writerandom(LONG file);
LONG verifyrandom(LONG file);
LONG changesizerandom(LONG file);

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

struct {char *dir;} arglist={NULL};

LONG filepos[FILES];
LONG filesize[FILES];
BPTR fh[FILES];
UBYTE *buffer[FILES];

UBYTE verifybuffer[MAXWRITESIZE];

LONG main() {
  struct RDArgs *readarg;
  UBYTE template[]="DIR/A\n";

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      if((buffer[0]=AllocVec(MAXSIZE*FILES, MEMF_CLEAR))!=0) {
        LONG errorcode=0;
        BPTR newdirlock;
        LONG ops=0;
        LONG n;

        newdirlock=Lock(arglist.dir,SHARED_LOCK);
        newdirlock=CurrentDir(newdirlock);

        for(n=0; n<FILES; n++) {
          buffer[n]=buffer[0]+n*MAXSIZE;

          if((errorcode=create(n))!=0) {
            break;
          }
        }

        if(errorcode==0) {
          for(;;) {
            LONG r=rangerandom(2000);

            if(rangerandom(5000)==0) {
              Delay(200);
            }

            ops++;

            if(r>1010) {
              if((errorcode=verifyrandom(rangerandom(FILES)))!=0) {
                PrintFault(errorcode, "error during verifying");
                break;
              }
            }
            else if(r>20) {
              if((errorcode=writerandom(rangerandom(FILES)))!=0) {
                PrintFault(errorcode, "error during writing");
                break;
              }
            }
            else {
              if((errorcode=changesizerandom(rangerandom(FILES)))!=0) {
                PrintFault(errorcode, "error during size changing");
                break;
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

          if(errorcode!=0){
            printf("file     size     offset\n");
            for(n=0; n<FILES; n++) {
              printf("%2ld: %9ld  %9ld\n", n, filesize[n], filepos[n]);
            }
          }
        }

        for(n=0; n<FILES; n++) {
          Close(fh[n]);
        }

        newdirlock=CurrentDir(newdirlock);
        UnLock(newdirlock);

        FreeVec(buffer[0]);
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


LONG extend(LONG f) {
  BPTR fh;
  UBYTE name[16];
  LONG errorcode=0;
  LONG size=rangerandom(16384);

  sprintf(name,"tf%06ld.tst",f);

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

      if(Write(fh,name,bytes)!=bytes) {
        errorcode=IoErr();
        break;
      }

      size-=bytes;
    }
  }
  else {
    SetFileSize(fh,size,OFFSET_END);
  }

  Close(fh);

  return(errorcode);
}



LONG delete(LONG f) {
  UBYTE name[16];

  sprintf(name,"tf%06ld.tst",f);

  if(DeleteFile(name)==0) {
    return(IoErr());
  }

  return(0);
}





void randomizedata(UBYTE *data, LONG bytes) {
  UBYTE b;
  UBYTE r=rangerandom(256);

  while(--bytes>=0) {
    b=*data;
    b++;
    *data++=b^r;
  }
}



LONG seek(LONG file, LONG offset) {
  UBYTE i=rangerandom(3);
  LONG errorcode=0;

  /* Seeks in 3 different ways. */

  if(i==0) {
    if(Seek(fh[file], offset, OFFSET_BEGINNING)==-1) {
      errorcode=IoErr();
      printf("File %ld: offset = %ld, OFFSET_BEGINNING\n", file, offset);
    }
  }
  else if(i==1) {
    if(Seek(fh[file], offset-filesize[file], OFFSET_END)==-1) {
      errorcode=IoErr();
      printf("File %ld: offset = %ld, OFFSET_END\n", file, offset);
    }
  }
  else {
    if(Seek(fh[file], offset-filepos[file], OFFSET_CURRENT)==-1) {
      errorcode=IoErr();
      printf("File %ld: offset = %ld, OFFSET_CURRENT\n", file, offset);
    }
  }

  if(errorcode==0) {
    filepos[file]=offset;
  }

  return(errorcode);
}



LONG writerandom(LONG file) {
  LONG size=rangerandom(MAXWRITESIZE-1)+1;
  LONG offset;
  LONG errorcode;

  if(size>filesize[file]) {
    size=filesize[file];
  }

  offset=rangerandom(filesize[file]-size+1);

  /* Writes a random number of bytes to a random location
     in the file. */

  if((errorcode=seek(file, offset))==0) {
    randomizedata(buffer[file]+offset, size);
    if(Write(fh[file], buffer[file]+offset, size)!=size) {
      errorcode=IoErr();
    }
    filepos[file]=offset+size;
  }

  return(errorcode);
}



LONG verifyrandom(LONG file) {
  LONG size=rangerandom(MAXWRITESIZE-1)+1;
  LONG offset;
  LONG errorcode;

  if(size>filesize[file]) {
    size=filesize[file];
  }

  offset=rangerandom(filesize[file]-size+1);

  /* Reads a random number of bytes from a random location
     in the file and checks them. */

  if((errorcode=seek(file, offset))==0) {
    if(Read(fh[file], verifybuffer, size)!=size) {
      errorcode=IoErr();
    }
    filepos[file]=offset+size;

    if(errorcode==0) {
      UBYTE *a=verifybuffer;
      UBYTE *b=buffer[file]+offset;
      LONG s=size;

      while(--s>=0 && *a++==*b++) {
      }

      if(s>=0) {
        printf("File %ld: Verify error at offset %ld while verifying %ld bytes at %ld.\n",file , offset+(a-verifybuffer), size, offset);
        errorcode=1;
      }
    }
  }

  return(errorcode);
}



LONG changesizerandom(LONG file) {
  LONG newsize=rangerandom(MAXSIZE-1)+1;
  LONG errorcode=0;

  if(SetFileSize(fh[file], newsize, OFFSET_BEGINNING)==-1) {
    errorcode=IoErr();
  }
  else {
    LONG oldsize=filesize[file];

    filesize[file]=newsize;
    if(newsize<filepos[file]) {
      if(Seek(fh[file], 0, OFFSET_BEGINNING)==-1) {
        errorcode=IoErr();
      }
      else {
        filepos[file]=0;
      }
    }

    if(newsize>oldsize) {
      if((errorcode=seek(file, oldsize))==0) {
        if(Write(fh[file], buffer[file]+oldsize, newsize-oldsize)!=newsize-oldsize) {
          errorcode=IoErr();
        }
        else {
          filepos[file]=newsize;
        }
      }
    }
  }

  return(errorcode);
}



LONG create(LONG file) {
  UBYTE name[16];
  LONG errorcode=0;

  sprintf(name,"tf%05ld.tst",file);

  if((fh[file]=Open(name,MODE_NEWFILE))==0) {
    return(IoErr());
  }

  if(Write(fh[file], buffer[file], MAXSIZE/10)!=MAXSIZE/10) {
    errorcode=IoErr();
    Close(fh[file]);
  }
  else {
    filepos[file]=MAXSIZE/10;
    filesize[file]=MAXSIZE/10;
  }

  return(errorcode);
}
