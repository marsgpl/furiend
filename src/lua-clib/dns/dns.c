#include "dns.h"

LUAMOD_API int luaopen_dns(lua_State *L) {
    if (luaL_newmetatable(L, MT_DNS_CLIENT)) {
        luaL_newlib(L, dns_client_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, dns_client_gc);
        lua_setfield(L, -2, "__gc");
    }

    lua_createtable(L, 0, 3);

    lua_pushcfunction(L, dns_client);
    lua_setfield(L, -2, "client");

    lua_createtable(L, 0, 3);
        luaF_set_kv_int(L, -1, "A", DNS_TYPE_A);
        luaF_set_kv_int(L, -1, "MX", DNS_TYPE_MX);
        luaF_set_kv_int(L, -1, "TXT", DNS_TYPE_TXT);
    lua_setfield(L, -2, "type");

    lua_createtable(L, 0, 4);
        luaF_set_kv_int(L, -1, "IN", DNS_CLASS_IN);
        luaF_set_kv_int(L, -1, "CS", DNS_CLASS_CS);
        luaF_set_kv_int(L, -1, "CH", DNS_CLASS_CH);
        luaF_set_kv_int(L, -1, "HS", DNS_CLASS_HS);
    lua_setfield(L, -2, "class");

    return 1;
}

int dns_resolve(lua_State *L) {
    luaF_need_args(L, 2, "dns.client.resolve");

    lua_State *T = luaF_new_thread_or_error(L);

    lua_insert(L, 1); // client, conf, T -> T, client, conf
    lua_pushcfunction(T, dns_resolve_start);
    lua_xmove(L, T, 2); // client, conf -> T

    int nres;
    lua_resume(T, L, 2, &nres); // should yield, 0 nres

    return 1; // T
}

static int dns_resolve_start(lua_State *L) {
    ud_dns_client *client = lua_touserdata(L, 1);

    luaL_checktype(L, 2, LUA_TTABLE); // params
    lua_getfield(L, 2, "name");
    lua_getfield(L, 2, "type");
    lua_getfield(L, 2, "class");

    const char *name = luaL_checkstring(L, 3);
    int type = luaL_optinteger(L, 4, DNS_DEFAULT_TYPE);
    int class = luaL_optinteger(L, 5, DNS_DEFAULT_CLASS);

    if (client->fd == -1) {
        luaL_error(L, "not bound to socket");
    }

    lua_getiuservalue(L, 1, DNS_UDUVIDX_ID_SUBS);

    int req_id = dns_pack(L, client, name, type, class);

    if (lua_rawgeti(L, -1, req_id) != LUA_TNIL) {
        luaL_error(L, "dns request id already exists: %d", req_id);
    }

    int tmt_fd = luaF_set_timeout(L, client->tmt);

    if (
        (!client->can_write) // socket is not ready or send buf is full
        || (!client->parallel && !client->can_send) // w8 for prev response
    ) {
        queue_push(L, client);
    } else {
        ssize_t sent = sendto(client->fd, client->buf, client->buf_len,
            MSG_NOSIGNAL, NULL, 0);

        if (sent != (ssize_t)client->buf_len) { // fail
            if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                queue_push(L, client);
                client->can_write = 0; // socket send buf is full
            } else {
                luaF_close_or_warning(L, tmt_fd);
                luaF_error_errno(L, "sendto failed; fd: %d; len: %d",
                    client->fd, client->buf_len);
            }
        } else if (!client->parallel) {
            client->can_send = 0; // need to wait for prev response
        }
    }

    lua_pushthread(L); // ..., id_subs, nil, L
    lua_rawseti(L, -3, req_id); // id_subs[req_id] = L

    lua_settop(L, 1); // client

    lua_pushinteger(L, tmt_fd);
    lua_pushinteger(L, req_id);

    return lua_yieldk(L, 0, 0, dns_resolve_continue); // longjmp
}

