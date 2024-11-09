local async = require "async"
local wait = async.wait
local http = require "http"
local json = require "json"

-- https://core.telegram.org/bots/api

local proto = {}
local mt = { __index = proto }

function proto:get_updates()
    -- does not work if webhook was set
    return self:request("POST", "getUpdates", {
        timeout = 1,
        allowed_updates = self.config.hook.allowed_updates,
    })
end

function proto:get_me()
    return self:request("GET", "getMe")
    -- {"ok":true,"result":{"id":000,"is_bot":true,"first_name":"xxx","username":"xxx","can_join_groups":false,"can_read_all_group_messages":false,"supports_inline_queries":false,"can_connect_to_business":false,"has_main_web_app":false}}
end

function proto:get_webhook_info()
    return self:request("GET", "getWebhookInfo")
    -- {"ok":true,"result":{"url":"https://xxx","has_custom_certificate":false,"pending_update_count":0,"max_connections":40,"ip_address":"xx.xx.xx.xx","allowed_updates":["message","edited_message","message_reaction"]}}
end

function proto:set_webhook(config)
    -- {"url":"https://xxx","secret_token":"xxx","allowed_updates":{"message","edited_message","message_reaction"}}
    assert(type(config) == "table")
    return self:request("POST", "setWebhook", {
        url = config.url,
        secret_token = config.secret_token,
        allowed_updates = config.allowed_updates,
    })
    -- {"ok":true,"result":true,"description":"Webhook was set"}
    -- {"ok":true,"result":true,"description":"Webhook is already set"}
    -- {"ok":false,"error_code":400,"description":"Bad Request: bad webhook: Failed to resolve host: No address associated with hostname"}
end

function proto:request(method, path, body)
    assert(type(method) == "string")
    assert(type(path) == "string")

    if not self.ip4 then
        local ips = wait(self.config.dns_client:resolve {
            name = self.config.host,
        })
        self.ip4 = assert(ips[1])
    end

    local conf = {
        https = true,
        https_verify_cert = false,
        ip4 = self.ip4,
        method = method,
        host = self.config.host,
        path = "/bot" .. self.config.token .. "/" .. path,
    }

    if body then
        assert(type(body) == "table")
        conf.body = json.stringify(body)
        conf.content_type = "application/json"
    end

    local res = wait(http.request(conf))

    local response = json.parse(res.body)

    -- print(res.body)
    -- {"ok":false,"error_code":404,"description":"Not Found"}
    -- {"ok":false,"error_code":400,"description":"Bad Request: ? is empty"}

    if type(response) ~= "table" or not response.ok then
        error("request failed; response: " .. res.body)
    end

    return response.result
end

return function(config)
    -- {"host":"api.telegram.org","token":"xxx","dns_client":xxx}
    assert(type(config) == "table")

    return setmetatable({
        ip4 = nil,
        config = config,
    }, mt)
end
