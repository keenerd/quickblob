# quickblob - count blobs really fast

# -ggdb -Werror -std=c99
CFLAGS := -O2 -std=gnu99 -Wall -pedantic -Wextra ${CFLAGS}
LDLIBS  = -lpng -lz

VERSION=$(shell date +%Y%m%d)

%.a: %.o
	strip -d -X $<
	ar rvs $@ $<

all: csv-blobs

csv-blobs: csv-blobs.o quickblob.a

quickblob.a: quickblob.o

blobmark: quickblob.a

strip: csv-blobs
	strip --strip-all $^

profile: CFLAGS += -pg
profile: clean blobmark
	$(RM) best_case.txt worst_case.txt
	./blobmark 640 480 0 1 1
	gprof blobmark gmon.out > worst_case.txt
	$(RM) gmon.out
	./blobmark 640 480 3 0 1
	gprof blobmark gmon.out > best_case.txt

tcc-blobs:
	tcc -o $@ -lpng quickblob.c csv-blobs.c

clean:
	$(RM) *.o *.a csv-blobs blobmark tcc-blobs gmon.out

.PHONY: all clean strip profile
