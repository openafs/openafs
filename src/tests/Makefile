# Generated automatically from Makefile.in by configure.
srcdir		= .
include /usr/src/openafs/openafs/src/config/Makefile.config

SHELL		= /bin/sh

CFLAGS		= -I. -I${srcdir} ${DBG} ${OPTMZ} -I${TOP_OBJDIR}/src/config -I${TOP_INCDIR} ${XCFLAGS}
LDFLAGS=${DBG} ${OPTMZ} ${XLDFLAGS}

SYS_LIBS	= ${TOP_LIBDIR}/libsys.a ${TOP_LIBDIR}/librx.a ${TOP_LIBDIR}/liblwp.a ${TOP_LIBDIR}/util.a

AUTH_LIBS	= ${TOP_LIBDIR}/libauth.a ${SYS_LIBS}

INT_LIBS	= ${TOP_LIBDIR}/libafsint.a ${TOP_LIBDIR}/libsys.a ${TOP_LIBDIR}/librxkad.a ${TOP_LIBDIR}/librx.a ${TOP_LIBDIR}/liblwp.a ${TOP_LIBDIR}/libcom_err.a ${TOP_LIBDIR}/util.a 

TEST_PROGRAMS = write-ro-file hello-world read-vs-mmap read-vs-mmap2	 \
		mmap-and-read large-dir large-dir2 large-dir3 mountpoint \
		test-setgroups test-setpag hardlink1 hardlink2 mkdir2	 \
		create-files create-symlinks create-dirs dup2-and-unlog	 \
		readdir-vs-lstat read-write create-remove mkdir3         \
		symlink echo-n test-parallel1 test-parallel2 create-stat \
		kill-softly kill-softer rm-rf exit-wo-close	         \
		mmap-vs-read mmap-vs-read2 strange-characters-c pine	 \
		append-over-page write-ucc utime-dir mmap-shared-write	 \
		rename5 rename-under-feet write-closed write-closed2	 \
		truncate fchmod make-page utime-file rename6 	         \
		write3 still-there-p write-large afscp hardlink4         \
		intr-read asu truncate-files mmap-cat blocks-new-file 	 \
		fsx write-rand

TEST_OBJS     = write-ro-file.o read-vs-mmap.o read-vs-mmap2.o		   \
		mmap-and-read.o large-dir.o large-dir2.o large-dir3.o	   \
		test-setgroups.o test-setpag.o hardlink1.o hardlink2.o	   \
		mkdir2.o create-files.o create-symlinks.o create-dirs.o	   \
		dup2-and-unlog.o readdir-vs-lstat.o read-write.o           \
		create-remove.o symlink.o echo-n.o test-parallel1.o	   \
		test-parallel1.o mkdir3.o rename6.o			   \
		create-stat.o kill-softly.o kill-softer.o rm-rf.o          \
		exit-wo-close.o mmap-vs-read.o mmap-vs-read2.o		   \
		strange-characters-c.o pine.o append-over-page.o	   \
		write-ucc.o utime-dir.o mmap-shared-write.o rename5.o	   \
		rename-under-feet.o write-closed.o write-closed2.o	   \
		truncate.o fchmod.o make-page.o utime-file.o	           \
		write3.o still-there-p.o write-large.o hardlink4.o         \
		intr-read.o asu.o truncate-files.o mmap-cat.o		   \
		blocks-new-file.o fsx.o afscp.o afscp_callback.o	   \
		write-rand.o

TEST_SRCS     = write-ro-file.c read-vs-mmap.c read-vs-mmap2.c		   \
		mmap-and-read.c large-dir.c large-dir2.c large-dir3.c	   \
		test-setgroups.c test-setpag.c hardlink1.c hardlink2.c	   \
		mkdir2.c create-files.c create-symlinks.c create-dirs.c	   \
		dup2-and-unlog.c readdir-vs-lstat.c read-write.c           \
		create-remove.c symlink.c echo-n.c test-parallel1.c	   \
		test-parallel2.c mkdir3.c rename6.c			   \
		create-stat.c kill-softly.c kill-softer.c rm-rf.c          \
		exit-wo-close.c mmap-vs-read.c mmap-vs-read2.c		   \
		strange-characters-c.c pine.c append-over-page.c	   \
		write-ucc.c utime-dir.c mmap-shared-write.c rename5.c	   \
		rename-under-feet.c write-closed.c write-closed2.c	   \
		truncate.c fchmod.c make-page.c utime-file.c	           \
		write3.c still-there-p.c write-large.c hardlink4.c         \
		intr-read.c asu.c truncate-files.c mmap-cat.c		   \
		blocks-new-file.c fsx.c afscp.c afscp_callback.c	   \
		write-rand.c

