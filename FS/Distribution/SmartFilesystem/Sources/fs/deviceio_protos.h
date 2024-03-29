#include <exec/types.h>

void update(void);
void motoroff(void);

UBYTE isdiskpresent(void);
LONG writeprotection(void);
ULONG deviceapiused(void);

LONG transfer(UWORD action, UBYTE *buffer, ULONG blockoffset, ULONG blocklength);

LONG initdeviceio(UBYTE *devicename, ULONG unit, ULONG flags, struct DosEnvec *de);
void cleanupdeviceio(void);

LONG addchangeint(struct Task *task, ULONG signal);
void removechangeint(void);
ULONG getchange(void);
