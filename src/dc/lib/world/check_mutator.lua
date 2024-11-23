local error_kv = require "error_kv"

return function(mut)
    if not mut.from then
        error_kv("no mutator.from", { mutator = mut })
    end

    if not mut.to then
        error_kv("no mutator.to", { mutator = mut })
    end

    if mut.from == mut.to then
        error_kv("from == to", { mutator = mut })
    end

    if not mut.wraps then
        error_kv("no mutator.wraps", { mutator = mut })
    end
end
