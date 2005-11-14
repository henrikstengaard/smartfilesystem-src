#ifndef TREE_H_
#define TREE_H_

#include <exec/types.h>

struct TreeNode {
  struct TreeNode *parent;
  struct TreeNode *left;
  struct TreeNode *right;
  ULONG data;   /* the sort criteria -- user defined */
  UWORD weight;
};

#endif /*TREE_H_*/
