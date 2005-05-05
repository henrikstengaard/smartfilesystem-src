#include <clib/macros.h>          // MAX, MIN & ABS :-)
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <dos.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/exall.h>
#include <dos/filehandler.h>
#include <exec/errors.h>
#include <exec/interrupts.h>
#include <exec/io.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/resident.h>
#include <libraries/iffparse.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/timer.h>
#include <proto/utility.h>

#include "fs.h"
#include "adminspaces.h"
#include "bitmap.h"
#include "btreenodes.h"
#include "locks.h"
#include "nodes.h"
#include "objects.h"
#include "transactions.h"

#include "adminspaces_protos.h"
#include "bitmap_protos.h"
#include "btreenodes_protos.h"
#include "cachebuffers_protos.h"
#include "debug.h"
#include "locks_protos.h"
#include "nodes_protos.h"
#include "objects_protos.h"
#include "packets.h"
#include "query.h"
#include "support_protos.h"
#include "transactions_protos.h"

#include "/bitfuncs_protos.h"

#include <string.h>

#include "cachedio_protos.h"
#include "deviceio_protos.h"

LONG fillgap(BLCK key);
LONG step(void);


#define BNODE

#define BITMAPFILL 0xFFFFFFFF    /* should be 0xFFFFFFFF !  Carefull.. admin containers are 32 blocks! */

/* defines: */

#define BLCKFACCURACY   (5)                          /* 2^5 = 32 */

#define TIMEOUT         (1)   /* Timeout in seconds */
#define FLUSHTIMEOUT    (20)  /* Flush timeout in seconds */

#define ID_BUSY         MAKE_ID('B','U','S','Y')


/* structs */

struct fsNotifyRequest {
  UBYTE *nr_Name;
  UBYTE *nr_FullName;           /* set by dos - don't touch */
  ULONG nr_UserData;            /* for applications use */
  ULONG nr_Flags;

  union {
    struct {
      struct MsgPort *nr_Port;  /* for SEND_MESSAGE */
    } nr_Msg;

    struct {
      struct Task *nr_Task;     /* for SEND_SIGNAL */
      UBYTE nr_SignalNum;       /* for SEND_SIGNAL */
      UBYTE nr_pad[3];
    } nr_Signal;
  } nr_stuff;

  ULONG nr_Reserved[2];         /* leave 0 for now */
  struct fsNotifyRequest *next;
  struct fsNotifyRequest *prev;

  /* internal use by handlers */
  ULONG nr_MsgCount;            /* # of outstanding msgs */
  struct MsgPort *nr_Handler;   /* handler sent to (for EndNotify) */
};

#define SFSM_ADD_VOLUMENODE    (1)
#define SFSM_REMOVE_VOLUMENODE (2)

struct SFSMessage {
  struct Message msg;
  ULONG command;
  ULONG data;
  LONG errorcode;
};

struct DefragmentStep {
  ULONG id;       // id of the step ("MOVE", "DONE" or 0)
  ULONG length;   // length in longwords (can be 0)
  ULONG data[0];  // size of this array is determined by length.
};


/* global variables */

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct Library *UtilityBase;
struct Library *TimerBase;

struct DosPacket *packet;

struct DeviceNode *devnode;
struct DeviceList *volumenode;
struct MsgPort *sdlhport;       // The SFS DosList handler port.

struct DosEnvec *dosenvec;
struct timerequest *inactivitytimer_ioreq;
struct timerequest *activitytimer_ioreq;        /* How long activity can delay flushing data. */
struct FileSysStartupMsg *startupmsg;
struct MsgPort *msgporttimer;
struct MsgPort *msgportflushtimer;
struct MsgPort *msgportnotify;
struct Process *mytask;

ULONG diskstate;     /* ID_WRITE_PROTECTED, ID_VALIDATING, ID_VALIDATED */
ULONG blockpercycle;
ULONG numsofterrors;
ULONG inhibitnestcounter=0;

ULONG block_defragptr=2;
ULONG *defragmentsteps;
ULONG defragmentlongs;

/* blocks_  = the number of blocks of something (blocks can contain 1 or more sectors)
   block_   = a block number of something (relative to start of partition)
   sectors_ = the number of sectors of something (1 or more sectors form a logical block)
   sector_  = a sector number (relative to the start of the disk)
   bytes_   = the number of bytes of something
   byte_    = a byte number of something
   shifts_  = the number of bytes written as 2^x of something
   mask_    = a mask for something */

extern UWORD shifts_block;               /* shift count needed to convert a blockoffset<->byteoffset */
extern ULONG bytes_block;                /* size of a block in bytes */
extern ULONG blocks_total;               /* size of the partition in blocks */
extern ULONG mask_block;

extern ULONG bufmemtype;

extern ULONG byte_low;                   /* the byte offset of our partition on the disk */
extern ULONG byte_lowh;                  /* high 32 bits */
extern ULONG byte_high;                  /* the byte offset of the end of our partition (excluding) on the disk */
extern ULONG byte_highh;                 /* high 32 bits */

ULONG blocks_reserved_start;      /* number of blocks reserved at start (=reserved) */
ULONG blocks_reserved_end;        /* number of blocks reserved at end (=prealloc) */

ULONG blocks_bitmap;              /* number of BTMP blocks for this partition */
ULONG blocks_inbitmap;            /* number of blocks a single bitmap block can contain info on */
ULONG blocks_admin;               /* the size of all AdminSpaces */

ULONG block_root;                 /* the block offset of the root block */
ULONG block_bitmapbase;           /* the block offset of the first bitmap block */
ULONG block_extentbnoderoot;      /* the block offset of the root of the extent bnode tree */
ULONG block_adminspace;           /* the block offset of the first adminspacecontainer block */
ULONG block_objectnoderoot;       /* the block offset of the root of the objectnode tree */

ULONG block_rovingblockptr;       /* the roving block pointer! */

ULONG node_containers;            /* number of containers per ExtentIndexContainer */

ULONG mask_block32;               /* masks the least significant bits of a BLCKf pointer */
ULONG mask_debug;

ULONG disktype;                   /* the type of media inserted (same as id_DiskType in InfoData
                                     structure) */

ULONG max_name_length=MAX_NAME_LENGTH;
ULONG activity_timeout=FLUSHTIMEOUT;
ULONG inactivity_timeout=TIMEOUT;

UWORD shifts_block32;             /* shift count needed to convert a blockoffset<->32byteoffset (only used by nodes.c!) */

UBYTE is_casesensitive=FALSE;     /* See ROOTBITS_CASESENSITIVE in blockstructure.h */
UBYTE has_recycled=FALSE;         /* See ROOTBITS_RECYCLED in blockstructure.h */

struct ExtFileLock *locklist=0;   /* pointer to the first lock in the list, or zero if none */

LONG totalbuffers;                /* Number of buffers currently in use by filesystem! */
struct MinList globalhandles;
struct fsNotifyRequest *notifyrequests=0;

BYTE activitytimeractive=FALSE;
BYTE pendingchanges=FALSE;  /* indicates that the commit timer is active and that there are pending changes. */
BYTE timerreset=FALSE;
BYTE pad7;

struct Space spacelist[SPACELIST_MAX+1];
UBYTE string[260];  /* For storing BCPL string (usually path) */
UBYTE string2[260]; /* For storing BCPL string (usually comment) */
UBYTE pathstring[520];    /* Used by fullpath to build a full path */

struct fsStatistics statistics;

#ifdef CHECKCODE_SLOW
extern struct MinList cblrulist;
#endif

/* Prototypes */

struct DosPacket *getpacket(struct Process *);
struct DosPacket *waitpacket(struct Process *);
void returnpacket(LONG,LONG);
void sdlhtask(void);

LONG request(UBYTE *,UBYTE *,UBYTE *,APTR, ... );

/* Prototypes of cachebuffer related functions */

LONG readcachebuffercheck(struct CacheBuffer **,ULONG,ULONG);
void outputcachebuffer(struct CacheBuffer *cb);

/* Prototypes of node related functions */

LONG deleteextents(ULONG key);
LONG findextentbnode(ULONG key,struct CacheBuffer **returned_cb,struct fsExtentBNode **returned_bnode);
LONG createextentbnode(ULONG key,struct CacheBuffer **returned_cb,struct fsExtentBNode **returned_bnode);

/* Prototypes of debug functions */

ULONG calcchecksum(void);
void checksum(void);

/* Misc prototypes */

void starttimeout(void);
LONG flushcaches(void);
void invalidatecaches(void);

BOOL freeupspace(void);

BOOL checkchecksum(struct CacheBuffer *);
void setchecksum(struct CacheBuffer *);

void checknotifyforobject(struct CacheBuffer *cb,struct fsObject *o,UBYTE notifyparent);
void checknotifyforpath(UBYTE *path,UBYTE notifyparent);
void notify(struct fsNotifyRequest *nr);
UBYTE *fullpath(struct CacheBuffer *cbstart,struct fsObject *o);

LONG initdisk(void);
void deinitdisk(void);

LONG handlesimplepackets(struct DosPacket *packet);
LONG dumppackets(struct DosPacket *packet,LONG);
void dumppacket(void);
void actioncurrentvolume(struct DosPacket *);
void actionsamelock(struct DosPacket *);
void actiondiskinfo(struct DosPacket *);
void fillinfodata(struct InfoData *);
void fillfib(struct FileInfoBlock *,struct fsObject *);
void diskchangenotify(ULONG class);

/* Prototypes of high-level filesystem functions */

LONG setfilesize(struct ExtFileLock *lock,ULONG bytes);
LONG seek(struct ExtFileLock *lock,ULONG offset);
LONG seektocurrent(struct ExtFileLock *lock);
LONG seekextent(struct ExtFileLock *lock,ULONG offset,struct CacheBuffer **returned_cb,struct fsExtentBNode **returned_ebn,ULONG *returned_extentoffset);
void seekforward(struct ExtFileLock *lock, UWORD ebn_blocks, BLCK ebn_next, ULONG bytestoseek);
LONG writetofile(struct ExtFileLock *lock, UBYTE *buffer, ULONG bytestowrite);

LONG extendblocksinfile(struct ExtFileLock *lock,ULONG blocks);
LONG addblocks(UWORD blocks, BLCK newspace, NODE objectnode, BLCK *io_lastextentbnode);
LONG deletefileslowly(struct CacheBuffer *cbobject, struct fsObject *o);
                                                   
void mainloop(void);

/* ASM prototypes */

extern ULONG __asm RANDOM(register __d0 ULONG);
extern LONG __asm STACKSWAP(register __d0 ULONG, register __a0 LONG(*)(void));
extern ULONG __asm CALCCHECKSUM(register __d0 ULONG,register __a0 ULONG *);
extern ULONG __asm MULU64(register __d0 ULONG,register __d1 ULONG,register __a0 ULONG *);
extern WORD __asm COMPRESSFROMZERO(register __a0 UWORD *,register __a1 UBYTE *,register __d0 ULONG);
extern void __asm CHECKSUM_WRITELONG(register __a0 struct fsBlockHeader *bh, register __a1 ULONG *dest, register __d0 ULONG data);
extern ULONG __asm SQRT(register __d0 ULONG);

#define MAJOR_VERSION (1)
#define MINOR_VERSION (84)

static const char ver_version[]={"\0$VER: " PROGRAMNAMEVER " 1.84 " __AMIGADATE__ "\r\n"};

static const struct Resident resident={RTC_MATCHWORD,&resident,&resident+sizeof(struct Resident),0,1,0,-81,PROGRAMNAME,&ver_version[7],0};
//                                                                                                 ^ version insist on using this as the first part of the version number.


/* Main */

extern const RESBASE;
extern const RESLEN;
extern const _LinkerDB;
extern const NEWDATAL;


LONG mainprogram(void);


LONG start(void) {
  return(STACKSWAP(4096, mainprogram));
/*  if(STACKSWAP()==0) {
    return(ERROR_NO_FREE_STORE);
  }

  return(mainprogram()); */
}


void request2(UBYTE *text);
LONG req(UBYTE *fmt, UBYTE *gads, ... );
LONG req_unusual(UBYTE *fmt, ... );
void dreq(UBYTE *fmt, ... );

// #define STARTDEBUG

// #ifdef STARTDEBUG
  LONG debugreqs=TRUE;
// #endif

LONG __saveds mainprogram() {
  {
    ULONG reslen;
    APTR old_a4;
    APTR newdata;

    SysBase=(*((struct ExecBase **)4));

    old_a4=(APTR)getreg(REG_A4);
    reslen=((ULONG)&RESLEN-(ULONG)old_a4)+64;

    newdata=AllocMem(reslen,MEMF_CLEAR|MEMF_PUBLIC);

    CopyMem(old_a4,newdata,*(((ULONG *)old_a4)-2));

    putreg(REG_A4,(LONG)newdata);
  }

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    mytask=(struct Process *)FindTask(0);
    packet=waitpacket(mytask);

    devnode=(struct DeviceNode *)BADDR(packet->dp_Arg3);
    devnode->dn_Task=&mytask->pr_MsgPort;
    startupmsg=BADDR(devnode->dn_Startup);

    if(initcachebuffers()==0) {

      if((IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37))!=0) {

        #ifdef STARTDEBUG
          dreq("(1) Filesystem initializing...");
        #endif

        if((UtilityBase=OpenLibrary("utility.library",37))!=0) {

          /* Create a msgport and iorequest for opening timer.device */

          if((msgportnotify=CreateMsgPort())!=0) {
            if((msgporttimer=CreateMsgPort())!=0) {
              if((msgportflushtimer=CreateMsgPort())!=0) {
                if((inactivitytimer_ioreq=CreateIORequest(msgporttimer, sizeof(struct timerequest)))!=0) {
                  if((activitytimer_ioreq=CreateIORequest(msgportflushtimer, sizeof(struct timerequest)))!=0) {

                    #ifdef STARTDEBUG
                      dreq("(2) Message ports and iorequests created");
                    #endif

                    if(OpenDevice("timer.device",UNIT_VBLANK,&inactivitytimer_ioreq->tr_node,0)==0) {
                      if(OpenDevice("timer.device",UNIT_VBLANK,&activitytimer_ioreq->tr_node,0)==0) {

                        dosenvec=(struct DosEnvec *)BADDR(startupmsg->fssm_Environ);

                        TimerBase=(struct Library *)inactivitytimer_ioreq->tr_node.io_Device;

                        /* Create a msgport and iorequest for opening the filesystem device */

                        initlist((struct List *)&globalhandles);

                        #ifdef STARTDEBUG
                          dreq("(3) Timer.device opened");
                        #endif

                        if(initcachedio(((UBYTE *)BADDR(startupmsg->fssm_Device)+1), startupmsg->fssm_Unit, startupmsg->fssm_Flags, dosenvec)==0) {

                          #ifdef STARTDEBUG
                            dreq("(4) Cached IO layer started");
                          #endif

                          shifts_block32=shifts_block-BLCKFACCURACY;

                          mask_block32=(1<<shifts_block32)-1;
                          blocks_inbitmap=(bytes_block-sizeof(struct fsBitmap))<<3;  /* must be a multiple of 32 !! */
                          blocks_bitmap=(blocks_total+blocks_inbitmap-1)/blocks_inbitmap;
                          blocks_admin=32;

                          blocks_reserved_start=MAX(dosenvec->de_Reserved,1);
                          blocks_reserved_end=MAX(dosenvec->de_PreAlloc,1);

                          {
                            ULONG blocks512, reserve;

                            blocks512=blocks_total<<(shifts_block-9);
                            reserve=SQRT(blocks512);
                            reserve=(reserve<<2) + reserve;

                            if(reserve > blocks512/100) {      // Do not use more than 1% of the disk.
                              reserve = blocks512/100;
                            }

                            if(reserve < blocks_admin) {       // Use atleast 32 blocks, even if it is more than 1%.
                              reserve = blocks_admin;
                              if(reserve > blocks_total>>1) {
                                reserve = 0;
                              }
                            }

                            block_rovingblockptr=blocks_reserved_start + blocks_admin + blocks_bitmap + reserve;

                            DEBUG(("RovingBlockPtr = %ld, reserve = %ld\n", block_rovingblockptr, reserve));

                            // block_rovingblockptr=0;
                          }

                          mask_debug=0x00000000;

                          node_containers=(bytes_block-sizeof(struct fsNodeContainer))/sizeof(BLCKn);

                          addchangeint((struct Task *)mytask, 1<<mytask->pr_MsgPort.mp_SigBit);

                          DEBUG(("Initializing transactions\n"));

                          if(inittransactions()==0) {

                            #ifdef STARTDEBUG
                              dreq("(5) Transaction layer started");
                            #endif

                            if(addcachebuffers(dosenvec->de_NumBuffers)==0) {

                              #ifdef STARTDEBUG
                                dreq("(6) Filesystem started succesfully!");
                              #endif

                              /* return startup-packet, the handler runs now */

                              DEBUG(("Filesystem started!  Volumenode = %ld\n",volumenode));
                              DEBUG(("Mountlist entry says: Allocate %ld buffers of memtype 0x%08lx\n",dosenvec->de_NumBuffers,dosenvec->de_BufMemType));

                              //  returnpacket(DOSTRUE,0);   // Sep 19 1999: Moved down again.

                              if(CreateNewProcTags(NP_Entry, (ULONG)sdlhtask,
                                                   NP_Name, (ULONG)"SFS DosList handler",
                                                   NP_Priority, 19, TAG_DONE)!=0) {

                                while((sdlhport=FindPort("SFS DosList handler"))==0) {
                                  Delay(2);
                                }

                                if(isdiskpresent()!=FALSE) {
                                  #ifdef STARTDEBUG
                                    dreq("There is a disk present.");
                                  #endif

                                  initdisk();
                                }
                                else {
                                  #ifdef STARTDEBUG
                                    dreq("No disk inserted.");
                                  #endif

                                  disktype=ID_NO_DISK_PRESENT;
                                }

                                returnpacket(DOSTRUE,0);    // Jul  4 1999: Moved up...

                                #ifdef STARTDEBUG
                                  dreq("(7) Informed DOS about the new partition!");
                                #endif

                                mainloop();
                              }
                            }

                            cleanuptransactions();
                          }

                          removechangeint();
                          cleanupcachedio();
                        }
                        CloseDevice(&activitytimer_ioreq->tr_node);
                      }
                      CloseDevice(&inactivitytimer_ioreq->tr_node);
                    }
                    DeleteIORequest(activitytimer_ioreq);
                  }
                  DeleteIORequest(inactivitytimer_ioreq);
                }
                DeleteMsgPort(msgportflushtimer);
              }
              DeleteMsgPort(msgporttimer);
            }
            DeleteMsgPort(msgportnotify);
          }
          CloseLibrary(UtilityBase);
        }

        #ifdef STARTDEBUG
          dreq("Filesystem failed.. exiting.");
        #endif

        CloseLibrary((struct Library *)IntuitionBase);
      }
    }

    DEBUG(("Returning startup packet with DOSFALSE\n"));

    returnpacket(DOSFALSE,ERROR_NO_FREE_STORE);

    CloseLibrary((struct Library *)DOSBase);
  }

  DEBUG(("Exiting filesystem\n"));

  return(ERROR_NO_FREE_STORE);
}

#include "/bitfuncs.c"



