NAME= redis
FU_SRC= /furiend/src
LUA_SRC= /furiend/vendor/lua-5.4.7/src
CC= gcc
LD= gcc
CCFLAGS= -c -fPIC -O2 -std=c2x -march=native -fno-ident \
    -Wall -Wextra -Wshadow -Wstrict-aliasing -Werror -pedantic
LDFLAGS= -shared -Wl,-z,max-page-size=0x1000
INCS= -I$(FU_SRC) -I$(LUA_SRC)
LIBS=

build: $(NAME).so

clean:
	rm -f *.o $(NAME).so

$(NAME).so: $(NAME).o resp.o \
    $(FU_SRC)/furiend/shared.o \
    $(FU_SRC)/furiend/strbuf.o
	$(LD) -o $@ $^ $(LIBS) $(LDFLAGS)

.c.o:
	$(CC) $(CCFLAGS) -o $@ $< $(INCS)

$(NAME).o: $(NAME).c $(NAME).h resp.h \
    $(FU_SRC)/furiend/shared.h \
    $(FU_SRC)/furiend/strbuf.h
resp.o: resp.c resp.h \
    $(FU_SRC)/furiend/shared.h \
    $(FU_SRC)/furiend/strbuf.h

.PHONY: build clean
