#include <exec/types.h>
#include <stdio.h>

#define DISKSIZE 128
#define EMPTY -1
#define MODES 2

BYTE disk[DISKSIZE];
LONG files[20];
LONG mode=1;
LONG rovingptr;

ULONG seed=0;

ULONG rnd(void) {
  seed=(seed+23)*257 % 65537;

  return seed;
}



void printdisk(void) {
  LONG n;
  LONG fsfrags=1, frags=0, filesnr=0, filesfragged=0;
  BYTE last=disk[0];

  for(n=0; n<20; n++) {
    files[n]=0;
  }

  for(n=0; n<DISKSIZE; n++) {
    if(disk[n]==EMPTY) {
      printf(".");
    }
    else {
      printf("%c", disk[n]+48);
    }

    if(disk[n]!=EMPTY) {
      if(files[disk[n]]==0) {
        files[disk[n]]=1;
        filesnr++;
      }
      else if(last!=disk[n] && files[disk[n]]!=2) {
        files[disk[n]]=2;
        filesfragged++;
      }
    }

    if(last!=disk[n]) {
      frags++;
      if(last==EMPTY) {
        fsfrags++;
      }
    }

    last=disk[n];
  }

  printf("\nMode=%ld: free space fragments: %2ld  fragments: %2ld  #files: %2ld  fragments-#files: %2ld  files fragged: %2ld\n", mode, fsfrags, frags, filesnr, frags-filesnr, filesfragged);
}



ULONG freespace(ULONG block, ULONG *minsize) {
  ULONG n;
  ULONG found=0;
  ULONG largest_block=0;
  ULONG largest_found=0;

  /* Returns the first free space that meets the requirements;
     it will return the next smaller space if minsize cannot be
     found. */

  for(n=block; n<DISKSIZE; n++) {
    if(disk[n]==EMPTY) {
      if(++found==*minsize) {
        return(n-found+1);
      }
    }
    else {
      if(found>largest_found) {
        largest_found=found;
        largest_block=n-found;
      }
      found=0;
    }
  }

  if(block!=0) {
    if(found>largest_found) {
      largest_found=found;
      largest_block=n-found;
    }
    found=0;

    for(n=0; n<block; n++) {
      if(disk[n]==EMPTY) {
        if(++found==*minsize) {
          return(n-found+1);
        }
      }
      else {
        if(found>largest_found) {
          largest_found=found;
          largest_block=n-found+1;
        }
        found=0;
      }
    }
  }

  *minsize=largest_found;
  return(largest_block);
}



void close(ULONG file) {
  switch(mode) {
  case 2:
    {
      rovingptr=files[file];
    }
    break;
  }
}

void write(ULONG file, ULONG blocks) {

  switch(mode) {
  case 1:
    {
      ULONG startblock;
      LONG n;

      /* Standard writing as used in SFS from the beginning. */

      if(files[file]==-1 || disk[files[file]]!=EMPTY) {
        startblock=rovingptr;
      }
      else {
        startblock=files[file];
      }

      while(blocks>0) {
        ULONG space=blocks;

        startblock=freespace(startblock, &space);

        for(n=startblock; n<startblock+space; n++) {
          disk[n]=file;
        }

        blocks-=space;
        startblock+=space;
      }

      files[file]=startblock;
      rovingptr=startblock;
    }
    break;
  case 2:
    {
      ULONG startblock;
      LONG n;
      BYTE usedroving=FALSE;

//      printf("writing file %ld.  files[file] = %ld, disk[files[file]] = %ld\n", file, files[file], disk[files[file]]);

      if(files[file]==-1 || disk[files[file]]!=EMPTY) {
        startblock=rovingptr;
        usedroving=TRUE;
      }
      else {
        startblock=files[file];
      }

      while(blocks>0) {
        ULONG space=blocks;

        startblock=freespace(startblock, &space);

        for(n=startblock; n<startblock+space; n++) {
          disk[n]=file;
        }

        blocks-=space;
        startblock+=space;
      }

      files[file]=startblock;
      if(usedroving==TRUE) {
        rovingptr=startblock+3;
      }
    }
    break;
  }
}


void cleardisk(void) {
  LONG n;

  for(n=0; n<DISKSIZE; n++) {
    disk[n]=EMPTY;
  }

  for(n=0; n<20; n++) {
    files[n]=-1;
  }

  rovingptr=0;
  seed=0;
}


void RndInTurnWriting(WORD tasks) {
  printf("\nRndInTurnWriting %ld tasks\n", tasks);
  for(mode=1; mode<MODES+1; mode++) {
    LONG n;

    cleardisk();

    for(n=0; n<60; n++) {
      write(rnd() % tasks, 1);
    }
    printdisk();
  }
}


void InTurnWriting(WORD tasks) {
  printf("\nInTurnWriting %ld tasks\n", tasks);
  for(mode=1; mode<MODES+1; mode++) {
    LONG n;

    cleardisk();

    for(n=0; n<60; n++) {
      write(n % tasks, 1);
    }
    printdisk();
  }
}


void InTurnWriting2(WORD tasks) {
  printf("\nInTurnWriting2 %ld tasks\n", tasks);
  for(mode=1; mode<MODES+1; mode++) {
    LONG n;
    LONG x=0;

    cleardisk();

    for(n=0; n<60; n++) {
      write(n % tasks + x, 1);
      if((n+1)%7==0) {
        close(x);
        x++;
      }
    }
    printdisk();
  }
}


void SingleFileWriting(void) {
  printf("\nSingleFileWriting\n");
  for(mode=1; mode<MODES+1; mode++) {
    LONG n;

    cleardisk();

    for(n=0; n<10; n++) {
      write(n, 1);
      close(n);
    }
    printdisk();
  }
}



void main() {
  InTurnWriting(2);
  InTurnWriting2(2);
  RndInTurnWriting(2);
  SingleFileWriting();
}

