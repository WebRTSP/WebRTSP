#!/bin/bash

# required open ports on firewall: tcp:3478, udp:3478,49152-65535

set -e

if [[ $# -ne 1 ]]; then
    echo "No target host specified."
    exit 1
fi

IFS='@' read -ra PARTS <<< "$1"
if [[ ${#PARTS[*]} -ne 2 ]]; then
    echo "Required target format it \"user@host[:port]\"."
    exit 1
fi

TARGET_USER=${PARTS[0]}

IFS=':' read -ra PARTS <<< "${PARTS[1]}"
if [[ ${#PARTS[*]} -eq 1 ]]; then
  TARGET_DOMAIN=${PARTS[0]}
  TARGET_PORT=22
elif [[ ${#PARTS[*]} -eq 2 ]]; then
  TARGET_DOMAIN=${PARTS[0]}
  TARGET_PORT=${PARTS[1]}
fi

ssh $TARGET_USER@$TARGET_DOMAIN -p $TARGET_PORT <<EOF

set -e

sudo apt install software-properties-common -y
sudo add-apt-repository universe
sudo add-apt-repository ppa:certbot/certbot

sudo apt update
sudo apt upgrade -y
sudo apt install certbot coturn -y


sudo mkdir -p /etc/coturn/certs
sudo chown -R turnserver:turnserver /etc/coturn/
sudo chmod -R 700 /etc/coturn/

sudo sed -i 's,#cert=/usr/local/etc/turn_server_cert.pem,cert=/etc/coturn/certs/$TARGET_DOMAIN.cert,g' /etc/turnserver.conf
sudo sed -i 's,#pkey=/usr/local/etc/turn_server_pkey.pem,pkey=/etc/coturn/certs/$TARGET_DOMAIN.key,g' /etc/turnserver.conf

sudo tee /etc/letsencrypt/renewal-hooks/deploy/coturn-certbot-deploy.sh > /dev/null <<'EOF2'
#!/bin/sh

set -e

for domain in \$RENEWED_DOMAINS; do
        case \$domain in
        $TARGET_DOMAIN)
                daemon_cert_root=/etc/coturn/certs

                # Make sure the certificate and private key files are
                # never world readable, even just for an instant while
                # we're copying them into daemon_cert_root.
                umask 077

                cp "\$RENEWED_LINEAGE/fullchain.pem" "\$daemon_cert_root/\$domain.cert"
                cp "\$RENEWED_LINEAGE/privkey.pem" "\$daemon_cert_root/\$domain.key"

                # Apply the proper file ownership and permissions for
                # the daemon to read its certificate and key.
                chown turnserver "\$daemon_cert_root/\$domain.cert" \\
                        "\$daemon_cert_root/\$domain.key"
                chmod 400 "\$daemon_cert_root/\$domain.cert" \\
                        "\$daemon_cert_root/\$domain.key"

                systemctl restart coturn > /dev/null
                # will work only from coturn v4.5.0.8, which is not available on ubuntu 18.04 LTS
                # /usr/bin/pkill -SIGUSR2 -F /var/run/turnserver.pid
                ;;
        esac
done
EOF2

sudo chmod 700 /etc/letsencrypt/renewal-hooks/deploy/coturn-certbot-deploy.sh

sudo certbot certonly --standalone --non-interactive --agree-tos --force-renewal -d $TARGET_DOMAIN

EOF
