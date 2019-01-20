all: ihr.o

ihr.o: ihr.c ihr.h
	$(CC) -ansi -Wall -Wextra -Wpedantic -c -o $@ $<

clean:
	$(RM) ihr.o
