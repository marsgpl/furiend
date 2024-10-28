#ifndef LUA_LIB_REDIS_H
#define LUA_LIB_REDIS_H

#include <furiend/shared.h>
#include "resp.h"

#define MT_REDIS_CLIENT "redis.client*"

#define REDIS_UDUVIDX_CONFIG 1
#define REDIS_UDUVIDX_Q_SUBS 2
#define REDIS_UDUVIDX_CONN_THREAD 3

#define READ_BUF_START_SIZE 8192
#define READ_BUF_MIN_SIZE READ_BUF_START_SIZE / 4

typedef struct {
    int fd;
    int connected;
    int can_write;
    int next_query_id;
    int next_answer_id;
    char *recv_buf;
    int recv_buf_len;
    int recv_buf_size;
} ud_redis_client;

LUAMOD_API int luaopen_redis(lua_State *L);

int redis_pack(lua_State *L);
int redis_unpack(lua_State *L);
int redis_client(lua_State *L);
int redis_client_gc(lua_State *L);
int redis_connect(lua_State *L);
int redis_query(lua_State *L);

static int connect_start(lua_State *L);
static int connect_continue(lua_State *L, int status, lua_KContext ctx);

static int router_start(lua_State *L);
static int router_continue(lua_State *L, int status, lua_KContext ctx);
static int router_process_event(lua_State *L);
static void router_read(lua_State *L, ud_redis_client *client, int t_subs_idx);

static int query_start(lua_State *L);
static int query_continue(lua_State *L, int status, lua_KContext ctx);

static const luaL_Reg redis_client_index[] = {
    { "connect", redis_connect },
    { "query", redis_query },
    { "close", redis_client_gc },
    { NULL, NULL }
};

#endif
