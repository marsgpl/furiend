FROM alpine:3.20.3

WORKDIR /furiend

RUN apk update
RUN apk upgrade
RUN apk add vim
RUN apk add gcc make
RUN apk add ncurses # tput
RUN apk add musl-dev # busybox libc headers
RUN apk add cmake # yyjson
RUN apk add openssl openssl-libs-static openssl-dev
RUN apk add git openssh # for cloning repos
RUN apk add valgrind # for checking memleaks
RUN apk add tcl # redis test

ENV IN_DOCKER=true

# fe2 tgwh
EXPOSE 19001/tcp
