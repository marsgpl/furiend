#ifndef LUA_LIB_JSON_H
#define LUA_LIB_JSON_H

#include <yyjson.h>
#include <furiend/lib.h>

LUAMOD_API int luaopen_json(lua_State *L);

int json_parse(lua_State *L);
int json_stringify(lua_State *L);

static inline int is_array(lua_State *L, int index);
static void json_parse_value(lua_State *L, yyjson_val *value);

static inline yyjson_mut_val *json_stringify_value(
    lua_State *L,
    yyjson_mut_doc *doc,
    int index,
    int cache_index);

static inline yyjson_mut_val *json_stringify_table(
    lua_State *L,
    yyjson_mut_doc *doc,
    int index,
    int cache_index);

static const luaL_Reg json_index[] = {
    { "parse", json_parse },
    { "stringify", json_stringify },
    { NULL, NULL }
};

#endif
