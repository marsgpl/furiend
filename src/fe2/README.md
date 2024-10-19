# Füriend Entourage #2 - fe2

## Logic

- connect to DC
- setup telegram web hook
- request tg bot info
- send all tg events to DC
- receive commands from DC
- execute commands via tg bot

## Events

- start
- exit
- tgwh info
- tg bot info
- tg bot event
- command executed

## Test tgwh

```sh
. provision/.env
curl -v -X POST 127.0.0.1:19001/tgwh -d '{"test":123}' \
    -H "Host: $FE2_HOST" \
    -H "X-Telegram-Bot-Api-Secret-Token: $TGWH_TOKEN"
```
