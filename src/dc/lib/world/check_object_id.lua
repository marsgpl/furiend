local error_kv = require "error_kv"

local expr = "^[a-z0-9_]+$"

return function(id)
    if not id
        or type(id) ~= "string"
        or not id:match("^[a-z0-9_]+$")
    then
        error_kv("invalid object.id format", {
            id = id,
            expected = expr,
        })
    end
end
