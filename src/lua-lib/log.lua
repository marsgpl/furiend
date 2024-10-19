local time = require "time"
local trace = require "trace"

local function log(msg, ...)
    assert(type(msg) == "string")

    local date = string.format("%.3f", time())
    local sep = select("#", ...) > 0 and ": " or "\n"

    io.stderr:write(date .. "  " .. msg .. sep):flush()
    trace(...)
end

return log
