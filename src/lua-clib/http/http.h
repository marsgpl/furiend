#ifndef LUA_LIB_HTTP_H
#define LUA_LIB_HTTP_H

#include "request.h"
#include "server.h"

LUAMOD_API int luaopen_http(lua_State *L);

static const luaL_Reg http_index[] = {
    { "request", http_request },
    { "server", http_server },
    { NULL, NULL }
};

#endif
