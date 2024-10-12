#ifndef LUA_LIB_HTTP_SERVER_H
#define LUA_LIB_HTTP_SERVER_H

#include "shared.h"

#define MT_HTTP_SERVER "http.server*"

int http_server(lua_State *L);
int http_server_gc(lua_State *L);

#endif
