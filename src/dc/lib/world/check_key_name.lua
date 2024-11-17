local error_kv = require "error_kv"

local expr = "^[A-Za-z0-9_=-]+$"

return function(value)
    if type(value) ~= "string" or not value:match(expr) then
        error_kv("invalid key name", {
            key_name = value,
            expected = expr,
        })
    end
end
