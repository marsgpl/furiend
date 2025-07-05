#include "request.h"

static int request_start(lua_State *L);
static int request_continue(lua_State *L, int status, lua_KContext ctx);
static void request_connecting(lua_State *L, ud_http_request *req);
static void request_connecting_tls(lua_State *L, ud_http_request *req);
static void request_sending_plain(lua_State *L, ud_http_request *req);
static void request_sending_tls(lua_State *L, ud_http_request *req);
static void request_reading_plain(lua_State *L, ud_http_request *req);
static void request_reading_tls(lua_State *L, ud_http_request *req);
static void request_shutdown_tls(lua_State *L, ud_http_request *req);
static void request_on_send_complete(lua_State *L, ud_http_request *req);
static int request_finish(lua_State *L, ud_http_request *req);
static void resize_response_buf(lua_State *L, ud_http_request *req);
static void parse_conf(lua_State *L, ud_http_request *req, int conf_idx);
static void check_conf(lua_State *L, ud_http_request *req);
static void build_headers(lua_State *L, ud_http_request *req);
static void move_chunked(ud_http_request *req, headers_parser_state *state);

int http_request(lua_State *L) {
    luaF_need_args(L, 1, "http request");
    luaL_checktype(L, 1, LUA_TTABLE); // conf

    lua_State *T = luaF_new_thread_or_error(L);

    lua_insert(L, 1); // conf, T -> T, conf
    lua_pushcfunction(T, request_start);
    lua_xmove(L, T, 1); // conf >> T

    lua_resume(T, L, 1, &(int){0}); // should yield, 0 nres

    return 1; // T
}

int http_request_gc(lua_State *L) {
    ud_http_request *req = luaL_checkudata(L, 1, MT_HTTP_REQUEST);

    if (req->ssl) {
        SSL_free(req->ssl);
        req->ssl = NULL;
    }

    if (req->ssl_ctx) {
        SSL_CTX_free(req->ssl_ctx);
        req->ssl_ctx = NULL;
    }

    if (req->fd != -1) {
        luaF_close_or_warning(L, req->fd);
        req->fd = -1;
    }

    if (req->headers) {
        free(req->headers);
        req->headers = NULL;
    }

    if (req->response) {
        free(req->response);
        req->response = NULL;
    }

    return 0;
}

static int request_start(lua_State *L) {
    ud_http_request *req = luaF_new_ud_or_error(L,
        sizeof(ud_http_request), 1);

    memset(req, 0, sizeof(ud_http_request));

    parse_conf(L, req, 1);
    check_conf(L, req);

    http_request_conf *conf = &(req->conf);
    const char *ip4 = conf->ip4;
    int port = conf->port;

    struct sockaddr_in sa = {0};
    luaF_set_ip4_port(L, &sa, ip4, port);

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (unlikely(fd < 0)) {
        luaF_error_errno(L, "socket failed (tcp nonblock)");
    }

    req->fd = fd;
    req->state = HTTP_REQ_STATE_CONNECTING;

    luaL_setmetatable(L, MT_HTTP_REQUEST);

    build_headers(L, req);

    int status = connect(fd, (struct sockaddr *)&sa, sizeof(sa));

    if (unlikely(status != -1)) {
        luaL_error(L, "connected immediately; addr: %s:%d", ip4, port);
    } else if (errno != EINPROGRESS) {
        luaF_error_errno(L, "connect failed; addr: %s:%d", ip4, port);
    }

    luaF_loop_watch(L, fd, EPOLLIN | EPOLLOUT | EPOLLET, 0);

    lua_insert(L, 1); // config, req -> req, config
    lua_setiuservalue(L, 1, HTTP_REQUEST_UV_IDX_CONFIG); // config >> req

    return lua_yieldk(L, 0, 0, request_continue);
}

