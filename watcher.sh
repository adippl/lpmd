#!/bin/sh
while true; do 
	inotifywait lpmd.c
	rm a.out
	clear
	ctags lpmd.c
	make buildDebug && ./lpmd 
	echo exit code $?
	done