void mainloop(void) {
  ULONG signalbits;
  struct MsgPort *msgportpackets;
  struct Message *msg;

  msgportpackets=&mytask->pr_MsgPort;    /* get port of our process */
  signalbits=1<<msgportpackets->mp_SigBit;
  signalbits|=1<<msgporttimer->mp_SigBit;
  signalbits|=1<<msgportnotify->mp_SigBit;
  signalbits|=1<<msgportflushtimer->mp_SigBit;

  #ifdef STARTDEBUG
    dreq("Entering packet loop.");
  #endif

  for(;;) {

    Wait(signalbits);

    do {
      while((msg=GetMsg(msgportflushtimer))!=0) {
        TDEBUG(("mainloop: activity timeout -> flushed transaction\n"));
        flushtransaction();
        activitytimeractive=FALSE;
      }

      while((msg=GetMsg(msgporttimer))!=0) {
        if(timerreset==TRUE) {
          /* There was another request during the timeout/2 period, so we extend the timeout a bit longer. */
          pendingchanges=FALSE;
          starttimeout();
        }
        else {
          TDEBUG(("mainloop: inactivity timeout -> flushed transaction\n"));
          flushcaches();
          pendingchanges=FALSE;
        }
      }

      while((msg=GetMsg(msgportnotify))!=0) {
        FreeMem(msg,sizeof(struct NotifyMessage));
      }

      if(getchange()!=0) {

        /* The disk was inserted or removed! */

        DEBUG(("mainloop: disk inserted or removed\n"));

        if(isdiskpresent()==FALSE) {
          /* Disk was removed */

          disktype=ID_NO_DISK_PRESENT;  /* Must be put before deinitdisk() */
          deinitdisk();
        }
        else {
          /* Disk was inserted */

          initdisk();
        }
      }

      if((msg=GetMsg(msgportpackets))!=0) {


        // diskstate=writeprotection();       /* Don't do this too often!!  It takes LOADS of time for scsi.device. */


#ifdef CHECKCODE_SLOW
  {
    struct CacheBuffer *cb;

    cb=(struct CacheBuffer *)cblrulist.mlh_Head;

    while(cb->node.mln_Succ!=0) {
      if(cb->locked!=0) {
        request(PROGRAMNAME " request","%s\n"\
                                       "mainloop: There was a locked CacheBuffer (lockcount = %ld, block = %ld, type = 0x%08lx)!\n"\
                                       "Nothing bad will happen, but let the author know.\n",
                                       "Ok",((UBYTE *)BADDR(devnode->dn_Name))+1, cb->locked, cb->blckno, *((ULONG *)cb->data));


        cb->locked=0; /* Nothing remains locked */
      }

      cb=(struct CacheBuffer *)(cb->node.mln_Succ);
    }
  }
#endif


        packet=(struct DosPacket *)msg->mn_Node.ln_Name;

        #ifdef STARTDEBUG
          dreq("Received packet 0x%08lx, %ld.",packet,packet->dp_Type);
        #endif

        switch(packet->dp_Type) {
        case ACTION_SFS_SET:
          {
            struct TagItem *taglist=(struct TagItem *)packet->dp_Arg1;
            struct TagItem *tag;

            while(tag=NextTagItem(&taglist)) {
              LONG data=tag->ti_Data;

              switch(tag->ti_Tag) {
              case ASS_MAX_NAME_LENGTH:
                if(data >= 30 && data <= 100) {
                  max_name_length=data;
                }
                break;
              case ASS_ACTIVITY_FLUSH_TIMEOUT:
                if(data >= 5 && data <= 120) {
                  activity_timeout=data;
                }
                break;
              case ASS_INACTIVITY_FLUSH_TIMEOUT:
                if(data >= 1 && data <= 5) {
                  inactivity_timeout=data;
                }
                break;
              }
            }

            returnpacket(DOSTRUE,0);
          }
          break;
        case ACTION_SFS_QUERY:
          {
            struct TagItem *taglist=(struct TagItem *)packet->dp_Arg1;
            struct TagItem *tag;

            while(tag=NextTagItem(&taglist)) {
              switch(tag->ti_Tag) {
              case ASQ_START_BYTEH:
                tag->ti_Data=byte_lowh;
                break;
              case ASQ_START_BYTEL:
                tag->ti_Data=byte_low;
                break;
              case ASQ_END_BYTEH:
                tag->ti_Data=byte_highh;
                break;
              case ASQ_END_BYTEL:
                tag->ti_Data=byte_high;
                break;
              case ASQ_DEVICE_API:
                tag->ti_Data=deviceapiused();
                break;
              case ASQ_BLOCK_SIZE:
                tag->ti_Data=bytes_block;
                break;
              case ASQ_TOTAL_BLOCKS:
                tag->ti_Data=blocks_total;
                break;
              case ASQ_ROOTBLOCK:
                tag->ti_Data=block_root;
                break;
              case ASQ_ROOTBLOCK_OBJECTNODES:
                tag->ti_Data=block_objectnoderoot;
                break;
              case ASQ_ROOTBLOCK_EXTENTS:
                tag->ti_Data=block_extentbnoderoot;
                break;
              case ASQ_FIRST_BITMAP_BLOCK:
                tag->ti_Data=block_bitmapbase;
                break;
              case ASQ_FIRST_ADMINSPACE:
                tag->ti_Data=block_adminspace;
                break;
              case ASQ_CACHE_LINES:
                tag->ti_Data=queryiocache_lines();
                break;
              case ASQ_CACHE_READAHEADSIZE:
                tag->ti_Data=queryiocache_readaheadsize();
                break;
              case ASQ_CACHE_MODE:
                tag->ti_Data=queryiocache_copyback();
                break;
              case ASQ_CACHE_BUFFERS:
                tag->ti_Data=totalbuffers;
                break;
              case ASQ_CACHE_ACCESSES:
                tag->ti_Data=statistics.cache_accesses;
                break;
              case ASQ_CACHE_MISSES:
                tag->ti_Data=statistics.cache_misses;
                break;
              case ASQ_OPERATIONS_DECODED:
                tag->ti_Data=statistics.cache_operationdecode;
                break;
              case ASQ_EMPTY_OPERATIONS_DECODED:
                tag->ti_Data=statistics.cache_emptyoperationdecode;
                break;
              case ASQ_IS_CASESENSITIVE:
                tag->ti_Data=is_casesensitive;
                break;
              case ASQ_HAS_RECYCLED:
                tag->ti_Data=has_recycled;
                break;
              case ASQ_VERSION:
                tag->ti_Data=MAJOR_VERSION * 65536 + MINOR_VERSION;
                break;
              case ASQ_MAX_NAME_LENGTH:
                tag->ti_Data=max_name_length;
                break;
              case ASQ_ACTIVITY_FLUSH_TIMEOUT:
                tag->ti_Data=activity_timeout;
                break;
              case ASQ_INACTIVITY_FLUSH_TIMEOUT:
                tag->ti_Data=inactivity_timeout;
                break;
              }
            }

            returnpacket(DOSTRUE,0);
          }
          break;
        case ACTION_SET_DEBUG:
          DEBUG(("New debug level set to 0x%08lx!\n",packet->dp_Arg1));
          {
            mask_debug=packet->dp_Arg1;

            returnpacket(DOSTRUE,0);
          }
          break;
        case ACTION_SET_CACHE:
          DEBUG(("ACTION_SET_CACHE\n"));
          {
            LONG errorcode;

            if((errorcode=setiocache(packet->dp_Arg1, packet->dp_Arg2, packet->dp_Arg3 & 1))!=0) {
              returnpacket(DOSFALSE, errorcode);
            }
            else {
              returnpacket(DOSTRUE, 0);
            }
          }

          break;
        case ACTION_SFS_FORMAT:
        case ACTION_FORMAT:
          DEBUG(("ACTION_FORMAT\n"));

          {
            struct CacheBuffer *cb;
            ULONG currentdate;
            BLCK block_recycled;
            LONG errorcode=0;

            UBYTE *name=0;
            UBYTE *recycledname=".recycled";
            BYTE casesensitive=FALSE;
            BYTE norecycled=FALSE;
            BYTE showrecycled=FALSE;
            currentdate=getdate();

            if(packet->dp_Type==ACTION_SFS_FORMAT) {
              struct TagItem *taglist=(struct TagItem *)packet->dp_Arg1;
              struct TagItem *tag;

              while(tag=NextTagItem(&taglist)) {
                switch(tag->ti_Tag) {
                case ASF_NAME:
                  name=(UBYTE *)tag->ti_Data;
                  break;
                case ASF_RECYCLEDNAME:
                  recycledname=(UBYTE *)tag->ti_Data;
                  break;
                case ASF_CASESENSITIVE:
                  casesensitive=tag->ti_Data;
                  break;
                case ASF_NORECYCLED:
                  norecycled=tag->ti_Data;
                  break;
                case ASF_SHOWRECYCLED:
                  showrecycled=tag->ti_Data;
                  break;
                }
              }
            }
            else {
              copybstrasstr((BSTR)packet->dp_Arg1, string, 30);
              name=string;
            }

            /* Global block numbers */

            block_adminspace=blocks_reserved_start;
            block_root=blocks_reserved_start+1;
            block_extentbnoderoot=block_root+3;
            block_bitmapbase=block_adminspace+blocks_admin;
            block_objectnoderoot=block_root+4;

            /* Temporary block numbers */

            block_recycled=block_root+5;

            cb=getcachebuffer();

            if(isvalidcomponentname(name)==FALSE || isvalidcomponentname(recycledname)==FALSE) {
              errorcode=ERROR_INVALID_COMPONENT_NAME;
            }

            if(errorcode==0) {
              struct fsAdminSpaceContainer *ac=cb->data;

              DEBUG(("ACTION_FORMAT: Creating AdminSpace container block\n"));

              /* Create AdminSpaceContainer block */

              cb->blckno=block_adminspace;
              clearcachebuffer(cb);

              ac->bheader.id=ADMINSPACECONTAINER_ID;
              ac->bheader.ownblock=block_adminspace;
              ac->bits=blocks_admin;
              ac->adminspace[0].space=block_adminspace;

              if(norecycled==FALSE) {
                /* NOT HASHED RECYCLED
                ac->adminspace[0].bits=0xFF000000;       /* admin + root + hashtable + restore + 2 * nodecontainers + recycled + hash */
                */
                ac->adminspace[0].bits=0xFE000000;       /* admin + root + hashtable + restore + 2 * nodecontainers + recycled */
              }
              else {
                ac->adminspace[0].bits=0xFC000000;       /* admin + root + hashtable + restore + 2 * nodecontainers */
              }

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }

            if(errorcode==0) {
              struct fsObjectContainer *oc=cb->data;
              struct fsRootInfo *ri=(struct fsRootInfo *)((UBYTE *)cb->data+bytes_block-sizeof(struct fsRootInfo));

              DEBUG(("ACTION_FORMAT: Creating Root block\n"));

              /* Create Root block */

              cb->blckno=block_root;
              clearcachebuffer(cb);

              oc->bheader.id=OBJECTCONTAINER_ID;
              oc->bheader.ownblock=block_root;
              oc->object[0].protection=FIBF_READ|FIBF_WRITE|FIBF_EXECUTE|FIBF_DELETE;
              oc->object[0].datemodified=currentdate;
              oc->object[0].bits=OTYPE_DIR;
              oc->object[0].objectnode=ROOTNODE;

              oc->object[0].object.dir.hashtable=block_root+1;

              if(norecycled==FALSE) {
                oc->object[0].object.dir.firstdirblock=block_recycled;
              }

              copystr(name, oc->object[0].name, 30);

              ri->freeblocks=blocks_total-blocks_admin-blocks_reserved_start-blocks_reserved_end-blocks_bitmap;
              ri->datecreated=oc->object[0].datemodified;

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }

            if(errorcode==0) {
              struct fsHashTable *ht=cb->data;

              DEBUG(("ACTION_FORMAT: Creating Root's HashTable block\n"));

              /* Create Root's HashTable block */

              cb->blckno=block_root+1;
              clearcachebuffer(cb);

              ht->bheader.id=HASHTABLE_ID;
              ht->bheader.ownblock=block_root+1;
              ht->parent=ROOTNODE;

              if(norecycled==FALSE) {
                ht->hashentry[HASHCHAIN(hash(".recycled", casesensitive))]=RECYCLEDNODE;
              }

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }

            if(errorcode==0) {
              struct fsBlockHeader *bh=cb->data;

              DEBUG(("ACTION_FORMAT: Creating Transaction block\n"));

              /* Create empty block as a placeholder for the TransactionFailure block. */

              cb->blckno=block_root+2;
              clearcachebuffer(cb);

              bh->id=TRANSACTIONOK_ID;
              bh->ownblock=block_root+2;

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }

            if(errorcode==0) {
              struct fsBNodeContainer *bnc=cb->data;
              struct BTreeContainer *btc=&bnc->btc;

              DEBUG(("ACTION_FORMAT: Creating ExtentNode root block\n"));

              /* Create NodeContainer block for ExtentNodes */

              cb->blckno=block_extentbnoderoot;
              clearcachebuffer(cb);

              bnc->bheader.id=BNODECONTAINER_ID;
              bnc->bheader.ownblock=block_extentbnoderoot;

              btc->isleaf=TRUE;
              btc->nodecount=0;
              btc->nodesize=sizeof(struct fsExtentBNode);

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }

            if(errorcode==0) {
              struct fsNodeContainer *nc=cb->data;
              struct fsObjectNode *on;

              DEBUG(("ACTION_FORMAT: Creating ObjectNode root block\n"));

              /* Create NodeContainer block for ObjectNodes */

              cb->blckno=block_objectnoderoot;
              clearcachebuffer(cb);

              nc->bheader.id=NODECONTAINER_ID;
              nc->bheader.ownblock=block_objectnoderoot;

              nc->nodenumber=1;  /* objectnode 0 is reserved :-) */
              nc->nodes=1;

              on=(struct fsObjectNode *)nc->node;
              on->node.data=block_root;

              on++;
              if(norecycled==FALSE) {
                on->node.data=block_recycled;
                on->hash16=hash(".recycled", casesensitive);
              }
              else {
                on->node.data=-1;         // reserved 2
              }

              on++;
              on->node.data=-1;         // reserved 3

              on++;
              on->node.data=-1;         // reserved 4

              on++;
              on->node.data=-1;         // reserved 5

              on++;
              on->node.data=-1;         // reserved 6

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }

            if(errorcode==0 && norecycled==FALSE) {
              struct fsObjectContainer *oc=cb->data;

              DEBUG(("ACTION_FORMAT: Creating Root ObjectContainer block\n"));

              cb->blckno=block_recycled;
              clearcachebuffer(cb);

              oc->bheader.id=OBJECTCONTAINER_ID;
              oc->bheader.ownblock=block_recycled;
              oc->parent=ROOTNODE;
              oc->object[0].protection=FIBF_READ|FIBF_WRITE;
              oc->object[0].datemodified=currentdate;
              oc->object[0].bits=OTYPE_DIR|OTYPE_UNDELETABLE|OTYPE_QUICKDIR;
              if(showrecycled==FALSE) {
                oc->object[0].bits|=OTYPE_HIDDEN;
              }

              oc->object[0].objectnode=RECYCLEDNODE;

              /* NOT HASHED RECYCLED
              oc->object[0].object.dir.hashtable=block_recycled+1;
              */
              oc->object[0].object.dir.hashtable=0;

              copystr(".recycled",oc->object[0].name,30);

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }

            /* NOT HASHED RECYCLED
            if(errorcode==0 && norecycled==FALSE) {
              struct fsHashTable *ht=cb->data;

              DEBUG(("ACTION_FORMAT: Creating Recycled's HashTable block\n"));

              cb->blckno=block_recycled+1;
              clearcachebuffer(cb);

              ht->bheader.id=HASHTABLE_ID;
              ht->bheader.ownblock=block_recycled+1;
              ht->parent=RECYCLEDNODE;

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }
            */

            if(errorcode==0) {
              struct fsBitmap *bm;
              UWORD cnt,cnt2;
              ULONG block=block_bitmapbase;
              LONG startfree=blocks_admin+blocks_bitmap+blocks_reserved_start;
              LONG sizefree;

              DEBUG(("ACTION_FORMAT: Creating the Bitmap blocks\n"));

              /* Create Bitmap blocks */

              sizefree=blocks_total-startfree-blocks_reserved_end;

              cnt=blocks_bitmap;
              while(cnt-->0 && errorcode==0) {
                clearcachebuffer(cb);

                bm=cb->data;
                bm->bheader.id=BITMAP_ID;
                bm->bheader.ownblock=block;

                for(cnt2=0; cnt2<(blocks_inbitmap>>5); cnt2++) {
                  if(startfree>0) {
                    startfree-=32;
                    if(startfree<0) {
                      bm->bitmap[cnt2]=(1<<(-startfree))-1;
                      sizefree+=startfree;
                    }
                  }
                  else if(sizefree>0) {
                    sizefree-=32;
                    if(sizefree<0) {
                      bm->bitmap[cnt2]=~((1<<(-sizefree))-1);
                    }
                    else {
                      bm->bitmap[cnt2]=BITMAPFILL;
                    }
                  }
                  else {
                    break;
                  }
                }

                cb->blckno=block++;

                setchecksum(cb);
                errorcode=writecachebuffer(cb);
              }
            }

            if(errorcode==0) {
              struct fsRootBlock *rb;

              DEBUG(("ACTION_FORMAT: Creating the Root blocks\n"));

              /* Create Root blocks */

              cb->blckno=0;
              clearcachebuffer(cb);

              rb=cb->data;
              rb->bheader.id=DOSTYPE_ID;
              rb->bheader.ownblock=0;

              rb->version=STRUCTURE_VERSION;
              rb->sequencenumber=0;

              rb->datecreated=currentdate;

              rb->firstbyteh=byte_lowh;
              rb->firstbyte=byte_low;

              rb->lastbyteh=byte_highh;
              rb->lastbyte=byte_high;

              rb->totalblocks=blocks_total;
              rb->blocksize=bytes_block;

              rb->bitmapbase=block_bitmapbase;
              rb->adminspacecontainer=block_adminspace;
              rb->rootobjectcontainer=block_root;
              rb->extentbnoderoot=block_extentbnoderoot;
              rb->objectnoderoot=block_objectnoderoot;

              if(casesensitive!=FALSE) {
                rb->bits|=ROOTBITS_CASESENSITIVE;
              }
              if(norecycled==FALSE) {
                rb->bits|=ROOTBITS_RECYCLED;
              }

              setchecksum(cb);
              if((errorcode=writecachebuffer(cb))==0) {
                cb->blckno=blocks_total-1;

                DEBUG(("ACTION_FORMAT: Creating the 2nd Root block\n"));

                rb->bheader.ownblock=blocks_total-1;

                setchecksum(cb);
                errorcode=writecachebuffer(cb);
              }
            }

            flushiocache();
            update();
            motoroff();

            if(errorcode!=0) {
              DEBUG(("ACTION_FORMAT: Exiting with errorcode %ld\n",errorcode));

              returnpacket(DOSFALSE, errorcode);
            }
            else {
              returnpacket(DOSTRUE, 0);
            }
          }

          break;
        case ACTION_INHIBIT:
          DEBUG(("ACTION_INHIBIT(%ld)\n",packet->dp_Arg1));

          /* This function nests.  Each call to inhibit the disk should be matched
             with one to uninhibit the disk.

             Inhibiting will only be allowed when there are no pending changes
             (operations) to be committed to the disk. */

          if(packet->dp_Arg1!=DOSFALSE) {
            if(pendingchanges==FALSE) {
              #ifdef STARTDEBUG
                dreq("Disk inhibited (nesting = %ld).", inhibitnestcounter);
              #endif

              if(inhibitnestcounter++==0) {   // Inhibited for the first time?
                disktype=ID_BUSY;  /* Must be put before deinitdisk() Feb 27 1999: Maybe not needed anymore */
                deinitdisk();
              }

              returnpacket(DOSTRUE,0);
            }
            else {
              returnpacket(DOSFALSE, ERROR_OBJECT_IN_USE);
            }
          }
          else if(inhibitnestcounter>0 && --inhibitnestcounter==0) {

            returnpacket(DOSTRUE, 0);      /* Workbench keeps doslist locked, and doesn't send any packets
                                              during that time to this handler.  As initdisk() needs to lock
                                              the doslist we MUST return the packet before calling initdisk() */
            initdisk();
          }
          else {
            /* Workbench revokes ACTION_INHIBIT without ever actually having
               inhibited the volume.  We'll just return the packet and ignore
               such requests. */

            returnpacket(DOSTRUE,0);
          }
          break;
        case ACTION_SERIALIZE_DISK:
          DEBUG(("ACTION_SERIALIZE_DISK\n"));

          {
            struct CacheBuffer *cb;
            LONG errorcode;

            if((errorcode=readcachebuffercheck(&cb,block_root,OBJECTCONTAINER_ID))==0) {
              struct fsObjectContainer *oc=cb->data;
              struct fsRootInfo *ri=(struct fsRootInfo *)((UBYTE *)cb->data+bytes_block-sizeof(struct fsRootInfo));

              oc->object[0].datemodified=getdate();
              ri->datecreated=oc->object[0].datemodified;

              setchecksum(cb);
              errorcode=writecachebuffer(cb);
            }

            if(errorcode!=0) {
              returnpacket(DOSFALSE,errorcode);
            }
            else {
              returnpacket(DOSTRUE,0);
            }
          }

          break;
        default:
          if(handlesimplepackets(packet)==0) {

            if(disktype==ID_DOS_DISK) {
              switch(packet->dp_Type) {
              case ACTION_MAKE_LINK:
                DEBUG(("ACTION_MAKE_LINK\n"));

                {
                  if(packet->dp_Arg4==LINK_HARD) {
                    returnpacket(DOSFALSE,ERROR_ACTION_NOT_KNOWN);
                  }
              /*  else if(packet->dp_Arg4!=LINK_SOFT) {
                    returnpacket(DOSFALSE,ERROR_BAD_NUMBER);
                  } */  /* Check removed because DOS apparantely defines non-zero as being a Soft link! */
                  else {
                    struct ExtFileLock *lock;
                    LONG errorcode;
  
                    lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);
                    copybstrasstr(packet->dp_Arg2,string,258);
  
                    DEBUG(("ACTION_MAKE_LINK: Name = '%s', LinkPath = '%s'\n",string,(UBYTE *)packet->dp_Arg3));

                    if((errorcode=findcreate(&lock,string,packet->dp_Type,(UBYTE *)packet->dp_Arg3))!=0) {
                      returnpacket(DOSFALSE,errorcode);
                    }
                    else {
//                      freelock(lock);
                      returnpacket(DOSTRUE,0);
                    }
                  }
                }

                break;
              case ACTION_READ_LINK:
                DEBUG(("ACTION_READ_LINK\n"));

                {
                  struct CacheBuffer *cb;
                  struct fsObject *o;
                  struct ExtFileLock *lock;
                  UBYTE *dest=(UBYTE *)packet->dp_Arg3;
                  LONG errorcode;
                  NODE objectnode;

                  lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);

                  if(lock==0) {
                    objectnode=ROOTNODE;
                  }
                  else {
                    objectnode=lock->objectnode;
                  }

                  if((errorcode=readobject(objectnode, &cb, &o))==0) {
                    UBYTE *path=(UBYTE *)packet->dp_Arg2;

                    DEBUG(("ACTION_READ_LINK: path = '%s', errorcode = %ld\n",path,errorcode));

                    errorcode=locateobject2(&path, &cb, &o);

                    if(errorcode!=ERROR_IS_SOFT_LINK) {
                      errorcode=ERROR_OBJECT_NOT_FOUND;
                    }
                    else {
                      struct CacheBuffer *cb2;

                      while(*path!=0) {
                        if(*path=='/') {
                          break;
                        }
                        path++;
                      }

                      DEBUG(("ACTION_READ_LINK: path = '%s'\n",path));

                      if((errorcode=readcachebuffercheck(&cb2, o->object.file.data, SOFTLINK_ID))==0) {
                        struct fsSoftLink *sl=cb2->data;
                        LONG length=packet->dp_Arg4;
                        UBYTE *src=sl->string;
                        UBYTE *s=string;

                        while((*s++=*path++)!=0) {        // Work-around for bug in ixemul, which sometimes provides the same pointer for dp_Arg2 (path) and dp_Arg3 (soft-link buffer)
                        }

                        s=string;

                        DEBUG(("ACTION_READ_LINK: length = %ld, sl->string = '%s', path = '%s'\n", length, sl->string, s));

                        /* cb is no longer valid at this point. */

                        while(length-->0 && (*dest++=*src++)!=0) {
                        }

                        dest--;
                        length++;

                        while(length-->0 && (*dest++=*s++)!=0) {
                        }

                        if(length<0) {
                          errorcode=ERROR_NO_FREE_STORE;
                        }
                      }
                    }
                  }

                  if(errorcode!=0) {
                    returnpacket(DOSFALSE, errorcode);
                  }
                  else {
//                    returnpacket(DOSTRUE, 0);
//                    returnpacket(packet->dp_Arg4,0);  /* hack! */
                    returnpacket((LONG)(dest - packet->dp_Arg3 - 1), 0);  /* hack!   Sep 19 1999: Why the hell did I do this??? -> Seems like FFS does it that way as well. */
                  }
                }
                break;
              case ACTION_CREATE_DIR:
                DEBUG(("ACTION_CREATE_DIR\n"));

                {
                  struct ExtFileLock *lock;
                  LONG errorcode;

                  lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);
                  copybstrasstr(packet->dp_Arg2,string,258);

                  if((errorcode=findcreate(&lock,string,packet->dp_Type,0))!=0) {
                    returnpacket(0,errorcode);
                  }
                  else {
                    returnpacket(TOBADDR(lock),0);
                  }
                }

                break;
              case ACTION_FINDUPDATE:
              case ACTION_FINDINPUT:
              case ACTION_FINDOUTPUT:
              case ACTION_FH_FROM_LOCK:
                XDEBUG((DEBUG_OBJECTS, "ACTION_FIND#? or ACTION_FH_FROM_LOCK\n"));

                {
                  struct ExtFileLock *lock;
                  struct FileHandle *fh;
                  LONG errorcode=0;

                  fh=(struct FileHandle *)BADDR(packet->dp_Arg1);
                  lock=(struct ExtFileLock *)BADDR(packet->dp_Arg2);

                  if(packet->dp_Type!=ACTION_FH_FROM_LOCK) {
                    copybstrasstr(packet->dp_Arg3,string,258);
                    DEBUG(("OPEN FILE: %s (mode = %ld)\n", string, packet->dp_Type));
                    errorcode=findcreate(&lock,string,packet->dp_Type,0);
                  }

                  if(errorcode==0) {
                    if((errorcode=createglobalhandle(lock))==0) {
                      fh->fh_Arg1=(LONG)lock;
                    }
                    else if(packet->dp_Type!=ACTION_FH_FROM_LOCK) {
                      freelock(lock);
                    }
                  }

                  if(errorcode!=0) {
                    returnpacket(DOSFALSE,errorcode);
                  }
                  else {
                    if(packet->dp_Type==ACTION_FINDOUTPUT && has_recycled!=FALSE) {
                      ULONG files,blocks;

                      if((errorcode=getrecycledinfo(&files, &blocks))==0) {
                        if(files>35) {
                          cleanupdeletedfiles();
                        }
                      }
                    }

                    returnpacket(DOSTRUE,0);
                  }
                }

                break;
              case ACTION_END:
                XDEBUG((DEBUG_OBJECTS, "ACTION_END\n"));

                /* ACTION_FREE_LOCK's code is similair */

                {
                  struct ExtFileLock *lock=(struct ExtFileLock *)packet->dp_Arg1;
                  LONG errorcode;

                  /* If a file has been modified by the use of ACTION_SET_FILE_SIZE or
                     ACTION_WRITE, or ACTION_FINDOUTPUT or ACTION_FINDUPDATE (only when
                     creating a new file) were used to create the file then we must
                     also update the datestamp of the file.  We also must send out a
                     notification.  Finally we also need to clear the A bit.

                     For this purpose a special flag in the lock tells us
                     whether or not any of the above actions has occured. */

                  if((lock->bits & EFL_MODIFIED) != 0) {
                    struct CacheBuffer *cb;
                    struct fsObject *o;

                    /* Aha! */

                    if(lock->lastextendedblock!=0) {
                      block_rovingblockptr=lock->lastextendedblock;
                      if(block_rovingblockptr>=blocks_total) {
                        block_rovingblockptr=0;
                      }
                    }

                    if((errorcode=readobject(lock->objectnode, &cb, &o))==0) {
                      newtransaction();

                      errorcode=bumpobject(cb, o);
                      checknotifyforobject(cb, o, TRUE);

                      if(errorcode==0) {
                        endtransaction();
                      }
                      else {
                        deletetransaction();
                      }
                    }

                    /* Ignore any errorcodes -- not really interesting when closing a file... */
                  }

                  if((errorcode=freelock(lock))!=0) {
                    returnpacket(DOSFALSE,errorcode);
                  }
                  else {
                    returnpacket(DOSTRUE,0);
                  }
                }
                break;
              case ACTION_DELETE_OBJECT:
                {
                  LONG errorcode;

                  copybstrasstr(packet->dp_Arg2,string,258);

                  DEBUG(("ACTION_DELETE_OBJECT(0x%08lx,'%s')\n",BADDR(packet->dp_Arg1),string));

                  do {
                    newtransaction();

                    if((errorcode=deleteobject(BADDR(packet->dp_Arg1), validatepath(string), TRUE))==0) {
                      endtransaction();
                    }
                    else {
                      deletetransaction();
                    }
                  } while(errorcode==ERROR_DISK_FULL && freeupspace()!=FALSE);

                  if(errorcode!=0) {
                    returnpacket(DOSFALSE,errorcode);
                  }
                  else {
                    ULONG files,blocks;

                    if((errorcode=getrecycledinfo(&files, &blocks))==0) {
                      if(files>35) {
                        cleanupdeletedfiles();
                      }
                    }

                    returnpacket(DOSTRUE,0);
                  }
                }
                break;
              case ACTION_RENAME_DISK:
                DEBUG(("ACTION_RENAME_DISK\n"));

                {
                  struct CacheBuffer *cb;
                  LONG errorcode;

                  if((errorcode=readcachebuffercheck(&cb,block_root,OBJECTCONTAINER_ID))==0) {
                    if(AttemptLockDosList(LDF_WRITE|LDF_VOLUMES)!=0) {
                      struct fsObjectContainer *oc=cb->data;
                      UBYTE *s;
                      UBYTE *d;
                      UBYTE len;

                      /* Succesfully locked the doslist */

                      newtransaction();

                      preparecachebuffer(cb);

                      s=BADDR(packet->dp_Arg1);
                      d=oc->object[0].name;
                      len=*s++;

                      if(len>30) {
                        len=30;
                      }

                      while(len-->0) {
                        *d++=*s++;
                      }
                      *d++=0;
                      *d=0;    /* Zero for comment */

                      if((errorcode=storecachebuffer(cb))==0 && volumenode!=0) {
                        s=BADDR(packet->dp_Arg1);
                        d=BADDR(volumenode->dl_Name);
                        len=*s++;

                        if(len>30) {
                          len=30;
                        }

                        *d++=len;
                        while(len-->0) {
                          *d++=*s++;
                        }
                        *d=0;
                      }

                      UnLockDosList(LDF_WRITE|LDF_VOLUMES);

                      if(errorcode==0) {
                        endtransaction();
                      }
                      else {
                        deletetransaction();
                      }
                    }
                    else {
                      /* Doslist locking attempt was unsuccesful.  Send this message back to our
                         port to try it again later. */
  
                      PutMsg(msgportpackets,msg);
                      break;
                    }
                  }
  
                  if(errorcode==0) {
                    returnpacket(DOSTRUE,0);
                  }
                  else {
                    returnpacket(DOSFALSE,errorcode);
                  }
                }
                break;
              case ACTION_RENAME_OBJECT:
                DEBUG(("ACTION_RENAME_OBJECT\n"));

                {
                  LONG errorcode;

                  do {
                    struct CacheBuffer *cb;
                    struct fsObject *o;
                    struct ExtFileLock *lock;
                    UBYTE *newname;
                    UBYTE *s;

                    lock=BADDR(packet->dp_Arg1);
                    copybstrasstr(packet->dp_Arg2,string,258);

                    if((errorcode=locateobjectfromlock(lock,validatepath(string),&cb,&o))==0) {
                      copybstrasstr(packet->dp_Arg4,string,258);

                      settemporarylock(o->objectnode);

                      newname=validatepath(string);
                      s=FilePart(newname);

                      if(*s!=0) {
                        newtransaction();
                        if((errorcode=renameobject(cb,o,BADDR(packet->dp_Arg3),newname))==0) {
                          endtransaction();
                        }
                        else {
                          deletetransaction();
                        }
                      }
                      else {
                        errorcode=ERROR_INVALID_COMPONENT_NAME;
                      }
                    }
                  } while(errorcode==ERROR_DISK_FULL && freeupspace()!=FALSE);

                  cleartemporarylock();

                  if(errorcode!=0) {
                    returnpacket(DOSFALSE,errorcode);
                  }
                  else {
                    returnpacket(DOSTRUE,0);
                  }
                }
                break;
              case ACTION_SET_COMMENT:
                DEBUG(("ACTION_SET_COMMENT\n"));
                {
                  LONG errorcode;

                  do {
                    copybstrasstr(packet->dp_Arg3,string,258);
                    copybstrasstr(packet->dp_Arg4,string2,258);

                    newtransaction();
                    if((errorcode=setcomment(BADDR(packet->dp_Arg2),validatepath(string),string2))!=0) {
                      deletetransaction();
                    }
                    else {
                      endtransaction();
                    }
                  } while(errorcode==ERROR_DISK_FULL && freeupspace()!=FALSE);

                  if(errorcode!=0) {
                    returnpacket(DOSFALSE,errorcode);
                  }
                  else {
                    returnpacket(DOSTRUE,0);
                  }
                }
                break;
              case ACTION_SET_DATE:
              case ACTION_SET_PROTECT:
              case ACTION_SET_OWNER:
              case ACTION_SFS_SET_OBJECTBITS:
                XDEBUG((DEBUG_OBJECTS, "ACTION_SET_DATE or ACTION_SET_PROTECT or ACTION_SET_OWNER\n"));

                {
                  LONG errorcode;

                  do {
                    struct CacheBuffer *cb;
                    struct fsObject *o;
                    struct ExtFileLock *lock;

                    lock=BADDR(packet->dp_Arg2);
                    copybstrasstr(packet->dp_Arg3,string,258);

                    if((errorcode=locatelockableobject(lock,validatepath(string),&cb,&o))==0) {
                      NODE objectnode=o->objectnode;

                      if(objectnode!=ROOTNODE) {

                        settemporarylock(objectnode);

                        newtransaction();
                        preparecachebuffer(cb);

                        if(packet->dp_Type==ACTION_SET_DATE) {
                          CHECKSUM_WRITELONG(cb->data, &o->datemodified, datestamptodate((struct DateStamp *)packet->dp_Arg4));

                          // o->datemodified=datestamptodate((struct DateStamp *)packet->dp_Arg4);
                        }
                        else if(packet->dp_Type==ACTION_SET_PROTECT) {
                          CHECKSUM_WRITELONG(cb->data, &o->protection, packet->dp_Arg4^(FIBF_READ|FIBF_WRITE|FIBF_EXECUTE|FIBF_DELETE));

                          // o->protection=packet->dp_Arg4^(FIBF_READ|FIBF_WRITE|FIBF_EXECUTE|FIBF_DELETE);
                        }
                        else if(packet->dp_Type==ACTION_SET_OWNER) {
                          // ULONG *owner=(ULONG *)&o->owneruid;

                          CHECKSUM_WRITELONG(cb->data, (ULONG *)&o->owneruid, packet->dp_Arg4);

                          // *owner=packet->dp_Arg4;
                        }
                        else {
                          o->bits=(packet->dp_Arg4 & (OTYPE_HIDDEN|OTYPE_UNDELETABLE)) | (o->bits & ~(OTYPE_HIDDEN|OTYPE_UNDELETABLE));
                          setchecksum(cb);      // new
                        }

                        if((errorcode=storecachebuffer_nochecksum(cb))==0) {
                          struct GlobalHandle *gh;

                          checknotifyforobject(cb,o,TRUE);
                          endtransaction();

                          if(packet->dp_Type==ACTION_SET_PROTECT && (gh=findglobalhandle(objectnode))!=0) {
                            gh->protection=packet->dp_Arg4^(FIBF_READ|FIBF_WRITE|FIBF_EXECUTE|FIBF_DELETE);
                          }
                        }
                        else {
                          deletetransaction();
                        }
                      }
                      else {
                        errorcode=ERROR_OBJECT_WRONG_TYPE;
                      }
                    }

                    /* this 'loop' will only be left if the disk wasn't full, or
                       cleanupdeletedfiles() couldn't free up any more space. */

                  } while(errorcode==ERROR_DISK_FULL && freeupspace()!=FALSE);

                  cleartemporarylock();

                  if(errorcode!=0) {
                    returnpacket(DOSFALSE,errorcode);
                  }
                  else {
                    returnpacket(DOSTRUE,0);
                  }
                }
                break;
              case ACTION_SET_FILE_SIZE:
                DEBUG(("ACTION_SET_FILE_SIZE(0x%08lx,0x%08lx,0x%08lx)\n",packet->dp_Arg1,packet->dp_Arg2,packet->dp_Arg3));

                {
                  struct ExtFileLock *lock=(struct ExtFileLock *)packet->dp_Arg1;
                  LONG newfilesize;
                  LONG errorcode;

                  if(packet->dp_Arg3==OFFSET_BEGINNING) {
                    newfilesize=packet->dp_Arg2;
                  }
                  else if(packet->dp_Arg3==OFFSET_END) {
                    newfilesize=lock->gh->size+packet->dp_Arg2;
                  }
                  else if(packet->dp_Arg3==OFFSET_CURRENT) {
                    newfilesize=lock->offset+packet->dp_Arg2;
                  }
                  else {
                    returnpacket(-1,ERROR_BAD_NUMBER);
                    break;
                  }

                  if(newfilesize>=0) {
                    ULONG data=lock->gh->data;

                    do {
                      errorcode=setfilesize(lock,newfilesize);
                    } while(errorcode==ERROR_DISK_FULL && freeupspace()!=FALSE);

                    if(errorcode!=0) {
                      lock->gh->data=data;
                    }
                  }
                  else {
                    errorcode=ERROR_SEEK_ERROR;
                  }

                  if(errorcode==0) {
                    returnpacket(lock->gh->size,0);
                  }
                  else {
                    returnpacket(-1,errorcode);
                  }
                }
                break;
              case ACTION_SEEK:
                XDEBUG((DEBUG_SEEK,"ACTION_SEEK(0x%08lx,0x%08lx,0x%08lx)\n",packet->dp_Arg1,packet->dp_Arg2,packet->dp_Arg3));
  
                {
                  struct ExtFileLock *lock=(struct ExtFileLock *)packet->dp_Arg1;
                  struct GlobalHandle *gh=lock->gh;
                  ULONG oldpos=lock->offset;
                  LONG newpos;
                  LONG errorcode;
  
                  if(packet->dp_Arg3==OFFSET_BEGINNING) {
                    newpos=packet->dp_Arg2;
                  }
                  else if(packet->dp_Arg3==OFFSET_END) {
                    newpos=gh->size+packet->dp_Arg2;
                  }
                  else if(packet->dp_Arg3==OFFSET_CURRENT) {
                    newpos=lock->offset+packet->dp_Arg2;
                  }
                  else {
                    returnpacket(-1,ERROR_BAD_NUMBER);
                    break;
                  }
  
                  if(newpos>=0 && newpos<=gh->size) {
                    if(newpos!=oldpos) {
                      errorcode=seek(lock,newpos);
                    }
                    else {
                      errorcode=0;
                    }
                  }
                  else {
                    errorcode=ERROR_SEEK_ERROR;
                  }

                  if(errorcode==0) {
                    returnpacket(oldpos,0);
                  }
                  else {
                    returnpacket(-1,errorcode);
                  }
                }
                break;
              case ACTION_READ:
                XDEBUG((DEBUG_IO,"ACTION_READ(0x%08lx,0x%08lx,%ld)\n",packet->dp_Arg1,packet->dp_Arg2,packet->dp_Arg3));

                {
                  struct ExtFileLock *lock=(struct ExtFileLock *)packet->dp_Arg1;
                  struct GlobalHandle *gh=lock->gh;
                  UBYTE *buffer=(UBYTE *)packet->dp_Arg2;
                  ULONG bytesleft;
                  UBYTE *startofbuf=buffer;
                  LONG errorcode;

                  bytesleft=packet->dp_Arg3;
                  if(lock->offset+bytesleft > gh->size) {
                    bytesleft=gh->size-lock->offset;
                  }

                  if((errorcode=seektocurrent(lock))==0) {
                    if((gh->protection & FIBF_READ)!=0) {
                      struct CacheBuffer *extent_cb;
                      struct fsExtentBNode *ebn;

                      while(bytesleft>0 && (errorcode=findextentbnode(lock->curextent, &extent_cb, &ebn))==0) {
                        ULONG bytestoread;
                        ULONG offsetinblock=lock->extentoffset & mask_block;
                        BLCK ebn_next=ebn->next;
                        UWORD ebn_blocks=ebn->blocks;

                        XDEBUG((DEBUG_IO,"ACTION_READ: bytesleft = %ld, offsetinblock = %ld, ExtentBNode = %ld, ebn->data = %ld, ebn->blocks = %ld\n",bytesleft,offsetinblock,lock->curextent,ebn->key,ebn->blocks));

                        if(offsetinblock!=0 || bytesleft<bytes_block) {

                          // XDEBUG((DEBUG_IO,"ACTION_READ: nextextentbnode = %ld, extentnodesize = %ld, en->blocks = %ld\n",nextextentbnode,extentnodesize,(ULONG)ebn->blocks));

                          bytestoread=bytes_block-offsetinblock;

                          /* Check if there are more bytes left in the block then we want to read */
                          if(bytestoread > bytesleft) {
                            bytestoread=bytesleft;
                          }

                          if((errorcode=readbytes(ebn->key+(lock->extentoffset>>shifts_block), buffer, offsetinblock, bytestoread))!=0) {
                            break;
                          }
                        }
                        else {

                          bytestoread=(((ULONG)ebn_blocks)<<shifts_block) - lock->extentoffset;

                          /* Check if there are more bytes left in the Extent then we want to read */
                          if(bytestoread > bytesleft) {
                            bytestoread=bytesleft & ~mask_block;
                          }

                          if((errorcode=read(ebn->key+(lock->extentoffset>>shifts_block), buffer, bytestoread>>shifts_block))!=0) {
                            break;
                          }
                        }

                        seekforward(lock, ebn_blocks, ebn_next, bytestoread);

                        bytesleft-=bytestoread;
                        buffer+=bytestoread;

                        XDEBUG((DEBUG_IO,"ACTION_READ: bytesleft = %ld, errorcode = %ld\n",bytesleft,errorcode));
                      }
                    }
                    else {
                      errorcode=ERROR_READ_PROTECTED;
                    }
                  }

                  XDEBUG((DEBUG_IO,"ACTION_READ: errorcode = %ld, buffer = %ld, startofbuf = %ld\n",errorcode,buffer,startofbuf));

                  if(errorcode!=0) {
                    returnpacket(-1,errorcode);
                  }
                  else {
                    returnpacket(buffer-startofbuf,0);
                  }
                }
                break;
              case ACTION_WRITE:
                XDEBUG((DEBUG_IO,"ACTION_WRITE(0x%08lx,0x%08lx,%ld)\n",packet->dp_Arg1,packet->dp_Arg2,packet->dp_Arg3));

                {
                  struct ExtFileLock *lock=(struct ExtFileLock *)packet->dp_Arg1;
                  ULONG bytestowrite=packet->dp_Arg3;
                  LONG errorcode=0;

                  do {
                    struct GlobalHandle *gh=lock->gh;

                    /* Save some values in case of an error: */

                    BLCK curextent=lock->curextent;
                    ULONG extentoffset=lock->extentoffset;
                    ULONG offset=lock->offset;
                    ULONG size=gh->size;
                    ULONG data=gh->data;

                    if(bytestowrite!=0) {
                      newtransaction();

                      if((errorcode=writetofile(lock, (UBYTE *)packet->dp_Arg2, bytestowrite))==0) {
                        endtransaction();
                      }
                      else {
                        lock->curextent=curextent;
                        lock->extentoffset=extentoffset;
                        lock->offset=offset;
                        gh->size=size;
                        gh->data=data;

                        deletetransaction();
                      }
                    }
                  } while(errorcode==ERROR_DISK_FULL && freeupspace()!=FALSE);

                  if(errorcode!=0) {
                    XDEBUG((DEBUG_IO,"ACTION_WRITE returns (-1, %ld)\n",errorcode));

                    returnpacket(-1,errorcode);
                  }
                  else {
                    XDEBUG((DEBUG_IO,"ACTION_WRITE returns (%ld, 0)\n",bytestowrite));

                    lock->bits|=EFL_MODIFIED;
                    returnpacket(bytestowrite,0);
                  }
                }
                break;
              case ACTION_FREE_LOCK:
                XDEBUG((DEBUG_LOCK,"ACTION_FREE_LOCK\n"));

                {
                  LONG errorcode;

                  if((errorcode=freelock((struct ExtFileLock *)BADDR(packet->dp_Arg1)))!=0) {
                    returnpacket(DOSFALSE,errorcode);
                    break;
                  }
                  returnpacket(DOSTRUE,0);
                }
                break;
              case ACTION_EXAMINE_ALL:
                DEBUG(("ACTION_EXAMINE_ALL\n"));

                {
                  struct ExtFileLock *lock;
                  struct ExAllData *ead;
                  struct ExAllData *prevead=0;
                  struct ExAllControl *eac;
                  struct CacheBuffer *cb;
                  struct fsObject *o;
                  ULONG eadsize;
                  ULONG stringsize;
                  LONG spaceleft;
                  LONG errorcode;

                  lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);
                  ead=(struct ExAllData *)packet->dp_Arg2;
                  eac=(struct ExAllControl *)packet->dp_Arg5;
                  spaceleft=packet->dp_Arg3;

                  eac->eac_Entries=0;

                  if(lock==0) {
                    DEBUG(("ACTION_EXAMINE_ALL: Zero lock was passed in...\n"));
  
                    returnpacket(DOSFALSE,ERROR_OBJECT_WRONG_TYPE);
                    break;
                  }
  
                  if(packet->dp_Arg4>ED_OWNER) {
                    returnpacket(DOSFALSE,ERROR_BAD_NUMBER);
                    break;
                  }
  
                  if(eac->eac_LastKey==0) {
                    if((errorcode=readobject(lock->objectnode,&cb,&o))==0) {
                      if((o->bits & OTYPE_DIR)!=0) {
                        if(o->object.dir.firstdirblock!=0) {
                          if((errorcode=readcachebuffercheck(&cb,o->object.dir.firstdirblock,OBJECTCONTAINER_ID))==0) {
                            struct fsObjectContainer *oc=cb->data;
  
                            o=oc->object;
                            eac->eac_LastKey=o->objectnode;
                          }
                        }
                        else {
                          errorcode=ERROR_NO_MORE_ENTRIES;
                        }
                      }
                      else {
                        errorcode=ERROR_OBJECT_WRONG_TYPE;
                      }
                    }
                  }
                  else {
                    if((errorcode=readobject(eac->eac_LastKey,&cb,&o))==ERROR_IS_SOFT_LINK) {
                      errorcode=0;
                    }
                  }
  
                  while(errorcode==0) {
                    WORD namelength=strlen(o->name);
                    WORD keepentry;
  
                    stringsize=0;
                    eadsize=0;
  
                    switch(packet->dp_Arg4) {
                    default:
                    case ED_OWNER:
                      eadsize+=4;
                    case ED_COMMENT:
                      stringsize+=strlen(o->name+namelength+1)+1;
                      eadsize+=4;
                    case ED_DATE:
                      eadsize+=12;
                    case ED_PROTECTION:
                      eadsize+=4;
                    case ED_SIZE:
                      eadsize+=4;
                    case ED_TYPE:
                      eadsize+=4;
                    case ED_NAME:
                      stringsize+=namelength+1;
                      eadsize+=8;
                      break;
                    }

//                    DEBUG(("ACTION_EXAMINE_ALL: eadsize = %ld, stringsize = %ld, spaceleft = %ld, packet->dp_Arg4 = %ld\n",eadsize,stringsize,spaceleft,packet->dp_Arg4));

                    if(spaceleft<eadsize+stringsize) {
                      break;
                    }

                    switch(packet->dp_Arg4) {
                    default:
                    case ED_OWNER:
                      ead->ed_OwnerUID=o->owneruid;
                      ead->ed_OwnerGID=o->ownergid;
                    case ED_COMMENT:
                      {
                        UBYTE *src=o->name+namelength+1;
                        UBYTE *dest=(UBYTE *)ead+eadsize;

                        ead->ed_Comment=dest;

                        while(*src!=0) {
                          *dest++=*src++;
                          eadsize++;
                        }

                        *dest=0;
                        eadsize++;
                      }
                    case ED_DATE:
                      datetodatestamp(o->datemodified,(struct DateStamp *)&ead->ed_Days);
                    case ED_PROTECTION:
                      ead->ed_Prot=o->protection^(FIBF_READ|FIBF_WRITE|FIBF_EXECUTE|FIBF_DELETE);
                    case ED_SIZE:
                      if((o->bits & OTYPE_DIR)==0) {
                        ead->ed_Size=o->object.file.size;
                      }
                      else {
                        ead->ed_Size=0;
                      }
                    case ED_TYPE:
                      if((o->bits & OTYPE_LINK)!=0) {
                        ead->ed_Type=ST_SOFTLINK;
                      }
                      if((o->bits & OTYPE_DIR)==0) {
                        ead->ed_Type=ST_FILE;
                      }
                      else {
                        ead->ed_Type=ST_USERDIR;
                      }
                    case ED_NAME:
                      {
                        UBYTE *src=o->name;
                        UBYTE *dest=(UBYTE *)ead+eadsize;

                        ead->ed_Name=dest;

                        while(*src!=0) {
                          *dest++=*src++;
                          eadsize++;
                        }

                        *dest=0;
                        eadsize++;

  //                    DEBUG(("Stored entry %s\n",ead->ed_Name));
                      }
                    }

                    if(eac->eac_MatchString!=0) {
                      keepentry=MatchPatternNoCase(eac->eac_MatchString,ead->ed_Name);
                    }
                    else {
                      keepentry=DOSTRUE;
                    }

                    if(keepentry!=DOSFALSE && eac->eac_MatchFunc!=0) {
                      LONG __asm(*hookfunc)(register __a0 struct Hook *,register __a1 struct ExAllData *,register __a2 ULONG)=(LONG __asm(*)(register __a0 struct Hook *,register __a1 struct ExAllData *,register __a2 ULONG))eac->eac_MatchFunc->h_Entry;

                      keepentry=hookfunc(eac->eac_MatchFunc,ead,packet->dp_Arg4);
                    }

                    if(keepentry!=DOSFALSE && (o->bits & OTYPE_HIDDEN)==0) {
                      ead->ed_Next=0;
                      eadsize=(eadsize+3) & 0xFFFFFFFC;
                      if(prevead!=0) {
                        prevead->ed_Next=ead;
                      }
                      prevead=ead;
                      ead=(struct ExAllData *)((UBYTE *)ead+eadsize);
                      spaceleft-=eadsize;
                      eac->eac_Entries++;
                    }
  
                    {
                      struct fsObjectContainer *oc=cb->data;
                      UBYTE *endadr;
  
                      o=nextobject(o);
  
                      endadr=(UBYTE *)oc+bytes_block-sizeof(struct fsObject)-2;
  
                      if((UBYTE *)o>=endadr || o->name[0]==0) {
                        if(oc->next!=0) {
                          if((errorcode=readcachebuffercheck(&cb,oc->next,OBJECTCONTAINER_ID))==0) {
                            struct fsObjectContainer *oc=cb->data;
  
                            o=oc->object;
                            eac->eac_LastKey=o->objectnode;
                          }
                        }
                        else {
                          errorcode=ERROR_NO_MORE_ENTRIES;
                        }
                      }
                      else {
                        eac->eac_LastKey=o->objectnode;
                      }
                    }
                  }
  
                  if(errorcode!=0) {
                    returnpacket(DOSFALSE,errorcode);
                  }
                  else {
                    returnpacket(DOSTRUE,0);
                  }
                }
                break;
              case ACTION_EXAMINE_NEXT:
                // DEBUG(("ACTION_EXAMINE_NEXT(0x%08lx,0x%08lx)\n",BADDR(packet->dp_Arg1),BADDR(packet->dp_Arg2)));

                /* An entry is added to a directory in the first dir block with
                   enough space to hold the entry.  If there is no space, then
                   a new block is added at the START of the directory, and the
                   new entry is added there.

                   In the directory block with enough space, the entry is added
                   at the end of the block.  The order of directory entries there
                   fore is like this (square parenthesis indicate blocks):

                   [789] [456] [123]

                   This means we should scan the directory in the exact opposite
                   order to avoid a trap when an entry is overwritten during
                   directory scanning (MODE_NEW_FILE removes old entry, and adds
                   a new one).  See below:

                   1. [123]; scan is at 1 (next = 2); 1 is overwritten.

                   2. [231]; scan is at 2 (next = 3); 2 is overwritten.

                   3. [312]; scan is at 3 (next = 1); 3 is overwritten.

                   4. [123]; same as 1 -> loop!

                   By scanning the internals of a dir block backwards the problem
                   is solved:

                   1. [123]; scan is at 3 (next = 2); 3 is overwritten.

                   2. [123]; scan is at 2 (next = 1); 2 is overwritten.

                   3. [132]; scan is at 1 (next = next block); 1 is overwritten.

                   4. new block is loaded -> no loop. */

                {
                  struct ExtFileLock *lock;
                  struct CacheBuffer *cb;
                  struct FileInfoBlock *fib=BADDR(packet->dp_Arg2);
                  LONG errorcode=0;

                  lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);

                  if(lock==0) {
                    DEBUG(("ACTION_EXAMINE_NEXT: Zero lock was passed in...\n"));

                    returnpacket(DOSFALSE,ERROR_OBJECT_WRONG_TYPE);
                    break;
                  }

                  /*
                  if(lock->ocblck==0 && lock->ocnode==0) {
                    DEBUG(("ACTION_EXAMINE_NEXT: Lock was never passed to EXAMINE_OBJECT or has been re-allocated or the object Examine()d was a file\n"));

                    returnpacket(DOSFALSE,ERROR_OBJECT_WRONG_TYPE);
                    break;
                  }
                  */

                  if(lock->currentnode!=0xFFFFFFFF && fib->fib_DiskKey!=lock->currentnode) {
                    struct fsObject *o;

                    /* It looks like the lock was reallocated!  In this case we must rebuild
                       the state information in the lock.  lock->currentnode, lock->nextnode
                       and lock->nextnodeblock. */

                    if((errorcode=readobject(fib->fib_DiskKey, &cb, &o))==0) {
                      struct fsObjectContainer *oc=cb->data;

                      lock->currentnode=fib->fib_DiskKey;

                      if((o=prevobject(o, oc))!=0) {
                        lock->nextnodeblock=cb->blckno;
                        lock->nextnode=o->objectnode;
                      }
                      else {
                        lock->nextnodeblock=oc->next;
                        lock->nextnode=0xFFFFFFFF;
                      }
                    }
                  }

                  if(lock->nextnodeblock==0) {
                    errorcode=ERROR_NO_MORE_ENTRIES;
                  }

                  /* The passed in lock describes a directory.  EXAMINE_NEXT should return
                     this directory's entries one at the time.  The lock has state information
                     which helps determine at which entry we currently are. */

                  while(errorcode==0 && (errorcode=readcachebuffercheck(&cb, lock->nextnodeblock, OBJECTCONTAINER_ID))==0) {
                    struct fsObjectContainer *oc=cb->data;
                    struct fsObject *o=0;
                    UBYTE bits;

                    if(lock->nextnode!=0xFFFFFFFF) {
                      /*** It is possible findobject returns 0... */
                      o=findobject(oc, lock->nextnode);
                    }

                    if(o==0) {
                      o=lastobject(oc);
                    }

                    bits=o->bits;
                    fillfib(fib, o);
                    lock->currentnode=o->objectnode;

                    /* prepare for another EXAMINE_NEXT */

                    /* We need to check if there is another object in this ObjectContainer
                       following the one we just returned.  If there is then return its
                       node.  If there isn't then return the next ObjectContainer ptr and
                       set ocnode to zero. */

                    if((o=prevobject(o, oc))!=0) {
                      /* There IS another object */
                      lock->nextnode=o->objectnode;
                    }
                    else {
                      lock->nextnodeblock=oc->next;
                      lock->nextnode=0xFFFFFFFF;
                    }

                    if((bits & OTYPE_HIDDEN)==0) {
                      break;
                    }
                    else if(lock->nextnodeblock==0) {
                      errorcode=ERROR_NO_MORE_ENTRIES;
                      break;
                    }
                  }

                  if(errorcode==0) {
                    returnpacket(DOSTRUE,0);
                  }
                  else {
                    returnpacket(DOSFALSE,errorcode);
                  }
                }
                break;

/******** OLD EXAMINE_NEXT CODE!
              case ACTION_EXAMINE_NEXT:
                // DEBUG(("ACTION_EXAMINE_NEXT(0x%08lx,0x%08lx)\n",BADDR(packet->dp_Arg1),BADDR(packet->dp_Arg2)));

                /* An entry is added to a directory in the first dir block with
                   enough space to hold the entry.  If there is no space, then
                   a new block is added at the START of the directory, and the
                   new entry is added there.

                   In the directory block with enough space, the entry is added
                   at the end of the block.  The order of directory entries there
                   fore is like this (square parenthesis indicate blocks):

                   [789] [456] [123]

                   This means we should scan the directory in the exact opposite
                   order to avoid a trap when an entry is overwritten during
                   directory scanning (MODE_NEW_FILE removes old entry, and adds
                   a new one).  See below:

                   1. [123]; scan is at 1 (next = 2); 1 is overwritten.

                   2. [231]; scan is at 2 (next = 3); 2 is overwritten.

                   3. [312]; scan is at 3 (next = 1); 3 is overwritten.

                   4. [123]; same as 1 -> loop!

                   By scanning the internals of a dir block backwards the problem
                   is solved:

                   1. [123]; scan is at 3 (next = 2); 3 is overwritten.

                   2. [123]; scan is at 2 (next = 1); 2 is overwritten.

                   3. [132]; scan is at 1 (next = next block); 1 is overwritten.

                   4. new block is loaded -> no loop. */

                {
                  struct ExtFileLock *lock;
                  struct CacheBuffer *cb;
                  struct FileInfoBlock *fib=BADDR(packet->dp_Arg2);
                  LONG errorcode=0;

                  lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);

                  if(lock==0) {
                    DEBUG(("ACTION_EXAMINE_NEXT: Zero lock was passed in...\n"));

                    returnpacket(DOSFALSE,ERROR_OBJECT_WRONG_TYPE);
                    break;
                  }

                  /*
                  if(lock->ocblck==0 && lock->ocnode==0) {
                    DEBUG(("ACTION_EXAMINE_NEXT: Lock was never passed to EXAMINE_OBJECT or has been re-allocated or the object Examine()d was a file\n"));

                    returnpacket(DOSFALSE,ERROR_OBJECT_WRONG_TYPE);
                    break;
                  }
                  */

                  if(lock->currentnode!=0xFFFFFFFF && fib->fib_DiskKey!=lock->currentnode) {
                    struct fsObject *o;

                    /* It looks like the lock was reallocated!  In this case we must rebuild
                       the state information in the lock.  lock->currentnode, lock->nextnode
                       and lock->nextnodeblock. */

                    if((errorcode=readobject(fib->fib_DiskKey, &cb, &o))==0) {
                      struct fsObjectContainer *oc=cb->data;

                      lock->currentnode=fib->fib_DiskKey;

                      o=nextobject(o);

                      if(isobject(o,oc)!=FALSE) {
                        lock->nextnodeblock=cb->blckno;
                        lock->nextnode=o->objectnode;
                      }
                      else {
                        lock->nextnodeblock=oc->next;
                        lock->nextnode=0xFFFFFFFF;
                      }
                    }
                  }

                  if(lock->nextnodeblock==0) {
                    errorcode=ERROR_NO_MORE_ENTRIES;
                  }

                  /* The passed in lock describes a directory.  EXAMINE_NEXT should return
                     this directory's entries one at the time.  The lock has state information
                     which helps determine at which entry we currently are. */

                  while(errorcode==0 && (errorcode=readcachebuffercheck(&cb,lock->nextnodeblock,OBJECTCONTAINER_ID))==0) {
                    struct fsObjectContainer *oc=cb->data;
                    struct fsObject *o=0;
                    UBYTE bits;

                    if(lock->nextnode!=0xFFFFFFFF) {
                      /*** It is possible findobject returns 0... */
                      o=findobject(oc,lock->nextnode);
                    }
                    if(o==0) {
                      o=oc->object;
                    }

                    bits=o->bits;
                    fillfib(fib,o);
                    lock->currentnode=o->objectnode;

                    /* prepare for another EXAMINE_NEXT */

                    /* We need to check if there is another object in this ObjectContainer
                       following the one we just returned.  If there is then return its
                       node.  If there isn't then return the next ObjectContainer ptr and
                       set ocnode to zero. */

                    o=nextobject(o);

                    if(isobject(o,oc)!=FALSE) {
                      /* There IS another object */
                      lock->nextnode=o->objectnode;
                    }
                    else {
                      lock->nextnodeblock=oc->next;
                      lock->nextnode=0xFFFFFFFF;
                    }

                    if((bits & OTYPE_HIDDEN)==0) {
                      break;
                    }
                    else if(lock->nextnodeblock==0) {
                      errorcode=ERROR_NO_MORE_ENTRIES;
                      break;
                    }
                  }

                  if(errorcode==0) {
                    returnpacket(DOSTRUE,0);
                  }
                  else {
                    returnpacket(DOSFALSE,errorcode);
                  }
                }
                break;
*/

              case ACTION_EXAMINE_OBJECT:
              case ACTION_EXAMINE_FH:
                DEBUG(("ACTION_EXAMINE_OBJECT\n"));

                {
                  struct ExtFileLock *lock;
                  struct CacheBuffer *cb;
                  struct fsObject *o;
                  NODE objectnode;
                  LONG errorcode;

                  if(packet->dp_Type==ACTION_EXAMINE_OBJECT) {
                    lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);
                  }
                  else {
                    lock=(struct ExtFileLock *)packet->dp_Arg1;
                  }

                  if(lock==0) {
                    objectnode=ROOTNODE;
                  }
                  else {
                    objectnode=lock->objectnode;
                  }

                  errorcode=readobject(objectnode,&cb,&o);
                  if(errorcode==0 || errorcode==ERROR_IS_SOFT_LINK) {
                    fillfib((struct FileInfoBlock *)BADDR(packet->dp_Arg2),o);

                    /* prepare for EXAMINE_NEXT */
                    if(lock!=0 && (o->bits & OTYPE_DIR)!=0) {
                      lock->currentnode=0xFFFFFFFF;
                      lock->nextnode=0xFFFFFFFF;
                      lock->nextnodeblock=o->object.dir.firstdirblock;
                    }

                    returnpacket(DOSTRUE,0);
                  }
                  else {
                    returnpacket(DOSFALSE,errorcode);
                  }
                }

                break;
              case ACTION_INFO:
                DEBUG(("ACTION_INFO\n"));

                {
                  struct ExtFileLock *lock=BADDR(packet->dp_Arg1);
                  struct InfoData *id=BADDR(packet->dp_Arg2);

                  if(lock!=0 && volumenode!=(struct DeviceList *)BADDR(lock->volume)) {
                    DEBUG(("ACTION_INFO: returning error\n"));

                    returnpacket(DOSFALSE,ERROR_DEVICE_NOT_MOUNTED);
                    break;
                  }
  
                  fillinfodata(id);
  
                  returnpacket(DOSTRUE,0);
                }
                break;
              case ACTION_MORE_CACHE:
                DEBUG(("ACTION_MORE_CACHE\n"));

                {
                  LONG errorcode;
  
                  errorcode=addcachebuffers(packet->dp_Arg1);
                  DEBUG(("ACTION_MORE_CACHE: dp_Arg1 = %ld, totalbuffers = %ld, errorcode = %ld\n",packet->dp_Arg1,totalbuffers,errorcode));
  
                  if(errorcode==0) {
                    // returnpacket(DOSTRUE,totalbuffers);
  
                    returnpacket(totalbuffers,0);
                  }
                  else {
                    returnpacket(DOSFALSE,errorcode);
                  }
                }
                break;
              case ACTION_CHANGE_MODE:
                {
                  struct ExtFileLock *lock=0;
                  LONG errorcode=0;
  
                  if(packet->dp_Arg1==CHANGE_FH) {
                    lock=(struct ExtFileLock *)((struct FileHandle *)(BADDR(packet->dp_Arg2)))->fh_Arg1;
                  }
                  else if(packet->dp_Arg1==CHANGE_LOCK) {
                    lock=BADDR(packet->dp_Arg2);
                  }
  
                  if(lock!=0) {
                    if(lock->access!=packet->dp_Arg3) {
                      if(lock->access!=EXCLUSIVE_LOCK) {
                        /* Convert shared lock into an exclusive lock.  We need to check
                           if there are no locks besides this one, and that we aren't
                           trying to get an exclusive lock on the root (which is never
                           allowed). */

                        if(lock->objectnode!=ROOTNODE) {
                          NODE objectnode=lock->objectnode;
  
                          lock->objectnode=0;  /* this makes sure that our lock is not taken into account by lockable() */
  
                          if(lockable(objectnode,EXCLUSIVE_LOCK)!=DOSFALSE) {
                            /* Lockable says it is possible to lock it exclusively! */

                            lock->access=EXCLUSIVE_LOCK;
                          }
                          else {
                            errorcode=ERROR_OBJECT_IN_USE;
                          }
  
                          lock->objectnode=objectnode;
                        }
                        else {
                          errorcode=ERROR_OBJECT_IN_USE;
                        }
                      }
                      else {
                        /* Convert exclusive lock into a shared lock.  Should always be
                           possible. */
  
                        lock->access=SHARED_LOCK;
                      }
                    }
                  }
                  else {
                    errorcode=ERROR_OBJECT_WRONG_TYPE;
                  }
  
                  if(errorcode==0) {
                    returnpacket(DOSTRUE,0);
                  }
                  else {
                    returnpacket(DOSFALSE,errorcode);
                  }
                }
                break;
              case ACTION_PARENT:
              case ACTION_PARENT_FH:
                DEBUG(("ACTION_PARENT\n"));

                {
                  LONG errorcode;
                  struct ExtFileLock *lock;
  
                  if(packet->dp_Type==ACTION_PARENT) {
                    lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);
                  }
                  else {
                    lock=(struct ExtFileLock *)packet->dp_Arg1;
                  }
  
                  if(lock==0 || lock->objectnode==ROOTNODE) {
                    returnpacket(0,0);
                    break;
                  }
  
                  string[0]='/';
                  string[1]=0;
                  if((errorcode=lockobject(lock,string,SHARED_LOCK,&lock))!=0) {
                    returnpacket(0,errorcode);
                  }
                  else {
                    returnpacket(TOBADDR(lock),0);
                  }
                }
                break;
              case ACTION_COPY_DIR:
              case ACTION_COPY_DIR_FH:
                DEBUG(("ACTION_COPY_DIR\n"));
  
                {
                  LONG errorcode;
                  struct ExtFileLock *lock;
  
                  if(packet->dp_Type==ACTION_COPY_DIR) {
                    lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);
                  }
                  else {
                    lock=(struct ExtFileLock *)packet->dp_Arg1;
                  }
  
                  if((errorcode=lockobject(lock,"",SHARED_LOCK,&lock))!=0) {
                    returnpacket(0,errorcode);
                  }
                  else {
                    returnpacket(TOBADDR(lock),0);
                  }
                }
                break;
              case ACTION_LOCATE_OBJECT:
                {
                  struct ExtFileLock *lock;
                  LONG errorcode;

                  copybstrasstr(packet->dp_Arg2,string,258);

                  XDEBUG((DEBUG_LOCK,"ACTION_LOCATE_OBJECT(0x%08lx,'%s',0x%08lx)\n",BADDR(packet->dp_Arg1),string,packet->dp_Arg3));

                  if((errorcode=lockobject((struct ExtFileLock *)BADDR(packet->dp_Arg1),validatepath(string),packet->dp_Arg3,&lock))!=0) {
                    returnpacket(0,errorcode);
                  }
                  else {
                    returnpacket(TOBADDR(lock),0);
                  }
                }
                break;
              case ACTION_SFS_LOCATE_OBJECT:
                {
                  struct CacheBuffer *cb;
                  struct fsObject *o;
                  struct ExtFileLock *lock=0;
                  LONG errorcode;

                  if((errorcode=readobject(packet->dp_Arg1, &cb, &o))==0) {
                    errorcode=lockobject2(o, packet->dp_Arg2, &lock);
                  }

                  if(errorcode!=0) {
                    returnpacket(0, errorcode);
                  }
                  else {
                    returnpacket((LONG)lock, 0);
                  }
                }
                break;
              case ACTION_ADD_NOTIFY:
                {
                  struct fsNotifyRequest *nr;

                  nr=(struct fsNotifyRequest *)packet->dp_Arg1;

                  nr->next=notifyrequests;
                  nr->prev=0;
                  if(notifyrequests!=0) {
                    notifyrequests->prev=nr;
                  }
                  notifyrequests=nr;

                  DEBUG(("ACTION_ADD_NOTIFY: Starting notification on %s (flags 0x%08lx)\n",nr->nr_FullName,nr->nr_Flags));

                  if((nr->nr_Flags & NRF_NOTIFY_INITIAL)!=0) {
                    notify(nr);
                  }

                  returnpacket(DOSTRUE,0);
                }
                break;
              case ACTION_REMOVE_NOTIFY:
                {
                  struct fsNotifyRequest *nr;

                  nr=(struct fsNotifyRequest *)packet->dp_Arg1;

                  DEBUG(("ACTION_REMOVE_NOTIFY: Removing notification of %s\n",nr->nr_FullName));

                  if(nr->nr_Flags & NRF_SEND_MESSAGE != 0) {
                    /* Removing all outstanding messages form msgport */
                    while(GetMsg(nr->nr_stuff.nr_Msg.nr_Port)!=0) {
                    }
                    nr->nr_MsgCount=0;
                  }

                  if(nr->prev!=0) {
                    nr->prev->next=nr->next;
                  }
                  else {
                    notifyrequests=nr->next;
                  }
                  if(nr->next!=0) {
                    nr->next->prev=nr->prev;
                  }

                  nr->next=0;
                  nr->prev=0;

                  returnpacket(DOSTRUE,0);
                }
                break;
              case ACTION_FLUSH:
                {
                  LONG errorcode;

                  if((errorcode=flushcaches())==0) {
                    returnpacket(DOSTRUE, 0);
                  }
                  else {
                    returnpacket(DOSFALSE, errorcode);
                  }
                }
                break;
              case ACTION_SFS_READ_BITMAP:
                {
                  LONG errorcode;

                  Forbid();
                  Permit();

                  errorcode=extractspace((UBYTE *)packet->dp_Arg1, packet->dp_Arg2, packet->dp_Arg3);

                  returnpacket(errorcode==0 ? DOSTRUE : DOSFALSE, errorcode);
                }
                break;
              case ACTION_SFS_DEFRAGMENT_INIT:
                {
                  block_defragptr=2;

                  returnpacket(DOSTRUE, 0);
                }
                break;
              case ACTION_SFS_DEFRAGMENT_STEP:
                {
                  LONG errorcode;

                  defragmentsteps=(ULONG *)packet->dp_Arg1;
                  defragmentlongs=packet->dp_Arg2 - 2;

                  if(defragmentsteps!=0) {
                    *defragmentsteps=0;
                  }

                  while((errorcode=flushtransaction())!=0 && req("Pending buffers couldn't be flushed\nto the disk before defragmentation\nbecause of error %ld.", "Retry|Cancel", errorcode)==1) {
                  }

                  if(errorcode==0) {

                    newtransaction();

                    if((errorcode=step())!=0) {
                      deletetransaction();
                    }
                    else {
                      endtransaction();

                      while((errorcode=flushtransaction())!=0 && req("Pending buffers couldn't be flushed\nto the disk during defragmentation\nbecause of error %ld.", "Retry|Cancel", errorcode)==1) {
                      }
                    }
                  }

                  if(errorcode==0) {
                    struct DefragmentStep *ds=(struct DefragmentStep *)packet->dp_Arg1;

                    while(ds->id!=0) {
                      if(ds->id==MAKE_ID('M','O','V','E') && ds->length==3) {
                        updatelocksaftermove(ds->data[1], ds->data[2], ds->data[0]);
                      }
                      ds=(struct DefragmentStep *)((ULONG *)ds + 2 + ds->length);
                    }

                    returnpacket(DOSTRUE, 0);
                  }
                  else {
                    returnpacket(DOSFALSE, errorcode);
                  }
                }
                break;
              default:
                DEBUG(("ERROR_ACTION_NOT_KNOWN (packettype = %ld)\n",packet->dp_Type));
                returnpacket(DOSFALSE,ERROR_ACTION_NOT_KNOWN);
                break;
              }
            }
            else if(disktype==ID_NO_DISK_PRESENT) {
              dumppackets(packet,ERROR_NO_DISK);
            }
            else {
              dumppackets(packet,ERROR_NOT_A_DOS_DISK);
            }
          }
          break;
        }
      }
    } while(msg!=0);
  }
}



