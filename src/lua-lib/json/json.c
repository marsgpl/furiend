#include "json.h"

LUAMOD_API int luaopen_json(lua_State *L) {
    luaL_newlib(L, json_index);
    return 1;
}

int json_parse(lua_State *L) {
    if (lua_gettop(L) != 1) {
        fail(L, "need 1 argument; provided: %d", lua_gettop(L));
    }

    if (lua_type(L, 1) != LUA_TSTRING) {
        fail(L, "arg #1 must be string; provided: %s", luaL_typename(L, 1));
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

            int next_index = 1;
            size_t idx, max;
            yyjson_val *el;

            yyjson_arr_foreach(value, idx, max, el) {
                if (json_parse_value(L, el) == 2) return 2;
                lua_rawseti(L, -2, next_index++);
            }

            return 1;
        } case YYJSON_TYPE_OBJ:
            lua_createtable(L, 0, yyjson_obj_size(value));
            return 1;
        default: // YYJSON_TYPE_NONE YYJSON_TYPE_RAW
            fail(L, "unsupported yyjson type: %d", type);
    }
}

int json_stringify(lua_State *L) {
    luaF_need_args(L, 1, "json.stringify");

    int index = 1;
    int type = lua_type(L, index);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root;

    switch (type) {
        case LUA_TNONE:
        case LUA_TNIL:
            root = yyjson_mut_null(doc);
            break;
        case LUA_TBOOLEAN:
            root = yyjson_mut_bool(doc, lua_toboolean(L, index));
            break;
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                root = yyjson_mut_int(doc, lua_tointeger(L, index));
            } else {
                root = yyjson_mut_real(doc, lua_tonumber(L, index));
            }
            break;
        case LUA_TSTRING: {
            size_t len;
            const char *str = lua_tolstring(L, index, &len);
            root = yyjson_mut_strn(doc, str, len);
            break;
        } case LUA_TTABLE:
            root = yyjson_mut_obj(doc);
            break;
        case LUA_TLIGHTUSERDATA:
        case LUA_TFUNCTION:
        case LUA_TUSERDATA:
        case LUA_TTHREAD:
            fail(L, "%s (%d) is not supported", lua_typename(L, type), type);
    }

    yyjson_mut_doc_set_root(doc, root);

    yyjson_write_err err;
    const char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);

    if (json) {
        lua_pushboolean(L, 1);
        lua_pushstring(L, json);
        free((void *)json);
    } else {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "json.stringify failed: %s (%d)",
            err.msg, err.code);
    }

    yyjson_mut_doc_free(doc);

    return 2;
}
