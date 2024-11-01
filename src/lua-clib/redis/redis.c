#include "redis.h"

LUAMOD_API int luaopen_redis(lua_State *L) {
    luaL_newmetatable(L, MT_REDIS_CLIENT);

    lua_pushcfunction(L, redis_client_gc);
    lua_setfield(L, -2, "__gc");
    luaL_newlib(L, redis_client_index);
    lua_setfield(L, -2, "__index");

    luaL_newmetatable(L, MT_REDIS);

    lua_pushcfunction(L, redis_gc);
    lua_setfield(L, -2, "__gc");
    luaL_newlib(L, redis_index);

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
        luaF_set_kv_int(L, -1, "ATTR", RESP_ATTR);
        luaF_set_kv_int(L, -1, "SET", RESP_SET);
        luaF_set_kv_int(L, -1, "PUSH", RESP_PUSH);
    lua_setfield(L, -2, "type");

    lua_setfield(L, -2, "__index");

    ud_redis *redis = luaF_new_uduv_or_error(L, sizeof(ud_redis), 0);

    redis->pack_buf = luaF_strbuf_create(PACK_BUF_START_SIZE);

    luaL_setmetatable(L, MT_REDIS);

    return 1;
}

int redis_gc(lua_State *L) {
    ud_redis *redis = luaL_checkudata(L, 1, MT_REDIS);
    luaF_strbuf *sb = &redis->pack_buf;

    luaF_strbuf_free_buf(sb);

    return 0;
}

int redis_pack(lua_State *L) {
    luaF_need_args(L, 2, "redis.pack");

    ud_redis *redis = luaL_checkudata(L, 1, MT_REDIS);
    luaF_strbuf *sb = &redis->pack_buf;

    resp_pack(L, sb, 1);
    lua_pushlstring(L, sb->buf, sb->filled);

    return 1;
}

int redis_unpack(lua_State *L) {
    luaF_need_args(L, 1, "redis.unpack");
    luaL_checktype(L, 1, LUA_TSTRING);

    size_t len;
    const char *buf = lua_tolstring(L, 1, &len);

    if (unlikely(len > INT_MAX)) {
        luaL_error(L, "buffer is too big: %d; max: %d", len, INT_MAX);
    }

    size_t parsed = resp_unpack(L, buf, len, 1);

    if (unlikely(parsed == 0)) {
        luaL_error(L, "not enough data");
    }

    if (parsed != len) {
        luaL_error(L, "bytes left after parsing: %d", len - parsed);
    }

    return 2; // data, type
}

int redis_client(lua_State *L) {
    luaF_need_args(L, 1, "redis.client");
    luaL_checktype(L, 1, LUA_TTABLE);

    ud_redis_client *client = luaF_new_uduv_or_error(L,
        sizeof(ud_redis_client), 4);

    client->fd = -1;
    client->connected = 0;
    client->can_write = 0;

    client->next_query_id = 1;
    client->next_answer_id = 1;

    client->pack_buf = luaF_strbuf_create(PACK_BUF_START_SIZE);
    client->send_buf = luaF_strbuf_create(SEND_BUF_START_SIZE);
    client->recv_buf = luaF_strbuf_create(RECV_BUF_START_SIZE);

    luaL_setmetatable(L, MT_REDIS_CLIENT);

    lua_insert(L, 1); // config, client -> client, config
    lua_setiuservalue(L, 1, REDIS_UDUVIDX_CONFIG);

    lua_createtable(L, 0, 8); // expect 8 queries at once
    lua_setiuservalue(L, 1, REDIS_UDUVIDX_Q_SUBS);

    lua_createtable(L, 0, 4); // expect 2 push cbs at once
    lua_setiuservalue(L, 1, REDIS_UDUVIDX_PUSH_CBS);
    // 4 fields for 2 cbs: each cb takes 2 fields
    // push_cbs[channel_name] = cb
    // push_cbs[cb] = confirmed (true/false)
    // this needed because redis response on subscibe cmd is a push

    return 1;
}

int redis_client_gc(lua_State *L) {
    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);

    luaF_strbuf_free_buf(&client->pack_buf);
    luaF_strbuf_free_buf(&client->send_buf);
    luaF_strbuf_free_buf(&client->recv_buf);

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
            router_on_connect(L, t_subs_idx);
        }

        if (unlikely(client->send_buf.filled > 0)) {
            router_process_send_buf(L, client);
        }
    }

    if (emask & EPOLLIN) {
        router_read(L, client, t_subs_idx);
    }

    return 0;
}

