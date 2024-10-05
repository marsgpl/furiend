#ifndef LUA_LIB_JSON_H
#define LUA_LIB_JSON_H

#include <yyjson.h>
#include <furiend/lib.h>

#define fail(L, msg, ...) { \
    lua_pushfstring(L, msg __VA_OPT__(,) __VA_ARGS__); \
    lua_pushboolean(L, 0); \
    lua_insert(L, -2); \
}

/* wait for pr https://github.com/ibireme/yyjson/issues/185
#define yyjson_mut_obj_add_keyn_val(DOC, OBJ, KEY, KEY_LEN, VAL) { \
    yyjson_mut_val *NEW_KEY = unsafe_yyjson_mut_val(DOC, 2); \
    if (yyjson_likely(NEW_KEY)) { \
        size_t LEN = unsafe_yyjson_get_len(OBJ); \
        yyjson_mut_val *NEW_VAL = NEW_KEY + 1; \
        bool noesc = unsafe_yyjson_is_str_noesc(KEY, KEY_LEN); \
        NEW_KEY->tag = YYJSON_TYPE_STR; \
        NEW_KEY->tag |= noesc ? YYJSON_SUBTYPE_NOESC : YYJSON_SUBTYPE_NONE; \
        NEW_KEY->tag |= (uint64_t)KEY_LEN << YYJSON_TAG_BIT; \
        NEW_KEY->uni.str = KEY; \
        NEW_VAL = VAL; \
        unsafe_yyjson_mut_obj_add(OBJ, NEW_KEY, NEW_VAL, LEN); \
    } \
}
*/

LUAMOD_API int luaopen_json(lua_State *L);

int json_parse(lua_State *L);
int json_stringify(lua_State *L);

static int is_array(lua_State *L, int index);
static int json_parse_value(lua_State *L, yyjson_val *value);

static yyjson_mut_val *json_stringify_value(
    lua_State *L,
    yyjson_mut_doc *doc,
    int index,
    int cache_index);

static yyjson_mut_val *json_stringify_table(
    lua_State *L,
    yyjson_mut_doc *doc,
    int index,
    int cache_index);

static const luaL_Reg json_index[] = {
    { "parse", json_parse },
    { "stringify", json_stringify },
    { NULL, NULL }
};

#endif
