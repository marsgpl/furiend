#ifndef LUA_LIB_REDIS_H
#define LUA_LIB_REDIS_H

#include <furiend/shared.h>

#define MT_REDIS_CLIENT "redis.client*"

typedef struct {
    int fd;
} ud_redis_client;

LUAMOD_API int luaopen_redis(lua_State *L);

int redis_client(lua_State *L);
int redis_client_gc(lua_State *L);

int redis_ping(lua_State *L);

static const luaL_Reg redis_index[] = {
    { "client", redis_client },
    { NULL, NULL }
};

static const luaL_Reg redis_client_index[] = {
    { "ping", redis_ping },
    { "close", redis_client_gc },
    { NULL, NULL }
};

#endif
