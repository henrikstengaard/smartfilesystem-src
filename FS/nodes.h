#ifndef _NODES_H
#define _NODES_H

#include <exec/types.h>
#include <libraries/iffparse.h>
#include "blockstructure.h"

#define ROOTNODE   (1)
#define RECYCLEDNODE (2)

/* a NODE is the number of a fsNode structure in a fsNodeContainer */

typedef unsigned long NODE;

/* Structures used by Node trees */

struct fsNode {
  ULONG data;
};

#define NODECONTAINER_ID         MAKE_ID('N','D','C',' ')

struct fsNodeContainer {
  struct fsBlockHeader bheader;
  NODE  nodenumber;        /* The Node number of the first Node in this block */
  ULONG nodes;             /* The total number of Nodes per NodeIndexContainer
                              or NodeIndexContainer from this point in the
                              Node-tree.  If this is 1 it is a leaf container. */

  BLCKn node[0];           /* An array of NodeIndexContainers or NodeContainers
                              depending on where this NodeIndexContainer is
                              within the Node-tree. */
};

#endif // _NODES_H
