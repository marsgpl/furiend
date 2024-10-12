#include "async.h"

LUAMOD_API int luaopen_async(lua_State *L) {
    luaL_newlib(L, async_index);
    return 1;
}

int async_loop(lua_State *L) {
    luaF_need_args(L, 1, "loop");
    luaL_checktype(L, 1, LUA_TFUNCTION); // loop fn

    int type = lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP);

    if (unlikely(type != LUA_TNIL)) {
        luaL_error(L, "only 1 loop is allowed; current: %p; ridx: %d",
            lua_topointer(L, -1),
            F_RIDX_LOOP);
    }
    lua_pop(L, 1); // lua_rawgeti

    lua_State *T = luaF_new_thread_or_error(L);

    lua_insert(L, 1); // fn, T -> T, fn
    lua_xmove(L, T, 1); // fn -> T

    ud_loop *loop = lua_newuserdatauv(L, sizeof(ud_loop), 0);

    if (unlikely(loop == NULL)) {
        luaF_error_errno(L, F_ERROR_NEW_UD, F_MT_LOOP, 0);
    }

    int fd = epoll_create1(0);

    if (unlikely(fd == -1)) {
        luaF_error_errno(L, "epoll_create1 failed; flags: %d", 0);
    }

    loop->fd = fd;

    // need loop ud at index 1 for fast manual loop_gc
    lua_pushvalue(L, -1); // T, ud, ud
    lua_insert(L, 1); // ud, T, ud

    if (likely(luaL_newmetatable(L, F_MT_LOOP))) {
        lua_pushcfunction(L, loop_gc);
        lua_setfield(L, -2, "__gc");
    }

    lua_setmetatable(L, -2); // mt -> ud
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP);

    lua_createtable(L, 0, 4);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);

    lua_createtable(L, 0, 4);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);

    int nres;
    int status = lua_resume(T, L, 0, &nres);

    switch (status) {
        case LUA_OK: return loop_gc(L);
        case LUA_YIELD: return loop_yield(L, T, nres, fd);
        default: return loop_error(L, T, status);
    }
}

int async_wait(lua_State *L) {
    luaF_need_args(L, 1, "wait");
    luaL_checktype(L, 1, LUA_TTHREAD); // thread to wait for

    if (unlikely(!lua_isyieldable(L))) {
        luaL_error(L, "current thread is not yieldable: %p", (void *)L);
    }

    lua_State *T = lua_tothread(L, 1);
    int status = lua_status(T);

    switch (status) {
        case LUA_OK: return wait_ok(L, T);
        case LUA_YIELD: return wait_yield(L, 1);
        default: return wait_error(L, T, status);
    }
}

int async_pwait(lua_State *L) {
    lua_pushcfunction(L, async_wait);
    lua_insert(L, 1);

    int status = lua_pcallk(L,
        lua_gettop(L) - 1,
        LUA_MULTRET,
        0, 0,
        pwait_continue);

    lua_pushboolean(L, status == LUA_OK);
    lua_insert(L, 1);

    return lua_gettop(L);
}

static int loop_gc(lua_State *L) {
    ud_loop *loop = luaL_checkudata(L, 1, F_MT_LOOP);

    if (unlikely(loop->fd == -1)) {
        return 0;
    }

    luaF_close_or_warning(L, loop->fd);
    loop->fd = -1;

    lua_settop(L, 1); // loop
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);

    int fd_subs_idx = 2;
    int t_subs_idx = 3;

    lua_pushnil(L);
    while (lua_next(L, fd_subs_idx)) {
        int fd = lua_tointeger(L, -2);
        loop_notify_fd_sub(L, fd_subs_idx, t_subs_idx, fd, 0, "interrupt");
        lua_pop(L, 1); // lua_next
    }

    lua_pushnil(L);
    while (lua_next(L, t_subs_idx)) {
        lua_len(L, -1);
        lua_State *thread = lua_tothread(L, -3);
        int subs_n = lua_tointeger(L, -1);
        luaF_warning(L, "zombie thread: %p; subs: %d", thread, subs_n);
        lua_pop(L, 2); // lua_next, lua_len
    }

    lua_pushnil(L);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP);

    lua_pushnil(L);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);

    lua_pushnil(L);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);

    return 0;
}

