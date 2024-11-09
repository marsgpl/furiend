#include "server.h"

static int listen_start(lua_State *L);
static int listen_continue(lua_State *L, int status, lua_KContext ctx);
static void listen_accept(lua_State *L, ud_http_serv *serv);
static int client_start(lua_State *L);
static int client_wait_read(lua_State *L, int status, lua_KContext ctx);
static int client_process_read(lua_State *L);
static void client_read(lua_State *L, ud_http_serv_client *client);
static void client_parse_headers(lua_State *L, ud_http_serv_client *client);
static int client_call_gc(lua_State *L);
static int client_respond(lua_State *L);
static void client_build_response(lua_State *L, ud_http_serv_client *client);
static int client_wait_write(lua_State *L, int status, lua_KContext ctx);
static int client_process_write(lua_State *L, ud_http_serv_client *client);
static int on_request_finish(lua_State *L, int status, lua_KContext ctx);
static int on_error_finish(lua_State *L, int status, lua_KContext ctx);
static int default_on_request(lua_State *L);
static int default_on_error(lua_State *L);
static void parse_conf(lua_State *L, ud_http_serv *serv, int conf_idx);
static void check_conf(lua_State *L, ud_http_serv *serv);
static int http_serv_join_start(lua_State *L);
static int http_serv_join_continue(lua_State *L, int status, lua_KContext ctx);

int http_serv(lua_State *L) {
    luaF_need_args(L, 1, "http server");
    luaL_checktype(L, 1, LUA_TTABLE); // conf

    ud_http_serv *serv = luaF_new_ud_or_error(L,
        sizeof(ud_http_serv), SERV_UV_IDX_N);

    serv->fd = -1;

    parse_conf(L, serv, 1);
    check_conf(L, serv);

    luaL_setmetatable(L, MT_HTTP_SERV);

    lua_insert(L, 1); // config, serv -> serv, config
    lua_setiuservalue(L, HTTP_SERV_IDX, SERV_UV_IDX_CONFIG);

    lua_createtable(L, 0, HTTP_SERV_EXPECT_MIN_CLIENTS);
    lua_setiuservalue(L, HTTP_SERV_IDX, SERV_UV_IDX_CLIENTS);

    lua_pushcfunction(L, default_on_request);
    lua_setiuservalue(L, HTTP_SERV_IDX, SERV_UV_IDX_ON_REQUEST);

    lua_pushcfunction(L, default_on_error);
    lua_setiuservalue(L, HTTP_SERV_IDX, SERV_UV_IDX_ON_ERROR);

    return 1;
}

int http_serv_gc(lua_State *L) {
    ud_http_serv *serv = luaL_checkudata(L, 1, MT_HTTP_SERV);

    if (serv->fd != -1) {
        luaF_close_or_warning(L, serv->fd);
        serv->fd = -1;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);
    int t_subs_idx = 4;

    lua_getiuservalue(L, 1, SERV_UV_IDX_JOIN_THREAD);
    luaF_resume(L, t_subs_idx, lua_tothread(L, -1), -1, 0);

    return 0;
}

int http_serv_listen(lua_State *L) {
    luaF_need_args(L, 1, "http server listen");
    luaL_checkudata(L, 1, MT_HTTP_SERV);

    lua_State *T = luaF_new_thread_or_error(L);

    lua_insert(L, 1); // serv, T -> T, serv
    lua_pushcfunction(T, listen_start);
    lua_xmove(L, T, 1); // serv >> T

    lua_resume(T, L, 1, &(int){0}); // should yield, 0 nres

    return 1;
}

int http_serv_on_request(lua_State *L) {
    luaF_need_args(L, 2, "on request");
    luaL_checkudata(L, 1, MT_HTTP_SERV);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_setiuservalue(L, 1, SERV_UV_IDX_ON_REQUEST);
    return 1;
}

