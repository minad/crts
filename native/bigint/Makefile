CFLAGS=-fsanitize=undefined -lgmp -O2 -Wall -Wextra -Wsign-conversion -Wconversion -g

all: test-zn8 test-zn16 test-zn32 test-zn64 test-gmp

test-zn8: *.c z.h znogmp.h Makefile
	$(CC) $(CFLAGS) -Wno-conversion -DZ_GMP=0 -DZ_BITS=8 test.c -o $@

test-zn16: *.c z.h znogmp.h Makefile
	$(CC) $(CFLAGS) -Wno-conversion -DZ_GMP=0 -DZ_BITS=16 test.c -o $@

test-zn32: *.c z.h znogmp.h Makefile
	$(CC) $(CFLAGS) -DZ_GMP=0 -DZ_BITS=32 test.c -o $@

test-zn64: *.c z.h znogmp.h Makefile
	$(CC) $(CFLAGS) -DZ_GMP=0 -DZ_BITS=64 test.c -o $@

test-gmp: *.c z.h zgmp.h Makefile
	$(CC) $(CFLAGS) -DZ_GMP=1 test.c -o $@
