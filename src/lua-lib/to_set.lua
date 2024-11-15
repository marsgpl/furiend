return function(array)
    local set = {}

    for _, v in ipairs(array) do
        set[v] = true
    end

    return set
end