int http_serv_on_error(lua_State *L) {
    luaF_need_args(L, 2, "on request error");
    luaL_checkudata(L, 1, MT_HTTP_SERV);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_setiuservalue(L, 1, SERV_UV_IDX_ON_ERROR);
    return 1;
}

int http_serv_client_gc(lua_State *L) {
    ud_http_serv_client *client = luaL_checkudata(L, 1, MT_HTTP_SERV_CLIENT);

    if (client->fd != -1) {
        lua_getiuservalue(L, 1, CLIENT_UV_IDX_CLIENTS);
        lua_pushnil(L);
        lua_rawseti(L, -2, client->fd);

        luaF_close_or_warning(L, client->fd);
        client->fd = -1;
    }

    if (client->req != NULL) {
        free(client->req);
        client->req = NULL;
    }

    if (client->res_headers != NULL && !client->is_fallback_res) {
        free(client->res_headers);
        client->res_headers = NULL;
    }

    return 0;
}

// res, 200, "OK"
int http_serv_res_set_status(lua_State *L) {
    luaF_min_max_args(L, 2, 3, "set response status");
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TNUMBER);

    if (lua_gettop(L) < 3) {
        lua_pushliteral(L, "");
    } else {
        luaL_checktype(L, 3, LUA_TSTRING);
    }

    lua_setfield(L, 1, "status_message");
    lua_setfield(L, 1, "status_code");

    return 1; // res
}

// res, "Content-Type", "application/json"
int http_serv_res_push_header(lua_State *L) {
    luaF_need_args(L, 3, "set response header");
    luaL_checktype(L, 1, LUA_TTABLE);

    int type = lua_getfield(L, 1, "headers");

    lua_pushfstring(L, "%s: %s" SEP,
        lua_tostring(L, 2),
        lua_tostring(L, 3));

    if (type == LUA_TSTRING) {
        lua_concat(L, 2);
    }

    lua_setfield(L, 1, "headers");
    lua_settop(L, 1);

    return 1; // res
}

// res, "{\"ok\":true}"
int http_serv_res_set_body(lua_State *L) {
    luaF_need_args(L, 2, "set response body");
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TSTRING);

    lua_setfield(L, 1, "body");

    return 1; // res
}

static int listen_start(lua_State *L) {
    ud_http_serv *serv = lua_touserdata(L, HTTP_SERV_IDX);

    int status;
    http_serv_conf *conf = &(serv->conf);

    struct sockaddr_in sa = {0};
    luaF_set_ip4_port(L, &sa, conf->ip4, conf->port);

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (unlikely(fd < 0)) {
        luaF_error_errno(L, "socket failed (tcp nonblock)");
    }

    serv->fd = fd;

    status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (unlikely(status < 0)) {
        luaF_error_errno(L, "setsockopt failed; fd: %d; SO_REUSEADDR", fd);
    }

    status = bind(fd, (struct sockaddr *)&sa, sizeof(sa));

    if (unlikely(status < 0)) {
        luaF_error_errno(L, "bind failed; fd: %d; ip4: %s; port: %d",
            fd, conf->ip4, conf->port);
    }

    status = listen(fd, HTTP_SERV_DEFAULT_BACKLOG);

    if (unlikely(status < 0)) {
        luaF_error_errno(L, "listen failed; fd: %d; backlog: %d",
            fd, HTTP_SERV_DEFAULT_BACKLOG);
    }

    luaF_loop_watch(L, fd, EPOLLIN | EPOLLET, 0);

    lua_settop(L, HTTP_SERV_IDX);
    return lua_yieldk(L, 0, 0, listen_continue);
}

static int listen_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;
    (void)status;

    luaF_loop_check_close(L);

    ud_http_serv *serv = lua_touserdata(L, HTTP_SERV_IDX);
    int fd = lua_tointeger(L, F_LOOP_FD_REL_IDX);
    int emask = lua_tointeger(L, F_LOOP_EMASK_REL_IDX);

    if (unlikely(emask_has_errors(emask))) {
        luaF_error_socket(L, fd, emask_error_label(emask));
    }

    listen_accept(L, serv);

    lua_settop(L, HTTP_SERV_IDX);
    return lua_yieldk(L, 0, 0, listen_continue);
}

