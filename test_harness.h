#ifndef _TEST_HARNESS_H
#define _TEST_HARNESS_H

enum operation_type {
	INSERT = 0,
	SEARCH,
	DELETE
};

typedef struct work {
	int value;
	int op_type;
} WORK;

#endif
