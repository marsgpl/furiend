local equal = require "equal"

local function eq(from, to)
    return equal(from, to)
end

local function neq(from, to)
    return not equal(from, to)
end

local function gt(from, to)
    return from > to
end

local function gte(from, to)
    return from >= to
end

local function lt(from, to)
    return from < to
end

local function lte(from, to)
    return from <= to
end

local function in_(from, to)
    return not not to[from]
end

return {
    ["eq"] = eq,
    ["neq"] = neq,
    ["gt"] = gt,
    ["gte"] = gte,
    ["lt"] = lt,
    ["lte"] = lte,
    ["in"] = in_,
}
