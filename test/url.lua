local trace = require "trace"
local url = require "url"

return function()
    local src = "https://admin:123@sub.domain.com:1234/path/to?a=b&c=dd#D&G"
    local parsed = url(src)

    trace(src, parsed)

    assert(parsed.protocol == "https", "protocol parse failed")
    assert(parsed.username == "admin", "username parse failed")
    assert(parsed.password == "123", "password parse failed")
    assert(parsed.host == "sub.domain.com", "host parse failed")
    assert(parsed.port == 1234, "port parse failed")
    assert(parsed.path == "/path/to", "path parse failed")
    assert(parsed.query == "a=b&c=dd", "query parse failed")
    assert(parsed.hash == "D&G", "hash parse failed")

    src = "http://[2001:db8::1]"
    parsed = url(src)

    trace(src, parsed)

    assert(parsed.protocol == "http", "protocol parse failed")
    assert(parsed.username == nil, "username parse failed")
    assert(parsed.password == nil, "password parse failed")
    assert(parsed.host == "2001:db8::1", "host parse failed")
    assert(parsed.port == nil, "port parse failed")
    assert(parsed.path == nil, "path parse failed")
    assert(parsed.query == nil, "query parse failed")
    assert(parsed.hash == nil, "hash parse failed")

    src = "http://admin:123@[2001:db8::1]:80"
    parsed = url(src)

    trace(src, parsed)

    assert(parsed.protocol == "http", "protocol parse failed")
    assert(parsed.username == "admin", "username parse failed")
    assert(parsed.password == "123", "password parse failed")
    assert(parsed.host == "2001:db8::1", "host parse failed")
    assert(parsed.port == 80, "port parse failed")
    assert(parsed.path == nil, "path parse failed")
    assert(parsed.query == nil, "query parse failed")
    assert(parsed.hash == nil, "hash parse failed")
end
