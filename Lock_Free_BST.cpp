#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <map>
#include <algorithm>
#include <atomic>
#include <set>

#include "Lock_Free_BST.h"
#include "threads.h"

std::array<std::atomic<LF_BST_Node *>, MAX_THREADS * NUM_HP_PER_THREAD> hp;
std::vector<LF_BST_Node *> rlist[MAX_THREADS];
int hp_off[MAX_THREADS];

/*
 * The base_root is also called the auxRoot in find().
 * The base root is not the real root. The real root is the right
 * child of the base_root. This is done for convenience so that we have
 * a predecessor for the real root as well.
 */
extern LF_BST_Node *base_root;
extern bool hazard_pointers;

void *SET_FLAG(void *ptr, int state)
{
	ptr = (void *) ((uintptr_t)ptr | (uintptr_t)state);
	return ptr;
}

int GET_FLAG(void *ptr)
{
	int flag = ((uintptr_t) ptr & (uintptr_t)THREE);
	return flag;
}

void *UNFLAG(void *ptr)
{
	ptr = (void *)((uintptr_t)ptr & ~(uintptr_t)THREE);
	return ptr;
}

void *SET_NULL(void *ptr)
{
	ptr = (void *) ((uintptr_t)ptr | (uintptr_t)ONE);
	return ptr;
}

bool IS_NULL(void *ptr)
{
	int val = ((uintptr_t)ptr & (uintptr_t)ONE);
	if(val == 1) {
		return true;
	}
	return false;
}

void add(int key, int thread_num)
{
	LF_BST_Node *pred, *curr, *newNode;
	void *pred_op, *curr_op;
	Child_CAS_OP *cas_op;
	int result;

	while (true) {
		/*
		 * Do a find first. If the value already exists return without doing anything.
		 * We don't allow duplicates
		 */
		result = find(key, pred, pred_op, curr, curr_op, base_root, thread_num);
		if (hazard_pointers) {
			add_to_hp_list(thread_num, curr);
		}

		if(result == FOUND) {
			printf("Value %d already exists in the tree\n", key);
			return;
		}

		// create a new node
		newNode = create_LF_node(key);
		if (hazard_pointers) {
			add_to_hp_list(thread_num, newNode);
		}

		// check if this will be the left node of the node gaining the child
		bool is_left = (result == NOTFOUND_L);
		if (hazard_pointers) {
			if (is_left) {
				add_to_hp_list(thread_num, curr->left);
			} else {
				add_to_hp_list(thread_num, curr->right);
			}
		}

		LF_BST_Node *old = is_left ? curr->left : curr->right;

		/*
		 * Create a new Child CAS operation
		 */
		cas_op = new Child_CAS_OP;
		cas_op->is_left = is_left;
		cas_op->expected = old;
		cas_op->update = newNode;

		/*
		 * Atomically store the newly created Child CAS operation in curr's op
		 */
		if(__sync_bool_compare_and_swap(&curr->op, curr_op, SET_FLAG((void *) cas_op, CHILDCAS))) {
			/*
			 *  if CAS on the op succeeded perform the actual operation.
			 *  In this case helpChildCAS() will replace the left or right 
			 *  child of curr (old) with the update (newNode)
			 */
			helpChildCAS(cas_op, curr, thread_num);
			return;
		} else {
			delete newNode;
			delete cas_op;
		}
	}
}

