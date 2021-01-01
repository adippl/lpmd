CC = gcc 
CFLAGS = -Wall -pedantic -O2

lpmd: 
	$(CC) $(CFLAGS) lpmd.c -o lpmd
debug: 
	$(CC) $(CFLAGS) -g -DDEBUG lpmd.c -o lpmd
clean:
	rm -f lpmd
install: lpmd
	cp lpmd /usr/local/bin/lpmd 
	cp lpmd-openrc /etc/init.d/lpmd 
	chmod +x /usr/local/bin/lpmd /etc/init.d/lpmd'
uninstall:
	rm -f /usr/local/bin/lpmd /etc/init.d/lmpd'


