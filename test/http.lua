local http_dns_resolve = require "http_dns_resolve"
local perf = require "test.perf"
local trace = require "trace"
local http = require "http"
local async = require "async"
local wait = async.wait

return function()
    perf()
        local ip4 = "127.0.0.1"
        local port = 24862

        local server = http.server {
            ip4 = ip4,
            port = port,
        }

        server:on_request(function(req, res)
            res:set_status(tonumber(req.body), req.body)
            res:set_body("123\0aaa\0" .. req.body)
        end)

        server:listen()

        for i = 1, 10000 do
            local request = http.request {
                ip4 = ip4,
                port = port,
                user_agent = "Furiend/Test; Linux; Lua",
                show_request = true,
                method = "POST",
                body = tostring(i),
            }

            local result = wait(request)

            assert(result.status_code == i,
                "status code mismatch")

            assert(result.status_message == tostring(i),
                "status message mismatch")

            assert(result.body == "123\0aaa\0" .. tostring(i),
                "response body mismatch")
        end

        server:stop()
    perf("http server")

    perf()
        local host = "lua.org"
        local addr = http_dns_resolve(host)
        trace("host:", host)
        trace("addr:", addr)
    perf("dns https resolve")
end