static void listen_accept(lua_State *L, ud_http_serv *serv) {
    char ip4[INET_ADDRSTRLEN];
    struct sockaddr_in sa;
    socklen_t len = sizeof(sa);

    while (1) {
        lua_settop(L, HTTP_SERV_IDX);

        int fd = accept4(serv->fd, (struct sockaddr *)&sa, &len, SOCK_NONBLOCK);

        if (unlikely(fd < 0)) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_error_errno(L, "accept4 failed; fd: %d", serv->fd);
            }
            return; // try again later
        }

        // accept() already calls getpeername() if sockaddr is provided
        // getpeername(fd, (struct sockaddr *)&sa, &len); // remote addr
        // getsockname(fd, (struct sockaddr *)&sa, &len); // local addr

        inet_ntop(sa.sin_family, &(sa.sin_addr), ip4, INET_ADDRSTRLEN);
        int port = ntohs(sa.sin_port);

        lua_State *T = lua_newthread(L);

        if (unlikely(T == NULL)) {
            luaF_close_or_warning(L, fd);
            luaF_warning_errno(L, "client_start failed: lua_newthread failed");
            continue;
        }

        lua_pushcfunction(T, client_start);

        lua_pushvalue(L, HTTP_SERV_IDX);
        lua_xmove(L, T, 1); // serv >> T

        lua_pushinteger(T, fd);

        lua_createtable(T, 0, 8);
        // fd, ip4, port, headers, method, path, version, body
        luaL_setmetatable(T, MT_HTTP_SERV_REQ);

        lua_createtable(T, 0, 4);
        // status_code, status_message, headers, body
        luaL_setmetatable(T, MT_HTTP_SERV_RES);

        int req_idx = lua_gettop(T) - 1;
        int res_idx = lua_gettop(T);

        // fill req

        lua_pushinteger(T, fd);
        lua_setfield(T, req_idx, "fd");
        lua_pushstring(T, ip4);
        lua_setfield(T, req_idx, "ip4");
        lua_pushinteger(T, port);
        lua_setfield(T, req_idx, "port");

        // fill res

        lua_pushinteger(T, 200);
        lua_setfield(T, res_idx, "status_code");
        lua_pushliteral(T, "OK");
        lua_setfield(T, res_idx, "status_message");

        // start

        int status = lua_resume(T, L, 4, &(int){0});

        if (unlikely(status != LUA_YIELD)) {
            luaF_warning(L, "client_start failed: %s", lua_tostring(T, -1));
            lua_settop(L, HTTP_SERV_IDX); // rm T
            lua_gc(L, LUA_GCCOLLECT); // call client gc to close fd
        }
    }
}

