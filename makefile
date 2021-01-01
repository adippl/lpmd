CC = gcc 
CFLAGS = -Wall -pedantic -O2

lpmd: 
	$(CC) $(CFLAGS) lpmd.c -o lpmd
debug: 
	$(CC) $(CFLAGS) -g -DDEBUG lpmd.c -o lpmd
clean:
	rm -f lpmd
install: lpmd
	su root -c 'cp lpmd /usr/bin/lpmd && cp lpmd-openrc /etc/init.d/lpmd && chmod +x /usr/bin/lpmd /etc/init.d/lpmd'
uninstall:
	su root -c 'rm -f /usr/bin/lpmd /etc/init.d/lmpd'


