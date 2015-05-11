#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <getopt.h>
#include <linux/limits.h>

#define MIXED_WORKLOAD_STEP		500
int fd;

enum type {
	SEQUENTIAL = 1,
	LOW_CONTENTION,
	MIXED
};

static struct option long_options[] =
{
	{"insert", required_argument, 0, 'i'},
	{"delete", required_argument, 0, 'd'},
	{"search", required_argument, 0, 's'},
	{"name", required_argument, 0, 'n'},
	{"type", required_argument, 0, 't'}
};

void create_lc_trace(unsigned long elem, unsigned long range)
{
	char buf[64];

	if (range == 1)
		return;

	sprintf(buf, "insert %lu\n", elem - range / 2);

	if (write(fd, buf, strlen(buf)) < 0) {
		printf("write failed\n");
		close(fd);
		return;
	}

	sprintf(buf, "insert %lu\n", elem + range / 2);

	if (write(fd, buf, strlen(buf)) < 0) {
		printf("write failed\n");
		close(fd);
		return;
	}

	create_lc_trace(elem - range / 2, range / 2);
	create_lc_trace(elem + range / 2, range / 2);
}

int generate_trace_file(unsigned long num_inserts, unsigned long num_deletes, unsigned long num_searches,
			char *fname, int type)
{
	unsigned long count = 1, start, end;
	char buf[128];

	// create the trace file
	printf("fname = %s\n", fname);
	fd = creat(fname, 0644);
	if (fd < 0) {
		printf("Could not open the file\n");
	}

	if (type == SEQUENTIAL) {
		// Do all the inserts
		while (count <= num_inserts) {
			sprintf(buf, "insert %lu\n", count++);
			if (write(fd, buf, strlen(buf)) < 0) {
				printf("write failed\n");
				close(fd);
				return -1;
			}
		}

		count = 1;
		// Do all the searches
		while (count <= num_searches) {
			sprintf(buf, "search %lu\n", count++);
			if (write(fd, buf, strlen(buf)) < 0) {
				printf("write failed\n");
				close(fd);
				return -1;
			}
		}

		count = 1;
		// Do all the deletes
		while (count <= num_deletes) {
			sprintf(buf, "delete %lu\n", count++);
			if (write(fd, buf, strlen(buf)) < 0) {
				printf("write failed\n");
				close(fd);
				return -1;
			}
		}
	} else if (type == LOW_CONTENTION) {
		if (num_inserts != 0) {
			sprintf(buf, "insert %lu\n", num_inserts / 2);
			if (write(fd, buf, strlen(buf)) < 0) {
				printf("write failed\n");
				close(fd);
				return -1;
			}
			create_lc_trace(num_inserts / 2, num_inserts / 2);
		}

		if (num_searches != 0) {
			start = 1, end = num_searches - 1;
			while (start <= end) {
				sprintf(buf, "search %lu\n", start);
				if (write(fd, buf, strlen(buf)) < 0) {
					printf("write failed\n");
					close(fd);
					return -1;
				}

				if (start != end) {
					sprintf(buf, "search %lu\n", end);
					if (write(fd, buf, strlen(buf)) < 0) {
						printf("write failed\n");
						close(fd);
						return -1;
					}
				}

				start++;
				end--;
			}
		}

		if (num_deletes != 0) {
			start = 1, end = num_deletes - 1;
			while (start <= end) {
				sprintf(buf, "delete %lu\n", start);
				if (write(fd, buf, strlen(buf)) < 0) {
					printf("write failed\n");
					close(fd);
					return -1;
				}

				if (start != end) {
					sprintf(buf, "delete %lu\n", end);
					if (write(fd, buf, strlen(buf)) < 0) {
						printf("write failed\n");
						close(fd);
						return -1;
					}
				}

				start++;
				end--;
			}
		}
	} else if (type == MIXED) {
		unsigned long completed = 0;

		while (completed < num_inserts) {
			unsigned long counter = 1;
			while (counter <= MIXED_WORKLOAD_STEP) {
				sprintf(buf, "insert %lu\n", completed + counter++);
				if (write(fd, buf, strlen(buf)) < 0) {
					printf("write failed\n");
					close(fd);
					return -1;
				}
			}

			counter = 1;
			while (counter <= MIXED_WORKLOAD_STEP) {
				sprintf(buf, "search %lu\n", completed + counter++);
				if (write(fd, buf, strlen(buf)) < 0) {
					printf("write failed\n");
					close(fd);
					return -1;
				}
			}

			counter = 1;
			while (counter <= MIXED_WORKLOAD_STEP) {
				sprintf(buf, "delete %lu\n", completed + counter++);
				if (write(fd, buf, strlen(buf)) < 0) {
					printf("write failed\n");
					close(fd);
					return -1;
				}
			}

			completed += MIXED_WORKLOAD_STEP;
		}
	}

	close(fd);

	return 0;
}

int main (int argc, char **argv)
{
	int idx = 0, c;
	unsigned long num_inserts = 0, num_deletes = 0, num_searches = 0, type = SEQUENTIAL;
	char fname[PATH_MAX];

	if (argc != 4 && argc != 6) {
		printf("Usage: ./tracegen --insert=x --delete=y --search=z --name=n --type=t\n");
		return -1;
	}

	while (true) {
		c = getopt_long(argc, argv, "i:d:s:t:", long_options, &idx);

		if (-1 == c) {
			// End of options
			break;
		}

		switch (c) {
			case 'i':
				num_inserts = strtoul(optarg, NULL, 10);
				break;

			case 'd':
				num_deletes = strtoul(optarg, NULL, 10);
				break;

			case 's':
				num_searches = strtoul(optarg, NULL, 10);
				break;

			case 'n':
				strcpy(fname, optarg);
				break;

			case 't':
				type = atoi(optarg);
				break;
		}
	}

	generate_trace_file(num_inserts, num_deletes, num_searches, fname, type);

	return 0;
}