static int client_start(lua_State *L) {
    int fd = lua_tointeger(L, CLIENT_FD_IDX);

    ud_http_serv_client *client = lua_newuserdatauv(L,
        sizeof(ud_http_serv_client), 1);

    if (unlikely(client == NULL)) {
        luaF_close_or_warning(L, fd);
        luaF_error_errno(L, "lua_newuserdatauv failed: MT_HTTP_SERV_CLIENT");
    }

    lua_getiuservalue(L, CLIENT_SERV_IDX, SERV_UV_IDX_CLIENTS);
    lua_setiuservalue(L, CLIENT_CLIENT_IDX, CLIENT_UV_IDX_CLIENTS);

    client->fd = fd;
    client->body_ready = 0;
    client->is_fallback_res = 0;

    client->req = luaF_malloc_or_error(L, HTTP_QUERY_HEADERS_MAX_LEN);
    client->req_len = 0;
    client->req_full_len = 0;
    client->req_size = HTTP_QUERY_HEADERS_MAX_LEN - 1; // for nul
    client->req_sep = NULL; // not found yet

    client->res_headers = NULL;
    client->res_headers_len = 0;
    client->res_headers_len_sent = 0;

    client->res_body = NULL;
    client->res_body_len = 0;
    client->res_body_len_sent = 0;

    luaL_setmetatable(L, MT_HTTP_SERV_CLIENT);

    // watch fd

    luaF_loop_watch(L, fd, EPOLLIN | EPOLLOUT | EPOLLET, 0);

    // clients[fd] = client T

    lua_getiuservalue(L, CLIENT_CLIENT_IDX, CLIENT_UV_IDX_CLIENTS);
    lua_pushthread(L);
    lua_rawseti(L, -2, fd);

    // wait for socket

    lua_settop(L, CLIENT_CLIENT_IDX);
    return lua_yieldk(L, 0, 0, client_wait_read);
}

static int client_wait_read(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    int fd_idx = lua_gettop(L) - 1;
    int emask_idx = lua_gettop(L);

    lua_pushcfunction(L, client_process_read);
    lua_pushvalue(L, CLIENT_CLIENT_IDX);
    lua_pushvalue(L, CLIENT_REQ_IDX);
    lua_pushvalue(L, fd_idx);
    lua_pushvalue(L, emask_idx);
    status = lua_pcall(L, 4, 0, 0);

    if (unlikely(status != LUA_OK)) {
        luaF_warning(L, "client_process_read failed: %s", lua_tostring(L, -1));
        return client_call_gc(L);
    }

    lua_settop(L, CLIENT_CLIENT_IDX);

    ud_http_serv_client *client = lua_touserdata(L, CLIENT_CLIENT_IDX);

    if (unlikely(!client->body_ready)) {
        return lua_yieldk(L, 0, 0, client_wait_read);
    }

    lua_getiuservalue(L, CLIENT_SERV_IDX, SERV_UV_IDX_ON_REQUEST);
    lua_pushvalue(L, CLIENT_REQ_IDX);
    lua_pushvalue(L, CLIENT_RES_IDX);
    status = lua_pcallk(L, 2, 0, 0, 0, on_request_finish);
    return on_request_finish(L, status, 0);
}

static int client_process_read(lua_State *L) {
    luaF_loop_check_close(L);

    ud_http_serv_client *client = lua_touserdata(L, CLIENT_PROC_CLIENT_IDX);
    int fd = lua_tointeger(L, F_LOOP_FD_REL_IDX);
    int emask = lua_tointeger(L, F_LOOP_EMASK_REL_IDX);

    if (unlikely(emask_has_errors(emask))) {
        luaF_error_socket(L, fd, emask_error_label(emask));
    }

    if (emask & EPOLLIN) {
        client_read(L, client);
    }

    return 0;
}

static void client_read(lua_State *L, ud_http_serv_client *client) {
    while (!client->body_ready) {
        if (unlikely(client->req_len == client->req_size)) {
            // req buf is full but headers sep was not found yet
            luaL_error(L, "request headers are too big: %d", client->req_len);
        }

        ssize_t read = recv(client->fd,
            client->req + client->req_len,
            client->req_size - client->req_len,
            0);

        if (unlikely(read == 0)) {
            luaL_error(L, "client dropped the connection");
        } else if (read < 0) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_error_errno(L, "recv failed; fd: %d", client->fd);
            }
            return; // try again later
        }

        client->req_len += read;

        if (likely(client->req_sep == NULL)) { // look for headers
            size_t scan_off = client->req_len - read;
            size_t scan_len = read; // scan only newly received chunk

            if (unlikely(scan_off >= 3)) { // chunk is not first
                // scan prev +3 bytes becasue prev chunk could end on \r\n\r
                scan_off -= 3;
                scan_len += 3;
            }

            char *pos = memmem(client->req + scan_off, scan_len, SEP SEP, 4);

            if (likely(pos != NULL)) {
                client->req_sep = pos;
                client_parse_headers(L, client);
            }
        }

        if (client->req_sep && client->req_len >= client->req_full_len) {
            client->body_ready = 1;

            lua_pushlstring(L,
                client->req_sep + 4,
                client->req_full_len - (client->req_sep + 4 - client->req));
            lua_setfield(L, CLIENT_PROC_REQ_IDX, "body");

            return;
        }
    }
}

