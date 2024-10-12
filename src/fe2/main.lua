warn "@on"

package.path = "/furiend/?.lua;/furiend/src/lua-lib/?.lua"
package.cpath = "/furiend/src/lua-clib/?/?.so"

local async = require "async"
local loop = async.loop
local tgbot = require "telegram-bot"
local config = require "src.entourage.config-e2"
local log = require "log"

loop(function()
    local bot = tgbot(config.telegram)

    local me = bot:get_me()

    log("id", me.id)
    log("login", me.username)
    log("name", me.first_name)

    local wh_info = bot:get_webhook_info()

    if wh_info.url == config.telegram.hook.url then
        log("webhook", wh_info.url)
    else
        bot:set_webhook(config.telegram.hook)
        log("webhook", config.telegram.hook.url)
    end
end)
