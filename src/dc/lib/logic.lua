local async = require "async"
local wait = async.wait
local json = require "json"
local next_id = require "lib.next_id"
local log = require "log"
local unwrap = require "unwrap"
local error_kv = require "error_kv"

return function(dc, event)
    log("event", event)

    if event.from == "fe2" and event.type == "tg_bot_event" then
        local cmd_id = next_id(dc.cmd_ids, event.from)
        local chat_id = unwrap(event, "payload.event.*.chat.id")

        if not chat_id then
            error_kv("unable to find chat id in tg event")
        end

        wait(dc.rc:publish(event.from, json.stringify {
            type = "cmd",
            from = dc.config.id,
            cmd = "sendChatAction",
            cmd_id = cmd_id,
            payload = {
                chat_id = chat_id,
                action = "typing",
            },
        }))
    end
end
