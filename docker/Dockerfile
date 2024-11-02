FROM alpine:3.20.3

WORKDIR /furiend

RUN apk update
RUN apk upgrade
RUN apk add gcc make
RUN apk add valgrind gdb perf tcpflow
RUN apk add musl-dev # busybox libc headers
RUN apk add openssl openssl-libs-static openssl-dev
RUN apk add ncurses # tput
RUN apk add cmake # yyjson
RUN apk add tcl # redis test
RUN apk add git openssh curl vim
RUN apk add luarocks5.4 # cjson
