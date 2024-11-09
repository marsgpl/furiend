#ifndef LUA_LIB_REDIS_H
#define LUA_LIB_REDIS_H

#define _GNU_SOURCE

#include <string.h>
#include <furiend/shared.h>
#include <furiend/strbuf.h>
#include "resp.h"

#define MT_REDIS "redis*"
#define MT_REDIS_CLIENT "redis.client*"

#define REDIS_UV_IDX_CONFIG 1
#define REDIS_UV_IDX_Q_SUBS 2
#define REDIS_UV_IDX_PUSH_CBS 3
#define REDIS_UV_IDX_CONN_THREAD 4
#define REDIS_UV_IDX_JOIN_THREAD 5
#define REDIS_UV_IDX_N 5

#define PACK_BUF_START_SIZE 8192
#define SEND_BUF_START_SIZE 8192
#define RECV_BUF_START_SIZE 8192
#define RECV_BUF_MIN_SIZE 2048

#define DEFAULT_RESP_VER 3
#define DEFAULT_USERNAME "default"

typedef struct {
    luaF_strbuf pack_buf;
} ud_redis;

typedef struct {
    int fd;
    int connected;
    int can_write;
    lua_Integer next_query_id;
    lua_Integer next_answer_id;
    luaF_strbuf pack_buf;
    luaF_strbuf send_buf;
    luaF_strbuf recv_buf;
} ud_redis_client;

LUAMOD_API int luaopen_redis(lua_State *L);

int redis_pack(lua_State *L);
int redis_unpack(lua_State *L);
int redis_gc(lua_State *L);
int redis_client(lua_State *L);
int redis_client_gc(lua_State *L);
int redis_connect(lua_State *L);
int redis_query(lua_State *L);
int redis_hello(lua_State *L);
int redis_ping(lua_State *L);
int redis_join(lua_State *L);
int redis_subscribe(lua_State *L);
int redis_publish(lua_State *L);
int redis_unsubscribe(lua_State *L);

static void error_if_dead(lua_State *L);
static int connect_start(lua_State *L);
static int connect_continue(lua_State *L, int status, lua_KContext ctx);
static int router_start(lua_State *L);
static int router_continue(lua_State *L, int status, lua_KContext ctx);
static int router_process_event(lua_State *L);
static void router_on_connect(lua_State *L, int t_subs_idx);
static void router_process_send_buf(lua_State *L, ud_redis_client *client);

static void router_read(
    lua_State *L,
    ud_redis_client *client,
    int t_subs_idx);

static int router_parse(
    lua_State *L,
    ud_redis_client *client,
    int t_subs_idx);

static int query_start(lua_State *L);
static int query_continue(lua_State *L, int status, lua_KContext ctx);

static int join_start(lua_State *L);
static int join_continue(lua_State *L, int status, lua_KContext ctx);

static const luaL_Reg redis_index[] = {
    { "client", redis_client },
    { "pack", redis_pack },
    { "unpack", redis_unpack },
    { "type", NULL }, // just reserve space
    { NULL, NULL }
};

static const luaL_Reg redis_client_index[] = {
    { "connect", redis_connect },
    { "query", redis_query },
    { "hello", redis_hello },
    { "ping", redis_ping },
    { "join", redis_join },
    { "subscribe", redis_subscribe },
    { "publish", redis_publish },
    { "unsubscribe", redis_unsubscribe },
    { "close", redis_client_gc },
    { NULL, NULL }
};

#endif
