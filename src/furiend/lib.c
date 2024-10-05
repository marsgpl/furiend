#include "lib.h"

extern void luaF_need_args(lua_State *L, int need_args_n, const char *label) {
    if (lua_gettop(L) != need_args_n) {
        luaL_error(L, "%s: need args: %d; provided: %d",
            label, need_args_n, lua_gettop(L));
    }
}

extern void luaF_close_or_warning(lua_State *L, int fd) {
    int errno_bkp = errno;

    if (close(fd) == -1) {
        luaF_warning_errno(L, "close failed; fd: %d", fd);
        errno = errno_bkp;
    }
}

extern lua_State *luaF_new_thread_or_error(lua_State *L) {
    lua_State *T = lua_newthread(L);

    if (T == NULL) {
        luaF_error_errno(L, "lua_newthread failed");
    }

    return T;
}

extern int get_socket_error_code(int fd) {
    int value;
    socklen_t value_len = sizeof(value);

    int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &value, &value_len);

    return ret == -1 ? F_GETSOCKOPT_FAILED : value;
}

extern int luaF_error_socket(lua_State *L, int fd, const char *cause) {
    int socket_error_code = get_socket_error_code(fd);
    luaF_push_error_socket(L, fd, cause, socket_error_code);
    return lua_error(L);
}

extern void luaF_push_error_socket(
    lua_State *L,
    int fd,
    const char *cause,
    int socket_error_code
) {
    if (cause == NULL) {
        cause = "socket operation failed (cause not provided)";
    }

    if (socket_error_code == F_GETSOCKOPT_FAILED) {
        lua_pushfstring(L, "%s; fd: %d; getsockopt failed: %s (%d)",
            strerror(errno), errno, fd, cause);
    } else {
        lua_pushfstring(L, "%s; fd: %d; error: %s (%d)",
            strerror(socket_error_code), socket_error_code, fd, cause);
    }
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
            fprintf(stream, "\\x%X%X", (c >> 4) % 16, c % 16);
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

    fprintf(stream, F_DEBUG_SEP);
    fprintf(stream, "%s\n", label);
    fprintf(stream, "stack size: %d\n", stack_size);

    for (int index = 1; index <= stack_size; ++index) {
        fprintf(stream, "    %d%*s", index, index > 9 ? 3 : 4, "");

        if (lua_type(L, index) == LUA_TTABLE) {
            fprintf(stream, "table: %p {\n", lua_topointer(L, index));

            luaL_checkstack(L, 4, "trace");
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
    int fd = luaL_checkinteger(L, 1);
    int emask = luaL_checkinteger(L, 2);
    // 3 = sub thread, can differ from L

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP); // fd, emask, sub, loop

    ud_loop *loop = luaL_checkudata(L, -1, F_MT_LOOP);

    if (loop->fd == -1) {
        luaL_error(L, "watch failed: loop is closed");
    }

    struct epoll_event ee = {0};

    ee.data.fd = fd;
    ee.events = emask;

    if (epoll_ctl(loop->fd, EPOLL_CTL_ADD, fd, &ee) == -1) {
        luaF_error_errno(L, "epoll_ctl add failed"
            "; epoll fd: %d; fd: %d; event mask: %d",
            loop->fd, fd, emask);
    }

    lua_settop(L, 3); // fd, emask, sub
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);
    lua_insert(L, 3); // fd, emask, fd_subs, sub
    lua_rawseti(L, 3, fd); // fd_subs[fd] = sub

    return 0;
}

// fd is auto removed from loop epoll on close(fd) (unless it was duped)
// fd is removed from fd_subs by loop on thread finish
// so luaF_loop_unwatch is not needed
extern int luaF_loop_pwatch(lua_State *L, int fd, int emask, int sub_idx) {
    luaL_checkstack(L, 4, "loop watch");

    lua_pushcfunction(L, loop_watch);
    lua_pushinteger(L, fd);
    lua_pushinteger(L, emask);

    if (sub_idx) {
        lua_pushvalue(L, sub_idx);
    } else {
        lua_pushthread(L);
    }

    return lua_pcall(L, 3, 0, 0);
}

extern void luaF_loop_notify_t_subs(
    lua_State *L,
    lua_State *T,
    int t_subs_idx,
    int status,
    int nres
) {
    luaL_checkstack(L, 4, "notify subs L");
    luaL_checkstack(T, nres, "notify subs T");

    lua_pushvalue(L, -1);
    if (lua_rawget(L, t_subs_idx) != LUA_TTABLE) { // no subs
        lua_pop(L, 1); // lua_rawget
        return;
    }

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_State *sub = lua_tothread(L, -1);

        luaL_checkstack(sub, nres + 1, "notify subs sub");
        lua_pushinteger(sub, status);

        for (int index = 1; index <= nres; ++index) {
            lua_pushvalue(T, index);
        }

        lua_xmove(T, sub, nres);

        int sub_nres;
        int sub_status = lua_resume(sub, L, nres + 1, &sub_nres);

        if (status == LUA_YIELD) {
            lua_pop(sub, sub_nres);
        } else {
            luaF_loop_notify_t_subs(L, sub, t_subs_idx, sub_status, sub_nres);
        }

        lua_pop(L, 1); // lua_next
    }

    lua_pop(L, 1); // lua_rawget

    lua_pushvalue(L, -1);
    lua_pushnil(L);
    lua_rawset(L, t_subs_idx); // t_subs[thread] = nil
}

extern int luaF_set_timeout(lua_State *L, lua_Number duration_s) {
    long sec = duration_s;
    long nsec = (duration_s - sec) * 1e9;

    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    if (fd == -1) {
        luaF_error_errno(L, "timerfd_create failed; clock id: %d; flags: %d",
            CLOCK_MONOTONIC, TFD_NONBLOCK);
    }

    struct itimerspec tm = {0};

    tm.it_value.tv_sec = sec;
    tm.it_value.tv_nsec = nsec;

    if (timerfd_settime(fd, 0, &tm, NULL) == -1) {
        luaF_close_or_warning(L, fd);
        luaF_error_errno(L, "timerfd_settime failed"
            "; fd: %d; sec: %d; nsec: %d",
            fd, sec, nsec);
    }

    int status = luaF_loop_pwatch(L, fd, EPOLLIN | EPOLLONESHOT, 0);

    if (status != LUA_OK) {
        luaF_close_or_warning(L, fd);
        lua_error(L);
    }

    return fd;
}
