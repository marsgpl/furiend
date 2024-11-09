local error_kv = require "error_kv"

local expr = "^[A-Za-z0-9]+$"

return function(id)
    if not id
        or type(id) ~= "string"
        or not id:match("^[A-Za-z0-9]+$")
    then
        error_kv("invalid class.id format", {
            id = id,
            expected = expr,
        })
    end
end
