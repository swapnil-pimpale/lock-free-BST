#!/bin/sh

count=0

while [ $count -le 1000 ]; do
	./test --create-file=$1 --test-file=$2 --lock-free
	count=$(expr $count + 1)
done
