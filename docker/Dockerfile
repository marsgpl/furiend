FROM alpine:3.20.3
WORKDIR /furiend
RUN apk update
RUN apk upgrade
RUN apk add vim
RUN apk add gcc make
RUN apk add ncurses # tput
RUN apk add musl-dev # busybox libc headers
RUN apk add openssl openssl-libs-static openssl-dev
