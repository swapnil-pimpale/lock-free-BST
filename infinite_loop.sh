#!/bin/sh

#./test create_tree_small.txt traces/insert.txt
./test create_tree_big.txt traces/search.txt

while [ $? -eq  0 ]; do
	./test create_tree_big.txt traces/delete2.txt
	#./test create_tree_big.txt traces/search.txt
done