EXTRA_OBJS = err.o errx.o warn.o warnx.o

all: run-tests $(TEST_PROGRAMS) OS.pm

OS.pm: OS-$(MKAFS_OSTYPE).pm
	$(CP) OS-$(MKAFS_OSTYPE).pm OS.pm

write-rand: write-rand.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ write-rand.o $(EXTRA_OBJS) $(LIBS)

write-ro-file: write-ro-file.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ write-ro-file.o $(EXTRA_OBJS) $(LIBS)

write-large: write-large.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ write-large.o $(EXTRA_OBJS) $(LIBS)

read-vs-mmap: read-vs-mmap.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ read-vs-mmap.o $(EXTRA_OBJS) $(LIBS)

read-vs-mmap2: read-vs-mmap2.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ read-vs-mmap2.o $(EXTRA_OBJS) $(LIBS)

mmap-vs-read: mmap-vs-read.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ mmap-vs-read.o $(EXTRA_OBJS) $(LIBS)

mmap-vs-read2: mmap-vs-read2.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ mmap-vs-read2.o $(EXTRA_OBJS) $(LIBS)

read-write: read-write.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ read-write.o $(EXTRA_OBJS) $(LIBS)

mmap-and-read: mmap-and-read.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ mmap-and-read.o $(EXTRA_OBJS) $(LIBS)

large-dir: large-dir.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ large-dir.o $(EXTRA_OBJS) $(LIBS)

large-dir2: large-dir2.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ large-dir2.o $(EXTRA_OBJS) $(LIBS)

large-dir3: large-dir3.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ large-dir3.o $(EXTRA_OBJS) $(LIBS)

fchmod: fchmod.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ fchmod.o $(EXTRA_OBJS) $(LIBS)

truncate: truncate.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ truncate.o $(EXTRA_OBJS) $(LIBS)

make-page: make-page.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ make-page.o $(EXTRA_OBJS) $(LIBS)

still-there-p: still-there-p.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ still-there-p.o $(EXTRA_OBJS) $(LIBS)

intr-read: intr-read.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ intr-read.o $(EXTRA_OBJS) $(LIBS)

blocks-new-file: blocks-new-file.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ blocks-new-file.o $(EXTRA_OBJS) $(LIBS)

asu: asu.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ asu.o $(EXTRA_OBJS) $(LIBS)

test-setgroups: test-setgroups.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ test-setgroups.o $(EXTRA_OBJS) $(SYS_LIBS) $(LIBS)

test-setpag: test-setpag.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ test-setpag.o $(EXTRA_OBJS) $(SYS_LIBS) $(LIBS)

hardlink1: hardlink1.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ hardlink1.o $(EXTRA_OBJS) $(LIBS)

hardlink2: hardlink2.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ hardlink2.o $(EXTRA_OBJS) $(LIBS)

hardlink4: hardlink4.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ hardlink4.o $(EXTRA_OBJS) $(LIBS)

mkdir2: mkdir2.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ mkdir2.o $(EXTRA_OBJS) $(LIBS)

mkdir3: mkdir3.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ mkdir3.o $(EXTRA_OBJS) $(LIBS)

create-files: create-files.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ create-files.o $(EXTRA_OBJS) $(LIBS)

create-symlinks: create-symlinks.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ create-symlinks.o $(EXTRA_OBJS) $(LIBS)

create-dirs: create-dirs.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ create-dirs.o $(EXTRA_OBJS) $(LIBS)

create-remove: create-remove.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ create-remove.o $(EXTRA_OBJS) $(LIBS)

