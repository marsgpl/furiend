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
    do
        local data, t = wait(client:query(
            "hello 3 auth default LocalPassword123 setname test\r\n"
        ))
        trace(data)
        assert(t == redis.type.MAP, "incorrect type")
        assert(data.proto == 3, "incorrect data")
    end
    perf("redis auth")

    perf()
    do
        local reqs = {
            client:query("ping\r\n"),
            client:query("lastsave\r\n"),
            client:query("client info\r\n"),
        }

        for _, req in ipairs(reqs) do
            local data, t = wait(req)
            local meta = type(data) == "table" and getmetatable(data) or nil

            if meta then
                trace(data, t, meta)
            else
                trace(data, t)
            end
        end
    end
    perf("redis simple requests")

    perf()
    do
        local status, errmsg = pwait(client:query("non-existing A B\r\n"))
        trace(status, errmsg)
        assert(not status, "non-existing command failed to raise error")
    end
    perf("redis check err")

    perf()
    do
        local channel_name = "test\0channel"

        trace("sub res:", wait(client:subscribe(channel_name, function(push)
            trace("push:", push)
        end)))

        trace("pub res:", wait(client:publish(channel_name, "some\0message")))

        trace(wait(client:query("ping\r\n")))

        trace("unsub res:", wait(client:unsubscribe(channel_name)))
    end
    perf("redis push")

    perf()
    do
        local reqs = {}

        for _ = 1, 10 do
            table.insert(reqs, client:query("lastsave\r\n"))
        end

        for _, req in ipairs(reqs) do
            wait(req)
        end
    end
    perf("redis flood")

    perf()
    do
        local msg = ""

        for _ = 1, 1000 do
            msg = msg
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            .. "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        end

        trace("len:", #msg)

        local ping = "*2\r\n$4\r\nping\r\n$" .. #msg .. "\r\n" .. msg .. "\r\n"
        local response = wait(client:query(ping))

        assert(response == msg, "response is invalid")
    end
    perf("redis big packet")

    client:close()
end