// req, sock_fd/tmt_fd, emask/errmsg
static int request_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;
    (void)status;

    luaF_loop_check_close(L);

    ud_http_request *req = lua_touserdata(L, 1);
    int fd = lua_tointeger(L, F_LOOP_FD_REL_IDX);
    int emask = lua_tointeger(L, F_LOOP_EMASK_REL_IDX);

    if (unlikely(emask_has_errors(emask))) {
        luaF_error_socket(L, fd, emask_error_label(emask));
    }

    if (emask & EPOLLOUT) {
        switch (req->state) {
            case HTTP_REQ_STATE_CONNECTING:
                request_connecting(L, req);
                break;
            case HTTP_REQ_STATE_CONNECTING_TLS:
                request_connecting_tls(L, req);
                break;
            case HTTP_REQ_STATE_SENDING_PLAIN:
                request_sending_plain(L, req);
                break;
            case HTTP_REQ_STATE_SENDING_TLS:
                request_sending_tls(L, req);
                break;
        }
    }

    if (emask & EPOLLIN) {
        switch (req->state) {
            case HTTP_REQ_STATE_CONNECTING_TLS:
                request_connecting_tls(L, req);
                break;
            case HTTP_REQ_STATE_READING_PLAIN:
                request_reading_plain(L, req);
                break;
            case HTTP_REQ_STATE_READING_TLS:
                request_reading_tls(L, req);
                break;
        }
    }

    if (req->state == HTTP_REQ_STATE_DONE) {
        return request_finish(L, req);
    }

    lua_settop(L, 1); // req

    return lua_yieldk(L, 0, 0, request_continue);
}

static void request_connecting(lua_State *L, ud_http_request *req) {
    int code = get_socket_error_code(req->fd);

    if (unlikely(code != 0)) {
        luaF_push_error_socket(L, req->fd, "connection failed", code);
        lua_error(L);
    }

    if (unlikely(!req->conf.https)) {
        req->state = HTTP_REQ_STATE_SENDING_PLAIN;
        request_sending_plain(L, req);
        return;
    }

    req->ssl_ctx = SSL_CTX_new(TLS_client_method());

    if (unlikely(req->ssl_ctx == NULL)) {
        ssl_error(L, "SSL_CTX_new");
    }

    if (!SSL_CTX_set_min_proto_version(req->ssl_ctx, TLS1_3_VERSION)) {
        ssl_error(L, "SSL_CTX_set_min_proto_version");
    }

    req->ssl = SSL_new(req->ssl_ctx);

    if (unlikely(req->ssl == NULL)) {
        ssl_error(L, "SSL_new");
    }

    if (unlikely(!SSL_set_fd(req->ssl, req->fd))) {
        ssl_error(L, "SSL_set_fd");
    }

    req->state = HTTP_REQ_STATE_CONNECTING_TLS;
    request_connecting_tls(L, req);
}

static void request_connecting_tls(lua_State *L, ud_http_request *req) {
    int ret = SSL_connect(req->ssl);

    if (unlikely(ret == 0)) {
        ssl_error_ret(L, "SSL_connect", req->ssl, ret);
    } else if (ret < 0) {
        int code = SSL_get_error(req->ssl, ret);

        if (code != SSL_ERROR_WANT_READ && code != SSL_ERROR_WANT_WRITE) {
            ssl_error_ret(L, "SSL_connect", req->ssl, ret);
        }
    } else { // ok
        if (likely(req->conf.https_verify_cert)) {
            long result = SSL_get_verify_result(req->ssl);

            if (unlikely(result != X509_V_OK)) {
                ssl_error_verify(L, result);
            }
        }

        req->ssl_cipher = SSL_get_cipher(req->ssl);

        req->state = HTTP_REQ_STATE_SENDING_TLS;
        request_sending_tls(L, req);
    }
}

static void request_sending_plain(lua_State *L, ud_http_request *req) {
    while (req->headers_len_sent < req->headers_len) {
        ssize_t sent = send(req->fd,
            req->headers + req->headers_len_sent,
            req->headers_len - req->headers_len_sent,
            MSG_NOSIGNAL);

        if (unlikely(sent == 0)) {
            luaL_error(L, "headers: send sent 0");
        } else if (sent < 0) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_error_errno(L, "headers: send failed");
            }
            return; // try again later
        }

        req->headers_len_sent += sent;
    }

    while (req->body_len_sent < req->body_len) {
        ssize_t sent = send(req->fd,
            req->body + req->body_len_sent,
            req->body_len - req->body_len_sent,
            MSG_NOSIGNAL);

        if (unlikely(sent == 0)) {
            luaL_error(L, "body: send sent 0");
        } else if (sent < 0) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_error_errno(L, "body: send failed");
            }
            return; // try again later
        }

        req->body_len_sent += sent;
    }

    request_on_send_complete(L, req);

    req->state = HTTP_REQ_STATE_READING_PLAIN;
    request_reading_plain(L, req);
}

