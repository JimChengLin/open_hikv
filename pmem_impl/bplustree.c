/*
 * Copyright (C) 2015, Leo Ma <begeekmyfriend@gmail.com>
 */

#include "bplustree.h"

#include <assert.h>
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  BPLUS_TREE_LEAF,
  BPLUS_TREE_NON_LEAF = 1,
};

enum {
  LEFT_SIBLING,
  RIGHT_SIBLING = 1,
};

#define BPLUS_PRE_ALLOC_FREE_SIZE 16

struct pre_alloc_nodes_t {
  struct bplus_non_leaf *non_leaf_nodes[BPLUS_PRE_ALLOC_FREE_SIZE];
  struct bplus_leaf *leaf_nodes[BPLUS_PRE_ALLOC_FREE_SIZE];
};

static void init_pre_alloc_nodes(struct pre_alloc_nodes_t *nodes) {
  for (int i = 0; i < BPLUS_PRE_ALLOC_FREE_SIZE; ++i) {
    struct bplus_non_leaf *node = calloc(1, sizeof(*node));
    assert(node != NULL);
    list_init(&node->link);
    node->type = BPLUS_TREE_NON_LEAF;
    node->parent_key_idx = -1;
    nodes->non_leaf_nodes[i] = node;
  }
  for (int i = 0; i < BPLUS_PRE_ALLOC_FREE_SIZE; ++i) {
    struct bplus_leaf *node = calloc(1, sizeof(*node));
    assert(node != NULL);
    list_init(&node->link);
    node->type = BPLUS_TREE_LEAF;
    node->parent_key_idx = -1;
    nodes->leaf_nodes[i] = node;
  }
}

static void deinit_pre_alloc_nodes(struct pre_alloc_nodes_t *nodes) {
  for (int i = 0; i < BPLUS_PRE_ALLOC_FREE_SIZE; ++i) {
    struct bplus_non_leaf *node = nodes->non_leaf_nodes[i];
    if (node != NULL) {
      free(node);
    }
  }
  for (int i = 0; i < BPLUS_PRE_ALLOC_FREE_SIZE; ++i) {
    struct bplus_leaf *node = nodes->leaf_nodes[i];
    if (node != NULL) {
      free(node);
    }
  }
}

struct defer_free_nodes_t {
  struct bplus_non_leaf *nodes[BPLUS_PRE_ALLOC_FREE_SIZE];
};

static void init_defer_free_nodes(struct defer_free_nodes_t *nodes) {
  for (int i = 0; i < BPLUS_PRE_ALLOC_FREE_SIZE; ++i) {
    nodes->nodes[i] = NULL;
  }
}

static void deinit_defer_free_nodes(struct defer_free_nodes_t *nodes) {
  for (int i = 0; i < BPLUS_PRE_ALLOC_FREE_SIZE; ++i) {
    struct bplus_non_leaf *node = nodes->nodes[i];
    if (node != NULL) {
      free(node);
    }
  }
}

static inline long long is_leaf(struct bplus_node *node) {
  return node->type == BPLUS_TREE_LEAF;
}

static inline int my_strcmp(my_key_t a, my_key_t b) {
  char *ap = (char *)a;
  char *bp = (char *)b;
  int a_len;
  int b_len;
  memcpy(&a_len, ap, sizeof(a_len));
  memcpy(&b_len, ap, sizeof(b_len));
  ap += sizeof(a_len);
  bp += sizeof(b_len);
  int max_len = a_len < b_len ? b_len : a_len;

  int n = 0;
  while (1) {
    ++n;
    if (*ap > *bp) {
      return +1;
    } else if (*ap < *bp) {
      return -1;
    } else if (n == max_len) {
      if (a_len > b_len) {
        return +1;
      } else if (a_len < b_len) {
        return -1;
      } else {
        return 0;
      }
    }
    ++ap;
    ++bp;
  }
}

static my_key_t key_binary_search(my_key_t *arr, long long len, my_key_t target,
                                  int use_strcmp) {
  if (!use_strcmp) {
    long long low = -1;
    long long high = len;
    while (low + 1 < high) {
      long long mid = low + (high - low) / 2;
      if (target > arr[mid]) {
        low = mid;
      } else {
        high = mid;
      }
    }
    if (high >= len || arr[high] != target) {
      return -high - 1;
    } else {
      return high;
    }
  } else {
    long long low = -1;
    long long high = len;
    while (low + 1 < high) {
      long long mid = low + (high - low) / 2;
      if (my_strcmp(target, arr[mid]) > 0) {
        low = mid;
      } else {
        high = mid;
      }
    }
    if (high >= len || my_strcmp(arr[high], target) != 0) {
      return -high - 1;
    } else {
      return high;
    }
  }
}

static struct bplus_non_leaf *non_leaf_new(
    struct pre_alloc_nodes_t *pre_alloc_nodes) {
  if (pre_alloc_nodes != NULL) {
    for (int i = 0; i < BPLUS_PRE_ALLOC_FREE_SIZE; ++i) {
      struct bplus_non_leaf *node = pre_alloc_nodes->non_leaf_nodes[i];
      if (node != NULL) {
        pre_alloc_nodes->non_leaf_nodes[i] = NULL;
        return node;
      }
    }
  }
  struct bplus_non_leaf *node = calloc(1, sizeof(*node));
  assert(node != NULL);
  list_init(&node->link);
  node->type = BPLUS_TREE_NON_LEAF;
  node->parent_key_idx = -1;
  return node;
}

