
#define SFS_PACKET_BASE         (0xf00000)

#define SFS_INQUIRY             (SFS_PACKET_BASE + 0x11000)

#define ACTION_GET_TOTAL_BLOCKS (SFS_INQUIRY + 0)
#define ACTION_GET_BLOCKSIZE    (SFS_INQUIRY + 1)
#define ACTION_GET_BLOCKDATA    (SFS_INQUIRY + 2)

#define ACTION_SET_DEBUG        (SFS_PACKET_BASE + 0xBDC0)
#define ACTION_SET_CACHE        (SFS_PACKET_BASE + 0xBDC0 + 1)
#define ACTION_FORMAT_ARGS      (SFS_PACKET_BASE + 0xBDC0 + 2)

/* Above are 'old' packet types.  Below are the new types. */

#define ACTION_SFS_QUERY             (SFS_PACKET_BASE + 1)

#define ACTION_SFS_SET_OBJECTBITS    (SFS_PACKET_BASE + 100 + 1)
#define ACTION_SFS_LOCATE_OBJECT     (SFS_PACKET_BASE + 100 + 2)
#define ACTION_SFS_FORMAT            (SFS_PACKET_BASE + 100 + 3)



/* Tags used by ACTION_SFS_FORMAT: */

#define ASFBASE            (TAG_USER)

#define ASF_NAME           (ASFBASE + 1)  /* The name of the disk.  If this tag is not specified
                                             format will fail with ERROR_INVALID_COMPONENT_NAME. */

#define ASF_NORECYCLED     (ASFBASE + 2)  /* If TRUE, then this tag will prevent the Recycled
                                             directory from being created.  It defaults to creating
                                             the Recycled directory. */

#define ASF_CASESENSITIVE  (ASFBASE + 3)  /* If TRUE, then this tag will use case sensitive file
                                             and directory names.  It defaults to case insensitive
                                             names. */

#define ASF_SHOWRECYCLED   (ASFBASE + 4)  /* If TRUE, then the Recycled directory will be visible
                                             in directory listings.  It defaults to an invisible
                                             Recycled directory. */

#define ASF_RECYCLEDNAME   (ASFBASE + 5)  /* The name of the Recycled directory.  Defaults to
                                             '.recycled'. */

/*

ACTION_SFS_QUERY

arg1: (struct TagItem *); Pointer to a TagList.

res1: DOSTRUE if no error occured.
res2: If res1 is DOSFALSE, then this contains the errorcode.

This packet fills the tags you provided in the taglist with 
the desired information.  See query.h for the tags.



ACTION_SFS_SET_OBJECTBITS

arg1: <unused>
arg2: BPTR to a struct FileLock
arg3: BSTR with the path and objectname
arg4: New value

res1: DOSTRUE if no error occured.
res2: If res1 is DOSFALSE, then this contains the errorcode.

This packet allows you to set SFS specific bits for objects.
At the moment it allows you to set or clear the OTYPE_HIDE
bit which causes objects to be excluded from directory
listings.



ACTION_SFS_LOCATE_OBJECT

arg1: ObjectNode number
arg2: Accessmode

res1: A (normal) pointer to a Lock, or 0 if an error occured.
res2: If res1 is 0, then this contains the errorcode.

This packet can be used to obtain a FileLock by ObjectNode
number.  The Recycled directory has a fixed ObjectNode number
RECYCLEDNODE (2) and you can use this packet to obtain a lock
on that directory.

Accessmode is similair to ACTION_LOCATE_OBJECT.  It can be
SHARED_LOCK or EXCLUSIVE_LOCK.



ACTION_SFS_FORMAT

arg1: (struct TagItem *); Pointer to a TagList.


*/
