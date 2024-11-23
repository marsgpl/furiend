#include "resp.h"

static size_t parse_len(const char *buf, size_t buf_len, int64_t *len) {
    const char *first = buf + 1; // skip first "type" byte
    const char *last = buf + buf_len - RESP_SEP_LEN;

    int neg = *first == '-';
    int pos = *first == '+';
    first += (neg + pos);

    *len = 0;

    while (first <= last && *first != '\r') {
        *len = (*len * 10) + (*first - '0');
        first++;
    }

    if (likely(*first == '\r')) {
        size_t parsed = first - buf + RESP_SEP_LEN;

        if (unlikely(parsed > buf_len)) {
            return 0;
        }

        if (unlikely(neg)) {
            *len *= -1;
        }

        return parsed;
    }

    return 0;
}

// +OK\r\n
// -Error message\r\n
// ([+|-]<number>\r\n
static size_t parse_string(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int type
) {
    char *sep = memmem(buf + 1, buf_len - 1, RESP_SEP, RESP_SEP_LEN);

    if (unlikely(sep == NULL)) {
        return 0;
    }

    lua_pushlstring(L, buf + 1, sep - buf - 1);
    if (type) lua_pushinteger(L, type);

    return sep - buf + RESP_SEP_LEN;
}

// :[<+|->]<value>\r\n
static size_t parse_int(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int type
) {
    int64_t integer;
    size_t parsed = parse_len(buf, buf_len, &integer);

    if (unlikely(parsed == 0)) {
        return 0;
    }

    lua_pushinteger(L, integer);
    if (type) lua_pushinteger(L, type);

    return parsed;
}

// !<length>\r\n<error>\r\n
// !21\r\nSYNTAX invalid syntax\r\n
// =<length>\r\n<encoding>:<data>\r\n
// =15\r\ntxt:Some string\r\n
// $<length>\r\n<data>\r\n
// $-1\r\n
// $0\r\n\r\n
// $5\r\nhello\r\n
static size_t parse_bulk(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int type
) {
    int64_t len;
    size_t parsed = parse_len(buf, buf_len, &len);

    if (unlikely(parsed == 0)) {
        return 0;
    }

    if (unlikely(len == -1)) { // null
        lua_pushnil(L);
        if (type) lua_pushinteger(L, RESP_NULL);
        return parsed;
    }

    size_t full_len = parsed + len + RESP_SEP_LEN;

    if (unlikely(buf_len < full_len)) {
        return 0; // data is not fully received yet
    }

    lua_pushlstring(L, buf + parsed, len);
    if (type) lua_pushinteger(L, type);

    return full_len;
}

// %<number-of-entries>\r\n<key-1><value-1>...<key-n><value-n>
// %7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n7.4.1\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:137\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n
// %0\r\n
// |1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n
static size_t parse_map(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int type
) {
    int64_t len;
    size_t parsed = parse_len(buf, buf_len, &len);
    size_t total_parsed = parsed;

    if (unlikely(total_parsed == 0)) {
        return 0;
    }

    if (unlikely(len == -1)) { // null
        lua_pushnil(L);
        if (type) lua_pushinteger(L, RESP_NULL);
        return parsed;
    }

    lua_createtable(L, 0, len);

    for (int i = 0; i < len; ++i) {
        parsed = resp_unpack(L, buf + total_parsed, buf_len - total_parsed, 0);

        if (unlikely(parsed == 0)) {
            lua_pop(L, 1); // lua_createtable
            return 0;
        }

        total_parsed += parsed;

        parsed = resp_unpack(L, buf + total_parsed, buf_len - total_parsed, 0);

        if (unlikely(parsed == 0)) {
            lua_pop(L, 2); // map, kd
            return 0;
        }

        total_parsed += parsed;

        lua_rawset(L, -3); // map[key] = value
    }

    if (unlikely(buf[0] == RESP_ATTR)) {
        size_t rest_len = buf_len - total_parsed;

        if (unlikely(rest_len < 3)) {
            lua_pop(L, 1); // rm attr lua_createtable
            return 0; // next value is not received yet
        }

        int next_type = buf[total_parsed];

        if (unlikely(next_type == RESP_ATTR)) {
            luaL_error(L, "2 attr maps in a row");
        }

        int next_is_table = next_type == RESP_ARR
            || next_type == RESP_SET
            || next_type == RESP_PUSH
            || next_type == RESP_MAP;

        if (unlikely(!next_is_table)) {
            lua_pop(L, 1); // remove attr table bc we can't set it as metatable
        }

        parsed = resp_unpack(L,
            buf + total_parsed,
            rest_len,
            type ? next_type : 0);

        if (unlikely(parsed == 0)) {
            if (likely(next_is_table)) {
                lua_pop(L, 1); // rm attr lua_createtable
            }
            return 0; // next value is not received yet
        }

        if (likely(next_is_table)) {
            if (type) { // attr, table, type
                lua_insert(L, lua_gettop(L) - 2); // type, attr, table
                lua_insert(L, lua_gettop(L) - 1); // type, table, attr
                lua_setmetatable(L, -2); // attr >> table
                lua_insert(L, lua_gettop(L) - 1); // table, type
            } else { // attr, table
                lua_insert(L, lua_gettop(L) - 1); // table, attr
                lua_setmetatable(L, -2); // attr >> table
            }
        }

        return total_parsed + parsed;
    } else if (type) {
        lua_pushinteger(L, type);
    }

    return total_parsed;
}

