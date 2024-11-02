warn "@on"

package.path = "/furiend/src/lua-lib/?.lua;/furiend/src/fe2/?.lua"
package.cpath = "/furiend/src/lua-clib/?/?.so"

local async = require "async"
local loop, wait = async.loop, async.wait
local redis = require "redis"
local log = require "log"
local json = require "json"
local dns = require "dns"
local tgbot = require "telegram-bot"
local tgwh = require "telegram-bot-webhook"
local is_dc_cmd = require "lib.is-dc-cmd"
local is_tg_event = require "lib.is-tg-event"

local config = require "config"
local in_docker = os.getenv("IN_DOCKER")

loop(function()
    log("env", in_docker and "in docker" or "not in docker")

    local rc -- redis client
    local tg -- telegram bot api
    local wh -- telegram bot web hook

    local dc = function(event)
        event.from = config.id
        wait(rc:publish(config.dc, json.stringify(event)))
    end

    local on_push = function(push)
        assert(is_dc_cmd(push), "bad push")
        log("cmd", push.cmd_id, push.cmd)

        local result = tg:request("POST", push.cmd, push.body)

        log("cmd ok", push.cmd_id)
        dc { type = "cmd_ok", cmd_id = push.cmd_id, result = result }
    end

    local on_push_safe = function(push)
        push = json.parse(push)
        local ok, errmsg = pcall(on_push, push)

        if not ok then
            log("push error", errmsg, push)
            dc { type = "push error", push = push, error = errmsg }
        end
    end

    -- redis

    rc = redis.client(config.redis)

    wait(rc:connect())
    log("connected to redis", config.redis.ip4, config.redis.port)

    wait(rc:hello(config.redis))
    log("redis authorized", config.redis.client_name)

    wait(rc:subscribe(config.id, on_push_safe))
    log("redis subscribed", config.id)

    dc { type = "start" }

    -- tgwh

    local tg_conf = config.telegram
    local wh_conf = tg_conf.webhook

    tg_conf.dns_client = dns.client {
        ip4 = config.dns.ip4,
        timeout = config.dns.timeout,
    }

    tg = tgbot(tg_conf)

    if in_docker then
        wh_conf.ip4 = wh_conf.ip4_docker
    end

    wh_conf.on_data = function(req, event)
        if not is_tg_event(event) then
            error("tgwh received bad event: " .. json.stringify(event))
        end

        log("tg event", event.update_id)
        dc { type = "tg_event", event = event }
    end

    wh_conf.on_error = function(req, errmsg)
        log("tg error", errmsg, req)
        dc { type = "tg_error", error = errmsg, req = req }
    end

    wh = tgwh(wh_conf)
    wh:listen()

    log("tgwh is listening", wh_conf.ip4, wh_conf.port)
    dc { type = "tg_start" }

    -- get me

    local me = tg:get_me()

    log("id", me.id)
    log("username", me.username)
    log("display name", me.first_name)
    dc { type = "tg_me", me = me }

    -- set webhook

    tg:set_webhook(wh_conf)
    log("tgwh set", wh_conf.url)

    local wh_info = tg:get_webhook_info()
    log("tgwh pending updates", wh_info.pending_update_count)
    dc { type = "tg_wh_info", info = wh_info }

    -- wait for graceful shutdown

    wait(rc:join()) -- TODO: migrate to race
    -- race(rc:join(), wh:join())
end)
