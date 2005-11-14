
#include <exec/types.h>
#include "tree.h"

/* Code to work with trees and to keep the tree balanced.  The tree is
   a weighted tree which will be kept optimal at all times. */



void addtreenode(struct TreeNode *head,struct TreeNode *node) {

}


void removetreenode(struct TreeNode *node) {
}


/*
   Po1    ->   Po2      -> Po2+1             Ro3    ->    Ro4        ->    Ro4+1              Ro5                To5
                 \           \               / \          / \              / \                / \                / \
                  \           \             /   \        /   \            /   \              /   \              /   \
                  Ro1         Ro2*    =>  Po1   So1    Po1   So2        Po1   So2+1        Po1   To3*         Ro3   Uo1
                                \                              \                \      =>        / \    =>    / \
                                 \                              \                \              /   \        /   \
                                 So1                            To1              To2*         So1   Uo1    Po1   So1
                                                                                   \
                                                                                    \
                                                                                    Uo1


   Po0    ->   Po1      -> Po1+1             Ro2    ->    Ro3        ->    Ro3+1              Ro4                To5
                 \           \               / \          / \              / \                / \                / \
                  \           \             /   \        /   \            /   \              /   \              /   \
                  Ro0         Ro1*    =>  Po0   So0    Po0   So1        Po0   So1+1        Po0   To2          Ro3   Uo1
                                \                              \                \      =>        / \    =>    / \
                                 \                              \                \              /   \        /   \
                                 So0                            To0              To1*         So0   Uo0    Po1   So1
                                                                                   \
                                                                                    \
                                                                                    Uo0




Unbalanced tree, definition:

A tree which isn't properly balanced will have nodes which
have childnodes which are larger than half the weight of the
parent node (*).

weightleft * 4 < weightright || weightright * 4 < weightleft


  0 vs 1
  1 vs 3
  2 vs 3
  3 vs 7
  4 vs 7
  5 vs 7
  6 vs 7
  7 vs 15
  8 vs 15
  9 vs 15
 10 vs 15
 11 vs 15
 12 vs 15
 13 vs 15
 14 vs 15
 15 vs 31


          Ro11
          / \
         /   \
        /     \
       /       \
     Go3       Uo7
     / \       / \
    /   \     /   \
  Ao1   Ho1 To3   Wo3
                  / \
                 /   \
               Vo1   Xo1
*/



#include <stdlib.h>
#include <stdio.h>

/* modify these lines to establish data type */
typedef int T;
#define CompLT(a,b) (a < b)
#define CompEQ(a,b) (a == b)

/* red-black tree description */
typedef enum { Black, Red } NodeColor;

typedef struct Node_ {
    struct Node_ *Left;         /* left child */
    struct Node_ *Right;        /* right child */
    struct Node_ *Parent;       /* parent */
    NodeColor Color;            /* node color (black, red) */
    T Data;                     /* data stored in node */
} Node;

#define NIL &Sentinel           /* all leafs are sentinels */
Node Sentinel = { NIL, NIL, 0, Black, 0};

Node *Root = NIL;               /* root of red-black tree */


Node *InsertNode(T Data) {
    Node *Current, *Parent, *X;

   /***********************************************
    *  allocate node for Data and insert in tree  *
    ***********************************************/

    /* find where node belongs */
    Current = Root;
    Parent = 0;
    while (Current != NIL) {
        if (CompEQ(Data, Current->Data)) return (Current);
        Parent = Current;
        Current = CompLT(Data, Current->Data) ?
            Current->Left : Current->Right;
    }

    /* setup new node */
    if ((X = malloc (sizeof(*X))) == 0) {
        printf ("insufficient memory (InsertNode)\n");
        exit(1);
    }
    X->Data = Data;
    X->Parent = Parent;
    X->Left = NIL;
    X->Right = NIL;
    X->Color = Red;

    /* insert node in tree */
    if(Parent) {
        if(CompLT(Data, Parent->Data))
            Parent->Left = X;
        else
            Parent->Right = X;
    } else {
        Root = X;
    }

    InsertFixup(X);
    return(X);
}

void InsertFixup(Node *X) {

   /*************************************
    *  maintain red-black tree balance  *
    *  after inserting node X           *
    *************************************/

    /* check red-black properties */
    while (X != Root && X->Parent->Color == Red) {
        /* we have a violation */
        if (X->Parent == X->Parent->Parent->Left) {
            Node *Y = X->Parent->Parent->Right;
            if (Y->Color == Red) {

                /* uncle is red */
                X->Parent->Color = Black;
                Y->Color = Black;
                X->Parent->Parent->Color = Red;
                X = X->Parent->Parent;
            } else {

                /* uncle is black */
                if (X == X->Parent->Right) {
                    /* make X a left child */
                    X = X->Parent;
                    RotateLeft(X);
                }

                /* recolor and rotate */
                X->Parent->Color = Black;
                X->Parent->Parent->Color = Red;
                RotateRight(X->Parent->Parent);
            }
        } else {

            /* mirror image of above code */
            Node *Y = X->Parent->Parent->Left;
            if (Y->Color == Red) {

                /* uncle is red */
                X->Parent->Color = Black;
                Y->Color = Black;
                X->Parent->Parent->Color = Red;
                X = X->Parent->Parent;
            } else {

                /* uncle is black */
                if (X == X->Parent->Left) {
                    X = X->Parent;
                    RotateRight(X);
                }
                X->Parent->Color = Black;
                X->Parent->Parent->Color = Red;
                RotateLeft(X->Parent->Parent);
            }
        }
    }
    Root->Color = Black;
}

