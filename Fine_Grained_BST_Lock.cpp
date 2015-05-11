#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "Fine_Grained_BST.h"
#include "cycle_timer.h"

extern pthread_mutex_t tree_lock;
extern FG_BST_Node *g_root;

void search(int val, FG_BST_Node *root, FG_BST_Node *parent)
{
	if(parent == NULL) { //I am at the root
		pthread_mutex_lock(&tree_lock);
		if(g_root == NULL) {
			printf("Search failed for node with value %d\n", val);
			pthread_mutex_unlock(&tree_lock);
			return;
		}
		pthread_mutex_lock(&g_root->lock);
		root = g_root;
		pthread_mutex_unlock(&tree_lock);		
	}

	if(val < root->value) {
		if (root->left == NULL) {
			printf("Search failed for node with value %d\n", val);
			pthread_mutex_unlock(&root->lock);
			return;
		} else {
			pthread_mutex_lock(&root->left->lock);
			pthread_mutex_unlock(&root->lock);
			search(val, root->left, root);
		}
	}
	else if (val > root->value) {
		if (root->right == NULL) {
			printf("Search failed for node with value %d\n", val);
			pthread_mutex_unlock(&root->lock);
			return;
		} else {
			pthread_mutex_lock(&root->right->lock);
			pthread_mutex_unlock(&root->lock);
			search(val, root->right, root);
		}
	} else {
		//printf("Search succeeded, found the node with value %d\n", val);
		pthread_mutex_unlock(&root->lock);
	}
}	
/**
 * insert:
 * This entered with lock on root held except for the very first call.
 */

void insert(int val, FG_BST_Node* root, FG_BST_Node *parent, int thread_num) {

	if(parent == NULL) { //I am at the root
		pthread_mutex_lock(&tree_lock);
		if(g_root == NULL) {
			g_root = createNode(val, parent);
			pthread_mutex_unlock(&tree_lock);
			return;
		}
		pthread_mutex_lock(&g_root->lock);
		root = g_root;
		pthread_mutex_unlock(&tree_lock);		
	}

	if(val < root->value) {
		if (root->left == NULL) {
			root->left = createNode(val, root);
			pthread_mutex_unlock(&root->lock);
		} else {
			pthread_mutex_lock(&root->left->lock);
			pthread_mutex_unlock(&root->lock);
			insert(val, root->left, root, thread_num);
		}
	}
	else if (val > root->value) {
		if (root->right == NULL) {
			root->right = createNode(val, root);
			pthread_mutex_unlock(&root->lock);
		} else {
			pthread_mutex_lock(&root->right->lock);
			pthread_mutex_unlock(&root->lock);
			insert(val, root->right, root, thread_num);
		}
	} else {
		printf("Duplicates not allowed");
		assert(0);
	}
}

FG_BST_Node* createNode(int val, FG_BST_Node *parent) {

	printf("creating a new node\n");
	FG_BST_Node* node = (FG_BST_Node *) malloc(sizeof(FG_BST_Node));

	if(node == NULL) {
		fprintf(stderr, "Failed to allocate memory for new node");
		return node;
	}

	node->value = val;
	node->left = NULL;
	node->right = NULL;
	node->parent = parent;
	pthread_mutex_init(&node->lock, NULL);

	return node;
}

