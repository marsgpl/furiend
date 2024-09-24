#ifndef LUA_LIB_SLEEP_H
#define LUA_LIB_SLEEP_H

#define _POSIX_C_SOURCE 199309L // CLOCK_MONOTONIC

#include <sys/timerfd.h>
#include <furiend/lib.h>

LUAMOD_API int luaopen_sleep(lua_State *L);

static int sleep_create(lua_State *L);
static int sleep_start(lua_State *L);
static int sleep_continue(lua_State *L, int status, lua_KContext ctx);

#endif