void fillfib(struct FileInfoBlock *fib,struct fsObject *o) {
  UBYTE *src;
  UBYTE *dest;
  UBYTE length;

  if((o->bits & OTYPE_LINK)!=0) {
    fib->fib_DirEntryType=ST_SOFTLINK;
//    fib->fib_DirEntryType=ST_USERDIR;   // For compatibility with Diavolo 3.4 -> screw it, DOpus fails...
    fib->fib_Size=o->object.file.size;
    fib->fib_NumBlocks=0;
  }
  else if((o->bits & OTYPE_DIR)==0) {
    fib->fib_DirEntryType=ST_FILE;
    fib->fib_Size=o->object.file.size;
    fib->fib_NumBlocks=(o->object.file.size+bytes_block-1) >> shifts_block;
  }
  else {
    fib->fib_DirEntryType=ST_USERDIR;
    fib->fib_Size=0;
    fib->fib_NumBlocks=1;
  }
  fib->fib_Protection=o->protection^(FIBF_READ|FIBF_WRITE|FIBF_EXECUTE|FIBF_DELETE);
  fib->fib_EntryType=fib->fib_DirEntryType;
  fib->fib_DiskKey=o->objectnode;
  fib->fib_OwnerUID=o->owneruid;
  fib->fib_OwnerGID=o->ownergid;
  datetodatestamp(o->datemodified,&fib->fib_Date);

  src=o->name;
  dest=&fib->fib_FileName[1];
  length=0;

  while(*src!=0) {
    *dest++=*src++;
    length++;
  }
  fib->fib_FileName[0]=length;

  src++;  /* comment follows name, so just skip the null-byte seperating them */

  dest=&fib->fib_Comment[1];
  length=0;

  while(*src!=0) {
    *dest++=*src++;
    length++;
  }
  fib->fib_Comment[0]=length;
}



