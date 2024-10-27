#ifndef LUA_LIB_REDIS_RESP_H
#define LUA_LIB_REDIS_RESP_H

#include <furiend/shared.h>

int resp_pack(lua_State *L);
int resp_unpack(lua_State *L);

#endif
