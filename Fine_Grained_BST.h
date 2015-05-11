#ifndef _FINE_GRAINED_BST_H_
#define _FINE_GRAINED_BST_H_

typedef struct Fine_Grained_BST_Node {
	int value;
	struct Fine_Grained_BST_Node *left;
	struct Fine_Grained_BST_Node *right;
	struct Fine_Grained_BST_Node *parent;
	pthread_mutex_t lock;
}FG_BST_Node;

void insert(int val, FG_BST_Node* root, FG_BST_Node *parent, int thread_num);
void search(int val, FG_BST_Node* root, FG_BST_Node *parent);
FG_BST_Node* createNode(int val, FG_BST_Node *parent);
FG_BST_Node *get_inorder_successor(FG_BST_Node *root);
FG_BST_Node *get_inorder_predecessor(FG_BST_Node *root);
FG_BST_Node* del_search(int val, FG_BST_Node* root, int thread_num);
int remove(int val, FG_BST_Node* root, int thread_num);

#endif
