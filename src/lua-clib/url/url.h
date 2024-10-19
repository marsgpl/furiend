#ifndef LUA_LIB_URL_H
#define LUA_LIB_URL_H

#define _GNU_SOURCE

#include <string.h>
#include <furiend/shared.h>

LUAMOD_API int luaopen_url(lua_State *L);

static int parse_url(lua_State *L);

#endif
