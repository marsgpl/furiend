#ifndef FURIEND_LIB_H
#define FURIEND_LIB_H

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <lauxlib.h>

#define F_DEBUG_SEP "-------------------------------------------------------\n"
#define F_DEBUG_STR_MAX_LEN 1024
#define F_ERROR_NEW_UD "lua_newuserdatauv failed; mt: %s; user values: %d"
#define F_MT_LOOP "loop*"

// check LUA_RIDX_LAST and freelist in lua src
#define F_RIDX_LOOP 1001
#define F_RIDX_LOOP_FD_SUBS 1002 // fd_subs[fd] = sub
#define F_RIDX_LOOP_T_SUBS 1003 // t_subs[thread] = { sub1, sub2, ... }

typedef struct {
    int fd;
} ud_loop;

#define luaF_addfstring(L, B, msg, ...) { \
    lua_pushfstring(L, msg __VA_OPT__(,) __VA_ARGS__); \
    size_t len; const char *str = lua_tolstring(L, -1, &len); \
    luaL_addlstring(B, str, len); \
    lua_pop(L, 1); \
}

#define luaF_warning(L, msg, ...) { \
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

#define epoll_emask_has_errors(emask) ( \
    (emask & EPOLLERR) || \
    (emask & EPOLLHUP) || \
    (emask & EPOLLRDHUP))

#define epoll_emask_error_label(emask) ( \
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
extern int luaF_error_fd(lua_State *L, int fd, const char *cause);
extern void luaF_need_args(lua_State *L, int args_n);
extern int luaF_loop_pwatch(lua_State *L, int fd, int emask);
extern const char *luaF_status_label(int status);

#endif
