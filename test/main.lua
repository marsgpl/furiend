warn "@on"

package.path = "/furiend/?.lua;/furiend/src/lua-lib/?.lua"
package.cpath = "/furiend/src/lua-clib/?/?.so"

local async = require "async"
local loop = async.loop

loop(function()
    require "test.url" ()
    require "test.sha2" ()
    require "test.equal" ()
    require "test.json" ()
    require "test.sleep" ()
    require "test.resp" ()
    require "test.redis" ()
    require "test.dns" ()
    require "test.http" ()
    require "test.json-perf" ()
end)
