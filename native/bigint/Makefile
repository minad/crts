CFLAGS=-fsanitize=undefined -lgmp -O2 -Wall -Wextra -Wsign-conversion -Wconversion -g

all: test-zn test-gmp

test-zn: *.c *.h Makefile
	$(CC) $(CFLAGS) -DZ_GMP=0 test.c -o $@

test-gmp: *.c *.h Makefile
	$(CC) $(CFLAGS) -DZ_GMP=1 test.c -o $@
