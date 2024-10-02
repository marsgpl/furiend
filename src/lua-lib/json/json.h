#ifndef LUA_LIB_JSON_H
#define LUA_LIB_JSON_H

#include <furiend/lib.h>

LUAMOD_API int luaopen_json(lua_State *L);

int json_parse(lua_State *L);
int json_stringify(lua_State *L);

static const luaL_Reg json_index[] = {
    { "parse", json_parse },
    { "stringify", json_stringify },
    { NULL, NULL }
};

#endif
