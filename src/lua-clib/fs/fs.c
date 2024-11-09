#include "fs.h"

LUAMOD_API int luaopen_fs(lua_State *L) {
    if (likely(luaL_newmetatable(L, MT_DIR))) {
        lua_pushcfunction(L, fs_dir_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, fs_dir_call);
        lua_setfield(L, -2, "__call");
    }

    luaL_newlib(L, fs_index);

    lua_createtable(L, 0, 9);
        luaF_set_kv_int(L, -1, "UNKNOWN", DT_UNKNOWN);
        luaF_set_kv_int(L, -1, "FIFO", DT_FIFO);
        luaF_set_kv_int(L, -1, "CHR", DT_CHR);
        luaF_set_kv_int(L, -1, "DIR", DT_DIR);
        luaF_set_kv_int(L, -1, "BLK", DT_BLK);
        luaF_set_kv_int(L, -1, "REG", DT_REG);
        luaF_set_kv_int(L, -1, "LNK", DT_LNK);
        luaF_set_kv_int(L, -1, "SOCK", DT_SOCK);
        luaF_set_kv_int(L, -1, "WHT", DT_WHT);
    lua_setfield(L, -2, "type");

    return 1;
}

int fs_readdir(lua_State *L) {
    luaF_need_args(L, 1, "readdir");
    luaL_checktype(L, 1, LUA_TSTRING);

    const char *path = lua_tostring(L, 1);

    ud_dir *dir = luaF_new_ud_or_error(L, sizeof(ud_dir), 1);

    dir->dir = opendir(path);

    if (unlikely(dir->dir == NULL)) {
        luaF_error_errno(L, "opendir failed; path: %s", path);
    }

    luaL_setmetatable(L, MT_DIR);

    lua_insert(L, 1); // dir, path
    lua_setiuservalue(L, 1, DIR_UV_IDX_PATH);

    return 1; // dir
}

static int fs_dir_gc(lua_State *L) {
    ud_dir *dir = luaL_checkudata(L, 1, MT_DIR);

    if (dir->dir != NULL) {
        int status = closedir(dir->dir);
        dir->dir = NULL;

        if (unlikely(status != 0)) {
            lua_getiuservalue(L, 1, DIR_UV_IDX_PATH);
            luaF_error_errno(L, "closedir failed; path: %s",
                lua_tostring(L, -1));
        }
    }

    return 0;
}

static int fs_dir_call(lua_State *L) {
    ud_dir *dir = luaL_checkudata(L, 1, MT_DIR);

    if (unlikely(dir->dir == NULL)) {
        return 0; // after gc
    }

    errno = 0;
    struct dirent *entry = readdir(dir->dir);

    if (unlikely(entry == NULL)) {
        if (unlikely(errno != 0)) {
            lua_getiuservalue(L, 1, DIR_UV_IDX_PATH);
            luaF_error_errno(L, "readdir failed; path: %s",
                lua_tostring(L, -1));
        }

        // end of directory reached
        fs_dir_gc(L);
        return 0;
    }

    const char *path = entry->d_name;

    if (path[0] == '.' && (!path[1] || (path[1] == '.' && !path[2]))) {
        lua_settop(L, 1);
        return fs_dir_call(L); // skip . and ..
    }

    lua_pushstring(L, path);
    lua_pushinteger(L, entry->d_type);

    return 2; // path, type
}

int fs_readfile(lua_State *L) {
    luaF_need_args(L, 1, "readfile");
    luaL_checktype(L, 1, LUA_TSTRING);

    const char *path = lua_tostring(L, 1);

    if (unlikely(strlen(path) == 0)) {
        luaL_error(L, "path is NULL");
    }

    struct stat st;

    if (unlikely(stat(path, &st) != 0)) {
        luaF_error_errno(L, "stat failed; path: %s", path);
    }

    if (unlikely(!S_ISREG(st.st_mode))) {
        luaL_error(L, "not a regular file; path: %s", path);
    }

    off_t size = st.st_size;

    if (unlikely(size == 0)) {
        lua_pushliteral(L, "");
        return 1;
    }

    char *buf = luaF_malloc_or_error(L, size);
    FILE *fp = fopen(path, "rb");

    if (unlikely(fp == NULL)) {
        free(buf);
        luaF_error_errno(L, "fopen failed; path: %s", path);
    }

    size_t read_n = fread(buf, size, 1, fp);

    if (unlikely(read_n != 1)) {
        int has_error = ferror(fp);

        free(buf);
        fclose(fp);

        if (has_error) {
            luaF_error_errno(L, "fread failed; path: %s", path);
        } else {
            luaL_error(L, "fread failed; path: %s", path);
        }
    }

    if (unlikely(fclose(fp) != 0)) {
        free(buf);
        luaF_error_errno(L, "fclose failed; path: %s", path);
    }

    lua_pushlstring(L, buf, size);

    free(buf);

    return 1;
}

int fs_parse_path(lua_State *L) {
    luaF_need_args(L, 1, "readfile");
    luaL_checktype(L, 1, LUA_TSTRING);

    size_t len;
    const char *path = lua_tolstring(L, 1, &len);
    const char *slash = memrchr(path, '/', len);

    if (likely(slash != NULL)) {
        len -= slash + 1 - path;
        path = slash + 1;
    }

    const char *dot = memrchr(path, '.', len);

    if (likely(dot != NULL)) {
        lua_pushlstring(L, path, dot - path); // name
        lua_pushlstring(L, dot + 1, len - (dot + 1 - path)); // ext
        return 2; // name without ext, ext
    }

    lua_pushlstring(L, path, len); // name
    lua_pushliteral(L, ""); // ext
    return 2; // name without ext, ext
}
