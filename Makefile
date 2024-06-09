CC = gcc
CFLAGS = -Wall -g
PROG = tinyFSDemo
OBJS = tinyFSDemo.o libTinyFS.o libDisk.o diskTest.o
EXTRACLEAN = libTinyFS

all: tinyFSDemo

clean:	
	rm -f $(OBJS) $(EXTRACLEAN) *.dsk *~ TAGS

tinyFSDemo: tinyFSDemo.o libDisk.c libDisk.h libTinyFS.c libTinyFS.h TinyFS_errno.h
	$(CC) $(CFLAGS) -o tinyFSDemo tinyFSDemo.o libDisk.c libDisk.h libTinyFS.c libTinyFS.h

tinyFSDemo.o: tinyFSDemo.c libDisk.c libDisk.h libTinyFS.c libTinyFS.h
	$(CC) $(CFLAGS) -c -o $@ $<

diskTest: diskTest.o libDisk.c libDisk.h
	$(CC) $(CFLAGS) -o diskTest diskTest.o libDisk.c libDisk.h

diskTest.o: diskTest.c libDisk.c libDisk.h
	$(CC) $(CFLAGS) -c -o $@ $<

libTinyFS: libTinyFS.o libDisk.c libDisk.h TinyFS_errno.h
	$(CC) $(CFLAGS) -o libTinyFS libTinyFS.o libDisk.c libDisk.h

libTinyFS.o: libTinyFS.c libTinyFS.h libDisk.h libDisk.o TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

libDisk.o: libDisk.c libDisk.h TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<
