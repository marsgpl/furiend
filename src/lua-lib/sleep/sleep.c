#include "sleep.h"

LUAMOD_API int luaopen_sleep(lua_State *L) {
    lua_pushcfunction(L, sleep_create);
    return 1;
}

static int sleep_create(lua_State *L) {
    luaF_need_args(L, 1);
    luaL_checktype(L, 1, LUA_TNUMBER); // duration_s

    lua_State *T = lua_newthread(L);

    if (T == NULL) {
        luaF_error_errno(L, "lua_newthread failed");
    }

    lua_insert(L, 1); // num, T -> T, num
    lua_pushcfunction(T, sleep_start);
    lua_xmove(L, T, 1); // num -> T

    int nres;
    lua_resume(T, L, 1, &nres);

    // ok: not possible (should yield)
    // yield: 0 results, do not bother to remove
    // error: wait() will process normally

    return 1; // T
}

static int sleep_start(lua_State *L) {
    lua_Number duration_s = lua_tonumber(L, 1);

    long sec = duration_s;
    long nsec = (duration_s - sec) * 1e9;

    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    if (fd == -1) {
        luaF_error_errno(L, "timerfd_create failed"
            "; clock id: CLOCK_MONOTONIC (%d)"
            "; flags: TFD_NONBLOCK (%d)",
            CLOCK_MONOTONIC,
            TFD_NONBLOCK);
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

    if (luaF_loop_pwatch(L, fd, EPOLLIN | EPOLLONESHOT) != LUA_OK) {
        luaF_close_or_warning(L, fd);
        lua_error(L);
    }

    return lua_yieldk(L, 0, 0, sleep_continue); // longjmp
}

static int sleep_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)status;
    (void)ctx;

    int fd = lua_tointeger(L, -2);
    int emask = lua_tointeger(L, -1);

    luaF_close_or_warning(L, fd);

    if (epoll_emask_has_errors(emask)) {
        luaF_error_fd(L, fd, epoll_emask_error_label(emask));
    }

    return 0;
}
