#include "json.h"

LUAMOD_API int luaopen_json(lua_State *L) {
    luaL_newlib(L, json_index);
    return 1;
}

int json_parse(lua_State *L) {
    if (lua_gettop(L) != 1) {
        FAIL(L, "need 1 argument; provided: %d", lua_gettop(L));
    }

    if (lua_type(L, 1) != LUA_TSTRING) {
        FAIL(L, "arg #1 must be string; provided: %s", luaL_typename(L, 1));
    }

    size_t len;
    const char *json = lua_tolstring(L, 1, &len);
    yyjson_doc *doc = yyjson_read(json, len, 0);

    lua_pushboolean(L, 1);
    parse_value(L, yyjson_doc_get_root(doc));
    yyjson_doc_free(doc);

    // true, value
    // false, errmsg
    return 2;
}

static int parse_value(lua_State *L, yyjson_val *value) {
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
            luaL_checkstack(L, 4, "json.parse");
            lua_createtable(L, yyjson_arr_size(value), 0);

            size_t idx, max;
            yyjson_val *val;

            yyjson_arr_foreach(value, idx, max, val) {
                if (parse_value(L, val) == 2) return 2; // fail
                lua_rawseti(L, -2, idx + 1); // lua table indexes start from 1
            }

            return 1;
        } case YYJSON_TYPE_OBJ: {
            luaL_checkstack(L, 4, "json.parse");
            lua_createtable(L, 0, yyjson_obj_size(value));

            size_t idx, max;
            yyjson_val *key, *val;

            yyjson_obj_foreach(value, idx, max, key, val) {
                if (parse_value(L, key) == 2) return 2; // fail
                if (parse_value(L, val) == 2) return 2; // fail
                lua_rawset(L, -3);
            }

            return 1;
        } default: // YYJSON_TYPE_NONE YYJSON_TYPE_RAW
            FAIL(L, "failed to parse");
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

int json_stringify(lua_State *L) {
    if (lua_gettop(L) != 1) {
        FAIL(L, "need 1 argument; provided: %d", lua_gettop(L));
    }

    static char *buf = NULL;
    static size_t buf_size = 0;

    size_t added = 0;
    size_t depth = 0;

    if (lua_type(L, 1) == LUA_TTABLE) {
        lua_createtable(L, 0, 1); // cache
        lua_insert(L, 1);
    }

    if (stringify_value(L, lua_gettop(L), &added, &buf, &buf_size, depth)) {
        return 2;
    }

    lua_pushboolean(L, 1);
    lua_pushlstring(L, buf, added);
    return 2;
}

static __inline__ int stringify_boolean(
    lua_State *L,
    int index,
    size_t *added,
    char **buf,
    size_t *buf_size
) {
    if (lua_toboolean(L, index)) {
        PUT_TRUE();
        return 0;
    }

    PUT_FALSE();
    return 0;
}

static __inline__ int stringify_number(
    lua_State *L,
    int index,
    size_t *added,
    char **buf,
    size_t *buf_size
) {
    if (lua_isinteger(L, index)) {
        lua_Integer integer = lua_tointeger(L, index);
        PUT_NUMBER(LUA_INTEGER_FMT, integer);
        return 0;
    }

    lua_Number number = lua_tonumber(L, index);

    if (!isfinite(number)) {
        lua_warning(L, "json: converting inf/nan to -1", 0);
        PUT_NUMBER(LUA_INTEGER_FMT, -1LL);
        return 0;
    }

    PUT_NUMBER(LUA_NUMBER_FMT, number);
    return 0;
}