static struct bplus_leaf *leaf_new(struct pre_alloc_nodes_t *pre_alloc_nodes) {
  if (pre_alloc_nodes != NULL) {
    for (int i = 0; i < BPLUS_PRE_ALLOC_FREE_SIZE; ++i) {
      struct bplus_leaf *node = pre_alloc_nodes->leaf_nodes[i];
      if (node != NULL) {
        pre_alloc_nodes->leaf_nodes[i] = NULL;
        return node;
      }
    }
  }
  struct bplus_leaf *node = calloc(1, sizeof(*node));
  assert(node != NULL);
  list_init(&node->link);
  node->type = BPLUS_TREE_LEAF;
  node->parent_key_idx = -1;
  return node;
}

static void non_leaf_delete(struct bplus_non_leaf *node,
                            struct defer_free_nodes_t *nodes) {
  list_del(&node->link);
  free(node);
}

static void leaf_delete(struct bplus_leaf *node,
                        struct defer_free_nodes_t *nodes) {
  list_del(&node->link);
  free(node);
}

static long long bplus_tree_search(struct bplus_tree *tree, my_key_t key,
                                   int use_strcmp) {
  long long i, ret = -1;
  struct bplus_node *node = tree->root;
  while (node != NULL) {
    if (is_leaf(node)) {
      struct bplus_leaf *ln = (struct bplus_leaf *)node;
      i = key_binary_search(ln->key, ln->entries, key, use_strcmp);
      ret = i >= 0 ? ln->data[i] : 0;
      break;
    } else {
      struct bplus_non_leaf *nln = (struct bplus_non_leaf *)node;
      i = key_binary_search(nln->key, nln->children - 1, key, use_strcmp);
      if (i >= 0) {
        node = nln->sub_ptr[i + 1];
      } else {
        i = -i - 1;
        node = nln->sub_ptr[i];
      }
    }
  }
  return ret;
}

static long long non_leaf_insert(
    struct bplus_tree *tree, struct bplus_non_leaf *node,
    struct bplus_node *l_ch, struct bplus_node *r_ch, my_key_t key,
    long long level, struct pre_alloc_nodes_t *pre_alloc_nodes, int use_strcmp);

static long long parent_node_build(struct bplus_tree *tree,
                                   struct bplus_node *left,
                                   struct bplus_node *right, my_key_t key,
                                   long long level,
                                   struct pre_alloc_nodes_t *pre_alloc_nodes,
                                   int use_strcmp) {
  if (left->parent == NULL && right->parent == NULL) {
    /* new parent */
    struct bplus_non_leaf *parent = non_leaf_new(pre_alloc_nodes);
    parent->key[0] = key;
    parent->sub_ptr[0] = left;
    parent->sub_ptr[0]->parent = parent;
    parent->sub_ptr[0]->parent_key_idx = -1;
    parent->sub_ptr[1] = right;
    parent->sub_ptr[1]->parent = parent;
    parent->sub_ptr[1]->parent_key_idx = 0;
    parent->children = 2;
    /* update root */
    tree->root = (struct bplus_node *)parent;
    list_add(&parent->link, &tree->list[++tree->level]);
    return 0;
  } else if (right->parent == NULL) {
    /* trace upwards */
    right->parent = left->parent;
    return non_leaf_insert(tree, left->parent, left, right, key, level + 1,
                           pre_alloc_nodes, use_strcmp);
  } else {
    /* trace upwards */
    left->parent = right->parent;
    return non_leaf_insert(tree, right->parent, left, right, key, level + 1,
                           pre_alloc_nodes, use_strcmp);
  }
}

static long long non_leaf_split_left(struct bplus_non_leaf *node,
                                     struct bplus_non_leaf *left,
                                     struct bplus_node *l_ch,
                                     struct bplus_node *r_ch, my_key_t key,
                                     long long insert) {
  long long i, j, order = node->children;
  my_key_t split_key;
  /* split = [m/2] */
  long long split = (order + 1) / 2;
  /* split as left sibling */
  __list_add(&left->link, node->link.prev, &node->link);
  /* replicate from sub[0] to sub[split - 1] */
  for (i = 0, j = 0; i < split; i++, j++) {
    if (j == insert) {
      left->sub_ptr[j] = l_ch;
      left->sub_ptr[j]->parent = left;
      left->sub_ptr[j]->parent_key_idx = j - 1;
      left->sub_ptr[j + 1] = r_ch;
      left->sub_ptr[j + 1]->parent = left;
      left->sub_ptr[j + 1]->parent_key_idx = j;
      j++;
    } else {
      left->sub_ptr[j] = node->sub_ptr[i];
      left->sub_ptr[j]->parent = left;
      left->sub_ptr[j]->parent_key_idx = j - 1;
    }
  }
  left->children = split;
  /* replicate from key[0] to key[split - 2] */
  for (i = 0, j = 0; i < split - 1; j++) {
    if (j == insert) {
      left->key[j] = key;
    } else {
      left->key[j] = node->key[i];
      i++;
    }
  }
  if (insert == split - 1) {
    left->key[insert] = key;
    left->sub_ptr[insert] = l_ch;
    left->sub_ptr[insert]->parent = left;
    left->sub_ptr[insert]->parent_key_idx = j - 1;
    node->sub_ptr[0] = r_ch;
    split_key = key;
  } else {
    node->sub_ptr[0] = node->sub_ptr[split - 1];
    split_key = node->key[split - 2];
  }
  node->sub_ptr[0]->parent = node;
  node->sub_ptr[0]->parent_key_idx = -1;
  /* left shift for right node from split - 1 to children - 1 */
  for (i = split - 1, j = 0; i < order - 1; i++, j++) {
    node->key[j] = node->key[i];
    node->sub_ptr[j + 1] = node->sub_ptr[i + 1];
    node->sub_ptr[j + 1]->parent = node;
    node->sub_ptr[j + 1]->parent_key_idx = j;
  }
  node->sub_ptr[j] = node->sub_ptr[i];
  node->children = j + 1;
  return split_key;
}

