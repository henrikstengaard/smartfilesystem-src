#include "debug_protos.h"
#include "fs.h"

#ifdef DEBUGCODE

  #ifndef DEBUGKPRINTF

    #define TDEBUG(x) tdebug x
    #define DEBUG(x) debug x
    #define XDEBUG(x) xdebug x

  #else
    #define TDEBUG(x) kprintf x
    #define DEBUG(x) kprintf x
    #define XDEBUG(x) xkprintf x

  #endif

#else

  #define TDEBUG(x)
  #define DEBUG(x)
  #define XDEBUG(x)

#endif


#define DEBUG_DETAILED (1)

#define DEBUG_CACHEBUFFER (2)
#define DEBUG_NODES       (4)
#define DEBUG_IO          (8)
#define DEBUG_SEEK        (16)
#define DEBUG_BITMAP      (32)
#define DEBUG_LOCK        (64)
#define DEBUG_OBJECTS     (128)
#define DEBUG_TRANSACTION (256)

#define DDEBUG_CACHEBUFFER (DEBUG_DETAILED + DEBUG_CACHEBUFFER)
#define DDEBUG_NODES       (DEBUG_DETAILED + DEBUG_NODES)
#define DDEBUG_IO          (DEBUG_DETAILED + DEBUG_IO)
#define DDEBUG_SEEK        (DEBUG_DETAILED + DEBUG_SEEK)
#define DDEBUG_BITMAP      (DEBUG_DETAILED + DEBUG_BITMAP)
#define DDEBUG_LOCK        (DEBUG_DETAILED + DEBUG_LOCK)
#define DDEBUG_OBJECTS     (DEBUG_DETAILED + DEBUG_OBJECTS)
#define DDEBUG_TRANSACTION (DEBUG_DETAILED + DEBUG_TRANSACTION)
