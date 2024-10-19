#ifndef LUA_LIB_HTTP_SERVER_H
#define LUA_LIB_HTTP_SERVER_H

#define _GNU_SOURCE

#include "shared.h"

#define MT_HTTP_SERV "http.server*"
#define MT_HTTP_SERV_CLIENT "http.server.client*"
#define MT_HTTP_SERV_REQ "http.server.request*"
#define MT_HTTP_SERV_RES "http.server.response*"

#define HTTP_SERV_DEFAULT_BACKLOG 32
#define HTTP_SERV_EXPECT_MIN_CLIENTS 4
#define HTTP_SERV_MAX_CLIENT_HEADERS_N 32

#define SERV_UV_IDX_CONFIG 1
#define SERV_UV_IDX_CLIENTS 2
#define SERV_UV_IDX_ON_REQUEST 3
#define SERV_UV_IDX_ON_ERROR 4

#define HTTP_SERV_IDX 1

#define CLIENT_SERV_IDX 1
#define CLIENT_FD_IDX 2
#define CLIENT_REQ_IDX 3
#define CLIENT_RES_IDX 4
#define CLIENT_CLIENT_IDX 5

#define CLIENT_UV_IDX_CLIENTS 1

#define CLIENT_PROC_CLIENT_IDX 1
#define CLIENT_PROC_REQ_IDX 2

#define CLIENT_FALLBACK_HEADERS \
    HTTP_VERSION " 500 Internal Server Error" SEP \
    HTTP_HDR_CONTENT_LEN ": 0" SEP \
    HTTP_HDR_CONN_CLOSE_SEP \
    SEP

typedef struct {
    const char *ip4;
    int port;
} http_serv_conf;

typedef struct {
    http_serv_conf conf;
    int fd;
} ud_http_serv;

typedef struct {
    int fd;
    int body_ready;
    int is_fallback_res;

    char *req;
    int req_len;
    int req_full_len;
    int req_size;
    char *req_sep; // pos of \r\n\r\n

    char *res_headers;
    int res_headers_len;
    int res_headers_len_sent;

    const char *res_body;
    int res_body_len;
    int res_body_len_sent;
} ud_http_serv_client;

int http_serv(lua_State *L);
int http_serv_gc(lua_State *L);
int http_serv_listen(lua_State *L);
int http_serv_on_request(lua_State *L);
int http_serv_on_error(lua_State *L);
int http_serv_client_gc(lua_State *L);
int http_serv_res_set_status(lua_State *L);
int http_serv_res_push_header(lua_State *L);
int http_serv_res_set_body(lua_State *L);

#endif
