#ifndef LUA_LIB_HTTP_REQUEST_H
#define LUA_LIB_HTTP_REQUEST_H

#include "shared.h"

#define MT_HTTP_REQUEST "http.request*"

#define HTTP_REQUEST_UDUVIDX_CONFIG 1

#define HTTP_REQ_STATE_CONNECTING 0
#define HTTP_REQ_STATE_CONNECTING_TLS 1
#define HTTP_REQ_STATE_SENDING_PLAIN 2
#define HTTP_REQ_STATE_SENDING_TLS 3
#define HTTP_REQ_STATE_READING_PLAIN 4
#define HTTP_REQ_STATE_READING_TLS 5
#define HTTP_REQ_STATE_DONE 6

typedef struct {
    int https;
    int https_verify_cert;
    int show_request;
    int can_have_body;
    int port;
    lua_Number timeout;
    const char *method;
    const char *host;
    const char *ip4;
    const char *path;
    const char *user_agent;
    const char *content_type;
} http_request_conf;

typedef struct {
    http_request_conf conf;
    int fd;
    int state;

    char *headers;
    size_t headers_len;
    size_t headers_len_sent;

    const char *body;
    size_t body_len;
    size_t body_len_sent;

    char *response;
    size_t response_len;
    size_t response_size;

    SSL_CTX *ssl_ctx;
    SSL *ssl;
    const char *ssl_cipher;
} ud_http_request;

int http_request(lua_State *L);
int http_request_gc(lua_State *L);

#endif