void RotateLeft(Node *X) {

   /**************************
    *  rotate Node X to left *
    **************************/

    Node *Y = X->Right;

    /* establish X->Right link */
    X->Right = Y->Left;
    if (Y->Left != NIL) Y->Left->Parent = X;

    /* establish Y->Parent link */
    if (Y != NIL) Y->Parent = X->Parent;
    if (X->Parent) {
        if (X == X->Parent->Left)
            X->Parent->Left = Y;
        else
            X->Parent->Right = Y;
    } else {
        Root = Y;
    }

    /* link X and Y */
    Y->Left = X;
    if (X != NIL) X->Parent = Y;
}

void RotateRight(Node *X) {

   /****************************
    *  rotate Node X to right  *
    ****************************/

    Node *Y = X->Left;

    /* establish X->Left link */
    X->Left = Y->Right;
    if (Y->Right != NIL) Y->Right->Parent = X;

    /* establish Y->Parent link */
    if (Y != NIL) Y->Parent = X->Parent;
    if (X->Parent) {
        if (X == X->Parent->Right)
            X->Parent->Right = Y;
        else
            X->Parent->Left = Y;
    } else {
        Root = Y;
    }

    /* link X and Y */
    Y->Right = X;
    if (X != NIL) X->Parent = Y;
}

void DeleteNode(Node *Z) {
    Node *X, *Y;

   /*****************************
    *  delete node Z from tree  *
    *****************************/

    if (!Z || Z == NIL) return;

    if (Z->Left == NIL || Z->Right == NIL) {
        /* Y has a NIL node as a child */
        Y = Z;
    } else {
        /* find tree successor with a NIL node as a child */
        Y = Z->Right;
        while (Y->Left != NIL) Y = Y->Left;
    }

    /* X is Y's only child */
    if (Y->Left != NIL)
        X = Y->Left;
    else
        X = Y->Right;

    /* remove Y from the parent chain */
    X->Parent = Y->Parent;
    if (Y->Parent)
        if (Y == Y->Parent->Left)
            Y->Parent->Left = X;
        else
            Y->Parent->Right = X;
    else
        Root = X;

    if (Y != Z) Z->Data = Y->Data;      // not allowed!!
    if (Y->Color == Black)
        DeleteFixup (X);
    free (Y);
}

void DeleteFixup(Node *X) {

   /*************************************
    *  maintain red-black tree balance  *
    *  after deleting node X            *
    *************************************/

    while (X != Root && X->Color == Black) {
        if (X == X->Parent->Left) {
            Node *W = X->Parent->Right;
            if (W->Color == Red) {
                W->Color = Black;
                X->Parent->Color = Red;
                RotateLeft (X->Parent);
                W = X->Parent->Right;
            }
            if (W->Left->Color == Black && W->Right->Color == Black) {
                W->Color = Red;
                X = X->Parent;
            } else {
                if (W->Right->Color == Black) {
                    W->Left->Color = Black;
                    W->Color = Red;
                    RotateRight (W);
                    W = X->Parent->Right;
                }
                W->Color = X->Parent->Color;
                X->Parent->Color = Black;
                W->Right->Color = Black;
                RotateLeft (X->Parent);
                X = Root;
            }
        } else {
            Node *W = X->Parent->Left;
            if (W->Color == Red) {
                W->Color = Black;
                X->Parent->Color = Red;
                RotateRight (X->Parent);
                W = X->Parent->Left;
            }
            if (W->Right->Color == Black && W->Left->Color == Black) {
                W->Color = Red;
                X = X->Parent;
            } else {
                if (W->Left->Color == Black) {
                    W->Right->Color = Black;
                    W->Color = Red;
                    RotateLeft (W);
                    W = X->Parent->Left;
                }
                W->Color = X->Parent->Color;
                X->Parent->Color = Black;
                W->Left->Color = Black;
                RotateRight (X->Parent);
                X = Root;
            }
        }
    }
    X->Color = Black;
}

Node *FindNode(T Data) {

   /*******************************
    *  find node containing Data  *
    *******************************/

    Node *Current = Root;
    while(Current != NIL)
        if(CompEQ(Data, Current->Data))
            return (Current);
        else
            Current = CompLT (Data, Current->Data) ?
                Current->Left : Current->Right;
    return(0);
}

