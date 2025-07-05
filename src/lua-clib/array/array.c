#include "array.h"

LUAMOD_API int luaopen_array(lua_State *L) {
    luaL_newlib(L, array_index);
    return 1;
}

int array_is_array(lua_State *L) {
    lua_pushboolean(L, luaF_is_array(L, 1));
    return 1;
}
