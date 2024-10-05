#ifndef LUA_LIB_HTTP_H
#define LUA_LIB_HTTP_H

#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <furiend/lib.h>

#define MT_HTTP_REQUEST "http.request*"

#define HTTP_DEFAULT_TIMEOUT 30.0
#define HTTP_DEFAULT_PORT 80
#define HTTPS_DEFAULT_PORT 443
#define HTTPS_VERIFY_CERT_DEFAULT 1
#define HTTP_DEFAULT_PATH "/"
#define HTTP_DEFAULT_METHOD "GET"
#define HTTP_DEFAULT_USER_AGENT "" // empty string means do not send it

#define HTTP_REQ_STATE_CONNECTING 0
#define HTTP_REQ_STATE_CONNECTING_TLS 1
#define HTTP_REQ_STATE_SENDING_PLAIN 2
#define HTTP_REQ_STATE_SENDING_TLS 3
#define HTTP_REQ_STATE_READING_PLAIN 4
#define HTTP_REQ_STATE_READING_TLS 5
#define HTTP_REQ_STATE_DONE 6

#define HTTP_VERSION "HTTP/1.1"
#define SEP "\r\n"
#define HTTP_HEADER_CONNECTION_CLOSE "Connection: close"

#define HTTP_RESPONSE_INITIAL_SIZE 8192
#define HTTP_RESPONSE_MAX_SIZE 1024 * 1024 * 2 // 2 Mb
#define HTTP_READ_MAX 2048
#define HTTP_EXPECT_RESPONSE_HEADERS_N 16

#define HTTP_HOST_MAX_LEN 255
#define HTTP_PATH_MAX_LEN 2047
#define HTTP_USER_AGENT_MAX_LEN 2047
#define HTTP_QUERY_HEADERS_MAX_LEN 4096
#define HTTP_CHUNK_MAX_LEN 1024 * 1024

typedef struct {
    char *line;
    int rest_len;
    int is_chunked;
    int ltrim;
    int rtrim;
} http_headers_state;

typedef struct {
    char *ver;
    int ver_len;
    int code;
    char *msg;
    int msg_len;
} http_headline;

typedef struct {
    const char *method;
    const char *host;
    const char *ip4;
    const char *path;
    const char *user_agent;
    lua_Number timeout;
    int port;
} http_params;

typedef struct {
    int https;
    int https_verify_cert;
    int show_request;
    int fd;
    int state;
    char *headers;
    int headers_len;
    int headers_len_sent;
    char *response;
    int response_size;
    int response_len;
    SSL_CTX *ssl_ctx;
    SSL *ssl;
    const char *ssl_cipher;
} ud_http_request;

static const int hex_to_dec[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,8,9,
    0,0,0,0,0,0,0,
    10,11,12,13,14,15,
    0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,
    10,11,12,13,14,15,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,
};

LUAMOD_API int luaopen_http(lua_State *L);

int http_request(lua_State *L);
static int http_request_gc(lua_State *L);
static int http_request_start(lua_State *L);
static int http_request_continue(lua_State *L, int status, lua_KContext ctx);
static void http_request_connecting(lua_State *L, ud_http_request *req);
static void http_request_connecting_tls(lua_State *L, ud_http_request *req);
static void http_request_sending_plain(lua_State *L, ud_http_request *req);
static void http_request_sending_tls(lua_State *L, ud_http_request *req);
static void http_request_reading_plain(lua_State *L, ud_http_request *req);
static void http_request_reading_tls(lua_State *L, ud_http_request *req);
static void http_request_shutdown_tls(lua_State *L, ud_http_request *req);
static void http_request_on_send_complete(lua_State *L, ud_http_request *req);
static int http_request_finish(lua_State *L, ud_http_request *req);
static void http_check_params(lua_State *L, http_params *params);
static int ssl_warn_err_stack(lua_State *L);
static int ssl_error(lua_State *L, const char *fn_name);
static int ssl_error_ret(lua_State *L, const char *fn_name, SSL *ssl, int ret);
static int ssl_error_verify(lua_State *L, long result);
static inline void realloc_response_buf(lua_State *L, ud_http_request *req);
static void parse_headline(http_headers_state *state, http_headline *headline);
static void parse_headers(lua_State *L, http_headers_state *state);
static void rechunk(ud_http_request *req, http_headers_state *state);

static void http_parse_params(
    lua_State *L,
    http_params *params,
    ud_http_request *req,
    int params_idx);

static void http_build_headers(
    lua_State *L,
    ud_http_request *req,
    http_params *params);

static const luaL_Reg http_index[] = {
    { "request", http_request },
    { NULL, NULL }
};

#endif
