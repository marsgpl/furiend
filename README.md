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

## Diagnostics

```sh
nm src/lua-lib/async/async.so
```