#if 0
int remove(int val, FG_BST_Node *root)
{
	FG_BST_Node *to_be_deleted, *parent, *successor_parent, *successor;

	if (root == NULL) {
		printf("Root of the tree is NULL\n");
		return -EINVAL;
	}

	to_be_deleted = search(val, root);
	if (to_be_deleted == NULL) {
		printf("Could not find the item to be deleted\n");
		return 0;
	}

	parent = to_be_deleted->parent;
	if ((to_be_deleted->left == NULL && to_be_deleted->right != NULL) ||
	    (to_be_deleted->left != NULL && to_be_deleted->right == NULL)) {
		/*
		 * node to be deleted has only one child. So just patch the child
		 * to the parent
		 */		
		if (parent != NULL) {
			if (to_be_deleted->left != NULL) {
				if (to_be_deleted->value >= parent->value) {
					parent->right = to_be_deleted->left;
				} else {
					parent->left = to_be_deleted->left;
				}
			} else {
				if (to_be_deleted->value >= parent->value) {
					parent->right = to_be_deleted->right;
				} else {
					parent->left = to_be_deleted->right;
				}
			}
		} else {
			if (to_be_deleted->left != NULL)
				g_root = to_be_deleted->left;
			else
				g_root = to_be_deleted->right;
		}

		free(to_be_deleted);
	}

	successor = get_inorder_successor(to_be_deleted);
	if (successor == NULL && parent == NULL) {
		// deleting the root and tree has only the root
		free(to_be_deleted);
	} else if (successor != NULL && parent == NULL) {
		// deleting the root and tree has other nodes
		successor_parent = successor->parent;
		to_be_deleted->value = successor->value;
		if (successor->value >= successor_parent->value) {
			successor_parent->right = NULL;
		} else {
			successor_parent->left = NULL;
		}

		free(successor);
	} else if (successor == NULL && parent != NULL) {
		// deleting a leaf node
		if (to_be_deleted->value >= parent->value) {
			parent->right = NULL;
		} else {
			parent->left = NULL;
		}

		free(to_be_deleted);
	} else if (successor != NULL && parent != NULL) {
		successor_parent = successor->parent;
		// deleting an internal node
		successor_parent = successor->parent;
		to_be_deleted->value = successor->value;
		if (successor->value >= successor_parent->value) {
			successor_parent->right = NULL;
		} else {
			successor_parent->left = NULL;
		}

		free(successor);
	} else {
		assert(0);
	}

	return 0;
}
#endif

/**
 * del_search:
 * search for the first node that matches the value val.
 * It is assumed that this function will be called with a lock on root held
 * If del_search() finds the required node it grabs the lock on the node and 
 * returns.
 * If it doesn't find the node it releases all the locks and returns
 */
FG_BST_Node* del_search(int val, FG_BST_Node* root, int thread_num)
{
	//printf("Thread-%d is at %d\n", thread_num, root->value);
	if(val == root->value) {
		/*
		 * We will come in here if we want to delete the root node.
		 * No need to lock the root because it is already locked by
		 * the caller.
		 */
		//printf("Found the node with value: %d\n", root->value);
		return root; 
	} else if (val < root->value) {
		if (root->left == NULL) {
			pthread_mutex_unlock(&root->lock);
			return NULL;
		} else {
			pthread_mutex_lock(&root->left->lock);
			if (val == root->left->value) {
				return root->left;
			} else {
				pthread_mutex_unlock(&root->lock);
				return del_search(val, root->left, thread_num);
			}
		}
	} else {
		if (root->right == NULL) {
			/*
			 * Could not find the node, unlock current root and return
			 */
			pthread_mutex_unlock(&root->lock);
			return NULL;
		} else {
			pthread_mutex_lock(&root->right->lock);
			if (val == root->right->value) {
				return root->right;
			} else {
				pthread_mutex_unlock(&root->lock);
				return del_search(val, root->right, thread_num);
			}
		}
	}

	return NULL;
}	

