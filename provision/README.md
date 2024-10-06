# Füriend - provision

## Digitalocean - Entourage #2

FRA, Alma 9, Basic/Regular, 4/mo, ipv6, PTR: $FE2_HOST

```sh
cd provision
. ./.env
rsync -rv etc furiend root usr root@$FE2_IP4:/
ssh -i ~/.ssh/id_rsa -o SendEnv="FE2_* CF_*" root@$FE2_IP4
open "https://dash.cloudflare.com/$CF_ACCOUNT_ID/$CF_EMAIL/dns/records"
open "http://$FE2_IP4"
open "https://$FE2_HOST"
```

```bash
unlink /etc/motd.d/cockpit
dnf remove rpcbind chrony -y
reboot
hostnamectl set-hostname $FE2_HOST
mkdir -p /usr/share/nginx/certs && cd /usr/share/nginx/certs
openssl genrsa -aes256 -passout pass:123 -out fe2.pass.key 4096
openssl rsa -passin pass:123 -in fe2.pass.key -out fe2.key
openssl req -new -key fe2.key -out fe2.csr
openssl x509 -req -sha256 -days 3650 -in fe2.csr -signkey fe2.key -out fe2.crt
cp fe2.crt fe2.cloudflare.crt # or use cloudflare ssl-tls/origin
openssl dhparam -dsaparam -out fe2.dh.pem 4096
dnf install vim nginx -y
systemctl start nginx
systemctl enable nginx
ss -plantu
systemctl list-sockets --all
cloudflare POST dns_records "{\"type\":\"A\",\"name\":\"fe2\",\"content\":\"$FE2_IP4\",\"proxied\":true}"
cloudflare POST dns_records "{\"type\":\"AAAA\",\"name\":\"fe2\",\"content\":\"$FE2_IP6\",\"proxied\":true}"
```

## selinux

on access error (in case files were moved)

```sh
restorecon -v -R /usr/share/nginx
restorecon -v -R /etc/nginx
```
