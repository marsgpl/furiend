#ifndef LUA_LIB_REDIS_RESP_H
#define LUA_LIB_REDIS_RESP_H

#define _GNU_SOURCE

#include <string.h>
#include <furiend/shared.h>
#include <furiend/strbuf.h>

#define RESP_SEP "\r\n"
#define RESP_SEP_LEN 2

// https://redis.io/docs/latest/develop/reference/protocol-spec/

// RESP2
#define RESP_STR '+'
#define RESP_ERR '-'
#define RESP_INT ':'
#define RESP_BULK '$'
#define RESP_ARR '*'
// RESP3
#define RESP_NULL '_'
#define RESP_BOOL '#'
#define RESP_DOUBLE ','
#define RESP_BIG_NUM '('
#define RESP_BULK_ERR '!'
#define RESP_VSTR '='
#define RESP_MAP '%'
#define RESP_ATTR '|'
#define RESP_SET '~'
#define RESP_PUSH '>'

void resp_pack(
    lua_State *L,
    luaF_strbuf *sb,
    int index);

size_t resp_unpack(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    int push_type);

#endif