static void client_parse_headers(lua_State *L, ud_http_serv_client *client) {
    int headers_n = 0;
    char *cur = client->req;
    char *end = client->req_sep;

    while (cur != end) {
        if (*cur == '\n') headers_n++;
        cur++;
    }

    if (headers_n > HTTP_SERV_MAX_CLIENT_HEADERS_N) {
        luaL_error(L, "too many headers; max: %d; received: %d",
            HTTP_SERV_MAX_CLIENT_HEADERS_N, headers_n);
    }

    client->req[client->req_len] = '\0';

    headers_parser_state state;
    req_headline hline = {0};

    state.line = client->req;
    state.rest_len = client->req_len;
    state.is_chunked = 0;
    state.content_len = 0;

    parse_req_headline(&state, &hline);

    lua_pushlstring(L, hline.method, hline.method_len);
    lua_setfield(L, CLIENT_PROC_REQ_IDX, "method");

    lua_pushlstring(L, hline.path, hline.path_len);
    lua_setfield(L, CLIENT_PROC_REQ_IDX, "path");

    lua_pushlstring(L, hline.ver, hline.ver_len);
    lua_setfield(L, CLIENT_PROC_REQ_IDX, "version");

    lua_createtable(L, 0, headers_n);
    if (state.line > client->req) { // headline ok
        parse_headers(L, &state);
    }
    lua_setfield(L, CLIENT_PROC_REQ_IDX, "headers");

    int headers_len = state.line - client->req;
    client->req_full_len = headers_len + state.content_len;

    if (unlikely(client->req_full_len > client->req_size)) {
        char *buf = realloc(client->req, client->req_full_len);

        if (unlikely(buf == NULL)) {
            luaF_error_errno(L, "realloc failed; from: %d; to: %d",
                client->req_size, client->req_full_len);
        }

        client->req = buf;
        client->req_size = client->req_full_len;
        client->req_sep = buf + headers_len - 4;
    }
}

static int client_call_gc(lua_State *L) {
    lua_pushcfunction(L, http_serv_client_gc);
    lua_pushvalue(L, CLIENT_CLIENT_IDX);
    lua_call(L, 1, 0);
    return 0;
}

static int client_respond(lua_State *L) {
    ud_http_serv_client *client = lua_touserdata(L, CLIENT_CLIENT_IDX);

    client_build_response(L, client);

    if (likely(client_process_write(L, client))) {
        return client_call_gc(L);
    }

    lua_settop(L, CLIENT_CLIENT_IDX);
    return lua_yieldk(L, 0, 0, client_wait_write);
}

