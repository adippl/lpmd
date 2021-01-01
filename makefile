CC = gcc 
CFLAGS = -Wall -pedantic -O2

lpmd: 
	$(CC) $(CFLAGS) lpmd.c -o lpmd
buildDebug: 
	$(CC) $(CFLAGS) -g -DDEBUG lpmd.c -o lpmd
clean:
	rm -f lpmd
install: lpmd
	su root -c 'cp lpmd /usr/local/bin/lpmd && cp lpmd-openrc /etc/init.d/lpmd && chmod +x /usr/local/bin/lpmd /etc/init.d/lpmd'

uninstall:
	su root -c 'rm /usr/local/bin/lpmd /etc/init.d/lmpd'


