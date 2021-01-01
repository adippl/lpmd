build: 
	gcc -Wall -pedantic -O2 lpmd.c -o lpmd
buildDebug: 
	gcc -Wall -pedantic -g -DDEBUG -O2 lpmd.c -o lpmd
clean:
	rm -f lpmd