static void client_build_response(lua_State *L, ud_http_serv_client *client) {
    int top_idx = lua_gettop(L);

    lua_getfield(L, CLIENT_RES_IDX, "status_code");
    lua_getfield(L, CLIENT_RES_IDX, "status_message");
    lua_getfield(L, CLIENT_RES_IDX, "body");
    lua_getfield(L, CLIENT_RES_IDX, "headers");

    if (unlikely(!lua_isinteger(L, top_idx + 1))) {
        lua_pushinteger(L, 500);
        lua_replace(L, top_idx + 1);
    }

    if (unlikely(!lua_isstring(L, top_idx + 2))) {
        lua_pushliteral(L, "Internal Server Error");
        lua_replace(L, top_idx + 2);
    }

    if (unlikely(!lua_isstring(L, top_idx + 3))) {
        lua_pushliteral(L, "");
        lua_replace(L, top_idx + 3);
    }

    if (unlikely(!lua_isstring(L, top_idx + 4))) {
        lua_pushliteral(L, "");
        lua_replace(L, top_idx + 4);
    }

    int status_code = lua_tointeger(L, top_idx + 1);
    size_t status_msg_len;
    const char *status_msg = lua_tolstring(L, top_idx + 2, &status_msg_len);
    size_t body_len;
    const char *body = lua_tolstring(L, top_idx + 3, &body_len);
    size_t headers_len;
    const char *headers = lua_tolstring(L, top_idx + 4, &headers_len);

    lua_pushfstring(L, "%s: %d" SEP, HTTP_HDR_CONTENT_LEN, body_len);
    lua_pushliteral(L, HTTP_HDR_CONN_CLOSE_SEP);
    lua_concat(L, 3);
    headers = lua_tolstring(L, top_idx + 4, &headers_len);

    size_t head_len = strlen(HTTP_VERSION) + 1
        + uint_len(status_code) + 1
        + status_msg_len + 2
        + headers_len + 2
        + 1; // snprintf nul

    client->res_headers_len = head_len - 1;
    client->res_headers = malloc(head_len);

    if (unlikely(client->res_headers == NULL)) {
        luaF_warning(L, "http response headers malloc failed; size: %d",
            head_len);

        client->is_fallback_res = 1;
        client->res_headers = CLIENT_FALLBACK_HEADERS;
        client->res_headers_len = strlen(CLIENT_FALLBACK_HEADERS);

        lua_settop(L, top_idx);
        return;
    }

    client->res_body = body;
    client->res_body_len = body_len;

    int written = snprintf(client->res_headers, head_len,
        "%s %d %s" SEP "%s" SEP,
        HTTP_VERSION, status_code, status_msg, headers);

    if (unlikely((size_t)written != client->res_headers_len)) {
        luaF_warning(L,
            "http response headers snprintf failed; written: %d of %d",
            written, client->res_headers_len);
    }

    lua_settop(L, top_idx);
    return;
}

static int client_wait_write(lua_State *L, int status, lua_KContext ctx) {
    (void)status;
    (void)ctx;

    if (unlikely(lua_type(L, F_LOOP_ERRMSG_REL_IDX) == LUA_TSTRING)) {
        luaF_warning(L, "http response send failed: loop.gc: %s",
            lua_tostring(L, F_LOOP_ERRMSG_REL_IDX));
        return client_call_gc(L);
    }

    ud_http_serv_client *client = lua_touserdata(L, CLIENT_CLIENT_IDX);
    int fd = lua_tointeger(L, F_LOOP_FD_REL_IDX);
    int emask = lua_tointeger(L, F_LOOP_EMASK_REL_IDX);

    if (unlikely(emask_has_errors(emask))) {
        int code = get_socket_error_code(fd);
        luaF_push_error_socket(L, fd, emask_error_label(emask), code);
        luaF_warning(L, "http response send failed: epoll: %s",
            lua_tostring(L, -1));
        return client_call_gc(L);
    }

    if (likely(emask & EPOLLOUT)) {
        if (likely(client_process_write(L, client))) {
            return client_call_gc(L);
        }
    }

    lua_settop(L, CLIENT_CLIENT_IDX);
    return lua_yieldk(L, 0, 0, client_wait_write);
}