int find(int key, LF_BST_Node *&pred, void *&pred_op, LF_BST_Node *&curr, void *&curr_op, LF_BST_Node *auxRoot, int thread_num)
{
	int result, curr_key;
	LF_BST_Node *next, *last_right;
	void *last_right_op;

retry:

	// Start find from the auxRoot
	result = NOTFOUND_R;
	if (hazard_pointers) {
		add_to_hp_list(thread_num, auxRoot);
	}
	curr = auxRoot;
	curr_op = curr->op;

	/*
	 * This is a special case where some thread is trying to add to an empty tree
	 * or remove the logical root. In both these cases, the auxRoot's op won't be
	 * NONE
	 */
	if(GET_FLAG(curr_op) != NONE) {
		if(auxRoot == base_root) {
			// help the ongoing operation at auxRoot and retry find
			helpChildCAS(((Child_CAS_OP *)UNFLAG(curr_op)), curr, thread_num);
			goto retry;
		}
		else {
			return ABORT;
		}
	}

	/*
	 * next = next node along the search path after curr
	 * last_right = last node for which the right child path was taken
	 */

	if (hazard_pointers) {
		add_to_hp_list(thread_num, curr->right);
	}
	next = curr->right;
	last_right = curr;
	last_right_op = curr_op;

	while(!IS_NULL(next) && next != NULL) {
		pred = curr;
		pred_op = curr_op;
		curr = next;
		curr_op = curr->op;
		
		if(GET_FLAG(curr_op) != NONE) {
			/*
			 * If we detect on our way that an operation is ongoing on a node,
			 * we help that node complete its operation and retry find().
			 *
			 * This call to help() can also help ensure removal of a MARKED node
			 * for which CAS failed in helpMarked()
			 */
			help(pred, pred_op, curr, curr_op, thread_num);
			goto retry;
		}
		
		curr_key = curr->key;

		if(key < curr_key) {
			result = NOTFOUND_L;
			if (hazard_pointers) {
				add_to_hp_list(thread_num, curr->left);
			}
			next = curr->left;
		}
		else if(key > curr_key) {
			result = NOTFOUND_R;
			if (hazard_pointers) {
				add_to_hp_list(thread_num, curr->right);
			}
			next = curr->right;
			last_right = curr;
			last_right_op = curr_op;
		}
		else {
			result = FOUND;
			break;
		}
	}

	/*
	 * If we didn't find the key, verify that having searched last_right's
	 * right subtree, the key could not have been added to last_right's left
	 * subtree.
	 * This can happen if there was a concurrent delete operation which
	 * replaced the key at last_right (thus increasing the search range of the 
	 * left subtree) followed by an insert of key.
	 * If so, retry the find() from the start.
	 */
	if( (result != FOUND) && (last_right_op != last_right->op) ) {
		goto retry;
	}

	/*
	 * If curr's op changed after we read its key, retry the find()
	 */
	if(curr_op != curr->op) {
		goto retry;
	}
	return result;
}

bool remove(int key, int thread_num)
{
	LF_BST_Node *pred, *curr, *replace;
	void *pred_op, *curr_op, *replace_op; 
	Relocate_OP *reloc_op;

	while(true) {
		
		/*
		 * find the key to be deleted.
		 * find will return curr = the node to be deleted, pred = its predecessor,
		 * and their corresponding op's
		 */
		if(find(key, pred, pred_op, curr, curr_op, base_root, thread_num) != FOUND) {
			return false;
		}

		if (!IS_NULL(curr->right) && hazard_pointers) {
			add_to_hp_list(thread_num, curr->right);
		}

		if (!IS_NULL(curr->left) && hazard_pointers) {
			add_to_hp_list(thread_num, curr->left);
		}

		/*
		 * if the node to be deleted has only one child.
		 * Change curr's op from NONE to MARK. At this point the node is
		 * logically deleted from the tree.
		 */
		if( IS_NULL(curr->right) || IS_NULL(curr->left) ) {
			//Node has less than 2 children
			if(__sync_bool_compare_and_swap(&curr->op, curr_op, SET_FLAG(curr_op, MARK))) {
				helpMarked(pred, pred_op, curr, thread_num);
				return true;
			}
		}
		else {
			//Node has 2 children
			/*
			 * Locate the node with the next largest key.
			 * Search for the same key in curr's right subtree by doing a find() on curr.
			 * This will either return NOTFOUND_L or NOTFOUND_R. If it returns FOUND then
			 * there are duplicates. Good place for an assert?
			 *
			 * replace = the node with the next largest key
			 * pred = replace's predecessor
			 */
			if( (find(key, pred, pred_op, replace, replace_op, curr, thread_num) == ABORT) || (curr->op != curr_op) ) {
				continue;
			}

			if (hazard_pointers) {
				add_to_hp_list(thread_num, pred);
				add_to_hp_list(thread_num, curr);
				add_to_hp_list(thread_num, replace);
			}

			/*
			 * Create a new Relocate_OP.
			 * To start with the state of the operation will be ONGOING
			 * reloc_op's dest = curr.
			 * curr is the node we want to remove
			 */
			reloc_op = new Relocate_OP;
			reloc_op->state = ONGOING;
			reloc_op->dest = curr;
			reloc_op->dest_op = curr_op;
			reloc_op->remove_key = key;
			reloc_op->replace_key = replace->key;

			/*
			 * Atomically try to insert this newly created operation in replace's op field
			 * to ensure that replace's key cannot be removed while this remove is in progress
			 */
			if(__sync_bool_compare_and_swap(&replace->op, replace_op, SET_FLAG((void *) reloc_op, RELOCATE))) {
				if(helpRelocate(reloc_op, pred, pred_op, replace, thread_num)) {
					return true;
				} else {
					delete reloc_op;
				}
			} else {
				delete reloc_op;
			}
		}
	}

	
}

