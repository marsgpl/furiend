#ifndef FURIEND_STRBUF_H
#define FURIEND_STRBUF_H

#include "shared.h"

typedef struct {
    char *buf;
    size_t filled;
    size_t capacity;
} luaF_strbuf;

luaF_strbuf luaF_strbuf_create(size_t capacity);
void luaF_strbuf_free_buf(luaF_strbuf *sb);

void luaF_strbuf_ensure_space(
    lua_State *L,
    luaF_strbuf *sb,
    size_t additional_space);

void luaF_strbuf_append(
    lua_State *L,
    luaF_strbuf *sb,
    const char *data,
    size_t data_len);

ssize_t luaF_strbuf_recv(
    lua_State *L,
    luaF_strbuf *sb,
    int fd,
    int flags);

void luaF_strbuf_shift(
    lua_State *L,
    luaF_strbuf *sb,
    size_t shift_bytes);

#endif
