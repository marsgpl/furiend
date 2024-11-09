local error_kv = require "error_kv"
local get_keys = require "get_keys"
local class_key_types = require "lib.world.class_key_types"
local check_class_id = require "lib.world.check_class_id"

local function link_key(key, class, classes)
    local sep_pos = key:find(":", 1, true)

    if not sep_pos then -- simple type
        local key_type = class[key]

        if not class_key_types[key_type] then
            error_kv("invalid class.key type", {
                class = class.id,
                key = key,
                type = key_type,
                expected = get_keys(class_key_types),
            })
        end

        return
    end

    local prefix = key:sub(1, sep_pos - 1)
    local suffix = key:sub(sep_pos + 1)

    if #suffix == 0 then
        error_kv("empty class.key suffix", {
            class = class.id,
            key = key,
        })
    end

    if prefix == "rel" then -- class relation
        local rel_class_id = class[key]
        local rel_class = classes[rel_class_id]

        if not rel_class then
            error_kv("class rel not found", {
                class = class.id,
                key = key,
                id = rel_class_id,
            })
        end

        class[suffix] = rel_class
        class[key] = nil

        return
    end

    error_kv("invalid class.key prefix", {
        class = class.id,
        key = key,
        prefix = prefix,
        expected = { "rel" },
    })
end

return function(classes)
    for id, class in pairs(classes) do
        class.id = id

        check_class_id(id)

        for _, key in ipairs(get_keys(class)) do
            if key ~= "id" then
                link_key(key, class, classes)
            end
        end
    end
end
