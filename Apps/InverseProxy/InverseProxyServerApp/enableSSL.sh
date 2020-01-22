#!/bin/bash

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
sudo apt install nginx certbot python-certbot-nginx -y

sudo systemctl stop nginx
sudo rm -rf /etc/nginx/sites-enabled/*

sudo certbot certonly --standalone --non-interactive --agree-tos -d $TARGET_DOMAIN

sudo tee /etc/nginx/conf.d/WebRTSPProxy.conf > /dev/null <<'EOF2'
server {
  server_name $TARGET_DOMAIN;

  location / {
      proxy_pass http://localhost:8000/;
      proxy_http_version 1.1;
      proxy_set_header Upgrade \$http_upgrade;
      proxy_set_header Connection "Upgrade";
  }

  error_page 497 https://\$server_name:\$server_port\$request_uri;

  listen [::]:8010 ssl ipv6only=on;
  listen 8010 ssl;
  ssl_certificate /etc/letsencrypt/live/$TARGET_DOMAIN/fullchain.pem;
  ssl_certificate_key /etc/letsencrypt/live/$TARGET_DOMAIN/privkey.pem;
}

server {
  server_name $TARGET_DOMAIN;

  location / {
      proxy_pass http://localhost:8001/;
      proxy_http_version 1.1;
      proxy_set_header Upgrade \$http_upgrade;
      proxy_set_header Connection "Upgrade";
  }

  error_page 497 https://\$server_name:\$server_port\$request_uri;

  listen [::]:8011 ssl ipv6only=on;
  listen 8011 ssl;
  ssl_certificate /etc/letsencrypt/live/$TARGET_DOMAIN/fullchain.pem;
  ssl_certificate_key /etc/letsencrypt/live/$TARGET_DOMAIN/privkey.pem;
}
EOF2

sudo nginx -t
sudo systemctl start nginx
EOF
