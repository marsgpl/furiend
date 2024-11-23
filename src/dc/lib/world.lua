local promise = require "promise"
local waitall = require "waitall"
local key_prefix = require "lib.world.key_prefix"
local load_entities = require "lib.world.load_entities"
local link_classes = require "lib.world.link_classes"
local link_objects = require "lib.world.link_objects"
local check_id = require "lib.world.check_id"
local check_key_name = require "lib.world.check_key_name"
local check_obj_key = require "lib.world.check_obj_key"
local types = require "lib.world.types"
local error_kv = require "error_kv"
local json = require "json"
local redis = require "redis"
local tensor = require "tensor"
local async = require "async"
local wait = async.wait

local proto = {}
local mt = { __index = proto }

function proto:stat()
    local classes_n = 0
    local objects_n = 0

    for _ in pairs(self.classes) do
        classes_n = classes_n + 1
    end

    for _ in pairs(self.objects) do
        objects_n = objects_n + 1
    end

    return {
        classes_n = classes_n,
        objects_n = objects_n,
    }
end

function proto:load_classes()
    self.classes = load_entities(self.rc, key_prefix.class)
end

function proto:load_objects()
    self.objects = load_entities(self.rc, key_prefix.object)
end

function proto:create_object(class_id, blueprint)
    local objects = self.objects
    local class = self.classes[class_id]

    if not class then
        error_kv("class not found", {
            class_id = class_id,
            blueprint = blueprint,
        })
    end

    blueprint = blueprint or {}

    if type(blueprint) ~= "table" then
        error_kv("blueprint is not a table", {
            class_id = class_id,
            blueprint = blueprint,
        })
    end

    blueprint.class = class

    if blueprint.id then
        if objects[blueprint.id] then
            error_kv("object id already exists", {
                id = blueprint.id,
                blueprint = blueprint,
            })
        end
    else
        blueprint.id = tensor.gen_id(class_id, objects)
    end

    self:validate_object(blueprint)

    objects[blueprint.id] = blueprint

    return blueprint
end

function proto:validate_object(obj)
    for key in pairs(obj) do
        local ok, err = pcall(self.validate_object_key, self, obj, key)

        if not ok then
            error_kv("invalid object key: " .. err, {
                obj = obj,
                key = key,
            })
        end
    end
end

function proto:validate_object_key(obj, key)
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

    check_obj_key[schema.type](obj[key], self.objects, self.classes)
end

-- expire_s: nil or 0 = do not expire
function proto:save_object(object, expire_s)
    local record_key = key_prefix.object .. object.id

    local query = {
        "hset",
        record_key,
    }

    for key, value in pairs(object) do
        if key ~= "id" then
            table.insert(query, key)

            if key == "class" then
                table.insert(query, value.id)
            elseif type(value) ~= "string" then
                table.insert(query, json.stringify(value))
            else
                table.insert(query, value)
            end
        end
    end

    wait(self.rc:query(redis:pack(query)))

    if expire_s and expire_s > 0 then
        wait(self.rc:query(redis:pack {
            "expire",
            record_key,
            tostring(expire_s),
        }))
    end
end

return function(props)
    assert(type(props) == "table")

    local world = setmetatable(props, mt)

    for _, type_name in ipairs(types) do
        assert(check_obj_key[type_name],
            "check_obj_key validator missing for type: " .. type_name)
    end

    waitall(
        promise(world.load_classes, world),
        promise(world.load_objects, world)
    )

    link_classes(world.classes)
    link_objects(world.objects, world.classes)

    return world
end