// client, tmt_fd, req_id, is_ok, req_id / err_msg
// client, tmt_fd, req_id, tmt_fd, emask
static int dns_resolve_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;
    (void)status;

    luaF_close_or_warning(L, lua_tointeger(L, 2));

    if (lua_type(L, 4) != LUA_TBOOLEAN) { // tmt
        int req_id = lua_tointeger(L, 3);

        unsub_req_id(L, 1, req_id);
        unqueue_req_id(L, 1, req_id);

        if (lua_type(L, -1) == LUA_TNUMBER) {
            lua_pushstring(L, "timeout");
        } // else it is loop.gc error

        return lua_error(L);
    }

    if (!lua_toboolean(L, 4)) { // loop fail
        return lua_error(L);
    }

    return dns_unpack(L, lua_touserdata(L, 1));
}

static int dns_client(lua_State *L) {
    luaF_need_args(L, 1, "dns.client");
    luaL_checktype(L, 1, LUA_TTABLE); // config

    lua_getfield(L, 1, "ip4");
    lua_getfield(L, 1, "port");
    lua_getfield(L, 1, "timeout");
    lua_getfield(L, 1, "parallel");

    const char *ip4 = luaL_checkstring(L, 2);
    int port = luaL_optinteger(L, 3, DNS_DEFAULT_PORT);
    lua_Number tmt = luaL_optnumber(L, 4, DNS_DEFAULT_TIMEOUT);
    int parallel = lua_toboolean(L, 5);

    int status, nres;

    ud_dns_client *client = lua_newuserdatauv(L, sizeof(ud_dns_client), 2);

    if (!client) {
        luaF_error_errno(L, F_ERROR_NEW_UD, MT_DNS_CLIENT, 2);
    }

    lua_createtable(L, 0, 1);
    lua_setiuservalue(L, -2, DNS_UDUVIDX_ID_SUBS);

    lua_createtable(L, 1, 0);
    lua_setiuservalue(L, -2, DNS_UDUVIDX_SEND_QUEUE);

    struct sockaddr_in sa = {0};

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    status = inet_pton(AF_INET, ip4, &sa.sin_addr);

    if (status == 0) { // invalid format
        luaL_error(L, "inet_pton: invalid address format; af: %d; address: %s",
            AF_INET, ip4);
    } else if (status != 1) { // other errors
        luaF_error_errno(L, "inet_pton failed; af: %d; address: %s",
            AF_INET, ip4);
    }

    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);

    if (fd == -1) {
        luaF_error_errno(L, "socket failed (udp nonblock)");
    }

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        // should bind immediately
        luaF_close_or_warning(L, fd);
        luaF_error_errno(L, "binding failed; addr: %s:%d", ip4, port);
    }

    client->fd = fd;
    client->can_write = 0;
    client->can_send = 1;
    client->parallel = parallel;
    client->tmt = tmt;
    client->queued_n = 0;
    client->next_id = 0;

    luaL_setmetatable(L, MT_DNS_CLIENT);

    lua_insert(L, 1); // ? <- client
    lua_settop(L, 1); // client

    lua_State *router = luaF_new_thread_or_error(L);

    lua_pushcfunction(router, dns_router);
    lua_pushvalue(L, 1); // clone client
    lua_xmove(L, router, 1); // client -> router

    status = lua_resume(router, L, 1, &nres);

    if (status != LUA_YIELD) {
        luaL_error(L, "router init failed: %s", lua_tostring(router, -1));
    }

    lua_settop(L, 1);

    return 1; // client
}