static long long non_leaf_split_right1(struct bplus_non_leaf *node,
                                       struct bplus_non_leaf *right,
                                       struct bplus_node *l_ch,
                                       struct bplus_node *r_ch, my_key_t key,
                                       long long insert) {
  long long i, j, order = node->children;
  my_key_t split_key;
  /* split = [m/2] */
  long long split = (order + 1) / 2;
  /* split as right sibling */
  list_add(&right->link, &node->link);
  /* split key is key[split - 1] */
  split_key = node->key[split - 1];
  /* left node's children always be [split] */
  node->children = split;
  /* right node's first sub-node */
  right->key[0] = key;
  right->sub_ptr[0] = l_ch;
  right->sub_ptr[0]->parent = right;
  right->sub_ptr[0]->parent_key_idx = -1;
  right->sub_ptr[1] = r_ch;
  right->sub_ptr[1]->parent = right;
  right->sub_ptr[1]->parent_key_idx = 0;
  /* insertion point is split point, replicate from key[split] */
  for (i = split, j = 1; i < order - 1; i++, j++) {
    right->key[j] = node->key[i];
    right->sub_ptr[j + 1] = node->sub_ptr[i + 1];
    right->sub_ptr[j + 1]->parent = right;
    right->sub_ptr[j + 1]->parent_key_idx = j;
  }
  right->children = j + 1;
  return split_key;
}

static long long non_leaf_split_right2(struct bplus_non_leaf *node,
                                       struct bplus_non_leaf *right,
                                       struct bplus_node *l_ch,
                                       struct bplus_node *r_ch, my_key_t key,
                                       long long insert) {
  long long i, j, order = node->children;
  my_key_t split_key;
  /* split = [m/2] */
  long long split = (order + 1) / 2;
  /* left node's children always be [split + 1] */
  node->children = split + 1;
  /* split as right sibling */
  list_add(&right->link, &node->link);
  /* split key is key[split] */
  split_key = node->key[split];
  /* right node's first sub-node */
  right->sub_ptr[0] = node->sub_ptr[split + 1];
  right->sub_ptr[0]->parent = right;
  right->sub_ptr[0]->parent_key_idx = -1;
  /* replicate from key[split + 1] to key[order - 1] */
  for (i = split + 1, j = 0; i < order - 1; j++) {
    if (j != insert - split - 1) {
      right->key[j] = node->key[i];
      right->sub_ptr[j + 1] = node->sub_ptr[i + 1];
      right->sub_ptr[j + 1]->parent = right;
      right->sub_ptr[j + 1]->parent_key_idx = j;
      i++;
    }
  }
  /* reserve a hole for insertion */
  if (j > insert - split - 1) {
    right->children = j + 1;
  } else {
    assert(j == insert - split - 1);
    right->children = j + 2;
  }
  /* insert new key and sub-node */
  j = insert - split - 1;
  right->key[j] = key;
  right->sub_ptr[j] = l_ch;
  right->sub_ptr[j]->parent = right;
  right->sub_ptr[j]->parent_key_idx = j - 1;
  right->sub_ptr[j + 1] = r_ch;
  right->sub_ptr[j + 1]->parent = right;
  right->sub_ptr[j + 1]->parent_key_idx = j;
  return split_key;
}

static void non_leaf_simple_insert(struct bplus_non_leaf *node,
                                   struct bplus_node *l_ch,
                                   struct bplus_node *r_ch, my_key_t key,
                                   long long insert) {
  long long i;
  for (i = node->children - 1; i > insert; i--) {
    node->key[i] = node->key[i - 1];
    node->sub_ptr[i + 1] = node->sub_ptr[i];
    node->sub_ptr[i + 1]->parent_key_idx = i;
  }
  node->key[i] = key;
  node->sub_ptr[i] = l_ch;
  node->sub_ptr[i]->parent_key_idx = i - 1;
  node->sub_ptr[i + 1] = r_ch;
  node->sub_ptr[i + 1]->parent_key_idx = i;
  node->children++;
}

