local perf = require "test.perf"
local trace = require "trace"
local http = require "http"
local dns = require "dns"
local async = require "async"
local wait, pwait = async.wait, async.pwait

return function()
    local host = "lua.org"

    perf()
        local dc = dns.client { ip4 = "1.1.1.1" }
        local ips = wait(dc:resolve { name = host })
        trace("host:", host)
        trace("ips:", ips)
    perf("dns resolve")

    perf()
        local req = http.request {
            https = true,
            https_verify_cert = false,
            host = host,
            ip4 = ips[1],
            user_agent = "Furiend/Test; Linux; Lua",
            show_request = true,
        }
    perf("http request prepare")

    perf()
        trace(pwait(req))
    perf("http request duration")
end
