local promise = require "promise"
local waitall = require "waitall"
local key_prefix = require "lib.world.key_prefix"
local load_entities = require "lib.world.load_entities"
local link_classes = require "lib.world.link_classes"
local link_objects = require "lib.world.link_objects"

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

return function(props)
    assert(type(props) == "table")

    local world = setmetatable(props, mt)

    waitall(
        promise(world.load_classes, world),
        promise(world.load_objects, world)
    )

    link_classes(world.classes)
    link_objects(world.objects, world.classes)

    return world
end
