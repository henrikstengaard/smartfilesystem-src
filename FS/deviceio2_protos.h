#ifndef _DEVICEIO2_PROTOS_H
#define _DEVICEIO2_PROTOS_H

#include <exec/types.h>
#include "blockstructure.h"

void update(void);
void motoroff(void);

UBYTE isdiskpresent(struct IOStdReq *ioreq);
LONG writeprotection(struct IOStdReq *ioreq);

LONG transfer(UWORD action, UBYTE *buffer, ULONG blockoffset, ULONG blocklength);

LONG initdeviceio(UBYTE *devicename, ULONG unit, ULONG flags, struct DosEnvec *de);
void cleanupdeviceio(void);

#endif // _DEVICEIO2_PROTOS_H
