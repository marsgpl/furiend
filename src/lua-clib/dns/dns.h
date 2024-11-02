#ifndef LUA_LIB_DNS_H
#define LUA_LIB_DNS_H

#include <sys/types.h>
#include <furiend/shared.h>

#define MT_DNS_CLIENT "dns.client*"

// rfc1035#section-3.2.2
#define DNS_TYPE_A 1 // ipv4 address
#define DNS_TYPE_MX 15 // mail exchange
#define DNS_TYPE_TXT 16 // text strings

#define DNS_CLASS_IN 1 // internet
#define DNS_CLASS_CS 2 // csnet - OBSOLETE
#define DNS_CLASS_CH 3 // chaos
#define DNS_CLASS_HS 4 // hesiod (dyer 87)

#define DNS_DEFAULT_PORT 53
#define DNS_DEFAULT_TYPE DNS_TYPE_A
#define DNS_DEFAULT_CLASS DNS_CLASS_IN
#define DNS_DEFAULT_TIMEOUT 1.0 // 0 - unlimited

#define NAME_MAX_LEN 255 // label.label
#define LABEL_MIN_LEN 1
#define LABEL_MAX_LEN 63
#define REQ_MAX_LEN 512

#define DNS_UV_IDX_ID_SUBS 1
#define DNS_UV_IDX_SEND_QUEUE 2
#define DNS_UV_IDX_ROUTER 3

#define parse_uint16(buf) ntohs(((buf)[0]) | (((buf)[1]) << 8))
#define parse_uint32(buf) ntohl(*(uint32_t *)(buf))

#define check_len(len, need_min) { \
    if ((int)(len) < (int)(need_min)) { \
        luaL_error(L, "response is incomplete"); \
    } \
}

typedef struct {
    int fd;
    int can_write;
    int can_send;
    int parallel;
    lua_Number tmt;
    int queued_n;
    uint16_t next_id;
    char buf[REQ_MAX_LEN];
    int buf_len;
} ud_dns_client;

typedef struct {
    unsigned char rd :1; // is recursion desired
    unsigned char tc :1; // is truncated
    unsigned char aa :1; // is answer authoritative
    unsigned char opcode :4; // kind: 0=standard, 1=inverse, 2=server status
    unsigned char qr :1; // 0=query, 1=response
    // 0=ok, 1=fmt err, 2=serv fail, 3=name err, 4=not impl, 5=refused
    unsigned char rcode :4; // response code
    unsigned char z :3; // must be zero
    unsigned char ra :1; // is recursion available
} __attribute__((packed)) dns_header_flags_t;

typedef struct {
    uint16_t id; // query id
    uint16_t flags;
    uint16_t questions_n;
    uint16_t answers_n;
    uint16_t authority_n;
    uint16_t additional_n;
} __attribute__((packed)) dns_header_t;

typedef struct {
    uint16_t type;
    uint16_t class;
} __attribute__((packed)) dns_question_t;

LUAMOD_API int luaopen_dns(lua_State *L);

int dns_resolve(lua_State *L);

static int dns_resolve_start(lua_State *L);
static int dns_resolve_continue(lua_State *L, int status, lua_KContext ctx);

static int dns_client(lua_State *L);
static int dns_client_gc(lua_State *L);

static int dns_router(lua_State *L);
static int dns_router_continue(lua_State *L, int status, lua_KContext ctx);
static int dns_router_process_event(lua_State *L);
static void dns_router_read(lua_State *L, ud_dns_client *client);

static void process_send_queue(lua_State *L, ud_dns_client *client);
static void unsub_req_id(lua_State *L, int client_idx, int req_id);
static void unqueue_req_id(lua_State *L, int client_idx, int req_id);
static void queue_push(lua_State *L, ud_dns_client *client);
static void shift_send_queue(
    lua_State *L,
    ud_dns_client *client,
    int queue_idx,
    int sent_n);

static void header_error(lua_State *L, dns_header_t *header, const char *msg);
static int parse_header(lua_State *L, char **buf, int *len);
static void parse_name(lua_State *L, char **buf, int *len);
static void parse_push_answer(lua_State *L, char **buf, int *len, int index);

static const char *dns_opcode_label(int opcode);
static const char *dns_rcode_label(int opcode);

static int dns_pack(
    lua_State *L,
    ud_dns_client *client,
    const char *name,
    int type,
    int class);

static int dns_unpack(lua_State *L, ud_dns_client *client);

static const luaL_Reg dns_client_index[] = {
    { "resolve", dns_resolve },
    { NULL, NULL }
};

#endif

// request:

// \x00\x00\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x0Acloudflare\x03com\x00\x00\x01\x00\x01

// \x00\x02\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x03api\x08telegram\x03org\x00\x00\x01\x00\x01

// responses:

// \x00\x00\x80\x01\x00\x00\x00\x00\x00\x00\x00\x00

// \x00\x00\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00\x06google\x03com\x00\x00\x01\x00\x01\xC0\x0C\x00\x01\x00\x01\x00\x00\x01\x14\x00\x04\x8E\xFA\xC3\x0E

// \x00\x01\x80\x80\x00\x01\x00\x01\x00\x00\x00\x00\x03api\x08telegram\x03org\x00\x00\x01\x00\x01\xC0\x0C\x00\x01\x00\x01\x00\x00\x00D\x00\x04\x95\x9A\xA7\xDC

// \x00\x00\x81\x80\x00\x01\x00\x06\x00\x00\x00\x00\x06google\x03com\x00\x00\x01\x00\x01\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x96\x00\x04\xD1U\xE9d\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x96\x00\x04\xD1U\xE9e\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x96\x00\x04\xD1U\xE9f\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x96\x00\x04\xD1U\xE9\x8A\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x96\x00\x04\xD1U\xE9q\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x96\x00\x04\xD1U\xE9\x8B
