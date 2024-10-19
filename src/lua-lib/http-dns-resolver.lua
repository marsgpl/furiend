local async = require "async"
local wait = async.wait
local http = require "http"
local json = require "json"

-- {"Status":0,"TC":false,"RD":true,"RA":true,"AD":false,"CD":false,"Question":[{"name":"api.telegram.org.","type":1}],"Answer":[{"name":"api.telegram.org.","type":1,"TTL":241,"data":"149.154.167.220"}]}
return function(host)
    assert(type(host) == "string")

    local res = wait(http.request {
        https = true,
        https_verify_cert = false,
        ip4 = "8.8.8.8",
        path = "/resolve?name=" .. host .. "&type=A",
    })

    local response = json.parse(res.body)
    local ip4 = response.Answer[1].data

    if not ip4 then
        error("no results found; host: " .. host .. "; body: " .. res.body)
    end

    return ip4
end
