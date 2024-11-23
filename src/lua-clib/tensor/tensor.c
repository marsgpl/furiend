#include "tensor.h"

LUAMOD_API int luaopen_tensor(lua_State *L) {
    luaL_newlib(L, tensor_index);
    return 1;
}

int tensor_gen_id(lua_State *L) {
    luaF_need_args(L, 2, "gen_id");
    luaL_checktype(L, 1, LUA_TSTRING); // prefix
    luaL_checktype(L, 2, LUA_TTABLE); // db

    const char *prefix = lua_tostring(L, 1);
    struct timespec ts;

    while (1) {
        if (unlikely(clock_gettime(CLOCK_REALTIME, &ts) < 0)) {
            luaF_error_errno(L, "clock_gettime failed");
        }

        lua_pushfstring(L, "%s" GEN_ID_SEP "%d" GEN_ID_SEP "%d",
            prefix, ts.tv_sec, ts.tv_nsec);
        lua_pushvalue(L, 3); // prefix, db, id, id

        if (lua_rawget(L, 2) != LUA_TTABLE) { // prefix, db, id, nil
            lua_pop(L, 1); // prefix, db, id
            return 1; // id
        } else { // prefix, db, id, obj
            // dup id, try again
            lua_settop(L, 2); // prefix, db
        }
    }
}

// unwrap(event, "payload.event.*.chat.id")
int tensor_unwrap(lua_State *L) {
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

                lua_pushcfunction(L, tensor_unwrap); // k, v, fn
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
