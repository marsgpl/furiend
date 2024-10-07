package.path = "/furiend/?.lua;/furiend/?.luac"
package.cpath = "/furiend/src/lua-lib/?/?.so;/furiend/vendor/lua-cjson/?.so"

local perf = require "test.perf"
local cjson = require "cjson"
local json = require "json"

local string = [[
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

local value = json.parse(string)

-- trace(json.stringify(value))
-- trace(cjson.encode(value))
-- trace(json.parse(string))
-- trace(cjson.decode(string))

local reps = 250000

perf()
    for _ = 1, reps do
        assert(json.stringify(value))
    end
perf("stringify: yyjson")

perf()
    for _ = 1, reps do
        assert(cjson.encode(value))
    end
perf("stringify: cjson")

perf()
    for _ = 1, reps do
        assert(json.parse(string))
    end
perf("parse: yyjson")

perf()
    for _ = 1, reps do
        assert(cjson.decode(string))
    end
perf("parse: cjson")