static void request_sending_tls(lua_State *L, ud_http_request *req) {
    while (req->headers_len_sent < req->headers_len) {
        int sent = SSL_write(req->ssl,
            req->headers + req->headers_len_sent,
            req->headers_len - req->headers_len_sent);

        if (unlikely(sent == 0)) {
            ssl_error_ret(L, "SSL_write headers 0", req->ssl, sent);
        } else if (sent < 0) {
            int code = SSL_get_error(req->ssl, sent);

            if (code != SSL_ERROR_WANT_READ && code != SSL_ERROR_WANT_WRITE) {
                ssl_error_ret(L, "SSL_write headers", req->ssl, sent);
            }

            return; // try again later
        }

        req->headers_len_sent += sent;
    }

    while (req->body_len_sent < req->body_len) {
        int sent = SSL_write(req->ssl,
            req->body + req->body_len_sent,
            req->body_len - req->body_len_sent);

        if (unlikely(sent == 0)) {
            ssl_error_ret(L, "SSL_write body 0", req->ssl, sent);
        } else if (sent < 0) {
            int code = SSL_get_error(req->ssl, sent);

            if (code != SSL_ERROR_WANT_READ && code != SSL_ERROR_WANT_WRITE) {
                ssl_error_ret(L, "SSL_write body", req->ssl, sent);
            }

            return; // try again later
        }

        req->body_len_sent += sent;
    }

    request_on_send_complete(L, req);

    req->state = HTTP_REQ_STATE_READING_TLS;
    request_reading_tls(L, req);
}

static void request_reading_plain(lua_State *L, ud_http_request *req) {
    while (1) {
        resize_response_buf(L, req);

        ssize_t read = recv(req->fd,
            req->response + req->response_len,
            HTTP_READ_MAX,
            0);

        if (read == 0) {
            req->state = HTTP_REQ_STATE_DONE;
            return;
        } else if (read < 0) {
            if (unlikely(errno != EAGAIN && errno != EWOULDBLOCK)) {
                luaF_error_errno(L, "recv failed; fd: %d", req->fd);
            }
            return; // try again later
        }

        req->response_len += read;

        if (0) {
            req->state = HTTP_REQ_STATE_DONE;
            return;
        }
    }
}

static void request_reading_tls(lua_State *L, ud_http_request *req) {
    while (1) {
        resize_response_buf(L, req);

        int read = SSL_read(req->ssl,
            req->response + req->response_len,
            HTTP_READ_MAX);

        if (read == 0) {
            request_shutdown_tls(L, req);
            req->state = HTTP_REQ_STATE_DONE;
            return;
        } else if (read < 0) {
            int code = SSL_get_error(req->ssl, read);

            if (code != SSL_ERROR_WANT_READ && code != SSL_ERROR_WANT_WRITE) {
                ssl_error_ret(L, "SSL_read", req->ssl, read);
            }

            return;
        }

        req->response_len += read;

        if (0) {
            request_shutdown_tls(L, req);
            req->state = HTTP_REQ_STATE_DONE;
            return;
        }
    }
}

static void request_shutdown_tls(lua_State *L, ud_http_request *req) {
    (void)L;

    SSL_shutdown(req->ssl);

    // -1: error
    //  0: initiated and in progress
    //  1: successfully shut down

    // not interested in result since lots of servers disrespect standards
}

static void request_on_send_complete(lua_State *L, ud_http_request *req) {
    if (!req->conf.show_request) {
        free(req->headers);
        req->headers = NULL;
    }

    req->response_size = HTTP_RESPONSE_INITIAL_SIZE - 1;
    req->response = luaF_malloc_or_error(L, HTTP_RESPONSE_INITIAL_SIZE);
}

