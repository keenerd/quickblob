# quickblob - count blobs really fast

# -ggdb -Werror -std=c99
CFLAGS := -O2 -std=gnu99 -Wall -pedantic -Wextra ${CFLAGS}
LDLIBS  = -lIL

VERSION=$(shell date +%Y%m%d)

all: quickblob csv-blobs

quickblob: quickblob.o
	strip -d -X quickblob.o
	ar rvs quickblob.a quickblob.o

csv-blobs: quickblob
	${CC} -o $@ ${CFLAGS} ${LDLIBS} csv-blobs.c quickblob.a

strip: csv-blobs
	strip --strip-all csv-blobs

blobmark: quickblob
	${CC} -o $@ ${CFLAGS} -lrt blobmark.c quickblob.a

# embarrassingly hacky
profile:
	CFLAGS='-pg' $(MAKE) clean all blobmark
	rm -f gmon.out best_case.txt worst_case.txt
	./blobmark 640 480 0 1 1
	gprof blobmark gmon.out > worst_case.txt
	rm -f gmon.out
	./blobmark 640 480 3 0 1
	gprof blobmark gmon.out > best_case.txt

clean:
	rm -f *.o *.so *.a *.so.* csv-blobs blobmark gmon.out

.PHONY: all clean strip

