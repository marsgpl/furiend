#include "redis.h"

LUAMOD_API int luaopen_redis(lua_State *L) {
    if (likely(luaL_newmetatable(L, MT_REDIS_CLIENT))) {
        luaL_newlib(L, redis_client_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, redis_client_gc);
        lua_setfield(L, -2, "__gc");
    }

    luaL_newlib(L, redis_index);
    return 1;
}

int redis_client(lua_State *L) {
    ud_redis_client *client = luaF_new_uduv_or_error(L, sizeof(ud_loop), 0);

    client->fd = -1;

    luaL_setmetatable(L, MT_REDIS_CLIENT);

    return 1;
}

int redis_client_gc(lua_State *L) {
    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);

    if (client->fd != -1) {
        luaF_close_or_warning(L, client->fd);
        client->fd = -1;
    }

    return 0;
}

int redis_ping(lua_State *L) {
    luaF_need_args(L, 1, "ping");

    ud_redis_client *client = lua_touserdata(L, 1);

    lua_State *T = luaF_new_thread_or_error(L);

    (void)client;
    (void)T;

    return 1;
}
