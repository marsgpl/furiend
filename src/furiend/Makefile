CC= gcc
LD= gcc
CCFLAGS= -c -O2 -std=c2x -march=native -fno-ident \
    -Wall -Wextra -Wshadow -Wstrict-aliasing -Werror -pedantic
INCS= -I/furiend/vendor/lua-5.4.7/src

lib: lib.o

clean:
	rm -f *.o

.c.o:
	$(CC) $(CCFLAGS) -fPIC -o $@ $< $(INCS)

lib.o: lib.c lib.h

.PHONY: lib clean
