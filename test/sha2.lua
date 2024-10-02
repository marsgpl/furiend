local trace = require "trace"
local sha2 = require "sha2"
local equal = require "equal"

local samples = {
    "",
    "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",

    "The quick brown fox jumps over the lazy dog",
    "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb642e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6",

    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. 1234567890",
    "543d79ccec281ba30536d5245929c1fbcde05167f34968936ed89202135e007a41125f0a3a19b81a8cfe95be962585bb39c628f2e05c2595ed059e33ded1c8a2",
}

return function()
    for i = 1, #samples, 2 do
        local input = samples[i]
        local result = sha2.sha512(input)
        local answer = samples[i + 1]

        trace("input:", input)
        print("result:", result)
        print("answer:", answer)

        assert(equal(result, answer), "result != answer")
    end
end
