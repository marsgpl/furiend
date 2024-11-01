local perf = require "test.perf"
local trace = require "trace"
local redis = require "redis"
local equal = require "equal"

local unpack_samples = {
    { "-Error message\r\n", "Error message", redis.type.ERR },
    { "+OK\r\n", "OK", redis.type.STR },
    { "_\r\n", nil, redis.type.NULL },

    { "#t\r\n", true, redis.type.BOOL },
    { "#f\r\n", false, redis.type.BOOL },

    { ":123456789\r\n", 123456789, redis.type.INT },
    { ":+123\r\n", 123, redis.type.INT },
    { ":-123\r\n", -123, redis.type.INT },

    { ",1.23\r\n", 1.23, redis.type.DOUBLE },
    { ",+1.2E+3\r\n", 1.2e3, redis.type.DOUBLE },
    { ",inf\r\n", 1/0, redis.type.DOUBLE },
    { ",-inf\r\n", -1/0, redis.type.DOUBLE },
    -- { ",nan\r\n", 0/0, redis.type.DOUBLE }, -- nan != nan, how to test?

    {
        "(3492890328409238509324850943850943825024385\r\n",
        "3492890328409238509324850943850943825024385",
        redis.type.BIG_NUM
    },
    { "(+2147483647002\r\n", "+2147483647002", redis.type.BIG_NUM },
    { "(-2147483647003\r\n", "-2147483647003", redis.type.BIG_NUM },

    { "$-1\r\n", nil, redis.type.NULL },
    { "$0\r\n\r\n", "", redis.type.BULK },
    { "$5\r\nhello\r\n", "hello", redis.type.BULK },
    { "!10\r\nSome\0error\r\n", "Some\0error", redis.type.BULK_ERR },
    { "=7\r\nutf8:hi\r\n", "utf8:hi", redis.type.VSTR },

    { "%-1\r\n", nil, redis.type.NULL },
    { "%0\r\n", {}, redis.type.MAP },
    {
        "%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\n"
        .. "version\r\n$5\r\n7.4.1\r\n$5\r\nproto\r\n"
        .. ":3\r\n$2\r\nid\r\n:137\r\n$4\r\nmode\r\n"
        .. "$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\n"
        .. "master\r\n$7\r\nmodules\r\n*0\r\n",
        {
            server = "redis",
            version = "7.4.1",
            proto = 3,
            id = 137,
            mode = "standalone",
            role = "master",
            modules = {},
        },
        redis.type.MAP
    },
    {
        "|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n"
        .. ",0.1923\r\n$1\r\nb\r\n,0.0012\r\n*2\r\n:1\r\n:2\r\n",
        {1,2},
        redis.type.ARR,
        {
            ["key-popularity"] = {
                a = 0.1923,
                b = 0.0012,
            },
        }
    },

    { "*-1\r\n", nil, redis.type.NULL },
    { "*0\r\n", {}, redis.type.ARR },
    { "~0\r\n", {}, redis.type.SET },
    { ">0\r\n", {}, redis.type.PUSH },
    {
        "*2\r\n$5\r\nhello\r\n$5\r\nworld\r\n",
        { "hello", "world" },
        redis.type.ARR
    },
    {
        "*3\r\n:1\r\n:2\r\n:3\r\n",
        { 1, 2, 3 },
        redis.type.ARR
    },
    {
        "*3\r\n:1\r\n:2\r\n|1\r\n+ttl\r\n:3600\r\n:3\r\n",
        { 1, 2, 3 },
        redis.type.ARR
    },
}

local pack_sample = {
    true,
    false,
    "some\0thing",
    1/0,
    -1/0,
    0/0,
    {
        b = -2.34E-10,
    }
}

local packet_sample = "*7\r\n"
    .. "#t\r\n"
    .. "#f\r\n"
    .. "$10\r\nsome\0thing\r\n"
    .. ",inf\r\n"
    .. ",-inf\r\n"
    .. ",nan\r\n"
    .. "%1\r\n"
        .. "$1\r\nb\r\n"
        .. ",-2.34e-10\r\n"

return function()
    perf()
    for _, sample in ipairs(unpack_samples) do
        local buffer, need_data, need_type, need_meta = table.unpack(sample)

        trace(buffer)

        local data, t = redis.unpack(buffer)
        local meta = type(data) == "table" and getmetatable(data) or nil

        if meta then
            trace(data, t, meta)
        else
            trace(data, t)
        end

        assert(equal(need_data, data), "incorrect data")
        assert(equal(need_type, t), "incorrect type")
        assert(equal(need_meta, meta), "incorrect meta")
    end
    perf("resp unpack")

    perf()
        local packet = redis.pack(pack_sample)
        trace(packet)
        trace(packet_sample)
        assert(equal(packet, packet_sample))
    perf("resp pack")
end
