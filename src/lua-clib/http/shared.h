#ifndef LUA_LIB_HTTP_SHARED_H
#define LUA_LIB_HTTP_SHARED_H

#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <furiend/shared.h>

#define HTTP_DEFAULT_TIMEOUT 30.0
#define HTTP_DEFAULT_PORT 80
#define HTTPS_DEFAULT_PORT 443
#define HTTPS_VERIFY_CERT_DEFAULT 1
#define HTTP_DEFAULT_PATH "/"
#define HTTP_DEFAULT_METHOD "GET"
#define HTTP_DEFAULT_USER_AGENT "" // empty string: do not send it
#define HTTP_DEFAULT_CONTENT_TYPE ""

#define SEP "\r\n"

#define HTTP_VERSION "HTTP/1.1"
#define HTTP_HDR_HOST "Host"
#define HTTP_HDR_USER_AGENT "User-Agent"
#define HTTP_HDR_CONTENT_LEN "Content-Length"
#define HTTP_HDR_CONTENT_TYPE "Content-Type"
#define HTTP_HDR_TRANSFER_ENC "Transfer-Encoding"
#define HTTP_HDR_CONN "Connection"
#define HTTP_HDR_CONN_CLOSE_SEP HTTP_HDR_CONN ": close" SEP
#define HTTP_CHUNKED "chunked"

#define HTTP_RESPONSE_INITIAL_SIZE 8192
#define HTTP_RESPONSE_MAX_SIZE 1024 * 1024 * 2 // 2 Mb
#define HTTP_READ_MAX 2048
#define HTTP_EXPECT_RESPONSE_HEADERS_N 16

#define HTTP_HOST_MAX_LEN 255
#define HTTP_QUERY_HEADERS_MAX_LEN 8192 // 8Kb
#define HTTP_QUERY_BODY_MAX_LEN 1024 * 1024 * 16 // 16Mb
#define HTTP_CHUNK_MAX_LEN 1024 * 1024 * 8 // 8Mb

typedef struct {
    char *line;
    int rest_len;
    int is_chunked;
    int content_len;

    int ltrim;
    int rtrim;
} headers_parser_state;

typedef struct {
    char *method;
    int method_len;
    char *path;
    int path_len;
    char *ver;
    int ver_len;
} req_headline;

typedef struct {
    char *ver;
    int ver_len;
    int code;
    char *msg;
    int msg_len;
} res_headline;

void parse_req_headline(headers_parser_state *state, req_headline *hline);
void parse_res_headline(headers_parser_state *state, res_headline *hline);
void parse_headers(lua_State *L, headers_parser_state *state);

int ssl_error(lua_State *L, const char *fn_name);
int ssl_error_ret(lua_State *L, const char *fn_name, SSL *ssl, int ret);
int ssl_error_verify(lua_State *L, long result);
int ssl_warn_err_stack(lua_State *L);

#endif
