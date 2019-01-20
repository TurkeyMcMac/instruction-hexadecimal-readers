all: ihr.o

ihr.o: ihr.c ihr.h
	$(CC) -O3 -ansi -Wall -Wextra -Wpedantic $(CFLAGS) -c -o $@ $<

clean:
	$(RM) ihr.o
