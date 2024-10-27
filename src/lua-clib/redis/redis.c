#include "redis.h"

LUAMOD_API int luaopen_redis(lua_State *L) {
    if (likely(luaL_newmetatable(L, MT_REDIS_CLIENT))) {
        luaL_newlib(L, redis_client_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, redis_client_gc);
        lua_setfield(L, -2, "__gc");
    }

    luaL_newlib(L, redis_index);
    return 1;
}

int redis_client(lua_State *L) {
    luaF_need_args(L, 1, "redis.client");
    luaL_checktype(L, 1, LUA_TTABLE);

    ud_redis_client *client = luaF_new_uduv_or_error(L,
        sizeof(ud_redis_client), 2);

    client->fd = -1;
    client->connected = 0;
    client->can_write = 0;

    luaL_setmetatable(L, MT_REDIS_CLIENT);

    lua_insert(L, 1); // config, client -> client, config
    lua_setiuservalue(L, 1, REDIS_UDUVIDX_CONFIG);

    return 1;
}

int redis_client_gc(lua_State *L) {
    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);

    if (client->fd != -1) {
        luaF_close_or_warning(L, client->fd);
        client->fd = -1;
    }

    return 0;
}

int redis_connect(lua_State *L) {
    luaF_need_args(L, 1, "connect");

    int status, nres;

    lua_State *T = luaF_new_thread_or_error(L);

    lua_pushcfunction(T, connect_start);
    lua_pushvalue(L, 1); // client
    lua_xmove(L, T, 1); // client >> T

    status = lua_resume(T, L, 1, &nres); // should yield, 0 nres

    if (unlikely(status != LUA_YIELD)) {
        return 1;
    }

    lua_State *router = luaF_new_thread_or_error(L);

    lua_pushcfunction(router, router_start);
    lua_pushvalue(L, 1); // client
    lua_xmove(L, router, 1); // client >> router

    status = lua_resume(router, L, 1, &nres); // should yield, 0 nres

    if (unlikely(status != LUA_YIELD)) {
        luaL_error(L, "router init failed: %s", lua_tostring(router, -1));
    }

    lua_settop(L, 2); // client, T, router -> client, T

    return 1; // T
}

int redis_ping(lua_State *L) {
    luaF_need_args(L, 1, "ping");

    lua_State *T = luaF_new_thread_or_error(L);
    lua_insert(L, 1); // client, T -> T, client
    lua_pushcfunction(T, ping_start);
    lua_xmove(L, T, 1); // client >> T

    int nres;
    lua_resume(T, L, 1, &nres); // should yield, 0 nres

    return 1;
}

static int connect_start(lua_State *L) {
    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);

    if (unlikely(client->connected)) {
        luaL_error(L, "already connected");
    }

    if (unlikely(client->fd != -1)) {
        luaL_error(L, "already connecting");
    }

    lua_getiuservalue(L, 1, REDIS_UDUVIDX_CONFIG);

    lua_getfield(L, 2, "ip4");
    lua_getfield(L, 2, "port");

    const char *ip4 = luaL_checkstring(L, 3);
    int port = luaL_checkinteger(L, 4);

    struct sockaddr_in sa = {0};
    luaF_set_ip4_port(L, &sa, ip4, port);

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (unlikely(fd < 0)) {
        luaF_error_errno(L, "socket failed (tcp nonblock)");
    }

    client->fd = fd;

    int status = connect(fd, (struct sockaddr *)&sa, sizeof(sa));

    if (status != -1) {
        luaF_error_errno(L, "connected immediately; addr: %s:%d", ip4, port);
    } else if (errno != EINPROGRESS) {
        luaF_error_errno(L, "connect failed; addr: %s:%d", ip4, port);
    }

    lua_pushthread(L);
    lua_setiuservalue(L, 1, REDIS_UDUVIDX_CONN_THREAD);

    lua_settop(L, 1); // client

    return lua_yieldk(L, 0, 0, connect_continue);
}

static int connect_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;
    (void)status;

    lua_pushnil(L);
    lua_setiuservalue(L, 1, REDIS_UDUVIDX_CONN_THREAD);

    ud_redis_client *client = lua_touserdata(L, 1);
    int code = get_socket_error_code(client->fd);

    if (unlikely(code != 0)) {
        luaF_push_error_socket(L, client->fd, "connection failed", code);
        lua_error(L);
    }

    client->connected = 1;

    return 0;
}

static int router_start(lua_State *L) {
    ud_redis_client *client = lua_touserdata(L, 1);

    luaF_loop_watch(L, client->fd, EPOLLIN | EPOLLOUT | EPOLLET, 0);

    return lua_yieldk(L, 0, 0, router_continue);
}

static int router_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    lua_pushcfunction(L, router_process_event);
    lua_pushvalue(L, 1); // client
    lua_pushvalue(L, 3); // emask or errmsg
    status = lua_pcall(L, 2, 0, 0);

    if (unlikely(status != LUA_OK)) {
        lua_insert(L, 2); // client, ? <- err msg
        lua_settop(L, 2); // client, err msg
        return redis_client_gc(L);
    }

    lua_settop(L, 1); // client

    return lua_yieldk(L, 0, 0, router_continue); // keep waiting
}

static int router_process_event(lua_State *L) {
    luaF_loop_check_close(L);

    ud_redis_client *client = lua_touserdata(L, 1);
    int fd = client->fd;
    int emask = lua_tointeger(L, 2);

    if (unlikely(fd < 0)) {
        return luaL_error(L, "dns.client is closed");
    }

    if (unlikely(emask_has_errors(emask))) {
        luaF_error_socket(L, fd, emask_error_label(emask));
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);
    int t_subs_idx = lua_gettop(L);

    if (emask & EPOLLOUT) {
        if (unlikely(!client->connected)) {
            lua_getiuservalue(L, 1, REDIS_UDUVIDX_CONN_THREAD);
            int sub_idx = lua_gettop(L);
            lua_State *sub = lua_tothread(L, sub_idx);

            if (unlikely(sub == NULL)) {
                luaL_error(L, "redis client conn thread not found");
            }

            int sub_nres;
            int sub_status = lua_resume(sub, L, 0, &sub_nres);

            luaF_loop_notify_t_subs(L, t_subs_idx,
                sub, sub_idx, sub_status, sub_nres);

            if (unlikely(sub_status != LUA_OK)) {
                luaL_error(L, lua_tostring(sub, -1));
            }
        }
    }

    if (emask & EPOLLIN) {
    }

    return 0;
}

static int ping_start(lua_State *L) {
    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);

    if (unlikely(!client->connected)) {
        luaL_error(L, "not connected");
    }

    (void)ping_continue;

    return 0;
}

static int ping_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)status;
    (void)ctx;
    (void)L;
    return 0;
}