static long long non_leaf_insert(struct bplus_tree *tree,
                                 struct bplus_non_leaf *node,
                                 struct bplus_node *l_ch,
                                 struct bplus_node *r_ch, my_key_t key,
                                 long long level,
                                 struct pre_alloc_nodes_t *pre_alloc_nodes,
                                 int use_strcmp) {
  /* search key location */
  long long insert =
      key_binary_search(node->key, node->children - 1, key, use_strcmp);
  assert(insert < 0);
  insert = -insert - 1;

  /* node is full */
  if (node->children == tree->order) {
    /* split = [m/2] */
    my_key_t split_key;
    long long split = (node->children + 1) / 2;
    struct bplus_non_leaf *sibling = non_leaf_new(pre_alloc_nodes);
    if (insert < split) {
      split_key = non_leaf_split_left(node, sibling, l_ch, r_ch, key, insert);
    } else if (insert == split) {
      split_key = non_leaf_split_right1(node, sibling, l_ch, r_ch, key, insert);
    } else {
      split_key = non_leaf_split_right2(node, sibling, l_ch, r_ch, key, insert);
    }
    /* build new parent */
    if (insert < split) {
      return parent_node_build(tree, (struct bplus_node *)sibling,
                               (struct bplus_node *)node, split_key, level,
                               pre_alloc_nodes, use_strcmp);
    } else {
      return parent_node_build(tree, (struct bplus_node *)node,
                               (struct bplus_node *)sibling, split_key, level,
                               pre_alloc_nodes, use_strcmp);
    }
  } else {
    non_leaf_simple_insert(node, l_ch, r_ch, key, insert);
  }

  return 0;
}

static void leaf_split_left(struct bplus_leaf *leaf, struct bplus_leaf *left,
                            my_key_t key, long long data, long long insert) {
  long long i, j;
  /* split = [m/2] */
  long long split = (leaf->entries + 1) / 2;
  /* split as left sibling */
  __list_add(&left->link, leaf->link.prev, &leaf->link);
  /* replicate from 0 to key[split - 2] */
  for (i = 0, j = 0; i < split - 1; j++) {
    if (j == insert) {
      left->key[j] = key;
      left->data[j] = data;
    } else {
      left->key[j] = leaf->key[i];
      left->data[j] = leaf->data[i];
      i++;
    }
  }
  if (j == insert) {
    left->key[j] = key;
    left->data[j] = data;
    j++;
  }
  left->entries = j;
  /* left shift for right node */
  for (j = 0; i < leaf->entries; i++, j++) {
    leaf->key[j] = leaf->key[i];
    leaf->data[j] = leaf->data[i];
  }
  leaf->entries = j;
}

static void leaf_split_right(struct bplus_leaf *leaf, struct bplus_leaf *right,
                             my_key_t key, long long data, long long insert) {
  long long i, j;
  /* split = [m/2] */
  long long split = (leaf->entries + 1) / 2;
  /* split as right sibling */
  list_add(&right->link, &leaf->link);
  /* replicate from key[split] */
  for (i = split, j = 0; i < leaf->entries; j++) {
    if (j != insert - split) {
      right->key[j] = leaf->key[i];
      right->data[j] = leaf->data[i];
      i++;
    }
  }
  /* reserve a hole for insertion */
  if (j > insert - split) {
    right->entries = j;
  } else {
    assert(j == insert - split);
    right->entries = j + 1;
  }
  /* insert new key */
  j = insert - split;
  right->key[j] = key;
  right->data[j] = data;
  /* left leaf number */
  leaf->entries = split;
}

static void leaf_simple_insert(struct bplus_leaf *leaf, my_key_t key,
                               long long data, long long insert) {
  long long i;
  for (i = leaf->entries; i > insert; i--) {
    leaf->key[i] = leaf->key[i - 1];
    leaf->data[i] = leaf->data[i - 1];
  }
  leaf->key[i] = key;
  leaf->data[i] = data;
  leaf->entries++;
}

static long long leaf_insert(struct bplus_tree *tree, struct bplus_leaf *leaf,
                             my_key_t key, long long data,
                             struct pre_alloc_nodes_t *pre_alloc_nodes,
                             int use_strcmp) {
  /* search key location */
  long long insert =
      key_binary_search(leaf->key, leaf->entries, key, use_strcmp);
  if (insert >= 0) {
    /* Already exists */
    return -1;
  }
  insert = -insert - 1;

  /* node full */
  if (leaf->entries == tree->entries) {
    /* split = [m/2] */
    long long split = (tree->entries + 1) / 2;
    /* splited sibling node */
    struct bplus_leaf *sibling = leaf_new(pre_alloc_nodes);
    /* sibling leaf replication due to location of insertion */
    if (insert < split) {
      leaf_split_left(leaf, sibling, key, data, insert);
    } else {
      leaf_split_right(leaf, sibling, key, data, insert);
    }
    /* build new parent */
    if (insert < split) {
      return parent_node_build(tree, (struct bplus_node *)sibling,
                               (struct bplus_node *)leaf, leaf->key[0], 0,
                               pre_alloc_nodes, use_strcmp);
    } else {
      return parent_node_build(tree, (struct bplus_node *)leaf,
                               (struct bplus_node *)sibling, sibling->key[0], 0,
                               pre_alloc_nodes, use_strcmp);
    }
  } else {
    leaf_simple_insert(leaf, key, data, insert);
  }

  return 0;
}

