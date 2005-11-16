/*
 * These functions implement bitfield and bitmap operations for any possible CPU
 * Used instead of m68k asm functions in asmsupport.s file.
 */
 
#include <exec/types.h>
#include <aros/macros.h>
#include <stdio.h>

#include "asmsupport.h"

#if defined __i386__
static inline int fls(ULONG mask)
{
    int bit = -1;

    if (mask)
    asm volatile("bsr %1, %0":"=r"(bit):"rm"(mask));

    return (bit + 1);
}

static inline int frs(ULONG mask)
{
    int bit = 0;

    if (mask)
    asm volatile("bsf %1, %0":"=r"(bit):"rm"(mask));

    return (32-bit);
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

static inline int frs(int mask)
{
    int bit;
    
    if (mask == 0)
    return (31);

    for (bit = 32; (mask & 1) != 1; bit--)
    mask = (unsigned int)mask >> 1;
    
    return (bit);
}
#endif

/* Finds first set bit in /data/ starting at /bitoffset/.  This function
   considers the MSB to be the first bit. */
inline WORD BFFFO(ULONG data, WORD bitoffset)
{
    ULONG mask = 0xffffffff >> bitoffset;
    data &= mask;
    return data == 0 ? 32 : 32-fls(data);
}

/* Finds last set bit in /data/ starting at /bitoffset/.  This function
   considers the MSB to be the first bit. */
inline WORD BFFLO(ULONG data, WORD bitoffset)
{
    ULONG mask = 0xffffffff << (32-bitoffset);
    printf("BFFLO data=%08x, offset=%d, mask=%08x\n", data, bitoffset, mask);
    data &= mask;
    return data == 0 ? 32 : frs(data)-1;
}

/* Finds last zero bit in /data/ starting at /bitoffset/.  This function
   considers the MSB to be the first bit. */
inline WORD BFFLZ(ULONG data, WORD bitoffset)
{
    return BFFLO(~data, bitoffset);
}

/* Finds first zero bit in /data/ starting at /bitoffset/.  This function
   considers the MSB to be the first bit. */
inline WORD BFFFZ(ULONG data, WORD bitoffset)
{
    return BFFFO(~data, bitoffset);
}

/* Sets /bits/ bits starting from /bitoffset/ in /data/.
   /bits/ must be between 1 and 32. */
inline ULONG BFSET(ULONG data, WORD bitoffset, WORD bits)
{
    ULONG mask = ~((1 << (32 - bits)) - 1);
    mask >>= bitoffset;
    return data | mask;
}

/* Clears /bits/ bits starting from /bitoffset/ in /data/.
   /bits/ must be between 1 and 32. */
inline ULONG BFCLR(ULONG data, WORD bitoffset, WORD bits)
{
    ULONG mask = ~((1 << (32 - bits)) - 1);
    mask >>= bitoffset;
    return data & ~mask;
}

ULONG BFCNTO(ULONG v)
{
    ULONG const w = v - ((v >> 1) & 0x55555555);                    // temp
    ULONG const x = (w & 0x33333333) + ((w >> 2) & 0x33333333);     // temp
    ULONG const c = ((x + (x >> 4) & 0xF0F0F0F) * 0x1010101) >> 24; // count
    
    return c;
}

ULONG BFCNTZ(ULONG v)
{
    return BFCNTO(~v);
}

LONG BMFLO(ULONG *bitmap, LONG bitoffset)
{
    ULONG *scan = bitmap;
    LONG longs; 
    int longoffset, bit;

    longoffset = (bitoffset) >> 5;
    longs = longoffset;
    scan += longoffset;

    printf("BMFLO longoffset=%d, bitoffset=%d\n", longs, bitoffset);

    bitoffset = bitoffset & 0x1F;


    if (bitoffset != 0) {
        if ((bit = BFFLO(AROS_BE2LONG(*scan), bitoffset)) != 32) {
            return (bit + ((scan - bitmap) << 5));
        }
        scan--;
        longs--;
    }

    while (longs-- > 0) {
        if (*scan-- != 0) {
            return (BFFLO(AROS_BE2LONG(*++scan),32) + ((scan - bitmap) << 5));
        }
    }

    return (-1);
}

LONG BMFLZ(ULONG *bitmap, LONG bitoffset)
{
    ULONG *scan = bitmap;
    LONG longs;
    int longoffset, bit;

    longoffset = (bitoffset) >> 5;
    longs = longoffset;
    scan += longoffset;

    bitoffset = bitoffset & 0x1F;

    if (bitoffset != 0) {
        if ((bit = BFFLZ(AROS_BE2LONG(*scan), bitoffset)) != 32) {
            return (bit + ((scan - bitmap) << 5));
        }
        scan--;
        longs--;
    }

    while (longs-- > 0) {
        if (*scan-- != 0xFFFFFFFF) {
            return (BFFLZ(AROS_BE2LONG(*++scan),32) + ((scan - bitmap) << 5));
        }
    }

    return (-1);
}

/* This function finds the first set bit in a region of memory starting
   with /bitoffset/.  The region of memory is /longs/ longs long.  It
   returns the bitoffset of the first set bit it finds. */

LONG BMFFO(ULONG *bitmap, LONG longs, LONG bitoffset)
{
    ULONG *scan = bitmap;
    int longoffset, bit;

    longoffset = bitoffset >> 5;
    longs -= longoffset;
    scan += longoffset;

    bitoffset = bitoffset & 0x1F;

    if (bitoffset != 0) {
        if ((bit = BFFFO(AROS_BE2LONG(*scan), bitoffset)) != 32) {
            return (bit + ((scan - bitmap) << 5));
        }
        scan++;
        longs--;
    }

    while (longs-- > 0) {
        if (*scan++ != 0) {
            return (BFFFO(AROS_BE2LONG(*--scan),0) + ((scan - bitmap) << 5));
        }
    }

    return (-1);
}

/* This function finds the first unset bit in a region of memory starting
   with /bitoffset/.  The region of memory is /longs/ longs long.  It
   returns the bitoffset of the first unset bit it finds. */

LONG BMFFZ(ULONG *bitmap, LONG longs, LONG bitoffset)
{
    ULONG *scan = bitmap;
    int longoffset, bit;

    longoffset = bitoffset >> 5;
    longs -= longoffset;
    scan += longoffset;

    bitoffset = bitoffset & 0x1F;

    if (bitoffset != 0) {
        if ((bit = BFFFZ(AROS_BE2LONG(*scan), bitoffset)) != 32) {
            return (bit + ((scan - bitmap) << 5));
        }
        scan++;
        longs--;
    }

    while (longs-- > 0) {
        if (*scan++ != 0xFFFFFFFF) {
            return (BFFFZ(AROS_BE2LONG(*--scan),0) + ((scan - bitmap) << 5));
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

LONG BMCLR(ULONG *bitmap, LONG longs, LONG bitoffset, LONG bits)
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
            *scan = AROS_LONG2BE(BFCLR(AROS_BE2LONG(*scan), bitoffset, bits));
        } else {
            *scan = AROS_LONG2BE(BFCLR(AROS_BE2LONG(*scan), bitoffset, 32));
        }
        scan++;
        longs--;
        bits -= 32 - bitoffset;
    }

    while (bits > 0 && longs-- > 0) {
        if (bits > 31) {
            *scan++ = 0;
        } else {
            *scan = AROS_LONG2BE(BFCLR(AROS_BE2LONG(*scan), 0, bits));
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

LONG BMSET(ULONG *bitmap, LONG longs, LONG bitoffset, LONG bits)
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
            *scan = AROS_LONG2BE(BFSET(AROS_BE2LONG(*scan), bitoffset, bits));
        } else {
            *scan = AROS_LONG2BE(BFSET(AROS_BE2LONG(*scan), bitoffset, 32));
        }
        scan++;
        longs--;
        bits -= 32 - bitoffset;
    }

    while (bits > 0 && longs-- > 0) {
        if (bits > 31) {
            *scan++ = 0xFFFFFFFF;
        } else {
            *scan = AROS_LONG2BE(BFSET(AROS_BE2LONG(*scan), 0, bits));
        }
        bits -= 32;
    }

    if (bits <= 0) {
        return (orgbits);
    }
    return (orgbits - bits);
}

BOOL BMTSTO(ULONG *bitmap, LONG bitoffset, LONG bits)
{
    LONG longoffset = bitoffset >> 5;
    ULONG *scan = bitmap;
    ULONG mask;
    
    scan += longoffset;
    bitoffset &= 0x1f;
    
    if (bitoffset != 0)
    {
        if ((bits + bitoffset) < 32)
        {
            mask = (0xffffffff >> bitoffset) & (0xffffffff << (32 - (bits+bitoffset)));
            bits=0;
        }
        else
        {
            mask = (0xffffffff >> bitoffset);
            bits -= (32-bitoffset);
        }

        if ((mask & AROS_BE2LONG(*scan++)) != mask)
            return FALSE;
    }
    
    while (bits > 0)
    {
        if (bits >= 32)
        {
            mask=0xffffffff;            
            bits -= 32;
        }
        else
        {
            mask = 0xffffffff << (32-bits);
            bits = 0;
        }
        if ((mask & AROS_BE2LONG(*scan++)) != mask)
            return FALSE;
    }
    
    return TRUE;
}

BOOL BMTSTZ(ULONG *bitmap, LONG bitoffset, LONG bits)
{
    LONG longoffset = bitoffset >> 5;
    ULONG *scan = bitmap;
    ULONG mask;
        
    scan += longoffset;
    bitoffset &= 0x1f;
    
    if (bitoffset != 0)
    {
        if ((bits + bitoffset) < 32)
        {
            mask = (0xffffffff >> bitoffset) & (0xffffffff << (32 - (bits+bitoffset)));
            bits=0;
        }
        else
        {
            mask = (0xffffffff >> bitoffset);
            bits -= (32-bitoffset);
        }
        if ((mask & AROS_BE2LONG(*scan++)) != 0)
            return FALSE;
    }
    
    while (bits > 0)
    {
        if (bits >= 32)
        {
            mask=0xffffffff;
            bits -= 32;
        }
        else
        {
            mask = 0xffffffff << (32-bits);
            bits = 0;
        }
        if ((mask & AROS_BE2LONG(*scan++)) != 0)
            return FALSE;
    }
    
    return TRUE;
}

ULONG BMCNTO(ULONG *bitmap, LONG bitoffset, LONG bits)
{
    LONG longoffset = bitoffset >> 5;
    ULONG *scan = bitmap;
    ULONG count = 0;
    ULONG mask;
    
    scan += longoffset;
    bitoffset &= 0x1f;
    
    if (bitoffset != 0)
    {
        if ((bits + bitoffset) < 32)
        {
            mask = (0xffffffff >> bitoffset) & (0xffffffff << (32 - (bits+bitoffset)));
            bits=0;
        }
        else
        {
            mask = (0xffffffff >> bitoffset);
            bits -= (32-bitoffset);
        }        
        count += BFCNTO(AROS_BE2LONG(*scan++) & mask);
    }
    
    while (bits > 0)
    {
        if (bits >= 32)
        {
            mask=0xffffffff;
            bits -= 32;
        }
        else
        {
            mask = 0xffffffff << (32-bits);
            bits = 0;
        }
        count += BFCNTO(AROS_BE2LONG(*scan++) & mask);        
    }
    
    return count;
}

ULONG BMCNTZ(ULONG *bitmap, LONG bitoffset, LONG bits)
{
    LONG longoffset = bitoffset >> 5;
    ULONG *scan = bitmap;
    ULONG count = 0;
    ULONG mask;
    
    scan += longoffset;
    bitoffset &= 0x1f;
    
    if (bitoffset != 0)
    {
        if ((bits + bitoffset) < 32)
        {
            mask = ~((0xffffffff >> bitoffset) & (0xffffffff << (32 - (bits+bitoffset))));
            bits=0;
        }
        else
        {
            mask = ~(0xffffffff >> bitoffset);
            bits -= (32-bitoffset);
        }
        
        count += BFCNTZ(AROS_BE2LONG(*scan++) | mask);
    }
    
    while (bits > 0)
    {
        if (bits >= 32)
        {
            mask=0;        
            bits -= 32;
        }
        else
        {
            mask = ~(0xffffffff << (32-bits));
            bits = 0;
        }

        count += BFCNTZ(AROS_BE2LONG(*scan++) | mask);
    }
    
    return count;
}
