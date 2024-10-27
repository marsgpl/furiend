local perf = require "test.perf"
local trace = require "trace"
local redis = require "redis"
local async = require "async"
local wait, pwait = async.wait, async.pwait

return function()
    perf()
        local client = redis.client {
            ip4 = "172.20.0.3",
            port = 30303,
        }

        wait(client:connect())
    perf("redis connect")

    perf()
        trace(wait(client:query("hello 3 auth default LocalPassword123 setname test\r\n")))
    perf("redis auth")

    perf()
        local reqs = {
            client:query("ping\r\n"),
            client:query("lastsave\r\n"),
            client:query("ping hello\r\n"),
            client:query("get non-existing-key\r\n"),
            client:query("eval 'return {1,2}' 0\r\n"),
        }

        for _, req in ipairs(reqs) do
            local data, dt = wait(req)
            trace(string.char(dt), data)
        end
    perf("redis types")

    perf()
        local status, errmsg = pwait(client:query("non-existing A B\r\n"))
        trace(status, errmsg)
        assert(not status, "non-existing command failed to raise error")
    perf("redis check err")

    client:close()
end