static long long bplus_tree_insert(struct bplus_tree *tree, my_key_t key,
                                   long long data,
                                   struct pre_alloc_nodes_t *pre_alloc_nodes,
                                   int use_strcmp) {
  struct bplus_node *node = tree->root;
  while (node != NULL) {
    if (is_leaf(node)) {
      struct bplus_leaf *ln = (struct bplus_leaf *)node;
      return leaf_insert(tree, ln, key, data, pre_alloc_nodes, use_strcmp);
    } else {
      struct bplus_non_leaf *nln = (struct bplus_non_leaf *)node;
      long long i =
          key_binary_search(nln->key, nln->children - 1, key, use_strcmp);
      if (i >= 0) {
        node = nln->sub_ptr[i + 1];
      } else {
        i = -i - 1;
        node = nln->sub_ptr[i];
      }
    }
  }

  /* new root */
  struct bplus_leaf *root = leaf_new(pre_alloc_nodes);
  root->key[0] = key;
  root->data[0] = data;
  root->entries = 1;
  tree->root = (struct bplus_node *)root;
  list_add(&root->link, &tree->list[tree->level]);
  return 0;
}

static long long non_leaf_sibling_select(struct bplus_non_leaf *l_sib,
                                         struct bplus_non_leaf *r_sib,
                                         struct bplus_non_leaf *parent,
                                         long long i) {
  if (i == -1) {
    /* the frist sub-node, no left sibling, choose the right one */
    return RIGHT_SIBLING;
  } else if (i == parent->children - 2) {
    /* the last sub-node, no right sibling, choose the left one */
    return LEFT_SIBLING;
  } else {
    /* if both left and right sibling found, choose the one with more entries */
    return l_sib->children >= r_sib->children ? LEFT_SIBLING : RIGHT_SIBLING;
  }
}

static void non_leaf_shift_from_left(struct bplus_non_leaf *node,
                                     struct bplus_non_leaf *left,
                                     long long parent_key_index,
                                     long long remove) {
  long long i;
  /* node's elements right shift */
  for (i = remove; i > 0; i--) {
    node->key[i] = node->key[i - 1];
  }
  for (i = remove + 1; i > 0; i--) {
    node->sub_ptr[i] = node->sub_ptr[i - 1];
    node->sub_ptr[i]->parent_key_idx = i - 1;
  }
  /* parent key right rotation */
  node->key[0] = node->parent->key[parent_key_index];
  node->parent->key[parent_key_index] = left->key[left->children - 2];
  /* borrow the last sub-node from left sibling */
  node->sub_ptr[0] = left->sub_ptr[left->children - 1];
  node->sub_ptr[0]->parent = node;
  node->sub_ptr[0]->parent_key_idx = -1;
  left->children--;
}

static void non_leaf_merge_into_left(struct bplus_non_leaf *node,
                                     struct bplus_non_leaf *left,
                                     long long parent_key_index,
                                     long long remove,
                                     struct defer_free_nodes_t *nodes) {
  long long i, j;
  /* move parent key down */
  left->key[left->children - 1] = node->parent->key[parent_key_index];
  /* merge into left sibling */
  for (i = left->children, j = 0; j < node->children - 1; j++) {
    if (j != remove) {
      left->key[i] = node->key[j];
      i++;
    }
  }
  for (i = left->children, j = 0; j < node->children; j++) {
    if (j != remove + 1) {
      left->sub_ptr[i] = node->sub_ptr[j];
      left->sub_ptr[i]->parent = left;
      left->sub_ptr[i]->parent_key_idx = i - 1;
      i++;
    }
  }
  left->children = i;
  /* delete empty node */
  non_leaf_delete(node, nodes);
}

static void non_leaf_shift_from_right(struct bplus_non_leaf *node,
                                      struct bplus_non_leaf *right,
                                      long long parent_key_index) {
  long long i;
  /* parent key left rotation */
  node->key[node->children - 1] = node->parent->key[parent_key_index];
  node->parent->key[parent_key_index] = right->key[0];
  /* borrow the frist sub-node from right sibling */
  node->sub_ptr[node->children] = right->sub_ptr[0];
  node->sub_ptr[node->children]->parent = node;
  node->sub_ptr[node->children]->parent_key_idx = node->children - 1;
  node->children++;
  /* left shift in right sibling */
  for (i = 0; i < right->children - 2; i++) {
    right->key[i] = right->key[i + 1];
  }
  for (i = 0; i < right->children - 1; i++) {
    right->sub_ptr[i] = right->sub_ptr[i + 1];
    right->sub_ptr[i]->parent_key_idx = i - 1;
  }
  right->children--;
}

