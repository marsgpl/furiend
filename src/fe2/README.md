# Füriend Entourage #2 - fe2

## bind

```sh
. provision/.env
ssh -p 2 -vnNT -R 19001:127.0.0.1:19001 root@$FE2_IP4

curl -v -X POST 127.0.0.1:19001/tgwh -d '{"test":123}' \
    -H "Host: $FE2_HOST" \
    -H "X-Telegram-Bot-Api-Secret-Token: $TGWH_TOKEN"
```
