#include "server.h"

int http_server(lua_State *L) {
    luaF_need_args(L, 1, "http.server");
    luaL_checktype(L, 1, LUA_TTABLE); // params

    return 1;
}

int http_server_gc(lua_State *L) {
    (void)L;
    return 0;
}
