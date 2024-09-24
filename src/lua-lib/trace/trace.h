#ifndef LUA_LIB_TRACE_H
#define LUA_LIB_TRACE_H

#include <furiend/lib.h>

#define MODE_VARIABLE 1
#define MODE_TABLE_KEY 2
#define MODE_TABLE_VALUE 3

#define RED "\033[0;31m" // string
#define YELLOW "\033[0;33m" // metatable
#define PURPLE "\033[0;35m" // float
#define CYAN "\033[0;36m" // int
#define RESET "\033[0m"

#define INDENT_LEN 4
#define MAX_DEPTH 16
#define CACHE_IDX 1
#define CACHE_MIN_SIZE 4

#define cache_get(L, table_index) \
    lua_rawgetp(L, CACHE_IDX, lua_topointer(L, table_index))

#define cache_set(L, table_index) { \
    lua_pushboolean(L, 1); \
    lua_rawsetp(L, CACHE_IDX, lua_topointer(L, table_index)); \
}

LUAMOD_API int luaopen_trace(lua_State *L);

static int trace(lua_State *L);

static void trace_value(lua_State *L, int index, int depth, int mode);
static void trace_table(lua_State *L, int index, int depth);

#endif
