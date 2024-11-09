return function(fn, ...)
    local t = coroutine.create(fn)
    coroutine.resume(t, ...)
    return t
end