static int loop_yield(lua_State *L, lua_State *MAIN, int nres, int epfd) {
    if (unlikely(nres > 0)) {
        lua_pop(MAIN, nres);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);

    int t_subs_idx = lua_gettop(L);
    int fd_subs_idx = t_subs_idx - 1;

    struct epoll_event events[EPOLL_WAIT_MAX_EVENTS];

    while (lua_status(MAIN) == LUA_YIELD) {
        int nfds = epoll_wait(epfd, events,
            EPOLL_WAIT_MAX_EVENTS,
            EPOLL_WAIT_TIMEOUT_MS);

        if (unlikely(nfds == -1)) {
            luaF_error_errno(L,
                "epoll_wait failed; fd: %d; max events: %d; timeout ms: %d",
                epfd,
                EPOLL_WAIT_MAX_EVENTS,
                EPOLL_WAIT_TIMEOUT_MS);
        }

        for (int n = 0; n < nfds; ++n) {
            int fd = events[n].data.fd;
            int emask = events[n].events;

            loop_notify_fd_sub(L, fd_subs_idx, t_subs_idx, fd, emask, NULL);
        }
    }

    int status = lua_status(MAIN);

    switch (status) {
        case LUA_OK: return loop_gc(L);
        default: return loop_error(L, MAIN, status);
    }
}

static int loop_error(lua_State *L, lua_State *T, int status) {
    loop_gc(L); // free ridx as fast as possible

    return luaL_error(L, "%s: %s",
        luaF_status_label(status),
        lua_tostring(T, -1)); // error msg is on top of T stack
}

static void loop_notify_fd_sub(
    lua_State *L,
    int fd_subs_idx,
    int t_subs_idx,
    int fd,
    int emask,
    const char *errmsg
) {
    // closing fd is a burden of the thread that spawned it
    // closing fd auto removes it from epoll (unless it was duped)

    int type = lua_rawgeti(L, fd_subs_idx, fd);

    if (unlikely(type != LUA_TTHREAD)) {
        // it's ok for fd_sub to be missing since prev events could remove it
        lua_pop(L, 1); // lua_rawgeti
        return;
    }

    lua_State *sub = lua_tothread(L, -1);

    if (unlikely(!lua_isyieldable(sub))) { // thread died, do not notify
        lua_pushnil(L);
        lua_rawseti(L, fd_subs_idx, fd); // remove fd sub
        lua_pop(L, 1); // lua_rawgeti
        return;
    }

    if (unlikely(errmsg)) {
        lua_pushinteger(sub, fd);
        lua_pushstring(sub, errmsg);
    } else {
        lua_pushinteger(sub, fd);
        lua_pushinteger(sub, emask);
    }

    int nres;
    int status = lua_resume(sub, L, 2, &nres);

    if (unlikely(status == LUA_YIELD)) {
        lua_pop(sub, nres);
    } else {
        lua_rawgeti(L, fd_subs_idx, fd); // rm if still the same
        if (lua_topointer(L, -1) == sub) {
            lua_pushnil(L);
            lua_rawseti(L, fd_subs_idx, fd); // remove fd sub
        }
        lua_pop(L, 1); // lua_rawgeti
        luaF_loop_notify_t_subs(L, sub, t_subs_idx, status, nres);
    }

    lua_pop(L, 1); // lua_rawgeti
}

static int wait_ok(lua_State *L, lua_State *T) {
    int nres = lua_gettop(T);

    if (unlikely(nres < 1)) {
        return 0;
    }

    // keep results in T in case T would be waited again

    luaL_checkstack(T, nres, "wait_ok T");
    luaL_checkstack(L, nres, "wait_ok L");

    for (int index = 1; index <= nres; ++index) {
        lua_pushvalue(T, index);
    }

    lua_xmove(T, L, nres);

    return nres;
}

static int wait_yield(lua_State *L, int thread_idx) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS); // t_subs
    lua_pushvalue(L, thread_idx); // t_subs, T

    if (unlikely(lua_rawget(L, -2) == LUA_TTABLE)) {
        // T already has subscribers
        lua_pushthread(L); // t_subs, subs, L
        lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
    } else { // T has no subs
        lua_pushvalue(L, thread_idx); // t_subs, nil, T
        lua_createtable(L, 1, 0); // t_subs, nil, T, subs
        lua_pushthread(L); // t_subs, nil, T, subs, L
        lua_rawseti(L, -2, 1); // subs[1] = L
        lua_rawset(L, -4); // t_subs[T] = subs
    }

    lua_settop(L, 1); // T

    return lua_yieldk(L, 0, 0, wait_continue); // longjmp
}

static int wait_error(lua_State *L, lua_State *T, int status) {
    return luaL_error(L, "%s: %s",
        luaF_status_label(status),
        lua_tostring(T, -1)); // error msg is on top of T stack
}

static int wait_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    status = lua_tointeger(L, 2);

    if (likely(status == LUA_OK)) {
        return lua_gettop(L) - 2; // exclude T, status
    }

    return wait_error(L, L, status);
}

static int pwait_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    lua_pushboolean(L, status == LUA_YIELD);
    lua_insert(L, 1);

    return lua_gettop(L);
}
