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
mkdir -p /furiend/vendor
cd /furiend/vendor
wget https://www.lua.org/ftp/lua-5.4.7.tar.gz
tar -xvf lua-5.4.7.tar.gz
cd lua-5.4.7
make linux test
mkdir -p /furiend/bin
cp src/lua /furiend/bin/
cp src/luac /furiend/bin/
```

## TODO

- json
- redis
- url
- log
- http: ip6, plain read/write, keep-alive, parse response len, server,
    reuse string buffers, params: url, timeout, custom request headers,
    param: query, POST body, crt verification, more validations, reuse ssl,
    http 2+, quic, prop: add_response_headline (http/1.1 200 ok)
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
