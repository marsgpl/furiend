local perf = require "test.perf"
local trace = require "trace"
local dns = require "dns"
local async = require "async"
local wait = async.wait

-- 1.1.1.1
-- 8.8.8.8
-- 8.8.4.4
-- 2001:4860:4860::8888
-- 2001:4860:4860::8844

local names = {
    "cloudflare.com",
    "google.com",
    "api.telegram.org",
}

return function()
    perf()
        local client = dns.client { ip4 = "1.1.1.1", parallel = true }
        local requests = {}
        for _, name in ipairs(names) do
            requests[name] = client:resolve { name = name }
        end
    perf("dns prepare")

    perf()
        for name, request in pairs(requests) do
            trace(name, wait(request))
        end
    perf("dns resolve")
end
