return function(event)
    local type = type

    return type(event) == "table"
        and type(event.type) == "string"
        and type(event.from) == "string"
end
