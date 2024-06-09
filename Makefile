CC = gcc
CFLAGS = -Wall -g
PROG = tinyFSDemo
OBJS = tinyFSDemo.o libTinyFS.o libDisk.o diskTest.o
EXTRACLEAN = tinyFSDemo

all: tinyFSDemo

clean:	
	rm -f $(OBJS) $(EXTRACLEAN) *.dsk *~ TAGS

tinyFSDemo: tinyFSDemo.o libDisk.c libDisk.h libTinyFS.c libTinyFS.h TinyFS_errno.h
	$(CC) $(CFLAGS) -o tinyFSDemo tinyFSDemo.o libDisk.c libDisk.h libTinyFS.c libTinyFS.h

tinyFSDemo.o: tinyFSDemo.c libDisk.c libDisk.h libTinyFS.c libTinyFS.h
	$(CC) $(CFLAGS) -c -o $@ $<

libTinyFS.o: libTinyFS.c libTinyFS.h libDisk.h libDisk.o TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

libDisk.o: libDisk.c libDisk.h TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<
