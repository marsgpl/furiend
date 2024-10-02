local time = require "time"

local ts1

return function(label)
    if label then
        local ts2 = time()
        print(label, string.format("%.5f", ts2 - ts1) .. "s")
    else
        ts1 = time()
    end
end
