CC = zig cc
CFLAGS = -O2
LDFLAGS = -llzma

all: m1u

m1u: m1u.c
	$(CC) $(CFLAGS) -o m1u m1u.c $(LDFLAGS)

clean:
	rm -f m1u

.PHONY: all clean