void help(LF_BST_Node *pred, void *pred_op, LF_BST_Node *curr, void *curr_op, int thread_num)
{

	if(GET_FLAG(curr_op) == CHILDCAS) {
		helpChildCAS( ( (Child_CAS_OP *) UNFLAG(curr_op) ), curr, thread_num);
	}
	else if(GET_FLAG(curr_op) == RELOCATE) {
		helpRelocate( (Relocate_OP *) UNFLAG(curr_op), pred, pred_op, curr, thread_num);
	}
	else if(GET_FLAG(curr_op) == MARK) {
		helpMarked(pred, pred_op, curr, thread_num);
	}
}

void add_to_hp_list(int thread_num, LF_BST_Node *node)
{
	hp[hp_off[thread_num]] = node;

	hp_off[thread_num]++;
	if (hp_off[thread_num] == thread_num * NUM_HP_PER_THREAD + NUM_HP_PER_THREAD) {
		hp_off[thread_num] = thread_num * NUM_HP_PER_THREAD;
	}
}

/**
 * helpChildCAS:
 *
 * Determine whether the node to be added is the left or right child of dest.
 * Atomically update the pointer
 * Atomically set the operation from CHILDCAS to NONE
 */
void helpChildCAS(Child_CAS_OP *op, LF_BST_Node *dest, int thread_num)
{
	if (op->is_left) {
		add_to_hp_list(thread_num, dest->left);
	} else {
		add_to_hp_list(thread_num, dest->right);
	}

	LF_BST_Node **address = op->is_left ? (LF_BST_Node **)&dest->left : (LF_BST_Node **)&dest->right;
	if (__sync_bool_compare_and_swap(address, op->expected, op->update) && hazard_pointers) {

		if (UNFLAG(op->expected) != NULL) {
			std::vector<LF_BST_Node *>::iterator rlist_vec_itr;

			if (std::find(rlist[thread_num].begin(), rlist[thread_num].end(),
			    (LF_BST_Node *)UNFLAG(op->expected)) == rlist[thread_num].end()) {
				rlist[thread_num].push_back((LF_BST_Node *)UNFLAG(op->expected));
			}
		}

		if (rlist[thread_num].size() > HP_THRESHOLD) {
			std::vector<LF_BST_Node *>::iterator vec_itr;
			std::vector<LF_BST_Node *> temp_rlist = rlist[thread_num];
			for (vec_itr = temp_rlist.begin(); vec_itr != temp_rlist.end(); vec_itr++) {
				LF_BST_Node *retired_node = *vec_itr;
				bool ok_to_free = true;

				for (int i = 0; i < MAX_THREADS * NUM_HP_PER_THREAD; i++) {
					if (retired_node == UNFLAG(hp[i])) {
						// Somebody has a reference to this retired node
						// Do not delete
						ok_to_free = false;
						break;
					}
				}

				if (ok_to_free) {
					delete retired_node;
					rlist[thread_num].erase(std::find(rlist[thread_num].begin(),
									  rlist[thread_num].end(),
									  retired_node));
				}
			}
		}
	}

	__sync_bool_compare_and_swap(&dest->op, SET_FLAG(op, CHILDCAS), SET_FLAG(op, NONE));
}

