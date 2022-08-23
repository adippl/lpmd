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

lpmd: lpmd.c
	$(CC) $(CFLAGS) lpmd.c -o lpmd
debug: lpmd.c
	$(CC) $(CFLAGS) -g -DDEBUG lpmd.c -o lpmd
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

