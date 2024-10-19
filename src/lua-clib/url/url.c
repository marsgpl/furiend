#include "url.h"

LUAMOD_API int luaopen_url(lua_State *L) {
    lua_pushcfunction(L, parse_url);
    return 1;
}

static int parse_url(lua_State *L) {
    luaF_need_args(L, 1, "url");

    size_t url_len;
    const char *url = lua_tolstring(L, 1, &url_len);

    const char *pos;
    const char *pos2;
    size_t len;

    // protocol, username, password, host, port, path, params, hash
    lua_createtable(L, 0, 8);

    // hash
    // https://admin:123@sub.domain.com:1234/path/to?a=b&c=dd#D&G

    pos = memchr(url, '#', url_len);

    if (unlikely(pos != NULL)) {
        len = url_len - (pos + 1 - url);
        url_len -= 1 + len;

        lua_pushlstring(L, pos + 1, len);
        lua_setfield(L, 2, "hash");
    }

    // query
    // https://admin:123@sub.domain.com:1234/path/to?a=b&c=dd

    pos = memchr(url, '?', url_len);

    if (likely(pos != NULL)) {
        len = url_len - (pos + 1 - url);
        url_len -= 1 + len;

        lua_pushlstring(L, pos + 1, len);
        lua_setfield(L, 2, "query");
    }

    // protocol
    // https://admin:123@sub.domain.com:1234/path/to

    pos = memmem(url, url_len, "://", 3);

    if (likely(pos != NULL)) {
        len = pos - url;
        url_len -= 3 + len;

        lua_pushlstring(L, url, len);
        lua_setfield(L, 2, "protocol");

        url = pos + 3;
    }

    // path
    // admin:123@sub.domain.com:1234/path/to

    pos = memchr(url, '/', url_len);

    if (likely(pos != NULL)) {
        len = url_len - (pos - url);
        url_len -= len;

        lua_pushlstring(L, pos, len);
        lua_setfield(L, 2, "path");
    }

    // port
    // admin:123@sub.domain.com:1234
    // admin:123@[2001:db8::1]

    pos = memrchr(url, ':', url_len);
    pos2 = memrchr(url, ']', url_len);

    if (unlikely(pos != NULL && (pos2 == NULL || (pos2 < pos)))) {
        const char *port_pos = pos + 1;
        int port = parse_dec((char **)&port_pos);

        len = url_len - (pos + 1 - url);
        url_len -= 1 + len;

        lua_pushinteger(L, port);
        lua_setfield(L, 2, "port");
    }

    // username + password
    // admin:123@sub.domain.com

    pos = memchr(url, '@', url_len);

    if (unlikely(pos != NULL)) {
        len = pos - url;
        url_len -= 1 + len;

        pos2 = memchr(url, ':', len);

        if (likely(pos2 != NULL)) {
            lua_pushlstring(L, url, pos2 - url);
            lua_setfield(L, 2, "username");

            lua_pushlstring(L, pos2 + 1, pos - pos2 - 1);
            lua_setfield(L, 2, "password");
        } else {
            lua_pushlstring(L, url, len);
            lua_setfield(L, 2, "username");
        }

        url = pos + 1;
    }

    // host
    // sub.domain.com
    // [2001:db8::1] -> 2001:db8::1

    if (likely(url_len > 0)) {
        if (unlikely(url[0] == '[' && url[url_len - 1] == ']')) {
            lua_pushlstring(L, url + 1, url_len - 2);
            lua_setfield(L, 2, "host");
        } else {
            lua_pushlstring(L, url, url_len);
            lua_setfield(L, 2, "host");
        }
    }

    return 1;
}
