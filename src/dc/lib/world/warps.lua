local function bool(value)
    return value and true or false
end

local function int(value)
    return math.tointeger(value) or 0
end

local function float(value)
    return tonumber(value) or 0
end

local function str(value)
    return tostring(value or "")
end

local function tbl(value)
    if type(value) == "table" then
        return value
    else
        return { value }
    end
end

local function rel(value, mut, warp, world)
    warp.
end

local function rels(value, mut, warp, world)
end

local function class(value, mut, warp, world)
end

return {
    ["bool"] = bool,
    ["int"] = int,
    ["float"] = float,
    ["str"] = str,
    ["table"] = tbl,
    ["rel"] = rel,
    ["rels"] = rels,
    ["class"] = class,
}
