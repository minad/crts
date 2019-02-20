SOURCES=$(wildcard native/*.c cby/*.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))
COMMON_WARNINGS=-Wformat=2 -Wdouble-promotion -Wnull-dereference -Wshadow \
   -Wpointer-arith -Wcast-qual -Wcast-align -Wall -Wextra -Wconversion -Wc++-compat \
   -Wmissing-prototypes -Wstrict-prototypes -Wvla \
   -Wnested-externs -Wdisabled-optimization \
   -Wmissing-format-attribute -Wdate-time \
   -Winit-self -Wundef \
   -Wswitch-enum \
   -Wno-packed-bitfield-compat \
   -Werror
FLAGS=-DCHI_CONFIG=native/release.h -DCHI_MODE=release -std=gnu11 $(COMMON_WARNINGS) -g -pthread -O2 -Inative/include -Inative -Igenerated -Iconfig -D_GNU_SOURCE

run: $(OBJECTS)
	$(CC) $(FLAGS) $^ -Wl,--export-dynamic -Wl,--script=config/sort-text.lds -lffi -ldl -lm -pthread -o $@

%.o : %.c
	$(CC) -DCHI_GUID=$$(echo -n $< | md5sum | colrm 33) $(FLAGS) -o $@ -c $<

clean:
	rm -f $(OBJECTS)
