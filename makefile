ifeq ($(DESTDIR),)
 DESTDIR := 
 PREFIX := /usr/local
else
 ifeq ($(PREFIX),)
  PREFIX := /usr
 endif
endif

ifeq ($(CC),)
 CC := cc 
endif
ifeq ($(CFLAGS),)
 CFLAGS := -Wall -Wextra -pedantic -O2 -g 
endif

lpmd: lpmd.c lpmctl.c error.c
	$(CC) $(CFLAGS) -c error.c
	$(CC) $(CFLAGS) lpmd.c error.o -o lpmd
	$(CC) $(CFLAGS) lpmctl.c error.o -o lpmctl
debug: lpmd.c lpmctl.c error.c
	$(CC) $(CFLAGS) -g -DDEBUG -c error.c
	$(CC) $(CFLAGS) -g -DDEBUG lpmd.c error.o -o lpmd
	$(CC) $(CFLAGS) -g -DDEBUG lpmctl.c error.o -o lpmctl
clean:
	rm -f lpmd lpmd-profiler gmon.out
install: lpmd
	install -D -m 755 lpmd ${DESTDIR}${PREFIX}/bin/lpmd
	install -D -m 755 lpmd-openrc ${DESTDIR}/etc/init.d/lpmd 
uninstall:
	rm -f $(DESTDIR)${PREFIX}/bin/lpmd $(DESTDIR)/etc/init.d/lmpd

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

