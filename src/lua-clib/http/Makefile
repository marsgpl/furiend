NAME= http
FU_SRC= /furiend/src
LUA_SRC= /furiend/vendor/lua-5.4.7/src
CC= gcc
LD= gcc
CCFLAGS= -g -fstack-protector-all -c -fPIC -O2 -std=c2x -march=native \
    -fno-ident -Wall -Wextra -Wshadow -Wstrict-aliasing -Werror -pedantic
LDFLAGS= -shared -Wl,-z,max-page-size=0x1000
INCS= -I$(FU_SRC) -I$(LUA_SRC)
LIBS= -lssl

build: $(NAME).so

clean:
	rm -f *.o $(NAME).so

$(NAME).so: $(NAME).o shared.o request.o server.o $(FU_SRC)/furiend/shared.o
	$(LD) -o $@ $^ $(LIBS) $(LDFLAGS)

.c.o:
	$(CC) $(CCFLAGS) -o $@ $< $(INCS)

$(NAME).o: $(NAME).c $(NAME).h request.h server.h
shared.o: shared.c shared.h $(FU_SRC)/furiend/shared.h
request.o: request.c request.h shared.h
server.o: server.c server.h shared.h

.PHONY: build clean
