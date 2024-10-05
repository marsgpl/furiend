#ifndef LUA_LIB_JSON_H
#define LUA_LIB_JSON_H

#include <math.h>
#include <yyjson.h> // for parsing
#include <furiend/lib.h>

#define CACHE_INDEX 1 // cache table index for stringify
#define MAX_STRINGIFY_DEPTH 32768
#define BUF_MIN_SIZE 8192
#define MAX_NUM_LEN 64

#define FAIL(L, msg, ...) { \
    lua_pushfstring(L, msg __VA_OPT__(,) __VA_ARGS__); \
    lua_pushboolean(L, 0); \
    lua_insert(L, -2); \
    return 2; \
}

#define CHECK_BUF(need_size) { \
    if (need_size > *buf_size) { \
        size_t new_buf_size = *buf_size ? *buf_size * 2 : BUF_MIN_SIZE; \
        while (new_buf_size < need_size) { \
            new_buf_size *= 2; \
        } \
        char *new_buf = realloc(*buf, new_buf_size); \
        if (!new_buf) { \
            FAIL(L, "json.stringify realloc failed; size: %d", new_buf_size); \
        } \
        *buf = new_buf; \
        *buf_size = new_buf_size; \
    } \
}

#define PUT_NUMBER(format, number) { \
    CHECK_BUF((*added) + MAX_NUM_LEN); \
    int written = snprintf(&(*buf)[*added], MAX_NUM_LEN, format, number); \
    if (written < 1) { \
        FAIL(L, "snprintf failed; format: %s; number: %d", format, number); \
    } \
    (*added) += written; \
}

#define PUT_NULL() { \
    CHECK_BUF((*added) + 4); \
    (*buf)[(*added)    ] = 'n'; \
    (*buf)[(*added) + 1] = 'u'; \
    (*buf)[(*added) + 2] = 'l'; \
    (*buf)[(*added) + 3] = 'l'; \
    (*added) += 4; \
}

#define PUT_TRUE() { \
    CHECK_BUF((*added) + 4); \
    (*buf)[(*added)    ] = 't'; \
    (*buf)[(*added) + 1] = 'r'; \
    (*buf)[(*added) + 2] = 'u'; \
    (*buf)[(*added) + 3] = 'e'; \
    (*added) += 4; \
}

#define PUT_FALSE() { \
    CHECK_BUF((*added) + 5); \
    (*buf)[(*added)    ] = 'f'; \
    (*buf)[(*added) + 1] = 'a'; \
    (*buf)[(*added) + 2] = 'l'; \
    (*buf)[(*added) + 3] = 's'; \
    (*buf)[(*added) + 4] = 'e'; \
    (*added) += 5; \
}

#define PUT_EMPTY_STRING() { \
    CHECK_BUF((*added) + 2); \
    (*buf)[(*added)    ] = '"'; \
    (*buf)[(*added) + 1] = '"'; \
    (*added) += 2; \
}

#define PUT_EMPTY_OBJECT() { \
    CHECK_BUF((*added) + 2); \
    (*buf)[(*added)    ] = '{'; \
    (*buf)[(*added) + 1] = '}'; \
    (*added) += 2; \
}

#define PUT_EMPTY_ARRAY() { \
    CHECK_BUF((*added) + 2); \
    (*buf)[(*added)    ] = '['; \
    (*buf)[(*added) + 1] = ']'; \
    (*added) += 2; \
}

#define PUT_ESC(c) { \
    CHECK_BUF((*added) + 2); \
    (*buf)[(*added)    ] = '\\'; \
    (*buf)[(*added) + 1] = c; \
    (*added) += 2; \
}

#define PUT_C(c) { \
    CHECK_BUF((*added) + 1); \
    (*buf)[(*added)++] = c; \
}

#define PUT_UTF16(c) { \
    CHECK_BUF((*added) + 6); \
    sprintf(&(*buf)[(*added)], "\\u%04x", c); \
    (*added) += 6; \
}

LUAMOD_API int luaopen_json(lua_State *L);

int json_parse(lua_State *L);
int json_stringify(lua_State *L);

static int is_array(lua_State *L, int index);
static int parse_value(lua_State *L, yyjson_val *value);

static int stringify_value(
    lua_State *L,
    int index,
    size_t *added,
    char **buf,
    size_t *buf_size,
    size_t depth);

static const luaL_Reg json_index[] = {
    { "parse", json_parse },
    { "stringify", json_stringify },
    { NULL, NULL }
};

#endif
