# quickblob - count blobs really fast

#CFLAGS := -O2 -std=c99 -Wall -pedantic -Wextra -Werror ${CFLAGS}
CFLAGS := -O2 -std=c99 -Wall -pedantic -Wextra ${CFLAGS}
LDLIBS  = -lIL

VERSION=$(shell date +%Y%m%d)

all: quickblob csv-blobs

quickblob: quickblob.o
	strip -d -X quickblob.o
	ar rvs quickblob.a quickblob.o

csv-blobs:
	${CC} -o $@ ${LDLIBS} csv-blobs.c quickblob.a

strip: csv-blobs
	strip --strip-all csv-blobs

clean:
	rm -f *.o *.so *.a *.so.* csv-blobs

.PHONY: all clean strip

