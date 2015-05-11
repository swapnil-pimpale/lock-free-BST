// Copyright 2013 15418 Course Staff.

#ifndef __WORKER_WORK_QUEUE_H__
#define __WORKER_WORK_QUEUE_H__

#include <vector>

template <class T>
class WorkQueue {
	private:
	std::vector<T> storage;
	pthread_mutex_t queue_lock;
	pthread_cond_t queue_cond;
	int curr_q_size;

	public:

	WorkQueue() {
		pthread_cond_init(&queue_cond, NULL);
		pthread_mutex_init(&queue_lock, NULL);
		curr_q_size = 0;
	}

	T get_work() {
		pthread_mutex_lock(&queue_lock);

		T item = storage.front();
		//storage.pop_front();
		storage.erase(storage.begin());

		pthread_mutex_unlock(&queue_lock);
		return item;
	}

	void put_work(const T& item) {
		pthread_mutex_lock(&queue_lock);
		storage.push_back(item);
		curr_q_size++;
		pthread_mutex_unlock(&queue_lock);
		pthread_cond_signal(&queue_cond);
	}

	void print_queue_size()
	{
		printf("QueueSize: %d\n", storage.size());
	}

	int get_queue_size()
	{
		pthread_mutex_lock(&queue_lock);
		int size = curr_q_size;
		curr_q_size--;
		pthread_mutex_unlock(&queue_lock);
		return size;
	}
};

#endif  // WORKER_WORK_QUEUE_H_
