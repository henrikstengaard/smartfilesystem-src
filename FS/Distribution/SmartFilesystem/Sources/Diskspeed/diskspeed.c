#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/filehandler.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/utility.h>

#include <m68881.h>
#include <math.h>
#include <stdio.h>

struct Library *UtilityBase;
struct Library *TimerBase;
struct MsgPort *msgporttimer;
struct timerequest *iorequesttimer;

BPTR maindirlock;
BPTR oldlock;

LONG init(void);
LONG cleanup(void);

struct EClockVal starttime;
struct EClockVal endtime;
ULONG unit;
ULONG timing;

LONG test_seekread(ULONG readsize,ULONG filesize);
LONG test_read(ULONG readsize,ULONG filesize);
LONG test_delayedread(ULONG readsize,ULONG filesize);
LONG test_write(ULONG readsize);
LONG test_stationaryread(ULONG readsize);
LONG test_createfiles(void);
LONG testfiles_createfiles(ULONG filesize);
LONG testfiles_deletefiles(void);
LONG testfiles_dirscan(void);
LONG testfiles_openclose(void);
LONG testfiles_load(void);
LONG testfiles_lockunlock(void);
LONG createtestfiles(ULONG filesize);

ULONG rangerandom(ULONG);
extern ULONG __asm RANDOM(register __d0 ULONG);
ULONG rndseed=1;

UBYTE string[512];

struct {char *dir;
        ULONG all;
        ULONG create;
        ULONG delete;
        ULONG dirscan;
        ULONG openclose;
        ULONG lockunlock;
        ULONG load;
        ULONG read;
        ULONG write;
        ULONG seek;
        LONG *files;
        LONG *createsize;
        LONG *seeksize;
        LONG *readsize;
        LONG *writesize;
        ULONG nounits;
        ULONG verbose;
        ULONG noheader;} arglist={NULL};

LONG files=1000;
LONG createsize=128;
LONG seeksize=2048;
LONG readsize=65536;
LONG writesize=65536;

//void testrandom(void);

