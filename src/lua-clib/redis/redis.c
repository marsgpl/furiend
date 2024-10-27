#include "redis.h"

LUAMOD_API int luaopen_redis(lua_State *L) {
    if (likely(luaL_newmetatable(L, MT_REDIS_CLIENT))) {
        luaL_newlib(L, redis_client_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, redis_client_gc);
        lua_setfield(L, -2, "__gc");
    }

    lua_createtable(L, 0, 2);

    lua_pushcfunction(L, redis_client);
    lua_setfield(L, -2, "client");

    lua_createtable(L, 0, 15);
        luaF_set_kv_int(L, -1, "STR", RESP_STR);
        luaF_set_kv_int(L, -1, "ERR", RESP_ERR);
        luaF_set_kv_int(L, -1, "INT", RESP_INT);
        luaF_set_kv_int(L, -1, "BULK", RESP_BULK);
        luaF_set_kv_int(L, -1, "ARR", RESP_ARR);
        luaF_set_kv_int(L, -1, "NULL", RESP_NULL);
        luaF_set_kv_int(L, -1, "BOOL", RESP_BOOL);
        luaF_set_kv_int(L, -1, "DOUBLE", RESP_DOUBLE);
        luaF_set_kv_int(L, -1, "BIG_NUM", RESP_BIG_NUM);
        luaF_set_kv_int(L, -1, "BULK_ERR", RESP_BULK_ERR);
        luaF_set_kv_int(L, -1, "VSTR", RESP_VSTR);
        luaF_set_kv_int(L, -1, "MAP", RESP_MAP);
        luaF_set_kv_int(L, -1, "AGGR", RESP_ATTR);
        luaF_set_kv_int(L, -1, "SET", RESP_SET);
        luaF_set_kv_int(L, -1, "PUSH", RESP_PUSH);
    lua_setfield(L, -2, "type");

    return 1;
}

int redis_client(lua_State *L) {
    luaF_need_args(L, 1, "redis.client");
    luaL_checktype(L, 1, LUA_TTABLE);

    ud_redis_client *client = luaF_new_uduv_or_error(L,
        sizeof(ud_redis_client), 3);

    client->fd = -1;
    client->connected = 0;
    client->can_write = 0;
    client->next_query_id = 1;
    client->next_answer_id = 1;

    client->recv_buf = malloc(READ_BUF_START_SIZE);
    client->recv_buf_len = 0;
    client->recv_buf_size = READ_BUF_START_SIZE;

    if (unlikely(client->recv_buf == NULL)) {
        luaF_error_errno(L, "malloc failed: %d", READ_BUF_START_SIZE);
    }

    luaL_setmetatable(L, MT_REDIS_CLIENT);

    lua_insert(L, 1); // config, client -> client, config
    lua_setiuservalue(L, 1, REDIS_UDUVIDX_CONFIG);

    lua_createtable(L, 0, 8); // expect 8 queries at once
    lua_setiuservalue(L, 1, REDIS_UDUVIDX_Q_SUBS);

    return 1;
}

int redis_client_gc(lua_State *L) {
    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);

    if (client->recv_buf != NULL) {
        free(client->recv_buf);
        client->recv_buf = NULL;
    }

    if (unlikely(client->fd < 0)) {
        return 0;
    }

    luaF_close_or_warning(L, client->fd);
    client->fd = -1;

    lua_settop(L, 2); // client, error msg or nil
    lua_getiuservalue(L, 1, REDIS_UDUVIDX_Q_SUBS);
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);

    int q_subs_idx = 3;
    int t_subs_idx = 4;

    lua_pushnil(L);
    while (lua_next(L, q_subs_idx)) {
        int sub_idx = lua_gettop(L);
        lua_State *sub = lua_tothread(L, sub_idx);

        lua_pushboolean(sub, 0);
        lua_pushstring(sub, lua_isstring(L, 2)
            ? lua_tostring(L, 2) // gc called from router
            : "redis.client.gc called");
        lua_pushinteger(sub, 0); // fake type

        int sub_nres;
        int sub_status = lua_resume(sub, L, 3, &sub_nres); // shouldn't yield

        luaF_loop_notify_t_subs(L, t_subs_idx,
            sub, sub_idx, sub_status, sub_nres);

        lua_pop(L, 1); // lua_next
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
        client->can_write = 1;

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
        router_read(L, client, t_subs_idx);
    }

    return 0;
}

