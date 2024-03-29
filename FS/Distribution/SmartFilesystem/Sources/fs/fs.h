#ifndef FS_H
#define FS_H

#include <exec/types.h>
#include <libraries/iffparse.h>

// #define CHECKCODE
#define CHECKCODE_BNODES

// #define CHECKCHECKSUMSALWAYS

#define DEBUGCODE
// #define DEBUGKPRINTF  /* If defined and DEBUGCODE is defined then uses kprintf as output */
// #define DEBUG115200

// #define WORDCOMPRESSION
#define LONGCOMPRESSION
// #define NOCOMPRESSION
// #define ALTERNATIVECOMPRESSION
// #define BLOCKCOMPRESSION

#define PROGRAMNAME     "Smart Filesystem"
#define PROGRAMNAMEVER  "SmartFilesystem"

#define MAX_NAME_LENGTH   (100)

#define INTERR_CHECKSUM_FAILURE       80
#define INTERR_BLOCK_WRONG_TYPE       81
#define INTERR_OWNBLOCK_WRONG         82
#define INTERR_BTREE                  83
#define INTERR_SAFE_DELETE            84
#define INTERR_NODES                  85
#define INTERR_EXTENT                 86
#define INTERR_DEFRAGMENTER           87

/* macros */

#define TOBADDR(x)      (((ULONG)(x)) >> 2)

#define remove(n)    (n)->ln_Succ->ln_Pred=(n)->ln_Pred; (n)->ln_Pred->ln_Succ=(n)->ln_Succ
#define addtail(l,n) (n)->ln_Succ=(l)->lh_TailPred->ln_Succ; (l)->lh_TailPred->ln_Succ=(n); (n)->ln_Pred=(l)->lh_TailPred; (l)->lh_TailPred=(n)

#define removem(n)    (n)->mln_Succ->mln_Pred=(n)->mln_Pred; (n)->mln_Pred->mln_Succ=(n)->mln_Succ
#define addtailm(l,n) (n)->mln_Succ=(l)->mlh_TailPred->mln_Succ; (l)->mlh_TailPred->mln_Succ=(n); (n)->mln_Pred=(l)->mlh_TailPred; (l)->mlh_TailPred=(n)
#define addheadm(l,n) (n)->mln_Succ=(l)->mlh_Head; (n)->mln_Pred=(struct MinNode *)(l); (l)->mlh_Head->mln_Pred=(n); (l)->mlh_Head=(n);

#define DOSTYPE_ID      MAKE_ID('S','F','S',0)

/* HASHENTRY(x) is used to determine which hashchain to use for
   a specific hash value. */

#define HASHCHAIN(x) (UWORD)(x % (UWORD)((bytes_block-sizeof(struct fsHashTable))>>2))

#endif

