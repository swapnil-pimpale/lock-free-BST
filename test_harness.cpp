#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <algorithm>

#include "Fine_Grained_BST.h"
#include "Lock_Free_BST.h"
#include "threads.h"
#include "work_queue.h"
#include "test_harness.h"
#include "cycle_timer.h"


extern int hp_off[MAX_THREADS];
pthread_mutex_t tree_lock;
FG_BST_Node *g_root = NULL;
LF_BST_Node *base_root = NULL;
bool hazard_pointers = false;
std::map<int, std::vector<FG_BST_Node *> > level_Map_FG; //this map is used purely for printing/debugging
std::map<int, std::vector<LF_BST_Node *> > level_Map_LF; //this map is used purely for printing/debugging
std::vector<int> tree_values_FG; //this vector is purely for debugging purposes
std::vector<int> tree_values_LF; //this vector is purely for debugging purposes

// this vector is used to determine algorithm correctness
std::vector<int> tree_values_correctness;
WorkQueue<WORK> *wq; 
bool all_threads_created = false;
bool perform_FG_test;
unsigned long perform_correctness = 0;
char create_file[PATH_MAX], test_file[PATH_MAX];

void print_FG_Tree(FG_BST_Node* root);
void add_FG_TreeToMap(FG_BST_Node* root, int level);
void print_LF_Tree(LF_BST_Node *root);
void add_LF_TreeToMap(LF_BST_Node *root, int level);
void check_valid_FG_Tree();
void check_valid_LF_Tree();
void populate_tree_values_FG(FG_BST_Node *root);
void populate_tree_values_LF(LF_BST_Node *root);
void runBasicTests();

static struct option long_options[] = 
{
	{"create-file", required_argument, 0, 'c'},
	{"test-file", required_argument, 0, 't'},
	{"lock-free", no_argument, 0, 'l'},
	{"correctness", required_argument, 0, 'o'},
	{"hazard-pointers", no_argument, 0, 'h'}
};

void *perform_ops_FG(void *thread_args)
{
	WORK work;
	int work_value;
	struct thread_info *tinfo = (struct thread_info *)thread_args;

	while (!all_threads_created);

	while (wq->get_queue_size() > 0) {
		work = wq->get_work();
		work_value = work.value;

		//printf("Got work (%d:%d)\n", work_value, work.op_type);
		if (work.op_type == INSERT) {
			insert(work_value, g_root, NULL, tinfo->thread_num);
		} else if (work.op_type == SEARCH) {
			search(work_value, g_root, NULL);
		} else if (work.op_type == DELETE) {
			remove(work_value, g_root, tinfo->thread_num);
		}
	}

	return 0;
}

void *perform_ops_LF(void *thread_args)
{
	WORK work;
	int work_value, result;
	LF_BST_Node *pred, *curr;
	void *pred_op, *curr_op;
	struct thread_info *tinfo = (struct thread_info *)thread_args;
	//struct timeval start_time, end_time;
	//long int time_diff;

	//printf("Lock_free tree: In thread with thread ID = %lu, thread number = %d\n", tinfo->thread_id,
	//       tinfo->thread_num);
	
	while (!all_threads_created);

	while (wq->get_queue_size() > 0) {
		work = wq->get_work();
		work_value = work.value;

		//printf("Got work (%d:%d)\n", work_value, work.op_type);
		if (work.op_type == INSERT) {
			add(work_value, tinfo->thread_num);
		} else if (work.op_type == SEARCH) {
			result = find(work_value, pred, pred_op, curr, curr_op, base_root, tinfo->thread_num);
			if (result == FOUND) {
				//printf("Found the node with value %d\n", work_value);
			} else if (result == ABORT) {
				//printf("Could not find the node with value %d\n", work_value);
				assert(0);
			}
		} else if (work.op_type == DELETE) {
			remove(work_value, tinfo->thread_num);
		}
	}

	return 0;
}