struct DosPacket *getpacket(struct Process *p) {
  struct MsgPort *port=&p->pr_MsgPort;   /* get port of our process */
  struct Message *msg;

  if((msg=GetMsg(port))!=0) {
    return((struct DosPacket *)msg->mn_Node.ln_Name);
  }
  else {
    return(0);
  }
}



struct DosPacket *waitpacket(struct Process *p) {
  struct MsgPort *port=&p->pr_MsgPort;   /* get port of our process */
  struct Message *msg;

  WaitPort(port);

  msg=GetMsg(port);

  return((struct DosPacket *)msg->mn_Node.ln_Name);
}



void returnpacket(LONG res1,LONG res2) {
  struct Message *msg;
  struct MsgPort *replyport;

  packet->dp_Res1=res1;  /* set return codes */
  packet->dp_Res2=res2;

  replyport=packet->dp_Port;  /* Get ReplyPort */

  msg=packet->dp_Link;  /* Pointer to the Exec-Message of the packet */

  packet->dp_Port=&mytask->pr_MsgPort;  /* Setting Packet-Port back */

  msg->mn_Node.ln_Name=(char *)packet;  /* Connect Message and Packet */
  msg->mn_Node.ln_Succ=NULL;
  msg->mn_Node.ln_Pred=NULL;

  PutMsg(replyport,msg);  /* Send the Message */
}



void returnpacket2(struct DosPacket *packet, LONG res1, LONG res2) {
  struct Message *msg;
  struct MsgPort *replyport;

  packet->dp_Res1=res1;  /* set return codes */
  packet->dp_Res2=res2;

  replyport=packet->dp_Port;  /* Get ReplyPort */

  msg=packet->dp_Link;  /* Pointer to the Exec-Message of the packet */

  packet->dp_Port=&mytask->pr_MsgPort;  /* Setting Packet-Port back */

  msg->mn_Node.ln_Name=(char *)packet;  /* Connect Message and Packet */
  msg->mn_Node.ln_Succ=NULL;
  msg->mn_Node.ln_Pred=NULL;

  PutMsg(replyport,msg);  /* Send the Message */
}



void starttimeout() {
  /* From the AbortIO AutoDocs:
     iORequest - pointer to an I/O request block (must have been used
                 at least once.  May be active or finished).  */

  if(pendingchanges==FALSE) {
    inactivitytimer_ioreq->tr_time.tv_secs=inactivity_timeout/2;
    inactivitytimer_ioreq->tr_time.tv_micro=(inactivity_timeout*500000)%1000000;
    inactivitytimer_ioreq->tr_node.io_Command=TR_ADDREQUEST;

    SendIO(&inactivitytimer_ioreq->tr_node);

    pendingchanges=TRUE;
    timerreset=FALSE;
  }
  else {
    timerreset=TRUE;  /* Indicates that during the timeout there was another request. */
  }

  if(activitytimeractive==FALSE) {
    activitytimer_ioreq->tr_time.tv_secs=activity_timeout;
    activitytimer_ioreq->tr_time.tv_micro=0;
    activitytimer_ioreq->tr_node.io_Command=TR_ADDREQUEST;

    SendIO(&activitytimer_ioreq->tr_node);

    activitytimeractive=TRUE;
  }
}



void stoptimeout(void) {

  if(pendingchanges!=FALSE) {
    AbortIO(&inactivitytimer_ioreq->tr_node);
    WaitIO(&inactivitytimer_ioreq->tr_node);

    pendingchanges=FALSE;
    timerreset=FALSE;
  }

//  if(activitytimeractive!=FALSE) {
//    AbortIO(&activitytimer_ioreq->tr_node);
//    WaitIO(&activitytimer_ioreq->tr_node);
//
//    activitytimeractive=FALSE;
//  }
}



LONG flushcaches() {
  LONG errorcode;

  /* Flushes any pending changes to disk and ask the user what to do when
     an error occurs. */

  while((errorcode=flushtransaction())!=0 && req("Pending buffers couldn't be flushed\nto the disk because of error %ld.", "Retry|Cancel", errorcode)==1) {
  }

  motoroff();

  return(errorcode);
}



void invalidatecaches() {

  /* Invalidates all caches without flushing.  Call flushcaches()
     first. */

  invalidatecachebuffers();
  invalidateiocaches();
}


void dreq(UBYTE *fmt, ... ) {
  ULONG args[4];
  UBYTE *fmt2;

  if(debugreqs!=FALSE) {
    args[0]=(ULONG)((UBYTE *)BADDR(devnode->dn_Name)+1);
    args[1]=(ULONG)((UBYTE *)BADDR(startupmsg->fssm_Device)+1);
    args[2]=startupmsg->fssm_Unit;
    args[3]=(ULONG)fmt;

    if((fmt2=AllocVec(strlen(fmt)+100,0))!=0) {
      DEBUG(("\nREQUESTER\n\n"));
      RawDoFmt("SmartFilesystem %s: (%s, unit %ld)\n\n%s",args,(void (*)())"\x16\xC0\x4E\x75",fmt2);

      {
        struct EasyStruct es;
        ULONG *args=(ULONG *)&fmt;

        args++;

        es.es_StructSize=sizeof(struct EasyStruct);
        es.es_Flags=0;
        es.es_Title=PROGRAMNAME;
        es.es_TextFormat=fmt2;
        es.es_GadgetFormat="Continue|No more requesters";

        if(EasyRequestArgs(0,&es,0,args)==0) {
          debugreqs=FALSE;
        }
      }

      FreeVec(fmt2);
    }
  }
}



LONG req(UBYTE *fmt, UBYTE *gads, ... ) {
  ULONG args[5];
  ULONG *arg=args;
  UBYTE *fmt2;
  LONG gadget=0;

  /* Simple requester function.  It will put up a requester
     in the form of:

     "Volume 'BOOT' (DH0: scsi.device, unit 0)"

     or:

     "Device DH0: (scsi.device, unit 0)"

     This depends on whether or not there is a valid
     VolumeNode. */

  if(volumenode!=0) {
    *arg++=(ULONG)((UBYTE *)BADDR(volumenode->dl_Name)+1);
  }

  *arg++=(ULONG)((UBYTE *)BADDR(devnode->dn_Name)+1);
  *arg++=(ULONG)((UBYTE *)BADDR(startupmsg->fssm_Device)+1);
  *arg++=startupmsg->fssm_Unit;
  *arg=(ULONG)fmt;

  if((fmt2=AllocVec(strlen(fmt)+100,0))!=0) {

    if(volumenode!=0) {
      RawDoFmt("Volume '%s' (%s: %s, unit %ld)\n\n%s",args,(void (*)())"\x16\xC0\x4E\x75",fmt2);
    }
    else {
      RawDoFmt("Device %s: (%s, unit %ld)\n\n%s",args,(void (*)())"\x16\xC0\x4E\x75",fmt2);
    }

    {
      struct EasyStruct es;
      ULONG *args=(ULONG *)&gads;

      args++;

      es.es_StructSize=sizeof(struct EasyStruct);
      es.es_Flags=0;
      es.es_Title=PROGRAMNAME " request";
      es.es_TextFormat=fmt2;
      es.es_GadgetFormat=gads;

      gadget=EasyRequestArgs(0,&es,0,args);
    }

    FreeVec(fmt2);
  }

  return(gadget);
}



LONG req_unusual(UBYTE *fmt, ... ) {
  ULONG args[5];
  UBYTE *fmt2;
  LONG gadget=0;

  /* Simple requester function. */

  args[0]=(ULONG)((UBYTE *)BADDR(devnode->dn_Name)+1);
  args[1]=(ULONG)((UBYTE *)BADDR(startupmsg->fssm_Device)+1);
  args[2]=startupmsg->fssm_Unit;
  args[3]=(ULONG)fmt;
  args[4]=(ULONG)"This is a safety check requester, which should\n"\
                 "never appear under normal conditions.  Please\n"\
                 "notify the author about the error above and if\n"\
                 "possible under what circumstances it appeared.\n\n"\
                 "BEWARE: SFS might crash if you click Continue.\n"\
                 "        Please save your work first!";

  if((fmt2=AllocVec(strlen(fmt)+400,0))!=0) {

    RawDoFmt("SmartFilesystem %s: (%s, unit %ld)\n\n%s\n\n%s",args,(void (*)())"\x16\xC0\x4E\x75",fmt2);

    {
      struct EasyStruct es;
      ULONG *args=(ULONG *)&fmt;

      args++;

      es.es_StructSize=sizeof(struct EasyStruct);
      es.es_Flags=0;
      es.es_Title=PROGRAMNAME " request";
      es.es_TextFormat=fmt2;
      es.es_GadgetFormat="Continue";

      gadget=EasyRequestArgs(0,&es,0,args);
    }

    FreeVec(fmt2);
  }

  return(gadget);
}


