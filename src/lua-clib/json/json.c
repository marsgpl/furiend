#include "json.h"

LUAMOD_API int luaopen_json(lua_State *L) {
    luaL_newlib(L, json_index);
    return 1;
}

int json_parse(lua_State *L) {
    luaF_need_args(L, 1, "json.parse");
    luaL_checktype(L, 1, LUA_TSTRING);

    size_t len;
    const char *json = lua_tolstring(L, 1, &len);

    int flags = YYJSON_READ_NOFLAG;
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)json, len, flags, NULL, &err);

    if (unlikely(!doc)) {
        luaL_error(L, "%s (%d)", err.msg, err.code);
    }

    json_parse_value(L, yyjson_doc_get_root(doc));
    yyjson_doc_free(doc);

    return 1;
}

int json_stringify(lua_State *L) {
    luaF_need_args(L, 1, "json.stringify");

    int cache_index = 0;

    if (likely(lua_type(L, 1) == LUA_TTABLE)) {
        lua_createtable(L, 0, 1);
        cache_index = lua_gettop(L);
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *value = json_stringify_value(L, doc, 1, cache_index);

    yyjson_mut_doc_set_root(doc, value);

    int flags = YYJSON_WRITE_ESCAPE_UNICODE;
    size_t json_len;
    yyjson_write_err err;
    const char *json = yyjson_mut_write_opts(doc, flags, NULL, &json_len, &err);

    if (unlikely(!json)) {
        yyjson_mut_doc_free(doc);
        luaL_error(L, "%s (%d)", err.msg, err.code);
    }

    lua_pushboolean(L, 1);
    lua_pushlstring(L, json, json_len);

    free((void *)json);
    yyjson_mut_doc_free(doc);

    return 1;
}

static void json_parse_value(lua_State *L, yyjson_val *value) {
    switch (yyjson_get_type(value)) {
        case YYJSON_TYPE_NULL:
            lua_pushnil(L);
            return;
        case YYJSON_TYPE_BOOL:
            lua_pushboolean(L,
                yyjson_get_subtype(value) == YYJSON_SUBTYPE_TRUE);
            return;
        case YYJSON_TYPE_NUM:
            switch (yyjson_get_subtype(value)) {
                case YYJSON_SUBTYPE_UINT:
                    lua_pushinteger(L, yyjson_get_uint(value));
                    return;
                case YYJSON_SUBTYPE_SINT:
                    lua_pushinteger(L, yyjson_get_sint(value));
                    return;
                default: // YYJSON_SUBTYPE_REAL
                    lua_pushnumber(L, yyjson_get_real(value));
                    return;
            }
        case YYJSON_TYPE_STR:
            lua_pushlstring(L, yyjson_get_str(value), yyjson_get_len(value));
            return;
        case YYJSON_TYPE_ARR: {
            luaL_checkstack(L, 4, "json.parse");
            lua_createtable(L, yyjson_arr_size(value), 0);

            size_t idx, max;
            yyjson_val *val;

            yyjson_arr_foreach(value, idx, max, val) {
                json_parse_value(L, val);
                lua_rawseti(L, -2, idx + 1); // lua table indexes start from 1
            }

            return;
        } case YYJSON_TYPE_OBJ: {
            luaL_checkstack(L, 4, "json.parse");
            lua_createtable(L, 0, yyjson_obj_size(value));

            size_t idx, max;
            yyjson_val *key, *val;

            yyjson_obj_foreach(value, idx, max, key, val) {
                json_parse_value(L, key);
                json_parse_value(L, val);
                lua_rawset(L, -3);
            }

            return;
        } default: // YYJSON_TYPE_NONE YYJSON_TYPE_RAW
            luaL_error(L, "failed to parse");
    }
}

static yyjson_mut_val *json_stringify_value(
    lua_State *L,
    yyjson_mut_doc *doc,
    int index,
    int cache_index
) {
    switch (lua_type(L, index)) {
        case LUA_TNONE:
        case LUA_TNIL:
            return yyjson_mut_null(doc);
        case LUA_TBOOLEAN:
            return yyjson_mut_bool(doc, lua_toboolean(L, index));
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                return yyjson_mut_int(doc, lua_tointeger(L, index));
            } else {
                return yyjson_mut_real(doc, lua_tonumber(L, index));
            }
        case LUA_TSTRING: {
            size_t len;
            const char *str = lua_tolstring(L, index, &len);
            return yyjson_mut_strn(doc, str, len);
        } case LUA_TTABLE:
            return json_stringify_table(L, doc, index, cache_index);
        default: // LUA_TFUNCTION LUA_TTHREAD LUA_TUSERDATA LUA_TLIGHTUSERDATA
            luaF_warning(L, "json.stringify: type is not supported; type: %s",
                luaL_typename(L, index));
            return yyjson_mut_null(doc);
    }
}

static yyjson_mut_val *json_stringify_table(
    lua_State *L,
    yyjson_mut_doc *doc,
    int index,
    int cache_index
) {
    luaL_checkstack(L, 4, "json.stringify");

    const void *cache_key = lua_topointer(L, index);

    if (unlikely(lua_rawgetp(L, cache_index, cache_key) != LUA_TNIL)) {
        luaL_error(L, "circular table: %p", cache_key);
    }

    lua_pop(L, 1); // lua_rawgetp
    lua_pushboolean(L, 1);
    lua_rawsetp(L, cache_index, cache_key);

    if (unlikely(luaF_is_array(L, index))) {
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        yyjson_mut_val *val;

        lua_pushnil(L);
        while (lua_next(L, index)) {
            val = json_stringify_value(L, doc, lua_gettop(L), cache_index);
            yyjson_mut_arr_add_val(arr, val);
            lua_pop(L, 1); // lua_next
        }

        return arr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_val *val;

    lua_pushnil(L);
    while (lua_next(L, index)) { // k, v
        if (unlikely(lua_type(L, -2) != LUA_TSTRING)) {
            luaL_error(L, "key is not a string; type: %s; key: %s",
                luaL_typename(L, -2),
                lua_tostring(L, -2));
        }

        size_t key_len;
        const char *key = lua_tolstring(L, lua_gettop(L) - 1, &key_len);
        val = json_stringify_value(L, doc, lua_gettop(L), cache_index);

        yyjson_mut_obj_add(obj, yyjson_mut_strn(doc, key, key_len), val);

        lua_pop(L, 1); // lua_next
    }

    return obj;
}
