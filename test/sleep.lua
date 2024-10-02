local perf = require "test.perf"
local trace = require "trace"
local sleep = require "sleep"
local async = require "async"
local wait = async.wait
local pwait = async.pwait

return function()
    perf()
        local t1 = sleep(0.02)
        local t2 = sleep(0.012)
        local t3 = coroutine.create(function(duration_s)
            return wait(sleep(duration_s))
        end)
        assert(coroutine.resume(t3, 0.03))
    perf("sleep prepare")

    perf()
        trace("t1:", pwait(t1))
        trace("t2:", pwait(t2))
        trace("t3:", pwait(t3))
    perf("sleep wait")
end
