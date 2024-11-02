package.cpath = "/furiend/src/lua-clib/?/?.so;/usr/local/lib/lua/5.4/?.so"

print "how to install lua cjson: luarocks-5.4 install lua-cjson"

local perf = require "test.perf"
local cjson = require "cjson"
local json = require "json"

local test_json = [[
{
    "glossary": {
        "title": "example glossary",
        "GlossDiv": {
            "title": "S",
            "GlossList": {
                "GlossEntry": {
                    "ID": "SGML",
                    "SortAs": "SGML",
                    "GlossTerm": "Standard Generalized Markup Language",
                    "Acronym": "SGML",
                    "Abbrev": "ISO 8879:1986",
                    "GlossDef": {
                        "para": "A meta-markup language, used to create markup languages such as DocBook.",
                        "GlossSeeAlso": ["GML", "XML"]
                    },
                    "GlossSee": "markup"
                }
            }
        }
    }
}
]]

return function()
    local string = test_json
    local value = json.parse(string)

    local reps = 250000
    local json_stringify = json.stringify
    local json_parse = json.parse
    local cjson_encode = cjson.encode
    local cjson_decode = cjson.decode

    perf()
        for _ = 1, reps do
            json_stringify(value)
        end
    perf("stringify: yyjson")

    perf()
        for _ = 1, reps do
            cjson_encode(value)
        end
    perf("stringify: cjson")

    perf()
        for _ = 1, reps do
            json_parse(string)
        end
    perf("parse: yyjson")

    perf()
        for _ = 1, reps do
            cjson_decode(string)
        end
    perf("parse: cjson")
end
