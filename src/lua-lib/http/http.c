#include "http.h"

LUAMOD_API int luaopen_http(lua_State *L) {
    if (luaL_newmetatable(L, MT_HTTP_REQUEST)) {
        lua_pushcfunction(L, http_request_gc);
        lua_setfield(L, -2, "__gc");
    }

    luaL_newlib(L, http_index);

    return 1;
}

int http_request(lua_State *L) {
    luaF_need_args(L, 1, "http.request");
    luaL_checktype(L, 1, LUA_TTABLE); // params

    lua_State *T = luaF_new_thread_or_error(L);
    lua_insert(L, 1); // params, T -> T, params
    lua_pushcfunction(T, http_request_start);
    lua_xmove(L, T, 1); // params -> T

    int nres;
    lua_resume(T, L, 1, &nres); // should yield, 0 nres

    return 1; // T
}

static int http_request_gc(lua_State *L) {
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

static int http_request_start(lua_State *L) {
    ud_http_request *req = lua_newuserdatauv(L, sizeof(ud_http_request), 0);

    if (req == NULL) {
        luaF_error_errno(L, F_ERROR_NEW_UD, MT_HTTP_REQUEST, 0);
    }

    memset(req, 0, sizeof(ud_http_request));

    http_params params;
    http_parse_params(L, &params, req, 1);
    http_check_params(L, &params);

    int status;

    struct sockaddr_in sa = {0};

    sa.sin_family = AF_INET;
    sa.sin_port = htons(params.port);

    status = inet_pton(AF_INET, params.ip4, &sa.sin_addr);

    if (status == 0) { // invalid format
        luaL_error(L, "inet_pton: invalid address format; af: %d; address: %s",
            AF_INET, params.ip4);
    } else if (status != 1) { // other errors
        luaF_error_errno(L, "inet_pton failed; af: %d; address: %s",
            AF_INET, params.ip4);
    }

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (fd == -1) {
        luaF_error_errno(L, "socket failed (tcp nonblock)");
    }

    req->fd = fd;
    req->state = HTTP_REQ_STATE_CONNECTING;

    luaL_setmetatable(L, MT_HTTP_REQUEST);

    http_build_headers(L, req, &params);

    status = connect(fd, (struct sockaddr *)&sa, sizeof(sa));

    if (status != -1) {
        luaF_error_errno(L, "connected immediately; addr: %s:%d",
            params.ip4, params.port);
    } else if (errno != EINPROGRESS) {
        luaF_error_errno(L, "connect failed; addr: %s:%d",
            params.ip4, params.port);
    }

    status = luaF_loop_pwatch(L, fd, EPOLLIN | EPOLLOUT | EPOLLET, 0);

    if (status != LUA_OK) {
        lua_error(L);
    }

    lua_insert(L, 1);
    lua_settop(L, 1); // params are dead after this point

    return lua_yieldk(L, 0, 0, http_request_continue); // longjmp
}

static void http_parse_params(
    lua_State *L,
    http_params *params,
    ud_http_request *req,
    int params_idx
) {
    int idx = lua_gettop(L);

    lua_getfield(L, params_idx, "https");
    lua_getfield(L, params_idx, "https_verify_cert");
    lua_getfield(L, params_idx, "method");
    lua_getfield(L, params_idx, "host");
    lua_getfield(L, params_idx, "ip4");
    lua_getfield(L, params_idx, "path");
    lua_getfield(L, params_idx, "timeout");
    lua_getfield(L, params_idx, "port");
    lua_getfield(L, params_idx, "user_agent");
    lua_getfield(L, params_idx, "show_request");

    req->show_request = lua_toboolean(L, idx + 10);
    req->https = lua_toboolean(L, idx + 1);
    req->https_verify_cert = lua_isnil(L, idx + 2)
        ? HTTPS_VERIFY_CERT_DEFAULT
        : lua_toboolean(L, idx + 2);

    params->method = luaL_optstring(L, idx + 3, HTTP_DEFAULT_METHOD);
    params->host = luaL_checkstring(L, idx + 4);
    params->ip4 = luaL_checkstring(L, idx + 5);
    params->path = luaL_optstring(L, idx + 6, HTTP_DEFAULT_PATH);
    params->timeout = luaL_optnumber(L, idx + 7, HTTP_DEFAULT_TIMEOUT);
    params->port = luaL_optinteger(L, idx + 8, req->https
        ? HTTPS_DEFAULT_PORT
        : HTTP_DEFAULT_PORT);
    params->user_agent = luaL_optstring(L, idx + 9, HTTP_DEFAULT_USER_AGENT);

    lua_settop(L, idx);
}

static void http_check_params(lua_State *L, http_params *params) {
    size_t len;

    len = strlen(params->host);
    if (!len || len > HTTP_HOST_MAX_LEN) {
        luaL_error(L, "invalid len for host: %s; len: %d; max: %d",
            params->host, len, HTTP_HOST_MAX_LEN);
    }

    len = strlen(params->path);
    if (!len || len > HTTP_PATH_MAX_LEN) {
        luaL_error(L, "invalid len for path: %s; len: %d; max: %d",
            params->path, len, HTTP_PATH_MAX_LEN);
    }

    len = strlen(params->user_agent);
    if (len > HTTP_USER_AGENT_MAX_LEN) {
        luaL_error(L, "invalid len for user agent: %s; len: %d; max: %d",
            params->user_agent, len, HTTP_USER_AGENT_MAX_LEN);
    }
}

static void http_build_headers(
    lua_State *L,
    ud_http_request *req,
    http_params *params
) {
    const char *ua = params->user_agent;

    int len = strlen(params->method)
        + strlen(" ") + strlen(params->path)
        + strlen(" ") + strlen(HTTP_VERSION)
        + strlen(SEP)
        + strlen("Host: ") + strlen(params->host)
        + strlen(SEP)
        + strlen(HTTP_HEADER_CONNECTION_CLOSE)
        + strlen(SEP)
        + strlen(SEP)
        + 1; // + nul for snprintf

    if (ua[0]) {
        len += strlen("User-Agent: ") + strlen(ua) + strlen(SEP);
    }

    if (len > HTTP_QUERY_HEADERS_MAX_LEN) {
        luaL_error(L, "request headers are too big; len: %d; max: %d",
            len, HTTP_QUERY_HEADERS_MAX_LEN);
    }

    req->headers = malloc(len);
    req->headers_len = len - 1; // exclude nul

    if (req->headers == NULL) {
        luaF_error_errno(L, "headers malloc failed; size: %d", len);
    }

    char *headers = req->headers;
    int r;

    #define PUSH(line, ...) { \
        r = snprintf(headers, len, line SEP __VA_OPT__(,) __VA_ARGS__); \
        if (r == -1) luaF_error_errno(L, "snprintf failed"); \
        headers += r; \
        len -= r; \
    }

    PUSH("%s %s %s", params->method, params->path, HTTP_VERSION);
    PUSH("Host: %s", params->host);
    PUSH(HTTP_HEADER_CONNECTION_CLOSE);
    if (ua[0]) PUSH("User-Agent: %s", ua);
    PUSH(""); // to produce \r\n\r\n

    #undef PUSH

    if (len != 1) {
        luaL_error(L, "request headers len calculated incorrectly");
    }
}

