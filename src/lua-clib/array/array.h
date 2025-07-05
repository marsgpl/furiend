#ifndef LUA_LIB_ARRAY_H
#define LUA_LIB_ARRAY_H

#include <furiend/shared.h>

LUAMOD_API int luaopen_array(lua_State *L);

int array_is_array(lua_State *L);

static const luaL_Reg array_index[] = {
    { "is_array", array_is_array },
    { NULL, NULL }
};

#endif