// *<number-of-elements>\r\n<element-1>...<element-n>
// ~<number-of-elements>\r\n<element-1>...<element-n>
// ><number-of-elements>\r\n<element-1>...<element-n>
// *0\r\n
// *2\r\n$5\r\nhello\r\n$5\r\nworld\r\n
// *3\r\n:1\r\n:2\r\n:3\r\n
static size_t parse_arr(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int type
) {
    int64_t len;
    size_t parsed = parse_len(buf, buf_len, &len);
    size_t total_parsed = parsed;

    if (unlikely(total_parsed == 0)) {
        return 0;
    }

    if (unlikely(len == -1)) { // null
        lua_pushnil(L);
        if (type) lua_pushinteger(L, RESP_NULL);
        return parsed;
    }

    int next_idx = 1;

    lua_createtable(L, len, 0);

    for (int i = 0; i < len; ++i) {
        parsed = resp_unpack(L, buf + total_parsed, buf_len - total_parsed, 0);

        if (unlikely(parsed == 0)) {
            lua_pop(L, 1); // lua_createtable
            return 0;
        }

        total_parsed += parsed;

        lua_rawseti(L, -2, next_idx++); // arr[i] = value
    }

    if (type) lua_pushinteger(L, type);

    return total_parsed;
}

// _\r\n
static size_t parse_null(lua_State *L, int type) {
    lua_pushnil(L);
    if (type) lua_pushinteger(L, type);
    return 3;
}

// #<t|f>\r\n
static size_t parse_bool(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int type
) {
    if (buf_len < 4) {
        return 0;
    }

    lua_pushboolean(L, buf[1] == 't');
    if (type) lua_pushinteger(L, type);

    return 4;
}

// ,[<+|->]<integral>[.<fractional>][<E|e>[sign]<exponent>]\r\n
// ,1.23\r\n
// ,inf\r\n
// ,-inf\r\n
// ,nan\r\n
static size_t parse_double(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int type
) {
    char *sep = memmem(buf + 1, buf_len - 1, RESP_SEP, RESP_SEP_LEN);

    if (unlikely(sep == NULL)) {
        return 0;
    }

    lua_Number value;
    int scanned_n = sscanf(buf + 1, "%lf", &value);

    if (scanned_n != 1) {
        luaF_error_errno(L, "sscanf failed for: %s",
            luaF_escape_string(L, buf, buf_len, 32));
    }

    lua_pushnumber(L, value);
    if (type) lua_pushinteger(L, type);

    return sep - buf + RESP_SEP_LEN;
}

size_t resp_unpack(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int push_type
) {
    if (unlikely(buf_len < 3)) {
        return 0;
    }

    int type = buf[0];

    if (push_type) {
        push_type = type;
    }

    switch (type) { // buf_len guaranteed > 2
        case RESP_ERR:
        case RESP_STR:
        case RESP_BIG_NUM:
            return parse_string(L, buf, buf_len, push_type);
        case RESP_INT:
            return parse_int(L, buf, buf_len, push_type);
        case RESP_BULK:
        case RESP_BULK_ERR:
        case RESP_VSTR:
            return parse_bulk(L, buf, buf_len, push_type);
        case RESP_ARR:
        case RESP_SET:
        case RESP_PUSH:
            return parse_arr(L, buf, buf_len, push_type);
        case RESP_NULL:
            return parse_null(L, push_type);
        case RESP_BOOL:
            return parse_bool(L, buf, buf_len, push_type);
        case RESP_MAP:
        case RESP_ATTR:
            return parse_map(L, buf, buf_len, push_type);
        case RESP_DOUBLE:
            return parse_double(L, buf, buf_len, push_type);
    }

    return luaL_error(L, "unsupported packet: %s (#%d)",
        luaF_escape_string(L, buf, buf_len, 32), buf_len);
}

