local equal = require "equal"

local function eq(from, to)
    for _, value in ipairs(to) do
        if not equal(from, value) then
            return false
        end
    end

    return true
end

local function neq(from, to)
    for _, value in ipairs(to) do
        if equal(from, value) then
            return false
        end
    end

    return true
end

local function gt(from, to)
    if type(from) ~= "number" then
        return false
    end

    for _, value in ipairs(to) do
        if from <= value then
            return false
        end
    end

    return true
end

local function gte(from, to)
    if type(from) ~= "number" then
        return false
    end

    for _, value in ipairs(to) do
        if from < value then
            return false
        end
    end

    return true
end

local function lt(from, to)
    if type(from) ~= "number" then
        return false
    end

    for _, value in ipairs(to) do
        if from >= value then
            return false
        end
    end

    return true
end

local function lte(from, to)
    if type(from) ~= "number" then
        return false
    end

    for _, value in ipairs(to) do
        if from > value then
            return false
        end
    end

    return true
end

local function inside(from, to)
    -- link_object converts "to" to set
    return not not to[from]
end

return {
    ["eq"] = eq,
    ["neq"] = neq,
    ["gt"] = gt,
    ["gte"] = gte,
    ["lt"] = lt,
    ["lte"] = lte,
    ["in"] = inside,
}
