#ifndef LUA_LIB_REDIS_H
#define LUA_LIB_REDIS_H

#include <furiend/shared.h>
#include "resp.h"

#define MT_REDIS_CLIENT "redis.client*"

#define REDIS_UDUVIDX_CONFIG 1
#define REDIS_UDUVIDX_CONN_THREAD 2

typedef struct {
    int fd;
    int connected;
    int can_write;
} ud_redis_client;

LUAMOD_API int luaopen_redis(lua_State *L);

int redis_client(lua_State *L);
int redis_client_gc(lua_State *L);
int redis_connect(lua_State *L);
int redis_ping(lua_State *L);

static int connect_start(lua_State *L);
static int connect_continue(lua_State *L, int status, lua_KContext ctx);

static int router_start(lua_State *L);
static int router_continue(lua_State *L, int status, lua_KContext ctx);
static int router_process_event(lua_State *L);

static int ping_start(lua_State *L);
static int ping_continue(lua_State *L, int status, lua_KContext ctx);

static const luaL_Reg redis_index[] = {
    { "client", redis_client },
    { NULL, NULL }
};

static const luaL_Reg redis_client_index[] = {
    { "connect", redis_connect },
    { "ping", redis_ping },
    { "close", redis_client_gc },
    { NULL, NULL }
};

#endif
