local trace = require "trace"
local equal = require "equal"

local t1 = coroutine.create(print)
local t2 = coroutine.create(print)
local registry = debug.getregistry()
local t = {}; t.t = t -- looped table

local good = {
    -- LUA_TNIL
    nil, nil,
    -- LUA_TBOOLEAN
    true, true,
    false, false,
    -- LUA_TFUNCTION
    print, print,
    -- LUA_TTHREAD
    t1, t1,
    -- LUA_TUSERDATA
    io.stdin, io.stdin,
    -- LUA_TLIGHTUSERDATA
    registry._CLIBS[1], registry._CLIBS[1],
    -- LUA_TNUMBER
    123, 123,
    12.3, 1.23e1,
    0, -0, -- 0 vs -0
    1/0, math.huge, -- inf
    -1/0, -math.huge, -- -inf
    math.mininteger, math.maxinteger + 1,
    -- LUA_TSTRING
    "hi\x00how", "hi\0how",
    -- LUA_TTABLE
    {}, {},
    {{{}}}, {{{}}},
    { "hi" }, { [1] = "hi" },
    { a = 1 }, { ["a"] = 0+1 },
    {{t,{1,2}}}, {{t,{1,2}}},
}

local bad = {
    -- LUA_TNIL
    nil, false,
    -- LUA_TBOOLEAN
    1, true,
    0, false,
    -- LUA_TFUNCTION
    print, trace,
    -- LUA_TTHREAD
    t1, t2,
    -- LUA_TUSERDATA
    io.stdout, io.stderr,
    -- LUA_TLIGHTUSERDATA
    registry._CLIBS[1], registry._CLIBS[2],
    -- LUA_TNUMBER
    123, 123.0,
    0/0, 1/0, -- nan
    0/0, 0/0, -- nan
    -- LUA_TSTRING
    "hi\x00how", "hi",
    -- LUA_TTABLE
    { 1, 2 }, { 1, 2, 3 },
    {{{}}}, {{{1}}},
    { "hi" }, { "ho" },
    { a = 1 }, { a = 1.0 },
    { a = 1 }, { b = 1 },
    {{t,{1,2}}}, {{t,{true,2}}},
}

return function()
    for i = 1, #good, 2 do
        local a = good[i]
        local b = good[i + 1]

        trace("equal:", a, b)
        assert(equal(a, b), "not equal")
    end

    for i = 1, #bad, 2 do
        local a = bad[i]
        local b = bad[i + 1]

        trace("not equal:", a, b)
        assert(not equal(a, b), "equal")
    end
end