// req, sock_fd/tmt_fd, emask/errmsg
static int http_request_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;
    (void)status;

    luaF_loop_check_close(L);

    ud_http_request *req = lua_touserdata(L, 1);
    int fd = lua_tointeger(L, F_LOOP_FD_REL_IDX);
    int emask = lua_tointeger(L, F_LOOP_EMASK_REL_IDX);

    if (emask_has_errors(emask)) {
        luaF_error_socket(L, fd, emask_error_label(emask));
    }

    if (emask & EPOLLOUT) {
        switch (req->state) {
            case HTTP_REQ_STATE_CONNECTING:
                http_request_connecting(L, req);
                break;
            case HTTP_REQ_STATE_CONNECTING_TLS:
                http_request_connecting_tls(L, req);
                break;
            case HTTP_REQ_STATE_SENDING_PLAIN:
                http_request_sending_plain(L, req);
                break;
            case HTTP_REQ_STATE_SENDING_TLS:
                http_request_sending_tls(L, req);
                break;
        }
    }

    if (emask & EPOLLIN) {
        switch (req->state) {
            case HTTP_REQ_STATE_CONNECTING_TLS:
                http_request_connecting_tls(L, req);
                break;
            case HTTP_REQ_STATE_READING_PLAIN:
                http_request_reading_plain(L, req);
                break;
            case HTTP_REQ_STATE_READING_TLS:
                http_request_reading_tls(L, req);
                break;
        }
    }

    if (req->state == HTTP_REQ_STATE_DONE) {
        return http_request_finish(L, req);
    }

    lua_settop(L, 1);

    return lua_yieldk(L, 0, 0, http_request_continue); // longjmp
}