static int request_finish(lua_State *L, ud_http_request *req) {
    int show_request = req->conf.show_request;

    req->response[req->response_len] = '\0'; // extra byte was reserved

    headers_parser_state state;
    res_headline hline = {0};

    state.line = req->response;
    state.rest_len = req->response_len;
    state.is_chunked = 0;
    state.content_len = 0;
    state.ltrim = 0;
    state.rtrim = 0;

    parse_res_headline(&state, &hline);

    // version, status_code, status_message, headers, body
    // request?, request_body?
    lua_createtable(L, 0, 5 + show_request + show_request);

    lua_pushlstring(L, hline.ver, hline.ver_len);
    lua_setfield(L, -2, "version");

    lua_pushinteger(L, hline.code);
    lua_setfield(L, -2, "status_code");

    lua_pushlstring(L, hline.msg, hline.msg_len);
    lua_setfield(L, -2, "status_message");

    lua_createtable(L, 0, HTTP_EXPECT_RESPONSE_HEADERS_N);

    if (state.line > req->response) { // headline ok
        parse_headers(L, &state);

        if (state.is_chunked) {
            // HEX_LEN\r\nCONTENT\r\n0\r\n\r\n
            // HEX_LEN\r\nCONTENT\r\nHEX_LEN\r\nCONTENT\r\n0\r\n\r\n
            move_chunked(req, &state);
        }
    }

    lua_setfield(L, -2, "headers");

    const char *body = state.line + state.ltrim;
    size_t body_len = state.rest_len - state.ltrim - state.rtrim;

    size_t left_pad = body - req->response;
    size_t right_pad = state.rtrim;
    size_t total = left_pad + body_len + right_pad;

    if (total != req->response_len) {
        luaL_error(L, "chunk len doesn't fit in response len"
            "; left pad (headers): %d; chunk len: %d; right pad: %d"
            "; total: %d; response len: %d",
            left_pad, body_len, right_pad, total, req->response_len);
    }

    lua_pushlstring(L, body, body_len);
    lua_setfield(L, -2, "body");

    if (show_request) {
        lua_pushlstring(L, req->headers, req->headers_len);
        lua_setfield(L, -2, "request");
        lua_pushlstring(L, req->body, req->body_len);
        lua_setfield(L, -2, "request_body");
    }

    // disconnect + free resources
    http_request_gc(L);

    return 1;
}

static void resize_response_buf(lua_State *L, ud_http_request *req) {
    if (req->response_size - req->response_len < HTTP_READ_MAX) {
        // to avoid lots of allocs, start parsing response in advance
        // and try to find response size (content length or chunk length)
        int size = req->response_size + 1 + HTTP_RESPONSE_INITIAL_SIZE;

        if (unlikely(size > HTTP_RESPONSE_MAX_SIZE)) {
            luaL_error(L, "response is too big; max: %d",
                HTTP_RESPONSE_MAX_SIZE);
        }

        char *buf = realloc(req->response, size);

        if (unlikely(buf == NULL)) {
            luaF_error_errno(L, "realloc failed; from: %d; to: %d",
                req->response_size, size);
        }

        req->response = buf;
        req->response_size = size - 1;
    }
}

static void parse_conf(lua_State *L, ud_http_request *req, int conf_idx) {
    http_request_conf *conf = &(req->conf);
    int idx = lua_gettop(L);

    lua_getfield(L, conf_idx, "https");
    lua_getfield(L, conf_idx, "https_verify_cert");
    lua_getfield(L, conf_idx, "method");
    lua_getfield(L, conf_idx, "host");
    lua_getfield(L, conf_idx, "ip4");
    lua_getfield(L, conf_idx, "path");
    lua_getfield(L, conf_idx, "timeout");
    lua_getfield(L, conf_idx, "port");
    lua_getfield(L, conf_idx, "user_agent");
    lua_getfield(L, conf_idx, "show_request");
    lua_getfield(L, conf_idx, "body");
    lua_getfield(L, conf_idx, "content_type");

    const char *method = luaL_optstring(L, idx + 3, HTTP_DEFAULT_METHOD);
    unsigned char method0c = method[0];

    conf->show_request = lua_toboolean(L, idx + 10);
    conf->https = lua_toboolean(L, idx + 1);
    conf->https_verify_cert = lua_isnil(L, idx + 2)
        ? HTTPS_VERIFY_CERT_DEFAULT
        : lua_toboolean(L, idx + 2);
    conf->can_have_body = (method0c == 'P' || method0c == 'p') && (
        strcasecmp(method, "POST") == 0 ||
        strcasecmp(method, "PUT") == 0 ||
        strcasecmp(method, "PATCH") == 0);
    req->body = luaL_optlstring(L, idx + 11, "", &(req->body_len));

    conf->method = method;
    conf->host = luaL_optstring(L, idx + 4, "");
    conf->ip4 = luaL_checkstring(L, idx + 5);
    conf->path = luaL_optstring(L, idx + 6, HTTP_DEFAULT_PATH);
    conf->timeout = luaL_optnumber(L, idx + 7, HTTP_DEFAULT_TIMEOUT);
    conf->user_agent = luaL_optstring(L, idx + 9, HTTP_DEFAULT_USER_AGENT);
    conf->content_type = luaL_optstring(L, idx + 12,
        HTTP_DEFAULT_CONTENT_TYPE);
    conf->port = luaL_optinteger(L, idx + 8, conf->https
        ? HTTPS_DEFAULT_PORT
        : HTTP_DEFAULT_PORT);

    lua_settop(L, idx);
}

