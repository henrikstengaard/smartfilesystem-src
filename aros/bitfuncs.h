/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

#ifndef __BITFUNCS_H
#define __BITFUNCS_H

#include <exec/types.h>

#if defined __i386__
static inline int fls(ULONG mask)
{
    int bit = -1;

    if (mask)
	asm volatile("bsr %1, %0":"=r"(bit):"rm"(mask));

    return (bit + 1);
}
#else
static inline int fls(int mask)
{
    int bit;
    
    if (mask == 0)
	return (0);

    for (bit = 1; mask != 1; bit++)
	mask = (unsigned int)mask >> 1;
    
    return (bit);
}
#endif

/* Finds first set bit in /data/ starting at /bitoffset/.  This function
   considers the MSB to be the first bit. */
static inline int bfffo(ULONG data, int bitoffset)
{
	ULONG mask = 0xffffffff >> bitoffset;
	data &= mask;
	return data == 0 ? -1 : 32-fls(data);
}

/* Finds first zero bit in /data/ starting at /bitoffset/.  This function
   considers the MSB to be the first bit. */
static inline int bfffz(ULONG data, int bitoffset)
{
	return bfffo(~data, bitoffset);
}

/* Sets /bits/ bits starting from /bitoffset/ in /data/.
   /bits/ must be between 1 and 32. */
static inline ULONG bfset(ULONG data, int bitoffset, int bits)
{
	ULONG mask = ~((1 << (32 - bits)) - 1);
	mask >>= bitoffset;
	return data | mask;
}

/* Clears /bits/ bits starting from /bitoffset/ in /data/.
   /bits/ must be between 1 and 32. */
static inline ULONG bfclr(ULONG data, int bitoffset, int bits)
{
	ULONG mask = ~((1 << (32 - bits)) - 1);
	mask >>= bitoffset;
	return data & ~mask;
}

/* bm??? functions assumes that in-memory bitmap is in bigendian byte order */
int bmffo(ULONG *, int, int);
int bmffz(ULONG *, int, int);
int bmclr(ULONG *, int, int, int);
int bmset(ULONG *, int, int, int);

#endif

