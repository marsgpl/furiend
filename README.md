# Füriend

## Docker

```sh
docker network create \
    --subnet 172.20.0.0/24 \
    --gateway 172.20.0.1 \
    furiend

docker builder build --tag furiend:latest docker

./shell

docker network ls
docker image ls -a
docker container ls

docker container rm furiend_shell
docker image rm furiend
docker network rm furiend
```

## Lua

```sh
mkdir -p /furiend/vendor && cd /furiend/vendor
wget https://www.lua.org/ftp/lua-5.4.7.tar.gz
tar -xvf lua-5.4.7.tar.gz
cd lua-5.4.7
make linux test
mkdir -p /furiend/bin
cp src/lua /furiend/bin/
cp src/luac /furiend/bin/
```

## yyjson

```sh
mkdir -p /furiend/vendor && cd /furiend/vendor
git clone https://github.com/ibireme/yyjson.git
cd yyjson
mkdir build && cd build
cmake ..
cmake --build .
```

## cjson

```sh
mkdir -p /furiend/vendor && cd /furiend/vendor
git clone https://github.com/mpx/lua-cjson.git
cd lua-cjson
sed -i "s/_CFLAGS =/_CFLAGS = \-I\/furiend\/vendor\/lua-5.4.7\/src/" Makefile
make
```

## TODO

- http: server
    - new lua thread on every request (pool? prob not)
    - nobody subscribes to these threads but router tracks them
    - wait for callback finish (callback can yield)
- tgwh + validate secret key
- provision fe2 ssh tunnel to develop locally
- redis
- http: crt verification
- url
- http: params: url, query
- tgbot: hook custom crt + static ip
- http: timeout
- http: application/x-www-form-urlencoded
- http: multipart/form-data
- http: gzip
- http: parse response len to preallocate buf
- http: buffer pool to reuse
- http: ip6
- http: keep-alive
- http: more validations
- http: reuse ssl
- http: multiple chunks
- http: headers normalization
- cookie
- dns: more types
- dns: ip6
- simplify async boilerplate
- fs: file
- fs: dir
- fs: stat
- tcp stream packet separation
- socket
- epoll
- review libs
- fe2 ci (build, pack, deploy)
