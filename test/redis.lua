local perf = require "test.perf"
local trace = require "trace"
local redis = require "redis"
local async = require "async"
local wait = async.wait

return function()
    perf()
        local client = redis.client {
            ip4 = "172.20.0.3",
            port = 30303,
        }

        wait(client:connect())
        wait(client:ping())

        client:close()
    perf("redis client")
end
