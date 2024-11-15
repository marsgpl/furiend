local async = require "async"
local wait = async.wait

return function(...)
    local results = {}

    for i = 1, select("#", ...) do
        results[i] = wait((select(i, ...)))
    end

    return results
end