static int http_request_finish(lua_State *L, ud_http_request *req) {
    req->response[req->response_len] = '\0'; // extra byte was reserved

    char *ver = NULL;
    int ver_len = 0;
    int code = 0;
    char *msg = NULL;
    int msg_len = 0;

    char *pos;
    char *line = req->response;
    int line_len;
    int rest_len = req->response_len;

    // HTTP/x.x CODE MESSAGE\r\n

    pos = memchr(line, '\r', rest_len);

    if (pos != NULL && *(pos + 1) == '\n') {
        line_len = pos - line + 2; // +2 for \r\n
        pos = memchr(line, ' ', line_len);

        if (pos != NULL) {
            // HTTP/1.1
            ver = line;
            ver_len = pos - line;

            // 200
            pos++;
            while (*pos >= '0' && *pos <= '9') {
                code = (code * 10) + (*pos - '0');
                pos++;
            }

            // OK
            msg = pos + 1; // skip 1 non-digit
            msg_len = line_len - 2 - 1 - (pos - line);
            if (msg_len < 0) msg_len = 0;
        }

        line += line_len;
        rest_len -= line_len;
    }

    // version, code, message, headers, body, request?
    lua_createtable(L, 0, 5 + req->show_request);

    lua_pushlstring(L, ver, ver_len);
    lua_setfield(L, -2, "version");

    lua_pushinteger(L, code);
    lua_setfield(L, -2, "code");

    lua_pushlstring(L, msg, msg_len);
    lua_setfield(L, -2, "message");

    lua_createtable(L, 0, HTTP_EXPECT_RESPONSE_HEADERS_N);
    lua_setfield(L, -2, "headers");

    lua_pushlstring(L, req->response + req->response_len - rest_len, rest_len);
    lua_setfield(L, -2, "body");

    if (req->show_request) {
        lua_pushlstring(L, req->headers, req->headers_len);
        lua_setfield(L, -2, "request");
    }

    // disconnect
    // free resources in advance
    http_request_gc(L);

    return 1;
}

static void http_request_sending_plain(lua_State *L, ud_http_request *req) {
    luaL_error(L, "TODO: http_request_sending_plain; fd: %d", req->fd);
    // after send complete: see tls
}

static void http_request_sending_tls(lua_State *L, ud_http_request *req) {
    while (req->headers_len_sent < req->headers_len) {
        int sent = SSL_write(req->ssl,
            req->headers + req->headers_len_sent,
            req->headers_len - req->headers_len_sent);

        if (sent == 0) {
            ssl_error_ret(L, "SSL_write", req->ssl, sent);
        } else if (sent < 0) {
            int code = SSL_get_error(req->ssl, sent);

            if (code != SSL_ERROR_WANT_READ && code != SSL_ERROR_WANT_WRITE) {
                ssl_error_ret(L, "SSL_write", req->ssl, sent);
            }

            return;
        }

        req->headers_len_sent += sent;
    }

    if (!req->show_request) {
        free(req->headers);
        req->headers = NULL;
    }

    req->response_size = HTTP_RESPONSE_INITIAL_SIZE;
    req->response = malloc(req->response_size);

    if (req->response == NULL) {
        luaF_error_errno(L, "response malloc failed; size: %d",
            req->response_size);
    }

    req->state = HTTP_REQ_STATE_READING_TLS;
    http_request_reading_tls(L, req);
}

static void http_request_reading_plain(lua_State *L, ud_http_request *req) {
    luaL_error(L, "TODO: http_request_reading_plain; fd: %d", req->fd);
    // after read complete: see tls
}

