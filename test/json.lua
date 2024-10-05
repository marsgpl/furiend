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
    { "\x00", '"\\u0000"' },
    { math.tointeger(1), "1" },
    { 1.000, "1.0" },
    { 1.1, "1.1" },
    { -1.0, "-1.0" },
    { -12.34e-5, "-0.0001234" },
    { math.maxinteger, "9223372036854775807" },
    { math.mininteger, "-9223372036854775808" },
    { math.pi, "3.141592653589793" },
    { 1234567890, "1234567890" },
    { -1234567890, "-1234567890" },
    { 123.456, "123.456" },
    { -0.0e0, "-0.0" },
    { 0, "0" },
    { 0.0, "0.0" },
    { -0, "0" },
    { 0, "-0", "0" },
    { -1, "-1" },
    { 0/-math.huge, "-0.0" },
    { 1e+28, "1e28" },
    { 1e-28, "1e-28" },
    { {}, "{}" },
    { {1}, "[1]" },
    { {[""]=1}, '{"":1}' },
    { {["\x00hi"]="\x01"}, '{"\\u0000hi":"\\u0001"}' },
    { {["✅"]=1}, '{"\\u2705":1}' },
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
        {["\x00oh\nno"]={k2={true,{"\x33",123,{4,5,6}},false,nil}}},
        '{"\\u0000oh\\nno":{"k2":[true,["3",123,[4,5,6]],false]}}',
    },
    {
        {{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
        {{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
        {{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
        }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
        }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
        }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}},
        '[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[' ..
        '[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[' ..
        '[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{' ..
        '}]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]' ..
        ']]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]' ..
        ']]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]',
    },
    { "Hello", '"Hello"' },
    {
        "\xD8\xAC\xD9\x8A\xD8\xAF \xD8\xAC\xD8\xAF\xD9\x8B\xD8\xA7",
        '"\x5Cu062C\x5Cu064A\x5Cu062F \x5Cu062C\x5Cu062F\x5Cu064B\x5Cu0627"',
    },
    { "I\'m under\x00 the\nwater", '"I\'m under\\u0000 the\\nwater"' },
    {
        0.00012345678901234567,
        "0.00012345678901234567",
    },
    {
        -0.00012345678901234567,
        "-0.00012345678901234567",
    },
    { -12.34e-10, "-1.234e-9" },
    {
        "\\\"\0\1\2\3\4\5\6\7\8\9\10\11\12\13\14\15\16\17\18\19\20\21\22\23\24\25\26\27\28\29\30\31",
        [["\\\"\u0000\u0001\u0002\u0003\u0004\u0005\u0006\u0007\b\t\n\u000B\f\r\u000E\u000F\u0010\u0011\u0012\u0013\u0014\u0015\u0016\u0017\u0018\u0019\u001A\u001B\u001C\u001D\u001E\u001F"]],
    },
    { {["-3.4e+20"]=1}, '{"-3.4e+20":1}' },
    { {["3.4e-20"]=1}, '{"3.4e-20":1}' },
    { "鳥", '"\\u9CE5"' },
    { "\239\160\128", '"\\uF800"' },
    { {["-3.4e+20"]=1}, '{"-3.4e+20":1}' },
    { {["3.4e-20"]=1}, '{"3.4e-20":1}' },
    { {a=1}, '{"a":1}' },
    { {"a"}, '["a"]' },
    { {123}, "[123]" },
    { {false,true}, "[false,true]" },
    { {{},{}}, "[{},{}]" },
}

local one_way_parse = {
    { "[null,1]", {nil,1} },
    { "[[]]", {{}} },
    { "[[],[]]", {{},{}} },
    { '"" ', "" },
    { ' ""', "" },
    { '\r\n""\t', "" },
    { "\r\n\tnull\r\n\t", nil },
    { "9999999999999999999", -8446744073709551617 },
    { "99999999999999999999", 1e+20 },
    { "-9999999999999999999", -1e+19 },
    { "-99999999999999999999", -1e+20 },
    { " 1 ", 1 },
    { "\r\n\t2\r\n\t", 2 },
    { "0.0", 0.0 },
    { "-0.1", -0.1 },
    {
        '[1,2,{"a":[4,5,6],"b":[{"a":[555],"b":"ccc"}]},4,5]',
        {1,2,{a={4,5,6},b={{a={555},b="ccc"}}},4,5},
    },
    { "123.456e-789", 0.0 },
}

local bad_input = {
    {[true]=1},
    {[false]=1},
    {[213]="aaa"},
    {[math.huge]=1},
    {[-math.huge]=1},
    {[math.maxinteger]=1},
    {[math.mininteger]=1},
    {[{}]=1},
    {[print]=1},
    {[function() end]=1},
    {[debug.getregistry()._CLIBS[1]]=1},
    {[io.stdout]=1},
    {[coroutine.create(print)]=1},
    circular_obj,
    circular_arr,
}

local bad_json = {
    "inf",
    "-inf",
    "nan",
    "tru",
    "tru ",
    "tru\ne",
    "alse",
    " alse",
    "True",
    "NULL",
    "true false",
    "n\x00ull",
    "n ull",
    "nil",
    "treu",
    '"',
    '\t"a',
    '"\\p"',
    '"\\"',
    '"\\',
    "",
    " ",
    "\r\n\t",
    "00",
    "01",
    "1.",
    "-1.e10",
    "1.1e",
    [["\uXXXX"]],
    [["\uDC00\uD800"]],
    [["\uDB00"]],
    [["\uDB00\uXXXX"]],
    [["\uDB00\uDB00"]],
    "[] {}",
    "[] []",
    "[}",
    "{]",
    "[{",
    " [ } ",
    " {\n] ",
    " [\t{ ",
    "[123",
    "[null",
    '["',
    '["a',
    '["a"',
    '["a",]',
    '["a",1,]',
    '{"',
    '{"a',
    '{"a"',
    '{"a":',
    '{"a":1',
    '{"a":1,',
    '{"a":1,}',
    "-123.456e+789",
    "123.456e789",
    "123.0456E0789",
}

return function()
    for _, pair in ipairs(samples) do
        local value, string = pair[1], pair[2]

        trace(string, value)

        local _, parsed = assert(json.parse(string))
        local _, stringified = assert(json.stringify(value))

        trace(stringified, parsed)
        assert(equal(value, parsed), "value != parsed")

        if not equal(string, stringified) and type(pair[3]) == "string" then
            assert(equal(pair[3], stringified), "string != stringified")
        else
            assert(equal(string, stringified), "string != stringified")
        end
    end

    for _, pair in ipairs(one_way_parse) do
        local string, value = pair[1], pair[2]
        trace(string, value)
        local _, parsed = assert(json.parse(string))
        trace(parsed)
        assert(equal(value, parsed), "value != parsed")
    end

    for _, value in ipairs(bad_input) do
        trace(value)
        local result, stringified = json.stringify(value)
        trace(result, stringified)
        assert(not result, "failed to fail stringify")
    end

    for _, string in ipairs(bad_json) do
        trace(string)
        local result, parsed = json.parse(string)
        trace(result, parsed)
        assert(not result, "failed to fail parse")
    end
end
