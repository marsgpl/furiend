#ifndef FURIEND_SHARED_H
#define FURIEND_SHARED_H

#define _POSIX_C_SOURCE 199309L // CLOCK_MONOTONIC
#define _GNU_SOURCE // clock_gettime

#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <lauxlib.h>
#include <assert.h>

static_assert(sizeof(char) == 1, "sizeof(char) is not 1");

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

#define F_GETSOCKOPT_FAILED -1 // see get_socket_error_code

#define F_ESC_STR_SUFFIX " ... (%lu more)"

#define inline __inline__
#define likely(expr) __builtin_expect(expr, 1)
#define unlikely(expr) __builtin_expect(expr, 0)

typedef struct {
    int fd;
} ud_loop;

#define luaF_warning(L, msg, ...) { \
    lua_pushfstring(L, msg __VA_OPT__(,) __VA_ARGS__); \
    lua_warning(L, lua_tostring(L, -1), 0); \
    lua_pop(L, 1); \
}

#define luaF_warning_errno(L, msg, ...) \
    luaF_warning(L, msg "; errno: %s (%d)" \
        __VA_OPT__(,) __VA_ARGS__, \
        strerror(errno), errno)

#define luaF_error_errno(L, msg, ...) \
    luaL_error(L, msg "; errno: %s (%d)" \
        __VA_OPT__(,) __VA_ARGS__, \
        strerror(errno), errno) \

#define luaF_set_kv_int(L, table_idx, key, value) { \
    lua_pushinteger(L, value); \
    lua_setfield(L, table_idx > 0 ? table_idx : table_idx - 1, key); \
}

#define luaF_loop_check_close(L) { \
    if (unlikely(lua_type(L, F_LOOP_ERRMSG_REL_IDX) == LUA_TSTRING)) { \
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

int get_socket_error_code(int fd);

int parse_dec(char **pos);
int parse_hex(char **pos);
int uint_len(uint64_t num);
int int_len(int64_t num);

void luaF_print_time(lua_State *L, FILE *stream);
void luaF_print_value(lua_State *L, FILE *stream, int index);
void luaF_print_string(lua_State *L, FILE *stream, int index);
void luaF_trace(lua_State *L, const char *label);
void luaF_need_args(lua_State *L, int need_args_n, const char *label);
void luaF_min_max_args(lua_State *L, int min, int max, const char *label);
void luaF_close_or_warning(lua_State *L, int fd);
int luaF_set_timeout(lua_State *L, lua_Number duration_s);
void luaF_push_error_socket(lua_State *L, int fd, const char *cause, int code);
int luaF_error_socket(lua_State *L, int fd, const char *cause);
void luaF_set_ip4_port(
    lua_State *L,
    struct sockaddr_in *sa,
    const char *ip4,
    int port);
lua_State *luaF_new_thread_or_error(lua_State *L);
void *luaF_new_uduv_or_error(lua_State *L, size_t size, int uv_n);
const char *luaF_status_label(int status);
const char *luaF_escape_string(
    lua_State *L,
    const char *buf,
    size_t buf_len,
    size_t max_len);
int luaF_loop_watch(lua_State *L, int fd, int emask, int sub_idx);
int luaF_loop_protected_watch(lua_State *L, int fd, int emask, int sub_idx);
void luaF_loop_notify_t_subs(
    lua_State *L,
    int t_subs_idx,
    lua_State *T,
    int t_idx,
    int t_status,
    int t_nres);
int luaF_is_array(lua_State *L, int index);
void *luaF_malloc_or_error(lua_State *L, size_t size);
int luaF_resume(
    lua_State *L,
    int t_subs_idx,
    lua_State *T,
    int t_idx,
    int t_nargs);

#endif

// luaL_argcheck(L, lua_isfunction(L, 1), 1, "message");
// bad argument #1 to 'function_name' (message)

// luaL_argexpected(L, lua_isfunction(L, 1), 1, "message");
// bad argument #1 to 'function_name' (message expected, got number)

// luaL_checktype(L, 1, LUA_TFUNCTION);
// bad argument #1 to 'function_name' (function expected, got number)
