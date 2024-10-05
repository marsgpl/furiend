#include "equal.h"

LUAMOD_API int luaopen_equal(lua_State *L) {
    lua_pushcfunction(L, equal);
    return 1;
}

static int is_equal(lua_State *L, int a_idx, int b_idx, int cache_idx) {
    int a_type = lua_type(L, a_idx);
    int b_type = lua_type(L, b_idx);

    if (a_type != b_type) {
        return 0; // type mismatch
    }

    switch (a_type) {
        case LUA_TNONE:
        case LUA_TNIL:
            return 1;
        case LUA_TBOOLEAN:
            return lua_toboolean(L, a_idx) == lua_toboolean(L, b_idx);
        case LUA_TFUNCTION:
        case LUA_TTHREAD:
        case LUA_TUSERDATA: // no deep compare
        case LUA_TLIGHTUSERDATA: // no mt compare
            return lua_topointer(L, a_idx) == lua_topointer(L, b_idx);
        case LUA_TNUMBER:
            if (lua_isinteger(L, a_idx) != lua_isinteger(L, b_idx)) {
                return 0; // number type mismatch
            } else if (lua_isinteger(L, a_idx)) {
                return lua_tointeger(L, a_idx) == lua_tointeger(L, b_idx);
            } else {
                return lua_tonumber(L, a_idx) == lua_tonumber(L, b_idx);
            }
        case LUA_TSTRING: {
            size_t a_len;
            size_t b_len;

            const char *a = lua_tolstring(L, a_idx, &a_len);
            const char *b = lua_tolstring(L, b_idx, &b_len);

            if (a_len != b_len) {
                return 0; // len mismatch
            }

            return memcmp(a, b, a_len) == 0;
        }
        case LUA_TTABLE: {
            if (luaL_len(L, a_idx) != luaL_len(L, b_idx)) {
                return 0; // len mismatch
            }

            lua_rawgetp(L, cache_idx, lua_topointer(L, a_idx));
            lua_rawgetp(L, cache_idx, lua_topointer(L, b_idx));

            const void *a_cached = lua_isnil(L, -2) ? 0 : lua_topointer(L, -2);
            const void *b_cached = lua_isnil(L, -1) ? 0 : lua_topointer(L, -1);

            lua_pop(L, 2);

            if (a_cached && b_cached) { // both cached
                return a_cached == lua_topointer(L, b_idx)
                    && b_cached == lua_topointer(L, a_idx);
            } else if (!a_cached && !b_cached) { // both not cached
                lua_pushvalue(L, a_idx);
                lua_rawsetp(L, cache_idx, lua_topointer(L, b_idx));
                lua_pushvalue(L, b_idx);
                lua_rawsetp(L, cache_idx, lua_topointer(L, a_idx));
            } else {
                return 0; // one is cached, other is not
            }

            luaL_checkstack(L, 4, "is_equal");

            int a_len = 0;
            int b_len = 0;

            lua_pushnil(L);
            while (lua_next(L, a_idx)) {
                a_len++;

                lua_pushvalue(L, -2); // key, a_val, key
                lua_gettable(L, b_idx); // key, a_val, b_val

                int a_val_idx = lua_gettop(L) - 1;
                int b_val_idx = lua_gettop(L);

                if (!is_equal(L, a_val_idx, b_val_idx, cache_idx)) {
                    return 0;
                }

                lua_pop(L, 2); // a_val, b_val
            }

            lua_pushnil(L);
            while (lua_next(L, b_idx)) {
                b_len++;
                lua_pop(L, 1);
            }

            return a_len == b_len;
        }
        default:
            return luaL_error(L, "unknown type: %s (%d)",
                lua_typename(L, a_type), a_type);
    }
}

static int equal(lua_State *L) {
    luaF_need_args(L, 2, "equal");

    if (lua_type(L, 1) == LUA_TTABLE && lua_type(L, 2) == LUA_TTABLE) {
        lua_createtable(L, 0, 2);
    }

    lua_pushboolean(L, is_equal(L, 1, 2, 3));

    return 1;
}
