#include <exec/types.h>
#include <libraries/iffparse.h>
#include "blockstructure.h"
#include "redblacktree.h"

/* Operation   : A single block modification
   Transaction : A series of block modifications which together result in a valid disk */

struct OperationInformation {
  UWORD length;     /* Length in bytes of the compressed data. */
  BLCK blckno;
  UBYTE bits;       /* Bit 0: When set indicates that this block has no original. */
  UBYTE data[0];
};

#define OI_EMPTY (1)



struct Operation {
  struct Operation *left;
  struct Operation *right;
  struct Operation *parent;
  NodeColor color;
  UBYTE new;

//  struct Operation *next;
//  struct Operation *previous;

  struct OperationInformation oi;
};


#define TRANSACTIONSTORAGE_ID   MAKE_ID('T','R','S','T')
#define TRANSACTIONFAILURE_ID   MAKE_ID('T','R','F','A')
#define TRANSACTIONOK_ID        MAKE_ID('T','R','O','K')

/* The blocks used for storing the Transaction buffer are linked in a
   singly linked list.  The data they hold is a direct copy of all
   Transaction data. */

struct fsTransactionStorage {
  struct fsBlockHeader bheader;

  BLCK next;

  UBYTE data[0];
};


struct fsTransactionFailure {
  struct fsBlockHeader bheader;

  BLCK firsttransaction;
};
