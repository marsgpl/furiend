return function(ids, key)
    local id = ids[key] or 1
    ids[key] = id + 1
    return id
end
