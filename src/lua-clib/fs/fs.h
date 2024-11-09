#ifndef LUA_LIB_FS_H
#define LUA_LIB_FS_H

#define _GNU_SOURCE

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <furiend/shared.h>

#define MT_DIR "dir*"

#define DIR_UV_IDX_PATH 1

typedef struct {
    DIR *dir;
} ud_dir;

LUAMOD_API int luaopen_fs(lua_State *L);

int fs_readdir(lua_State *L);
int fs_readfile(lua_State *L);
int fs_parse_path(lua_State *L);

static int fs_dir_gc(lua_State *L);
static int fs_dir_call(lua_State *L);

static const luaL_Reg fs_index[] = {
    { "readdir", fs_readdir },
    { "readfile", fs_readfile },
    { "parse_path", fs_parse_path },
    { "type", NULL }, // just reserve space
    { NULL, NULL }
};

#endif
