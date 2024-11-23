local json = require "json"
local error_kv = require "error_kv"
local is_array = require "is_array"
local check_id = require "lib.world.check_id"
local check_key_name = require "lib.world.check_key_name"
local check_mutator = require "lib.world.check_mutator"
local to_set = require "to_set"

local linkers

local function link_key(obj, key, objects, classes)
    if key == "id" then
        return check_id(obj.id)
    end

    if key == "class" then
        assert(obj.class, "object class not found")
        return check_id(obj.class.id)
    end

    check_key_name(key)

    local schema = obj.class[key]
    assert(schema, "key not found in class definition")

    local linker = linkers[schema.type]
    assert(linker, "linker not found for the key type")

    obj[key] = linker(obj[key], schema, objects, classes)
end

linkers = {
    bool = function(value)
        if value == "true" then
            return true
        elseif value == "false" then
            return false
        else
            error("invalid bool")
        end
    end,
    int = function(value)
        return assert(math.tointeger(value), "invalid int")
    end,
    float = function(value)
        return assert(tonumber(value), "invalid float")
    end,
    str = function(value)
        return value -- already string
    end,
    table = function(value)
        value = json.parse(value)
        assert(type(value) == "table", "invalid table")
        return value
    end,
    rel = function(rel_object_id, schema, objects)
        local rel_object = objects[rel_object_id]
        assert(rel_object, "rel object not found")

        local expected_class_id = schema.class.id
        local rel_class_id = rel_object.class.id or rel_object.class
        assert(expected_class_id == rel_class_id, "rel class mismatch")

        return rel_object
    end,
    rels = function(value, schema, objects)
        local ids = json.parse(value)
        assert(is_array(ids), "invalid array")

        for i, rel_object_id in ipairs(ids) do
            ids[i] = linkers.rel(rel_object_id, schema, objects)
        end

        return ids
    end,
    class = function(rel_class_id, _, _, classes)
        local rel_class = classes[rel_class_id]
        assert(rel_class, "rel class not found")
        return rel_class
    end,
}

return function(objects, classes, mutators)
    for id, obj in pairs(objects) do
        local class_id = obj.class
        local class = classes[class_id]

        obj.id = id
        obj.class = class

        for key in pairs(obj) do
            local ok, err = pcall(link_key, obj, key, objects, classes)

            if not ok then
                error_kv("object key link failed: " .. err, {
                    object_id = id,
                    object_class_id = class_id,
                    key = key,
                    value = obj[key],
                })
            end
        end

        if class_id == "mutator" then
            check_mutator(obj)

            local from_id = obj.from.id
            local list = mutators[from_id]

            if not list then
                list = {}
                mutators[from_id] = list
            end

            table.insert(list, obj)
        end

        if class_id == "if" and obj.op == "in" then
            obj.values = to_set(obj.values)
        end
    end
end
