local trace = require "trace"
local json = require "json"
local equal = require "equal"

local samples = {
    1, "1",
    "hi", "\"hi\"",
}

return function()
    for i = 1, #samples, 2 do
        local value = samples[i]
        local string = samples[i + 1]
        local parsed = json.parse(string)
        local stringified = json.stringify(value)

        trace("value:", value, "parsed:", parsed)
        trace("string:", string, "stringified:", stringified)

        assert(equal(value, parsed), "value != parsed")
        assert(equal(string, stringified), "string != stringified")
    end
end
