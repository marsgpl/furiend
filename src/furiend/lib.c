#include "lib.h"

extern void luaF_need_args(lua_State *L, int args_n) {
    if (lua_gettop(L) != args_n) {
        luaL_error(L, "need args: %d; have: %d", args_n, lua_gettop(L));
    }
}

extern void luaF_close_or_warning(lua_State *L, int fd) {
    int errno_bkp = errno;

    if (close(fd) == -1) {
        luaF_warning_errno(L, "close failed; fd: %d", fd);
        errno = errno_bkp;
    }
}

extern int luaF_error_fd(lua_State *L, int fd, const char *cause) {
    int val;
    socklen_t val_len = sizeof(val);

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &val_len) == -1) {
        luaF_warning_errno(L, "getsockopt failed"
            "; fd: %d; level: SOL_SOCKET (%d)"
            "; opt: SO_ERROR (%d); opt val len: %d",
            fd, SOL_SOCKET, SO_ERROR, val_len);
    }

    return luaL_error(L, "fd error: %s (#%d); fd: %d; cause: %s",
        strerror(val), val, fd, cause);
}

// https://www.lua.org/manual/5.4/manual.html#4.4.1
extern const char *luaF_status_label(int status) {
    switch (status) {
        case LUA_OK: return "ok";
        case LUA_ERRRUN: return "runtime error";
        case LUA_ERRMEM: return "memory allocation error";
        case LUA_ERRERR: return "error while running the message handler";
        case LUA_ERRSYNTAX: return "syntax error during precompilation";
        case LUA_YIELD: return "thread yields";
        case LUA_ERRFILE: return "file error";
        default: return "unknown status";
    }
}

extern void luaF_print_string(lua_State *L, int index, FILE *stream) {
    size_t len;
    const char *str = lua_tolstring(L, index, &len);

    putc('"', stream);

    for (size_t i = 0; i < len; ++i) {
        unsigned char c = str[i];

        if (c < 32 || c == '"' || c == '\\' || c >= 127) {
            fprintf(stream, "\\%u", c);
        } else {
            putc(c, stream);
        }
    }

    putc('"', stream);
}

extern void luaF_print_value(lua_State *L, int index, FILE *stream) {
    switch (lua_type(L, index)) {
        case LUA_TNONE:
            fprintf(stream, "NONE");
            return;
        case LUA_TNIL:
            fprintf(stream, "nil");
            return;
        case LUA_TBOOLEAN:
            lua_toboolean(L, index)
                ? fprintf(stream, "true")
                : fprintf(stream, "false");
            return;
        case LUA_TLIGHTUSERDATA:
            fprintf(stream, "light userdata: %p", lua_topointer(L, index));
            return;
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                fprintf(stream, LUA_INTEGER_FMT, lua_tointeger(L, index));
            } else {
                fprintf(stream, LUA_NUMBER_FMT, lua_tonumber(L, index));
            }
            return;
        case LUA_TSTRING:
            luaF_print_string(L, index, stream);
            return;
        case LUA_TTABLE:
            fprintf(stream, "table: %p", lua_topointer(L, index));
            return;
        case LUA_TFUNCTION:
            if (lua_iscfunction(L, index)) {
                fprintf(stream, "c function: %p", lua_topointer(L, index));
            } else {
                fprintf(stream, "function: %p", lua_topointer(L, index));
            }
            return;
        case LUA_TUSERDATA:
            fprintf(stream, "userdata: %p", lua_topointer(L, index));
            return;
        case LUA_TTHREAD:
            if (lua_isyieldable(lua_tothread(L, index))) {
                fprintf(stream, "thread: %p", lua_topointer(L, index));
            } else {
                fprintf(stream, "main thread: %p", lua_topointer(L, index));
            }
            return;
        default:
            putc('?', stream);
            return;
    }
}

extern void luaF_trace(lua_State *L, const char *label) {
    FILE *stream = stderr;
    int stack_size = lua_gettop(L);

    fprintf(stream, F_DEBUG_SEP "%s\nstack size: %d\n", label, stack_size);

    for (int index = 1; index <= stack_size; ++index) {
        fprintf(stream, "    %d%*s", index, index > 9 ? 3 : 4, "");

        if (lua_type(L, index) == LUA_TTABLE) {
            fprintf(stream, "table: %p {\n", lua_topointer(L, index));

            lua_pushnil(L);
            while (lua_next(L, index)) {
                fprintf(stream, "%*s", 13, "");
                luaF_print_value(L, -2, stream);
                fprintf(stream, " = ");
                luaF_print_value(L, -1, stream);
                putc('\n', stream);
                lua_pop(L, 1); // lua_next
            }

            fprintf(stream, "%*s}", 9, "");
        } else {
            luaF_print_value(L, index, stream);
        }

        putc('\n', stream);
    }

    fprintf(stream, F_DEBUG_SEP);
}

static int loop_watch(lua_State *L) {
    luaF_need_args(L, 2);

    int fd = luaL_checkinteger(L, 1);
    int emask = luaL_checkinteger(L, 2);

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP);

    ud_loop *loop = luaL_checkudata(L, -1, F_MT_LOOP);

    struct epoll_event ee = {0};

    ee.data.fd = fd;
    ee.events = emask;

    if (epoll_ctl(loop->fd, EPOLL_CTL_ADD, fd, &ee) == -1) {
        luaF_error_errno(L, "epoll_ctl add failed"
            "; epoll fd: %d; fd: %d; event mask: %d",
            loop->fd, fd, emask);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);

    lua_pushthread(L);
    lua_rawseti(L, -2, fd); // fd_subs[fd] = L

    lua_pop(L, 2); // F_RIDX_LOOP, F_RIDX_LOOP_FD_SUBS

    return 0;
}

// fd is auto removed from loop epoll on close(fd) (unless it was duped)
// fd is removed from fd_subs by loop on thread finish (ok or error)
// so luaF_loop_unwatch is not needed
extern int luaF_loop_pwatch(lua_State *L, int fd, int emask) {
    lua_pushcfunction(L, loop_watch);
    lua_pushinteger(L, fd);
    lua_pushinteger(L, emask);
    return lua_pcall(L, 2, 0, 0);
}
