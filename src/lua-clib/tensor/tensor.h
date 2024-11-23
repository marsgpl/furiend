#ifndef LUA_LIB_TENSOR_H
#define LUA_LIB_TENSOR_H

#include <furiend/shared.h>

#define GEN_ID_LOOP_LIMIT 1000
#define GEN_ID_SEP "-"

LUAMOD_API int luaopen_tensor(lua_State *L);

int tensor_unwrap(lua_State *L);
int tensor_gen_id(lua_State *L);

static const luaL_Reg tensor_index[] = {
    { "unwrap", tensor_unwrap },
    { "gen_id", tensor_gen_id },
    { NULL, NULL }
};

#endif