void helpMarked(LF_BST_Node *pred, void *pred_op, LF_BST_Node *curr, int thread_num)
{
	LF_BST_Node *new_ref;
	Child_CAS_OP *cas_op;
	//int key;

	//key = curr->key;

	if(IS_NULL(curr->left)) {
		
		if(IS_NULL(curr->right)) {
			new_ref = (LF_BST_Node *) SET_NULL((void *) curr);
		}
		else {
			if (hazard_pointers) {
				add_to_hp_list(thread_num, curr->right);
			}
			new_ref = curr->right;
		}
	}
	else {
		if (hazard_pointers) {
			add_to_hp_list(thread_num, curr->left);
		}
		new_ref = curr->left;
	}


	cas_op = new Child_CAS_OP;
	cas_op->is_left = (curr == pred->left);
	cas_op->expected = curr;
	cas_op->update = new_ref;

	if(__sync_bool_compare_and_swap(&pred->op, pred_op, SET_FLAG((void *) cas_op, CHILDCAS))) {
		helpChildCAS(cas_op, pred, thread_num);
	} else {
		delete cas_op;
#if 0
		/*
		 * pred_op may have changed since it was read so removing the marked node may fail.
		 * So we do a find(key) again to ensure that the marked node is removed before we 
		 * return from this function
		 */

		if (find(key, pred, pred_op, curr, curr_op, base_root, thread_num) == ABORT) {
			printf("find returned ABORT\n");
			assert(0);
		}
#endif
	}

}

bool helpRelocate(Relocate_OP *op, LF_BST_Node *pred, void *pred_op, LF_BST_Node *curr, int thread_num)
{
	int seen_state = op->state;
	
	if (hazard_pointers) {
		add_to_hp_list(thread_num, op->dest);
	}

	if(seen_state == ONGOING) {
		
		/*
		 * op->dest is the node with the key that needs to be deleted.
		 * Try to insert the Relocate_OP into op-dest's op field.
		 */
		void *seen_op = __sync_val_compare_and_swap(&op->dest->op, op->dest_op, SET_FLAG((void *) op, RELOCATE));

		/*
		 * if the above CAS succeeded or if someone else had already inserted the Relocate_OP
		 * then change the state from ONGOING to SUCCESSFUL
		 *
		 * At this point, the relocate operation cannot fail and the key can be considered 
		 * to be logically removed from the set.
		 */
		if( (seen_op == op->dest_op) || (seen_op == SET_FLAG((void *) op, RELOCATE)) ) {
			__sync_bool_compare_and_swap(&op->state, ONGOING, SUCCESSFUL);
			seen_state = SUCCESSFUL;
		}
		else {
			seen_state = __sync_val_compare_and_swap(&op->state, ONGOING, FAILED);
		}

	}
	
	/*
	 * If successful replace op->dest's (node to be deleted) key and
	 * reset the op field to NONE
	 */
	if(seen_state == SUCCESSFUL) {
		__sync_bool_compare_and_swap(&op->dest->key, op->remove_key, op->replace_key);
		__sync_bool_compare_and_swap(&op->dest->op, SET_FLAG((void *) op, RELOCATE), SET_FLAG((void *) op, NONE));
	}
	
	bool result = (seen_state == SUCCESSFUL);

	if(op->dest == curr) {
		return result;
	}

	/*
	 * We now want to remove replace.
	 * So if the result was 1 mark the replace node
	 * curr = replace -> Check the call to helpRelocate()
	 */
	__sync_bool_compare_and_swap(&curr->op, SET_FLAG((void *) op, RELOCATE), SET_FLAG((void *) op, result ? MARK : NONE));

	if(result) {
		if(op->dest == pred) {
			pred_op = SET_FLAG((void *) op, NONE);
		}

		// remove curr (replace) node
		helpMarked(pred, pred_op, curr, thread_num);
	}

	return result;
}

LF_BST_Node *create_LF_node(int key)
{
	LF_BST_Node *newNode = new LF_BST_Node;
	newNode->key = key;
	newNode->op = NULL;
	newNode->left = (LF_BST_Node *) SET_NULL(NULL);
	newNode->right = (LF_BST_Node *) SET_NULL(NULL);
	return newNode;

}
