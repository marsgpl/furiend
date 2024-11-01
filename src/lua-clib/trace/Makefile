NAME= trace
FU_SRC= /furiend/src
LUA_SRC= /furiend/vendor/lua-5.4.7/src
CC= gcc
LD= gcc
CCFLAGS= -g -fstack-protector-all -c -fPIC -O2 -std=c2x -march=native \
    -fno-ident -Wall -Wextra -Wshadow -Wstrict-aliasing -Werror -pedantic
LDFLAGS= -shared -Wl,-z,max-page-size=0x1000
INCS= -I$(FU_SRC) -I$(LUA_SRC)
LIBS=

build: $(NAME).so

clean:
	rm -f *.o $(NAME).so

$(NAME).so: $(NAME).o $(FU_SRC)/furiend/shared.o
	$(LD) -o $@ $^ $(LIBS) $(LDFLAGS)

.c.o:
	$(CC) $(CCFLAGS) -o $@ $< $(INCS)

$(NAME).o: $(NAME).c $(NAME).h $(FU_SRC)/furiend/shared.h

.PHONY: build clean