void request2(UBYTE *text) {
  request(PROGRAMNAME, text, "Ok", 0);
}


LONG request(UBYTE *title,UBYTE *fmt,UBYTE *gads,APTR arg1, ... ) {
  struct EasyStruct es;

  es.es_StructSize=sizeof(struct EasyStruct);
  es.es_Flags=0;
  es.es_Title=title;
  es.es_TextFormat=fmt;
  es.es_GadgetFormat=gads;

  return(EasyRequestArgs(0,&es,0,&arg1));
}



void outputcachebuffer(struct CacheBuffer *cb) {
  ULONG *a;
  UWORD n;

  DEBUG(("CacheBuffer at address 0x%08lx of block %ld (Locked = %ld, Bits = 0x%02lx)\n",cb,cb->blckno,(LONG)cb->locked,(LONG)cb->bits));

  a=cb->data;

  for(n=0; n<(bytes_block>>5); n++) {
    DEBUG(("%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]));
    a+=8;
  }
}




LONG readroots(void) {
  struct CacheBuffer *cb1;
  struct CacheBuffer *cb2;
  struct fsRootBlock *rb1;
  struct fsRootBlock *rb2;
  WORD rb1okay=TRUE;
  WORD rb2okay=TRUE;
  LONG errorcode;

  if((errorcode=readcachebuffer(&cb1,0))!=0) {
    return(errorcode);
  }

  lockcachebuffer(cb1);

  if((errorcode=readcachebuffer(&cb2,blocks_total-1))!=0) {
    unlockcachebuffer(cb1);
    return(errorcode);
  }

  unlockcachebuffer(cb1);

  rb1=cb1->data;
  rb2=cb2->data;

  if(checkchecksum(cb1)==DOSFALSE || rb1->bheader.id!=DOSTYPE_ID || rb1->bheader.ownblock!=0) {
    rb1okay=FALSE;
  }

  if(checkchecksum(cb2)==DOSFALSE || rb2->bheader.id!=DOSTYPE_ID || rb2->bheader.ownblock!=rb2->totalblocks-1) {
    rb2okay=FALSE;
  }

  if(rb1okay!=FALSE && rb2okay!=FALSE) {
    /* Both root blocks look okay. */

    /*
    if(rb1->sequencenumber!=rb2->sequencenumber) {
      /* Sequence numbers differ! */
    }
    */

    if(rb1->blocksize!=bytes_block) {
      return(ERROR_NOT_A_DOS_DISK);
    }

    if(rb1->firstbyteh!=byte_lowh || rb1->firstbyte!=byte_low || rb1->lastbyteh!=byte_highh || rb1->lastbyte!=byte_high || rb1->totalblocks!=blocks_total) {
      return(ERROR_NOT_A_DOS_DISK);
    }

    if(rb1->version!=STRUCTURE_VERSION) {
      /* Different version! */

      request(PROGRAMNAME " request","%s\n"\
                                     "is in a format unsupported by this version\n"\
                                     "of the filesystem.  Please reformat the disk or\n"\
                                     "install the correct version of this filesystem.",
                                     "Ok",((UBYTE *)BADDR(devnode->dn_Name))+1);

      return(ERROR_NOT_A_DOS_DISK);
    }

    block_bitmapbase=rb1->bitmapbase;
    block_adminspace=rb1->adminspacecontainer;
    block_root=rb1->rootobjectcontainer;
    block_extentbnoderoot=rb1->extentbnoderoot;
    block_objectnoderoot=rb1->objectnoderoot;

    if((rb1->bits & ROOTBITS_CASESENSITIVE)!=0) {
      is_casesensitive=TRUE;
    }
    else {
      is_casesensitive=FALSE;
    }

    if((rb1->bits & ROOTBITS_RECYCLED)!=0) {
      has_recycled=TRUE;
    }
    else {
      has_recycled=FALSE;
    }
  }
  else {
    errorcode=ERROR_NOT_A_DOS_DISK;
  }

  return(errorcode);
}



struct DeviceList *usevolumenode(UBYTE *name, ULONG creationdate) {
  struct DosList *dol;
  struct DeviceList *vn=0;

  /* This function locates the specified volumenode, and if found
     uses it for the current volume inserted.  If the node is not
     found this function returns 0.  If the specificied volumenode
     is found, but is found to be in use, then this function
     returns -1.  Otherwise the found volumenode is returned. */

  dol=LockDosList(LDF_READ|LDF_VOLUMES);

  while((dol=FindDosEntry(dol, name, LDF_VOLUMES))!=0) {
    if(datestamptodate(&dol->dol_misc.dol_volume.dol_VolumeDate)==creationdate) {                  // Do volumes have same creation date?
      Forbid();
      if(dol->dol_misc.dol_volume.dol_LockList!=0 || ((struct DeviceList *)dol)->dl_unused!=0) {   // Is volume not in use?
        struct fsNotifyRequest *nr;
        struct ExtFileLock *lock;

        /* Volume is not currently in use, so grab locklist & notifyrequests and patch fl_Task fields */

        DEBUG(("usevolumenode: Found DosEntry with same date, and locklist!=0\n"));

        lock=(struct ExtFileLock *)dol->dol_misc.dol_volume.dol_LockList;
        nr=(struct fsNotifyRequest *)(((struct DeviceList *)dol)->dl_unused);
        dol->dol_misc.dol_volume.dol_LockList=0;
        ((struct DeviceList *)dol)->dl_unused=0;

        Permit();

        locklist=lock;
        notifyrequests=nr;

        while(lock!=0) {
          lock->task=&mytask->pr_MsgPort;
          lock=lock->next;
        }

        while(nr!=0) {
          nr->nr_Handler=msgportnotify;
          nr=nr->next;
        }

        vn=(struct DeviceList *)dol;
        break;
      }
      else {
        Permit();

        DEBUG(("usevolumenode: Found DosEntry with same date, but it is in use!\n"));

        vn=(struct DeviceList *)-1;
      }
    }

    /* Volume nodes are of different date, continue search */
    dol=NextDosEntry(dol, LDF_VOLUMES);
  }

  UnLockDosList(LDF_READ|LDF_VOLUMES);

  return(vn);
}



LONG initdisk() {

  /* This routine is called whenever a disk has changed (via ACTION_INHIBIT or
     on startup).  The routine scans the disk and initializes the necessary
     variables.  Steps:

     - Check if the disk is a valid DOS disk
         If so, get name and datestamp of the disk
             If valid, look for volume node of the same name and datestamp
                If so, retrieve locklist from dol_LockList (fix fl_Task fields!)
                and use existing volume node.
                If not, a new volume node is created.
                If the disk carries the same attributes as another ACTIVE volume
                the handler ignores the newly inserted volume.
             If not valid, R/W error requester -> "NDOS"
         If not valid type is "NDOS"

  */

  changegeometry(dosenvec);

  if(volumenode==0) {
    struct CacheBuffer *cb;
    struct fsObjectContainer *oc=0;
    ULONG newdisktype;
    LONG errorcode;

    diskstate=writeprotection();

    if((errorcode=readroots())==0) {

      /* Root blocks are valid for this filesystem */

      DEBUG(("Initdisk: Root blocks read\n"));

      #ifdef STARTDEBUG
        dreq("Root blocks are okay!");
      #endif

      if((errorcode=checkfortransaction())==0) {

        DEBUG(("Initdisk: Checked for an old Transaction and applied it if it was present.\n"));

        if((errorcode=readcachebuffercheck(&cb,block_root,OBJECTCONTAINER_ID))==0) {
          struct fsRootInfo *ri=(struct fsRootInfo *)((UBYTE *)cb->data+bytes_block-sizeof(struct fsRootInfo));
          ULONG blocksfree=0;

          lockcachebuffer(cb);
          oc=cb->data;

          DEBUG(("Initdisk: '%s' was inserted\n",oc->object[0].name));

          /* ROOT block is valid for this filesystem */

          /* We should count the number of set bits in the bitmap now */

          {
            struct CacheBuffer *cb;
            struct fsBitmap *b;
            BLCK bitmapblock=block_bitmapbase;
            UWORD cnt=blocks_bitmap;
            WORD n;

            while(cnt-->0 && (errorcode=readcachebuffercheck(&cb,bitmapblock,BITMAP_ID))==0) {
              b=cb->data;

              for(n=0; n<((bytes_block-sizeof(struct fsBitmap))>>2); n++) {
                if(b->bitmap[n]!=0) {
                  if(b->bitmap[n]==0xFFFFFFFF) {
                    blocksfree+=32;
                  }
                  else {
                    blocksfree+=bfco(b->bitmap[n]);
                  }
                }
              }
              bitmapblock++;
            }
          }

          unlockcachebuffer(cb);

          DEBUG(("Initdisk: Traversed bitmap, found %ld free blocks\n",blocksfree));

          if(errorcode==0 && ri->freeblocks!=blocksfree) {
            if(ri->freeblocks!=0) {
              dreq("The number of free blocks (%ld) is incorrect.\n"\
                   "According to the bitmap it should be %ld.\n"\
                   "The number of free blocks will now be updated.", ri->freeblocks, blocksfree);
            }

            newtransaction();

            preparecachebuffer(cb);

            ri->freeblocks=blocksfree;

            if((errorcode=storecachebuffer(cb))==0) {
              endtransaction();
            }
            else {
              deletetransaction();
            }
          }

          if(errorcode==0) {
            DEBUG(("Initdisk: A valid DOS disk\n"));

            newdisktype=ID_DOS_DISK;

            #ifdef STARTDEBUG
              dreq("There is a valid SFS disk present!");
            #endif
          }
          else {

            #ifdef STARTDEBUG
              dreq("SFS disk is invalid; bitmap error.");
            #endif

            newdisktype=ID_NOT_REALLY_DOS;
            errorcode=ERROR_NOT_A_DOS_DISK;
          }
        }
        else {

          #ifdef STARTDEBUG
            dreq("SFS disk is invalid; root objectcontainer error.");
          #endif

          newdisktype=ID_NOT_REALLY_DOS;
          errorcode=ERROR_NOT_A_DOS_DISK;
        }
      }
      else {

        #ifdef STARTDEBUG
          dreq("SFS disk is invalid; transaction error.");
        #endif

        newdisktype=ID_NOT_REALLY_DOS;
        errorcode=ERROR_NOT_A_DOS_DISK;
      }
    }
    else if(errorcode==INTERR_CHECKSUM_FAILURE || errorcode==ERROR_NOT_A_DOS_DISK) {
      newdisktype=ID_NOT_REALLY_DOS;
      errorcode=ERROR_NOT_A_DOS_DISK;

      #ifdef STARTDEBUG
        dreq("SFS disk is invalid; checksum failure.");
      #endif
    }
    else if(errorcode==INTERR_BLOCK_WRONG_TYPE) {
      #ifdef STARTDEBUG
        dreq("SFS disk is invalid; wrong block type.");
      #endif

      newdisktype=ID_NOT_REALLY_DOS;
      errorcode=ERROR_NOT_A_DOS_DISK;
    }
    else if(errorcode==TDERR_DiskChanged) {
      #ifdef STARTDEBUG
        dreq("SFS disk is invalid; disk was changed.");
      #endif

      newdisktype=ID_NO_DISK_PRESENT;
      errorcode=ERROR_NO_DISK;
    }
    else {
      #ifdef STARTDEBUG
        dreq("SFS disk is invalid; unreadable disk.");
      #endif

      newdisktype=ID_UNREADABLE_DISK;
      errorcode=ERROR_NOT_A_DOS_DISK;
    }

    if(errorcode==0) {
      struct DeviceList *vn;
      struct fsRootInfo *ri=(struct fsRootInfo *)((UBYTE *)oc+bytes_block-sizeof(struct fsRootInfo));

      DEBUG(("initdisk: Checking for an existing volume-node\n"));

      if((vn=usevolumenode(oc->object[0].name, ri->datecreated))!=(struct DeviceList *)-1) {
        if(vn==0) {
          /* VolumeNode was not found, so we need to create a new one. */

          DEBUG(("initdisk: No volume-node found, creating new one instead.\n"));

          if((vn=(struct DeviceList *)MakeDosEntry("                              ",DLT_VOLUME))!=0) {
            struct SFSMessage *sfsm;
            UBYTE *d2=(UBYTE *)BADDR(vn->dl_Name);
            UBYTE *d=d2+1;
            UBYTE *s=oc->object[0].name;
            UBYTE len=0;

            while(*s!=0 && len<30) {
              *d++=*s++;
              len++;
            }

            *d=0;
            *d2=len;

            datetodatestamp(ri->datecreated, &vn->dl_VolumeDate);

            DEBUG(("initdisk: Sending msg.\n"));

            if((sfsm=AllocVec(sizeof(struct SFSMessage), MEMF_CLEAR))!=0) {
              sfsm->command=SFSM_ADD_VOLUMENODE;
              sfsm->data=(ULONG)vn;
              sfsm->msg.mn_Length=sizeof(struct SFSMessage);

              PutMsg(sdlhport, (struct Message *)sfsm);
            }
          }
          else {
            errorcode=ERROR_NO_FREE_STORE;
          }
        }

        DEBUG(("initdisk: Using new or old volumenode.\n"));

        if(errorcode==0) {    /* Reusing the found VolumeNode or using the new VolumeNode */
          vn->dl_Task=devnode->dn_Task;
          vn->dl_DiskType=dosenvec->de_DosType;
          volumenode=vn;
        }
      }
      else { /* Volume is in use by another handler -- stay off */
        DEBUG(("Initdisk: Found DosEntry with same date, and locklist==0\n"));
        newdisktype=ID_NO_DISK_PRESENT;             /* Hmmm... EXTREMELY unlikely, but may explain the strange bug Laire had. */
        errorcode=ERROR_NO_DISK;
     //   vn=0;
        volumenode=0;
      }
    }

    if(errorcode==0) {
      diskchangenotify(IECLASS_DISKINSERTED);
    }

    disktype=newdisktype;

    return(errorcode);
  }
  else {
    return(0);
  }
}



struct DosList *attemptlockdoslist(LONG tries, LONG delay) {
  struct DosList *dol;
  struct DosPacket *dp;

  for(;;) {
    dol=AttemptLockDosList(LDF_WRITE|LDF_VOLUMES);

    if(((ULONG)dol & ~1)!=0) {
      return(dol);
    }

    if(--tries<=0) {
      return(0);
    }

    if((dp=getpacket(mytask))!=0) {
      if(handlesimplepackets(dp)==0) {
        dumppackets(dp, ERROR_NOT_A_DOS_DISK);
      }
    }
    else {
      Delay(delay);
    }
  }
}


void removevolumenode(struct DosList *dol, struct DosList *vn) {
  while((dol=NextDosEntry(dol, LDF_VOLUMES))!=0) {
    if(dol==vn) {
      RemDosEntry(dol);
      break;
    }
  }
}


void deinitdisk() {

  /* This function flushes all caches, and then invalidates them.
     If succesful, this function then proceeds to either remove
     the volumenode, or transfer any outstanding locks/notifies to
     it.  Finally it notifies the system of the disk removal. */

  DEBUG(("deinitdisk: entry\n"));

  flushcaches();
  invalidatecaches();

  if(volumenode!=0) {

    /* We first check if the VolumeNode needs to be removed; if not
       then we donot lock the DosList and modify some of its fields.
       If it must be removed, we first attempt to lock the DosList
       synchronously; whether this fails or not, we always send a
       removal message to the DosList subtask. */

    if(locklist!=0 || notifyrequests!=0) {

      /* There are locks or notifyrequests, so we cannot kill the volumenode. */

      /* We haven't got the DosList locked here, but I think it is fairly
         safe to modify these fields directly. */

      Forbid();

      volumenode->dl_Task=0;    /* This seems to be necessary */
      volumenode->dl_LockList=(LONG)locklist;
      volumenode->dl_unused=(LONG)notifyrequests;

      Permit();

      locklist=0;
      notifyrequests=0;
    }
    else {
      struct SFSMessage *sfsm;
      struct DosList *dol=attemptlockdoslist(5, 1);

      DEBUG(("deinitdisk: dol = %ld\n", dol));

      if(dol!=0) {   /* Is DosList locked? */
        removevolumenode(dol, (struct DosList *)volumenode);
        UnLockDosList(LDF_WRITE|LDF_VOLUMES);
      }

      DEBUG(("deinitdisk: sending msg\n"));

      /* Even if we succesfully locked the DosList, we still should notify
         the DosList task to remove the node as well (and to free it), just
         in case a VolumeNode Add was still pending. */

      if((sfsm=AllocVec(sizeof(struct SFSMessage), MEMF_CLEAR))!=0) {
        sfsm->command=SFSM_REMOVE_VOLUMENODE;
        sfsm->data=(ULONG)volumenode;
        sfsm->msg.mn_Length=sizeof(struct SFSMessage);

        PutMsg(sdlhport, (struct Message *)sfsm);
      }
    }

    DEBUG(("deinitdisk: done\n"));

    volumenode=0;

    diskchangenotify(IECLASS_DISKREMOVED);
  }
}



LONG handlesimplepackets(struct DosPacket *packet) {
  LONG type=packet->dp_Type;  /* After returnpacket, packet->dp_Type is invalid! */

  switch(packet->dp_Type) {
  case ACTION_IS_FILESYSTEM:
    returnpacket2(packet, DOSTRUE,0);
    break;
  case ACTION_GET_DISK_FSSM:
    returnpacket2(packet, (LONG)startupmsg,0);
    break;
  case ACTION_FREE_DISK_FSSM:
    returnpacket2(packet, DOSTRUE,0);
    break;
  case ACTION_DISK_INFO:
    actiondiskinfo(packet);
    break;
  case ACTION_SAME_LOCK:
    actionsamelock(packet);
    break;
  case ACTION_CURRENT_VOLUME:
    actioncurrentvolume(packet);
    break;
  default:
    return(0);
  }

  return(type);
}



LONG dumppackets(struct DosPacket *packet,LONG returncode) {
  LONG type=packet->dp_Type;  /* After returnpacket, packet->dp_Type is invalid! */

  /* Routine which returns ERROR_NOT_A_DOS_DISK for all known
     packets which cannot be handled under such a situation */

  switch(type) {
  case ACTION_READ:
  case ACTION_WRITE:
  case ACTION_SEEK:
  case ACTION_SET_FILE_SIZE:
    returnpacket2(packet, -1,returncode);
    break;
  case ACTION_CREATE_DIR:
  case ACTION_FINDUPDATE:
  case ACTION_FINDINPUT:
  case ACTION_FINDOUTPUT:
  case ACTION_END:
  case ACTION_EXAMINE_OBJECT:
  case ACTION_EXAMINE_NEXT:
  case ACTION_COPY_DIR:
  case ACTION_LOCATE_OBJECT:
  case ACTION_PARENT:          /* till here verified to return 0 on error! */
  case ACTION_LOCK_RECORD:
  case ACTION_FREE_RECORD:
  case ACTION_FREE_LOCK:
  case ACTION_CHANGE_MODE:
  case ACTION_FH_FROM_LOCK:
  case ACTION_COPY_DIR_FH:
  case ACTION_PARENT_FH:
  case ACTION_EXAMINE_FH:
  case ACTION_EXAMINE_ALL:
  case ACTION_DELETE_OBJECT:
  case ACTION_RENAME_OBJECT:
  case ACTION_MAKE_LINK:
  case ACTION_READ_LINK:
  case ACTION_SET_COMMENT:
  case ACTION_SET_DATE:
  case ACTION_SET_PROTECT:
  case ACTION_INFO:
  case ACTION_RENAME_DISK:
  case ACTION_SERIALIZE_DISK:
  case ACTION_GET_DISK_FSSM:
  case ACTION_MORE_CACHE:
  case ACTION_WRITE_PROTECT:
  case ACTION_ADD_NOTIFY:
  case ACTION_REMOVE_NOTIFY:
  case ACTION_INHIBIT:        /* These packets are only dumped when doslist is locked */
  case ACTION_FORMAT:
    returnpacket2(packet, DOSFALSE,returncode);
    break;
  default:
    returnpacket2(packet, DOSFALSE,ERROR_ACTION_NOT_KNOWN);
    break;
  }

  return(type);
}


void dumppacket() {
  struct DosPacket *packet;

  /* routine which gets a packet and returns ERROR_NOT_A_DOS_DISK for all
     known packets which cannot be executed while we are attempting to access
     the doslist.  Some packets which don't require disk access will be
     handled as normal. */

  packet=waitpacket(mytask);

  if(handlesimplepackets(packet)==0) {
    dumppackets(packet,ERROR_NOT_A_DOS_DISK);
  }
}



void actioncurrentvolume(struct DosPacket *packet) {
  struct ExtFileLock *lock=(struct ExtFileLock *)packet->dp_Arg1;

  DEBUG(("ACTION_CURRENT_VOLUME(%ld)\n",lock));

  if(lock==0) {
    DEBUG(("ACTION_CURRENT_VOLUME: volumenode = %ld\n",volumenode));
    returnpacket2(packet, TOBADDR(volumenode),startupmsg->fssm_Unit);
  }
  else {
    returnpacket2(packet, lock->volume,startupmsg->fssm_Unit);
  }
}



void actionsamelock(struct DosPacket *packet) {
  struct ExtFileLock *lock;
  struct ExtFileLock *lock2;

  lock=(struct ExtFileLock *)BADDR(packet->dp_Arg1);
  lock2=(struct ExtFileLock *)BADDR(packet->dp_Arg2);

  if(lock->objectnode==lock2->objectnode && lock->task==lock2->task) {
    returnpacket2(packet, DOSTRUE,0);
    return;
  }
  returnpacket2(packet, DOSFALSE,0);
}



void actiondiskinfo(struct DosPacket *packet) {
  struct InfoData *id=BADDR(packet->dp_Arg1);

//  DEBUG(("ACTION_DISK_INFO\n"));

  fillinfodata(id);

  returnpacket2(packet, DOSTRUE,0);
}



void fillinfodata(struct InfoData *id) {
  ULONG usedblocks;

  id->id_NumSoftErrors=numsofterrors;
  id->id_UnitNumber=startupmsg->fssm_Unit;
  id->id_DiskState=diskstate;
  id->id_NumBlocks=blocks_total-blocks_reserved_start-blocks_reserved_end;

  usedblocks=id->id_NumBlocks;

  if(disktype==ID_DOS_DISK) {
    ULONG deletedfiles, deletedblocks;

    getusedblocks(&usedblocks);
    if(getrecycledinfo(&deletedfiles, &deletedblocks)==0) {
      usedblocks-=deletedblocks;
    }
  }

  id->id_NumBlocksUsed=usedblocks;
  id->id_BytesPerBlock=bytes_block;
  id->id_DiskType=disktype;

//  kprintf("Filling InfoData structure with a volumenode ptr to address %ld.  Disktype = 0x%08lx\n",volumenode,disktype);

  id->id_VolumeNode=TOBADDR(volumenode);

//  DEBUG(("fillinfodata: volumenode = %ld, disktype = 0x%08lx\n",volumenode,disktype));

  if(locklist!=0) {
    id->id_InUse=DOSTRUE;
  }
  else {
    id->id_InUse=DOSFALSE;
  }
}



BOOL checkchecksum(struct CacheBuffer *cb) {

#ifdef CHECKCHECKSUMSALWAYS
  if(CALCCHECKSUM(bytes_block,cb->data)==0) {
#else
  if((cb->bits & CB_CHECKSUM)!=0 || CALCCHECKSUM(bytes_block,cb->data)==0) {   //    -> copycachebuffer screws this up!
#endif
    cb->bits|=CB_CHECKSUM;
    return(DOSTRUE);
  }
  return(DOSFALSE);
}



void setchecksum(struct CacheBuffer *cb) {
  struct fsBlockHeader *bh=cb->data;

  bh->checksum=0;    /* Important! */
  bh->checksum=-CALCCHECKSUM(bytes_block,cb->data);
}



LONG readcachebuffercheck(struct CacheBuffer **returnedcb,ULONG blckno,ULONG type) {
  LONG errorcode;
  struct fsBlockHeader *bh;

  while((errorcode=readcachebuffer(returnedcb,blckno))==0) {
    bh=(*returnedcb)->data;
    if(type!=0 && bh->id!=type) {
      dumpcachebuffers();
      outputcachebuffer(*returnedcb);
      emptycachebuffer(*returnedcb);

      if(blckno!=0) {
        if(request(PROGRAMNAME " request","%s\n"\
                                          "has a blockid error in block %ld.\n"\
                                          "Expected was blockid 0x%08lx,\n"\
                                          "but the block says it is blockid 0x%08lx.",
                                          "Reread|Cancel",((UBYTE *)BADDR(devnode->dn_Name))+1,blckno,type,bh->id)<=0) {
          return(INTERR_BLOCK_WRONG_TYPE);
        }
      }
      else {
/*        if(request(PROGRAMNAME " request","%s\n"\
                                          "is not a DOS disk.",
                                          "Retry|Cancel",((UBYTE *)BADDR(devnode->dn_Name))+1,blckno,type,bh->id)<=0) {
          return(INTERR_BLOCK_WRONG_TYPE);
        } */

        return(INTERR_BLOCK_WRONG_TYPE);
      }
      continue;
    }
    if(checkchecksum(*returnedcb)==DOSFALSE) {
      dumpcachebuffers();
      outputcachebuffer(*returnedcb);
      emptycachebuffer(*returnedcb);

      if(request(PROGRAMNAME " request","%s\n"\
                                        "has a checksum error in block %ld.",
                                        "Reread|Cancel",((UBYTE *)BADDR(devnode->dn_Name))+1,blckno)<=0) {
        return(INTERR_CHECKSUM_FAILURE);
      }
      continue;
    }
    if(bh->ownblock!=blckno) {
      dumpcachebuffers();
      outputcachebuffer(*returnedcb);
      emptycachebuffer(*returnedcb);

      if(request(PROGRAMNAME " request","%s\n"\
                                        "has a block error in block %ld.\n"\
                                        "Expected was block %ld,\n"\
                                        "but the block says it is block %ld.",
                                        "Reread|Cancel",((UBYTE *)BADDR(devnode->dn_Name))+1,blckno,blckno,bh->ownblock)<=0) {
        return(INTERR_OWNBLOCK_WRONG);
      }
      continue;
    }
    break;
  }
  return(errorcode);
}



/* How the CacheBuffers work:
   ==========================

When the filesystem starts the number of buffers indicated
by the Mountlist are allocated.  If the number of buffers
is below the minimum then the filesystem will increase the
number of buffers to the minimum.  AddBuffers can be used
later to change the number of CacheBuffers.

Each CacheBuffer is linked into two chains.  An LRU chain,
to quickly determine the least recently used CacheBuffer,
and a Hash chain.  The Hash chain is used to quickly locate
a CacheBuffer which contains a specific block.  Empty
cachebuffers are not linked in the Hash chain and have a
blckno of zero.

After having read a few blocks there comes a time when you
want to modify them.  The CacheBuffer systems helps to make
it simple to do safe updates to blocks without ever running
the risk of leaving the disk in a state where links or
pointers are not yet valid.  If you want to modify a block
or series of blocks you need to tell this to the caching
system by calling newtransaction().  An operation is a series
of modifications which, when written to disk completely,
would result in a valid disk.  By calling newtransaction() you
signal that you are about to start a series of modifications
which together will again result in a valid disk.

If however at any point during an operation somekind of
error occurs which prevents you from finishing the operation
you should call deletetransaction().  This will remove all
changes you made from the point when you called
newtransaction() from the cache.  These blocks will now never
get written to disk, so there is no chance that the disk
becomes invalid by partial modifications.

This sums up how the caching mechanism works.  The internal
workings of routines like newtransaction() and
deletetransaction() will be explained later on.  One very
important routine hasn't been mentioned here:
flushtransaction().  This is the routine which actually makes
modifications to the disk.  It does this by flushing one
operation at a time to disk (in old to new order of course)
in such a way that even if the machine crashed in the middle
of the flush that the disk would remain valid.  After
flushing all operations the cache is cleaned up.

*/




/* Creating and moving objects

   1. Changing the comment on an object
   2. Changing the name of an object
   3. Creating a new object
  (4. Moving an object)

Each of these three operations involves one or more of the following:

   1,2,3,4 - Locating enough new room for the object
   1,2,3,4 - Making sure the object-node points to the new block the
             object is located in.
   2 & 3   - Hashing the object
   3       - Allocating an ObjectNode.

Order:

'Allocating an ObjectNode' always precedes 'Hashing the Object' and 'Fixing the objectnode pointer'.
'FindObjectSpace' always precedes 'Fixing the objectnode pointer'.

 1. FindObjectSpace OR Allocating an ObjectNode
 2. Fixing the objectnode pointer OR Hashing the Object */



void fixlocks(struct GlobalHandle *gh,ULONG lastextentkey,ULONG lastextentoffset,ULONG filelength) {
  struct ExtFileLock *lock=locklist;

  while(lock!=0) {
    if(lock->gh==gh) {
      if(filelength==0) {
        lock->offset=0;
        lock->extentoffset=0;
        lock->curextent=0;
      }
      else if(lock->offset>filelength) {
        /* Hmmm, this lock is positioned beyond the EOF... */

        if(lock->offset-lock->extentoffset >= filelength) {
          lock->extentoffset=filelength-(lock->offset-lock->extentoffset);
        }
        else {
          lock->extentoffset=filelength-lastextentoffset;
          lock->curextent=lastextentkey;
        }
        lock->offset=filelength;
      }
    }
    lock=lock->next;
  }
}



LONG extendblocksinfile(struct ExtFileLock *lock, ULONG blocks) {
  BLCK lastextentbnode;
  LONG errorcode;

  /* This function extends the file identified by the lock with a number
     of blocks.  Only new Extents will be created -- the size of the file
     will not be altered, and changing it is left up to the caller.  If
     the file did not have any blocks yet, then lock->curextent will be
     set to the first (new) ExtentBNode.  It's left up up to the caller
     to reflect this change in object.file.data.

     This function must be called from within a transaction. */

  XDEBUG((DEBUG_IO,"extendblocksinfile: Increasing number of blocks by %ld.  lock->curextent = %ld\n",blocks,lock->curextent));

  if((errorcode=seektocurrent(lock))==0) {

    if((lastextentbnode=lock->curextent)!=0) {
      struct CacheBuffer *cb;
      struct fsExtentBNode *ebn;

      while((errorcode=findextentbnode(lastextentbnode,&cb,&ebn))==0 && ebn->next!=0) {
        lastextentbnode=ebn->next;
      }
    }

    if(errorcode==0) {
      struct GlobalHandle *gh=lock->gh;

      /* We found the last Extent.  Now we should see if we can extend it
         or if we need to add a new one and attach it to this one. */

      while(blocks!=0) {
        struct Space *sl=spacelist;
        BLCK searchstart;

        if(lastextentbnode!=0) {
          struct CacheBuffer *cb;
          struct fsExtentBNode *ebn;

          if((errorcode=findextentbnode(lastextentbnode,&cb,&ebn))!=0) {
            break;
          }
          searchstart=ebn->key+ebn->blocks;
        }
        else {
          searchstart=block_rovingblockptr;
        }

        if((errorcode=smartfindandmarkspace(searchstart,blocks))!=0) {   /* only within transaction! */
          DEBUG(("extendblocksinfile: sfams returned errorcode %ld\n", errorcode));
          break;
        }

        XDEBUG((DEBUG_IO,"extendblocksinfile: Found some space!  blocks = %ld\n",blocks));

        while(blocks!=0 && sl->block!=0) {
          BLCK newspace=sl->block;
          ULONG newspaceblocks;

          newspaceblocks=sl->blocks > blocks ? blocks : sl->blocks;

          while(newspaceblocks!=0) {
            ULONG extentblocks;

            extentblocks=newspaceblocks > 8192 ? 8192 : newspaceblocks;

            newspaceblocks-=extentblocks;

            XDEBUG((DEBUG_IO,"extendblocksinfile: sl->block = %ld, lastextentbnode = %ld, extentblocks = %ld\n",sl->block,lastextentbnode,extentblocks));

            if((errorcode=addblocks(extentblocks, newspace, gh->objectnode, &lastextentbnode))!=0) {
              DEBUG(("extendblocksinfile: addblocks returned errorcode %ld\n", errorcode));
              return(errorcode);
            }

            if(lock->curextent==0) {
              lock->curextent=lastextentbnode;
            }

            lock->lastextendedblock=newspace + extentblocks;

            XDEBUG((DEBUG_IO,"extendblocksinfile: Done adding or extending an extent -- blocks = %ld, extentblocks = %ld\n",blocks,extentblocks));

            blocks-=extentblocks;
            newspace+=extentblocks;
          }
          sl++;
        }
      }
    }
  }

  return(errorcode);
}



LONG truncateblocksinfile(struct ExtFileLock *lock,ULONG blocks,ULONG *lastextentkey,ULONG *lastextentoffset) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  ULONG offset;
  ULONG extentoffset;
  LONG errorcode;

  /* Truncates the specified file to /blocks/ blocks.  This function does not
     take care of setting the filesize.  It also doesn't check for any locks
     which have a fileptr which is beyond the end of the file after calling
     this function.  Its sole purpose is to reduce the number of blocks in
     the Extent list. */

  offset=blocks<<shifts_block;

  DEBUG(("truncateblocksinfile: truncating file by %ld blocks\n",blocks));

  if((errorcode=seekextent(lock,offset,&cb,&ebn,&extentoffset))==0) {

    DEBUG(("truncateblocksinfile: offset = %ld, extentoffset = %ld\n",offset,extentoffset));

    if(offset-extentoffset==0) {
      ULONG prevkey=ebn->prev;

      DEBUG(("truncateblocksinfile: prevkey = %ld\n",prevkey));

      /* Darn.  This Extent needs to be killed completely, meaning that we
         have to set the Next pointer of the previous Extent to zero. */

      deleteextents(ebn->key);

      if((prevkey & 0x80000000)==0) {
        if((errorcode=findextentbnode(prevkey,&cb,&ebn))==0) {
          preparecachebuffer(cb);

          ebn->next=0;

          errorcode=storecachebuffer(cb);

          *lastextentkey=prevkey;
          *lastextentoffset=extentoffset-(ebn->blocks<<shifts_block);
        }
      }
      else {
        struct fsObject *o;

        if((errorcode=readobject((prevkey & 0x7fffffff), &cb, &o))==0) {
          preparecachebuffer(cb);

          DEBUG(("truncateblocksinfile: cb->blckno = %ld\n",cb->blckno));

          o->object.file.data=0;
          lock->gh->data=0;

          errorcode=storecachebuffer(cb);

          *lastextentkey=0;
          *lastextentoffset=0;
        }
      }
    }
    else {
      ULONG newblocks=blocks-(extentoffset>>shifts_block);

      *lastextentkey=ebn->key;
      *lastextentoffset=extentoffset;

      DEBUG(("truncateblocksinfile: newblocks = %ld, ebn->blocks = %ld\n",newblocks,ebn->blocks));

      if(newblocks!=ebn->blocks) {
        /* There is only one case where newblocks could equal en->blocks here,
           and that is if we tried to truncate the file to the same number of
           blocks the file already had! */

        lockcachebuffer(cb);

        if((errorcode=freespace(ebn->key+newblocks,ebn->blocks-newblocks))==0) {
          BLCK next=ebn->next;

          preparecachebuffer(cb);

          ebn->blocks=newblocks;
          ebn->next=0;

          if((errorcode=storecachebuffer(cb))==0) {
            errorcode=deleteextents(next);   // Careful, deleting BNode's may change the location of other BNode's as well!
          }
        }

        unlockcachebuffer(cb);
      }
    }
  }

  return(errorcode);
}



LONG setfilesize(struct ExtFileLock *lock, ULONG bytes) {
  struct GlobalHandle *gh=lock->gh;
  LONG errorcode=0;

  /* This function sets the filesize of a file to /bytes/ bytes.  If the
     file is not yet long enough, additional blocks are allocated and
     added to the file.  If the file is too long the file will be shortened
     and the lost blocks are freed.

     Any filehandles using this file are set to the EOF if their current
     position would be beyond the new EOF when truncating the file.

     In any other case the position of the file ptr is not altered. */

  DEBUG(("setfilesize: gh = 0x%08lx.  Setting to %ld bytes. Current size is %ld bytes.\n",gh,bytes,gh->size));

  if(bytes!=gh->size) {
    LONG blocksdiff;
    LONG curblocks;

    curblocks=(gh->size+bytes_block-1)>>shifts_block;
    blocksdiff=((bytes+bytes_block-1)>>shifts_block) - curblocks;

    DEBUG(("setfilesize: blocksdiff = %ld\n",blocksdiff));

    if(blocksdiff>0) {

      newtransaction();

      if((errorcode=extendblocksinfile(lock, blocksdiff))==0) {
        struct CacheBuffer *cb;
        struct fsObject *o;

        /* File (now) has right amount of blocks, only exact size in bytes may differ */

        if((errorcode=readobject(gh->objectnode, &cb, &o))==0) {
          preparecachebuffer(cb);

          o->object.file.size=bytes;
          if(o->object.file.data==0) {
            o->object.file.data=lock->curextent;
          }

          errorcode=storecachebuffer(cb);
        }
      }

      if(errorcode==0) {
        endtransaction();
        gh->size=bytes;
        if(gh->data==0) {
          gh->data=lock->curextent;
        }
      }
      else {
        deletetransaction();
      }
    }
    else {
      ULONG lastextentkey=0;
      ULONG lastextentoffset=0;

      newtransaction();

      if(blocksdiff<0) {
        /* File needs to be shortened by -/blocksdiff/ blocks. */

        DEBUG(("setfilesize: Decreasing number of blocks\n"));

        errorcode=truncateblocksinfile(lock,curblocks+blocksdiff,&lastextentkey,&lastextentoffset);
      }

      DEBUG(("setfilesize: lastextentkey = %ld, lastextentoffset = %ld\n",lastextentkey,lastextentoffset));

      if(errorcode==0) {
        struct CacheBuffer *cb;
        struct fsObject *o;

        /* File (now) has right amount of blocks, only exact size in bytes may differ */

        if((errorcode=readobject(gh->objectnode,&cb,&o))==0) {
          preparecachebuffer(cb);

          DEBUG(("setfilesize: gh->objectnode = %ld, cb->blcno = %ld\n",gh->objectnode,cb->blckno));

          o->object.file.size=bytes;

          errorcode=storecachebuffer(cb);
        }
      }

      if(errorcode==0) {
        endtransaction();
        gh->size=bytes;

        fixlocks(gh,lastextentkey,lastextentoffset,bytes);
      }
      else {
        deletetransaction();
      }
    }

    if(errorcode==0) {
      lock->bits|=EFL_MODIFIED;
    }
  }

  return(errorcode);
}



void seekforward(struct ExtFileLock *lock, UWORD ebn_blocks, BLCK ebn_next, ULONG bytestoseek) {

  /* This function does a simple forward seek.  It assumes /bytestoseek/ is
     only large enough to skip within the current extent or to the very
     beginning of the next extent. */

  lock->offset+=bytestoseek;
  lock->extentoffset+=bytestoseek;
  if(lock->extentoffset >= ebn_blocks<<shifts_block && ebn_next!=0) {
    lock->extentoffset=0;
    lock->curextent=ebn_next;
  }
}



LONG seektocurrent(struct ExtFileLock *lock) {
  LONG errorcode=0;

  /* This function checks if the currentextent is still valid.  If not,
     it will attempt to locate the currentextent. */

  if(lock->curextent==0) {
  
    /* lock->curextent==0 can indicate 2 things:

       - A previously empty file was extended by another handle; in this
         case the file-ptr (lock->offset) of this handle is still zero.
  
       - The extent has been moved by the defragmenter, and will have to
         be re-located.  In this case the file-ptr doesn't have to be
         zero. */
  
    if(lock->offset==0) {
      lock->curextent=lock->gh->data;
    }
    else {
      struct CacheBuffer *cb;
      struct fsExtentBNode *ebn;
      ULONG extentoffset;

      if((errorcode=seekextent(lock, lock->offset, &cb, &ebn, &extentoffset))==0) {
        lock->curextent=ebn->key;
        lock->extentoffset=lock->offset - extentoffset;
      }
    }
  }
  
  return(errorcode);
}
  
  
LONG seek(struct ExtFileLock *lock,ULONG offset) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  ULONG extentoffset;
  LONG errorcode;
  
  /* This function moves the file-ptr to the specified absolute file
     position.  It will return an error if you try to seek beyond the
     end of the file. */
  
  XDEBUG((DEBUG_SEEK,"seek: Attempting to seek to %ld\n",offset));
  
  if((errorcode=seekextent(lock,offset,&cb,&ebn,&extentoffset))==0) {
    lock->curextent=ebn->key;
    lock->extentoffset=offset-extentoffset;
    lock->offset=offset;
  
    XDEBUG((DEBUG_SEEK,"seek: lock->curextent = %ld, lock->extentoffset = %ld, lock->offset = %ld\n",lock->curextent,lock->extentoffset,lock->offset));
  }

  XDEBUG((DEBUG_SEEK,"seek: Exiting with errorcode %ld\n",errorcode));

  return(errorcode);
}
  
  
  
