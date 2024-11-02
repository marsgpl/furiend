warn "@on"

package.path = "/furiend/src/lua-lib/?.lua;/furiend/src/dc/?.lua"
package.cpath = "/furiend/src/lua-clib/?/?.so"

local async = require "async"
local loop, wait = async.loop, async.wait
local redis = require "redis"
local log = require "log"
local time = require "time"
local json = require "json"
local logic = require "lib.logic"

local config = require "config"

loop(function()
    local rc = redis.client(config.redis)

    local dc = {
        cmd_ids = {},
        config = config,
        rc = rc,
    }

    wait(rc:connect())
    log("connected to redis", config.redis.ip4, config.redis.port)

    wait(rc:hello(config.redis))
    log("redis authorized", config.redis.client_name)

    wait(rc:subscribe(config.id, function(push)
        local event = json.parse(push)
        event.time = time()
        logic(dc, event)
    end))
    log("redis subscribed", config.id)

    logic(dc, {
        from = config.id,
        type = "start",
        time = time(),
    })

    wait(rc:join())
end)