void main(void) {
  char template[]="DIRECTORY,ALL/S,CREATE/S,DELETE/S,DIRSCAN/S,OPENCLOSE/S,LOCKUNLOCK/S,LOAD/S,READ/S,WRITE/S,SEEK/S,FILES/N,CREATESIZE/N,SEEKSIZE/N,READSIZE/N,WRITESIZE/N,NOUNITS/S,VERBOSE/S,NOHEADER/S";
  struct RDArgs *readarg;

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {

//    testrandom();

    readarg=ReadArgs(template,(LONG *)&arglist,0);
    if(readarg!=0) {
      WORD parametersokay=TRUE;

      if(arglist.noheader==0) {
        printf("Diskspeed 1.2 (12 July 1998) - Programmed by John Hendrikx\n\n");
      }

      if(arglist.all!=0) {
        arglist.create=-1;
        arglist.delete=-1;
        arglist.dirscan=-1;
        arglist.openclose=-1;
        arglist.lockunlock=-1;
        arglist.load=-1;
        arglist.read=-1;
        arglist.write=-1;
        arglist.seek=-1;
      }
      else if(arglist.delete!=0 || arglist.dirscan!=0 || arglist.openclose!=0 || arglist.lockunlock!=0 || arglist.load!=0) {
        arglist.create=-1;
      }

      if(arglist.files!=0) {
        files=*arglist.files;
      }
      if(arglist.createsize!=0) {
        createsize=*arglist.createsize;
      }
      if(arglist.seeksize!=0) {
        seeksize=*arglist.seeksize;
      }
      if(arglist.readsize!=0) {
        readsize=*arglist.readsize;
      }
      if(arglist.writesize!=0) {
        writesize=*arglist.writesize;
      }

      if(files<10 || files>50000) {
        parametersokay=FALSE;
        printf("Illegal parameter value: files must be between 10 and 50000\n");
      }

      if(createsize<0 || createsize>1024*1024) {
        parametersokay=FALSE;
        printf("Illegal parameter value: CREATESIZE must be between 0 and 1048576 (1 MB)\n");
      }

      if(readsize<1 || readsize>1024*1024) {
        parametersokay=FALSE;
        printf("Illegal parameter value: READSIZE must be between 1 and 1048576 (1 MB)\n");
      }

      if(writesize<1 || writesize>1024*1024) {
        parametersokay=FALSE;
        printf("Illegal parameter value: WRITESIZE must be between 1 and 1048576 (1 MB)\n");
      }

      if(seeksize<1 || seeksize>65536) {
        parametersokay=FALSE;
        printf("Illegal parameter value: SEEKSIZE must be between 1 and 65536 (64 kB)\n");
      }


      if(parametersokay == TRUE && (UtilityBase=OpenLibrary("utility.library",37))!=0) {
        BPTR newdirlock=0;

        if(arglist.dir!=0) {
          newdirlock=Lock(arglist.dir,SHARED_LOCK);
        }

        if(arglist.dir==0 || newdirlock!=0) {

          if(arglist.dir!=0) {
            newdirlock=CurrentDir(newdirlock);
          }

          if((msgporttimer=CreateMsgPort())!=0) {
            if((iorequesttimer=CreateIORequest(msgporttimer,sizeof(struct timerequest)))!=0) {
              if(OpenDevice("timer.device",UNIT_VBLANK,&iorequesttimer->tr_node,0)==0) {
                struct InfoData id;
                BPTR lock;
                UBYTE *s;
                LONG errorcode=0;

                GetCurrentDirName(string,500);

                s=string;
                while(*s!=0 && *s++!=':') {
                }
                *s=0;

                lock=CurrentDir(0);
                Info(lock,&id);
                CurrentDir(lock);

                if(arglist.noheader==0) {
                  printf("Volume %s %ld blocks (%ld MB), %ld bytes per block.\n",string,id.id_NumBlocks,(id.id_NumBlocks*(id.id_BytesPerBlock>>9))>>11,id.id_BytesPerBlock);
                  printf("The volume has %ld buffers and the DosType is 0x%08lx.\n\n",AddBuffers(string,0),id.id_DiskType);
                }

                TimerBase=(struct Library *)iorequesttimer->tr_node.io_Device;

                init();



                if(arglist.create!=0 && (errorcode=testfiles_createfiles(createsize))==0) {
                  if(arglist.dirscan!=0 && (errorcode=testfiles_dirscan())!=0) {
                    PrintFault(errorcode,"dirscan test failed");
                  }

                  if(arglist.lockunlock!=0 && (errorcode=testfiles_lockunlock())!=0) {
                    PrintFault(errorcode,"lockunlock test failed");
                  }

                  if(arglist.openclose!=0 && (errorcode=testfiles_openclose())!=0) {
                    PrintFault(errorcode,"openclose test failed");
                  }

                  if(arglist.load!=0 && (errorcode=testfiles_load())!=0) {
                    PrintFault(errorcode,"load test failed");
                  }

                  if((errorcode=testfiles_deletefiles())!=0) {
                    PrintFault(errorcode,"deletefiles test failed");
                  }
                }
                else {
                  PrintFault(errorcode,"couldn't execute files tests");
                }


                /*

                if((errorcode=testfiles_createfiles(1024))==0) {
                  errorcode=testfiles_lockunlock();
                  if(errorcode!=0) {
                    PrintFault(errorcode,"lockunlock test failed");
                  }

                  errorcode=testfiles_openclose();
                  if(errorcode!=0) {
                    PrintFault(errorcode,"openclose test failed");
                  }

                  errorcode=testfiles_load();
                  if(errorcode!=0) {
                    PrintFault(errorcode,"openclose test failed");
                  }

                  errorcode=testfiles_deletefiles();
                  if(errorcode!=0) {
                    PrintFault(errorcode,"deletefiles test failed");
                  }
                }
                else {
                  PrintFault(errorcode,"couldn't execute files tests");
                }



                if((errorcode=testfiles_createfiles(8192))==0) {
                  errorcode=testfiles_lockunlock();
                  if(errorcode!=0) {
                    PrintFault(errorcode,"lockunlock test failed");
                  }

                  errorcode=testfiles_openclose();
                  if(errorcode!=0) {
                    PrintFault(errorcode,"openclose test failed");
                  }

                  errorcode=testfiles_load();
                  if(errorcode!=0) {
                    PrintFault(errorcode,"openclose test failed");
                  }

                  errorcode=testfiles_deletefiles();
                  if(errorcode!=0) {
                    PrintFault(errorcode,"deletefiles test failed");
                  }
                }
                else {
                  PrintFault(errorcode,"couldn't execute files tests");
                }

                */

                if(arglist.seek!=0 && (errorcode=test_seekread(seeksize,2*1024*1024))!=0) {
                  PrintFault(errorcode,"seek/read test failed");
                }


                if(arglist.read!=0 && (errorcode=test_read(readsize,2*1024*1024))!=0) {
                  PrintFault(errorcode,"read test failed");
                }

                /*

                errorcode=test_read(4096,2*1024*1024);
                if(errorcode!=0) {
                  PrintFault(errorcode,"read test failed");
                }


                errorcode=test_read(32768,2*1024*1024);
                if(errorcode!=0) {
                  PrintFault(errorcode,"read test failed");
                }


                errorcode=test_read(262144,2*1024*1024);
                if(errorcode!=0) {
                  PrintFault(errorcode,"read test failed");
                }

                */

                /*

                errorcode=test_stationaryread(262144);
                if(errorcode!=0) {
                  PrintFault(errorcode,"stationary read test failed");
                }

                */

                if(arglist.write!=0 && (errorcode=test_write(writesize))!=0) {
                  PrintFault(errorcode,"write test failed");
                }

                /*

                errorcode=test_write(4096);
                if(errorcode!=0) {
                  PrintFault(errorcode,"write test failed");
                }

                errorcode=test_write(32768);
                if(errorcode!=0) {
                  PrintFault(errorcode,"write test failed");
                }

                errorcode=test_write(262144);
                if(errorcode!=0) {
                  PrintFault(errorcode,"write test failed");
                }

                */

                cleanup();

                CloseDevice(&iorequesttimer->tr_node);
              }
              DeleteIORequest(iorequesttimer);
            }
            DeleteMsgPort(msgporttimer);
          }

          if(arglist.dir!=0) {
            newdirlock=CurrentDir(newdirlock);
          }
        }
        else {
          PutStr("Specified directory couldn't be locked!\n");
        }
        CloseLibrary((struct Library *)UtilityBase);
      }
      FreeArgs(readarg);
    }
    CloseLibrary((struct Library *)DOSBase);
  }
}

