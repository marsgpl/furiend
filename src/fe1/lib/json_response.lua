local json = require "json"

return function(res, data)
    res:push_header("Content-Type", "application/json")
    res:set_body(json.stringify(data))
end
