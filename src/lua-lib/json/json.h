#ifndef LUA_LIB_JSON_H
#define LUA_LIB_JSON_H

#include <yyjson.h>
#include <furiend/lib.h>

#define fail(L, msg, ...) { \
    lua_pushboolean(L, 0); \
    lua_pushfstring(L, msg __VA_OPT__(,) __VA_ARGS__); \
}

LUAMOD_API int luaopen_json(lua_State *L);

int json_parse(lua_State *L);
int json_stringify(lua_State *L);

static int is_array(lua_State *L, int index);
static int json_parse_value(lua_State *L, yyjson_val *value);
static yyjson_mut_val *json_stringify_value(
    lua_State *L,
    int index,
    yyjson_mut_doc *doc);

static const luaL_Reg json_index[] = {
    { "parse", json_parse },
    { "stringify", json_stringify },
    { NULL, NULL }
};

#endif
