local json = require "json"
local error_kv = require "error_kv"
local to_set = require "to_set"
local check_id = require "lib.world.check_id"
local check_key_name = require "lib.world.check_key_name"
local types = require "lib.world.types"

local types_set = to_set(types)

local function link_key(class, key, classes)
    if key == "id" then
        return check_id(class.id)
    end

    check_key_name(key)

    local schema = json.parse(class[key])
    assert(type(schema) == "table", "schema is not a table")

    local schema_type = schema.type
    assert(types_set[schema_type], "schema type not found")

    if schema_type == "rel" or schema_type == "rels" then
        local rel_class = classes[schema.class]
        assert(rel_class, "rel class not found")
        schema.class = rel_class
    end

    class[key] = schema
end

return function(classes)
    for id, class in pairs(classes) do
        class.id = id

        for key in pairs(class) do
            local ok, err = pcall(link_key, class, key, classes)

            if not ok then
                error_kv("class key link failed: " .. err, {
                    class_id = id,
                    key = key,
                    value = class[key],
                })
            end
        end
    end
end
