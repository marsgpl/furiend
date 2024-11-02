return {
    id = "fe2",
    dc = "dc",
    redis = {
        ip4 = "172.20.0.3",
        port = 30303,
        password = "LocalPassword123",
        client_name = "fe2",
    },
    dns = {
        ip4 = "1.1.1.1",
        timeout = 3,
    },
    telegram = {
        host = "api.telegram.org",
        token = "000:xxx",
        webhook = {
            url = "https://xxx",
            secret_token = "xxx",
            allowed_updates = {
                "message",
                "edited_message",
                "message_reaction",
            },
            ip4 = "127.0.0.1",
            ip4_docker = "0.0.0.0",
            ports = { 19001, 19002 },
            port = 19001,
        }
    },
}
