#!/bin/sh

count=0

while [ $count -le 1000 ]; do
	./test --create-file=$1 --test-file=$2
	count=$(expr $count + 1)
done