static void router_on_connect(lua_State *L, int t_subs_idx) {
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

static void router_read(
    lua_State *L,
    ud_redis_client *client,
    int t_subs_idx
) {
    int top = lua_gettop(L);
    luaF_strbuf *sb = &client->recv_buf;

    while (1) {
        lua_settop(L, top);
        luaF_strbuf_ensure_space(L, sb, RECV_BUF_MIN_SIZE);

        ssize_t read = luaF_strbuf_recv(L, sb, client->fd, 0);

        if (unlikely(read == 0)) {
            luaL_error(L, "server dropped out during client read");
        }

        if (unlikely(read < 0)) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_error_errno(L, "recv failed; fd: %d", client->fd);
            }
            return; // try to read again later
        }

        size_t parsed = router_parse(L, client, t_subs_idx);

        if (unlikely(client->fd < 0)) {
            return; // router_parse could trigger client gc
        }

        luaF_strbuf_shift(L, sb, parsed);
    }
}

static int router_parse(
    lua_State *L,
    ud_redis_client *client,
    int t_subs_idx
) {
    lua_getiuservalue(L, 1, REDIS_UDUVIDX_PUSH_CBS);
    lua_getiuservalue(L, 1, REDIS_UDUVIDX_Q_SUBS);

    int push_cbs_idx = lua_gettop(L) - 1;
    int q_subs_idx = lua_gettop(L);

    size_t total_parsed = 0;
    luaF_strbuf *sb = &client->recv_buf;

    while (sb->filled - total_parsed > 2) {
        lua_settop(L, q_subs_idx);

        size_t parsed = resp_unpack(L,
            sb->buf + total_parsed,
            sb->filled - total_parsed,
            1);

        if (unlikely(parsed == 0)) {
            return total_parsed; // incomplete packet, stop parsing
        }

        total_parsed += parsed;

        if (lua_tointeger(L, -1) == RESP_PUSH) {
            lua_rawgeti(L, -2, 2); // push[2] - channel name
            lua_rawget(L, push_cbs_idx);

            // cbs[cb] = confirmed
            int type = lua_rawgetp(L, push_cbs_idx, lua_topointer(L, -1));

            if (type == LUA_TBOOLEAN) { // normal push
                lua_pop(L, 1); // lua_rawgetp
                lua_insert(L, lua_gettop(L) - 2); // cb, data, type

                lua_State *T = luaF_new_thread_or_error(L); // cb, data, type, T
                lua_insert(L, lua_gettop(L) - 3); // T, cb, data, type
                lua_xmove(L, T, 3); // cb, data, type >> T

                int nres;
                int status = lua_resume(T, L, 2, &nres);

                if (unlikely(status != LUA_OK && status != LUA_YIELD)) {
                    luaL_error(L, "push callback error: %s",
                        lua_tostring(T, -1));
                }

                continue;
            } else { // notify "subscribe" cmd with this push
                lua_pop(L, 1); // lua_rawgetp
                lua_pushboolean(L, 1); // data, type, cb, true
                lua_rawsetp(L, push_cbs_idx, lua_topointer(L, -2));
                lua_pop(L, 1); // data, type
                // notify normally: data, type
            }
        }

        lua_Integer answer_id = client->next_answer_id++;

        if (unlikely(lua_rawgeti(L, q_subs_idx, answer_id) != LUA_TTHREAD)) {
            luaL_error(L, "query thread not found by id: %d", answer_id);
        }

        lua_pushnil(L);
        lua_rawseti(L, q_subs_idx, answer_id); // subs[answer_id] = nil

        int sub_idx = lua_gettop(L) - 2;
        lua_insert(L, sub_idx); // data, type, sub -> sub, data, type
        lua_State *sub = lua_tothread(L, sub_idx);

        lua_pushboolean(sub, 1);
        lua_xmove(L, sub, 2); // data, type >> sub

        int sub_nres;
        int sub_status = lua_resume(sub, L, 3, &sub_nres); // doesn't yield

        luaF_loop_notify_t_subs(L, t_subs_idx,
            sub, sub_idx, sub_status, sub_nres);

        if (unlikely(client->fd < 0)) {
            break; // lua_resume could trigger client gc
        }
    }

    return total_parsed;
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
        luaF_strbuf_append(L, &client->send_buf, query, query_len);
    }

    lua_Integer query_id = client->next_query_id++;

    lua_getiuservalue(L, 1, REDIS_UDUVIDX_Q_SUBS);
    lua_pushthread(L);
    lua_rawseti(L, -2, query_id);

    lua_settop(L, 1);

    return lua_yieldk(L, 0, 0, query_continue);
}

