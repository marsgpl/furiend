local http = require "http"
local json = require "json"
local parse_url = require "url"

local function error_bad_request(msg)
    error { code = 400, msg = msg }
end

local function validate_req(req, url, secret_token)
    local method = req.method
    local path = req.path
    local host = req.headers["Host"]
    local req_token = req.headers["X-Telegram-Bot-Api-Secret-Token"]

    if method ~= "POST" then
        error_bad_request("bad method")
    end

    if path ~= url.path then
        error_bad_request("bad path")
    end

    if host ~= url.host then
        error_bad_request("bad host")
    end

    if req_token ~= secret_token then
        error_bad_request("bad token")
    end
end

return function(config)
    local url = parse_url(config.url)

    local server = http.server {
        ip4 = config.ip4,
        port = config.port,
    }

    server:on_request(function(req, res)
        validate_req(req, url, config.secret_token)

        local data = json.parse(req.body)

        config.on_data(req, data)

        res:push_header("Content-Type", "application/json")
        res:set_body(json.stringify({
            ok = true,
        }))
    end)

    server:on_error(function(req, res, err)
        config.on_error(req, err)

        if err.code then
            res:set_status(err.code)
        end

        res:push_header("Content-Type", "application/json")
        res:set_body(json.stringify({
            ok = false,
        }))
    end)

    return server
end
