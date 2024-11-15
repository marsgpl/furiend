return function(push, config)
    local type = type

    return type(push) == "table"
        and push.type == "cmd"
        and push.from == config.dc
        and type(push.cmd) == "string"
        and type(push.cmd_id) == "number"
end
