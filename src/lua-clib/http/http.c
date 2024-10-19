#include "http.h"

LUAMOD_API int luaopen_http(lua_State *L) {
    if (luaL_newmetatable(L, MT_HTTP_REQUEST)) {
        lua_pushcfunction(L, http_request_gc);
        lua_setfield(L, -2, "__gc");
    }

    if (luaL_newmetatable(L, MT_HTTP_SERV)) {
        lua_pushcfunction(L, http_serv_gc);
        lua_setfield(L, -2, "__gc");
        luaL_newlib(L, http_serv_index);
        lua_setfield(L, -2, "__index");
    }

    if (luaL_newmetatable(L, MT_HTTP_SERV_CLIENT)) {
        lua_pushcfunction(L, http_serv_client_gc);
        lua_setfield(L, -2, "__gc");
    }

    luaL_newmetatable(L, MT_HTTP_SERV_REQ);

    if (luaL_newmetatable(L, MT_HTTP_SERV_RES)) {
        luaL_newlib(L, http_serv_res_index);
        lua_setfield(L, -2, "__index");
    }

    luaL_newlib(L, http_index);
    return 1;
}
