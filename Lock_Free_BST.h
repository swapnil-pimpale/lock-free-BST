#ifndef _LOCK_FREE_BST_H_
#define _LOCK_FREE_BST_H_

#define ONE				0x00000001
#define TWO				0x00000002
#define THREE				0x00000003
#define HP_THRESHOLD			6
#define NUM_HP_PER_THREAD               10

enum flag_type {
	NONE = 0,
	MARK,
	CHILDCAS,
	RELOCATE
};

enum find_result_type {
	ABORT = 0,
	NOTFOUND_L,
	NOTFOUND_R,
	FOUND
};

enum operation_state {
	ONGOING = 0,
	SUCCESSFUL,
	FAILED
};

typedef struct Lock_Free_BST_Node {
	int volatile key;
	void * volatile op;
	struct Lock_Free_BST_Node * volatile left;
	struct Lock_Free_BST_Node *volatile right;
} LF_BST_Node;

typedef struct Child_Compare_And_Swap_Operation {
	bool is_left;
	LF_BST_Node * volatile expected;
	LF_BST_Node * volatile update;
} Child_CAS_OP;

typedef struct Relocate_Operation {
	int volatile state;
	LF_BST_Node * volatile dest;
	void *dest_op;
	int remove_key;
	int replace_key;
} Relocate_OP;

void *SET_FLAG(void *ptr, int state);
int GET_FLAG(void *ptr);
void *UNFLAG(void *ptr);
void *SET_NULL(void *ptr);
bool IS_NULL(void *ptr);
void test_ptr_functions();

//Main BST functions
void add(int key, int thread_num);
int find(int key, LF_BST_Node *&pred, void *&pred_op, LF_BST_Node *&curr, void *&curr_op, LF_BST_Node *auxRoot, int thread_num);
bool remove(int key, int thread_num);

//helper functions
void help(LF_BST_Node *pred, void *pred_op, LF_BST_Node *curr, void *curr_op, int thread_num);
void helpChildCAS(Child_CAS_OP *op, LF_BST_Node *dest, int thread_num);
void helpMarked(LF_BST_Node *pred, void *pred_op, LF_BST_Node *curr, int thread_num);
void helpRelocateMarked(Relocate_OP *op, LF_BST_Node *pred, void *pred_op, LF_BST_Node *curr, int thread_num);
bool helpRelocate(Relocate_OP *op, LF_BST_Node *pred, void *pred_op, LF_BST_Node *curr, int thread_num);

//other functions
LF_BST_Node *create_LF_node(int key);
void add_to_hp_list(int thread_num, LF_BST_Node *node);
#endif
