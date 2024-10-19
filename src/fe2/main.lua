warn "@on"

package.path = "/furiend/?.lua;/furiend/src/lua-lib/?.lua"
package.cpath = "/furiend/src/lua-clib/?/?.so"

local async = require "async"
local loop, wait = async.loop, async.wait
local tgbot = require "telegram-bot"
local tgwh = require "telegram-bot-webhook"
local dns = require "dns"
local log = require "log"

local config = require "src.fe2.config"
local in_docker = os.getenv("IN_DOCKER")

loop(function()
    log("env", in_docker and "in docker" or "not in docker")

    local tg_conf = config.telegram
    local wh_conf = tg_conf.webhook

    tg_conf.dns_client = dns.client { ip4 = config.dns.ip4 }

    local tg = tgbot(tg_conf)

    if in_docker then
        wh_conf.ip4 = wh_conf.ip4_docker
    end

    wh_conf.on_data = function(req, data)
        log("tg new event", req, data)
    end

    wh_conf.on_error = function(req, err)
        log("error while processing tg event", err, req)
    end

    local wh = tgwh(wh_conf)
    local wh_thread = wh:listen()

    log("tgwh is listening on", wh_conf.ip4, wh_conf.port)

    local me = tg:get_me()

    log("id", me.id)
    log("username", me.username)
    log("display name", me.first_name)

    local wh_info = tg:get_webhook_info()

    if wh_info.url == wh_conf.url then
        log("tgwh exists", wh_info.url)
        log("pending updates", wh_info.pending_update_count)
    else
        tg:set_webhook(wh_conf)
        log("tgwh was set", wh_conf.url)
    end

    wait(wh_thread)
end)
