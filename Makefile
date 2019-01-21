header = ihr.h
source = ihr.c
object = ihr.o

test-header = test.h
tests = $(patsubst %.c, %.o, $(wildcard tests/*.c))

all: $(object)

ihr.o: $(source) $(header)
	$(CC) -O3 -ansi -Wall -Wextra -Wpedantic $(CFLAGS) -c -o $@ $<

tests: $(tests)
	./run-tests

tests/%.o: tests/%.c $(object) $(test-header)
	$(CC) $(CFLAGS) -c -o $@ $< && $(CC) -o $@ $@ $(object)

clean:
	$(RM) ihr.o tests/*.o


.PHONY: all tests clean
