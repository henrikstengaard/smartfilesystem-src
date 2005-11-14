#ifndef _DEBUG_H
#define _DEBUG_H

#include <exec/types.h>

#include "debug_protos.h"
#include "fs.h"

extern ULONG mask_debug;

#ifdef DEBUGCODE

  #ifdef __AROS__

        #include <aros/debug.h>
        
        #define TDEBUG(x)                                           \
                do {                                                \
                    struct DateStamp ds;                            \
                    DateStamp(&ds);                                 \
                    bug("%4ld.%4ld ", ds.ds_Minute, ds.ds_Tick*2);  \
                    bug x;                                         \
                } while (0)

        #define DEBUG(x) bug x

        #define xdebug(type,x...)                                   \
                do {                                                \
                    ULONG debug=mask_debug;                         \
                    ULONG debugdetailed=mask_debug &                \
                        ~(DEBUG_CACHEBUFFER|DEBUG_NODES|DEBUG_LOCK|DEBUG_BITMAP);   \
                    if((debugdetailed & type)!=0 || ((type & 1)==0 && (debug & type)!=0)) \
                    {                                               \
                        bug(x);                                     \
                    }                                               \
                } while (0)
  
        #define XDEBUG(x) xdebug x
  
  
  #else

      #ifndef DEBUGKPRINTF
    
        #define TDEBUG(x) tdebug x
        #define DEBUG(x) debug x
        #define XDEBUG(x) xdebug x
    
      #else
        #define TDEBUG(x) kprintf x
        #define DEBUG(x) kprintf x
        #define XDEBUG(x) xkprintf x
    
      #endif
      
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

#endif // _DEBUG_H
