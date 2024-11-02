#ifndef LUA_LIB_HTTP_H
#define LUA_LIB_HTTP_H

#include "request.h"
#include "server.h"

LUAMOD_API int luaopen_http(lua_State *L);

static const luaL_Reg http_index[] = {
    { "request", http_request },
    { "server", http_serv },
    { NULL, NULL }
};

static const luaL_Reg http_serv_index[] = {
    { "on_request", http_serv_on_request },
    { "on_error", http_serv_on_error },
    { "listen", http_serv_listen },
    { "stop", http_serv_gc },
    { "join", http_serv_join },
    { NULL, NULL }
};

static const luaL_Reg http_serv_request_index[] = {
    { NULL, NULL }
};

static const luaL_Reg http_serv_res_index[] = {
    { "set_status", http_serv_res_set_status },
    { "push_header", http_serv_res_push_header },
    { "set_body", http_serv_res_set_body },
    { NULL, NULL }
};

#endif
