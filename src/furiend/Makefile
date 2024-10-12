LUA_SRC= /furiend/vendor/lua-5.4.7/src
CC= gcc
LD= gcc
CCFLAGS= -c -fPIC -O2 -std=c2x -march=native -fno-ident \
    -Wall -Wextra -Wshadow -Wstrict-aliasing -Werror -pedantic
INCS= -I$(LUA_SRC)

shared: shared.o

clean:
	rm -f *.o

.c.o:
	$(CC) $(CCFLAGS) -fPIC -o $@ $< $(INCS)

shared.o: shared.c shared.h $(LUA_SRC)/lauxlib.h

.PHONY: shared clean
