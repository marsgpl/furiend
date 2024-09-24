local async = require "async"
local sleep = require "sleep"
local time = require "time"
local wait = async.wait

return function()
    local ts1 = time()

    local t1 = sleep(.5)
    local t2 = sleep(.7)
    local t3 = coroutine.create(function(duration_s)
        return wait(sleep(duration_s))
    end)

    assert(coroutine.resume(t3, 1))

    wait(t1)
    wait(t2)
    wait(t3)

    local ts2 = time()

    print("ts start:", ts1)
    print("ts finish:", ts2)
    print("duration:", ts2 - ts1, "s")
end
