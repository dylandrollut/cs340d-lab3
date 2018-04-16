CC = gcc
OBJECTS = grep-v7.o
CFLAGS =

grep all: $(OBJECTS)
		$(CC) $(CFLAGS) $(OBJECTS) -o v7grep

grep-v7.o:

clean:
	rm -f *.o v7grep core