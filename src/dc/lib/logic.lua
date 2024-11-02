local async = require "async"
local wait = async.wait
local json = require "json"
local next_id = require "lib.next-id"
local log = require "log"

return function(dc, event)
    log("event", event)

    if event.from == "fe2" and event.type == "tg_event" then
        local cmd_id = next_id(dc.cmd_ids, event.from)

        wait(dc.rc:publish(event.from, json.stringify {
            from = dc.config.id,
            type = "cmd",
            cmd = "sendChatAction",
            cmd_id = cmd_id,
            body = {
                chat_id = event.event.message.chat.id,
                action = "typing",
            },
        }))
    end
end
