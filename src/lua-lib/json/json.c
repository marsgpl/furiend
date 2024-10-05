#include "json.h"

LUAMOD_API int luaopen_json(lua_State *L) {
    luaL_newlib(L, json_index);
    return 1;
}

int json_parse(lua_State *L) {
    if (lua_gettop(L) != 1) {
        fail(L, "need 1 argument; provided: %d", lua_gettop(L));
        return 2;
    }

    if (lua_type(L, 1) != LUA_TSTRING) {
        fail(L, "arg #1 must be string; provided: %s", luaL_typename(L, 1));
        return 2;
    }

    size_t len;
    const char *json = lua_tolstring(L, 1, &len);
    yyjson_doc *doc = yyjson_read(json, len, 0);

    lua_pushboolean(L, 1);
    json_parse_value(L, yyjson_doc_get_root(doc));
    yyjson_doc_free(doc);

    // true, value
    // false, errmsg
    return 2;
}

static int json_parse_value(lua_State *L, yyjson_val *value) {
    yyjson_type type = yyjson_get_type(value);

    switch (type) {
        case YYJSON_TYPE_NULL:
            lua_pushnil(L);
            return 1;
        case YYJSON_TYPE_BOOL:
            lua_pushboolean(L,
                yyjson_get_subtype(value) == YYJSON_SUBTYPE_TRUE);
            return 1;
        case YYJSON_TYPE_NUM:
            switch (yyjson_get_subtype(value)) {
                case YYJSON_SUBTYPE_UINT:
                    lua_pushinteger(L, yyjson_get_uint(value));
                    return 1;
                case YYJSON_SUBTYPE_SINT:
                    lua_pushinteger(L, yyjson_get_sint(value));
                    return 1;
                default: // YYJSON_SUBTYPE_REAL
                    lua_pushnumber(L, yyjson_get_real(value));
                    return 1;
            }
        case YYJSON_TYPE_STR:
            lua_pushlstring(L, yyjson_get_str(value), yyjson_get_len(value));
            return 1;
        case YYJSON_TYPE_ARR: {
            luaL_checkstack(L, lua_gettop(L) + 3, "json.parse");
            lua_createtable(L, yyjson_arr_size(value), 0);

            size_t idx, max;
            yyjson_val *val;

            yyjson_arr_foreach(value, idx, max, val) {
                if (json_parse_value(L, val) == 2) return 2; // fail
                lua_rawseti(L, -2, idx + 1); // lua table indexes start from 1
            }

            return 1;
        } case YYJSON_TYPE_OBJ: {
            luaL_checkstack(L, lua_gettop(L) + 3, "json.parse");
            lua_createtable(L, 0, yyjson_obj_size(value));

            size_t idx, max;
            yyjson_val *key, *val;

            yyjson_obj_foreach(value, idx, max, key, val) {
                if (json_parse_value(L, key) == 2) return 2; // fail
                if (json_parse_value(L, val) == 2) return 2; // fail
                lua_rawset(L, -3);
            }

            return 1;
        } default: // YYJSON_TYPE_NONE YYJSON_TYPE_RAW
            fail(L, "unsupported yyjson type: %d", type);
            return 2;
    }
}

static int is_array(lua_State *L, int index) { // type is already LUA_TTABLE
    int next_idx = 1;

    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (!lua_isinteger(L, -2) || lua_tointeger(L, -2) != next_idx++) {
            lua_pop(L, 2); // lua_next (key, value)
            return 0;
        }

        lua_pop(L, 1); // lua_next (value)
    }

    return next_idx - 1; // return array len, 0 - not array
}

static yyjson_mut_val *json_stringify_table(
    lua_State *L,
    int index,
    yyjson_mut_doc *doc
) {
    luaL_checkstack(L, lua_gettop(L) + 3, "json.stringify");

    if (is_array(L, index)) {
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        yyjson_mut_val *val;

        lua_pushnil(L);
        while (lua_next(L, index)) {
            val = json_stringify_value(L, index + 2, doc);
            yyjson_mut_arr_add_val(arr, val);
            lua_pop(L, 1); // lua_next
        }

        return arr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_val *val;

    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (lua_type(L, -2) != LUA_TSTRING) {
            fail(L, "key is not a string; type: %s",
                luaL_typename(L, -2));
            return NULL;
        }

        size_t key_len;
        const char *key = lua_tolstring(L, index + 1, &key_len);
        val = json_stringify_value(L, index + 2, doc);

        yyjson_mut_obj_add_keyn_val(doc, obj, key, key_len, val);

        lua_pop(L, 1); // lua_next
    }

    return obj;
}

static yyjson_mut_val *json_stringify_value(
    lua_State *L,
    int index,
    yyjson_mut_doc *doc
) {
    int type = lua_type(L, index);

    switch (type) {
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
            return json_stringify_table(L, index, doc);
        default: // LUA_TFUNCTION LUA_TTHREAD LUA_TUSERDATA LUA_TLIGHTUSERDATA
            fail(L, "%s type is not supported", lua_typename(L, type));
            return NULL;
    }
}

int json_stringify(lua_State *L) {
    if (lua_gettop(L) != 1) {
        fail(L, "need 1 argument; provided: %d", lua_gettop(L));
        return 2;
    }

    yyjson_write_err err;
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *value = json_stringify_value(L, 1, doc);

    if (!value) { // fail
        yyjson_mut_doc_free(doc);
        return 2;
    }

    yyjson_mut_doc_set_root(doc, value);

    const char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);

    if (!json) {
        fail(L, "json.stringify failed: %s (%d)", err.msg, err.code);
        yyjson_mut_doc_free(doc);
        return 2;
    }

    lua_pushboolean(L, 1);
    lua_pushstring(L, json);

    free((void *)json);
    yyjson_mut_doc_free(doc);

    return 2;
}