static int dns_client_gc(lua_State *L) {
    ud_dns_client *client = luaL_checkudata(L, 1, MT_DNS_CLIENT);

    if (client->fd == -1) {
        return 0;
    }

    luaF_close_or_warning(L, client->fd);
    client->fd = -1;

    lua_settop(L, 2); // client, error msg or nil
    lua_getiuservalue(L, 1, DNS_UDUVIDX_ID_SUBS);
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);

    int id_subs_idx = 3;
    int t_subs_idx = 4;

    lua_pushnil(L);
    while (lua_next(L, id_subs_idx)) {
        lua_State *sub = lua_tothread(L, -1);

        lua_pushboolean(sub, 0);
        lua_pushstring(sub, lua_isstring(L, 2)
            ? lua_tostring(L, 2) // gc called from router
            : "dns.client.gc called");

        int nres;
        int status = lua_resume(sub, L, 2, &nres); // shouldn't yield

        luaF_loop_notify_t_subs(L, sub, t_subs_idx, status, nres);

        lua_pop(L, 1); // lua_next
    }

    return 0;
}

static int dns_router(lua_State *L) {
    ud_dns_client *client = lua_touserdata(L, 1);

    int emask = EPOLLIN | EPOLLOUT | EPOLLET;
    int status = luaF_loop_pwatch(L, client->fd, emask, 0);

    if (status != LUA_OK) {
        lua_error(L);
    }

    return lua_yieldk(L, 0, 0, dns_router_continue); // longjmp
}

// called when epoll emits event on client->fd
static int dns_router_continue(lua_State *L, int status, lua_KContext ctx) {
    (void)ctx;

    lua_pushcfunction(L, dns_router_process_event);
    lua_pushvalue(L, 1); // client
    lua_pushvalue(L, 3); // emask or errmsg
    status = lua_pcall(L, 2, 0, 0);

    if (status != LUA_OK) {
        lua_insert(L, 2); // client, ? <- err msg
        lua_settop(L, 2); // client, err msg
        return dns_client_gc(L);
    }

    lua_settop(L, 1); // client

    return lua_yieldk(L, 0, 0, dns_router_continue); // keep waiting
}

static int dns_router_process_event(lua_State *L) {
    luaF_loop_check_close(L);

    ud_dns_client *client = lua_touserdata(L, 1);
    int emask = lua_tointeger(L, 2);

    if (client->fd == -1) {
        return luaL_error(L, "dns.client is closed");
    }

    if (emask_has_errors(emask)) {
        luaF_error_socket(L, client->fd, emask_error_label(emask));
    }

    if (emask & EPOLLOUT) {
        client->can_write = 1;
        process_send_queue(L, client);
    }

    if (emask & EPOLLIN) {
        dns_router_read(L, client);

        if (!client->parallel) {
            client->can_send = 1;
            process_send_queue(L, client);
        }
    }

    return 0;
}

static void dns_router_read(lua_State *L, ud_dns_client *client) {
    lua_getiuservalue(L, 1, DNS_UDUVIDX_ID_SUBS);
    lua_rawgeti(L, LUA_REGISTRYINDEX, F_RIDX_LOOP_T_SUBS);

    int id_subs_idx = lua_gettop(L) - 1;
    int t_subs_idx = lua_gettop(L);

    while (client->fd != -1) {
        ssize_t len = recv(client->fd, client->buf, REQ_MAX_LEN, 0);

        if (len < (int)sizeof(uint16_t)) { // error or too small
            if (len > -1) {
                luaF_warning(L, "response is too small"
                    "; fd: %d; len: %d; min len: %d",
                    client->fd, len, sizeof(uint16_t));
                continue;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                luaF_error_errno(L, "recv failed; fd: %d; cap: %d",
                    client->fd, REQ_MAX_LEN);
            }
            return; // busy, try again later
        }

        uint16_t req_id = parse_uint16(client->buf);
        int type = lua_rawgeti(L, id_subs_idx, req_id);

        if (type == LUA_TTHREAD) {
            lua_pushnil(L);
            lua_rawseti(L, id_subs_idx, req_id);

            client->buf_len = len;

            lua_State *sub = lua_tothread(L, -1);

            lua_pushboolean(sub, 1);

            int nres;
            int status = lua_resume(sub, L, 1, &nres); // shouldn't yield

            luaF_loop_notify_t_subs(L, sub, t_subs_idx, status, nres);
        } else {
            luaF_warning(L, "dns sub not found; req id: %d", req_id);
        }

        lua_pop(L, 1); // lua_rawgeti
    }
}

