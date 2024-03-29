
/* Tags used by ACTION_SFS_QUERY: */

#define ASQBASE            (TAG_USER)

/* Physical properties of the filesystem */

#define ASQ_START_BYTEH             (ASQBASE+1)
#define ASQ_START_BYTEL             (ASQBASE+2)
#define ASQ_END_BYTEH               (ASQBASE+3)
#define ASQ_END_BYTEL               (ASQBASE+4)
#define ASQ_DEVICE_API              (ASQBASE+5)  /* Returns the method of communication used with
                                                    the device.  See defines below */

/* Logical properties */

#define ASQ_BLOCK_SIZE              (ASQBASE+1001)
#define ASQ_TOTAL_BLOCKS            (ASQBASE+1002)

/* Locations of special blocks */

#define ASQ_ROOTBLOCK               (ASQBASE+2001)
#define ASQ_ROOTBLOCK_OBJECTNODES   (ASQBASE+2002)
#define ASQ_ROOTBLOCK_EXTENTS       (ASQBASE+2003)
#define ASQ_FIRST_BITMAP_BLOCK      (ASQBASE+2004)
#define ASQ_FIRST_ADMINSPACE        (ASQBASE+2005)

/* Cache information */

#define ASQ_CACHE_LINES             (ASQBASE+3001)
#define ASQ_CACHE_READAHEADSIZE     (ASQBASE+3002)
#define ASQ_CACHE_MODE              (ASQBASE+3003)
#define ASQ_CACHE_BUFFERS           (ASQBASE+3004)

#define ASQ_CACHE_ACCESSES          (ASQBASE+3010)
#define ASQ_CACHE_MISSES            (ASQBASE+3011)
#define ASQ_OPERATIONS_DECODED      (ASQBASE+3012)
#define ASQ_EMPTY_OPERATIONS_DECODED (ASQBASE+3013)

/* Special properties */

#define ASQ_IS_CASESENSITIVE        (ASQBASE+4001)
#define ASQ_HAS_RECYCLED            (ASQBASE+4002)


/* Defines for ASQ_DEVICE_API: */

#define ASQDA_NORMAL     (0)
#define ASQDA_NSD        (1)
#define ASQDA_TD64       (2)
#define ASQDA_SCSIDIRECT (3)
