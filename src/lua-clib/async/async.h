#ifndef LUA_LIB_ASYNC_H
#define LUA_LIB_ASYNC_H

#include <furiend/shared.h>

#define EPOLL_WAIT_TIMEOUT_MS -1 // infinite: -1
#define EPOLL_WAIT_MAX_EVENTS 256

LUAMOD_API int luaopen_async(lua_State *L);

int async_loop(lua_State *L);
int async_wait(lua_State *L);
int async_pwait(lua_State *L);

static int loop_gc(lua_State *L);
static int loop_yield(lua_State *L, lua_State *MAIN, int nres, int epfd);
static int loop_error(lua_State *L, lua_State *T, int status);
static void loop_notify_fd_sub(
    lua_State *L,
    int fd_subs_idx,
    int t_subs_idx,
    int fd,
    int emask,
    const char *errmsg);

static int wait_ok(lua_State *L, lua_State *T);
static int wait_yield(lua_State *L, int thread_idx);
static int wait_error(lua_State *L, lua_State *T, int status);
static int wait_continue(lua_State *L, int status, lua_KContext ctx);
static int pwait_continue(lua_State *L, int status, lua_KContext ctx);

static const luaL_Reg async_index[] = {
    { "loop", async_loop },
    { "wait", async_wait },
    { "pwait", async_pwait },
    { NULL, NULL }
};

#endif