// <type><length>\r\n
static void pack_len(
    lua_State *L,
    luaF_strbuf *sb,
    unsigned char type,
    lua_Integer len
) {
    size_t full_len = 1 + int_len(len) + RESP_SEP_LEN;

    luaF_strbuf_ensure_space(L, sb, full_len + 1);

    int written = snprintf(sb->buf + sb->filled, full_len + 1,
        "%c%lld" RESP_SEP,
        type, len);

    if (unlikely((size_t)written != full_len)) {
        luaL_error(L, "pack_string snprintf failed; written: %d of %d",
            written, full_len);
    }

    sb->filled += full_len;
}

// _\r\n
static void pack_null(lua_State *L, luaF_strbuf *sb) {
    size_t len = 1 + RESP_SEP_LEN;

    luaF_strbuf_ensure_space(L, sb, len);

    memcpy(sb->buf + sb->filled, "_" RESP_SEP, len);

    sb->filled += len;
}

// #<t|f>\r\n
static void pack_bool(lua_State *L, luaF_strbuf *sb, int index) {
    size_t len = 1 + 1 + RESP_SEP_LEN;

    luaF_strbuf_ensure_space(L, sb, len);

    if (lua_toboolean(L, index)) {
        memcpy(sb->buf + sb->filled, "#t" RESP_SEP, len);
    } else {
        memcpy(sb->buf + sb->filled, "#f" RESP_SEP, len);
    }

    sb->filled += len;
}

// $<length>\r\n<data>\r\n
static void pack_string(lua_State *L, luaF_strbuf *sb, int index) {
    size_t str_len;
    const char *str = lua_tolstring(L, index, &str_len);

    pack_len(L, sb, '$', str_len);

    size_t len = str_len + RESP_SEP_LEN;

    luaF_strbuf_ensure_space(L, sb, len);

    memcpy(sb->buf + sb->filled, str, str_len);
    memcpy(sb->buf + sb->filled + str_len, RESP_SEP, RESP_SEP_LEN);

    sb->filled += len;
}

// :[<+|->]<value>\r\n
// ,[<+|->]<integral>[.<fractional>][<E|e>[sign]<exponent>]\r\n
static void pack_number(lua_State *L, luaF_strbuf *sb, int index) {
    if (lua_isinteger(L, index)) {
        lua_Integer number = lua_tointeger(L, index);
        pack_len(L, sb, ':', number);
    } else {
        lua_Number number = lua_tonumber(L, index);

        size_t max_len = 1 + 15 + RESP_SEP_LEN;

        luaF_strbuf_ensure_space(L, sb, max_len + 1);

        int written = snprintf(sb->buf + sb->filled, max_len + 1,
            "," LUA_NUMBER_FMT RESP_SEP,
            number);

        sb->filled += written;
    }
}

// *<number-of-elements>\r\n<element-1>...<element-n>
static void pack_array(lua_State *L, luaF_strbuf *sb, int index, int keys_n) {
    pack_len(L, sb, '*', keys_n);

    lua_pushnil(L);
    while (lua_next(L, index)) {
        resp_pack(L, sb, index + 2);
        lua_pop(L, 1); // lua_next
    }
}

// %<number-of-entries>\r\n<key-1><value-1>...<key-n><value-n>
static void pack_map(lua_State *L, luaF_strbuf *sb, int index, int keys_n) {
    pack_len(L, sb, '%', keys_n);

    lua_pushnil(L);
    while (lua_next(L, index)) {
        resp_pack(L, sb, index + 1);
        resp_pack(L, sb, index + 2);
        lua_pop(L, 1); // lua_next
    }
}

static void pack_table(lua_State *L, luaF_strbuf *sb, int index) {
    int is_array = 1;
    int next_idx = 1;

    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (unlikely(is_array && (
            !lua_isinteger(L, -2) || lua_tointeger(L, -2) != next_idx
        ))) {
            is_array = 0;
        }

        next_idx++;
        lua_pop(L, 1); // lua_next
    }

    if (likely(is_array)) {
        pack_array(L, sb, index, next_idx - 1);
    } else {
        pack_map(L, sb, index, next_idx - 1);
    }
}

void resp_pack(lua_State *L, luaF_strbuf *sb, int index) {
    switch (lua_type(L, index)) {
        case LUA_TNONE:
        case LUA_TNIL:
            pack_null(L, sb);
            return;
        case LUA_TBOOLEAN:
            pack_bool(L, sb, index);
            return;
        case LUA_TNUMBER:
            pack_number(L, sb, index);
            return;
        case LUA_TSTRING:
            pack_string(L, sb, index);
            return;
        case LUA_TTABLE:
            pack_table(L, sb, index);
            return;
        default: // LUA_TLIGHTUSERDATA LUA_TFUNCTION LUA_TUSERDATA LUA_TTHREAD
            luaL_error(L, "type not supported: %s",
                luaL_typename(L, index));
    }
}
