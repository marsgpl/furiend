#ifndef LUA_LIB_TIME_H
#define LUA_LIB_TIME_H

#include <furiend/lib.h>

LUAMOD_API int luaopen_time(lua_State *L);

static int get_time(lua_State *L);

#endif
