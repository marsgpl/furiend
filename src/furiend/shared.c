#include "shared.h"

static int loop_watch(lua_State *L);

static const int dec_to_int[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,
};

static const int hex_to_int[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    -1,-1,-1,-1,-1,-1,-1,
    10, 11, 12, 13, 14, 15,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    10, 11, 12, 13, 14, 15,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,
};

int parse_dec(char **pos) {
    int result = 0;
    int digit = dec_to_int[(int)(**pos)];

    while (likely(digit != -1)) {
        result = (result * 10) + digit;
        (*pos)++;
        digit = dec_to_int[(int)(**pos)];
    }

    return result;
}

int parse_hex(char **pos) {
    int result = 0;
    int digit = hex_to_int[(int)(**pos)];

    while (likely(digit != -1)) {
        result = (result * 16) + digit;
        (*pos)++;
        digit = hex_to_int[(int)(**pos)];
    }

    return result;
}

int uint_len(uint64_t num) {
    if (num < 10ULL) return 1;
    if (num < 100ULL) return 2;
    if (num < 1000ULL) return 3;
    if (num < 10000ULL) return 4;
    if (num < 100000ULL) return 5;
    if (num < 1000000ULL) return 6;
    if (num < 10000000ULL) return 7;
    if (num < 100000000ULL) return 8;
    if (num < 1000000000ULL) return 9;
    if (num < 10000000000ULL) return 10;
    if (num < 100000000000ULL) return 11;
    if (num < 1000000000000ULL) return 12;
    if (num < 10000000000000ULL) return 13;
    if (num < 100000000000000ULL) return 14;
    if (num < 1000000000000000ULL) return 15;
    if (num < 10000000000000000ULL) return 16;
    if (num < 100000000000000000ULL) return 17;
    if (num < 1000000000000000000ULL) return 18;
    if (num < 10000000000000000000ULL) return 19;
    return 20; // max: 18446744073709551615ULL
}

int get_socket_error_code(int fd) {
    int value;
    socklen_t value_len = sizeof(value);
    int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &value, &value_len);

    if (unlikely(ret < 0)) {
        return F_GETSOCKOPT_FAILED;
    }

    return value;
}

void luaF_print_time(lua_State *L, FILE *stream) {
    struct timespec ts;

    if (unlikely(clock_gettime(CLOCK_REALTIME, &ts) < 0)) {
        luaF_error_errno(L, "clock_gettime failed");
    }

    fprintf(stream, "%f", ts.tv_sec + ts.tv_nsec * 1e-9);
}

void luaF_print_value(lua_State *L, FILE *stream, int index) {
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
            luaF_print_string(L, stream, index);
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

void luaF_print_string(lua_State *L, FILE *stream, int index) {
    size_t len;
    const char *str = lua_tolstring(L, index, &len);

    putc('"', stream);

    for (size_t i = 0; i < len; ++i) {
        unsigned char c = str[i];

        switch (c) {
            case '\r': fputs("\\r", stream); break;
            case '\n': fputs("\\n", stream); break;
            case '\t': fputs("\\t", stream); break;
            case '\\': fputs("\\", stream); break;
            case '"': fputs("\\\"", stream); break;
            default:
                if (unlikely(c < 32 || c >= 127)) {
                    fprintf(stream, "\\x%X%X", (c >> 4) % 16, c % 16);
                } else {
                    putc(c, stream);
                }
        }
    }

    putc('"', stream);
}

void luaF_trace(lua_State *L, const char *label) {
    FILE *stream = stderr;
    int stack_size = lua_gettop(L);

    fprintf(stream, "--------------------------------------------------\n");
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
                luaF_print_value(L, stream, -2);
                fprintf(stream, " = ");
                luaF_print_value(L, stream, -1);
                putc('\n', stream);
                lua_pop(L, 1); // lua_next
            }

            fprintf(stream, "%*s}", 9, "");
        } else {
            luaF_print_value(L, stream, index);
        }

        putc('\n', stream);
    }

    fprintf(stream, "--------------------------------------------------\n");
}

void luaF_need_args(lua_State *L, int need_args_n, const char *label) {
    if (unlikely(lua_gettop(L) != need_args_n)) {
        luaL_error(L, "%s: need args: %d; provided: %d",
            label, need_args_n, lua_gettop(L));
    }
}

void luaF_min_max_args(lua_State *L, int min, int max, const char *label) {
    int args_n = lua_gettop(L);

    if (unlikely(args_n < min && args_n > max)) {
        luaL_error(L, "%s: min args: %d; max args: %d; provided: %d",
            label, min, max, lua_gettop(L));
    }
}

void luaF_close_or_warning(lua_State *L, int fd) {
    int errno_bkp = errno;

    if (unlikely(close(fd) < 0)) {
        luaF_warning_errno(L, "close failed; fd: %d", fd);
        errno = errno_bkp;
    }
}