static void router_read(lua_State *L, ud_redis_client *client, int t_subs_idx) {
    int fd = client->fd;
    char *buf = client->recv_buf;
    int len = client->recv_buf_len;
    int size = client->recv_buf_size;

    while (1) {
        if (unlikely(size - len < READ_BUF_MIN_SIZE)) {
            luaL_error(L, "TODO: resize recv buf");
        }

        ssize_t read = recv(fd, buf + len, size - len, 0);

        if (unlikely(read == 0)) {
            luaL_error(L, "server dropped out during client read");
        } else if (read < 0) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_error_errno(L, "recv failed; fd: %d", fd);
            }
            break;
        }

        len += read;
    }

    if (unlikely(client->recv_buf_len == len)) {
        return; // did not get new bytes, no need to parse again
    }

    lua_getiuservalue(L, 1, REDIS_UDUVIDX_Q_SUBS);
    int q_subs_idx = lua_gettop(L);

    int total_parsed = 0;

    while (len - total_parsed > 2) {
        int parsed = resp_unpack(L, buf + total_parsed, len - total_parsed, 1);

        if (unlikely(parsed == 0)) {
            break; // not enough data, wait for more
        }

        total_parsed += parsed;

        int answer_id = client->next_answer_id++;
        int type = lua_rawgeti(L, q_subs_idx, answer_id);

        if (unlikely(type != LUA_TTHREAD)) {
            luaL_error(L, "query thread not found by id: %d", answer_id);
        }

        lua_pushnil(L);
        lua_rawseti(L, q_subs_idx, answer_id);

        int sub_idx = lua_gettop(L) - 2;
        lua_insert(L, sub_idx); // data, type, sub -> sub, data, type
        lua_State *sub = lua_tothread(L, sub_idx);

        lua_pushboolean(sub, 1);
        lua_xmove(L, sub, 2); // data, type >> sub

        int sub_nres;
        int sub_status = lua_resume(sub, L, 3, &sub_nres); // doesn't yield

        luaF_loop_notify_t_subs(L, t_subs_idx,
            sub, sub_idx, sub_status, sub_nres);

        lua_pop(L, 1); // lua_rawgeti
    }

    client->recv_buf_len = len - total_parsed;

    if (unlikely(total_parsed > 0 && total_parsed < len)) {
        memmove(client->recv_buf,
            client->recv_buf + total_parsed,
            client->recv_buf_len);
    }
}

int redis_query(lua_State *L) {
    luaF_need_args(L, 2, "query");
    luaL_checktype(L, 2, LUA_TSTRING);

    lua_State *T = luaF_new_thread_or_error(L);

    lua_insert(L, 1); // client, query, T -> T, client, query
    lua_pushcfunction(T, query_start);
    lua_xmove(L, T, 2); // client, query >> T

    int nres;
    lua_resume(T, L, 2, &nres); // should yield, 0 nres

    return 1;
}

static int query_start(lua_State *L) {
    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);
    size_t query_len;
    const char *query = lua_tolstring(L, 2, &query_len);

    if (unlikely(!client->connected)) {
        luaL_error(L, "not connected");
    }

    if (unlikely(query_len < 3)) {
        luaL_error(L, "query is too small: %d; min: %d", query_len, 3);
    }

    if (query[query_len - 2] != '\r' || query[query_len - 1] != '\n') {
        luaL_error(L, "query should end with \\r\\n");
    }

    if (likely(client->can_write)) {
        while (query_len > 0) {
            ssize_t sent = send(client->fd, query, query_len, MSG_NOSIGNAL);

            if (unlikely(sent == 0)) {
                luaL_error(L, "server dropped out during client send");
            } else if (sent < 0) {
                if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                    luaF_error_errno(L, "query failed");
                }

                client->can_write = 0;

                break; // try again later
            }

            query += sent;
            query_len -= sent;
        }
    }

    if (unlikely(query_len > 0)) {
        luaL_error(L, "TODO: redis client send buf: append; can_write: %d; query_len: %d", client->can_write, query_len);
    }

    int query_id = client->next_query_id++;

    lua_getiuservalue(L, 1, REDIS_UDUVIDX_Q_SUBS);
    lua_pushthread(L);
    lua_rawseti(L, -2, query_id);

    lua_settop(L, 1);

    return lua_yieldk(L, 0, 0, query_continue);
}

static int query_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;
    (void)status;

    if (
        lua_toboolean(L, -3) == 0 || // communication error
        lua_tointeger(L, -1) == RESP_ERR ||
        lua_tointeger(L, -1) == RESP_BULK_ERR
    ) {
        lua_pop(L, 1); // type
        lua_error(L); // -1 = err msg
    }

    return 2; // type, response
}
