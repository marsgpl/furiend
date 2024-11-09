local async = require "async"
local wait = async.wait

return function(redis_client, keys_prefix)
    local keys = wait(redis_client:query("keys " .. keys_prefix .. "*\r\n"))
    local prefix_sub = 1 + #keys_prefix
    local entities = {}

    for _, key in ipairs(keys) do
        local id = key:sub(prefix_sub)
        entities[id] = redis_client:query("hgetall " .. key .. "\r\n")
    end

    for id, query in pairs(entities) do
        entities[id] = wait(query)
    end

    return entities
end
