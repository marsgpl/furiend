#include "async.h"

LUAMOD_API int luaopen_async(lua_State *L) {
    luaL_newlib(L, async_index);
    return 1;
}

int async_loop(lua_State *L) {
    luaF_need_args(L, 1);
    luaL_checktype(L, 1, LUA_TFUNCTION);

    if (lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP) != LUA_TNIL) {
        luaL_error(L, "only 1 loop is allowed; current: %p; ridx: %d",
            lua_topointer(L, -1),
            F_RIDX_LOOP);
    }
    lua_pop(L, 1); // lua_rawgeti

    lua_State *T = lua_newthread(L);

    if (T == NULL) {
        luaF_error_errno(L, "lua_newthread failed");
    }

    lua_insert(L, 1); // fn, T -> T, fn
    lua_xmove(L, T, 1); // fn -> T

    ud_loop *loop = lua_newuserdatauv(L, sizeof(ud_loop), 0);

    if (loop == NULL) {
        luaF_error_errno(L, F_ERROR_NEW_UD, F_MT_LOOP, 0);
    }

    int fd = epoll_create1(0);

    if (fd == -1) {
        luaF_error_errno(L, "epoll_create1 failed; flags: %d", 0);
    }

    loop->fd = fd;

    // need loop ud at index 1 for fast manual loop_gc
    lua_pushvalue(L, -1); // T, ud, ud
    lua_insert(L, 1); // ud, T, ud

    luaL_newmetatable(L, F_MT_LOOP);
    lua_pushcfunction(L, loop_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2); // mt -> ud

    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP);

    lua_newtable(L);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);

    lua_newtable(L);
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
    luaF_need_args(L, 1);
    luaL_checktype(L, 1, LUA_TTHREAD);

    if (!lua_isyieldable(L)) {
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

    lua_pcallk(L, lua_gettop(L) - 1, LUA_MULTRET, 0, 0, pwait_continue);

    // if lua_pcallk fails before yielding, it exits with err status
    // otherwise it never returns

    lua_pushboolean(L, 0);
    lua_insert(L, lua_gettop(L) - 1);
    return 2;
}

static int loop_gc(lua_State *L) {
    ud_loop *loop = luaL_checkudata(L, 1, F_MT_LOOP);

    if (loop->fd == -1) {
        return 0;
    }

    luaF_close_or_warning(L, loop->fd);
    loop->fd = -1;

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        int fd = lua_tointeger(L, -2);
        lua_State *fd_sub = lua_tothread(L, -1);
        luaF_warning(L, "zombie fd sub; fd: %d; sub: %p", fd, fd_sub);
        lua_pop(L, 1); // lua_next
    }
    lua_pop(L, 1); // lua_rawgeti

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_len(L, -1);
        lua_State *thread = lua_tothread(L, -3);
        int subs_n = lua_tointeger(L, -1);
        luaF_warning(L, "zombie thread: %p; subs: %d", thread, subs_n);
        lua_pop(L, 2); // lua_next, lua_len
    }
    lua_pop(L, 1); // lua_rawgeti

    lua_pushnil(L);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP);

    lua_pushnil(L);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_FD_SUBS);

    lua_pushnil(L);
    lua_rawseti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);

    return 0;
}

static int loop_yield(lua_State *L, lua_State *MAIN, int nres, int epfd) {
    if (nres > 0) {
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

        if (nfds == -1) {
            return luaF_error_errno(L,
                "epoll_wait failed; fd: %d; max events: %d; timeout ms: %d",
                epfd,
                EPOLL_WAIT_MAX_EVENTS,
                EPOLL_WAIT_TIMEOUT_MS);
        }

        for (int n = 0; n < nfds; ++n) {
            int fd = events[n].data.fd;
            int emask = events[n].events;

            loop_notify_fd_sub(L, fd_subs_idx, t_subs_idx, fd, emask);
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

static inline void loop_notify_fd_sub(
    lua_State *L,
    int fd_subs_idx,
    int t_subs_idx,
    int fd,
    int emask
) {
    // closing fd is a burden of the thread that spawned it
    // closing fd auto removes it from epoll (unless it was duped)

    if (lua_rawgeti(L, fd_subs_idx, fd) != LUA_TTHREAD) {
        // it's ok for fd_sub to be missing since prev events could remove it
        lua_pop(L, 1); // lua_rawgeti
        return;
    }

    lua_State *SUB = lua_tothread(L, -1);

    lua_pushinteger(SUB, fd);
    lua_pushinteger(SUB, emask);

    int nres;
    int status = lua_resume(SUB, L, 2, &nres);

    if (status == LUA_YIELD) {
        lua_pop(SUB, nres);
    } else {
        loop_remove_fd_sub(L, fd_subs_idx, fd);
        loop_notify_t_subs(L, SUB, t_subs_idx, status, nres);
    }

    lua_pop(L, 1); // lua_rawgeti
}

static void loop_notify_t_subs(
    lua_State *L,
    lua_State *T,
    int t_subs_idx,
    int status,
    int nres
) {
    luaL_checkstack(L, lua_gettop(L) + 3, "notify subs L");
    luaL_checkstack(T, lua_gettop(T) + nres, "notify subs T");

    lua_pushvalue(L, -1);
    if (lua_rawget(L, t_subs_idx) != LUA_TTABLE) { // no subs
        lua_pop(L, 1); // lua_rawget
        return;
    }

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_State *SUB = lua_tothread(L, -1);

        luaL_checkstack(SUB, lua_gettop(SUB) + nres + 1, "notify subs SUB");
        lua_pushinteger(SUB, status);

        for (int index = 1; index <= nres; ++index) {
            lua_pushvalue(T, index);
        }

        lua_xmove(T, SUB, nres);

        int sub_nres;
        int sub_status = lua_resume(SUB, L, nres + 1, &sub_nres);

        if (status == LUA_YIELD) {
            lua_pop(SUB, sub_nres);
        } else {
            loop_notify_t_subs(L, SUB, t_subs_idx, sub_status, sub_nres);
        }

        lua_pop(L, 1); // lua_next
    }

    lua_pop(L, 1); // lua_rawget

    lua_pushvalue(L, -1);
    lua_pushnil(L);
    lua_rawset(L, t_subs_idx); // t_subs[thread] = nil
}

static int wait_ok(lua_State *L, lua_State *T) {
    int nres = lua_gettop(T);

    if (nres < 1) {
        return 0;
    }

    // keep results in T in case T would be waited again

    luaL_checkstack(T, lua_gettop(T) + nres, "wait_ok T");
    luaL_checkstack(L, lua_gettop(L) + nres, "wait_ok L");

    for (int index = 1; index <= nres; ++index) {
        lua_pushvalue(T, index);
    }

    lua_xmove(T, L, nres);

    return nres;
}

static int wait_yield(lua_State *L, int t_index) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS); // t_subs
    lua_pushvalue(L, t_index); // t_subs, T

    if (lua_rawget(L, -2) == LUA_TTABLE) { // T already has subscribers
        lua_pushthread(L); // t_subs, subs, L
        lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
    } else { // T has no subs
        lua_pushvalue(L, t_index); // t_subs, nil, T
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
    (void)status;
    (void)ctx;

    status = lua_tointeger(L, 2);

    if (status == LUA_OK) {
        return lua_gettop(L) - 2; // exclude T, status
    }

    return wait_error(L, L, status);
}

static int pwait_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    // LUA_YIELD on success, LUA_ERR* on error
    lua_pushboolean(L, status == LUA_YIELD);
    lua_insert(L, 1);

    return lua_gettop(L);
}
