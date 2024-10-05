local trace = require "trace"
local json = require "json"
local equal = require "equal"

local circular_obj = {}
circular_obj.self = circular_obj

local circular_arr = {1,2}
table.insert(circular_arr, circular_arr)

local samples = {
    { nil, "null" },
    { true, "true" },
    { false, "false" },
    { "", '""' },
    { "\x7f", '"\x7f"' }, -- yyjson does not encode \x7f as \\u007f
    { math.tointeger(1), "1" },
    { 1.000, "1.0" },
    { 1.1, "1.1" },
    { -12.34e-5, "-0.0001234" },
    { math.maxinteger, "9223372036854775807" },
    { math.mininteger, "-9223372036854775808" },
    { math.pi, "3.141592653589793" },
    { 0, "0" },
    { 0.0, "0.0" },
    { -0, "0" },
    { 0/-math.huge, "-0.0" },
    { 1e+28, "1e28" },
    { 1e-28, "1e-28" },
    { {}, "{}" },
    { {1}, "[1]" },
    { {[""]=1}, '{"":1}' },
    { {["\x00hi"]="\x01"}, '{"\\u0000hi":"\\u0001"}' },
    { circular_obj, '{"self":{}}' },
    { circular_arr, '[1,2,[]]' },
    { {["✅"]=1}, '{"✅":1}' },
    { {["-3.4e+20"]=1}, '{"-3.4e+20":1}' },
    { {["3.4e-20"]=1}, '{"3.4e-20":1}' },
    {
        {1,2,3,{4,5,6,{7,8,9}}},
        "[1,2,3,[4,5,6,[7,8,9]]]",
    },
    {
        { a = 1, b = {} },
        '{"a":1,"b":{}}',
        '{"b":{},"a":1}',
    },
    {
        {"\x00\b\f\n\r\t\"\\"},
        '["\\u0000\\b\\f\\n\\r\\t\\"\\\\"]',
    },
    {
        {["\x00oh\nno"]={k2={json.null,true,{"\x33",123,{4,5,6}},false,nil}}},
        '{"\\u0000oh\\nno":{"k2":[null,true,["3",123,[4,5,6]],false]}}',
    },
    {
        {{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
        {{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
        {{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
        }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
        }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
        }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}},
        '[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[' ..
        '[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[' ..
        '[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{' ..
        '}]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]' ..
        ']]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]' ..
        ']]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]',
    },
}

local fails = {
    math.huge, -- inf
    -1/0, -- -inf
    0/0, -- nan
    print, -- cfunction
    function() end, -- function
    io.stdout, -- userdata
    debug.getregistry()._CLIBS[1], -- lightuserdata
    coroutine.create(print), -- thread
    {[true]=1},
    {[false]=1},
    {[{}]=1},
    {[213]="aaa"},
    {[math.huge]=1},
    {[-math.huge]=1},
    {[math.maxinteger]=1},
    {[math.mininteger]=1},
    {[print]=1},
    {[function() end]=1},
    {[debug.getregistry()._CLIBS[1]]=1},
    {[io.stdout]=1},
    {[coroutine.create(print)]=1},
}

return function()
    for i, pair in ipairs(samples) do
        local value, string = pair[1], pair[2]

        trace(string, value)

        local _, parsed = assert(json.parse(string))
        local _, stringified = assert(json.stringify(value))

        trace(stringified, parsed)

        assert(equal(value, parsed), "value != parsed")
        assert(equal(string, stringified), "string != stringified")
    end
end