int remove(int val, FG_BST_Node *root, int thread_num)
{
	FG_BST_Node *to_be_deleted, *parent, *successor_parent, *successor;
	FG_BST_Node *predecessor, *predecessor_parent;

	pthread_mutex_lock(&tree_lock);
	if (g_root == NULL) {
		pthread_mutex_unlock(&tree_lock);
		return -EINVAL;
	}

	root = g_root;
	pthread_mutex_lock(&root->lock);
	/*
	 * Check if we are deleting the root and that the root is the only node 
	 * in the tree
	 */
	if (val == root->value && root->left == NULL && root->right == NULL) {
		/*
		 * free the root node,
		 * set the tree's global root as NULL,
		 * unlock the treelock and return
		 */
		//pthread_mutex_unlock(&root->lock);
		free(root);
		g_root = NULL;
		pthread_mutex_unlock(&tree_lock);
		return 0;
	}

	to_be_deleted = del_search(val, root, thread_num);
	if (to_be_deleted == NULL) {
		/*
		 * if del_search() returns NULL we can be sure that it is not
		 * holding any locks. So we can just return from here.
		 */
		printf("Could not find the item (%d) to be deleted\n", val);
		pthread_mutex_unlock(&tree_lock);
		return 0;
	}

	/*
	 * If we found the node to be deleted we can be sure that we have a lock
	 * on the node to be deleted and its parent (if parent exists)
	 */

	// store the parent
	parent = to_be_deleted->parent;

	/*
	 * Because we use the data swapping mechanism we need the lock on parent only 
	 * if the node to be deleted is a leaf node. Because that is the only case 
	 * where we need to update to_be_deleted's parent's pointer to NULL
	 */
	if (to_be_deleted->left == NULL && to_be_deleted->right == NULL &&
	    parent == NULL) {
#if 0
		/*
		 * Should never come here.
		 * This means the to_be_deleted_ node is the root with no other nodes
		 * in the tree. This case is handled above.
		 */
		assert(0);
#endif
		//pthread_mutex_unlock(&to_be_deleted->lock);
		free(to_be_deleted);
		g_root = NULL;
		pthread_mutex_unlock(&tree_lock);
		return 0;
	}

	pthread_mutex_unlock(&tree_lock);
	// Leaf node to be deleted
	if (to_be_deleted->left == NULL && to_be_deleted->right == NULL) {
		if (to_be_deleted->value < parent->value) {
			// node to be deleted is the left child of its parent
			/*
			 * Unlock to_be_deleted. We can safely unlock here because we hold 
			 * a lock on the parent and nobody else can come and modify to_be_deleted
			 */
			//pthread_mutex_unlock(&to_be_deleted->lock);
			// free the node
			free(to_be_deleted);
			// set parent's left child as NULL
			parent->left = NULL;
			// unlock the parent and return
			pthread_mutex_unlock(&parent->lock);
			return 0;
		} else {
			// node to be deleted is the right child of its parent
			// Follow the same procedure as above
			//pthread_mutex_unlock(&to_be_deleted->lock);
			free(to_be_deleted);
			parent->right = NULL;
			pthread_mutex_unlock(&parent->lock);
			return 0;
		}
	}

	/*
	 * At this point, we are either deleting the root or an internal node.
	 * If deleting an internal node, then unlock the parent.
	 */
	if (parent != NULL) {
		pthread_mutex_unlock(&parent->lock);
	}

	/*
	 * Find inorder successor or inorder predecessor (whichever exists) for the 
	 * node to be deleted
	 */
	if (to_be_deleted->right != NULL) {
		// perform pre-emptive check
		pthread_mutex_lock(&to_be_deleted->right->lock);
		if (to_be_deleted->right->left == NULL) {
			/*
			 * The right node is the successor,
			 * copy the value,
			 * lock successor's right --- we had not discussed this. But this is needed
			 * update pointers,
			 * unlock successor's right,
			 * unlock successor,
			 * free successor,
			 * unlock to_be_deleted and return.
			 */
			successor = to_be_deleted->right;

			if (successor->right != NULL) {
				pthread_mutex_lock(&successor->right->lock);
				to_be_deleted->value = successor->value;
				to_be_deleted->right = successor->right;
				successor->right->parent = to_be_deleted;
				pthread_mutex_unlock(&successor->right->lock);
			} else {
				to_be_deleted->value = successor->value;
				to_be_deleted->right = NULL;
			}
			//pthread_mutex_unlock(&successor->lock);
			free(successor);
			pthread_mutex_unlock(&to_be_deleted->lock);
			return 0;
		}

		/*
		 * Else,
		 * Find the successor,
		 * copy the value,
		 * update successor's parent AND successor->right,
		 * unlock successor,
		 * free successor,
		 * unlock successor's parent,
		 * unlock to_be_deleted and return
		 */

		successor = get_inorder_successor(to_be_deleted);
		successor_parent = successor->parent;

		if (successor->right != NULL) {
			pthread_mutex_lock(&successor->right->lock);
			// the successor will always be the left child of it's parent
			successor_parent->left = successor->right;
			successor->right->parent = successor_parent;
			to_be_deleted->value = successor->value;
			pthread_mutex_unlock(&successor->right->lock);
		} else {
			to_be_deleted->value = successor->value;
			successor_parent->left = NULL;
		}

		//pthread_mutex_unlock(&successor->lock);
		free(successor);
		pthread_mutex_unlock(&successor_parent->lock);
		pthread_mutex_unlock(&to_be_deleted->lock);
		return 0;
	}

	/*
	 * The node to be deleted doesn't have a successor. We need to find 
	 * its predecessor.
	 * Very similar logic to finding successor.
	 */

	if (to_be_deleted->left != NULL) {
		pthread_mutex_lock(&to_be_deleted->left->lock);
		// perform pre-emptive check
		if (to_be_deleted->left->right == NULL) {
			/*
			 * The left node is the predecessor.
			 * copy the value,
			 * lock predecessor's left ---- We had discussed this but it is needed
			 * update pointers,
			 * unlock predecessor,
			 * free predecessor,
			 * unlock to_be_deleted and return.
			 */
			predecessor = to_be_deleted->left;

			if (predecessor->left != NULL) {
				pthread_mutex_lock(&predecessor->left->lock);
				to_be_deleted->value = predecessor->value;
				to_be_deleted->left = predecessor->left;
				predecessor->left->parent = to_be_deleted;
				pthread_mutex_unlock(&predecessor->left->lock);
			} else {
				to_be_deleted->value = predecessor->value;
				to_be_deleted->left = NULL;
			}

			//pthread_mutex_unlock(&predecessor->lock);
			free(predecessor);
			pthread_mutex_unlock(&to_be_deleted->lock);
			return 0;
		}

		/*
		 * Else,
		 * Find the predecessor,
		 * Copy the value,
		 * Update predecessor's parent AND predecessor->left,
		 * unlock predecessor,
		 * free predecessor,
		 * unlock predecessor's parent,
		 * unlock to_be_deleted and return.
		 */

		predecessor = get_inorder_predecessor(to_be_deleted);
		predecessor_parent = predecessor->parent;

		if (predecessor->left != NULL) {
			pthread_mutex_lock(&predecessor->left->lock);
			// predecessor will always be the right child of its parent
			predecessor_parent->right = predecessor->left;
			predecessor->left->parent = predecessor_parent;
			to_be_deleted->value = predecessor->value;
			pthread_mutex_unlock(&predecessor->left->lock);
		} else {
			to_be_deleted->value = predecessor->value;
			predecessor_parent->right = NULL;
		}
		//pthread_mutex_unlock(&predecessor->lock);
		free(predecessor);
		pthread_mutex_unlock(&predecessor_parent->lock);
		pthread_mutex_unlock(&to_be_deleted->lock);
		return 0;
	}

	return -1;
}