static void router_process_send_buf(lua_State *L, ud_redis_client *client) {
    luaF_strbuf *sb = &client->send_buf;

    while (sb->filled > 0) {
        ssize_t sent = send(client->fd, sb->buf, sb->filled, MSG_NOSIGNAL);

        if (unlikely(sent == 0)) {
            luaL_error(L, "server dropped out during client send");
        } else if (sent < 0) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_error_errno(L, "query failed");
            }

            client->can_write = 0;

            break; // try again later
        }

        luaF_strbuf_shift(L, sb, sent);
    }
}

static int query_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    status = lua_toboolean(L, -3);
    int type = lua_tointeger(L, -1);

    if (unlikely(!status || type == RESP_ERR || type == RESP_BULK_ERR)) {
        lua_pop(L, 1); // -type
        lua_error(L); // -1 = err msg
    }

    return 2; // data, type
}

int redis_subscribe(lua_State *L) {
    luaF_need_args(L, 3, "subscribe");
    luaL_checktype(L, 2, LUA_TSTRING); // channel name
    luaL_checktype(L, 3, LUA_TFUNCTION); // on push callback

    lua_getiuservalue(L, 1, REDIS_UDUVIDX_PUSH_CBS); // ud, ch, cb, cbs
    lua_insert(L, 2); // ud, cbs, ch, cb
    lua_pushvalue(L, 3); // ud, cbs, ch, cb, ch
    lua_insert(L, 2); // ud, ch, cbs, ch, cb
    lua_rawset(L, 3); // push_cbs[ch_name] = on_push
    lua_pop(L, 1); // ud, ch

    lua_createtable(L, 2, 0);
    lua_insert(L, 2); // ud, table, ch_name

    lua_pushstring(L, "subscribe");
    lua_rawseti(L, 2, 1); // table[1] = "subscribe"
    lua_rawseti(L, 2, 2); // table[2] = ch_name

    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);
    luaF_strbuf *sb = &client->pack_buf;

    resp_pack(L, sb, 2);
    lua_pushlstring(L, sb->buf, sb->filled); // ud, table, query
    lua_insert(L, 2); // ud, query, table
    lua_pop(L, 1); // ud, query

    lua_pushcfunction(L, redis_query); // ud, query, fn
    lua_insert(L, 1); // fn, ud, query
    lua_call(L, 2, 1); // thread

    return 1; // thread
}

int redis_unsubscribe(lua_State *L) {
    luaF_need_args(L, 2, "unsubscribe");
    luaL_checktype(L, 2, LUA_TSTRING); // channel name

    lua_getiuservalue(L, 1, REDIS_UDUVIDX_PUSH_CBS); // ud, ch, cbs
    lua_pushvalue(L, 2); // ud, ch, cbs, ch
    lua_pushnil(L); // ud, ch, cbs, ch, nil
    lua_rawset(L, 3); // push_cbs[ch_name] = nil
    lua_pop(L, 1); // ud, ch

    lua_createtable(L, 2, 0);
    lua_insert(L, 2); // ud, table, ch_name

    lua_pushstring(L, "unsubscribe");
    lua_rawseti(L, 2, 1); // table[1] = "unsubscribe"
    lua_rawseti(L, 2, 2); // table[2] = ch_name

    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);
    luaF_strbuf *sb = &client->pack_buf;

    resp_pack(L, sb, 2);
    lua_pushlstring(L, sb->buf, sb->filled); // ud, table, query
    lua_insert(L, 2); // ud, query, table
    lua_pop(L, 1); // ud, query

    lua_pushcfunction(L, redis_query); // ud, query, fn
    lua_insert(L, 1); // fn, ud, query
    lua_call(L, 2, 1); // thread

    return 1; // thread
}

int redis_publish(lua_State *L) {
    luaF_need_args(L, 3, "publish");
    luaL_checktype(L, 2, LUA_TSTRING); // channel name
    luaL_checktype(L, 3, LUA_TSTRING); // message

    lua_createtable(L, 3, 0); // ud, ch_name, payload, table
    lua_insert(L, 2); // ud, table, ch_name, payload
    lua_insert(L, 3); // ud, table, payload, ch_name

    lua_pushstring(L, "publish");
    lua_rawseti(L, 2, 1); // table[1] = "publish"
    lua_rawseti(L, 2, 2); // table[2] = ch_name
    lua_rawseti(L, 2, 3); // table[3] = payload

    ud_redis_client *client = luaL_checkudata(L, 1, MT_REDIS_CLIENT);
    luaF_strbuf *sb = &client->pack_buf;

    resp_pack(L, sb, 2);
    lua_pushlstring(L, sb->buf, sb->filled); // ud, table, query
    lua_insert(L, 2); // ud, query, table
    lua_pop(L, 1); // ud, query

    lua_pushcfunction(L, redis_query); // ud, query, fn
    lua_insert(L, 1); // fn, ud, query
    lua_call(L, 2, 1); // thread

    return 1; // thread
}
