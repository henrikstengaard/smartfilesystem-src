/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

#include <exec/types.h>
#include <aros/macros.h>
#include "bitfuncs.h"

/* Bitmap (bm) functions:
   These functions perform bit-operations on regions of memory which
   are a multiple of 4 bytes in length. Bitmap is in bigendian byte order.
*/

/* This function finds the first set bit in a region of memory starting
   with /bitoffset/.  The region of memory is /longs/ longs long.  It
   returns the bitoffset of the first set bit it finds. */

int bmffo(ULONG *bitmap, int longs, int bitoffset)
{
	ULONG *scan = bitmap;
	int longoffset, bit;

	longoffset = bitoffset >> 5;
	longs -= longoffset;
	scan += longoffset;

	bitoffset = bitoffset & 0x1F;

	if (bitoffset != 0) {
		if ((bit = bfffo(AROS_BE2LONG(*scan), bitoffset)) >= 0) {
			return (bit + ((scan - bitmap) << 5));
		}
		scan++;
		longs--;
	}

	while (longs-- > 0) {
		if (*scan++ != 0) {
			return (bfffo(AROS_BE2LONG(*--scan), 0) + ((scan - bitmap) << 5));
		}
	}

	return (-1);
}

/* This function finds the first unset bit in a region of memory starting
   with /bitoffset/.  The region of memory is /longs/ longs long.  It
   returns the bitoffset of the first unset bit it finds. */

int bmffz(ULONG *bitmap, int longs, int bitoffset)
{
	ULONG *scan = bitmap;
	int longoffset, bit;

	longoffset = bitoffset >> 5;
	longs -= longoffset;
	scan += longoffset;

	bitoffset = bitoffset & 0x1F;

	if (bitoffset != 0) {
		if ((bit = bfffz(AROS_BE2LONG(*scan), bitoffset)) >= 0) {
			return (bit + ((scan - bitmap) << 5));
		}
		scan++;
		longs--;
	}

	while (longs-- > 0) {
		if (*scan++ != 0xFFFFFFFF) {
			return (bfffz(AROS_BE2LONG(*--scan), 0) + ((scan - bitmap) << 5));
		}
	}

	return (-1);
}

/* This function clears /bits/ bits in a region of memory starting
   with /bitoffset/.  The region of memory is /longs/ longs long.  If
   the region of memory is too small to clear /bits/ bits then this
   function exits after having cleared all bits till the end of the
   memory region.  In any case it returns the number of bits which
   were actually cleared. */

int bmclr(ULONG *bitmap, int longs, int bitoffset, int bits)
{
	ULONG *scan = bitmap;
	int longoffset;
	int orgbits = bits;

	longoffset = bitoffset >> 5;
	longs -= longoffset;
	scan += longoffset;

	bitoffset = bitoffset & 0x1F;

	if (bitoffset != 0) {
		if (bits < 32) {
			*scan = AROS_LONG2BE(bfclr(AROS_BE2LONG(*scan), bitoffset, bits));
		} else {
			*scan = AROS_LONG2BE(bfclr(AROS_BE2LONG(*scan), bitoffset, 32));
		}
		scan++;
		longs--;
		bits -= 32 - bitoffset;
	}

	while (bits > 0 && longs-- > 0) {
		if (bits > 31) {
			*scan++ = 0;
		} else {
			*scan = AROS_LONG2BE(bfclr(AROS_BE2LONG(*scan), 0, bits));
		}
		bits -= 32;
	}

	if (bits <= 0) {
		return (orgbits);
	}
	return (orgbits - bits);
}

/* This function sets /bits/ bits in a region of memory starting
   with /bitoffset/.  The region of memory is /longs/ longs long.  If
   the region of memory is too small to set /bits/ bits then this
   function exits after having set all bits till the end of the
   memory region.  In any case it returns the number of bits which
   were actually set. */

int bmset(ULONG *bitmap, int longs, int bitoffset, int bits)
{
	ULONG *scan = bitmap;
	int longoffset;
	int orgbits = bits;

	longoffset = bitoffset >> 5;
	longs -= longoffset;
	scan += longoffset;

	bitoffset = bitoffset & 0x1F;

	if (bitoffset != 0) {
		if (bits < 32) {
			*scan = AROS_LONG2BE(bfset(AROS_BE2LONG(*scan), bitoffset, bits));
		} else {
			*scan = AROS_LONG2BE(bfset(AROS_BE2LONG(*scan), bitoffset, 32));
		}
		scan++;
		longs--;
		bits -= 32 - bitoffset;
	}

	while (bits > 0 && longs-- > 0) {
		if (bits > 31) {
			*scan++ = 0xFFFFFFFF;
		} else {
			*scan = AROS_LONG2BE(bfset(AROS_BE2LONG(*scan), 0, bits));
		}
		bits -= 32;
	}

	if (bits <= 0) {
		return (orgbits);
	}
	return (orgbits - bits);
}

