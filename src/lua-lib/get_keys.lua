return function(object)
    local keys = {}

    for key in pairs(object) do
        table.insert(keys, key)
    end

    return keys
end
