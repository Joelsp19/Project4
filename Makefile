CC = gcc
CFLAGS = -Wall -g
PROG = tinyFSDemo
OBJS = tinyFSDemo.o libTinyFS.o libDisk.o diskTest.o
EXTRACLEAN = tinyFSDemo libDisk diskTest

all: testfile diskTest libTinyFS

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS)

clean:	
	rm -f $(OBJS) $(EXTRACLEAN) *.dsk *~ TAGS

testfile: testfile.o libTinyFS.c libDisk.c libDisk.h
	$(CC) $(CFLAGS) -o testfile testfile.o libTinyFS.c libTinyFS.h libDisk.c libDisk.h

testfile.o: tfsTest.c libTinyFS.c libDisk.c libDisk.h
	$(CC) $(CFLAGS) -c -o $@ $<

diskTest: diskTest.o libDisk.c libDisk.h
	$(CC) $(CFLAGS) -o diskTest diskTest.o libDisk.c libDisk.h

diskTest.o: diskTest.c libDisk.c libDisk.h
	$(CC) $(CFLAGS) -c -o $@ $<

tinyFsDemo.o: tinyFSDemo.c libTinyFS.h tinyFS.h TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

libTinyFS: libTinyFS.o libDisk.c libDisk.h TinyFS_errno.h
	$(CC) $(CFLAGS) -o libTinyFS libTinyFS.o libDisk.c libDisk.h

libTinyFS.o: libTinyFS.c libTinyFS.h tinyFS.h libDisk.h libDisk.o TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

libDisk.o: libDisk.c libDisk.h tinyFS.h TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<
