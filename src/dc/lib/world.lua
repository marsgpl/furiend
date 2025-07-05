local promise = require "promise"
local waitall = require "waitall"
local key_prefix = require "lib.world.key_prefix"
local load_entities = require "lib.world.load_entities"
local link_classes = require "lib.world.link_classes"
local link_objects = require "lib.world.link_objects"
local link_mutators = require "lib.world.link_mutators"
local check_id = require "lib.world.check_id"
local check_key_name = require "lib.world.check_key_name"
local check_obj_key = require "lib.world.check_obj_key"
local ops = require "lib.world.ops"
local warps = require "lib.world.warps"
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
    local mutators_n = 0

    for _ in pairs(self.classes) do
        classes_n = classes_n + 1
    end

    for obj in pairs(self.objects) do
        if obj.class.id == "mut" then
            mutators_n = mutators_n + 1
        else
            objects_n = objects_n + 1
        end
    end

    return {
        classes_n = classes_n,
        objects_n = objects_n,
        mutators_n = mutators_n,
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
        error_kv("class not found by id", {
            class_id = class_id,
            blueprint = blueprint,
        })
    end

    blueprint = blueprint or {}
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
    local validator = self.validate_object_key

    for key in pairs(obj) do
        local ok, err = pcall(validator, self, obj, key)

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

function proto:mutate_any(obj)
    local mutators = self.mutators[obj.class.id]

    if not mutators then
        return
    end

    for _, mut in ipairs(mutators) do
        local mut_obj = self:mutate(obj, mut)

        if mut_obj then
            return mut_obj
        end
    end
end

function proto:mutate(obj, mut)
    for _, ifx in ipairs(mut.ifs) do
        local from = tensor.unwrap(obj, ifx.path)
        local to = ifx.values

        if not ops[ifx.op](from, to) then
            return
        end
    end

    local class = mut.to
    local blueprint = {}

    for _, warp in ipairs(mut.warps) do
        local value = tensor.unwrap(obj, warp.from)
        local to = warp.to
        local schema = class[to]

        blueprint[to] = warps[schema.type](value, mut, warp, self)
    end

    return self:create_object(class.id, blueprint)
end

return function(props)
    assert(type(props) == "table")

    local world = setmetatable(props, mt)

    for _, field_type in ipairs(types) do
        assert(check_obj_key[field_type],
            "obj key validator missing for type: " .. field_type)

        assert(warps[field_type],
            "warp missing for type: " .. field_type)
    end

    waitall(
        promise(world.load_classes, world),
        promise(world.load_objects, world)
    )

    link_classes(world.classes)
    link_objects(world.objects, world.classes)
    world.mutators = link_mutators(world.objects)

    return world
end
