#ifndef LUA_LIB_SLEEP_H
#define LUA_LIB_SLEEP_H

#include <furiend/shared.h>

LUAMOD_API int luaopen_sleep(lua_State *L);

static int sleep_create(lua_State *L);
static int sleep_start(lua_State *L);
static int sleep_continue(lua_State *L, int status, lua_KContext ctx);

#endif