static void process_send_queue(lua_State *L, ud_dns_client *client) {
    if (!client->can_write // socket is not ready or send buf is full
        || (!client->parallel && !client->can_send) // w8 for prev response
        || (client->queued_n == 0) // nothing to send
    ) {
        return;
    }

    lua_getiuservalue(L, 1, DNS_UDUVIDX_SEND_QUEUE);

    int queue_idx = lua_gettop(L);
    int sent_n = 0;

    for (int i = 1; i <= client->queued_n; ++i) {
        lua_rawgeti(L, queue_idx, i); // msg to send

        size_t len;
        const char *msg = lua_tolstring(L, -1, &len);
        ssize_t sent = sendto(client->fd, msg, len, MSG_NOSIGNAL, NULL, 0);

        lua_pop(L, 1); // lua_rawgeti

        if (sent != (ssize_t)len) { // error
            if (sent > -1) {
                luaL_error(L, "sendto sent less; fd: %d; len: %d; sent: %d",
                    client->fd, len, sent);
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                luaF_error_errno(L, "sendto failed; fd: %d; len: %d",
                    client->fd, len);
            }

            client->can_write = 0; // socket send buf is full
            break;
        }

        sent_n++;

        if (!client->parallel) {
            client->can_send = 0; // need to wait for prev response
            break;
        }
    }

    if (sent_n == client->queued_n) { // sent everything
        client->queued_n = 0;
    } else {
        shift_send_queue(L, client, queue_idx, sent_n);
    }
}

static void unsub_req_id(lua_State *L, int client_idx, int req_id) {
    lua_getiuservalue(L, client_idx, DNS_UDUVIDX_ID_SUBS);
    lua_pushnil(L);
    lua_rawseti(L, -2, req_id);
    lua_pop(L, 1); // lua_getiuservalue
}

static void unqueue_req_id(lua_State *L, int client_idx, int req_id) {
    lua_getiuservalue(L, client_idx, DNS_UDUVIDX_SEND_QUEUE);

    ud_dns_client *client = lua_touserdata(L, client_idx);
    int queue_idx = lua_gettop(L);
    int found = 0;

    for (int i = 1; i <= client->queued_n; ++i) {
        lua_rawgeti(L, queue_idx, i);

        if (found) {
            lua_rawseti(L, queue_idx, i - 1);
        } else {
            size_t len;
            const char *buf = lua_tolstring(L, -1, &len);
            int queued_req_id = parse_uint16(buf);

            if (queued_req_id == req_id) {
                found = 1;
            }
        }

        lua_pop(L, 1); // lua_rawgeti
    }

    if (found) {
        client->queued_n--;
    }

    lua_pop(L, 1); // lua_getiuservalue
}

static void queue_push(lua_State *L, ud_dns_client *client) {
    lua_getiuservalue(L, 1, DNS_UDUVIDX_SEND_QUEUE);
    lua_pushlstring(L, client->buf, client->buf_len);
    lua_rawseti(L, -2, ++(client->queued_n));
    lua_pop(L, 1); // lua_getiuservalue
}

static void shift_send_queue(
    lua_State *L,
    ud_dns_client *client,
    int queue_idx,
    int sent_n
) {
    int new_n = 0;

    for (int i = sent_n + 1; i <= client->queued_n; ++i) {
        lua_rawgeti(L, queue_idx, i);
        lua_rawseti(L, queue_idx, ++new_n);
    }

    client->queued_n = new_n;
}

