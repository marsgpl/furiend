local json = require "json"

return function(error_message, key_value_data)
    local chunks = {
        tostring(error_message),
    }

    for key, value in pairs(key_value_data) do
        if type(value) == "table" then
            value = json.stringify(value)
        end

        table.insert(chunks, tostring(key) .. ": " .. tostring(value))
    end

    error(table.concat(chunks, "; "))
end
