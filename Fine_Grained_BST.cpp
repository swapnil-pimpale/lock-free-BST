#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "Fine_Grained_BST.h"

FG_BST_Node* insert(int val, FG_BST_Node* root, FG_BST_Node *parent) {

	if(root == NULL) {
		root = createNode(val, parent);
		return root;
	}		

	//TODO: Change the implementation of both inserts to be 'pre-emptive' (aka check if left ptr is null before traversing left)
	if(val < root->value) {
		root->left = insert(val, root->left, root);
	}
	else {
		root->right = insert(val, root->right, root);
	}

	return root;
}

FG_BST_Node* createNode(int val, FG_BST_Node *parent) {

	FG_BST_Node* node = (FG_BST_Node *) malloc(sizeof(FG_BST_Node));

	if(node == NULL) {
		fprintf(stderr, "Failed to allocate memory for new node");
		return node;
	}

	node->value = val;
	node->left = NULL;
	node->right = NULL;
	node->parent = parent;

	return node;
}

FG_BST_Node* search(int val, FG_BST_Node* root) {

	if(root == NULL) {
		printf("Could not find the node with value: %d\n", val);
		return root;
	}

	if(val == root->value) {
		printf("Found the node with value: %d\n", root->value);
		return root; 
	} else if(val < root->value) {
		return search(val, root->left);
	} else {
		return search(val, root->right);
	}
}	

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

FG_BST_Node *get_inorder_successor(FG_BST_Node *node)
{
	FG_BST_Node *successor = node->right;

	if (successor == NULL)
		return successor;

	while (successor->left != NULL) {
		successor = successor->left;
	}

	return successor;
}
