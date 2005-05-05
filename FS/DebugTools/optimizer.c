#include <exec/types.h>
#include <proto/exec.h>
#include <stdio.h>

typedef ULONG BLCK;

LONG insertextent(BLCK key,BLCK next,BLCK prev,ULONG blocks);

ULONG block_extentbnoderoot=2;


void main(void) {
  printf("Errorcode from insertextent = %ld\n",insertextent(7,11,2,3));
}



/*

          1    1    2    2    3
0....5....0....5....0....5....0
  |--|

*/


struct CacheBuffer {
  void *data;
};

struct fsExtentBNode {
  ULONG key;
  ULONG next;
  ULONG prev;
  UWORD blocks;
};


void preparecachebuffer(struct CacheBuffer *cb) {
}

LONG storecachebuffer(struct CacheBuffer *cb) {
  return(0);
}


LONG deletebnode(ULONG root,ULONG key) {

  printf("deleteextentbnode: Deleting key %ld\n",key);

  return(0);
}

LONG createextentbnode(ULONG key,struct CacheBuffer **cb,struct fsExtentBNode **ebn) {

  printf("createextentbnode: Creating key %ld\n",key);

  *cb=AllocMem(sizeof(struct CacheBuffer),0);
  *ebn=AllocMem(sizeof(struct fsExtentBNode),0);

  (*ebn)->key=key;
  (*ebn)->next=0;
  (*ebn)->prev=0;
  (*ebn)->blocks=0;

  return(0);
}


LONG findextentbnode(ULONG key,struct CacheBuffer **cb,struct fsExtentBNode **ebn) {

  printf("findextentbnode: Looking for key %ld\n",key);

  *cb=AllocMem(sizeof(struct CacheBuffer),0);
  *ebn=AllocMem(sizeof(struct fsExtentBNode),0);

  if(key==2) {
    (*ebn)->key=2;
    (*ebn)->next=11;
    (*ebn)->prev=0;
    (*ebn)->blocks=4;
  }
  else if(key==11) {
    (*ebn)->key=11;
    (*ebn)->next=0;
    (*ebn)->prev=2;
    (*ebn)->blocks=10;
  }
  else {
    printf("key %ld does not exists!\n",key);

    return(-1);
  }

  return(0);
}




LONG insertextent(BLCK key,BLCK next,BLCK prev,ULONG blocks) {
  struct CacheBuffer *cb;
  struct fsExtentBNode *ebn;
  LONG errorcode;

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

    printf("insertextent: Previous extent and new extent can be merged.\n");

    preparecachebuffer(cb);

    ebn->blocks+=blocks;

    printf("insertextent: Previous extent and new extent merged.\n");

    if(next!=0 && key+blocks == next) {
      struct CacheBuffer *cb2;
      struct fsExtentBNode *ebn2;

      printf("insertextent: Newly merged extent is touching next extent.\n");

      /* The next extent is touching the just merged extent! */

      if((errorcode=findextentbnode(next,&cb2,&ebn2))!=0) {
        return(errorcode);
      }

      if(ebn2->blocks+ebn->blocks < 65536) {
        /* Merge next extent with current extent */

        printf("insertextent: Newly merged extent is mergeable with next extent.\n");

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

    printf("insertextent: Previous extent and new extent couldn't be merged.\n");

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

    if(prev!=0) {
      ebn->next=key;

      if((errorcode=storecachebuffer(cb))!=0) {
        return(errorcode);
      }
    }

    if(next!=0) {
      if((errorcode=findextentbnode(next,&cb,&ebn))!=0) {
        return(errorcode);
      }

      /* We need to fix the previous pointer of the next extent.  If the next
         extent is touching our new extent then try to merge it. */

      if(key+blocks == next && ebn->blocks+blocks < 65536) {
        /* Merge next extent with current extent */

        printf("insertextent: Next extent can be merged with new extent.\n");

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
    else {
      printf("insertextent: No next extent.\n");
    }

    if((errorcode=storecachebuffer(cb2))!=0) {
      return(errorcode);
    }
  }

  return(0);
}
