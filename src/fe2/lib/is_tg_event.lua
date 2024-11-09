return function(event)
    local type = type
    local message = type(event) == "table" and event.message

    return type(message) == "table"
        and type(message.message_id) == "number"
        and type(message.chat) == "table"
        and type(message.chat.id) == "number"
        and type(message.from) == "table"
        and type(message.from.id) == "number"
        and type(event.update_id) == "number"
end
