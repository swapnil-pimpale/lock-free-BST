#!/bin/sh

while true
do
	cat /proc/meminfo | grep -i memfree > memfree_output
	sleep 1
done
