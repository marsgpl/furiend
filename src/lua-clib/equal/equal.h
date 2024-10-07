#ifndef LUA_LIB_EQUAL_H
#define LUA_LIB_EQUAL_H

#include <furiend/lib.h>

LUAMOD_API int luaopen_equal(lua_State *L);

static int equal(lua_State *L);
static int is_equal(lua_State *L, int a_idx, int b_idx, int cache_idx);

#endif
