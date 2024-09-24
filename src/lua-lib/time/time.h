#ifndef LUA_LIB_TIME_H
#define LUA_LIB_TIME_H

#define _GNU_SOURCE // clock_gettime

#include <time.h>
#include <furiend/lib.h>

LUAMOD_API int luaopen_time(lua_State *L);

static int get_time(lua_State *L);

#endif