LONG deleteextents(ULONG key) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode=0;
  
  /* Deletes an fsExtentBNode structure by key and any fsExtentBNodes linked to it.
     This function DOES NOT fix the next pointer in a possible fsExtentBNode which
     might have been pointing to the first BNode we are deleting.  Make sure you check
     this yourself, if needed.

     If key is zero, than this function does nothing.
  
     newtransaction() should have been called prior to calling this function. */
  
  XDEBUG((DEBUG_NODES,"deleteextents: Entry -- deleting extents from key %ld\n",key));
  
  
  while(key!=0 && (errorcode=findbnode(block_extentbnoderoot,key,&cb,(struct BNode **)&ebn))==0) {
    /* node to be deleted located. */
  
    // XDEBUG((DDEBUG_NODES,"deleteextents: Now deleting key %ld.  Next key is %ld\n",key,ebn->next));
  
    key=ebn->next;
  
    lockcachebuffer(cb);     /* Makes sure freespace() doesn't reuse this cachebuffer */

    if((errorcode=freespace(ebn->key,ebn->blocks))==0) {
      unlockcachebuffer(cb);

      // XDEBUG((DDEBUG_NODES,"deleteextents: deletebnode from root %ld, with key %ld\n",block_extentbnoderoot,ebn->key));
  
      if((errorcode=deletebnode(block_extentbnoderoot,ebn->key))!=0) {   /*** Maybe use deleteinternalnode here??? */
        break;
      }
    }
    else {
      unlockcachebuffer(cb);
      break;
    }
  }
  
  // XDEBUG((DEBUG_NODES,"deleteextents: Exiting with errorcode %ld\n",errorcode));
  
  return(errorcode);
}
  
  
  
LONG findextentbnode(ULONG key,struct CacheBuffer **returned_cb,struct fsExtentBNode **returned_bnode) {
  LONG errorcode;
  
  errorcode=findbnode(block_extentbnoderoot,key,returned_cb,(struct BNode **)returned_bnode);
  
  #ifdef CHECKCODE_BNODES
    if(*returned_bnode==0 || (*returned_bnode)->key!=key) {
      dreq("findextentbnode: findbnode() can't find key %ld!",key);
      outputcachebuffer(*returned_cb);
      return(INTERR_BTREE);
    }
  #endif
  
  return(errorcode);
}
  

/*
LONG findobjectnode(NODE nodeno,struct CacheBuffer **returned_cb,struct fsObjectNode **returned_node) {
  return(findnode(block_objectnoderoot, sizeof(struct fsObjectNode), nodeno, returned_cb, (struct fsNode **)returned_node));
}
*/

  
LONG createextentbnode(ULONG key,struct CacheBuffer **returned_cb,struct fsExtentBNode **returned_bnode) {
  return(createbnode(block_extentbnoderoot,key,returned_cb,(struct BNode **)returned_bnode));
}

  
  
LONG seekextent(struct ExtFileLock *lock, ULONG offset, struct CacheBuffer **returned_cb, struct fsExtentBNode **returned_ebn, ULONG *returned_extentoffset) {
  BLCK extentbnode;
  LONG errorcode=0;

  /* When seeking there are 2 options; we start from the current extent,
     or we start from the first extent.  Below we determine the best
     startpoint: */
  
  if(offset>lock->gh->size) {
    XDEBUG((DEBUG_SEEK,"seekextent: Attempting to seek beyond file\n"));
  
    return(ERROR_SEEK_ERROR);
  }

//  DEBUG(("seekextent: offset = %ld, lock->offset = %ld, lock->extentoffset = %ld, lock->curextent = %ld\n",offset, lock->offset, lock->extentoffset, lock->curextent));

  if(lock->curextent!=0 && offset >= lock->offset - lock->extentoffset) {
    extentbnode=lock->curextent;
    *returned_extentoffset=lock->offset - lock->extentoffset;
  }
  else {
    extentbnode=lock->gh->data;
    *returned_extentoffset=0;
  }

  /* Starting point has been determined.  Let the seeking begin!
     We keep getting the next extent, until we find the extent which
     contains the required offset. */

  if(extentbnode!=0) {
    while((errorcode=findextentbnode(extentbnode,returned_cb,returned_ebn))==0) {
      ULONG endbyte=*returned_extentoffset+((*returned_ebn)->blocks<<shifts_block);

      if(offset>=*returned_extentoffset && offset<endbyte) {
        /* Hooray!  We found the correct extent. */
        break;
      }

      if((*returned_ebn)->next==0) {
        /* This break is here in case we run into the end of the file.
           This prevents *returned_extentoffset and extentbnode
           from being destroyed in the lines below, since it is still
           valid to seek to the EOF */

        if(offset>endbyte) {
          /* There where no more blocks, but there should have been... */
          errorcode=ERROR_SEEK_ERROR;
        }
        break;
      }

      *returned_extentoffset+=(*returned_ebn)->blocks<<shifts_block;
      extentbnode=(*returned_ebn)->next;
    }
  }

  return(errorcode);
}



/* Notify support functions. */

UBYTE *fullpath(struct CacheBuffer *cbstart,struct fsObject *o) {
  struct fsObjectContainer *oc=cbstart->data;
  struct CacheBuffer *cb=cbstart;
  UBYTE *path=&pathstring[519];
  UBYTE *name;

  /* Returns the full path of an object, or 0 if it fails. */

  lockcachebuffer(cbstart);

  *path=0;

  while(oc->parent!=0) {     /* Checking parent here means name in ROOT will be ignored. */
    name=o->name;
    while(*++name!=0) {
    }

    while(name!=o->name) {
      *--path=upperchar(*--name);
    }

    if(readobject(oc->parent,&cb,&o)!=0) {
      path=0;
      break;
    }

    oc=cb->data;

    if(oc->parent!=0) {
      *--path='/';
    }
  }

  unlockcachebuffer(cbstart);

  return(path);
}



void checknotifyforpath(UBYTE *path,UBYTE notifyparent) {
  struct fsNotifyRequest *nr;
  UBYTE *s1,*s2;
  UBYTE *lastslash;

  /* /path/ doesn't have a trailing slash and start with a root directory (no colon) */

  s1=path;
  lastslash=path-1;
  while(*s1!=0) {
    if(*s1=='/') {
      lastslash=s1;
    }
    s1++;
  }

  nr=notifyrequests;

  // DEBUG(("checknotify: path = '%s'\n", path));

  while(nr!=0) {

    s1=path;
    s2=stripcolon(nr->nr_FullName);

    while(*s1!=0 && *s2!=0 && *s1==upperchar(*s2)) {
      s1++;  // If last character doesn't match, this increment won't take place.
      s2++;
    }

    /* "" == "hallo"  -> no match
       "" == "/"      -> match
       "" == ""       -> match
       "/shit" == ""  -> match (parent dir)
       "shit" == ""   -> match (parent dir) */

    // DEBUG(("checknotify: fullpath = '%s', nr->FullName = '%s'\n",s1,s2));

    if( (s1[0]==0 && (s2[0]==0 || (s2[0]=='/' && s2[1]==0))) || ((s2[0]==0 && (s1==lastslash || s1==lastslash+1)) && notifyparent==TRUE) ) {
      /* Wow, the string in the NotifyRequest matches!  We need to notify someone! */

      DEBUG(("checknotify: Notificating!! nr->FullName = %s, UserData = 0x%08lx\n",nr->nr_FullName, nr->nr_UserData));

      notify(nr);

      /* No else, if neither flag is set then do nothing. */
    }

    nr=nr->next;
  }
}



void checknotifyforobject(struct CacheBuffer *cb,struct fsObject *o,UBYTE notifyparent) {
  checknotifyforpath(fullpath(cb,o),notifyparent);
}



void notify(struct fsNotifyRequest *nr) {

  /* This function sends a Notify to the client indicated by the passed
     in notifyrequest structure. */

  if((nr->nr_Flags & NRF_SEND_SIGNAL)!=0) {
    /* Sending them a signal. */

    DEBUG(("notify: Sending signal\n"));

    Signal(nr->nr_stuff.nr_Signal.nr_Task,1<<nr->nr_stuff.nr_Signal.nr_SignalNum);
  }
  else if((nr->nr_Flags & NRF_SEND_MESSAGE)!=0) {
    struct NotifyMessage *nm;

    /* Sending them a message. */

    DEBUG(("notify: Sending message\n"));

    if((nm=AllocMem(sizeof(struct NotifyMessage),MEMF_CLEAR))!=0) {
      nm->nm_ExecMessage.mn_ReplyPort=msgportnotify;
      nm->nm_ExecMessage.mn_Length=sizeof(struct NotifyMessage);

      nm->nm_Class=NOTIFY_CLASS;
      nm->nm_Code=NOTIFY_CODE;
      nm->nm_NReq=(struct NotifyRequest *)nr;

      DEBUG(("notify: PutMsg() - UserData = 0x%08lx\n", nr->nr_UserData));

      PutMsg(nr->nr_stuff.nr_Msg.nr_Port,(struct Message *)nm);
    }
  }
}






/*
LONG writedata(ULONG newbytes, ULONG extentblocks, BLCK newspace, UBYTE *data) {
  ULONG blocks=newbytes>>shifts_block;
  LONG errorcode=0;

  if(blocks>extentblocks) {
    blocks=extentblocks;
  }

  if(blocks!=0 && (errorcode=write(newspace,data,blocks))!=0) {
    return(errorcode);
  }

  if(blocks<extentblocks) {
    struct CacheBuffer *cb;

    XDEBUG((DEBUG_IO,"  writedata: blocks = %ld, newbytes = %ld\n",blocks,newbytes));

    newbytes-=blocks<<shifts_block;
    data+=blocks<<shifts_block;

    if((cb=newcachebuffer(newspace+blocks))!=0) {
      unlockcachebuffer(cb);

      CopyMem(data,cb->data,newbytes);

      errorcode=writecachebuffer(cb);
      /* At this point we can do 2 things with this cachebuffer.  We can leave it
         hashed so if the user requests part of this block again it can quickly be grabbed
         from this CacheBuffer.  The other option is the call emptycachebuffer()
         and let this buffer be reused ASAP. */

      emptycachebuffer(cb);
    }
    else {
      errorcode=ERROR_NO_FREE_STORE;
    }
  }

  return(errorcode);
}
*/








