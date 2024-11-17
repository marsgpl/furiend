local fs = require "fs"
local redis = require "redis"
local async = require "async"
local wait = async.wait
local json_response = require "lib.json_response"
local load_entities = require "world.load_entities"
local types = require "world.types"
local key_prefix = require "world.key_prefix"

local function cmd_delete_key(req, res, mode)
    local session = req.session
    local rc, dc, body = session.rc, session.dc, session.parsed_body
    local id, key = tostring(body.id), tostring(body.key)

    wait(rc:query(redis:pack {
        "hdel",
        key_prefix[mode] .. id,
        key,
    }))

    dc("delete_key", {
        mode = mode,
        id = id,
        key = key,
    })

    json_response(res, {})
end

local function cmd_delete_entity(req, res, mode)
    local session = req.session
    local rc, dc, body = session.rc, session.dc, session.parsed_body
    local id = tostring(body.id)

    wait(rc:query(redis:pack {
        "del",
        key_prefix[mode] .. id,
    }))

    dc("delete_entity", {
        mode = mode,
        id = id,
    })

    json_response(res, {})
end

local function cmd_create_entity(req, res, mode)
    local session = req.session
    local rc, dc, entity = session.rc, session.dc, session.parsed_body
    local id = tostring(entity.id)

    local query = {
        "hset",
        key_prefix[mode] .. id,
    }

    for key, value in pairs(entity) do
        if key ~= "id" then
            table.insert(query, key)
            table.insert(query, tostring(value))
        end
    end

    wait(rc:query(redis:pack(query)))

    dc("create_entity", {
        mode = mode,
        entity = entity,
    })

    json_response(res, {})
end

local function cmd_rename_entity(req, res, mode)
    local session = req.session
    local rc, dc, body = session.rc, session.dc, session.parsed_body
    local old_id, new_id = tostring(body.id), tostring(body.value)

    wait(rc:query(redis:pack {
        "rename",
        key_prefix[mode] .. old_id,
        key_prefix[mode] .. new_id,
    }))

    dc("rename_entity", {
        mode = mode,
        old_id = old_id,
        new_id = new_id,
    })

    json_response(res, {})
end

local function cmd_set_key(req, res, mode)
    local session = req.session
    local rc, dc, body = session.rc, session.dc, session.parsed_body
    local id, key, new_key, value =
        tostring(body.id),
        tostring(body.key),
        tostring(body.new_key),
        tostring(body.value)

    if key == "id" then
        if key ~= new_key then
            error("id key cannot be renamed")
        end

        return cmd_rename_entity(req, res, mode)
    end

    wait(rc:query(redis:pack {
        "hset",
        key_prefix[mode] .. id,
        new_key,
        value,
    }))

    dc("set_key", {
        mode = mode,
        id = id,
        key = new_key,
        value = value,
    })

    if key ~= new_key then
        wait(rc:query(redis:pack {
            "hdel",
            key_prefix[mode] .. id,
            key,
        }))

        dc("delete_key", {
            reason = "rename",
            mode = mode,
            id = id,
            key = key,
        })
    end

    json_response(res, {})
end

local function cmd_get_entities(req, res)
    local rc = req.session.rc

    local classes = load_entities(rc, key_prefix.class)
    local objects = load_entities(rc, key_prefix.object)

    json_response(res, {
        classes = classes,
        objects = objects,
        types = types,
    })
end

local function serve_html(req, res, path)
    res:push_header("Content-Type", "text/html")
    res:set_body(fs.readfile(req.session.static .. path))
end

local function serve_svg(req, res, path)
    res:push_header("Content-Type", "image/svg+xml")
    res:set_body(fs.readfile(req.session.static .. path))
end

local function serve_css(req, res, path)
    res:push_header("Content-Type", "text/css")
    res:set_body(fs.readfile(req.session.static .. path))
end

local function serve_js(req, res, path)
    res:push_header("Content-Type", "text/javascript")
    res:set_body(fs.readfile(req.session.static .. path))
end

local function serve_woff2(req, res, path)
    res:push_header("Content-Type", "font/woff2")
    res:set_body(fs.readfile(req.session.static .. path))
end

return function(req, res)
    local method, path = req.method, req.path

    if method == "GET" then
        if path == "/" then
            return serve_html(req, res, "/index.html")
        elseif path:match("^/[%w_]+%.svg$") then
            return serve_svg(req, res, path)
        elseif path:match("^/[%w_]+%.css$") then
            return serve_css(req, res, path)
        elseif path:match("^/[%w_]+%.js$") then
            return serve_js(req, res, path)
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
