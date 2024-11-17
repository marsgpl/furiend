local is_array = require "is_array"

local function bool(value)
    assert(type(value) == "boolean", "invalid bool")
end

local function int(value)
    assert(math.type(value) == "integer", "invalid int")
end

local function float(value)
    assert(math.type(value) == "float", "invalid float")
end

local function str(value)
    assert(type(value) == "string", "invalid str")
end

local function table(value)
    assert(type(value) == "table", "invalid table")
end

local function rel(value, objects)
    assert(type(value) == "table", "rel: invalid table")
    assert(value == objects[value.id], "rel: not found")
end

local function rels(value, objects)
    assert(is_array(value), "rels: invalid array")

    for _, rel_obj in ipairs(value) do
        rel(rel_obj, objects)
    end
end

local function class(value, _, classes)
    assert(type(value) == "table", "class: invalid table")
    assert(value == classes[value.id], "class: not found")
end

return {
    bool = bool,
    int = int,
    float = float,
    str = str,
    table = table,
    rel = rel,
    rels = rels,
    class = class,
}
