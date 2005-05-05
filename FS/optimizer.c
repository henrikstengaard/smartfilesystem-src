
#include "fs.h"

#include "bitmap_protos.h"
#include "deviceio_protos.h"

ULONG block_optimizeptr;
UBYTE *optimizebuffer;

#define OPTBUFSIZE (65536)

LONG initoptimizer() {
  LONG errorcode=0;

  block_optimizeptr=block_reserved_start;

  if((optimizebuffer=AllocMem(OPTBUFSIZE,bufmemtype))==0) {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}

void cleanupoptimizer() {
  if(optimizebuffer!=0) {
    FreeMem(optimizebuffer,OPTBUFSIZE);
  }
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


LONG optimize() {
  LONG space;
  LONG errorcode;

  if((space=availablespace(block_optimizer,8000))>=0) {
    if(space==0) {
      /* No free space at this location.
    }
  }
  else {
    errorcode=ERROR_NO_FREE_STORE;
  }

  return(errorcode);
}



LONG move(BLCK dest,ULONG blocks,struct CacheBuffer *cb,struct fsExtentBNode *ebn) {
  ULONG oldstart,next,prev,oldblocks;
  LONG errorcode;

  /* Moves (part of) the passed in Extent to /dest/.  The number of blocks to
     move is specified by /blocks/.  This function will automatically merge
     adjacent Extents.  This function also takes care of marking blocks free
     and full. */

  if((errorcode=copy(ebn->key,dest,blocks))==0) {
    struct CacheBuffer *prevcb;
    struct fsExtentBNode *prevebn;

    /* Data physically copied. */

    next=ebn->next;
    prev=ebn->prev;
    oldstart=ebn->key;
    oldblocks=ebn->blocks;

    if(ebn->blocks==blocks) {
      if((errorcode=deleteextent(cb,ebn))!=0) {
        return(errorcode);
      }

      if(next!=0) {
        if((errorcode=findextentbnode(next,&cb,&ebn))!=0) {
          return(errorcode);
        }

        preparecachebuffer(cb);
      }
    }
    else {
      if((errorcode=deleteextent(cb,ebn))!=0) {
        return(errorcode);
      }

      if((errorcode=createextentbnode(oldstart+blocks,&cb,&ebn))!=0) {
        return(errorcode);
      }

      ebn->key=oldstart+blocks;
      ebn->next=next;
      ebn->blocks=oldblocks-blocks;

      setchecksum(cb);
    }

    /* cb,ebn = either a new truncated extent, the next extent or if next==0 nothing at all. */

    if(prev!=0 && (errorcode=findbnode(prev,&cbprev,&ebnprev))!=0) {
      return(errorcode);
    }

    if(prev!=0 && ebnprev->key+ebnprev->blocks == dest && ebnprev->blocks+blocks < 65536) {
      /* We are able to extend the previous extent! */

      preparecachebuffer(cbprev);

      ebnprev->blocks+=blocks;
      if(next!=0) {
        ebnprev->next=ebn->key;
        ebn->prev=ebnprev->key;

        if((errorcode=storecachebuffer(cb))!=0) {
          return(errorcode);
        }
      }
      else {
        ebnprev->next=0;
      }

      if((errorcode=storecachebuffer(cbprev))!=0) {
        return(errorcode);
      }
    }
    else {
      struct CacheBuffer *cb2;
      struct fsExtentBNode *ebn2;

      if(prev!=0) {
        preparecachebuffer(cbprev);
      }

      if((errorcode=createextentbnode(dest,&cb2,&ebn2))!=0) {
        return(errorcode);
      }

      ebn2->key=dest;
      ebn2->prev=prev;
      ebn2->blocks=blocks;
      if(next!=0) {
        if(dest+blocks==ebn->key && blocks+ebn->blocks < 65536) {
          /* Merge with next extent */

          ebn2->blocks+=ebn->blocks;
          ebn2->next=ebn->next;

          if((errorcode=storecachebuffer(cb))!=0) {
            return(errorcode);
          }

          if((errorcode=deleteextent(cb,ebn))!=0) {
            return(errorcode);
          }



        }
        else {
          ebn2->next=ebn->key;
          ebn->prev=dest;

          setchecksum(cb);
          if((errorcode=storecachebuffer(cb))!=0) {
            return(errorcode);
          }
        }
      }
      else {
        ebn2->next=0;
      }

      if((errorcode=storecachebuffer(cb2))!=0) {
        return(errorcode);
      }

      if(prev!=0) {
        ebnprev->next=dest;

        if((errorcode=storecachebuffer(cbprev))!=0) {
          return(errorcode);
        }
      }
    }

    if(



    if (extent is moved directly after previous extent) {
      extend previous extent; fix next pointer.

      if (new combined extent touches next extent) {
        delete next extent.
        extend new combined extent.
      }
    }
    else {
      create new extent; fix previous extent next pointer; fix next extent previous pointer.

      if (new extent touches next extent) {
        delete next extent.
        extend new extent.
      }
    }


  }

  return(errorcode);
}

/*

struct fsExtentBNode {
  ULONG key;     /* data! */
  ULONG next;
  ULONG prev;
  UWORD blocks;  /* The size in blocks of the region this Extent controls */
};

*/



LONG copy(BLCK source,BLCK dest,ULONG totblocks) {
  ULONG blocks;
  LONG errorcode=0;

  // CERTIFIED //
  /* Low-level function to copy blocks from one place to another. */

  while(totblocks!=0) {
    blocks=totblocks;
    if(blocks > (OPTBUFSIZE>>shifts_block)) {
      blocks=OPTBUFSIZE>>shifts_block;
    }

    if((errorcode=read(source,optimizebuffer,blocks))!=0) {
      break;
    }

    if((errorcode=write(dest,optimizebuffer,blocks))!=0) {
      break;
    }

    totblocks-=blocks;
    source+=blocks;
    dest+=blocks;
  }

  return(errorcode);
}







LONG deleteextent(struct CacheBuffer *cb,struct fsExtentBNode *ebn) {
  BLCK next,prev;
  LONG errorcode=0;

  /* Deletes an fsExtentBNode structure and properly relinks the rest of the chain.
     No space will be given free.

     newtransaction() should have been called prior to calling this function. */

  next=ebn->next;
  prev=ebn->prev;

  if((errorcode=deletebnode(block_extentbnoderoot,ebn->key))==0) {        /*** Maybe use deleteinternalnode here??? */
    if(next!=0) {
      if((errorcode=findbnode(block_extentbnoderoot,next,&cb,(struct BNode **)&ebn))!=0) {
        return(errorcode);
      }

      preparecachebuffer(cb);

      ebn->prev=prev;

      if((errorcode=storecachebuffer(cb))!=0) {
        return(errorcode);
      }
    }

    if(prev!=0) {
      if((errorcode=findbnode(block_extentbnoderoot,prev,&cb,(struct BNode **)&ebn))!=0) {
        return(errorcode);
      }

      preparecachebuffer(cb);

      ebn->next=next;

      if((errorcode=storecachebuffer(cb))!=0) {
        return(errorcode);
      }
    }
  }

  return(errorcode);
}




LONG insertextent(BLCK key,BLCK next,BLCK prev,ULONG blocks) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode;

  // CERTIFIED //
  /* This function creates a new extent, but won't create one if it can
     achieve the same effect by extending the next or previous extent.
     In the unlikely case that the new extent is located EXACTLY between
     its predecessor and successor then an attempt will be made to merge
     these 3 extents into 1. */

  if(prev!=0 && (errorcode=findextentbnode(prev,&cb,&ebn))!=0) {
    return(errorcode);
  }

  if(prev!=0 && prev+ebn->blocks == key && ebn->blocks+blocks < 65536) {

    /* Extent we are inserting is mergeable with its previous extent. */

    preparecachebuffer(cb);

    ebn->blocks+=blocks;   /* This merges the previous and the new extent */

    if(next!=0 && key+blocks == next) {
      struct CacheBuffer *cb2;
      struct fsExtentBNode *ebn2;

      /* The next extent is touching the just merged extent! */

      if((errorcode=findextentbnode(next,&cb2,&ebn2))!=0) {
        return(errorcode);
      }

      if(ebn2->blocks+ebn->blocks < 65536) {
        /* Merge next extent with current extent */

        ebn->blocks+=ebn2->blocks;
        ebn->next=ebn2->next;

        if((errorcode=deletebnode(block_extentbnoderoot,ebn2->key))!=0) {        /*** Maybe use deleteinternalnode here??? */
          return(errorcode);
        }

        if(ebn->next!=0) {
          if((errorcode=findextentbnode(ebn->next,&cb2,&ebn2))!=0) {
            return(errorcode);
          }

          preparecachebuffer(cb2);

          ebn2->prev=ebn->key;

          if((errorcode=storecachebuffer(cb2))!=0) {
            return(errorcode);
          }
        }
      }
    }

    if((errorcode=storecachebuffer(cb))!=0) {
      return(errorcode);
    }
  }
  else {
    struct CacheBuffer *cb2;
    struct fsExtentBNode *ebn2;

    /* Extent we are inserting couldn't be merged with its previous extent. */

    if(prev!=0) {
      preparecachebuffer(cb);  /* Makes sure that createextentbnode doesn't destroy this cachebuffer */
    }

    if((errorcode=createextentbnode(key,&cb2,&ebn2))!=0) {
      return(errorcode);
    }

    ebn2->key=key;
    ebn2->prev=prev;
    ebn2->next=next;
    ebn2->blocks=blocks;

    /* We created a new extent.  Don't forget to fix the previous extent's (if any) next pointer. */

    if(prev!=0) {
      ebn->next=key;

      if((errorcode=storecachebuffer(cb))!=0) {
        return(errorcode);
      }
    }

    if(next!=0) {

      /* If there is a next, we need to check if we can merge it.  Even if we can't
         we still need to set its previous pointer correctly. */

      if((errorcode=findextentbnode(next,&cb,&ebn))!=0) {
        return(errorcode);
      }

      /* We need to fix the previous pointer of the next extent.  If the next
         extent is touching our new extent then try to merge it. */

      if(key+blocks == next && ebn->blocks+blocks < 65536) {
        /* Merge next extent with current extent */

        next=ebn->next;

        ebn2->blocks+=ebn->blocks;
        ebn2->next=next;

        if((errorcode=deletebnode(block_extentbnoderoot,ebn->key))!=0) {        /*** Maybe use deleteinternalnode here??? */
          return(errorcode);
        }

        if(next!=0 && (errorcode=findextentbnode(next,&cb,&ebn))!=0) {
          return(errorcode);
        }
      }

      /* Fix previous pointer of next extent (even after merging) */

      if(next!=0) {
        preparecachebuffer(cb);

        ebn->prev=key;

        if((errorcode=storecachebuffer(cb))!=0) {
          return(errorcode);
        }
      }
    }

    if((errorcode=storecachebuffer(cb2))!=0) {
      return(errorcode);
    }
  }

  return(0);
}





/*
    if (extent was moved completely) {

      delete current extent.

      if (extent is moved directly after previous extent) {
        extend previous extent; fix next pointer.

        if (new combined extent touches next extent) {
          delete next extent.
          extend new combined extent.
        }
      }
      else {
        create new extent; fix previous extent next pointer; fix next extent previous pointer.

        if (new extent touches next extent) {
          delete next extent.
          extend new extent.
        }
      }
    }
    else {
      /* in this case it is impossible for the moved data to touch the next extent. */

      truncate current extent.

      if (part of extent is moved directly after previous extent) {
        extend previous extent; fix next pointer.
      }
      else {
        create new extent; fix previous extent next pointer; fix next extent previous pointer.
      }
    }
*/