dup2-and-unlog: dup2-and-unlog.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ dup2-and-unlog.o  $(EXTRA_OBJS) $(AUTH_LIBS) $(LIBS)

readdir-vs-lstat: readdir-vs-lstat.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ readdir-vs-lstat.o $(EXTRA_OBJS) $(LIBS)

symlink: symlink.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ symlink.o $(EXTRA_OBJS) $(LIBS)

echo-n: echo-n.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ echo-n.o $(EXTRA_OBJS)

test-parallel1: test-parallel1.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ test-parallel1.o $(EXTRA_OBJS) $(LIBS)

test-parallel2: test-parallel2.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ test-parallel2.o $(EXTRA_OBJS) $(LIBS)

create-stat: create-stat.o fs_lib.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ create-stat.o fs_lib.o $(EXTRA_OBJS) $(SYS_LIBS) $(LIBS)

kill-softly: kill-softly.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ kill-softly.o $(EXTRA_OBJS) $(LIBS)

kill-softer: kill-softer.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ kill-softer.o $(EXTRA_OBJS) $(LIBS)

rm-rf: rm-rf.o fs_lib.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ rm-rf.o fs_lib.o $(EXTRA_OBJS) $(SYS_LIBS) $(LIBS)

exit-wo-close: exit-wo-close.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ exit-wo-close.o $(EXTRA_OBJS) $(LIBS)

strange-characters-c: strange-characters-c.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ strange-characters-c.o $(EXTRA_OBJS) $(LIBS)

pine: pine.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ pine.o $(EXTRA_OBJS) $(LIBS)

append-over-page: append-over-page.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ append-over-page.o $(EXTRA_OBJS) $(LIBS)

write-ucc: write-ucc.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ write-ucc.o $(EXTRA_OBJS) $(LIBS)

utime-dir: utime-dir.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ utime-dir.o $(EXTRA_OBJS) $(LIBS)

utime-file: utime-file.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ utime-file.o $(EXTRA_OBJS) $(LIBS)

mmap-shared-write: mmap-shared-write.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ mmap-shared-write.o $(EXTRA_OBJS) $(LIBS)

rename5: rename5.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ rename5.o $(EXTRA_OBJS) $(LIBS)

rename6: rename6.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ rename6.o $(EXTRA_OBJS) $(LIBS)

write3: write3.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ write3.o $(EXTRA_OBJS) $(LIBS)

rename-under-feet: rename-under-feet.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ rename-under-feet.o $(EXTRA_OBJS) $(LIBS)

write-closed: write-closed.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ write-closed.o $(EXTRA_OBJS) $(LIBS)

write-closed2: write-closed2.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ write-closed2.o $(EXTRA_OBJS) $(SYS_LIBS) $(LIBS)

truncate-files: truncate-files.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ truncate-files.o $(EXTRA_OBJS) $(LIBS)

mmap-cat: mmap-cat.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ mmap-cat.o $(EXTRA_OBJS) $(LIBS)

run-tests: run-tests.in
	(cd ..; CONFIG_FILES=tests/run-tests CONFIG_HEADERS= $(SHELL) /usr/src/openafs/openafs/config.status)
	@chmod +x run-tests

#.c.o:
#	$(CC) -c $(CPPFLAGS) $(DEFS) -I$(srcdir) -I. $(REALCFLAGS) $<

afscp: afscp.o afscp_callback.o $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -o $@ afscp.o afscp_callback.o $(EXTRA_OBJS) $(INT_LIBS)

hello-world:   hello-world.in
	sed -e "s!%CC%!$(CC)!" $(srcdir)/hello-world.in > $@
	chmod +x hello-world

mountpoint:   mountpoint.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/mountpoint.in > $@
	chmod +x mountpoint

clean:
	rm -f run-tests $(TEST_PROGRAMS) *.o *~

realclean:

distclean: clean
	rm -f Makefile

mostlyclean:

install:

uninstall:


TAGS:	$(TEST_SRCS)
	etags $(TEST_SRCS)

check: run-tests $(TEST_PROGRAMS)
	./run-tests -all

check-fast: run-tests $(TEST_PROGRAMS)
	./run-tests -all -fast

.PHONY:		all install clean realclean distclean mostlyclean install uninstall check
