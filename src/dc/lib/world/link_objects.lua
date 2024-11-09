local json = require "json"
local is_array = require "is_array"
local error_kv = require "error_kv"
local check_object_id = require "lib.world.check_object_id"

local function link_key(key, obj, class, objects)
    local key_type = class[key]
    local value = obj[key]

    if not value then
        error_kv("empty value", {
            object = obj.id,
            key = key,
        })
    end

    if not key_type then
        error_kv("object.key not defined in class", {
            object = obj.id,
            key = key,
            class = class.id,
        })
    elseif type(key_type) == "table" then -- rel
        local rel_obj_id = value
        local rel_obj = objects[rel_obj_id]

        if not rel_obj then
            error_kv("object rel not found", {
                object = obj.id,
                key = key,
                rel = rel_obj_id,
                rel_class = key_type.id,
            })
        elseif rel_obj.class ~= key_type.id then
            error_kv("object rel class mismatch", {
                object = obj.id,
                key = key,
                rel = rel_obj_id,
                rel_class = rel_obj.class,
                expected_rel_class = key_type.id,
            })
        end

        obj[key] = rel_obj
    elseif key_type == "integer" then
        local integer, err = math.tointeger(value)
        obj[key] = integer

        if err then
            error_kv("object.key: invalid integer", {
                object = obj.id,
                key = key,
                value = value,
            })
        end
    elseif key_type == "float" then
        local float, err = tonumber(value, 10)
        obj[key] = float

        if err then
            error_kv("object.key: invalid float", {
                object = obj.id,
                key = key,
                value = value,
            })
        end
    elseif key_type == "string" then
        -- already string
    elseif key_type == "object" then
        obj[key] = json.parse(value)

        if type(obj[key]) ~= "table" then
            error_kv("object.key: invalid object", {
                object = obj.id,
                key = key,
                value = value,
            })
        end
    elseif key_type == "array" then
        obj[key] = json.parse(obj[key])

        if not is_array(obj[key]) then
            error_kv("object.key: invalid array", {
                object = obj.id,
                key = key,
                value = value,
            })
        end
    elseif key_type == "boolean" then
        obj[key] = value == "true"

        if value ~= "true" and value ~= "false" then
            error_kv("object.key: invalid boolean", {
                object = obj.id,
                key = key,
                value = value,
            })
        end
    else
        error_kv("object.key: unsupported type", {
            object = obj.id,
            class = obj.class,
            key = key,
            type = key_type,
        })
    end
end

return function(objects, classes)
    for id, obj in pairs(objects) do
        obj.id = id

        check_object_id(id)

        local class = classes[obj.class]

        if not class then
            error_kv("object class not found", {
                object = obj,
                class = obj.class,
            })
        end

        for key in pairs(obj) do
            if key ~= "id" and key ~= "class" then
                link_key(key, obj, class, objects)
            end
        end
    end
end
