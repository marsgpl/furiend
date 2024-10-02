#include "trace.h"

LUAMOD_API int luaopen_trace(lua_State *L) {
    lua_pushcfunction(L, trace);
    return 1;
}

static int trace(lua_State *L) {
    int args_n = lua_gettop(L);

    if (args_n == 0) {
        return 0;
    }

    lua_createtable(L, 0, CACHE_MIN_SIZE);
    lua_insert(L, CACHE_IDX); // must be 1

    for (int index = 1; index <= args_n; ++index) {
        trace_value(L, index + 1, 0, MODE_VARIABLE);
        fputc(index == args_n ? '\n' : ' ', stderr);
    }

    return 0;
}

static void trace_value(lua_State *L, int index, int depth, int mode) {
    FILE *stream = stderr;

    if (depth > 0 && mode != MODE_TABLE_VALUE) {
        fprintf(stream, "%*s", depth * INDENT_LEN, "");
    }

    int type = lua_type(L, index);

    if (type == LUA_TSTRING) {
        fprintf(stream, RED);
    } else if (type == LUA_TNUMBER) {
        if (lua_isinteger(L, index)) {
            fprintf(stream, CYAN);
        } else {
            fprintf(stream, PURPLE);
        }
    }

    luaF_print_value(L, index, stream);

    if (type == LUA_TSTRING) {
        fprintf(stream, RESET);
    } else if (type == LUA_TNUMBER) {
        fprintf(stream, RESET);
    } else if (type == LUA_TTABLE) {
        trace_table(L, index, depth);
    }

    if (type == LUA_TTABLE || type == LUA_TUSERDATA) {
        if (lua_getmetatable(L, index)) {
            fprintf(stream, " " YELLOW "* metatable: %p" RESET,
                lua_topointer(L, -1));
            lua_pop(L, 1); // lua_getmetatable
        }
    }

    if (mode == MODE_TABLE_KEY) {
        fprintf(stream, " = ");
    } else if (mode == MODE_TABLE_VALUE) {
        fputc('\n', stream);
    }
}

static void trace_table(lua_State *L, int index, int depth) {
    FILE *stream = stderr;

    luaL_checkstack(L, 4, "trace_table");

    if (cache_get(L, index) == LUA_TNIL) {
        lua_pop(L, 1); // cache_get
        cache_set(L, index);
    } else {
        lua_pop(L, 1); // cache_get
        fprintf(stream, " { ... }");
        return;
    }

    fprintf(stream, " {");

    int k_index = 0;
    int v_index = 0;

    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (!v_index) {
            v_index = lua_gettop(L);
            k_index = v_index - 1;
            fputc('\n', stream);
        }

        if (depth >= MAX_DEPTH) {
            luaF_warning(L, "trace: max depth exceeded: %d", MAX_DEPTH);
        } else {
            trace_value(L, k_index, depth + 1, MODE_TABLE_KEY);
            trace_value(L, v_index, depth + 1, MODE_TABLE_VALUE);
        }

        lua_pop(L, 1); // lua_next
    }

    if (depth > 0 && v_index > 0) {
        fprintf(stream, "%*s", depth * INDENT_LEN, "");
    }

    fputc('}', stream);
}
