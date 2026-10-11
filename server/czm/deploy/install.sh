#!/bin/bash
# Install or update the Counter Zombie Mobile service on the VDS. As root:
#   curl -fsSL https://raw.githubusercontent.com/kurostreet/cs16-client/<commit>/server/czm/deploy/install.sh | bash -s <commit>
# Secrets stay on the server in /etc/czm/env; the first install takes the bot from the
# video project settings (/etc/fva/env) and makes a new pepper. Nothing is printed.
set -euo pipefail
COMMIT=${1:?usage: install.sh <commit>}
REPO=https://raw.githubusercontent.com/kurostreet/cs16-client/$COMMIT/server/czm
FILES="requirements.txt czm/__init__.py czm/config.py czm/db.py czm/profanity.py czm/service.py czm/api.py
       czm/bot.py czm/steam.py czm/main.py czm/cli.py deploy/czm.service deploy/czm.caddy deploy/install.sh"
DEST=/opt/czm/releases/$COMMIT

id czm >/dev/null 2>&1 || useradd --system --home-dir /var/lib/czm --shell /usr/sbin/nologin czm
mkdir -p "$DEST/czm" "$DEST/deploy" /var/lib/czm/avatars /var/lib/czm/apk /var/lib/czm/server-list /etc/czm
for f in $FILES; do curl -fsSL "$REPO/$f" -o "$DEST/$f"; done

[ -x /opt/czm/venv/bin/python ] || python3 -m venv /opt/czm/venv
/opt/czm/venv/bin/pip install -q --disable-pip-version-check -r "$DEST/requirements.txt"

if [ ! -f /etc/czm/env ]; then
  (
    umask 077
    {
      echo "CZM_PEPPER=$(head -c 48 /dev/urandom | base64 | tr -d '/+=\n')"
      grep -E '^FVA_BOT_(TOKEN|API_URL|OWNER_IDS)=' /etc/fva/env 2>/dev/null | sed 's/^FVA_/CZM_/' || true
      echo "CZM_STEAM_KEY="
    } > /etc/czm/env
  )
fi
chown root:czm /etc/czm/env
chmod 640 /etc/czm/env

# the web server reads avatars, APK files and the server list
chown -R czm:czm /var/lib/czm
chmod 755 /var/lib/czm /var/lib/czm/avatars /var/lib/czm/apk /var/lib/czm/server-list

ln -sfn "$DEST" /opt/czm/current
install -m 644 "$DEST/deploy/czm.service" /etc/systemd/system/czm.service
systemctl daemon-reload
systemctl enable --quiet czm
systemctl restart czm
sleep 3
systemctl is-active czm
curl -fsS http://127.0.0.1:8200/api/v1/ping && echo
