ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif
ifeq ($(D),)
    D := /
endif

CC = gcc 
CFLAGS = -Wall -pedantic -O2

lpmd: 
	$(CC) $(CFLAGS) lpmd.c -o lpmd
debug: 
	$(CC) $(CFLAGS) -g -DDEBUG lpmd.c -o lpmd
clean:
	rm -f lpmd lpmd-profiler gmon.out
install: lpmd
	cp lpmd $(PREFIX)/bin/lpmd 
	cp lpmd-openrc /etc/init.d/lpmd 
	chmod +x /usr/bin/lpmd /etc/init.d/lpmd
uninstall:
	rm -f $(PREFIX)/bin/lpmd /etc/init.d/lmpd

su-install: lpmd
	su root -c 'cp lpmd /usr/bin/lpmd && cp lpmd-openrc /etc/init.d/lpmd && chmod +x /usr/bin/lpmd /etc/init.d/lpmd'
su-uninstall:
	su root -c 'rm -f /usr/bin/lpmd /etc/init.d/lmpd'


compile_profiler: 
	$(CC) $(CFLAGS) lpmd.c -o lpmd_profiler -DDEBUG -pg

run-profiler: compile_profiler
	./lpmd_profiler

profiler: run-profiler
	gprof lpmd_profiler gmon.out