int luaF_set_timeout(lua_State *L, lua_Number duration_s) {
    long sec = duration_s;
    long nsec = (duration_s - sec) * 1e9;

    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    if (unlikely(fd < 0)) {
        luaF_error_errno(L, "timerfd_create failed; clock id: %d; flags: %d",
            CLOCK_MONOTONIC, TFD_NONBLOCK);
    }

    struct itimerspec tm = {0};

    tm.it_value.tv_sec = sec;
    tm.it_value.tv_nsec = nsec;

    if (unlikely(timerfd_settime(fd, 0, &tm, NULL) < 0)) {
        luaF_close_or_warning(L, fd);
        luaF_error_errno(L, "timerfd_settime failed"
            "; fd: %d; sec: %d; nsec: %d",
            fd, sec, nsec);
    }

    int status = luaF_loop_protected_watch(L, fd, EPOLLIN | EPOLLONESHOT, 0);

    if (unlikely(status != LUA_OK)) {
        luaF_close_or_warning(L, fd);
        lua_error(L);
    }

    return fd;
}

void luaF_push_error_socket(lua_State *L, int fd, const char *cause, int code) {
    if (unlikely(!cause)) {
        cause = "socket operation failed (cause not provided)";
    }

    if (unlikely(code == F_GETSOCKOPT_FAILED)) {
        lua_pushfstring(L, "%s; fd: %d; getsockopt failed: %s (%d)",
            cause, fd, strerror(errno), errno);
    } else {
        lua_pushfstring(L, "%s; fd: %d; error: %s (%d)",
            cause, fd, strerror(code), code);
    }
}

int luaF_error_socket(lua_State *L, int fd, const char *cause) {
    int code = get_socket_error_code(fd);
    luaF_push_error_socket(L, fd, cause, code);
    return lua_error(L);
}

lua_State *luaF_new_thread_or_error(lua_State *L) {
    lua_State *T = lua_newthread(L);

    if (unlikely(T == NULL)) {
        luaF_error_errno(L, "lua_newthread failed");
    }

    return T;
}

void *luaF_new_uduv_or_error(lua_State *L, size_t size, int uv_n) {
    void *ud = lua_newuserdatauv(L, size, uv_n);

    if (unlikely(ud == NULL)) {
        luaF_error_errno(L, "lua_newuserdatauv failed; size: %d; uv_n: %d",
            size, uv_n);
    }

    return ud;
}

// https://www.lua.org/manual/5.4/manual.html#4.4.1
const char *luaF_status_label(int status) {
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

static int loop_watch(lua_State *L) {
    int fd = luaL_checkinteger(L, 1);
    int emask = luaL_checkinteger(L, 2);
    // 3 = sub thread, can differ from L

    if (unlikely(fd <= 0)) {
        luaL_error(L, "loop.watch: invalid fd: %d", fd);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP); // fd, emask, sub, loop

    ud_loop *loop = luaL_checkudata(L, -1, F_MT_LOOP);

    if (unlikely(loop->fd < 0)) {
        luaL_error(L, "watch failed: loop is closed");
    }

    struct epoll_event ee = {0};

    ee.data.fd = fd;
    ee.events = emask;

    if (unlikely(epoll_ctl(loop->fd, EPOLL_CTL_ADD, fd, &ee) < 0)) {
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

int luaF_loop_watch(lua_State *L, int fd, int emask, int sub_idx) {
    luaL_checkstack(L, 4, "loop watch");

    lua_pushcfunction(L, loop_watch);
    lua_pushinteger(L, fd);
    lua_pushinteger(L, emask);

    if (unlikely(sub_idx != 0)) {
        lua_pushvalue(L, sub_idx);
    } else {
        lua_pushthread(L);
    }

    lua_call(L, 3, 0);

    return LUA_OK;
}

// fd is auto removed from loop epoll on close(fd) (unless it was duped)
// fd is removed from fd_subs by loop on thread finish
// so luaF_loop_unwatch is not needed
int luaF_loop_protected_watch(lua_State *L, int fd, int emask, int sub_idx) {
    luaL_checkstack(L, 4, "loop protected watch");

    lua_pushcfunction(L, loop_watch);
    lua_pushinteger(L, fd);
    lua_pushinteger(L, emask);

    if (unlikely(sub_idx != 0)) {
        lua_pushvalue(L, sub_idx);
    } else {
        lua_pushthread(L);
    }

    return lua_pcall(L, 3, 0, 0);
}

void luaF_loop_notify_t_subs(
    lua_State *L,
    lua_State *T,
    int t_subs_idx,
    int status,
    int nres
) {
    lua_pushvalue(L, -1);
    if (unlikely(lua_rawget(L, t_subs_idx) != LUA_TTABLE)) { // no subs
        lua_pop(L, 1); // lua_rawget
        return;
    }

    lua_pushvalue(L, -2);
    lua_pushnil(L);
    lua_rawset(L, t_subs_idx); // t_subs[thread] = nil

    luaL_checkstack(L, 4, "notify subs L");
    luaL_checkstack(T, nres, "notify subs T");

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

        if (unlikely(status == LUA_YIELD)) {
            lua_pop(sub, sub_nres);
        } else {
            luaF_loop_notify_t_subs(L, sub, t_subs_idx, sub_status, sub_nres);
        }

        lua_pop(L, 1); // lua_next
    }

    lua_pop(L, 1); // lua_rawget
}