static __inline__ int stringify_string(
    lua_State *L,
    int index,
    size_t *added,
    char **buf,
    size_t *buf_size
) {
    size_t str_len;
    const char *str = lua_tolstring(L, index, &str_len);

    if (!str_len) {
        PUT_EMPTY_STRING();
        return 0;
    }

    PUT_C('"');

    for (size_t i = 0; i < str_len; ++i) {
        unsigned char c = str[i];

        switch (c) {
            case '\\': PUT_ESC('\\'); break;
            case '"': PUT_ESC('"'); break;
            case 'b': PUT_ESC('b'); break;
            case 'f': PUT_ESC('f'); break;
            case 'n': PUT_ESC('n'); break;
            case 'r': PUT_ESC('r'); break;
            case 't': PUT_ESC('t'); break;
            case  0: case  1: case  2: case  3: case  4: case  5:
            case  6: case  7: case  8: case  9: case 10: case 11:
            case 12: case 13: case 14: case 15: case 16: case 17:
            case 18: case 19: case 20: case 21: case 22: case 23:
            case 24: case 25: case 26: case 27: case 28: case 29:
            case 30: case 31: case 0x7f: PUT_UTF16(c); break;
            default: PUT_C(c); break;
        }
    }

    PUT_C('"');

    return 0;
}

static __inline__ int stringify_table(
    lua_State *L,
    int index,
    size_t *added,
    char **buf,
    size_t *buf_size,
    int depth
) {
    int is_arr = is_array(L, index);

    if (lua_rawgetp(L, CACHE_INDEX, lua_topointer(L, index))) {
        lua_warning(L, "json: ignoring circular contents", 0);

        if (is_arr) {
            PUT_EMPTY_ARRAY();
            return 0;
        }

        PUT_EMPTY_OBJECT();
        return 0;
    }

    lua_pop(L, 1); // lua_rawgetp

    lua_pushboolean(L, 1);
    lua_rawsetp(L, CACHE_INDEX, lua_topointer(L, index));

    int written = 0;

    if (is_arr) {
        PUT_C('[');

        luaL_checkstack(L, 4, "json.stringify");
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
            if (written) {
                PUT_C(',');
            } else {
                written = 1;
            }

            if (stringify_value(L, index+2, added, buf, buf_size, depth+1)) {
                return 2;
            }

            lua_pop(L, 1); // lua_next
        }

        PUT_C(']');

        return 0;
    }

    // object

    PUT_C('{');

    luaL_checkstack(L, 4, "json.stringify");
    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (written) {
            PUT_C(',');
        } else {
            written = 1;
        }

        int type = lua_type(L, index + 1); // key

        if (type != LUA_TSTRING) {
            lua_pushvalue(L, index + 1);

            size_t str_len;
            const char *str = lua_tolstring(L, index + 3, &str_len);

            if (!str) {
                FAIL(L, "table key must be a string; type: %s",
                    lua_typename(L, type));
            } else {
                luaF_warning(L, "json.stringify: table key converted to string"
                    "; type: %s", lua_typename(L, type));
            }

            if (stringify_value(L, index+3, added, buf, buf_size, depth+1)) {
                return 2; // soft error
            }

            lua_pop(L, 1);
        } else {
            if (stringify_value(L, index+1, added, buf, buf_size, depth+1)) {
                return 2; // soft error
            }
        }

        PUT_C(':');

        if (stringify_value(L, index+2, added, buf, buf_size, depth+1)) {
            return 2; // soft error
        }

        lua_pop(L, 1); // lua_next
    }

    PUT_C('}');

    return 0;
}

static int stringify_value(
    lua_State *L,
    int index,
    size_t *added,
    char **buf,
    size_t *buf_size,
    size_t depth
) {
    if (lua_gettop(L) >= MAX_STRINGIFY_DEPTH) {
        FAIL(L, "max stringify depth exceeded: %d", MAX_STRINGIFY_DEPTH);
    }

    int type = lua_type(L, index);

    switch (type) {
        case LUA_TBOOLEAN:
            return stringify_boolean(L, index, added, buf, buf_size);
        case LUA_TSTRING:
            return stringify_string(L, index, added, buf, buf_size);
        case LUA_TNUMBER:
            return stringify_number(L, index, added, buf, buf_size);
        case LUA_TTABLE:
            return stringify_table(L, index, added, buf, buf_size, depth);
        default:
            PUT_NULL();
            return 0;
    }
}
