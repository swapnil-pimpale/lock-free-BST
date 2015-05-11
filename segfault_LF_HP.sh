#!/bin/sh

count=0

while [ $count -le 1000 ]; do
	#./test --create-file=$1 --test-file=$2 --lock-free
	./test --create-file=traces/empty.txt --test-file=traces/tracegen.insert.delete.1000 --lock-free --hazard-pointers
	count=$(expr $count + 1)
done