static void http_request_reading_tls(lua_State *L, ud_http_request *req) {
    while (1) {
        // -1 reserves extra byte for '\0'
        if (req->response_size - req->response_len < HTTP_READ_MAX - 1) {
            // to avoid lots of allocs, start parsing response in advance
            // and try to find response size (content length or chunk length)
            int size = req->response_size * 2;

            if (size > HTTP_RESPONSE_MAX_SIZE) {
                luaL_error(L, "response is too big; max: %d",
                    HTTP_RESPONSE_MAX_SIZE);
            }

            char *p = realloc(req->response, size);

            if (p == NULL) {
                luaF_error_errno(L, "response realloc error; size: %d", size);
            }

            req->response = p;
            req->response_size = size;
        }

        int read = SSL_read(req->ssl,
            req->response + req->response_len,
            HTTP_READ_MAX);

        if (read == 0) {
            http_request_shutdown_tls(L, req);
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

        if (0) { // all data received
            http_request_shutdown_tls(L, req);
            req->state = HTTP_REQ_STATE_DONE;
            return;
        }
    }
}

static void http_request_shutdown_tls(lua_State *L, ud_http_request *req) {
    (void)L;

    SSL_shutdown(req->ssl);

    // -1: error
    //  0: initiated and in progress
    //  1: successfully shut down

    // not interested in result since lots of servers disrespect standards
}

static void http_request_connecting_tls(lua_State *L, ud_http_request *req) {
    int ret = SSL_connect(req->ssl);

    if (ret == 0) {
        ssl_error_ret(L, "SSL_connect", req->ssl, ret);
    } else if (ret < 0) {
        int code = SSL_get_error(req->ssl, ret);

        if (code != SSL_ERROR_WANT_READ && code != SSL_ERROR_WANT_WRITE) {
            ssl_error_ret(L, "SSL_connect", req->ssl, ret);
        }
    } else { // ok
        if (req->https_verify_cert) {
            long result = SSL_get_verify_result(req->ssl);

            if (result != X509_V_OK) {
                ssl_error_verify(L, result);
            }
        }

        req->ssl_cipher = SSL_get_cipher(req->ssl);

        req->state = HTTP_REQ_STATE_SENDING_TLS;
        http_request_sending_tls(L, req);
    }
}

static void http_request_connecting(lua_State *L, ud_http_request *req) {
    int code = get_socket_error_code(req->fd);

    if (code != 0) {
        luaF_push_error_socket(L, req->fd, "connection failed", code);
        lua_error(L);
    }

    if (!req->https) {
        req->state = HTTP_REQ_STATE_SENDING_PLAIN;
        http_request_sending_plain(L, req);
        return;
    }

    req->ssl_ctx = SSL_CTX_new(TLS_client_method());

    if (req->ssl_ctx == NULL) {
        ssl_error(L, "SSL_CTX_new");
    }

    if (!SSL_CTX_set_min_proto_version(req->ssl_ctx, TLS1_3_VERSION)) {
        ssl_error(L, "SSL_CTX_set_min_proto_version");
    }

    req->ssl = SSL_new(req->ssl_ctx);

    if (req->ssl == NULL) {
        ssl_error(L, "SSL_new");
    }

    if (!SSL_set_fd(req->ssl, req->fd)) {
        ssl_error(L, "SSL_set_fd");
    }

    req->state = HTTP_REQ_STATE_CONNECTING_TLS;
    http_request_connecting_tls(L, req);
}

static int ssl_error_verify(lua_State *L, long result) {
    const char *result_label = "unknown result";

    #define CASE(value) { case value: result_label = #value; break; }
    switch (result) {
        CASE(X509_V_OK);
        CASE(X509_V_ERR_UNSPECIFIED);
        CASE(X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT);
        CASE(X509_V_ERR_UNABLE_TO_GET_CRL);
        CASE(X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE);
        CASE(X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE);
        CASE(X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY);
        CASE(X509_V_ERR_CERT_SIGNATURE_FAILURE);
        CASE(X509_V_ERR_CRL_SIGNATURE_FAILURE);
        CASE(X509_V_ERR_CERT_NOT_YET_VALID);
        CASE(X509_V_ERR_CERT_HAS_EXPIRED);
        CASE(X509_V_ERR_CRL_NOT_YET_VALID);
        CASE(X509_V_ERR_CRL_HAS_EXPIRED);
        CASE(X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD);
        CASE(X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD);
        CASE(X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD);
        CASE(X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD);
        CASE(X509_V_ERR_OUT_OF_MEM);
        CASE(X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT);
        CASE(X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN);
        CASE(X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY);
        CASE(X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE);
        CASE(X509_V_ERR_CERT_CHAIN_TOO_LONG);
        CASE(X509_V_ERR_CERT_REVOKED);
        CASE(X509_V_ERR_NO_ISSUER_PUBLIC_KEY);
        CASE(X509_V_ERR_PATH_LENGTH_EXCEEDED);
        CASE(X509_V_ERR_INVALID_PURPOSE);
        CASE(X509_V_ERR_CERT_UNTRUSTED);
        CASE(X509_V_ERR_CERT_REJECTED);
        CASE(X509_V_ERR_SUBJECT_ISSUER_MISMATCH);
        CASE(X509_V_ERR_AKID_SKID_MISMATCH);
        CASE(X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH);
        CASE(X509_V_ERR_KEYUSAGE_NO_CERTSIGN);
        CASE(X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER);
        CASE(X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION);
        CASE(X509_V_ERR_KEYUSAGE_NO_CRL_SIGN);
        CASE(X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION);
        CASE(X509_V_ERR_INVALID_NON_CA);
        CASE(X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED);
        CASE(X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE);
        CASE(X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED);
        CASE(X509_V_ERR_INVALID_EXTENSION);
        CASE(X509_V_ERR_INVALID_POLICY_EXTENSION);
        CASE(X509_V_ERR_NO_EXPLICIT_POLICY);
        CASE(X509_V_ERR_DIFFERENT_CRL_SCOPE);
        CASE(X509_V_ERR_UNSUPPORTED_EXTENSION_FEATURE);
        CASE(X509_V_ERR_UNNESTED_RESOURCE);
        CASE(X509_V_ERR_PERMITTED_VIOLATION);
        CASE(X509_V_ERR_EXCLUDED_VIOLATION);
        CASE(X509_V_ERR_SUBTREE_MINMAX);
        CASE(X509_V_ERR_APPLICATION_VERIFICATION);
        CASE(X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE);
        CASE(X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX);
        CASE(X509_V_ERR_UNSUPPORTED_NAME_SYNTAX);
        CASE(X509_V_ERR_CRL_PATH_VALIDATION_ERROR);
        CASE(X509_V_ERR_PATH_LOOP);
        CASE(X509_V_ERR_SUITE_B_INVALID_VERSION);
        CASE(X509_V_ERR_SUITE_B_INVALID_ALGORITHM);
        CASE(X509_V_ERR_SUITE_B_INVALID_CURVE);
        CASE(X509_V_ERR_SUITE_B_INVALID_SIGNATURE_ALGORITHM);
        CASE(X509_V_ERR_SUITE_B_LOS_NOT_ALLOWED);
        CASE(X509_V_ERR_SUITE_B_CANNOT_SIGN_P_384_WITH_P_256);
        CASE(X509_V_ERR_HOSTNAME_MISMATCH);
        CASE(X509_V_ERR_EMAIL_MISMATCH);
        CASE(X509_V_ERR_IP_ADDRESS_MISMATCH);
        CASE(X509_V_ERR_DANE_NO_MATCH);
        CASE(X509_V_ERR_EE_KEY_TOO_SMALL);
        CASE(X509_V_ERR_CA_KEY_TOO_SMALL);
        CASE(X509_V_ERR_CA_MD_TOO_WEAK);
        CASE(X509_V_ERR_INVALID_CALL);
        CASE(X509_V_ERR_STORE_LOOKUP);
        CASE(X509_V_ERR_NO_VALID_SCTS);
        CASE(X509_V_ERR_PROXY_SUBJECT_NAME_VIOLATION);
        CASE(X509_V_ERR_OCSP_VERIFY_NEEDED);
        CASE(X509_V_ERR_OCSP_VERIFY_FAILED);
        CASE(X509_V_ERR_OCSP_CERT_UNKNOWN);
        CASE(X509_V_ERR_UNSUPPORTED_SIGNATURE_ALGORITHM);
        CASE(X509_V_ERR_SIGNATURE_ALGORITHM_MISMATCH);
        CASE(X509_V_ERR_SIGNATURE_ALGORITHM_INCONSISTENCY);
        CASE(X509_V_ERR_INVALID_CA);
        CASE(X509_V_ERR_PATHLEN_INVALID_FOR_NON_CA);
        CASE(X509_V_ERR_PATHLEN_WITHOUT_KU_KEY_CERT_SIGN);
        CASE(X509_V_ERR_KU_KEY_CERT_SIGN_INVALID_FOR_NON_CA);
        CASE(X509_V_ERR_ISSUER_NAME_EMPTY);
        CASE(X509_V_ERR_SUBJECT_NAME_EMPTY);
        CASE(X509_V_ERR_MISSING_AUTHORITY_KEY_IDENTIFIER);
        CASE(X509_V_ERR_MISSING_SUBJECT_KEY_IDENTIFIER);
        CASE(X509_V_ERR_EMPTY_SUBJECT_ALT_NAME);
        CASE(X509_V_ERR_EMPTY_SUBJECT_SAN_NOT_CRITICAL);
        CASE(X509_V_ERR_CA_BCONS_NOT_CRITICAL);
        CASE(X509_V_ERR_AUTHORITY_KEY_IDENTIFIER_CRITICAL);
        CASE(X509_V_ERR_SUBJECT_KEY_IDENTIFIER_CRITICAL);
        CASE(X509_V_ERR_CA_CERT_MISSING_KEY_USAGE);
        CASE(X509_V_ERR_EXTENSIONS_REQUIRE_VERSION_3);
        CASE(X509_V_ERR_EC_KEY_EXPLICIT_PARAMS);
        CASE(X509_V_ERR_RPK_UNTRUSTED);
    }
    #undef CASE

    return luaL_error(L, "SSL_get_verify_result failed; code: %s (%d)",
        result_label, result);
}

static int ssl_error_ret(lua_State *L, const char *fn_name, SSL *ssl, int ret) {
    int errors_in_stack = ssl_warn_err_stack(L);
    int err_code = SSL_get_error(ssl, ret);
    const char *err_label = "unknown error code";

    #define CASE(value) { case value: err_label = #value; break; }
    switch (err_code) {
        CASE(SSL_ERROR_NONE);
        CASE(SSL_ERROR_SSL);
        CASE(SSL_ERROR_WANT_READ);
        CASE(SSL_ERROR_WANT_WRITE);
        CASE(SSL_ERROR_WANT_X509_LOOKUP);
        CASE(SSL_ERROR_SYSCALL);
        CASE(SSL_ERROR_ZERO_RETURN);
        CASE(SSL_ERROR_WANT_CONNECT);
        CASE(SSL_ERROR_WANT_ACCEPT);
        CASE(SSL_ERROR_WANT_ASYNC);
        CASE(SSL_ERROR_WANT_ASYNC_JOB);
        CASE(SSL_ERROR_WANT_CLIENT_HELLO_CB);
        CASE(SSL_ERROR_WANT_RETRY_VERIFY);
    }
    #undef CASE

    return luaL_error(L, "%s failed; code: %s (%d); errors: %d",
        fn_name, err_label, err_code, errors_in_stack);
}

static int ssl_error(lua_State *L, const char *fn_name) {
    int errors_in_stack = ssl_warn_err_stack(L);
    return luaL_error(L, "%s failed; errors: %d", fn_name, errors_in_stack);
}

static int ssl_warn_err_stack(lua_State *L) {
    int errors_in_stack = 0;
    unsigned long error_code;
    static char error_msg[256];

    while ((error_code = ERR_get_error()) != 0) {
        errors_in_stack++;

        ERR_error_string_n(error_code, error_msg, sizeof(error_msg));

        const char *lib = ERR_lib_error_string(error_code);
        const char *reason = ERR_reason_error_string(error_code);

        luaF_warning(L, "ssl stack error #%d"
            "; code: %d; msg: %s; lib: %s; reason: %s",
            errors_in_stack, error_code, error_msg, lib, reason);
    }

    return errors_in_stack;
}
