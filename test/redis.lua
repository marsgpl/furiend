local perf = require "test.perf"
local trace = require "trace"
local redis = require "redis"
local async = require "async"
local wait = async.wait

return function()
    perf()
        local ip4 = "127.0.0.1"
        local port = 18001

        local client = redis.client {
            ip4 = ip4,
            port = port,
        }

        wait(client:ping())

        client:close()
    perf("redis client")
end
