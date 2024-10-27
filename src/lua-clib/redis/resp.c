#include "resp.h"

static int parse_len(const char *buf, int buf_len, int *len) {
    const char *p = buf + 1; // skip first 'type' byte
    const char *end = buf + buf_len - 1; // \r

    *len = 0;

    while (p <= end) {
        if (unlikely(*p == '\r')) {
            return p + RESP_SEP_LEN - buf;
        }

        *len = (*len * 10) + (*p - '0');
        p++;
    }

    return 0;
}

// -Error message\r\n
static int parse_error(lua_State *L, const char *buf, int buf_len, int pt) {
    char *sep = memmem(buf + 1, buf_len - 1, RESP_SEP, RESP_SEP_LEN);

    if (unlikely(sep == NULL)) {
        return 0;
    }

    lua_pushlstring(L, buf + 1, sep - buf - 1);
    if (pt) lua_pushinteger(L, RESP_ERR);

    return sep + RESP_SEP_LEN - buf;
}

// +OK\r\n
static int parse_string(lua_State *L, const char *buf, int buf_len, int pt) {
    char *sep = memmem(buf + 1, buf_len - 1, RESP_SEP, RESP_SEP_LEN);

    if (unlikely(sep == NULL)) {
        return 0;
    }

    lua_pushlstring(L, buf + 1, sep - buf - 1);
    if (pt) lua_pushinteger(L, RESP_STR);

    return sep + RESP_SEP_LEN - buf;
}

// :[<+|->]<value>\r\n
static int parse_int(lua_State *L, const char *buf, int buf_len, int pt) {
    unsigned char sign = buf[1];
    int neg = sign == '-';
    int pos = sign == '+';
    int has_sign = neg || pos;

    int integer;
    int parsed = parse_len(buf + has_sign, buf_len - has_sign, &integer);

    if (unlikely(parsed == 0)) {
        return 0;
    }

    lua_pushinteger(L, integer);
    if (pt) lua_pushinteger(L, RESP_INT);

    return parsed + has_sign;
}

// $<length>\r\n<data>\r\n
static int parse_bulk(lua_State *L, const char *buf, int buf_len, int pt) {
    int len;
    int parsed = parse_len(buf, buf_len, &len);

    if (unlikely(parsed == 0)) {
        return 0;
    }

    int full_len = parsed + len + RESP_SEP_LEN;

    if (unlikely(buf_len < full_len)) {
        return 0; // data is not fully received yet
    }

    lua_pushlstring(L, buf + parsed, len);
    if (pt) lua_pushinteger(L, RESP_BULK);

    return full_len;
}

// %<number-of-entries>\r\n<key-1><value-1>...<key-n><value-n>
// %7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n7.4.1\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:137\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n
static int parse_map(lua_State *L, const char *buf, int buf_len, int pt) {
    int len;
    int parsed = parse_len(buf, buf_len, &len);
    int total_parsed = parsed;

    if (unlikely(total_parsed == 0)) {
        return 0;
    }

    lua_createtable(L, 0, len);

    for (int i = 0; i < len; ++i) {
        parsed = resp_unpack(L, buf + total_parsed, buf_len - total_parsed, 0);
        total_parsed += parsed;

        if (unlikely(parsed == 0)) {
            lua_pop(L, 1); // lua_createtable
            return 0;
        }

        parsed = resp_unpack(L, buf + total_parsed, buf_len - total_parsed, 0);
        total_parsed += parsed;

        if (unlikely(parsed == 0)) {
            lua_pop(L, 2); // map, kd
            return 0;
        }

        lua_rawset(L, -3); // map[key] = value
    }

    if (pt) lua_pushinteger(L, RESP_MAP);

    return total_parsed;
}

// *<number-of-elements>\r\n<element-1>...<element-n>
static int parse_arr(lua_State *L, const char *buf, int buf_len, int pt) {
    int len;
    int parsed = parse_len(buf, buf_len, &len);
    int total_parsed = parsed;

    if (unlikely(total_parsed == 0)) {
        return 0;
    }

    int next_idx = 1;

    lua_createtable(L, len, 0);

    for (int i = 0; i < len; ++i) {
        parsed = resp_unpack(L, buf + total_parsed, buf_len - total_parsed, 0);
        total_parsed += parsed;

        if (unlikely(parsed == 0)) {
            lua_pop(L, 1); // lua_createtable
            return 0;
        }

        lua_rawseti(L, -2, next_idx++); // arr[i] = value
    }

    if (pt) lua_pushinteger(L, RESP_ARR);

    return total_parsed;
}

// _\r\n
static int parse_null(lua_State *L, int pt) {
    lua_pushnil(L);
    if (pt) lua_pushinteger(L, RESP_NULL);
    return 3;
}

// #<t|f>\r\n
static int parse_bool(lua_State *L, const char *buf, int buf_len, int pt) {
    if (buf_len < 4) {
        return 0;
    }

    lua_pushboolean(L, buf[1] == 't');
    if (pt) lua_pushinteger(L, RESP_BOOL);

    return 4;
}

int resp_unpack(lua_State *L, const char *buf, int buf_len, int pt) {
    switch (buf[0]) { // buf_len guaranteed > 2
        case RESP_ERR: return parse_error(L, buf, buf_len, pt);
        case RESP_STR: return parse_string(L, buf, buf_len, pt);
        case RESP_INT: return parse_int(L, buf, buf_len, pt);
        case RESP_BULK: return parse_bulk(L, buf, buf_len, pt);
        case RESP_ARR: return parse_arr(L, buf, buf_len, pt);
        case RESP_NULL: return parse_null(L, pt);
        case RESP_BOOL: return parse_bool(L, buf, buf_len, pt);
        // case RESP_DOUBLE: return parse_double(L, buf, buf_len, pt);
        // case RESP_BIG_NUM: return parse_big_num(L, buf, buf_len, pt);
        // case RESP_BULK_ERR: return parse_bulk_err(L, buf, buf_len, pt);
        // case RESP_VSTR: return parse_verbatim_str(L, buf, buf_len, pt);
        case RESP_MAP: return parse_map(L, buf, buf_len, pt);
        // case RESP_ATTR: return parse_attr(L, buf, buf_len, pt);
        // case RESP_SET: return parse_set(L, buf, buf_len, pt);
        // case RESP_PUSH: return parse_push(L, buf, buf_len, pt);
    }

    return luaL_error(L, "unsupported packet: %s",
        luaF_escape_string(L, buf, buf_len, 32));
}
