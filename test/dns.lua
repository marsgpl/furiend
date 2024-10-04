local perf = require "test.perf"
local trace = require "trace"
local dns = require "dns"
local async = require "async"
local pwait = async.pwait

-- 1.1.1.1
-- 8.8.8.8
-- 8.8.4.4
-- 2001:4860:4860::8888
-- 2001:4860:4860::8844

return function()
    perf()
        local client = dns.client { ip4 = "1.1.1.1" }
        local r1 = client:resolve { name = "cloudflare.com" }
        local r2 = client:resolve { name = "google.com" }
    perf("dns prepare")

    perf()
        trace("google.com:", pwait(r1))
        trace("cloudflare.com:", pwait(r2))
    perf("dns resolve")
end
