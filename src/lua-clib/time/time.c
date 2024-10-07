#include "time.h"

LUAMOD_API int luaopen_time(lua_State *L) {
    lua_pushcfunction(L, get_time);
    return 1;
}

static int get_time(lua_State *L) {
    luaF_need_args(L, 0, "time");

    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        luaF_error_errno(L, "clock_gettime failed");
    }

    lua_pushnumber(L, ts.tv_sec + ts.tv_nsec * 1e-9);

    return 1;
}
