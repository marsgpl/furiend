warn "@on"

package.path = "/furiend/src/lua-lib/?.lua;/furiend/src/dc/?.lua"
package.cpath = "/furiend/src/lua-clib/?/?.so"

local async = require "async"
local loop, wait = async.loop, async.wait
local redis = require "redis"
local log = require "log"
local time = require "time"
local json = require "json"
local error_kv = require "error_kv"
local logic = require "lib.logic"
local world = require "lib.world"
local is_event = require "lib.is_event"

local config = require "config"

loop(function()
    log("started")

    local rc = redis.client(config.redis)

    wait(rc:connect())
    log("connected to redis", config.redis.ip4, config.redis.port)

    wait(rc:hello(config.redis))
    log("redis authorized", config.redis.client_name)

    local dc = {
        cmd_ids = {},
        config = config,
        rc = rc,
        world = world {
            rc = rc,
        },
    }

    log("world loaded", dc.world:stat())

    wait(rc:subscribe(config.id, function(push)
        local event = json.parse(push)

        if not is_event(event) then
            logic(dc, {
                type = "bad_push",
                from = config.id,
                time = time(),
                payload = { push = push },
            })
        else
            event.time = time()
            logic(dc, event)
        end
    end))
    log("redis subscribed", config.id)

    logic(dc, {
        type = "start",
        from = config.id,
        time = time(),
    })

    wait(rc:join())
end)
