local fs = require "fs"
local redis = require "redis"
local trim = require "trim"
local error_kv = require "error_kv"
local get_keys = require "get_keys"
local json_response = require "lib.json_response"
local load_entities = require "world.load_entities"
local check_class_id = require "world.check_class_id"
local check_object_id = require "world.check_object_id"
local class_key_types = require "world.class_key_types"
local async = require "async"
local wait = async.wait

local function check_kv(key, value)
    local trimmed_key = trim(key)
    local trimmed_value = trim(value)

    if #trimmed_key == 0 then
        error("empty key")
    elseif trimmed_key ~= key then
        error("non-trimmed key: " .. key)
    elseif #trimmed_value == 0 then
        error_kv("empty value", { key = key })
    elseif trimmed_value ~= value then
        error_kv("non-trimmed value", { key = key, value = value })
    end
end

local function cmd_get_entities(req, res)
    local rc = req.session.rc

    local classes = load_entities(rc, "class:")
    local objects = load_entities(rc, "object:")

    json_response(res, {
        classes = classes,
        objects = objects,
        class_key_types = get_keys(class_key_types),
    })
end

local function cmd_create_entity(req, res, ent_type)
    local session = req.session
    local rc, dc, entity = session.rc, session.dc, session.parsed_body
    local id = entity.id

    if ent_type == "class" then
        check_class_id(id)
    else
        check_object_id(id)
    end

    local query = {
        "hset",
        ent_type .. ":" .. id,
    }

    for key, value in pairs(entity) do
        check_kv(key, value)

        if key ~= "id" then
            table.insert(query, key)
            table.insert(query, value)
        end
    end

    wait(rc:query(redis:pack(query)))

    dc(ent_type .. "_new", { [ent_type] = entity })

    json_response(res, {
        [ent_type] = entity,
    })
end

local function cmd_delete_entity(req, res, ent_type)
    local session = req.session
    local rc, dc, body = session.rc, session.dc, session.parsed_body
    local id = body.id

    wait(rc:query(redis:pack {
        "del",
        ent_type .. ":" .. id,
    }))

    dc(ent_type .. "_del", { id = id })

    json_response(res, {})
end

local function cmd_set_key(req, res, ent_type)
    local session = req.session
    local rc, dc, entity = session.rc, session.dc, session.parsed_body
    local id, key, value = entity.id, entity.key, entity.value

    if ent_type == "class" then
        check_class_id(id)
    else
        check_object_id(id)
    end

    check_kv(key, value)

    wait(rc:query(redis:pack {
        "hset",
        ent_type .. ":" .. id,
        key,
        value,
    }))

    dc(ent_type .. "_key_set", {
        id = id,
        key = key,
        value = value,
    })

    json_response(res, {})
end

local function cmd_delete_key(req, res, ent_type)
    local session = req.session
    local rc, dc, entity = session.rc, session.dc, session.parsed_body
    local id, key = entity.id, entity.key

    wait(rc:query(redis:pack {
        "hdel",
        ent_type .. ":" .. id,
        key,
    }))

    dc(ent_type .. "_key_del", {
        id = id,
        key = key,
    })

    json_response(res, {})
end

local function serve_html(req, res)
    res:push_header("Content-Type", "text/html")
    res:set_body(fs.readfile(req.session.static .. "/index.html"))
end

local function serve_logo(req, res)
    res:push_header("Content-Type", "image/svg+xml")
    res:set_body(fs.readfile(req.session.static .. "/logo.svg"))
end

local function serve_css(req, res)
    res:push_header("Content-Type", "text/css")
    res:set_body(fs.readfile(req.session.static .. "/index.css"))
end

local function serve_js(req, res)
    res:push_header("Content-Type", "text/javascript")
    res:set_body(fs.readfile(req.session.static .. "/index.js"))
end

return function(req, res)
    local method, path = req.method, req.path

    if method == "GET" then
        if path == "/" then
            return serve_html(req, res)
        elseif path == "/logo.svg" then
            return serve_logo(req, res)
        elseif path == "/index.css" then
            return serve_css(req, res)
        elseif path == "/index.js" then
            return serve_js(req, res)
        elseif path == "/entities" then
            return cmd_get_entities(req, res)
        end
    elseif method == "PUT" then
        if path == "/object" then
            return cmd_create_entity(req, res, "object")
        elseif path == "/class" then
            return cmd_create_entity(req, res, "class")
        elseif path == "/object/key" then
            return cmd_set_key(req, res, "object")
        elseif path == "/class/key" then
            return cmd_set_key(req, res, "class")
        end
    elseif method == "DELETE" then
        if path == "/object" then
            return cmd_delete_entity(req, res, "object")
        elseif path == "/class" then
            return cmd_delete_entity(req, res, "class")
        elseif path == "/object/key" then
            return cmd_delete_key(req, res, "object")
        elseif path == "/class/key" then
            return cmd_delete_key(req, res, "class")
        end
    end

    res:set_status(404)
    json_response(res, {
        error = "endpoint not found: " .. method .. " " .. path,
    })
end
