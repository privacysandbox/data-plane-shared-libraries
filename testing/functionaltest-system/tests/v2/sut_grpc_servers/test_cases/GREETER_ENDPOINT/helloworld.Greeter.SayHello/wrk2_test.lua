wrk.method = "POST"
wrk.headers["Content-Type"] = "application/json"
wrk.headers["X-User-Agent"] = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36"
wrk.headers["x-accept-language"] = "en-US,en;q=0.9"
wrk.headers["X-BnA-Client-IP"] = "104.133.126.32"

requests = {}

init = function()
    f = assert(io.open("empty-name.request.json", "rb"))
    body_1 = f:read("*all")
    body_1 = body_1:gsub("[\n\r]", " ")
    f:close()

    f = assert(io.open("simple-filter.request.json", "rb"))
    body_2 = f:read("*all")
    body_2 = body_2:gsub("[\n\r]", " ")
    f:close()

    f = assert(io.open("simple.request.json", "rb"))
    body_3 = f:read("*all")
    body_3 = body_3:gsub("[\n\r]", " ")
    f:close()

    f = assert(io.open("unicode.request.json", "rb"))
    body_4 = f:read("*all")
    body_4 = body_4:gsub("[\n\r]", " ")
    f:close()

    requests[1] = wrk.format(nil, nil, nil, body_1)
    requests[2] = wrk.format(nil, nil, nil, body_2)
    requests[3] = wrk.format(nil, nil, nil, body_3)
    requests[4] = wrk.format(nil, nil, nil, body_4)
end

function request()
    return requests[math.random(#requests)]
end

response = function(status, header, body)
    if status > 200 then
        print("status:" .. status)
        print(body)
        print("-------------------------------------------------");
    end
end
