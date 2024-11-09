local async = require "async"
local wait = async.wait
local json = require "json"
local next_id = require "lib.next_id"
local log = require "log"
local unwrap = require "unwrap"

return function(dc, event)
    log("event", event)

    -- event is of type class:EntourageEvent
    -- classify event further

    -- event is of type class:TelegramBotEvent:
        -- event.type == "tg_event"
        -- event.from == "fe2"

    -- could not classify event:
        -- trigger "unknown event" logic

    if event.from == "fe2" and event.type == "tg_bot_event" then
        local cmd_id = next_id(dc.cmd_ids, event.from)
        local chat_id = unwrap(event, "payload.event.*.chat.id")

        if not chat_id then
            wait(dc.rc:publish(event.from, json.stringify {
                from = dc.config.id,
                type = "cmd",
                cmd = "sendChatAction",
                cmd_id = cmd_id,
                body = {
                    chat_id = chat_id,
                    action = "typing",
                },
            }))
        else
            log("unable to find chat id in tg event")
        end
    end
end
