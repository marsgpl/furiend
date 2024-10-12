#include "http.h"

LUAMOD_API int luaopen_http(lua_State *L) {
    if (luaL_newmetatable(L, MT_HTTP_REQUEST)) {
        lua_pushcfunction(L, http_request_gc);
        lua_setfield(L, -2, "__gc");
    }

    if (luaL_newmetatable(L, MT_HTTP_SERVER)) {
        lua_pushcfunction(L, http_server_gc);
        lua_setfield(L, -2, "__gc");
    }

    luaL_newlib(L, http_index);
    return 1;
}
