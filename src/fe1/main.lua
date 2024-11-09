warn "@on"

package.path = "/furiend/src/lua-lib/?.lua;/furiend/src/fe1/?.lua"
    .. ";/furiend/src/dc/lib/?.lua"
package.cpath = "/furiend/src/lua-clib/?/?.so"

local async = require "async"
local loop, wait = async.loop, async.wait
local redis = require "redis"
local http = require "http"
local log = require "log"
local json = require "json"
local waitall = require "waitall"
local json_response = require "lib.json_response"
local serve_content = require "lib.serve_content"

local config = require "config"
local in_docker = os.getenv("IN_DOCKER")

loop(function()
    log("env", in_docker and "in docker" or "not in docker")

    local rc -- redis client

    local dc = function(type, payload)
        local event = {
            type = type,
            from = config.id,
            payload = payload,
        }

        wait(rc:publish(config.dc, json.stringify(event)))
    end

    -- redis

    rc = redis.client(config.redis)

    wait(rc:connect())
    log("connected to redis", config.redis.ip4, config.redis.port)

    wait(rc:hello(config.redis))
    log("redis authorized", config.redis.client_name)

    dc "start"

    -- http server

    local serv_conf = config.http_server

    if in_docker then
        serv_conf.ip4 = serv_conf.ip4_docker
    end

    local server = http.server(serv_conf)

    server:on_request(function(req, res)
        req.session = {
            rc = rc,
            dc = dc,
            static = serv_conf.static,
        }

        local body = req.body

        if body and #body > 0 then
            req.session.parsed_body = json.parse(body)
        end

        serve_content(req, res)
    end)

    server:on_error(function(req, res, err)
        log("http server error", err, req)
        dc("http_server_error", { error = err, req = req })
        json_response(res, { error = err })
    end)

    server:listen()

    log("http server is listening on", serv_conf.ip4, serv_conf.port)
    dc "http_server_start"

    waitall(
        rc:join(),
        server:join()
    )
end)
