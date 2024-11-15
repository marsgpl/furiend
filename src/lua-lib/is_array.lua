return function(value)
    if type(value) ~= "table" then
        return false
    end

    local next_idx = 1

    for idx in pairs(value) do
        if math.type(idx) ~= "integer" or idx ~= next_idx then
            return false
        end

        next_idx = next_idx + 1
    end

    return true
end
