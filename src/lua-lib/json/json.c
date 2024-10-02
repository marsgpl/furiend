#include "json.h"

LUAMOD_API int luaopen_json(lua_State *L) {
    luaL_newlib(L, json_index);
    return 1;
}

int json_parse(lua_State *L) {
    luaF_need_args(L, 1, "json.parse");
    luaL_checktype(L, 1, LUA_TSTRING);

    lua_pushnil(L);
    return 1;
}

int json_stringify(lua_State *L) {
    luaF_need_args(L, 1, "json.stringify");

    lua_pushstring(L, "");
    return 1;
}
