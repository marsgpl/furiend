#ifndef LUA_LIB_UNWRAP_H
#define LUA_LIB_UNWRAP_H

#include <furiend/shared.h>

LUAMOD_API int luaopen_unwrap(lua_State *L);

static int unwrap(lua_State *L);

#endif
