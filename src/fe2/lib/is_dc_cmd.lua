return function(push)
    local type = type
    local body = push.body

    return push.from == "dc"
        and push.type == "cmd"
        and type(push.cmd) == "string"
        and type(push.cmd_id) == "number"
        and (not body or type(body) == "table")
end
