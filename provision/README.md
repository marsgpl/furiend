# Füriend - provision

## Digitalocean - Entourage #2

FRA, Alma 9, Basic/Regular, 4/mo, ipv6, PTR: $FE2_HOST

```sh
cd provision
. ./.env

open "https://dash.cloudflare.com/$CF_ACCOUNT_ID/$CF_EMAIL/dns/records"
open "http://$FE2_IP4"
open "https://$FE2_HOST"

rsync -i ~/.ssh/id_rsa -rv etc furiend root usr root@$FE2_IP4:/
ssh -i ~/.ssh/id_rsa root@$FE2_IP4

rsync -e 'ssh -p 2' -rv etc furiend root usr root@$FE2_IP4:/
ssh -p 2 -o SendEnv="FE2_* CF_*" root@$FE2_IP4

unlink /etc/motd.d/cockpit
dnf remove rpcbind chrony -y
dnf install vim nginx -y
mkdir -p /usr/share/nginx/certs && cd /usr/share/nginx/certs
openssl genrsa -aes256 -passout pass:123 -out fe2.pass.key 4096
openssl rsa -passin pass:123 -in fe2.pass.key -out fe2.key
openssl req -new -key fe2.key -out fe2.csr
openssl x509 -req -sha256 -days 3650 -in fe2.csr -signkey fe2.key -out fe2.crt
cp fe2.crt fe2.cloudflare.crt # or use cloudflare ssl-tls/origin
openssl dhparam -dsaparam -out fe2.dh.pem 4096
semanage port -a -t ssh_port_t -p tcp 2
semanage port -a -t http_port_t -p tcp 19999
setsebool -P httpd_can_network_connect 1
systemctl start nginx
systemctl enable nginx
reboot
ss -plantu
systemctl list-sockets --all
hostnamectl set-hostname $FE2_HOST
cloudflare POST dns_records "{\"type\":\"A\",\"name\":\"fe2\",\"content\":\"$FE2_IP4\",\"proxied\":true}"
cloudflare POST dns_records "{\"type\":\"AAAA\",\"name\":\"fe2\",\"content\":\"$FE2_IP6\",\"proxied\":true}"
```

## selinux

on access error (in case files were moved)

```sh
restorecon -v -R /usr/share/nginx
```
