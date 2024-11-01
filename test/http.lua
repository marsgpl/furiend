local resolve_host = require "http-dns-resolver"
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
            res:set_body("123\0aaa")
            trace(req)
            trace(res)
        end)

        server:listen()

        local request = http.request {
            ip4 = ip4,
            port = port,
            user_agent = "Furiend/Test; Linux; Lua",
            show_request = true,
        }

        local result = wait(request)

        trace(result)
        assert(result.status_code == 200, "status code")

        server:stop()
    perf("http server")

    perf()
        local host = "lua.org"
        local addr = resolve_host(host)
        trace("host:", host)
        trace("addr:", addr)
    perf("dns https resolve")
end