static void non_leaf_merge_from_right(struct bplus_non_leaf *node,
                                      struct bplus_non_leaf *right,
                                      long long parent_key_index,
                                      struct defer_free_nodes_t *nodes) {
  long long i, j;
  /* move parent key down */
  node->key[node->children - 1] = node->parent->key[parent_key_index];
  node->children++;
  /* merge from right sibling */
  for (i = node->children - 1, j = 0; j < right->children - 1; i++, j++) {
    node->key[i] = right->key[j];
  }
  for (i = node->children - 1, j = 0; j < right->children; i++, j++) {
    node->sub_ptr[i] = right->sub_ptr[j];
    node->sub_ptr[i]->parent = node;
    node->sub_ptr[i]->parent_key_idx = i - 1;
  }
  node->children = i;
  /* delete empty right sibling */
  non_leaf_delete(right, nodes);
}

static void non_leaf_simple_remove(struct bplus_non_leaf *node,
                                   long long remove) {
  assert(node->children >= 2);
  for (; remove < node->children - 2; remove++) {
    node->key[remove] = node->key[remove + 1];
    node->sub_ptr[remove + 1] = node->sub_ptr[remove + 2];
    node->sub_ptr[remove + 1]->parent_key_idx = remove;
  }
  node->children--;
}

static void non_leaf_remove(struct bplus_tree *tree,
                            struct bplus_non_leaf *node, long long remove,
                            struct defer_free_nodes_t *nodes) {
  if (node->children <= (tree->order + 1) / 2) {
    struct bplus_non_leaf *l_sib = list_prev_entry(node, link);
    struct bplus_non_leaf *r_sib = list_next_entry(node, link);
    struct bplus_non_leaf *parent = node->parent;
    if (parent != NULL) {
      /* decide which sibling to be borrowed from */
      long long i = node->parent_key_idx;
      if (non_leaf_sibling_select(l_sib, r_sib, parent, i) == LEFT_SIBLING) {
        if (l_sib->children > (tree->order + 1) / 2) {
          non_leaf_shift_from_left(node, l_sib, i, remove);
        } else {
          non_leaf_merge_into_left(node, l_sib, i, remove, nodes);
          /* trace upwards */
          non_leaf_remove(tree, parent, i, nodes);
        }
      } else {
        /* remove first in case of overflow during merging with sibling */
        non_leaf_simple_remove(node, remove);
        if (r_sib->children > (tree->order + 1) / 2) {
          non_leaf_shift_from_right(node, r_sib, i + 1);
        } else {
          non_leaf_merge_from_right(node, r_sib, i + 1, nodes);
          /* trace upwards */
          non_leaf_remove(tree, parent, i + 1, nodes);
        }
      }
    } else {
      if (node->children == 2) {
        /* delete old root node */
        assert(remove == 0);
        node->sub_ptr[0]->parent = NULL;
        tree->root = node->sub_ptr[0];
        non_leaf_delete(node, nodes);
        tree->level--;
      } else {
        non_leaf_simple_remove(node, remove);
      }
    }
  } else {
    non_leaf_simple_remove(node, remove);
  }
}

static long long leaf_sibling_select(struct bplus_leaf *l_sib,
                                     struct bplus_leaf *r_sib,
                                     struct bplus_non_leaf *parent,
                                     long long i) {
  if (i == -1) {
    /* the frist sub-node, no left sibling, choose the right one */
    return RIGHT_SIBLING;
  } else if (i == parent->children - 2) {
    /* the last sub-node, no right sibling, choose the left one */
    return LEFT_SIBLING;
  } else {
    /* if both left and right sibling found, choose the one with more entries */
    return l_sib->entries >= r_sib->entries ? LEFT_SIBLING : RIGHT_SIBLING;
  }
}

static void leaf_shift_from_left(struct bplus_leaf *leaf,
                                 struct bplus_leaf *left,
                                 long long parent_key_index, long long remove) {
  /* right shift in leaf node */
  for (; remove > 0; remove--) {
    leaf->key[remove] = leaf->key[remove - 1];
    leaf->data[remove] = leaf->data[remove - 1];
  }
  /* borrow the last element from left sibling */
  leaf->key[0] = left->key[left->entries - 1];
  leaf->data[0] = left->data[left->entries - 1];
  left->entries--;
  /* update parent key */
  leaf->parent->key[parent_key_index] = leaf->key[0];
}

static void leaf_merge_into_left(struct bplus_leaf *leaf,
                                 struct bplus_leaf *left, long long remove,
                                 struct defer_free_nodes_t *nodes) {
  long long i, j;
  /* merge into left sibling */
  for (i = left->entries, j = 0; j < leaf->entries; j++) {
    if (j != remove) {
      left->key[i] = leaf->key[j];
      left->data[i] = leaf->data[j];
      i++;
    }
  }
  left->entries = i;
  /* delete merged leaf */
  leaf_delete(leaf, nodes);
}

static void leaf_shift_from_right(struct bplus_leaf *leaf,
                                  struct bplus_leaf *right,
                                  long long parent_key_index) {
  long long i;
  /* borrow the first element from right sibling */
  leaf->key[leaf->entries] = right->key[0];
  leaf->data[leaf->entries] = right->data[0];
  leaf->entries++;
  /* left shift in right sibling */
  for (i = 0; i < right->entries - 1; i++) {
    right->key[i] = right->key[i + 1];
    right->data[i] = right->data[i + 1];
  }
  right->entries--;
  /* update parent key */
  leaf->parent->key[parent_key_index] = right->key[0];
}