/*
void testrandom(void) {
  DOUBLE s;
  ULONG totalh=0;
  ULONG totall=0;
  ULONG cnt=1;
  ULONG seed=0,oldseed;

  for(;;) {
    s=(((DOUBLE)seed + 7.0) * 257.0);
    s=fmod(s, 2147483647.0);

    oldseed=seed;
    if((seed=RANDOM(seed))==0) {
      printf("Duplicate found after %ld randoms, seed was %ld\n",cnt,oldseed);
      break;
    }

    if(seed!=(ULONG)s) {
      printf("Seed %ld delivered %ld, while it should be %ld\n",oldseed,seed,(ULONG)s);
      break;
    }

    totall+=seed;
    if(totall<seed) {
      totalh++;
    }

    if((cnt & 0xFFFFFF)==0) {
      ULONG avg;

      avg=(totalh / (cnt>>24))<<8;
      printf("Total: 0x%08lx:%08lx, cnt = %ld, avg = %ld\n",totalh, totall, cnt, avg);
    }

    cnt++;
  }
}
*/

ULONG rangerandom(ULONG range) {
  rndseed=RANDOM(rndseed);
  return(rndseed%range);
}



LONG init() {
  if((maindirlock=CreateDir("_test_directory_"))!=0) {
    oldlock=CurrentDir(maindirlock);
    return(0);
  }
  return(IoErr());
}



LONG cleanup() {
  CurrentDir(oldlock);
  UnLock(maindirlock);
  if(DeleteFile("_test_directory_")==DOSFALSE) {
    return(IoErr());
  }
  return(0);
}



