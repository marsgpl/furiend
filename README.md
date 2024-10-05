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
wget https://github.com/ibireme/yyjson/archive/refs/heads/master.zip
mv master.zip yyjson-master.zip
unzip yyjson-master.zip
cd yyjson-master
mkdir build && cd build
cmake ..
cmake --build .
mkdir -p /furiend/include/vendor
cp libyyjson.a /furiend/include/vendor
```

## TODO

- redis
- url
- log
- json: pass params to yyjson
- http: ip6, keep-alive, parse response len, server, reuse string buffers,
    params: url, timeout, custom request headers, param: query, POST body,
    crt verification, more validations, reuse ssl, http 2+, gzip, quic,
    prop: add_response_headline, headers normalization
- cookie
- dns: more types, ip6
- simplify async boilerplate
- fs: file, dir, stat
- string utils: tcp stream packet separation
- socket
- epoll
- sandbox
- posix threads
- fork
- review all libs
