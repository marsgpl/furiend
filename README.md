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
mkdir -p /furiend/bin
cd /furiend/vendor
wget https://www.lua.org/ftp/lua-5.4.7.tar.gz
tar -xvf lua-5.4.7.tar.gz
cd lua-5.4.7
make linux test
cp src/lua /furiend/bin/
cp src/luac /furiend/bin/
```

## LuaRocks

```sh
cd /furiend/vendor
wget https://luarocks.org/releases/luarocks-3.11.1.tar.gz
tar -xvf luarocks-3.11.1.tar.gz
./configure --with-lua-include=/furiend/vendor/lua-5.4.7/src
make install
```

## TODO

- url
- http client
- tls
- redis client
- log
- simplify dns async boilerplate
- dns client support more types
- dns client ip6
- fs (file, dir, stat)
- socket
- buffer (tcp stream packet separation)
- custom epoll
- string utils
- sandbox
- thread
