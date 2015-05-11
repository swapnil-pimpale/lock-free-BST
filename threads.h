#ifndef _THREADS_H_
#define _THREADS_H_

#define MAX_THREADS		24

struct thread_info {
	pthread_t	thread_id;
	int		thread_num;
};

#endif