LONG addblocks(UWORD blocks, BLCK newspace, NODE objectnode, BLCK *io_lastextentbnode) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode;

  /* This function adds /blocks/ blocks starting at block /newspace/ to a file
     identified by /objectnode/ and /lastextentbnode/.  /io_lastextentbnode/ can
     be zero if there is no ExtentBNode chain attached to this file yet.

     This function must be called from within a transaction.

     /blocks/ ranges from 1 to 8192.  To be able to extend Extents which are
     almost full, it is wise to make this value no higher than 8192 blocks.

     /io_lastextentbnode/ will contain the new lastextentbnode value when this
     function completes.

     This function makes no attempt to update any locks associated with this file,
     nor does it update the filesize information in a possible globalhandle.

     If there was no chain yet, then this function will create a new one.  However
     it will NOT update object.file.data -- this is left up to the caller. */

  if(*io_lastextentbnode!=0) {
    /* There was already a ExtentBNode chain for this file.  Extending it. */

    XDEBUG((DEBUG_IO,"  addblocks: Extending existing ExtentBNode chain.\n"));

    if((errorcode=findextentbnode(*io_lastextentbnode,&cb,&ebn))==0) {

      preparecachebuffer(cb);

      if(ebn->key+ebn->blocks==newspace && ebn->blocks+blocks<65536) {
        /* It is possible to extent the last ExtentBNode! */

        XDEBUG((DEBUG_IO,"  addblocks: Extending last ExtentBNode.\n"));

        ebn->blocks+=blocks;

        errorcode=storecachebuffer(cb);
      }
      else {
        /* It isn't possible to extent the last ExtentBNode so we create
           a new one and link it to the last ExtentBNode. */

        ebn->next=newspace;

        if((errorcode=storecachebuffer(cb))==0 && (errorcode=createextentbnode(newspace,&cb,&ebn))==0) {

          XDEBUG((DEBUG_IO,"  addblocks: Created new ExtentBNode.\n"));

          ebn->key=newspace;
          ebn->prev=*io_lastextentbnode;
          ebn->next=0;
          ebn->blocks=blocks;

          *io_lastextentbnode=newspace;

          if((errorcode=storecachebuffer(cb))==0) {
            block_rovingblockptr=newspace+blocks;
            if((blocks<<shifts_block) <= ROVING_SMALL_WRITE) {
              block_rovingblockptr+=ROVING_RESERVED_SPACE>>shifts_block;
            }

            if(block_rovingblockptr>=blocks_total) {
              block_rovingblockptr=0;
            }
          }
        }
      }
    }
  }
  else {
    /* There is no ExtentBNode chain yet for this file.  Attaching one! */

    if((errorcode=createextentbnode(newspace,&cb,&ebn))==0) {

      XDEBUG((DEBUG_IO,"  addblocks: Created new ExtentBNode chain.\n"));

      ebn->key=newspace;
      ebn->prev=objectnode+0x80000000;
      ebn->next=0;
      ebn->blocks=blocks;

      *io_lastextentbnode=newspace;

      if((errorcode=storecachebuffer(cb))==0) {
        block_rovingblockptr=newspace+blocks;
        if((blocks<<shifts_block) <= ROVING_SMALL_WRITE) {
          block_rovingblockptr+=ROVING_RESERVED_SPACE>>shifts_block;
        }

        if(block_rovingblockptr>=blocks_total) {
          block_rovingblockptr=0;
        }
      }
    }
  }

  return(errorcode);
}



void diskchangenotify(ULONG class) {
  struct IOStdReq *inputreq;
  struct MsgPort *inputport;
  struct InputEvent ie;
  struct timeval tv;

  if((inputport=CreateMsgPort())!=0) {
    if((inputreq=(struct IOStdReq *)CreateIORequest(inputport,sizeof(struct IOStdReq)))!=0) {
      if(OpenDevice("input.device",0,(struct IORequest *)inputreq,0)==0) {

        GetSysTime(&tv);

        ie.ie_NextEvent=0;
        ie.ie_Class=class;
        ie.ie_SubClass=0;
        ie.ie_Code=0;
        ie.ie_Qualifier=IEQUALIFIER_MULTIBROADCAST;
        ie.ie_EventAddress=0;
        ie.ie_TimeStamp=tv;

        inputreq->io_Command = IND_WRITEEVENT;
        inputreq->io_Length  = sizeof(struct InputEvent);
        inputreq->io_Data    = &ie;

        DoIO((struct IORequest *)inputreq);

        CloseDevice((struct IORequest *)inputreq);
      }
      DeleteIORequest(inputreq);
    }
    DeleteMsgPort(inputport);
  }
}



/*

 () {
  struct ExtFileLock *lock;

  lock=locklist;

  while(lock!=0) {
    if((lock->bits & EFL_MODIFIED)!=0) {
      /* File belonging to this lock was modified. */

      if(lock->gh!=0 && lock->curextent!=0) {
        BLCK lastblock=lock->curextent;

        lastblock+=lock->extentoffset>>shifts_block;

        /* lastblock is now possibly the last block which is used by the file.  This
           should always be true for newly created files which are being extended. */


      }
    }

    lock=lock->next;
  }
}

*/


BOOL freeupspace(void) {
  BOOL spacefreed=FALSE;

  /* This function tries to free up space.  It does this by
     permanently deleting deleted files (if case of a recycled)
     and by flushing the current transaction if there is one.

     If no space could be freed, then this function returns
     FALSE.  This is the go-ahead sign to report disk full
     to the user. */

  if(hastransaction()) {
    flushtransaction();
    spacefreed=TRUE;
  }

  if(cleanupdeletedfiles()!=FALSE) {
    spacefreed=TRUE;
  }

  return(spacefreed);
}



LONG writetofile(struct ExtFileLock *lock, UBYTE *buffer, ULONG bytestowrite) {
  struct GlobalHandle *gh=lock->gh;
  LONG errorcode;

  /* This function must be called from within a transaction! */

  if((gh->protection & FIBF_WRITE)!=0) {
    ULONG maxbytes;
    LONG newbytes;

    /* First thing we need to do is extend the file (if needed) to
       accomodate for all the data we are about to write. */

    maxbytes=(gh->size + bytes_block-1) & ~mask_block;  // Maximum number of bytes file can hold with the current amount of blocks.
    newbytes=bytestowrite-(maxbytes-lock->offset);      // Number of new bytes which would end up in newly allocated blocks.

    if((errorcode=seektocurrent(lock))==0 && (newbytes<=0 || (errorcode=extendblocksinfile(lock, (newbytes+bytes_block-1)>>shifts_block))==0)) {
      struct CacheBuffer *extent_cb;
      struct fsExtentBNode *ebn=0;
      ULONG newfilesize;

      /* At this point, the file either didn't need extending, or was succesfully extended. */

      newfilesize=lock->offset+bytestowrite;

      if(newfilesize<gh->size) {
        newfilesize=gh->size;
      }

      /* If the filesize will change, then we set it below */

      if(newfilesize!=gh->size) {
        struct CacheBuffer *cb;
        struct fsObject *o;

        if((errorcode=readobject(lock->objectnode,&cb,&o))==0) {
          preparecachebuffer(cb);

          CHECKSUM_WRITELONG(cb->data, &o->object.file.size, newfilesize);
          gh->size=newfilesize;

          if(o->object.file.data==0) {
            CHECKSUM_WRITELONG(cb->data, &o->object.file.data, lock->curextent);
            gh->data=lock->curextent;
          }

          errorcode=storecachebuffer_nochecksum(cb);
        }
      }

      if(errorcode==0) {
        while(bytestowrite!=0 && (errorcode=findextentbnode(lock->curextent, &extent_cb, &ebn))==0) {
          ULONG bytes;
          ULONG offsetinblock=lock->extentoffset & mask_block;
          BLCK ebn_next=ebn->next;
          UWORD ebn_blocks=ebn->blocks;

          if(ebn->blocks==lock->extentoffset>>shifts_block) {
            /* We are at the end +1 of this extent.  Skip to next one. */

            lock->curextent=ebn->next;
            lock->extentoffset=0;
            continue;
          }

          if(offsetinblock!=0 || bytestowrite<bytes_block) {

            /** Partial writes to the last block of the file will cause a
                read of that block, even if this block didn't yet contain
                any valid data, because the file was just extended.  This
                is unneeded... */

            /* File-ptr is located somewhere in the middle of a block, or at the
               start of the block but not at the end of the file.  To add data
               to it we'll first need to read this block. */

            XDEBUG((DEBUG_IO,"writetofile: Partially overwriting a single block of a file.  ebn->key = %ld, lock->extentoffset = %ld\n",ebn->key,lock->extentoffset));

            bytes=bytes_block-offsetinblock;

            if(bytes>bytestowrite) {
              bytes=bytestowrite;
            }

//            if(newbytes>0 && offsetinblock==0) {      /** offsetinblock check is NOT redundant. */
//              struct CacheBuffer *cb=getcachebuffer();
//
//              CopyMem(buffer, cb->data, bytes);
//              errorcode=writethrough(lock->curextent + (lock->extentoffset>>shifts_block), cb->data, 1);
//              emptycachebuffer(cb);
//
//              if(errorcode!=0) {
//                break;
//              }
//            }
//            else {
            if((errorcode=writebytes(ebn->key+(lock->extentoffset>>shifts_block), buffer, offsetinblock, bytes))!=0) {
              break;
            }
//            }
          }
          else {
            bytes=(((ULONG)ebn_blocks)<<shifts_block) - lock->extentoffset;

            if(bytes > bytestowrite) {
              bytes=bytestowrite & ~mask_block;

              /** This is a hack to speed up writes.

                  What it does is write more bytes than there are in the buffer
                  available (runaway writing), to avoid a seperate partial block
                  write.  It makes sure the extra data written doesn't extend
                  into a new 4K page (assuming MMU is using 4K pages). */

//              #define MMU_PAGESIZE (524288)
//
//              if(((ULONG)(buffer+bytes) & ~(MMU_PAGESIZE-1))==((ULONG)(buffer+bytes+bytes_block-1) & ~(MMU_PAGESIZE-1))) {
//                bytes=bytestowrite;
//              }
            }

            // XDEBUG((DEBUG_IO,"writetofile: Writing multiple blocks: blockstowrite = %ld, ebn->key = %ld, lock->extentoffset = %ld, lock->offset = %ld\n",blockstowrite, ebn->key, lock->extentoffset, lock->offset));

            if((errorcode=write(ebn->key+(lock->extentoffset>>shifts_block), buffer, (bytes+bytes_block-1)>>shifts_block))!=0) {
              break;
            }
          }

          seekforward(lock, ebn_blocks, ebn_next, bytes);

          bytestowrite-=bytes;
          buffer+=bytes;
        }
      }
    }
  }
  else {
    errorcode=ERROR_WRITE_PROTECTED;
  }

  return(errorcode);
}



LONG deletefileslowly(struct CacheBuffer *cbobject, struct fsObject *o) {
  ULONG size=o->object.file.size;
  ULONG key=o->object.file.data;
  LONG errorcode=0;

  /* cbobject & o refer to the file to be deleted (don't use this for objects
     other than files!).  The file is deleted a piece at the time.  This is
     because then even in low space situations files can be deleted.

     Note: This function deletes an object without first checking if
           this is allowed.  Use deleteobject() instead. */

  DEBUG(("deletefileslowly: Entry\n"));

  /* First we search for the last ExtentBNode */

  if(key!=0) {
    struct CacheBuffer *cb;
    struct fsExtentBNode *ebn;
    ULONG currentkey=0, prevkey=0;
    ULONG blocks=0;

    lockcachebuffer(cbobject);

    while((errorcode=findbnode(block_extentbnoderoot,key,&cb,(struct BNode **)&ebn))==0) {
      if(ebn->next==0) {
        currentkey=ebn->key;
        prevkey=ebn->prev;
        blocks=ebn->blocks;
        break;
      }
      key=ebn->next;
    }

    /* Key could be zero (in theory) or contains the last ExtentBNode for this file. */

    if(errorcode==0) {
      while((key & 0x80000000)==0) {
        key=prevkey;

        newtransaction();

        lockcachebuffer(cb);   /* Makes sure freespace() doesn't reuse this cachebuffer */

        if((errorcode=freespace(currentkey, blocks))==0) {
          unlockcachebuffer(cb);

          if((errorcode=deletebnode(block_extentbnoderoot, currentkey))==0) {
            if((key & 0x80000000)==0) {
              if((errorcode=findbnode(block_extentbnoderoot, key, &cb, (struct BNode **)&ebn))==0) {
                preparecachebuffer(cb);

                ebn->next=0;
                currentkey=ebn->key;
                prevkey=ebn->prev;
                blocks=ebn->blocks;

                errorcode=storecachebuffer(cb);
              }
            }
            else {
              preparecachebuffer(cbobject);

              CHECKSUM_WRITELONG(cbobject->data, &o->object.file.size, 0);
              CHECKSUM_WRITELONG(cbobject->data, &o->object.file.data, 0);

              // o->object.file.data=0;
              // o->object.file.size=0;

              if((errorcode=storecachebuffer_nochecksum(cbobject))==0) {
                errorcode=setrecycledinfodiff(0, -((size+bytes_block-1)>>shifts_block));
              }
            }
          }
        }
        else {
          unlockcachebuffer(cb);
        }

        if(errorcode==0) {
          /* Unlocked CacheBuffers could be killed after an endtransaction()! */
          endtransaction();
        }
        else {
          deletetransaction();
          break;
        }
      }
    }

    unlockcachebuffer(cbobject);
  }

  DEBUG(("deletefileslowly: Alternative way errorcode = %ld\n",errorcode));

  if(errorcode==0) {

    /* File-data was succesfully freed.  Now remove the empty object. */

    newtransaction();

    if((errorcode=removeobject(cbobject, o))==0) {
      endtransaction();
      return(0);
    }

    deletetransaction();
  }

  DEBUG(("deletefileslowly: Exiting with errorcode = %ld\n",errorcode));

  return(errorcode);
}


/*

Transactions
------------

ReadCacheBuffer
ReadOriginalCacheBuffer
LockCacheBuffer
UnLockCacheBuffer
NewTransaction
DeleteTransaction
EndTransaction
StoreCacheBuffer
PrepareCacheBuffer

The transaction system allows the filesystem to keep track
of changes made to blocks in the cache.  Every change is
stored in a transaction buffer.  The transaction buffer and
the original version of a block can be used to restore the
latest version of a block at any time.

At any time you can use readcachebuffer() to get the latest
version of a block.  This could be a partially modified
version if your in the middle of a transaction, the latest
version stored in the transaction buffer or the original
version from disk.

When you want to make changes to a block you call
preparecachebuffer().  This checks if the block you're
preparing to change is the original version, and if so makes
a backup copy.  This copy is not strictly needed, but it
speeds up the filesystem to keep the original version around
in case we need it later.  The preparecachebuffer() function
also calls lockcachebuffer() so the block you're changing
won't be reused in subsequent cache operations.

When you're satisfied with the changes you've made to a
specific block you should call storecachebuffer().  This
stores the changes into the transaction buffer.  From that
point on readcachebuffer() will be able to recreate the
block with your new modifications.

Before calling preparecachebuffer() for the first time, you
need to call newtransaction().  This is to let the
transaction system know you're about to start a new series
of modifications.

When you're done making all the changes to multiple blocks
you need to call endtransaction().  This makes all the
changes permanent.  If there was an error along the way you
can call deletetransaction() and all the changes you stored
with storecachebuffer() since you're last call to
newtransaction() will automatically be discarded.

Any cachebuffers you had locked at the time which had
changes in them will be restored to their original state (in
reality these buffers will simply be reread using
readcachebuffer()).

Examples:

x=readcachebuffer(0);    // x.data = 1

newtransaction();

preparecachebuffer(x);

x.data=2;

storecachebuffer(cb);

deletetransaction();     // x.data = 1;

read(x) -> "AA";  newtr(); prep(x); write("CC") -> "CC"; store(x); endtr() -> "CC";

read(x) -> "AA";  newtr(); prep(x); write("CC") -> "CC"; store(x); deltr() -> "AA";

read(x) -> "AA";  newtr(); prep(x); write("CC") -> "CC"; store(x); newtr(); prep(x); write("EE") -> "EE"; store(x); deltr() -> "CC"; deltr() -> "AA";

*/


#define OPTBUFSIZE (131072)

ULONG defrag_maxfilestoscan=512;


LONG setnextextent(BLCK next, BLCK key) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode=0;

  /* This function set the previous value of the
     passed Extent.  If the passed Extent is zero
     then this function does nothing. */

  if(next!=0 && (errorcode=findextentbnode(next, &cb, &ebn))==0) {
    preparecachebuffer(cb);

    ebn->prev=key;

    errorcode=storecachebuffer(cb);
  }

  return(errorcode);
}



LONG setprevextent(BLCK prev, BLCK key) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode;

  /* This function sets the next value of the
     passed Extent/Object to key. */

  if((prev & 0x80000000)==0 && (errorcode=findextentbnode(prev, &cb, &ebn))==0) {
    preparecachebuffer(cb);

    ebn->next=key;

    errorcode=storecachebuffer(cb);
  }
  else {
    struct fsObject *o;

    if((errorcode=readobject((prev & 0x7fffffff), &cb, &o))==0) {
      preparecachebuffer(cb);

      o->object.file.data=key;

      errorcode=storecachebuffer(cb);
    }
  }

  return(errorcode);
}



LONG mergeextent(BLCK key) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode;

  /* This function tries to merge the current extent with the
     next extent.  If the extent can't be merged, or was
     succesfully merged then this function returns 0.  Any
     error which occured during merging will be returned. */

  if((errorcode=findextentbnode(key, &cb, &ebn))==0) {

    /* We check if we found the right key, if there is a next Extent and
       if the two Extents touch each other: */

    if(ebn->key==key && ebn->next!=0 && ebn->key+ebn->blocks == ebn->next) {
      struct CacheBuffer *cb2;
      struct fsExtentBNode *ebn2;

      if((errorcode=findextentbnode(ebn->next, &cb2, &ebn2))==0 && ebn2->blocks+ebn->blocks < 65536) {
        BLCK next=ebn2->next;

        /* Merge next extent with our extent */

        preparecachebuffer(cb);

        ebn->blocks+=ebn2->blocks;
        ebn->next=next;

        if((errorcode=storecachebuffer(cb))==0) {   // call storecachebuffer() here, because deletebnode() may move our BNode.

          if((errorcode=deletebnode(block_extentbnoderoot, ebn2->key))==0) {    /*** Maybe use deleteinternalnode here??? */
            errorcode=setnextextent(next, key);
          }
        }
      }
    }
  }

  return(errorcode);
}



LONG insertextent(BLCK key, BLCK next, BLCK prev, ULONG blocks) {
  struct CacheBuffer *cb=0;
  struct fsExtentBNode *ebn=0;
  LONG errorcode;

  /* This function creates a new extent, but won't create one if it can
     achieve the same effect by extending the next or previous extent.
     In the unlikely case that the new extent is located EXACTLY between
     its predecessor and successor then an attempt will be made to merge
     these 3 extents into 1. */

  /*
  prev available & mergeable        -> merge.
  prev available, but not mergeable -> create new -> update next -> update previous.
  prev not available                -> create new -> update next -> update object
  */

  if((prev & 0x80000000)!=0 || (errorcode=findextentbnode(prev, &cb, &ebn))==0) {

    if((prev & 0x80000000)==0 && prev+ebn->blocks == key && ebn->blocks+blocks < 65536) {

      /* Extent we are inserting is mergeable with its previous extent. */

      preparecachebuffer(cb);

      ebn->blocks+=blocks;   /* This merges the previous and the new extent */

      if((errorcode=storecachebuffer(cb))==0) {
        errorcode=mergeextent(ebn->key);
      }
    }
    else {

      /* Extent we are inserting couldn't be merged with its previous extent. */

      if((errorcode=setprevextent(prev, key))==0 && (errorcode=createextentbnode(key, &cb, &ebn))==0) {

        /* Succesfully updated previous extent, or the object.  Also created new BNode */

        ebn->key=key;
        ebn->prev=prev;
        ebn->next=next;
        ebn->blocks=blocks;

        if((errorcode=storecachebuffer(cb))==0) {
          if((errorcode=setnextextent(next, key))==0) {
            mergeextent(key);
          }
        }
      }
    }
  }

  return(errorcode);
}



LONG truncateextent(BLCK key, LONG blocks) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode=0;

  /* Truncates the extended by the given amount of blocks.
     If the amount is negative then the truncation occurs
     at the start, otherwise at the end.

     This function returns INTERR_EXTENT if you tried to
     truncate to zero blocks (or beyond).  Any errorcode
     is returned.  If blocks is zero, then this function
     does nothing.

     Mar 20 1999: The truncation at the start could cause
                  BNode's to get lost (because they are
                  indexed by their start block).  Fixed. */

  if(blocks!=0) {
    if((errorcode=findextentbnode(key, &cb, &ebn))==0) {
      ULONG b;

      b=blocks<0 ? -blocks : blocks;

      if(b<ebn->blocks && ebn->key==key) {
        if(blocks<0) {
          ULONG next=ebn->next;
          ULONG prev=ebn->prev;
          UWORD blocks=ebn->blocks-b;

          /* Truncating at the start. */

          if((errorcode=deletebnode(block_extentbnoderoot, key))==0) {

            key+=b;

            if((errorcode=createbnode(block_extentbnoderoot, key, &cb, (struct BNode **)&ebn))==0) {
              ebn->key=key;
              ebn->next=next;
              ebn->prev=prev;
              ebn->blocks=blocks;

              if((errorcode=storecachebuffer(cb))==0) {
                /* Truncating at start means changing the key value.  This
                   means that the next and previous BNode's must also be
                   adjusted. */

                if((errorcode=setnextextent(next, key))==0) {
                  errorcode=setprevextent(prev, key);
                }
              }
            }
          }
        }
        else {

          /* Truncating at the end. */

          preparecachebuffer(cb);

          ebn->blocks-=b;

          errorcode=storecachebuffer(cb);
        }
      }
      else {
        errorcode=INTERR_EXTENT;
      }
    }
  }

  return(errorcode);
}



LONG copy(BLCK source, BLCK dest, ULONG totblocks, UBYTE *optimizebuffer) {
  ULONG blocks;
  LONG errorcode=0;

  /* Low-level function to copy blocks from one place to another. */

  while(totblocks!=0) {
    blocks=totblocks;
    if(blocks > (OPTBUFSIZE>>shifts_block)) {
      blocks=OPTBUFSIZE>>shifts_block;
    }

    if((errorcode=read(source, optimizebuffer, blocks))!=0) {
      break;
    }

    if((errorcode=write(dest, optimizebuffer, blocks))!=0) {
      break;
    }

    totblocks-=blocks;
    source+=blocks;
    dest+=blocks;
  }

  return(errorcode);
}



LONG deleteextent(struct CacheBuffer *cb, struct fsExtentBNode *ebn) {
  BLCK next,prev;
  LONG errorcode;

  /* Deletes an fsExtentBNode structure and properly relinks the rest of the chain.
     No space will be given free.

     newtransaction() should have been called prior to calling this function. */

  next=ebn->next;
  prev=ebn->prev;

  if((errorcode=deletebnode(block_extentbnoderoot,ebn->key))==0) {        /*** Maybe use deleteinternalnode here??? */
    if((errorcode=setnextextent(next, prev))==0) {
      errorcode=setprevextent(prev, next);
    }
  }

  return(errorcode);
}



WORD enough_for_add_moved(void) {
  if(defragmentlongs>5) {
    return TRUE;
  }
  return FALSE;
}



void add_moved(ULONG blocks, ULONG from, ULONG to) {
  if(defragmentsteps!=0) {
    *defragmentsteps++=MAKE_ID('M','O','V','E');
    *defragmentsteps++=3;
    *defragmentsteps++=blocks;
    *defragmentsteps++=from;
    *defragmentsteps++=to;
    *defragmentsteps=0;

    defragmentlongs-=5;
  }
}


void add_done(void) {
  if(defragmentsteps!=0) {
    *defragmentsteps++=MAKE_ID('D','O','N','E');
    *defragmentsteps++=0;
    *defragmentsteps=0;

    defragmentlongs-=2;
  }
}



LONG moveextent(struct fsExtentBNode *ebn, BLCK dest, UWORD blocks) {
  UBYTE *buf;
  LONG errorcode;

  /* This function (partially) moves the Extent to /dest/.
     /blocks/ is the number of blocks moved; if blocks is
     equal to the size of the Extent, then the Extent will
     be deleted.  Else the start of the Extent will be
     truncated. */

  if((buf=AllocVec(OPTBUFSIZE, bufmemtype))!=0) {
    BLCK key=ebn->key;
    BLCK next=ebn->next;
    BLCK prev=ebn->prev;
    UWORD blocksinextent=ebn->blocks;

    if((errorcode=copy(key, dest, blocks, buf))==0) {       // This functions knows that OPTBUFSIZE is the size of the buffer!

      /* Data has been physically moved -- nothing has been
         permanently altered yet. */

      if((errorcode=markspace(dest, blocks))==0) {

        if((errorcode=freespace(key, blocks))==0) {

          /* Bitmap has been altered to reflect the new location of the
             moved data.  Now we either need to truncate the Extent (if
             it was partially moved) or remove it. */

          if(blocksinextent==blocks) {
            struct CacheBuffer *cb;

            if((errorcode=findextentbnode(key, &cb, &ebn))==0) {
              errorcode=deleteextent(cb, ebn);
            }
          }
          else {
            next=key+blocks;
            errorcode=truncateextent(key, -blocks);
          }

          if(errorcode==0) {
            if((errorcode=insertextent(dest, next, prev, blocks))==0) {
              add_moved(blocks, key, dest);
            }
          }
        }
      }
    }

    FreeVec(buf);
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}



LONG fillgap(BLCK key) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode;

  /* This function will attempt to fill the gap behind an extent with
     data from the next extent.  This in effect merges the extent and
     the next extent (partially). */

  if((errorcode=findextentbnode(key, &cb, &ebn))==0) {
    ULONG next=ebn->next;

    key+=ebn->blocks;

    while(next!=0 && (errorcode=findextentbnode(next, &cb, &ebn))==0) {          //      !! failed !!
      UWORD blocks=ebn->blocks;
      LONG free;

      lockcachebuffer(cb);

      if((free=availablespace(key, 1024))>0) {

        unlockcachebuffer(cb);

        DEBUG(("fillgap: availablespace() returned %ld\n",free));

        /* The gap consists of /free/ blocks. */

        if(free > blocks && enough_for_add_moved()!=FALSE) {
          next=ebn->next;
        }
        else {
          next=0;
        }

        if((errorcode=moveextent(ebn, key, MIN(free, blocks)))!=0) {
          return(errorcode);
        }

        key+=blocks;
      }
      else {
        unlockcachebuffer(cb);
      }
    }
  }

  DEBUG(("fillgap: exiting with errorcode %ld\n",errorcode));

  return(errorcode);
}



