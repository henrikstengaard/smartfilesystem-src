#include <exec/types.h>
#include <stdio.h>

/*

The idea behind a multiway tree is that multiway trees do
not require a very good balance to be effective.

x-Way trees

We store new nodes in a leaf-container which can contain
upto x nodes.  When the leaf-container is full it is split
into two seperate leaf-containers containing x/2 and x/2+1
nodes.  A node-container is created which keeps track of the
two leaf-containers.  The new leaf-containers will be split
when they are full as well, and so on.

At some point the node-container will be full.  When this
happens we split it in much the same way we split the
leaf-containers.  In effect this will mean adding a new
level to the tree.

When deleting a leaf from a leaf-container we will check if
the container contains less than x/2 leafs.  If this is the
case we attempt to merge the leaf-container with the next or
previous leaf-container (or both).  For this to work the
total number of free leafs in the next and previous
container must be equal to the number of leafs in the
current leaf-container.

*/

#define BRANCHES (4)

struct TreeContainer {
  struct TreeContainer *parent;
  struct TreeContainer *next;
  struct TreeContainer *prev;
  UBYTE isleaf;
  UBYTE nodecount;
  UWORD pad2;

  struct TreeNode treenode[BRANCHES];
};

struct TreeNode {
  ULONG key;
  ULONG data;
};


struct TreeContainer *root;

void main() {
  struct TreeContainer first;

  root=&first;

  first.parent=0;
  first.next=0;
  first.prev=0;
  first.isleaf=TRUE;
  first.nodecount=0;

  addnode(0x232,0,root);
  printtree(root);

}


void printtree(struct TreeContainer *tc) {
  WORD n;

  for(n=0; n<tc->nodecount; n++) {
    printf("%03lx                                                             ",tc->treenode[n].key);
  }
  printf("\n");

  if(tc->isleaf==FALSE) {
    struct TreeContainer *tc2;

    tc=tc->treenode[0];
    tc2=tc;

    while(tc2!=0) {
      for(n=0; n<tc2->nodecount; n++) {
        printf("%03lx             ",tc2->treenode[n].key);
      }
      tc2=tc2->next;
    }
    printf("\n");


    if(tc->isleaf==FALSE) {
      tc=tc->treenode[0];
      tc2=tc;

      while(tc2!=0) {
        for(n=0; n<tc2->nodecount; n++) {
          printf("%03lx ",tc2->treenode[n].key);
        }
        tc2=tc2->next;
      }
      printf("\n");
    }
  }
}


LONG addnode(ULONG key,ULONG data,struct TreeContainer *tc) {
  while(tc->isleaf==FALSE) {
    WORD n;

    if(tc->nodecount<2) {
      tc=tc->treenode[0];
    }
    else {
      tc=tc->treenode[tc->nodecount-1];
      for(n=1; n<tc->nodecount-1; n++) {
        if(key < tc->treenode[n].key) {
          tc=tc->treenode[n-1];
          break;
        }
      }
    }
  }

  if(tc->nodecount<BRANCHES) {
    /* Simply insert new node in this TreeContainer */
    insertnode(key,data,tc);
  }
  else {
    struct TreeContainer *tcnew;

    /* Split the TreeContainer into two new containers */
    if((tcnew=split(tc))==0) {
      return(FALSE);
    }

    if(tcnew->treenode[0].key >= key) {
      insertnode(key,data,tcnew);
    }
    else {
      insertnode(key,data,tc);
    }
  }

  return(TRUE);
}


struct TreeContainer *split(struct TreeContainer *tc) {
  struct TreeContainer *tcnew;
  struct TreeNode *tnsrc,*tndest;
  WORD n;

  if(tc->parent==0) {
    struct TreeContainer *tcparent;

    /* We need to create Root tree-container! */

    if((tcparent=AllocMem(sizeof(struct TreeContainer),MEMF_CLEAR))==0) {
      return(0);
    }

    tcparent->isleaf=FALSE;
    /*
    tcparent->nodecount=1;
    tcparent->treenode[0].key=tc->treenode[0].key;
    tcparent->treenode[0].data=tc;
    */
    insertnode(tc->treenode[0].key,(ULONG)tc,tcparent);

    tc->parent=tcparent;
    root=tcparent;
  }

  if(tc->parent->nodecount==BRANCHES) {
    /* We need to split the parent tree-container first! */

    if(split(tc->parent)==0) {
      return(0);
    }
  }

  /* We can split this container and add it to the parent
     because the parent has enough room. */

  if((tcnew=AllocMem(sizeof(struct TreeContainer),MEMF_CLEAR))==0) {
    return(0);
  }

  tnsrc=tc->treenode[BRANCHES/2];
  tndest=tcnew->treenode[0];

  n=BRANCHES-BRANCHES/2;
  tcnew->nodecount=n;
  tcnew->parent=tc->parent;
  tcnew->next=tc->next;
  tcnew->prev=tc;
  tcnew->isleaf=tc->isleaf;
  tc->next=tcnew;
  tc->nodecount=BRANCHES/2;

  while(n-->0) {
    tndest=tnsrc++;
    if(tcnew->isleaf==FALSE) {
      ((struct TreeContainer *)tndest->data)->parent=tcnew;
    }
    tndest++;
  }

  insertnode(tcnew->treenode[0].key,tcnew,tc->parent);

  return(tcnew);
}



void insertnode(ULONG key,ULONG data,struct TreeContainer *tc) {
  struct TreeNode *tn=tc->treenode;
  WORD n=tc->nodecount;

  /* This routine inserts a node sorted into a TreeContainer.  It does
     this by starting at the end, and moving the nodes one by one to
     a higher slot until the empty slot has the correct position for
     this key.  Donot use this function on completely filled
     TreeContainers! */

  for(;;) {
    if(n==0 || key > tc->treenode[n-1].key) {
      tc->treenode[n].key=key;
      tc->treenode[n].data=data;
      tc->nodecount++;
      break;
    }
    else {
      tc->treenode[n]=tc->treenode[n-1];
    }

    n--;
  }
}