int init_harness(void)
{
	int thread_count = 0, ret;
	pthread_attr_t attr;
	struct thread_info *tinfo;

	if(perform_FG_test) {
	 	//Initialize the root mutex for fine-grained tree
		pthread_mutex_init(&tree_lock, NULL);
	}
	else {
		//Intialize auxiliary/base root for lock-free tree
		base_root = create_LF_node(-1);
	}

	/*
	 * Create the initial tree
	 */
	//printf("Attempting to create tree from create_file = %s\n", create_file);
	std::ifstream create_tree_file(create_file);
	std::string str;

	tree_values_correctness.clear();
	while (std::getline(create_tree_file, str)) {
		std::string value = str.substr(str.find(' '));
		int val = std::stoi(value);
		//printf("Inserting value %d into tree...\n", val);
		
		if(perform_FG_test) {
			//perform insertion into fine-grained tree
			insert(val, g_root, NULL, -1);
		}
		else {
			//perform insertion into lock-free tree
			add(val, 0);
		}

		/*
		 * If performing correctness test, also add this value to the vector
		 */
		if (perform_correctness == 1) {
			tree_values_correctness.push_back(val);
		}
	}
	create_tree_file.close();

	/*
	 * Read from the tracefile and fill-up the work queue
	 */
	wq = new WorkQueue<WORK>;
	if (wq == NULL) {
		printf("Cannot allocate memory\n");
		return -ENOMEM;
	}

	WORK w;
	//printf("Using the test file %s\n", test_file);
	std::ifstream tracefile(test_file);
	while (std::getline(tracefile, str)) {
		std::string operation = str.substr(0, str.find(' '));
		std::string value = str.substr(str.find(' '));
		int val = std::stoi(value);

		if (operation.compare("insert") == 0) {
			w.op_type = INSERT;
		} else if (operation.compare("search") == 0) {
			w.op_type = SEARCH;
		} else if (operation.compare("delete") == 0) {
			w.op_type = DELETE;
			/*
			 * If performing correctness test then remove the element from the vector too
			 */
			if (perform_correctness == 1) {
				tree_values_correctness.erase(std::find(tree_values_correctness.begin(),
									tree_values_correctness.end(),
									val));
			}
		}

		w.value = val;
		wq->put_work(w);
	}
	tracefile.close();


	/*
	 * Create and start the threads
	 */
	ret = pthread_attr_init(&attr);
	if (ret != 0) {
		printf("pthread_attr_init() failed\n");
		return -errno;
	}

	tinfo = (struct thread_info *)calloc(MAX_THREADS, sizeof(struct thread_info));
	if (tinfo == NULL) {
		printf("calloc failed\n");
		return -errno;
	}

	while (thread_count < MAX_THREADS) {
		tinfo[thread_count].thread_num = thread_count;
		hp_off[thread_count] = thread_count * NUM_HP_PER_THREAD;

		if(perform_FG_test) {
			ret = pthread_create(&tinfo[thread_count].thread_id, &attr, perform_ops_FG, &tinfo[thread_count]);
		}
		else {			
			ret = pthread_create(&tinfo[thread_count].thread_id, &attr, perform_ops_LF, &tinfo[thread_count]);
		}
		if (ret != 0) {
			printf("pthread_create failed\n");
			return -errno;
		}
		thread_count++;
	}
	all_threads_created = true;

	ret = pthread_attr_destroy(&attr);
	thread_count = 0;
	while (thread_count < MAX_THREADS) {
		ret = pthread_join(tinfo[thread_count].thread_id, NULL);
		if (ret != 0) {
			printf("pthread_join failed\n");
		}

		thread_count++;
	}

	free(tinfo);
	free(wq);

	if (perform_correctness != 0) {
		if(perform_FG_test) {
			//print_FG_Tree(g_root);
			check_valid_FG_Tree();
		}
		else {
			//print_LF_Tree(base_root);
			check_valid_LF_Tree();
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int idx = 0, c;

	if(argc < 3) {
		fprintf(stderr, "Usage: test --create-file=<tree_creation_file_name> --test-file=<trace_file_name> --lock-free --hazard-pointers\n");
		return -EINVAL;
	}

	/*
	 * Fine grained is true by default. Can be over-ridden by passing in a 
	 * command line parameter.
	 */
	perform_FG_test = true;
	while (true) {
		c = getopt_long(argc, argv, "c:t:l", long_options, &idx);

		if (-1 == c) {
			// End of options
			break;
		}

		switch (c) {
			case 'c':
				strcpy(create_file, optarg);
				break;

			case 't':
				strcpy(test_file, optarg);
				break;

			case 'l':
				perform_FG_test = false;
				break;

			case 'o':
				perform_correctness = strtoul(optarg, NULL, 10);
				break;

			case 'h':
				hazard_pointers = true;
				break;
		}
	}

	init_harness();
	//test_ptr_functions();

	return 0;
}	

void print_FG_Tree(FG_BST_Node* root)
{
	level_Map_FG.clear();
	add_FG_TreeToMap(root, 1);
	std::map<int, std::vector<FG_BST_Node *>>::iterator map_itr;

	for(map_itr = level_Map_FG.begin(); map_itr != level_Map_FG.end(); map_itr++) {

		int level = map_itr->first;
		std::vector<FG_BST_Node *> vec = map_itr->second;
		std::vector<FG_BST_Node *>::iterator vec_itr;
		printf("Level %d:", level);

		for(vec_itr = vec.begin(); vec_itr != vec.end(); vec_itr++) {
			FG_BST_Node *node = *vec_itr;
			if (node->parent != NULL)
				printf("Node:%d, Parent:%d, ", node->value, node->parent->value);
			else
				printf("Node:%d, Parent:NULL, ", node->value);

		}
		printf("\n");
	}
}

void add_FG_TreeToMap(FG_BST_Node* root, int level)
{
	if(root == NULL)
		return;

	std::map<int, std::vector<FG_BST_Node *>>::iterator map_itr;
	map_itr = level_Map_FG.find(level);

	if(map_itr == level_Map_FG.end()) { //not in map
		std::vector<FG_BST_Node *> vec;
		vec.push_back(root);
		level_Map_FG[level] = vec;
	}
	else {
		level_Map_FG[level].push_back(root);		
	}

	add_FG_TreeToMap(root->left, level+1);
	add_FG_TreeToMap(root->right, level+1);
	return;
}

void print_LF_Tree(LF_BST_Node* root)
{
	level_Map_LF.clear();
	add_LF_TreeToMap(root, 1);
	std::map<int, std::vector<LF_BST_Node *>>::iterator map_itr;

	for(map_itr = level_Map_LF.begin(); map_itr != level_Map_LF.end(); map_itr++) {

		int level = map_itr->first;
		std::vector<LF_BST_Node *> vec = map_itr->second;
		std::vector<LF_BST_Node *>::iterator vec_itr;
		printf("Level %d:", level);

		for(vec_itr = vec.begin(); vec_itr != vec.end(); vec_itr++) {
			LF_BST_Node *node = *vec_itr;
			printf("Node:%d, ", node->key);
		}
		printf("\n");
	}
}

void add_LF_TreeToMap(LF_BST_Node* root, int level)
{
	if(IS_NULL(root))
		return;

	std::map<int, std::vector<LF_BST_Node *>>::iterator map_itr;
	map_itr = level_Map_LF.find(level);

	if(map_itr == level_Map_LF.end()) { //not in map
		std::vector<LF_BST_Node *> vec;
		vec.push_back(root);
		level_Map_LF[level] = vec;
	}
	else {
		level_Map_LF[level].push_back(root);		
	}

	add_LF_TreeToMap(root->left, level+1);
	add_LF_TreeToMap(root->right, level+1);
	return;
}

void check_valid_FG_Tree()
{
	bool valid = true, incorrect = false;
	std::vector<int>::iterator it;
	int prev = INT_MIN;

	populate_tree_values_FG(g_root);

	if (perform_correctness == 1) {
		if (tree_values_correctness.size() != tree_values_FG.size()) {
			printf("All elements were not correctly deleted\n");
		} else {
			printf("Fine-grained tree is valid in terms of number of operations performed.\n");
		}
	}

	printf("Printing out fine-grained tree in-order: ");
	for(it = tree_values_FG.begin(); it != tree_values_FG.end(); it++) {
		
		int val = *it;
		printf("%d, ", val);
		
		if(val <= prev) {
			printf("\nTree is not in order!\n");
			valid = false;
			break;
		}
		
		prev = val;

		/*
		 * check if it is also present in the correctness vector
		 */
		if (perform_correctness == 1) {
			if (std::find(tree_values_correctness.begin(),
							 tree_values_correctness.end(),
							 val) == tree_values_correctness.end()) {
				printf("Value %d not present in the correctness vector\n", val);
				incorrect = true;;
			}
		}
	}

	if(valid) {
		printf("\nFine-grained tree is valid.\n");
	}
	else {
		printf("\nFine-grained tree is NOT VALID!!\n");
		assert(0);
	}

	if (incorrect) {
		printf("Fine-grained tree is incorrect!!\n");
		assert(0);
	} else {
		printf("Fine-grained tree is correct in terms of the remaining tree elements.\n");
	}
}

void populate_tree_values_FG(FG_BST_Node *root)
{
	if(root == NULL)
		return;

	populate_tree_values_FG(root->left);
	tree_values_FG.push_back(root->value);
	populate_tree_values_FG(root->right);
}

void check_valid_LF_Tree()
{
	bool valid = true;
	bool incorrect = false;
	std::vector<int>::iterator it;
	int prev = INT_MIN;

	populate_tree_values_LF(base_root);

	if (perform_correctness == 1) {
		if (tree_values_correctness.size() != tree_values_LF.size() - 1) {
			printf("All elements were not correctly deleted\n");
		} else {
			printf("Lock-free tree is valid in terms of number of operations performed.\n");
		}
	}

	printf("Printing out lock-free tree in-order: ");
	for(it = tree_values_LF.begin(); it != tree_values_LF.end(); it++) {
		
		int val = *it;
		printf("%d, ", val);
		
		if(val <= prev) {
			printf("\nTree is not in order!\n");
			valid = false;
			break;
		}
		
		prev = val;

		/*
		 * check if it is also present in the correctness vector
		 */
		if (perform_correctness == 1 && val != -1) {
			if (std::find(tree_values_correctness.begin(),
							 tree_values_correctness.end(),
							 val) == tree_values_correctness.end()) {
				printf("Value %d not present in the correctness vector\n", val);
				incorrect = true;;
			}
		}
	}

	if(valid) {
		printf("\nLock-free tree is valid.\n");
	}
	else {
		printf("\nLock-free tree is NOT VALID!!\n");
		assert(0);
	}

	if (incorrect) {
		printf("Lock-free tree is incorrect!!\n");
		assert(0);
	} else {
		printf("Lock-free tree is correct in terms of the remaining tree elements.\n");
	}
}

void populate_tree_values_LF(LF_BST_Node *root)
{
	int key;

	if(IS_NULL(root) || GET_FLAG(root->op) == MARK)
		return;

	populate_tree_values_LF(root->left);
	key = root->key;
	tree_values_LF.push_back(key);
	//printf("Ptr: %p, Left: %p, Val: %d, Right: %p, Flag: %d\n", root, root->left, root->key, root->right,
	//	GET_FLAG(root->op));
	populate_tree_values_LF(root->right);
}

void runBasicTests()
{
#if 0
	printTree(g_root);
	search(4, g_root);
	search(12, g_root);
	search(6, g_root);
	search(8, g_root);
	search(-110, g_root);
	remove(4, g_root);
#endif
	insert(21, g_root, NULL, -1);
	insert(39, g_root, NULL, -1);
	insert(5, g_root, NULL, -1);
	insert(2, g_root, NULL, -1);
}
