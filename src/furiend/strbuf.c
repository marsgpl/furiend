#include "strbuf.h"

luaF_strbuf luaF_strbuf_create(size_t capacity) {
    luaF_strbuf sb = {
        .buf = NULL, // lazy init
        .filled = 0,
        .capacity = capacity,
    };

    return sb;
}

void luaF_strbuf_free_buf(luaF_strbuf *sb) {
    if (sb->buf != NULL) {
        free(sb->buf);
        sb->buf = NULL;
    }
}

void luaF_strbuf_ensure_space(
    lua_State *L,
    luaF_strbuf *sb,
    size_t additional_space
) {
    if (unlikely(additional_space == 0)) {
        // while it is technically possible to add zero more bytes,
        // such input is considered buggy
        luaL_error(L, "strbuf ensure space: zero space requested");
    }

    size_t new_filled = sb->filled + additional_space;

    if (unlikely(new_filled <= sb->filled)) { // overflow
        luaL_error(L, "strbuf ensure space: too much space requested;"
            "; filled: %d; requested additional: %d",
            sb->filled, additional_space);
    }

    size_t new_capacity = sb->capacity;

    while (new_capacity < new_filled) {
        new_capacity *= 2;

        if (unlikely(new_capacity <= sb->capacity)) { // overflow
            new_capacity = new_filled;
            break;
        }
    }

    if (unlikely(sb->buf == NULL || new_capacity > sb->capacity)) {
        char *new_buf = realloc(sb->buf, new_capacity);

        if (unlikely(new_buf == NULL)) {
            luaF_error_errno(L, "realloc failed; from %d; to: %d",
                sb->buf ? sb->capacity : 0,
                new_capacity);
        }

        sb->buf = new_buf;
        sb->capacity = new_capacity;
    }
}

void luaF_strbuf_append(
    lua_State *L,
    luaF_strbuf *sb,
    const char *data,
    size_t data_len
) {
    if (unlikely(data == NULL)) {
        luaL_error(L, "strbuf append: data is NULL");
    }

    luaF_strbuf_ensure_space(L, sb, data_len);

    memmove(sb->buf + sb->filled, data, data_len);

    sb->filled += data_len;
}

ssize_t luaF_strbuf_recv(
    lua_State *L,
    luaF_strbuf *sb,
    int fd,
    int flags
) {
    if (unlikely(sb->buf == NULL)) {
        luaL_error(L, "strbuf recv: buf is not allocated");
    }

    ssize_t read = recv(fd,
        sb->buf + sb->filled,
        sb->capacity - sb->filled,
        flags);

    if (likely(read > 0)) {
        sb->filled += read;
    }

    return read;
}

void luaF_strbuf_shift(
    lua_State *L,
    luaF_strbuf *sb,
    size_t shift_bytes
) {
    if (unlikely(sb->buf == NULL)) {
        luaL_error(L, "strbuf recv: buf is not allocated");
    }

    if (unlikely(shift_bytes < sb->filled)) {
        luaL_error(L, "strbuf filled < shift; filled: %d; shift: %d",
            sb->filled, shift_bytes);
    } else if (likely(shift_bytes == sb->filled)) {
        sb->filled = 0;
    } else {
        memmove(sb->buf,
            sb->buf + shift_bytes,
            sb->filled - shift_bytes);

        sb->filled -= shift_bytes;
    }
}
