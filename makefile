ifeq ($(DESTDIR),)
    DESTDIR := /
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
	cp lpmd ${DESTDIR}/bin/lpmd 
	cp lpmd-openrc ${DESTDIR}/etc/init.d/lpmd 
	chmod +x ${DESTDIR}/bin/lpmd ${DESTDIR}/etc/init.d/lpmd
uninstall:
	rm -f $(PREFIX)/bin/lpmd /etc/init.d/lmpd

su-install: lpmd
	su root -c 'make install'
su-uninstall:
	su root -c 'make uninstall'

compile_profiler: 
	$(CC) $(CFLAGS) lpmd.c -o lpmd_profiler -DDEBUG -pg

run-profiler: compile_profiler
	./lpmd_profiler

profiler: run-profiler
	gprof lpmd_profiler gmon.out