static void leaf_merge_from_right(struct bplus_leaf *leaf,
                                  struct bplus_leaf *right,
                                  struct defer_free_nodes_t *nodes) {
  long long i, j;
  /* merge from right sibling */
  for (i = leaf->entries, j = 0; j < right->entries; i++, j++) {
    leaf->key[i] = right->key[j];
    leaf->data[i] = right->data[j];
  }
  leaf->entries = i;
  /* delete right sibling */
  leaf_delete(right, nodes);
}

static void leaf_simple_remove(struct bplus_leaf *leaf, long long remove) {
  for (; remove < leaf->entries - 1; remove++) {
    leaf->key[remove] = leaf->key[remove + 1];
    leaf->data[remove] = leaf->data[remove + 1];
  }
  leaf->entries--;
}

static long long leaf_remove(struct bplus_tree *tree, struct bplus_leaf *leaf,
                             my_key_t key, struct defer_free_nodes_t *nodes,
                             int use_strcmp) {
  long long remove =
      key_binary_search(leaf->key, leaf->entries, key, use_strcmp);
  if (remove < 0) {
    /* Not exist */
    return -1;
  }

  if (leaf->entries <= (tree->entries + 1) / 2) {
    struct bplus_non_leaf *parent = leaf->parent;
    struct bplus_leaf *l_sib = list_prev_entry(leaf, link);
    struct bplus_leaf *r_sib = list_next_entry(leaf, link);
    if (parent != NULL) {
      /* decide which sibling to be borrowed from */
      long long i = leaf->parent_key_idx;
      if (leaf_sibling_select(l_sib, r_sib, parent, i) == LEFT_SIBLING) {
        if (l_sib->entries > (tree->entries + 1) / 2) {
          leaf_shift_from_left(leaf, l_sib, i, remove);
        } else {
          leaf_merge_into_left(leaf, l_sib, remove, nodes);
          /* trace upwards */
          non_leaf_remove(tree, parent, i, nodes);
        }
      } else {
        /* remove first in case of overflow during merging with sibling */
        leaf_simple_remove(leaf, remove);
        if (r_sib->entries > (tree->entries + 1) / 2) {
          leaf_shift_from_right(leaf, r_sib, i + 1);
        } else {
          leaf_merge_from_right(leaf, r_sib, nodes);
          /* trace upwards */
          non_leaf_remove(tree, parent, i + 1, nodes);
        }
      }
    } else {
      if (leaf->entries == 1) {
        /* delete the only last node */
        assert(key == leaf->key[0]);
        tree->root = NULL;
        leaf_delete(leaf, nodes);
        return 0;
      } else {
        leaf_simple_remove(leaf, remove);
      }
    }
  } else {
    leaf_simple_remove(leaf, remove);
  }

  return 0;
}

static long long bplus_tree_delete(struct bplus_tree *tree, my_key_t key,
                                   struct defer_free_nodes_t *nodes,
                                   int use_strcmp) {
  struct bplus_node *node = tree->root;
  while (node != NULL) {
    if (is_leaf(node)) {
      struct bplus_leaf *ln = (struct bplus_leaf *)node;
      return leaf_remove(tree, ln, key, nodes, use_strcmp);
    } else {
      struct bplus_non_leaf *nln = (struct bplus_non_leaf *)node;
      long long i =
          key_binary_search(nln->key, nln->children - 1, key, use_strcmp);
      if (i >= 0) {
        node = nln->sub_ptr[i + 1];
      } else {
        i = -i - 1;
        node = nln->sub_ptr[i];
      }
    }
  }
  return -1;
}

long long bplus_tree_get(struct bplus_tree *tree, my_key_t key,
                         int use_strcmp) {
  long long data = 0;
  while (1) {
    unsigned int tsx_code = _xbegin();
    if (tsx_code == _XBEGIN_STARTED) {
      data = bplus_tree_search(tree, key, use_strcmp);
      _xend();
      printf("pass\n");
      break;
    } else {
      printf("fail %u\n", tsx_code);
    }
  }
  if (data) {
    return data;
  } else {
    return -1;
  }
}

long long bplus_tree_put(struct bplus_tree *tree, my_key_t key, long long data,
                         int use_strcmp) {
  if (data) {
    struct pre_alloc_nodes_t nodes;
    init_pre_alloc_nodes(&nodes);
    long long ret = 0;
    while (1) {
      unsigned int tsx_code = _xbegin();
      if (tsx_code == _XBEGIN_STARTED) {
        ret = bplus_tree_insert(tree, key, data, &nodes, use_strcmp);
        _xend();
        printf("pass\n");
        break;
      } else {
        printf("fail %u\n", tsx_code);
      }
    }
    deinit_pre_alloc_nodes(&nodes);
    return ret;
  } else {
    struct defer_free_nodes_t nodes;
    init_defer_free_nodes(&nodes);
    long long ret = bplus_tree_delete(tree, key, &nodes, 0);
    deinit_defer_free_nodes(&nodes);
    return ret;
  }
}

struct bplus_tree *bplus_tree_init(long long order, long long entries) {
  /* The max order of non leaf nodes must be more than two */
  assert(BPLUS_MAX_ORDER > BPLUS_MIN_ORDER);
  assert(order <= BPLUS_MAX_ORDER && entries <= BPLUS_MAX_ENTRIES);

