# Füriend - provision

## Digitalocean - Entourage #2

FRA, Alma 9, Basic/Regular, 4/mo, ipv6, PTR: fe2.marsgpl.com

```sh
. provision/.env
rsync -rv provision/etc provision/root provision/furiend root@$FE2_IP4:/
ssh -i ~/.ssh/id_rsa -o SendEnv="FE2_* CF_*" root@$FE2_IP4
open "https://dash.cloudflare.com/$CF_ACCOUNT_ID/marsgpl.com/dns/records"
```

```bash
hostnamectl set-hostname fe2.marsgpl.com
unlink /etc/motd.d/cockpit
dnf remove rpcbind chrony -y
dnf install vim -y
reboot
echo $FE2_IP4
ss -plantu
systemctl list-sockets --all
cloudflare POST dns_records "{\"type\":\"A\",\"name\":\"fe2\",\"content\":\"$FE2_IP4\",\"proxied\":true}"
cloudflare POST dns_records "{\"type\":\"AAAA\",\"name\":\"fe2\",\"content\":\"$FE2_IP6\",\"proxied\":true}"
```
