# Füriend

## Docker

```sh
docker network create \
    --subnet 172.20.0.0/24 \
    --gateway 172.20.0.1 \
    furiend

docker builder build --tag furiend:latest docker

./shell
./redis
./dc
./fe1
./fe2

docker network ls
docker image ls -a
docker container ls

docker container stop furiend_shell furiend_redis furiend_dc furiend_fe1 furiend_fe2
docker container rm furiend_shell furiend_redis furiend_dc furiend_fe1 furiend_fe2
docker image rm furiend
docker network rm furiend
```

## net map

- 172.20.0.1 - gateway
- 172.20.0.2 - shell
- 172.20.0.3 - redis - 30303
- 172.20.0.10 - dc
- 172.20.0.11 - fe1 - 18001
- 172.20.0.12 - fe2 - 19001

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
make test
mkdir -p /furiend/bin
cp src/redis-cli /furiend/bin/
cp src/redis-server /furiend/bin/
cp src/redis-sentinel /furiend/bin/
mkdir -p /furiend/data
```

```sh
REDISCLI_AUTH=LocalPassword123 redis-cli -3 -u redis://redis:30303/0
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

## luarocks

```sh
luarocks-5.4 config variables.LUA_INCDIR /furiend/vendor/lua-5.4.7/src
luarocks-5.4 install lua-cjson
```

## segfaults, leaks

```sh
find src -type f -name *.o | xargs rm
build && while tests; do :; done
gdb --args lua test/main.lua # r, bt 100, q, y
valgrind --leak-check=full --show-leak-kinds=all -s tests
tcpflow -c port 30303 > dump.txt &
build && tests
killall -9 tcpflow
cat dump.txt
```

## todo

- async.race(t1, t2)
- socket
- fifo encryption
- ci
- tgbot: hook custom crt + static ip
- http stress test + vuln test
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
- get rid of yyjson?
