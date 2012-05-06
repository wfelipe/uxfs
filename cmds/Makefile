CC = gcc
CFLAGS = -g -O0 -Wall
headers = ../kern/uxfs.h
objects = mkfs.o fsdb.o

all: mkfs fsdb

.c.o:
	$(CC) $(CFLAGS) -c $<

mkfs: mkfs.o $(headers)
	$(CC) $(CFLAGS) -o mkfs mkfs.o

fsdb: fsdb.o $(headers)
	$(CC) $(CFLAGS) -o fsdb fsdb.o

$(objects): $(headers)

clean:
	rm -f $(objects) mkfs fsdb
