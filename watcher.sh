#!/bin/sh
while true; do 
	inotifywait *.c *.h
	sleep 0.1
	clear
	ctags *.c
	make debug 
	echo exit code $?
	done
