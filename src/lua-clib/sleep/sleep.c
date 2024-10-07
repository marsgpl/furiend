#include "sleep.h"

LUAMOD_API int luaopen_sleep(lua_State *L) {
    lua_pushcfunction(L, sleep_create);
    return 1;
}

static int sleep_create(lua_State *L) {
    luaF_need_args(L, 1, "sleep");
    luaL_checktype(L, 1, LUA_TNUMBER); // duration_s

    lua_State *T = luaF_new_thread_or_error(L);

    lua_insert(L, 1); // num, T -> T, num
    lua_pushcfunction(T, sleep_start);
    lua_xmove(L, T, 1); // num -> T

    int nres;
    lua_resume(T, L, 1, &nres); // should yield, 0 nres

    return 1; // T
}

static int sleep_start(lua_State *L) {
    lua_Number duration_s = lua_tonumber(L, 1);

    luaF_set_timeout(L, duration_s);

    lua_settop(L, 0);

    return lua_yieldk(L, 0, 0, sleep_continue); // longjmp
}

static int sleep_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;
    (void)status;

    int fd = lua_tointeger(L, F_LOOP_FD_REL_IDX);

    luaF_close_or_warning(L, fd);
    luaF_loop_check_close(L);

    int emask = lua_tointeger(L, F_LOOP_EMASK_REL_IDX);

    if (emask_has_errors(emask)) {
        luaF_error_socket(L, fd, emask_error_label(emask));
    }

    return 0;
}