LONG getbnode(BLCK block, struct CacheBuffer **returned_cb, struct fsExtentBNode **returned_ebn) {
  LONG errorcode;

  /* This function gets the ExtentBNode which starts at the given
     block or the first one after the given block.  Zero *ebn indicates
     there were no ExtentBNode's at or after the given block. */

  if((errorcode=findbnode(block_extentbnoderoot, block, returned_cb, (struct BNode **)returned_ebn))==0) {

    DEBUG(("getbnode: ebn->key = %ld, ebn->prev = %ld, ebn->blocks = %ld\n",(*returned_ebn)->key, (*returned_ebn)->prev, (*returned_ebn)->blocks));

    if(*returned_ebn!=0 && (*returned_ebn)->key<block) {
      errorcode=nextbnode(block_extentbnoderoot, returned_cb, (struct BNode **)returned_ebn);

      DEBUG(("getbnode: 2: ebn->key = %ld, ebn->prev = %ld, ebn->blocks = %ld\n",(*returned_ebn)->key, (*returned_ebn)->prev, (*returned_ebn)->blocks));
    }
  }

  return(errorcode);
}



/*
LONG makefreespace(BLCK block) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode;

  /* This function tries to move the data located at /block/ to
     another area of the disk. */

  if((errorcode=findextentbnode(block, &cb, &ebn))==0) {
    ULONG blocks;
    ULONG newblocks;
    BLCK startblock;

    lockcachebuffer(cb);

    blocks=MIN(OPTBUFSIZE>>shifts_block, ebn->blocks);

    if((errorcode=findspace2_backwards(blocks, block, blocks_total, &startblock, &newblocks))==0) {
      unlockcachebuffer(cb);

      if(newblocks!=0) {

        DEBUG(("makefreespace: Looking for %ld blocks from block %ld, and found %ld blocks at block %ld.\n", blocks, block, newblocks, startblock));

        errorcode=moveextent(ebn, startblock, newblocks);
      }
      else {
        errorcode=ERROR_DISK_FULL;
      }
    }
    else {
      unlockcachebuffer(cb);
    }
  }

  return(errorcode);
}
*/


LONG makefreespace(BLCK block) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode;
  WORD count=0;

  /* This function tries to move the data located at /block/ to
     another area of the disk. */

  if((errorcode=findextentbnode(block, &cb, &ebn))==0) {
    do {
      ULONG blocks;
      ULONG newblocks;
      BLCK startblock;

      lockcachebuffer(cb);

      blocks=MIN(OPTBUFSIZE>>shifts_block, ebn->blocks);

      if((errorcode=findspace2_backwards(blocks, ebn->key, blocks_total, &startblock, &newblocks))==0) {      // ebn->key should not be changed to block.
        unlockcachebuffer(cb);

        if(newblocks!=0) {

          DEBUG(("makefreespace: Looking for %ld blocks from block %ld, and found %ld blocks at block %ld.\n", blocks, block, newblocks, startblock));

          if((errorcode=moveextent(ebn, startblock, newblocks))!=0) {
            break;
          }
        }
        else {
          if(count==0) {
            errorcode=ERROR_DISK_FULL;
          }
          break;
        }
      }
      else {
        unlockcachebuffer(cb);
        break;
      }

      if(count++>=1) {
        break;
      }

      if((errorcode=getbnode(block, &cb, &ebn))!=0 || ebn==0) {
        break;
      }

    } while((ebn->prev & 0x80000000)==0);
  }

  return(errorcode);
}



LONG skipunmoveable(BLCK block) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;

  /* This function looks for moveable data or free space starting
     from the given block.  It returns -1 in case of failure, or
     the first moveable block it finds.  If there ain't no more
     moveable blocks, then blocks_total is returned. */

  if((findbnode(block_extentbnoderoot, block, &cb, (struct BNode **)&ebn))==0) {
    if(ebn!=0 && ebn->key==block) {
      return((LONG)block);
    }
    else if(ebn==0 || ebn->key>=block || nextbnode(block_extentbnoderoot, &cb, (struct BNode **)&ebn)==0) {
      if(ebn!=0) {
        BLCK key=ebn->key;
        LONG used;

        /* Found something moveable, but maybe there was some free space before the
           moveable Extent. */

        if((used=allocatedspace(block, key-block))!=-1) {
          if(block+used<key) {
            return((LONG)block+used);
          }
          else {
            return((LONG)key);
          }
        }
      }
      else {
        LONG errorcode;

        if((errorcode=findspace(1, block, blocks_total, &block))==0) {
          return((LONG)block);
        }
        else if(errorcode==ERROR_DISK_FULL) {
          return((LONG)blocks_total);
        }
      }
    }
  }

  return(-1);
}


struct fsExtentBNode *startofextentbnodechain(struct fsExtentBNode *ebn) {
  struct CacheBuffer *cb;
  LONG errorcode=0;

  while((ebn->prev & 0x80000000)==0 && (errorcode=findextentbnode(ebn->prev, &cb, &ebn))==0) {
  }

  if(errorcode==0) {
    return(ebn);
  }

  return(0);
}



/*
LONG findmatch(BLCK startblock, ULONG blocks, ULONG *bestkey) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
//  struct fsExtentBNode *ebn_start;
  ULONG bestblocks=0;
  LONG errorcode;

  /* This function looks for the start of a ExtentBNode chain
     which matches the given size.  If none is found, then the
     next smaller chain is returned.  If none is found, then a
     larger chain is returned.  If there are no chains at all
     0 is returned.

     The first ExtentBNode examined is determined by the start
     block number which is passed. */

  DEBUG(("findmatch: Looking for a chain of %ld blocks starting from block %ld\n", blocks, startblock));

  *bestkey=0;

  if((errorcode=getbnode(startblock, &cb, &ebn))==0 && ebn!=0) {

    DEBUG(("findmatch: 1, ebn->key = %ld, ebn->blocks = %ld\n", ebn->key, ebn->blocks));

    do {
      struct CacheBuffer *cb2;
      struct fsExtentBNode *ebn_start=ebn;
      struct fsObject *o;
      ULONG total=ebn->blocks;      /* If larger than bestblocks, then we can stop looking for the start of the ExtentBNode chain early. */
      ULONG key, newblocks;

      lockcachebuffer(cb);
//      ebn_start=startofextentbnodechain(ebn);
      {
        struct CacheBuffer *cb;

        while((total<bestblocks || bestblocks==0) && (ebn_start->prev & 0x80000000)==0 && ebn_start->prev >= ebn->key && (errorcode=findextentbnode(ebn_start->prev, &cb, &ebn_start))==0) {
          total+=ebn_start->blocks;
        }
      }
      unlockcachebuffer(cb);

      if(errorcode!=0) {
        break;
      }

      if((total<bestblocks || bestblocks==0) && (ebn_start->prev & 0x80000000)!=0) {

        DEBUG(("findmatch: 2, ebn->key = %ld\n", ebn->key));

        key=ebn_start->key;

        lockcachebuffer(cb);
        errorcode=readobject(ebn_start->prev & 0x7FFFFFFF, &cb2, &o);
        unlockcachebuffer(cb);

        if(errorcode!=0) {
          break;
        }

        DEBUG(("findmatch: 3, filesize = %ld\n",o->object.file.size));

        newblocks=(o->object.file.size+bytes_block-1)>>shifts_block;

        if(newblocks==blocks) {          /* Found ideal match */
          *bestkey=key;
          break;
        }
        else if(newblocks<blocks) {
          if(newblocks>bestblocks || bestblocks>blocks) {
            *bestkey=key;
            bestblocks=newblocks;
          }
        }
        else if(bestblocks==0 || newblocks<bestblocks) {
          *bestkey=key;
          bestblocks=newblocks;
        }
      }

    } while((errorcode=nextbnode(block_extentbnoderoot, &cb, (struct BNode **)&ebn))==0 && ebn!=0);
  }

  return(errorcode);
}
*/

#define FRAGMENTS (300)

ULONG fragment[FRAGMENTS];
//UBYTE fragmenttype[FRAGMENTS];

ULONG bestkey;
ULONG bestblocks;
ULONG searchedblocks;



void newfragmentinit_large(ULONG blocks) {
  bestkey=0;
  bestblocks=0;
  searchedblocks=blocks;
}



ULONG newfragment_large(ULONG block, ULONG blocks) {
  if(blocks==searchedblocks) {          /* Found ideal match */
    return(block);
  }
  else if(blocks<searchedblocks) {
    if(blocks>bestblocks || bestblocks>searchedblocks) {
      bestkey=block;
      bestblocks=blocks;
    }
  }
  else if(bestblocks==0 || blocks<bestblocks) {
    bestkey=block;
    bestblocks=blocks;
  }

  return(0);
}



/*
ULONG newfragment_large(ULONG block, ULONG blocks, UBYTE type) {
  if(type==0) {
    if(blocks==searchedblocks) {          /* Found ideal match */
      return(block);
    }
    else if(blocks<searchedblocks) {
      if(blocks>bestblocks || bestblocks>searchedblocks) {
        bestkey=block;
        bestblocks=blocks;
      }
    }
    else if(bestblocks==0 || blocks<bestblocks) {
      bestkey=block;
      bestblocks=blocks;
    }
  }

  return(0);
}
*/


ULONG newfragmentend_large(void) {
  return(bestkey);
}




void newfragmentinit_small(ULONG blocks) {
  ULONG *f=fragment;
  WORD n=FRAGMENTS;

  newfragmentinit_large(blocks);

  while(--n>=0) {
    *f++=0;
  }
}





/*
void newfragmentinit_small(ULONG blocks) {
  ULONG *f=fragment;
  UBYTE *fb=fragmenttype;
  WORD n=FRAGMENTS;

  newfragmentinit_large(blocks);

  while(--n>=0) {
    *f++=0;
    *fb++=0;
  }
}



ULONG newfragment_small(ULONG block, ULONG blocks, UBYTE type) {
  if(blocks<searchedblocks) {
    if(fragment[blocks]==0 || (fragmenttype[blocks]!=0 && type==0)) {
      ULONG blocks2=searchedblocks-blocks;

      fragment[blocks]=block;
      fragmenttype[blocks]=type;

      if(blocks2==blocks) {
        blocks2=0;
      }

      if(fragment[blocks2]!=0) {
        if(fragmenttype[blocks]==0 && fragmenttype[blocks2]==0) {
          return(fragment[blocks]);
        }
      }
    }
  }

  return(newfragment_large(block, blocks, type));
}
*/



ULONG newfragment_small(ULONG block, ULONG blocks) {
  if(blocks<searchedblocks && fragment[blocks]==0) {
    ULONG blocks2=searchedblocks-blocks;

    fragment[blocks]=block;

    if(blocks2==blocks) {
      blocks2=0;
    }

    if(fragment[blocks2]!=0) {
      return(fragment[blocks]);
    }
  }

  return(newfragment_large(block, blocks));
}



ULONG newfragmentend_small(void) {
  return(newfragmentend_large());
}




/*
LONG findmatch(BLCK startblock, ULONG blocks, ULONG *bestkey) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  ULONG lastextentend=0;
  LONG maxscan=defrag_maxfilestoscan;
  LONG errorcode;

  /* This function looks for the start of a ExtentBNode chain
     which matches the given size.  If none is found, then the
     next smaller chain is returned.  If none is found, then a
     larger chain is returned.  If there are no chains at all
     0 is returned.

     The first ExtentBNode examined is determined by the start
     block number which is passed. */

  DEBUG(("findmatch: Looking for a chain of %ld blocks starting from block %ld\n", blocks, startblock));

  *bestkey=0;

  if(blocks>FRAGMENTS-1) {
    newfragmentinit_large(blocks);
  }
  else {
    newfragmentinit_small(blocks);
  }

  if((errorcode=getbnode(startblock, &cb, &ebn))==0 && ebn!=0) {

    DEBUG(("findmatch: ebn->key = %ld, ebn->blocks = %ld\n", ebn->key, ebn->blocks));

    do {
      if((ebn->prev & 0x80000000)!=0) {
        struct CacheBuffer *cb2;
        struct fsObject *o;
        ULONG newblocks;
        UBYTE fragmenttype;

        /* Found the start of a candidate chain. */

        lockcachebuffer(cb);
        errorcode=readobject(ebn->prev & 0x7FFFFFFF, &cb2, &o);
        unlockcachebuffer(cb);

        if(errorcode!=0) {
          break;
        }

        newblocks=(o->object.file.size+bytes_block-1)>>shifts_block;

        fragmenttype=lastextentend==ebn->key ? 1 : 0;

        if(blocks>FRAGMENTS-1) {
          *bestkey=newfragment_large(ebn->key, newblocks, fragmenttype);
        }
        else {
          *bestkey=newfragment_small(ebn->key, newblocks, fragmenttype);
        }

        if(*bestkey!=0) {
          return(0);
        }

        if(--maxscan<0) {
          break;
        }

        lastextentend=ebn->next==0 ? ebn->key + ebn->blocks : 0;
      }
    } while((errorcode=nextbnode(block_extentbnoderoot, &cb, (struct BNode **)&ebn))==0 && ebn!=0);

    if(errorcode==0) {
      if(blocks>FRAGMENTS-1) {
        *bestkey=newfragmentend_large();
      }
      else {
        *bestkey=newfragmentend_small();
      }
    }
  }

  return(errorcode);
}
*/



LONG findmatch_fromend(BLCK startblock, ULONG blocks, ULONG *bestkey) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG maxscan=defrag_maxfilestoscan;
  LONG errorcode;

  /* This function looks for the start of a ExtentBNode chain
     which matches the given size.  If none is found, then the
     next smaller chain is returned.  If none is found, then a
     larger chain is returned.  If there are no chains at all
     0 is returned.

     The first ExtentBNode examined is determined by the start
     block number which is passed. */

  DEBUG(("findmatch_fromend: Looking for a chain of %ld blocks starting from block %ld\n", blocks, startblock));

  *bestkey=0;

  if(blocks>FRAGMENTS-1) {
    newfragmentinit_large(blocks);
  }
  else {
    newfragmentinit_small(blocks);
  }

  if((errorcode=lastbnode(block_extentbnoderoot, &cb, (struct BNode **)&ebn))==0 && ebn!=0 && ebn->key>=startblock) {

    DEBUG(("findmatch_fromend: ebn->key = %ld, ebn->blocks = %ld\n", ebn->key, ebn->blocks));

    do {
      if((ebn->prev & 0x80000000)!=0) {   // Is this a 'first fragment' of something?
        struct CacheBuffer *cb2;
        struct fsObject *o;
        ULONG newblocks;

        DEBUG(("findmatch_fromend!: ebn->key = %ld, ebn->blocks = %ld\n", ebn->key, ebn->blocks));

        /* Found the start of a candidate chain. */

        lockcachebuffer(cb);
        errorcode=readobject(ebn->prev & 0x7FFFFFFF, &cb2, &o);
        unlockcachebuffer(cb);

        if(errorcode!=0) {
          break;
        }

        newblocks=(o->object.file.size+bytes_block-1)>>shifts_block;

        if(blocks>FRAGMENTS-1) {
          *bestkey=newfragment_large(ebn->key, newblocks);
        }
        else {
          *bestkey=newfragment_small(ebn->key, newblocks);
        }

        if(*bestkey!=0) {
          return(0);
        }

        if(--maxscan<0) {
          break;
        }
      }
    } while((errorcode=previousbnode(block_extentbnoderoot, &cb, (struct BNode **)&ebn))==0 && ebn!=0 && ebn->key>=startblock);

    if(errorcode==0) {
      if(blocks>FRAGMENTS-1) {
        *bestkey=newfragmentend_large();
      }
      else {
        *bestkey=newfragmentend_small();
      }
    }
    else {
      DEBUG(("findmatch_fromend: errorcode=%ld\n", errorcode));
    }
  }

  return(errorcode);
}



LONG step(void) {
  LONG errorcode;
  LONG free;

  if((free=availablespace(block_defragptr, 256))!=-1) {

    DEBUG(("Defragmenter: Found %ld blocks of free space at block %ld.\n", free, block_defragptr));

    if(free==0) {
      struct CacheBuffer *cb;
      struct fsExtentBNode *ebn;

      /* Determine in which extent block_defragptr is located. */

      if((errorcode=findbnode(block_extentbnoderoot, block_defragptr, &cb, (struct BNode **)&ebn))==0) {
        if(ebn==0 || ebn->key!=block_defragptr) {
          LONG block;

          DEBUG(("Defragmenter: Found unmoveable data at block %ld.\n", block_defragptr));

          /* Skip unmoveable data */

          if((block=skipunmoveable(block_defragptr))!=-1) {
            block_defragptr=block;
          }
          else {
            errorcode=INTERR_DEFRAGMENTER;
          }
        }
        else if((ebn->prev & 0x80000000)!=0 || ebn->prev<block_defragptr) {

          DEBUG(("Defragmenter: Found a (partially) defragmented extent at block %ld.\n", block_defragptr));

          if(ebn->next==0 || ebn->next == ebn->key+ebn->blocks) {
            /* If there is no next Extent, or if the next Extent is touching
               this one, then skip the current one. */

            DEBUG(("Defragmenter: Extent has no next or next is touching this one.\n"));

            block_defragptr+=ebn->blocks;
          }
          else {
            LONG freeafter;
            BLCK key=ebn->key;
            BLCK next=ebn->next;
            UWORD blocks=ebn->blocks;

            DEBUG(("Defragmenter: Extent has a next extent.\n"));

            if((freeafter=availablespace(key+blocks, 256))!=-1) {

              DEBUG(("Defragmenter: There are %ld blocks of free space after the extent at block %ld.\n", freeafter, key));

              if(freeafter>0) {
                /* Move (part of) data located in next extent to this free space. */

                DEBUG(("Defragmenter: Filling the gap.\n"));

                /* The function below can be called multiple times in a row.  When there is a large
                   gap in which multiple extents of the file will fit, then these can all be transfered
                   into the gap at once. */

                errorcode=fillgap(key);
              }
              else {
                LONG block;

                /* Determine which extent it is which is located directly after the current extent. */

                if((block=skipunmoveable(key+blocks))!=-1) {
                  if(block==key+blocks) {

                    DEBUG(("Defragmenter: There was a moveable extent after the extent at block %ld.\n", key));

                    /* There was no unmoveable data, so let's move it. */

                    errorcode=makefreespace(block);
                  }
                  else {
                    LONG freeafter;

                    DEBUG(("Defragmenter: Skipped %ld blocks of unmoveable data.\n", block-(key+blocks)));

                    /* Unmoveable data was skipped. */

                    if(block!=next) {  // Mar 20 1999: Check to see if the extent after the unmoveable data is not already the correct one.

                      if((freeafter=availablespace(block, 256))!=-1) {

                        DEBUG(("Defragmenter: There are %ld blocks of free space after the unmoveable data.\n", freeafter));

                        if(freeafter==0) {

                          DEBUG(("Defragmenter: Clearing some space at block %ld.\n", block));

                          if((errorcode=makefreespace(block))==0) {
                            if((freeafter=availablespace(block, 256))==-1) {
                              errorcode=INTERR_DEFRAGMENTER;
                            }

                            DEBUG(("Defragmenter: There are now %ld blocks of cleared space after the unmoveable data.\n", freeafter));
                          }
                        }

                        if(errorcode==0) {
                          struct CacheBuffer *cb;
                          struct fsExtentBNode *ebn;

                          if((errorcode=findextentbnode(next, &cb, &ebn))==0) {

                            DEBUG(("Defragmenter: Moved next extent of our extent directly after the unmoveable space (block = %ld).\n", block));

                            if((errorcode=moveextent(ebn, block, MIN(freeafter, ebn->blocks)))==0) {
                              block_defragptr=block;
                            }
                          }
                        }
                      }
                      else {
                        errorcode=INTERR_DEFRAGMENTER;
                      }
                    }
                    else {

                      /* Extent after unmoveable data is already correct, so no need to move it. */

                      block_defragptr=block;
                    }
                  }
                }
                else {
                  errorcode=INTERR_DEFRAGMENTER;
                }
              }
            }
            else {
              errorcode=INTERR_DEFRAGMENTER;
            }
          }
        }
        else {

          DEBUG(("Defragmenter: Found an extent at block %ld which must be moved away.\n", block_defragptr));

          errorcode=makefreespace(block_defragptr);
        }
      }
    }
    else {
      ULONG bestkey;

      if((errorcode=findmatch_fromend(block_defragptr, free, &bestkey))==0) {
        if(bestkey!=0) {
          struct CacheBuffer *cb;
          struct fsExtentBNode *ebn;

          if((errorcode=findextentbnode(bestkey, &cb, &ebn))==0) {

            DEBUG(("Defragmenter: Moving a new first Extent to %ld\n", block_defragptr));

            errorcode=moveextent(ebn, block_defragptr, MIN(free, ebn->blocks));
          }
        }
        else {
          DEBUG(("Defragmenter: Nothing more to optimize.\n"));

          add_done();
        }
      }
    }
  }
  else {
    errorcode=INTERR_DEFRAGMENTER;
  }

  DEBUG(("Defragmenter: Exiting with errorcode %ld\n\n",errorcode));

  return(errorcode);
}



/*

  if (block at block_optimizeptr is full) {

    determine in which extent block_optimizeptr is located.

    if (not located in extent) {
      skip unmoveable data: add 1 to block_optimizeptr.
    }
    else if (extent is first extent OR previous extent is located before block_optimizeptr) {
      if (extent is last extent (as well)) {
        file was defragmented: set block_optimizeptr to point just after this extent.
      }
      else {
        determine if there is free space located after this extent.

        if (there is free space) {
          move (part of) data located in next extent to this free space.
        }
        else {

          determine which extent it is which is located directly after the current extent.

          if (no such extent exists) {
            skip unmoveable data

            if (no free space after unmoveable data) {
              make some free space by moving data.
            }
            move (part of) data located in next extent to the (newly made) free space: set block_optimizeptr in this part.
          }
          else {
            make some free space first by moving data.
          }
        }
      }
    }
    else {
      make some free space by moving data at block_optimizeptr.
    }
  }
  else {

    determine amount of free space.

    look for an extent-chain equal to the amount of free space beyond this location.

    if (found suitable extent-chain) {
      move data to block_optimizeptr; set block_optimizeptr to point just after this data.
    }
    else {
      scan for ANY 'first' extent beyond this location.

      if (none found) {
        optimization is done.
      }
      else {
        move found 'first' extent to block_optimizeptr.
      }
    }
  }


*/


/* The simple purpose of the SFS DosList handler task is to
   provide the following non-blocking functions:

   - Adding a VolumeNode to the DosList
     data = VolumeNode

   - Removing a VolumeNode from the DosList (synchronously)
     data = VolumeNode

   All messages send to the DosList handler are freed by
   the DosList handler itself.  This is to avoid having to
   wait for the reply and then free the message yourself.
*/


void sdlhtask(void) {
  struct ExecBase *SysBase=(*((struct ExecBase **)4));
  struct DosLibrary *DOSBase;

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    Forbid();
    if(FindPort("SFS DosList handler")==0) {
      struct MsgPort *port;

      if((port=CreateMsgPort())!=0) {
        struct SFSMessage *sfsm;

        port->mp_Node.ln_Name="SFS DosList handler";
        port->mp_Node.ln_Pri=1;
        AddPort(port);
        Permit();

        for(;;) {
          struct DosList *dol;

          WaitPort(port);

          dol=LockDosList(LDF_WRITE|LDF_VOLUMES);

          while((sfsm=(struct SFSMessage *)GetMsg(port))!=0) {
            if(sfsm->command==SFSM_ADD_VOLUMENODE) {

              /* AddDosEntry rejects volumes based on their name and date. */

              if(AddDosEntry((struct DosList *)sfsm->data)==DOSFALSE) {
                sfsm->errorcode=IoErr();
              }

//            if(AddDosEntry((struct DosList *)sfsm->data)==DOSFALSE) {
//              errorcode=IoErr();
//              FreeDosEntry((struct DosList *)vn);
//              vn=0;
//            }
            }
            else if(sfsm->command==SFSM_REMOVE_VOLUMENODE) {
              struct DosList *vn=(struct DosList *)sfsm->data;

              while((dol=NextDosEntry(dol, LDF_VOLUMES))!=0) {
                if(dol==vn) {
                  RemDosEntry(dol);
                  break;
                }
              }
              //  removevolumenode(dol, (struct DosList *)sfsm->data);      /* Dangerous because of DOSBase?? */
              FreeDosEntry(vn);
            }

            FreeVec(sfsm);
          }

          UnLockDosList(LDF_WRITE|LDF_VOLUMES);
        }
      }
      else {
        Permit();
      }
    }
    else {
      Permit();
    }

    CloseLibrary((struct Library *)DOSBase);
  }
}