/**
 * get_inorder_successor:
 * This function is expected to return with a lock on both 
 * 1) the successor and
 * 2) the successor's parent
 */
FG_BST_Node *get_inorder_successor(FG_BST_Node *node)
{
	FG_BST_Node *parent, *successor;

	parent = node->right;
	successor = parent->left;

	// lock the successor
	pthread_mutex_lock(&successor->lock);

	while (successor->left != NULL) {
		successor = successor->left;
		// unlock the old parent
		pthread_mutex_unlock(&parent->lock);
		// lock the new successor
		pthread_mutex_lock(&successor->lock);
		// update the parent
		parent = successor->parent;
	}

	return successor;
}

/**
 * get_inorder_predecessor:
 * This function is expected to return with a lock on both
 * 1) the predecessor and 
 * 2) the predecessor's parent
 */
FG_BST_Node *get_inorder_predecessor(FG_BST_Node *node)
{
	FG_BST_Node *parent, *predecessor;

	parent = node->left;
	predecessor = parent->right;

	// lock the predecessor
	pthread_mutex_lock(&predecessor->lock);

	while (predecessor->right != NULL) {
		predecessor = predecessor->right;
		// unlock the old parent
		pthread_mutex_unlock(&parent->lock);
		// lock the new predecessor
		pthread_mutex_lock(&predecessor->lock);
		// update the parent
		parent = predecessor->parent;
	}

	return predecessor;
}
