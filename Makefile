CC = gcc
OBJECTS = grep-v7.o
CFLAGS =

grep all: $(OBJECTS)
		$(CC) $(CFLAGS) $(OBJECTS) -o mygrep

grep-v7.o:

clean:
	rm -f *.o grep core