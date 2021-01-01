while true; do 
	inotifywait lpm.c
	rm a.out
	clear
	gcc -Wall -pedantic -g -DDEBUG lpm.c && ./a.out 
	echo exit code $?
	done
