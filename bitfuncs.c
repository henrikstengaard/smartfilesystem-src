// #include <exec/types.h>
// #include <stdio.h>

#include "bitfuncs_protos.h"

/*
main() {
  ULONG bitmap[]={0x00000000,0x3fffffff,0xffffffff};

  printf("bmffo returns %ld\n",bmffo(bitmap,3,0));
  printf("bfco returns %ld\n",bfco(0xFFFFFFFF));
  printf("bfco returns %ld\n",bfco(0x0));
  printf("bfco returns %ld\n",bfco(0xAAAAAAAA));
}
*/

/* bitfield (bf) functions:

   These functions perform bit-operations on a single longword.
*/



WORD bfco(ULONG data) {
  WORD n=32;
  WORD cnt=0;

  /* Counts the number of set bits in the supplied 32-bit value */

  while(n-->0) {
    if((LONG)data<0) {
      cnt++;
    }
    data<<=1;
  }

  return(cnt);
}



WORD bfffo(ULONG data,WORD bitoffset) {
  ULONG bitmask=1<<(31-bitoffset);

  /** Finds first set bit in /data/ starting at /bitoffset/.  This function
      considers the MSB to be the first bit. */

  do {
    if((data & bitmask)!=0) {
      return(bitoffset);
    }
    bitoffset++;
    bitmask>>=1;
  } while(bitmask!=0);

  return(-1);
}


WORD bfffz(ULONG data,WORD bitoffset) {
  ULONG bitmask=1<<(31-bitoffset);

  /** Finds first zero bit in /data/ starting at /bitoffset/.  This function
      considers the MSB to be the first bit. */

  do {
    if((data & bitmask)==0) {
      return(bitoffset);
    }
    bitoffset++;
    bitmask>>=1;
  } while(bitmask!=0);

  return(-1);
}



WORD bfffz8(UBYTE data,WORD bitoffset) {
  UBYTE bitmask=1<<(7-bitoffset);

  do {
    if((data & bitmask)==0) {
      return(bitoffset);
    }
    bitoffset++;
    bitmask>>=1;
  } while(bitmask!=0);

  return(-1);
}


ULONG bfset(ULONG data,WORD bitoffset,WORD bits) {
  ULONG mask;

  /** Sets /bits/ bits starting from /bitoffset/ in /data/.
      /bits/ must be between 1 and 32. */

  /* we want a mask which sets /bits/ bits starting from bit 31 */

  mask=~((1<<(32-bits))-1);
  mask>>=bitoffset;

  return(data | mask);
}


ULONG bfclr(ULONG data,WORD bitoffset,WORD bits) {
  ULONG mask;

  /* we want a mask which sets /bits/ bits starting from bit 31 */

  mask=~((1<<(32-bits))-1);
  mask>>=bitoffset;

  return(data & ~mask);
}


/* bitmap (bm) functions:

   These functions perform bit-operations on regions of memory which
   are a multiple of 4 bytes in length.
*/


LONG bmffo(ULONG *bitmap,LONG longs,LONG bitoffset) {
  ULONG *scan=bitmap;
  ULONG longoffset;
  WORD bit;

  /* This function finds the first set bit in a region of memory starting
     with /bitoffset/.  The region of memory is /longs/ longs long.  It
     returns the bitoffset of the first set bit it finds. */

  longoffset=bitoffset>>5;
  longs-=longoffset;
  scan+=longoffset;

  bitoffset=bitoffset & 0x1F;

  if(bitoffset!=0) {
    if((bit=bfffo(*scan,bitoffset))>=0) {
      return(bit+((scan-bitmap)<<5));
    }

    scan++;
    longs--;
  }

  while(longs-->0) {
    if(*scan++!=0) {
      return(bfffo(*--scan,0)+((scan-bitmap)<<5));
    }
  }

  return(-1);
}


LONG bmffz(ULONG *bitmap,LONG longs,LONG bitoffset) {
  ULONG *scan=bitmap;
  ULONG longoffset;
  WORD bit;

  /* This function finds the first unset bit in a region of memory starting
     with /bitoffset/.  The region of memory is /longs/ longs long.  It
     returns the bitoffset of the first unset bit it finds. */

  longoffset=bitoffset>>5;
  longs-=longoffset;
  scan+=longoffset;

  bitoffset=bitoffset & 0x1F;

  if(bitoffset!=0) {
    if((bit=bfffz(*scan,bitoffset))>=0) {
      return(bit+((scan-bitmap)<<5));
    }

    scan++;
    longs--;
  }

  while(longs-->0) {
    if(*scan++!=0xFFFFFFFF) {
      return(bfffz(*--scan,0)+((scan-bitmap)<<5));
    }
  }

  return(-1);
}

LONG bmclr(ULONG *bitmap,LONG longs,LONG bitoffset,LONG bits) {
  ULONG *scan=bitmap;
  ULONG longoffset;
  LONG orgbits=bits;

  /* This function clears /bits/ bits in a region of memory starting
     with /bitoffset/.  The region of memory is /longs/ longs long.  If
     the region of memory is too small to clear /bits/ bits then this
     function exits after having cleared all bits till the end of the
     memory region.  In any case it returns the number of bits which
     were actually cleared. */

  longoffset=bitoffset>>5;
  longs-=longoffset;
  scan+=longoffset;

  bitoffset=bitoffset & 0x1F;

  if(bitoffset!=0) {
    if(bits<32) {
      *scan=bfclr(*scan,bitoffset,bits);
    }
    else {
      *scan=bfclr(*scan,bitoffset,32);
    }

    scan++;
    longs--;
    bits-=32-bitoffset;
  }

  while(bits>0 && longs-->0) {
    if(bits>31) {
      *scan++=0;
    }
    else {
      *scan=bfclr(*scan,0,bits);
    }
    bits-=32;
  }

  if(bits<=0) {
    return(orgbits);
  }
  return(orgbits-bits);
}

LONG bmset(ULONG *bitmap,LONG longs,LONG bitoffset,LONG bits) {
  ULONG *scan=bitmap;
  ULONG longoffset;
  LONG orgbits=bits;

  /* This function sets /bits/ bits in a region of memory starting
     with /bitoffset/.  The region of memory is /longs/ longs long.  If
     the region of memory is too small to set /bits/ bits then this
     function exits after having set all bits till the end of the
     memory region.  In any case it returns the number of bits which
     were actually set. */

  longoffset=bitoffset>>5;
  longs-=longoffset;
  scan+=longoffset;

  bitoffset=bitoffset & 0x1F;

  if(bitoffset!=0) {
    if(bits<32) {
      *scan=bfset(*scan,bitoffset,bits);
    }
    else {
      *scan=bfset(*scan,bitoffset,32);
    }

    scan++;
    longs--;
    bits-=32-bitoffset;
  }

  while(bits>0 && longs-->0) {
    if(bits>31) {
      *scan++=0xFFFFFFFF;
    }
    else {
      *scan=bfset(*scan,0,bits);
    }
    bits-=32;
  }

  if(bits<=0) {
    return(orgbits);
  }
  return(orgbits-bits);
}
