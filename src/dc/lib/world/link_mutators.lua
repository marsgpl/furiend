local error_kv = require "error_kv"
local array = require "array"
local ops = require "lib.world.ops"
local check_key_name = require "lib.world.check_key_name"

local function check_warp(mut, warp)
    local ok, err = pcall(check_key_name, warp.to)

    if not ok then
        error_kv("warp.to must be a flat key: " .. err, {
            mutator = mut,
            warp = warp,
        })
    end

    if not mut.to[warp.to] then
        error_kv("warp.to not found in mut.to", {
            mutator = mut,
            warp = warp,
        })
    end
end

local function check_if(mut, ifx)
    local op, vals = ifx.op, ifx.vals

    if not ops[op] then
        error_kv("bad if.op", {
            mutator = mut,
            ["if"] = ifx,
        })
    end

    if op ~= "in" or not array.is_array(vals) or #vals == 0 then
        error_kv("bad if.vals", {
            mutator = mut,
            ["if"] = ifx,
        })
    end
end

local function check_mutator(mut)
    local from, to, ifs, warps = mut.from, mut.to, mut.ifs, mut.warps

    if from == to then
        error_kv("from == to", { mutator = mut })
    end

    if not from then
        error_kv("no from", { mutator = mut })
    end

    if not to then
        error_kv("no to", { mutator = mut })
    end

    if not ifs then
        error_kv("no ifs", { mutator = mut })
    end

    if not warps then
        error_kv("no warps", { mutator = mut })
    end

    for _, ifx in ipairs(ifs) do
        check_if(mut, ifx)
    end

    for _, warp in ipairs(warps) do
        check_warp(mut, warp)
    end
end

return function(objects)
    local mutators = {}

    for _, obj in pairs(objects) do
        if obj.class.id ~= "mut" then
            return
        end

        check_mutator(obj)

        local from_id = obj.from.id
        local list = mutators[from_id]

        if not list then
            list = {}
            mutators[from_id] = list
        end

        table.insert(list, obj)
    end

    return mutators
end
