# quick_segment - count blobs really fast

#CFLAGS := -O2 -std=c99 -Wall -pedantic -Wextra -Werror ${CFLAGS}
CFLAGS := -O2 -std=c99 -Wall -pedantic -Wextra ${CFLAGS}
LDLIBS  = -lIL

VERSION=$(shell date +%Y%m%d)

all: quick_segment csv-segment

quick_segment: quick_segment.o
	strip -d -X quick_segment.o
	ar rvs quick_segment.a quick_segment.o

csv-segment:
	${CC} -o $@ ${LDLIBS} csv-segment.c quick_segment.a

strip: csv-segment
	strip --strip-all csv-segment

clean:
	rm -f *.o *.so *.a *.so.* csv-segment

.PHONY: all clean strip