void starttimer(void) {
  Delay(5*50);

  rndseed=1;
  unit=ReadEClock(&starttime);
}



LONG finished(void) {
  ULONG diff;

  ReadEClock(&endtime);
  diff=endtime.ev_lo-starttime.ev_lo;

  if(diff/unit>=8) {
    timing=diff/unit*1000+(diff%unit * 1000)/unit;
    return(TRUE);
  }
  return(FALSE);
}



void endtimer(void) {
  ULONG diff;

  ReadEClock(&endtime);
  diff=endtime.ev_lo-starttime.ev_lo;

  timing=diff/unit*1000+(diff%unit * 1000)/unit;
}



LONG extendfile(BPTR fh,ULONG bytes) {
  LONG errorcode=0;
  UBYTE *mem;

  if((mem=AllocVec(131072,MEMF_CLEAR))!=0) {
    ULONG bytestowrite;

    while(bytes>0) {
      bytestowrite=bytes;
      if(bytestowrite>131072) {
        bytestowrite=131072;
      }

      if(Write(fh,mem,bytestowrite)!=bytestowrite) {
        errorcode=IoErr();
        break;
      }

      bytes-=bytestowrite;
    }

    FreeVec(mem);

    if(errorcode==0) {
      if(Seek(fh,0,OFFSET_BEGINNING)==-1) {
        errorcode=IoErr();
      }
    }
  }

  return(errorcode);
}



LONG deletetestfiles(void) {
  ULONG files2=files+1;
  UBYTE filename[16];

  while(--files2>0) {
    sprintf(filename,"tf%06ld.tst",files2);
    filename[12]=0;
    DeleteFile(filename);
  };

  return(0);
}



LONG createtestfiles(ULONG filesize) {
  LONG errorcode=0;
  BPTR fh;
  ULONG files2=files+1;
  UBYTE filename[16];

  while(--files2>0) {
    sprintf(filename,"tf%06ld.tst",files2);
    filename[12]=0;

    if((fh=Open(filename,MODE_NEWFILE))==0) {
      errorcode=IoErr();
      break;
    }

    if(filesize!=0) {
      if((errorcode=extendfile(fh,filesize))!=0) {
        Close(fh);
        break;
      }
    }

    Close(fh);
  }

  if(errorcode!=0) {
    deletetestfiles();
  }

  return(errorcode);
}



