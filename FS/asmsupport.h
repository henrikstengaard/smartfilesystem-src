#ifndef ASMSUPPORT_H_
#define ASMSUPPORT_H_

#include <exec/types.h>

inline WORD BFFFO(ULONG data, WORD bitoffset);
inline WORD BFFLO(ULONG data, WORD bitoffset);
inline WORD BFFLZ(ULONG data, WORD bitoffset);
inline WORD BFFFZ(ULONG data, WORD bitoffset);
inline ULONG BFSET(ULONG data, WORD bitoffset, WORD bits);
inline ULONG BFCLR(ULONG data, WORD bitoffset, WORD bits);
ULONG BFCNTO(ULONG v);
ULONG BFCNTZ(ULONG v);
LONG BMFLO(ULONG *bitmap, LONG bitoffset);
LONG BMFLZ(ULONG *bitmap, LONG bitoffset);
LONG BMFFO(ULONG *bitmap, LONG longs, LONG bitoffset);
LONG BMFFZ(ULONG *bitmap, LONG longs, LONG bitoffset);
LONG BMCLR(ULONG *bitmap, LONG longs, LONG bitoffset, LONG bits);
LONG BMSET(ULONG *bitmap, LONG longs, LONG bitoffset, LONG bits);
BOOL BMTSTO(ULONG *bitmap, LONG bitoffset, LONG bits);
BOOL BMTSTZ(ULONG *bitmap, LONG bitoffset, LONG bits);
ULONG BMCNTO(ULONG *bitmap, LONG bitoffset, LONG bits);
ULONG BMCNTZ(ULONG *bitmap, LONG bitoffset, LONG bits);

#endif /*ASMSUPPORT_H_*/
