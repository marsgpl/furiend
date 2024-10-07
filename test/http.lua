local perf = require "test.perf"
local trace = require "trace"
local http = require "http"
local dns = require "dns"
local json = require "json"
local async = require "async"
local wait = async.wait

return function()
    local _, r
    local host = "lua.org"

    perf()
        local dc = dns.client { ip4 = "1.1.1.1" }
        local ips = wait(dc:resolve { name = host })
        trace("host:", host)
        trace("ips:", ips)
    perf("dns resolve")

    perf()
        r = wait(http.request {
            host = host,
            ip4 = ips[1],
            user_agent = "Furiend/Test; Linux; Lua",
            show_request = true,
        })
        trace(r)
        assert(r.code == 301, "code 301 expected")
    perf("http request")

    perf()
        r = wait(http.request {
            https = true,
            https_verify_cert = false,
            ip4 = "8.8.8.8",
            path = "/resolve?name=" .. host .. "&type=A",
            show_request = true,
        })
        trace(r)
        assert(r.code == 200, "code 200 expected")
        r = json.parse(r.body)
        trace(r)
        assert(r.Answer[1].data == ips[1], "ip4 mismatch")
    perf("https request")
end