  long long i;
  struct bplus_tree *tree = calloc(1, sizeof(*tree));
  if (tree != NULL) {
    tree->root = NULL;
    tree->order = order;
    tree->entries = entries;
    for (i = 0; i < BPLUS_MAX_LEVEL; i++) {
      list_init(&tree->list[i]);
    }
  }

  return tree;
}

void bplus_tree_deinit(struct bplus_tree *tree) { free(tree); }

long long bplus_tree_get_range(struct bplus_tree *tree, my_key_t key1,
                               my_key_t key2) {
  long long i, data = 0;
  my_key_t min = key1 <= key2 ? key1 : key2;
  my_key_t max = min == key1 ? key2 : key1;
  struct bplus_node *node = tree->root;

  while (node != NULL) {
    if (is_leaf(node)) {
      struct bplus_leaf *ln = (struct bplus_leaf *)node;
      i = key_binary_search(ln->key, ln->entries, min, 0);
      if (i < 0) {
        i = -i - 1;
        if (i >= ln->entries) {
          if (list_is_last(&ln->link, &tree->list[0])) {
            return -1;
          }
          ln = list_next_entry(ln, link);
        }
      }
      while (ln->key[i] <= max) {
        data = ln->data[i];
        if (++i >= ln->entries) {
          if (list_is_last(&ln->link, &tree->list[0])) {
            return -1;
          }
          ln = list_next_entry(ln, link);
          i = 0;
        }
      }
      break;
    } else {
      struct bplus_non_leaf *nln = (struct bplus_non_leaf *)node;
      i = key_binary_search(nln->key, nln->children - 1, min, 0);
      if (i >= 0) {
        node = nln->sub_ptr[i + 1];
      } else {
        i = -i - 1;
        node = nln->sub_ptr[i];
      }
    }
  }

  return data;
}

#ifdef _BPLUS_TREE_DEBUG
struct node_backlog {
  /* Node backlogged */
  struct bplus_node *node;
  /* The index next to the backtrack point, must be >= 1 */
  long long next_sub_idx;
};

static inline long long children(struct bplus_node *node) {
  return ((struct bplus_non_leaf *)node)->children;
}

static void node_key_dump(struct bplus_node *node) {
  long long i;
  if (is_leaf(node)) {
    for (i = 0; i < node->count; i++) {
      printf("%d ", ((struct bplus_leaf *)node)->key[i]);
    }
  } else {
    for (i = 0; i < node->count - 1; i++) {
      printf("%d ", ((struct bplus_non_leaf *)node)->key[i]);
    }
  }
  printf("\n");
}

static my_key_t node_key(struct bplus_node *node, long long i) {
  if (is_leaf(node)) {
    return ((struct bplus_leaf *)node)->key[i];
  } else {
    return ((struct bplus_non_leaf *)node)->key[i];
  }
}

static void key_print(struct bplus_node *node) {
  long long i;
  if (is_leaf(node)) {
    struct bplus_leaf *leaf = (struct bplus_leaf *)node;
    printf("leaf:");
    for (i = 0; i < leaf->entries; i++) {
      printf(" %d", leaf->key[i]);
    }
  } else {
    struct bplus_non_leaf *non_leaf = (struct bplus_non_leaf *)node;
    printf("node:");
    for (i = 0; i < non_leaf->children - 1; i++) {
      printf(" %d", non_leaf->key[i]);
    }
  }
  printf("\n");
}

void bplus_tree_dump(struct bplus_tree *tree) {
  long long level = 0;
  struct bplus_node *node = tree->root;
  struct node_backlog *p_nbl = NULL;
  struct node_backlog nbl_stack[BPLUS_MAX_LEVEL];
  struct node_backlog *top = nbl_stack;

  for (;;) {
    if (node != NULL) {
      /* non-zero needs backward and zero does not */
      long long sub_idx = p_nbl != NULL ? p_nbl->next_sub_idx : 0;
      /* Reset each loop */
      p_nbl = NULL;

      /* Backlog the path */
      if (is_leaf(node) || sub_idx + 1 >= children(node)) {
        top->node = NULL;
        top->next_sub_idx = 0;
      } else {
        top->node = node;
        top->next_sub_idx = sub_idx + 1;
      }
      top++;
      level++;

      /* Draw the whole node when the first entry is passed through */
      if (sub_idx == 0) {
        long long i;
        for (i = 1; i < level; i++) {
          if (i == level - 1) {
            printf("%-8s", "+-------");
          } else {
            if (nbl_stack[i - 1].node != NULL) {
              printf("%-8s", "|");
            } else {
              printf("%-8s", " ");
            }
          }
        }
        key_print(node);
      }

      /* Move deep down */
      node = is_leaf(node) ? NULL
                           : ((struct bplus_non_leaf *)node)->sub_ptr[sub_idx];
    } else {
      p_nbl = top == nbl_stack ? NULL : --top;
      if (p_nbl == NULL) {
        /* End of traversal */
        break;
      }
      node = p_nbl->node;
      level--;
    }
  }
}
#endif