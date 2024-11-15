local redis = require "redis"
local async = require "async"
local wait = async.wait

return function(redis_client, prefix)
    local keys = wait(redis_client:query("keys " .. prefix .. "*\r\n"))
    local prefix_sub = 1 + #prefix
    local entities = {}

    for _, key in ipairs(keys) do
        local id = key:sub(prefix_sub)
        entities[id] = redis_client:query(redis:pack { "hgetall", key })
    end

    for id, query in pairs(entities) do
        entities[id] = wait(query)
    end

    return entities
end