static void header_error(lua_State *L, dns_header_t *header, const char *msg) {
    dns_header_flags_t *flags = (dns_header_flags_t *)&header->flags;

    luaL_error(L, "%s"
        "; id: %d"
        "; questions: %d"
        "; answers: %d"
        "; authority: %d"
        "; additional: %d"
        "; kind (opcode): %s (%d)"
        "; response code: %s (%d)"
        "; truncated: %s"
        "; authoritative: %s"
        "; response: %s"
        "; recursion available: %s"
        "; z: %d",
            msg,
            header->id,
            header->questions_n,
            header->answers_n,
            header->authority_n,
            header->additional_n,
            dns_opcode_label(flags->opcode), flags->opcode,
            dns_rcode_label(flags->rcode), flags->rcode,
            flags->tc ? "yes" : "no",
            flags->aa ? "yes" : "no",
            flags->qr ? "yes" : "no",
            flags->ra ? "yes" : "no",
            flags->z);
}

// \x00\x00\x80\x01\x00\x00\x00\x00\x00\x00\x00\x00
static int parse_header(lua_State *L, char **buf, int *len) {
    check_len(*len, sizeof(dns_header_t));

    dns_header_t header = {
        .id = ntohs(*(uint16_t *)(*buf)),
        .flags = ntohs(*(uint16_t *)(*buf + 2)),
        .questions_n = ntohs(*(uint16_t *)(*buf + 4)),
        .answers_n = ntohs(*(uint16_t *)(*buf + 6)),
        .authority_n = ntohs(*(uint16_t *)(*buf + 8)),
        .additional_n = ntohs(*(uint16_t *)(*buf + 10)),
    };

    dns_header_flags_t *flags = (dns_header_flags_t *)&header.flags;

    if (header.questions_n != 1) header_error(L, &header, "questions n != 1");
    if (header.answers_n < 1) header_error(L, &header, "answers n < 1");
    if (!flags->qr) header_error(L, &header, "not a response");
    if (flags->opcode != 0) header_error(L, &header, "unexpected opcode");
    if (flags->rcode != 0) header_error(L, &header, "unexpected response code");
    if (flags->z) header_error(L, &header, "z != 0");
    if (flags->tc) header_error(L, &header, "truncated");
    if (!flags->ra) header_error(L, &header, "answer is not recursive");

    *buf += sizeof(dns_header_t);
    *len -= sizeof(dns_header_t);

    return header.answers_n;
}

// "\x06google\x03com\x00" - name
// "\xC0\x0C" - pointer
// "\x06google\x03com\xC0\x0C" - name with pointer
static const char *parse_name(lua_State *L, char **buf, int *len) {
    check_len(*len, 2);

    if (**buf == 0) {
        luaL_error(L, "zero length name");
    } else if (**buf > LABEL_MAX_LEN) {
        *buf += 2;
        *len -= 2;
        return "*";
    }

    const char *name = *buf + 1;

    *buf += 1 + **buf;
    *len -= 1 + **buf;

    while (*len > 0 && **buf) {
        int label_len = **buf;

        if (**buf > LABEL_MAX_LEN) {
            **buf = '*';
            (*buf)++;
            (*len)--;
            check_len(*len, 1);
            **buf = '\0';
            break;
        }

        **buf = '.';
        *buf += 1 + label_len;
        *len -= 1 + label_len;
    }

    (*buf)++;
    (*len)--;

    check_len(*len, 0);

    return name;
}

static void parse_push_answer(lua_State *L, char **buf, int *len, int index) {
    parse_name(L, buf, len);
    check_len(*len, 10); // TYPE (2), CLASS (2), TTL (4), RDLENGTH (2)

    int type = parse_uint16(*buf);
    int data_len = parse_uint16(*buf + 8);

    *buf += 10;
    *len -= 10;

    check_len(*len, data_len);

    switch (type) {
        case DNS_TYPE_A:
            if (data_len != sizeof(struct in_addr)) {
                luaL_error(L, "A: data len != in_addr size");
            }

            lua_pushstring(L, inet_ntoa(*(struct in_addr *)*buf));
            lua_rawseti(L, -2, index);

            *buf += data_len;
            *len -= data_len;
            return;
        default:
            luaL_error(L, "unsupported type: %d", type);
    }
}

