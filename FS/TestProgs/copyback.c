#include <dos/dos.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

/* Copyback test program */


UBYTE *createtestdata(LONG size);
void freetestdata(UBYTE *buf);
LONG verifydata(BPTR fh,UBYTE *buf,LONG size,LONG step);
LONG writedata(BPTR fh,UBYTE *buf,LONG size,LONG step);
void dumpdata(void *data, LONG bytes);

#define SIZES 34
#define STEPS 11

LONG sizes[SIZES]={1,2,3,13,257,511,512,513,777,1000,1023,1024,1025,1111,2000,2047,2048,2049,2222,3333,4095,4096,4097,4444,6000,6666,8191,8192,8193,9999,12121,16384,23333,32768};
LONG step[STEPS]={1,2,3,111,512,777,1025,4096,7777,16384,33000};

main() {
  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    LONG errorcode=0;
    WORD n;

    for(n=0; n<SIZES; n++) {
      UBYTE *buf;

      if((buf=createtestdata(sizes[n]))!=0) {
        BPTR fh;
        WORD m;

        for(m=0; m<STEPS; m++) {
          if((fh=Open("copyback$testfile",MODE_NEWFILE))!=0) {
            if((errorcode=writedata(fh,buf,sizes[n],step[m]))!=0) {
              PrintFault(errorcode,"copyback - error while writing data");
              Close(fh);
              DeleteFile("copyback$testfile");
              break;
            }

            Close(fh);

            if((fh=Open("copyback$testfile",MODE_OLDFILE))!=0) {
              if((errorcode=verifydata(fh,buf,sizes[n],step[STEPS-m-1]))!=0) {
                if(errorcode==-1) {
                  printf("copyback - verify error: data read doesn't match data written (size = %ld, step = %ld/%ld)\n",sizes[n],step[m],step[STEPS-m-1]);
//                  dumpdata(buf,sizes[n]);
                }
                else {
                  PrintFault(errorcode,"copyback - error while reading data");
                }
                Close(fh);
                // DeleteFile("copyback$testfile");
                break;
              }

              Close(fh);
              DeleteFile("copyback$testfile");
            }
            else {
              PrintFault(IoErr(),"copyback - error while opening old file");
              DeleteFile("copyback$testfile");
              break;
            }
          }
          else {
            PrintFault(IoErr(),"copyback - error while creating new file");
            DeleteFile("copyback$testfile");
            break;
          }
        }

        freetestdata(buf);
      }

      if(errorcode!=0) {
        break;
      }
    }

    CloseLibrary((struct Library *)DOSBase);
  }
}


UBYTE *createtestdata(LONG size) {
  UBYTE *buf;

  if((buf=AllocVec(size,MEMF_CLEAR))!=0) {
    UBYTE *b=buf;
    UBYTE c=96;

    while(size-->0) {
      *b++=c;

      if(c=='z') {
        c=96;
      }
      else {
        c++;
      }
    }
  }

  return(buf);
}


void freetestdata(UBYTE *buf) {
  FreeVec(buf);
}


LONG writedata(BPTR fh,UBYTE *buf,LONG size,LONG step) {
  LONG errorcode=0;
  LONG s;

  while(size>0) {
    s=step;

    if(s>size) {
      s=size;
    }

    if(Write(fh,buf,s)!=s) {
      errorcode=IoErr();
      break;
    }

    buf+=step;
    size-=step;
  }

  return(errorcode);
}


LONG verifydata(BPTR fh,UBYTE *buf,LONG size,LONG step) {
  UBYTE *readbuf;
  UBYTE *r;
  LONG s;
  LONG errorcode=0;
  LONG size2=size;

  if((readbuf=AllocVec(step,MEMF_CLEAR))==0) {
    return(ERROR_NO_FREE_STORE);
  }

  while(errorcode==0 && size>0) {
    r=readbuf;
    s=step;

    if(s>size) {
      s=size;
    }

    if(Read(fh,readbuf,s)!=s) {
      errorcode=IoErr();
      break;
    }


    while(s-->0) {
      if(*r++!=*buf++) {
        errorcode=-1;
        break;
      }
    }

    if(errorcode==-1) {
      printf("copyback - verify error at %ld (block offset is %ld).\n",r-readbuf,size2-size);
      dumpdata(readbuf,step);
    }

    size-=step;
  }

  FreeVec(readbuf);

  return(errorcode);
}



void dumpdata(void *data, LONG bytes) {
  ULONG *d=(ULONG *)data;
  UBYTE *d2=(UBYTE *)data;
  UWORD off=0;
  UBYTE s[40];

  while(bytes>0) {
    WORD n;
    UBYTE c;
    UBYTE *s2;

    n=16;
    s2=s;

    while(--n>=0) {
      c=*d2++;

      if(c<32) {
        c+=64;
      }
      if(c>=127 && c<=160) {
        c='.';
      }

      *s2++=c;
    }
    *s2=0;

    printf("0x%04lx: %08lx %08lx %08lx %08lx %s\n",off,d[0],d[1],d[2],d[3],s);

    bytes-=16;
    d+=4;
    off+=16;
  }
}
