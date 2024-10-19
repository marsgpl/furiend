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

## Redis

```sh
mkdir -p /furiend/vendor && cd /furiend/vendor
wget https://download.redis.io/redis-stable.tar.gz
tar -xvf redis-stable.tar.gz
cd redis-stable
make
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

- redis
- socket
- fifo encryption
- basic dc
- ci

- tgbot: hook custom crt + static ip
- http serv: req pool
- http serv: max clients
- http serv: chunked
- http: timeout
- http: ip6
- http: keep-alive
- http: more validations
- http: headers normalization
- http req: crt verification
- http req: params: url, query
- http req: application/x-www-form-urlencoded
- http req: multipart/form-data
- http req: gzip
- http req: check content-len, prealloc, rm if (0)
- http req: buffer pool
- http req: reuse ssl
- http req: multiple chunks
- cookies
- dns: more types
- dns: ip6
- socket: tcp + msg separation
- fs: file, dir, stat
