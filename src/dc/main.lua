warn "@on"

package.path = "/furiend/?.lua;/furiend/src/lua-lib/?.lua"
package.cpath = "/furiend/src/lua-clib/?/?.so"

local async = require "async"
local loop, wait = async.loop, async.wait
local redis = require "redis"
local log = require "log"
local json = require "json"
local time = require "time"
local trace = require "trace"

local config = require "src.dc.config"

loop(function()
    local cmd_ids = {}

    local rc -- redis client

    local on_push = function(push)
        push = json.parse(push)
        push.time = time()

        trace("push:", push)

        if push.from == "fe2"
            and push.type == "tg_event"
            and push.event.message.text
        then
            local cmd_id = cmd_ids[push.from] or 1

            wait(rc:publish(push.from, json.stringify {
                from = config.id,
                type = "cmd",
                cmd = "sendChatAction",
                cmd_id = cmd_id,
                body = {
                    chat_id = push.event.message.chat.id,
                    action = "typing",
                },
            }))

            cmd_ids[push.from] = cmd_id + 1
        end
    end

    -- redis

    config.redis.client_name = config.id

    rc = redis.client(config.redis)

    wait(rc:connect())
    log("connected to redis", config.redis.ip4, config.redis.port)

    wait(rc:hello(config.redis))
    log("redis authorized", config.redis.client_name)

    wait(rc:subscribe(config.id, on_push))
    log("redis subscribed", config.id)

    -- wait for graceful shutdown

    wait(rc:join())
end)
