CC=g++
CFLAGS=-std=c++11 -Wall -Werror -g
EXECUTABLE=test
SOURCES=test_harness.cpp Fine_Grained_BST_Lock.cpp  
LDFLAGS=-lpthread

test: test_harness.o Fine_Grained_BST_Lock.o Lock_Free_BST.o
	$(CC) $(CFLAGS) -o test test_harness.o Fine_Grained_BST_Lock.o Lock_Free_BST.o $(LDFLAGS) 

test_harness.o: test_harness.cpp Fine_Grained_BST.h threads.h work_queue.h
	$(CC) $(CFLAGS) -c test_harness.cpp

Fine_Grained_BST_Lock.o: Fine_Grained_BST_Lock.cpp Fine_Grained_BST.h threads.h
	$(CC) $(CFLAGS) -c Fine_Grained_BST_Lock.cpp

Lock_Free_BST.o: Lock_Free_BST.cpp Lock_Free_BST.h	
	$(CC) $(CFLAGS) -c Lock_Free_BST.cpp

tracegen: tracegen.o
	$(CC) $(CFLAGS) -o tracegen tracegen.o

tracegen.o: tracegen.cpp
	$(CC) $(CFLAGS) -c tracegen.cpp
clean:
	rm *.o test *~
