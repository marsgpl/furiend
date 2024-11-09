local async = require "async"
local wait = async.wait

return function(...)
    local results = {}
    local types = {}

    for i = 1, select("#", ...) do
        local result, type = wait((select(i, ...)))
        table.insert(results, result)
        table.insert(types, type)
    end

    return results, types
end
