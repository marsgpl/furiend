local json = require "json"

return function(error_msg, map)
    local chunks = {
        tostring(error_msg),
    }

    for key, value in pairs(map) do
        if type(value) == "table" then
            value = json.stringify(value)
        end

        table.insert(chunks, tostring(key) .. ": " .. tostring(value))
    end

    error(table.concat(chunks, "; "))
end