LONG test_seekread(ULONG readsize,ULONG filesize) {
  LONG errorcode=0;
  UBYTE *mem;
  BPTR fh;

  if(filesize<=readsize) {
    return(ERROR_NO_FREE_STORE);
  }

  if((mem=AllocVec(readsize,0))!=0) {
    if((fh=Open("seekread.tst",MODE_NEWFILE))!=0) {
      if((errorcode=extendfile(fh,filesize))==0) {
        ULONG seekreads=0;

        starttimer();

        do {
          if(Seek(fh,rangerandom(filesize-readsize),OFFSET_BEGINNING)==-1) {
            errorcode=IoErr();
            break;
          }

          if(Read(fh,mem,readsize)!=readsize) {
            errorcode=IoErr();
            break;
          }

          seekreads++;
        } while(finished()==FALSE);

        if(errorcode==0) {
          if(arglist.verbose!=0) {
            printf("Seek&Read  : Seeks to random locations in a file of %ld bytes & reads %ld bytes.\n",filesize,readsize);
            printf("             %ld seeks & reads/second (%ld seeks & reads in %ld ms)\n",seekreads*1000/timing,seekreads,timing);
          }
          else {
            printf("Random seek/reads of %5ld bytes in\n",readsize);
            printf("                   %7ld byte file : %5ld",filesize,seekreads*1000/timing);

            if(arglist.nounits==0) {
              printf(" times/second");
            }
            printf("\n");
          }
        }
      }

      Close(fh);
      DeleteFile("seekread.tst");
    }
    else {
      errorcode=IoErr();
    }
    FreeVec(mem);
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}



LONG test_read(ULONG readsize,ULONG filesize) {
  LONG errorcode=0;
  UBYTE *mem;
  BPTR fh;

  if(filesize<=readsize) {
    return(ERROR_NO_FREE_STORE);
  }

  if((mem=AllocVec(readsize,0))!=0) {
    if((fh=Open("read.tst",MODE_NEWFILE))!=0) {
      if((errorcode=extendfile(fh,filesize))==0) {
        ULONG reads=0;
        ULONG offset=0;

        starttimer();

        do {
          if(Read(fh,mem,readsize)!=readsize) {
            errorcode=IoErr();
            break;
          }

          offset+=readsize;

          if(offset+readsize>filesize) {
            if(Seek(fh,0,OFFSET_BEGINNING)==-1) {
              errorcode=IoErr();
              break;
            }
            offset=0;
          }

          reads++;
        } while(finished()==FALSE);

        if(errorcode==0) {
          if(arglist.verbose!=0) {
            printf("Read       : Reads %ld bytes at a time from a file of %ld bytes.\n",readsize,filesize);
            printf("             %ld kB/second (%ld reads in %ld ms)\n",(LONG)((double)reads*1000.0/(double)timing*(double)readsize/1024.0),reads,timing);
          }
          else {
            printf("Read data using %7ld byte buffer  : %5ld",readsize,(LONG)((double)reads*1000.0/(double)timing*(double)readsize/1024.0));

            if(arglist.nounits==0) {
              printf(" kB/second");
            }
            printf("\n");
          }
        }
      }

      Close(fh);
      DeleteFile("read.tst");
    }
    else {
      errorcode=IoErr();
    }
    FreeVec(mem);
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}



LONG test_delayedread(ULONG readsize,ULONG filesize) {
  LONG errorcode=0;
  UBYTE *mem;
  BPTR fh;

  if(filesize<=readsize) {
    return(ERROR_NO_FREE_STORE);
  }

  if((mem=AllocVec(readsize,0))!=0) {
    if((fh=Open("read.tst",MODE_NEWFILE))!=0) {
      if((errorcode=extendfile(fh,filesize))==0) {
        ULONG reads=0;
        ULONG offset=0;

        starttimer();

        do {
          if(Read(fh,mem,readsize)!=readsize) {
            errorcode=IoErr();
            break;
          }

          offset+=readsize;

          if(offset+readsize>filesize) {
            if(Seek(fh,0,OFFSET_BEGINNING)==-1) {
              errorcode=IoErr();
              break;
            }
            offset=0;
          }

          Delay(25);

          reads++;
        } while(finished()==FALSE);

        if(errorcode==0) {
          timing-=500*reads;  /* Substract the delays. */
          printf("DelayedRead: Reads %ld bytes at a time from a file of %ld bytes with a 0.5 second delay between reads.\n",readsize,filesize);
          printf("             %ld kB/second (%ld reads in %ld ms)\n",(LONG)((double)reads*1000.0/(double)timing*(double)readsize/1024.0),reads,timing);
        }
      }

      Close(fh);
      DeleteFile("read.tst");
    }
    else {
      errorcode=IoErr();
    }
    FreeVec(mem);
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}



LONG test_stationaryread(ULONG readsize) {
  LONG errorcode=0;
  UBYTE *mem;
  BPTR fh;

  if((mem=AllocVec(readsize,0))!=0) {
    if((fh=Open("statread.tst",MODE_NEWFILE))!=0) {
      if((errorcode=extendfile(fh,readsize))==0) {
        ULONG reads=0;

        starttimer();

        do {
          if(Read(fh,mem,readsize)!=readsize) {
            errorcode=IoErr();
            break;
          }

          if(Seek(fh,0,OFFSET_BEGINNING)==-1) {
            errorcode=IoErr();
            break;
          }

          reads++;
        } while(finished()==FALSE);

        if(errorcode==0) {
          printf("Stat.Read  : Reads the same %ld bytes each time.\n",readsize);
          printf("             %ld kB/second (%ld stationary reads in %ld ms)\n",(LONG)((double)reads*1000.0/(double)timing*(double)readsize/1024.0),reads,timing);
        }
      }

      Close(fh);
      DeleteFile("statread.tst");
    }
    else {
      errorcode=IoErr();
    }
    FreeVec(mem);
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}




LONG test_write(ULONG writesize) {
  LONG errorcode=0;
  UBYTE *mem;
  BPTR fh;

  if((mem=AllocVec(writesize,0))!=0) {
    if((fh=Open("write.tst",MODE_NEWFILE))!=0) {
      ULONG writes=0;

      starttimer();

      do {
        if(Write(fh,mem,writesize)!=writesize) {
          errorcode=IoErr();
          break;
        }

        writes++;
      } while(finished()==FALSE);

      if(errorcode==0) {
        if(arglist.verbose!=0) {
          printf("Write      : Writes %ld bytes at a time to a file.\n",writesize);
          printf("             %ld kB/second (%ld writes in %ld ms)\n",(LONG)((double)writes*1000.0/(double)timing*(double)writesize/1024.0),writes,timing);
        }
        else {
          printf("Write data using %7ld byte buffer : %5ld",writesize,(LONG)((double)writes*1000.0/(double)timing*(double)writesize/1024.0));

          if(arglist.nounits==0) {
            printf(" kB/second");
          }
          printf("\n");
        }
      }

      Close(fh);
      DeleteFile("write.tst");
    }
    else {
      errorcode=IoErr();
    }
    FreeVec(mem);
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}



LONG test_createfiles() {
  LONG errorcode=0;
  BPTR fh;
  ULONG files=0;
  UBYTE filename[16];

  starttimer();

  do {
    sprintf(filename,"cf%06ld.tst",files);
    filename[12]=0;

    if((fh=Open(filename,MODE_NEWFILE))==0) {
      errorcode=IoErr();
      break;
    }

    Close(fh);

    files++;
  } while(finished()==FALSE);

  if(errorcode==0) {
    printf("CreateFiles: Creates empty files.\n");
    printf("             %ld files created/second (%ld files created in %ld ms)\n",files*1000/timing,files,timing);
  }

  while(files-->0) {
    sprintf(filename,"cf%06ld.tst",files);
    filename[12]=0;
    DeleteFile(filename);
  };

  return(errorcode);
}



LONG testfiles_dirscan() {
  struct FileInfoBlock *fib;
  ULONG entries=0;
  LONG errorcode=0;

  if((fib=AllocDosObjectTags(DOS_FIB,0))!=0) {
    if(Examine(maindirlock,fib)!=DOSFALSE) {

      starttimer();

      do {
        if(ExNext(maindirlock,fib)==DOSFALSE) {
          errorcode=IoErr();
          if(errorcode!=ERROR_NO_MORE_ENTRIES) {
            break;
          }

          if(Examine(maindirlock,fib)==DOSFALSE) {
            errorcode=IoErr();
            break;
          }

          errorcode=0;
        }

        entries++;
      } while(finished()==FALSE);
    }
    else {
      errorcode=IoErr();
    }

    FreeDosObject(DOS_FIB,fib);
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  if(errorcode==0) {
    if(arglist.verbose!=0) {
      printf("DirScan    : Scans all files in a directory.\n");
      printf("             %ld files/second (%ld files in %ld ms)\n",entries*1000/timing,entries,timing);
    }
    else {
      printf("Scan all files in a directory        : %5ld",entries*1000/timing);

      if(arglist.nounits==0) {
        printf(" files/second");
      }
      printf("\n");
    }
  }

  return(errorcode);
}



LONG testfiles_openclose() {
  ULONG openscloses=0;
  LONG errorcode=0;
  UBYTE filename[16];
  BPTR fh;

  starttimer();

  do {
    sprintf(filename,"tf%06ld.tst",rangerandom(files)+1);

    if((fh=Open(filename,MODE_OLDFILE))!=0) {
      Close(fh);
    }
    else {
      errorcode=IoErr();
    }

    openscloses++;
  } while(finished()==FALSE);

  if(errorcode==0) {
    if(arglist.verbose!=0) {
      printf("Open&Close : Opens a random file and closes it again.\n");
      printf("             %ld opens & closes/second (%ld opens & closes in %ld ms)\n",openscloses*1000/timing,openscloses,timing);
    }
    else {
      printf("Open and close a random file         : %5ld",openscloses*1000/timing);

      if(arglist.nounits==0) {
        printf(" files/second");
      }
      printf("\n");
    }
  }

  return(errorcode);
}



LONG testfiles_lockunlock() {
  ULONG locksunlocks=0;
  LONG errorcode=0;
  UBYTE filename[16];
  BPTR lock;

  starttimer();

  do {
    sprintf(filename,"tf%06ld.tst",rangerandom(files)+1);

    if((lock=Lock(filename,SHARED_LOCK))!=0) {
      UnLock(lock);
    }
    else {
      errorcode=IoErr();
    }

    locksunlocks++;
  } while(finished()==FALSE);

  if(errorcode==0) {
    if(arglist.verbose!=0) {
      printf("Lock&Unlock: Locks a random file and unlocks it again.\n");
      printf("             %ld locks & unlocks/second (%ld locks & unlocks in %ld ms)\n",locksunlocks*1000/timing,locksunlocks,timing);
    }
    else {
      printf("Lock and unlock a random file        : %5ld",locksunlocks*1000/timing);

      if(arglist.nounits==0) {
        printf(" files/second");
      }
      printf("\n");
    }
  }

  return(errorcode);
}



LONG testfiles_deletefiles() {

  starttimer();

  deletetestfiles();

  endtimer();

  if(arglist.verbose!=0) {
    printf("DeleteFiles: Deletes %ld files.\n",files);
    printf("             %ld files deleted/second (%ld files deleted in %ld ms)\n",files*1000/timing,files,timing);
  }
  else {
    printf("Deletes %5ld files                  : %5ld",files,files*1000/timing);

    if(arglist.nounits==0) {
      printf(" files/second");
    }
    printf("\n");
  }

  return(0);
}



LONG testfiles_createfiles(ULONG filesize) {
  LONG errorcode;

  starttimer();

  errorcode=createtestfiles(filesize);

  endtimer();

  if(errorcode==0) {
    if(arglist.verbose!=0) {
      printf("CreateFiles: Creates %ld files of %ld bytes.\n",files,filesize);
      printf("             %ld files created/second (%ld files created in %ld ms)\n",files*1000/timing,files,timing);
    }
    else {
      printf("Creates %5ld files of %7ld bytes : %5ld",files,filesize,files*1000/timing);

      if(arglist.nounits==0) {
        printf(" files/second");
      }
      printf("\n");
    }
  }

  return(errorcode);
}



LONG testfiles_load() {
  ULONG loads=0;
  LONG errorcode=0;
  UBYTE filename[16];
  BPTR fh;
  UBYTE *mem;

  if((mem=AllocVec(131072,0))!=0) {

    starttimer();

    do {
      sprintf(filename,"tf%06ld.tst",rangerandom(files)+1);

      if((fh=Open(filename,MODE_OLDFILE))!=0) {

        while(Read(fh,mem,131072)==131072) {
        }

        Close(fh);
      }
      else {
        errorcode=IoErr();
      }

      loads++;
    } while(finished()==FALSE);

    FreeVec(mem);

    if(errorcode==0) {
      if(arglist.verbose!=0) {
        printf("Load       : Opens a random file, loads it and closes it again.\n");
        printf("             %ld loads/second (%ld loads in %ld ms)\n",loads*1000/timing,loads,timing);
      }
      else {
        printf("Load a random file                   : %5ld",loads*1000/timing);

        if(arglist.nounits==0) {
          printf(" files/second");
        }
        printf("\n");
      }
    }
  }

  return(errorcode);
}

/*

Creates 50000 files of 1048576 bytes : 10000 files/second
Open and close a random file         : 10000 files/second
Lock and unlock a random file        : 10000 files/second
Load a random file                   : 10000 files/second
Deletes 50000 files                  : 10000 files/second

Random seek/reads of 65536 bytes in
                   1048576 byte file : 10000 times/second

Read data using 1048576 byte buffer  : 10000 kB/second
Write data using 1048576 byte buffer : 10000 kB/second

*/