static int client_process_write(lua_State *L, ud_http_serv_client *client) {
    while (client->res_headers_len_sent < client->res_headers_len) {
        ssize_t sent = send(client->fd,
            client->res_headers + client->res_headers_len_sent,
            client->res_headers_len - client->res_headers_len_sent,
            MSG_NOSIGNAL);

        if (unlikely(sent == 0)) {
            luaF_warning_errno(L, "client dropped out during http server send");
            return 1; // writing done
        } else if (sent < 0) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_warning_errno(L, "http response headers send failed");
                return 1; // writing done
            }
            return 0; // try again later
        }

        client->res_headers_len_sent += sent;
    }

    while (client->res_body_len_sent < client->res_body_len) {
        ssize_t sent = send(client->fd,
            client->res_body + client->res_body_len_sent,
            client->res_body_len - client->res_body_len_sent,
            MSG_NOSIGNAL);

        if (unlikely(sent == 0)) {
            luaF_warning_errno(L, "client dropped out during http server send");
            return 1; // writing done
        } else if (sent < 0) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_warning_errno(L, "http response body send failed");
                return 1; // writing done
            }
            return 0; // try again later
        }

        client->res_body_len_sent += sent;
    }

    return 1; // writing done
}

static int on_request_finish(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    if (unlikely(status != LUA_OK && status != LUA_YIELD)) { // failed
        // reset response

        lua_pushnil(L);
        lua_setfield(L, CLIENT_RES_IDX, "headers");
        lua_pushinteger(L, 500);
        lua_setfield(L, CLIENT_RES_IDX, "status_code");
        lua_pushliteral(L, "Internal Server Error");
        lua_setfield(L, CLIENT_RES_IDX, "status_message");
        lua_pushnil(L);
        lua_setfield(L, CLIENT_RES_IDX, "body");

        // call on_error

        lua_getiuservalue(L, CLIENT_SERV_IDX, SERV_UV_IDX_ON_ERROR);
        lua_pushvalue(L, CLIENT_REQ_IDX);
        lua_pushvalue(L, CLIENT_RES_IDX);
        lua_pushvalue(L, -4);

        status = lua_pcallk(L, 3, 0, 0, 0, on_error_finish);
        return on_error_finish(L, status, 0);
    }

    return client_respond(L);
}

static int on_error_finish(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    if (unlikely(status != LUA_OK && status != LUA_YIELD)) { // failed
        luaF_warning(L, "http server on_error failed: %s", lua_tostring(L, -1));
    }

    return client_respond(L);
}

static int default_on_request(lua_State *L) {
    luaF_warning(L, "http server on_request is not set");
    luaF_warning(L, "incoming http request");
    return 0;
}

static int default_on_error(lua_State *L) {
    luaF_warning(L, "http server on_error is not set");
    luaF_warning(L, "http server on_request failed: %s", lua_tostring(L, 3));
    return 0;
}

static void parse_conf(lua_State *L, ud_http_serv *serv, int conf_idx) {
    http_serv_conf *conf = &(serv->conf);
    int idx = lua_gettop(L);

    lua_getfield(L, conf_idx, "ip4");
    lua_getfield(L, conf_idx, "port");

    conf->ip4 = luaL_checkstring(L, idx + 1);
    conf->port = luaL_checkinteger(L, idx + 2);

    lua_settop(L, idx);
}

static void check_conf(lua_State *L, ud_http_serv *serv) {
    (void)L;
    (void)serv;
}

int http_serv_join(lua_State *L) {
    luaF_need_args(L, 1, "join");

    lua_State *T = luaF_new_thread_or_error(L);

    lua_pushcfunction(T, http_serv_join_start);
    lua_insert(L, 1);
    lua_xmove(L, T, 1);
    lua_resume(T, L, 1, &(int){0});

    return 1;
}

static int http_serv_join_start(lua_State *L) {
    ud_http_serv *serv = luaL_checkudata(L, 1, MT_HTTP_SERV);

    if (serv->fd == -1) {
        return 0;
    }

    lua_pushthread(L);
    lua_setiuservalue(L, 1, SERV_UV_IDX_JOIN_THREAD);

    return lua_yieldk(L, 0, 0, http_serv_join_continue);
}

static int http_serv_join_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)L;
    (void)status;
    (void)ctx;
    return 0;
}