static void check_conf(lua_State *L, ud_http_request *req) {
    http_request_conf *conf = &(req->conf);
    size_t len;

    len = strlen(conf->host);
    if (len > HTTP_HOST_MAX_LEN) {
        luaL_error(L, "invalid len for host: %s; len: %d; max: %d",
            conf->host, len, HTTP_HOST_MAX_LEN);
    }

    if (!conf->can_have_body && req->body_len > 0) {
        luaL_error(L, "specified http method does not support body"
            "; method: %s; body len: %d", conf->method, req->body_len);
    }

    if (req->body_len > HTTP_QUERY_BODY_MAX_LEN) {
        luaL_error(L, "body is too big; len: %d; max: %d",
            req->body_len,
            HTTP_QUERY_BODY_MAX_LEN);
    }
}

static void build_headers(lua_State *L, ud_http_request *req) {
    http_request_conf *conf = &(req->conf);

    const char *method = conf->method;
    const char *host = conf->host;
    const char *user_agent = conf->user_agent;
    const char *content_type = conf->content_type;
    int can_have_body = conf->can_have_body;

    size_t len = strlen(method)
        + strlen(" ") + strlen(conf->path)
        + strlen(" ") + strlen(HTTP_VERSION)
        + strlen(SEP)
        + strlen(HTTP_HDR_CONN_CLOSE_SEP)
        + strlen(SEP)
        + 1; // + nul for snprintf

    if (host[0]) {
        len += strlen(HTTP_HDR_HOST ": ")
            + strlen(host)
            + strlen(SEP);
    }

    if (user_agent[0]) {
        len += strlen(HTTP_HDR_USER_AGENT ": ")
            + strlen(user_agent)
            + strlen(SEP);
    }

    if (content_type[0]) {
        len += strlen(HTTP_HDR_CONTENT_TYPE ": ")
            + strlen(content_type)
            + strlen(SEP);
    }

    if (can_have_body) {
        len += strlen(HTTP_HDR_CONTENT_LEN ": ")
            + uint_len(req->body_len)
            + strlen(SEP);
    }

    if (len > HTTP_QUERY_HEADERS_MAX_LEN) {
        luaL_error(L, "request headers are too big; len: %d; max: %d",
            len, HTTP_QUERY_HEADERS_MAX_LEN);
    }

    req->headers = luaF_malloc_or_error(L, len);
    req->headers_len = len - 1; // exclude nul

    char *headers = req->headers;
    int written;

    #define PUSH(line, ...) { \
        written = snprintf(headers, len, line SEP __VA_OPT__(,) __VA_ARGS__); \
        if (unlikely(written < 0)) { \
            luaF_error_errno(L, "snprintf failed; written: %d of %d", \
                written, len); \
        } \
        headers += written; \
        len -= written; \
    }

    PUSH("%s %s %s", method, conf->path, HTTP_VERSION);
    if (host[0]) PUSH(HTTP_HDR_HOST ": %s", host);
    if (user_agent[0]) PUSH(HTTP_HDR_USER_AGENT ": %s", user_agent);
    if (content_type[0]) PUSH(HTTP_HDR_CONTENT_TYPE ": %s", content_type);
    if (can_have_body) PUSH(HTTP_HDR_CONTENT_LEN ": %lu", req->body_len);
    PUSH(HTTP_HDR_CONN_CLOSE_SEP);

    #undef PUSH

    if (len != 1) {
        luaL_error(L, "request headers len calculated incorrectly: %d", len);
    }
}

static void move_chunked(ud_http_request *req, headers_parser_state *state) {
    char *pos = state->line;
    char *end = memchr(state->line, '\r', state->rest_len);

    if (!end) {
        return;
    }

    state->ltrim = end - pos + 2;

    int chunk_len = parse_hex(&pos);

    if (chunk_len <= 0 || chunk_len > HTTP_CHUNK_MAX_LEN) {
        return;
    }

    state->rtrim = req->response_len
        - (state->line - req->response)
        - chunk_len
        - state->ltrim;
}
