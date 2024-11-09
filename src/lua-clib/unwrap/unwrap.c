#include "unwrap.h"

LUAMOD_API int luaopen_unwrap(lua_State *L) {
    lua_pushcfunction(L, unwrap);
    return 1;
}

// unwrap(event, "payload.event.*.chat.id")
static int unwrap(lua_State *L) {
    luaF_need_args(L, 2, "unwrap"); // value, path
    luaL_checktype(L, 2, LUA_TSTRING);
    lua_insert(L, 1); // path, value

    int value_idx = 2;

    size_t path_len;
    const char *path = lua_tolstring(L, 1, &path_len);

    while (path_len > 0) {
        if (unlikely(!lua_istable(L, value_idx))) {
            return 0; // can read fields only from table. fail
        }

        const char *pos = memchr(path, '.', path_len);

        if (unlikely(!pos)) { // no dots
            lua_pushlstring(L, path, path_len);
            lua_rawget(L, value_idx);
            return 1;
        }

        const char *chunk = path;
        size_t chunk_len = pos - path;

        if (unlikely(chunk_len == 1 && *chunk == '*')) {
            path += 2;
            path_len -= 2;

            lua_pushnil(L);
            while (lua_next(L, value_idx)) {
                if (path_len > 0 && !lua_istable(L, -1)) {
                    lua_pop(L, 1); // lua_next
                    continue; // can't read from value, skip
                }

                lua_pushcfunction(L, unwrap); // k, v, fn
                lua_insert(L, lua_gettop(L) - 1); // k, fn, v
                lua_pushlstring(L, path, path_len); // fn, v, path
                lua_call(L, 2, 1);

                if (!lua_isnoneornil(L, -1)) {
                    return 1; // found
                }

                lua_pop(L, 1); // lua_next
            }

            return 0; // could not find. fail
        }

        lua_pushlstring(L, chunk, chunk_len);
        lua_rawget(L, value_idx);
        lua_replace(L, value_idx);

        path += chunk_len + 1; // 1 for dot
        path_len -= chunk_len + 1; // 1 for dot
    }

    return 1;
}
