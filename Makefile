SOURCES=$(filter-out native/alias.c native/module.c, $(wildcard native/*.c cby/*.c cby/instrument/*.c cby/interpreter/*.c))
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
FLAGS=-DCHI_CBY_SUPPORT_ENABLED=1 -DCHI_TARGET=default -DCHI_CONFIG=native/release.h -DCHI_MODE=release -std=gnu11 $(COMMON_WARNINGS) -g -pthread -O2 -Inative/include -I. -Igenerated -Iconfig -D_GNU_SOURCE -DCBY_PROF_ENABLED

run: $(OBJECTS)
	$(CC) $(FLAGS) $^ -Wl,--export-dynamic -Wl,--script=config/sort-text.lds -lgmp -lz -lffi -ldl -lm -pthread -o $@

%.o : %.c
	$(CC) -DCHI_GUID=$$(echo -n $< | md5sum | colrm 33) $(FLAGS) -o $@ -c $<

clean:
	rm -f $(OBJECTS)