static const char *dns_opcode_label(int opcode) {
    switch (opcode) {
        case 0: return "standard";
        case 1: return "inverse";
        case 2: return "server status";
        default: return "unknown";
    }
}

static const char *dns_rcode_label(int opcode) {
    switch (opcode) {
        case 0: return "ok";
        case 1: return "invalid format";
        case 2: return "server error";
        case 3: return "invalid name";
        case 4: return "not implemented";
        case 5: return "refused";
        default: return "unknown";
    }
}

// put req to client->buf, put req len to client->buf_len, return req_id
static int dns_pack(
    lua_State *L,
    ud_dns_client *client,
    const char *name,
    int type,
    int class
) {
    int name_len = strlen(name);

    if (!name_len) {
        luaL_error(L, "name is empty");
    } else if (name_len > NAME_MAX_LEN) {
        luaL_error(L, "name is too long; length: %d; max len: %d",
            name_len, NAME_MAX_LEN);
    }

    int req_len = sizeof(dns_header_t) + name_len + 2 + sizeof(dns_question_t);

    if (req_len > REQ_MAX_LEN) {
        luaL_error(L, "request is too big; size: %d; max len: %d",
            req_len, REQ_MAX_LEN);
    }

    int req_id = client->next_id++;
    char *buf = client->buf;

    client->buf_len = req_len;

    dns_header_t header = {0};
    dns_header_flags_t flags = {0};
    char *cf = (char *)&flags;

    flags.rd = 1; // desire recursive search

    header.id = htons(req_id);
    header.flags = htons(cf[0] | cf[1] << 8);
    header.questions_n = htons(1);

    dns_question_t question;

    question.type = htons(type);
    question.class = htons(class);

    memcpy(buf, &header, sizeof(header));
    buf += sizeof(dns_header_t) + 1; // extra space for first label len

    memcpy(buf, name, name_len);
    buf += name_len;
    *buf++ = 0; // domain name null terminator

    memcpy(buf, &question, sizeof(question));

    int label_len = 0;
    buf = client->buf + sizeof(dns_header_t);

    for (int i = 0; i < name_len; ++i) {
        if (name[i] == '.') {
            if (label_len == 0) {
                luaL_error(L, "label is empty");
            } else if (label_len > LABEL_MAX_LEN) {
                luaL_error(L, "label is too long; length: %d; max len: %d",
                    label_len, LABEL_MAX_LEN);
            }

            *buf = label_len;
            buf += i + 1;
            label_len = 0;
        } else {
            label_len++;
        }
    }

    *buf = label_len;

    // fprintf(stderr, "request: ");
    // lua_pushlstring(L, client->buf, client->buf_len);
    // luaF_print_value(L, stderr, -1);
    // fprintf(stderr, "\n");
    // lua_pop(L, 1);

    return req_id;
}

static int dns_unpack(lua_State *L, ud_dns_client *client) {
    char *buf = client->buf;
    int len = client->buf_len;

    // fprintf(stderr, "response: ");
    // lua_pushlstring(L, client->buf, client->buf_len);
    // luaF_print_value(L, stderr, -1);
    // fprintf(stderr, "\n");
    // lua_pop(L, 1);

    int answers_n = parse_header(L, &buf, &len);
    parse_name(L, &buf, &len);

    check_len(len, sizeof(dns_question_t));
    buf += sizeof(dns_question_t);
    len -= sizeof(dns_question_t);

    lua_createtable(L, answers_n, 0);

    for (int index = 1; index <= answers_n; ++index) {
        parse_push_answer(L, &buf, &len, index);
    }

    return 1;
}
