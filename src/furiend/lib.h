#ifndef FURIEND_LIB_H
#define FURIEND_LIB_H

#define _POSIX_C_SOURCE 199309L // CLOCK_MONOTONIC

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <lauxlib.h>

#define F_DEBUG_SEP "-------------------------------------------------------\n"
#define F_DEBUG_STR_MAX_LEN 1024
#define F_ERROR_NEW_UD "lua_newuserdatauv failed; mt: %s; user values: %d"
#define F_MT_LOOP "loop*"

// when loop resumes fd sub: fd, emask
// when loop.gc called, it closes all fd subs: fd, errmsg
#define F_LOOP_FD_REL_IDX -2
#define F_LOOP_EMASK_REL_IDX -1
#define F_LOOP_ERRMSG_REL_IDX -1

// check LUA_RIDX_LAST and freelist in lua src
#define F_RIDX_LOOP 1001
#define F_RIDX_LOOP_FD_SUBS 1002 // fd_subs[fd] = sub
#define F_RIDX_LOOP_T_SUBS 1003 // t_subs[thread] = { sub1, sub2, ... }

typedef struct {
    int fd;
} ud_loop;

#define luaF_warning(L, msg, ...) { \
    luaL_checkstack(L, lua_gettop(L) + 1, "luaF_warning"); \
    lua_pushfstring(L, msg __VA_OPT__(,) __VA_ARGS__); \
    lua_warning(L, lua_tostring(L, -1), 0); \
    lua_pop(L, 1); \
}

#define luaF_warning_errno(L, msg, ...) \
    luaF_warning(L, msg "; errno: %s (#%d)" \
        __VA_OPT__(,) __VA_ARGS__, \
        strerror(errno), \
        errno)

#define luaF_error_errno(L, msg, ...) \
    luaL_error(L, msg "; errno: %s (#%d)" \
        __VA_OPT__(,) __VA_ARGS__, \
        strerror(errno), \
        errno) \

#define luaF_error_socket(L, fd, cause) { \
    luaF_push_error_socket(L, fd, cause); \
    lua_error(L); \
}

#define luaF_push_error_errno(L, cause) \
    lua_pushfstring(L, "%s; errno: %s (#%d)", cause, strerror(errno), errno)

#define luaF_set_kv_int(L, table_idx, key, value) { \
    lua_pushinteger(L, value); \
    lua_setfield(L, table_idx > 0 ? table_idx : table_idx - 1, key); \
}

#define luaF_loop_check_close(L) { \
    if (lua_type(L, F_LOOP_ERRMSG_REL_IDX) == LUA_TSTRING) { \
        luaL_error(L, "loop.gc: %s", lua_tostring(L, F_LOOP_ERRMSG_REL_IDX)); \
    } \
}

#define emask_has_errors(emask) ( \
    (emask & EPOLLERR) || \
    (emask & EPOLLHUP) || \
    (emask & EPOLLRDHUP))

#define emask_error_label(emask) ( \
    (emask & EPOLLERR) ? "communication error (EPOLLERR)" : \
    (emask & EPOLLHUP) ? "connection closed (EPOLLHUP)" : \
    (emask & EPOLLRDHUP) ? "connection read/write shut down (EPOLLRDHUP)" : \
    "epoll event mask did not indicate any errors")

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

extern void luaF_trace(lua_State *L, const char *label);
extern void luaF_print_value(lua_State *L, int index, FILE *stream);
extern void luaF_print_string(lua_State *L, int index, FILE *stream);
extern void luaF_close_or_warning(lua_State *L, int fd);
extern lua_State *luaF_new_thread_or_error(lua_State *L);
extern void luaF_push_error_socket(lua_State *L, int fd, const char *cause);
extern void luaF_need_args(lua_State *L, int need_args_n, const char *label);
extern int luaF_loop_pwatch(lua_State *L, int fd, int emask, int sub_idx);
extern const char *luaF_status_label(int status);
extern int luaF_set_timeout(lua_State *L, lua_Number duration_s);

extern void luaF_loop_notify_t_subs(
    lua_State *L,
    lua_State *T,
    int t_subs_idx,
    int status,
    int nres);

#endif

// luaL_argcheck(L, lua_isfunction(L, 1), 1, "message");
// bad argument #1 to 'function_name' (message)

// luaL_argexpected(L, lua_isfunction(L, 1), 1, "message");
// bad argument #1 to 'function_name' (message expected, got number)

// luaL_checktype(L, 1, LUA_TFUNCTION);
// bad argument #1 to 'function_name' (function expected, got number)
