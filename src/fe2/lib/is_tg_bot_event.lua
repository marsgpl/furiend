return function(event)
    return type(event) == "table"
        and type(event.update_id) == "number"
end
