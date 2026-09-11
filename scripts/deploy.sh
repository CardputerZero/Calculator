#!/usr/bin/env bash
set -euo pipefail

REMOTE="${1:-pi@192.168.199.179}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="$ROOT/dist/Calculator"
DESKTOP="$ROOT/applications/calculator.desktop"
ICON="$ROOT/share/images/calculator.png"

if command -v sshpass >/dev/null 2>&1 && [[ -n "${SSHPASS:-}" ]]; then
    SSH=(sshpass -e ssh -o PubkeyAuthentication=no -o PreferredAuthentications=password)
    SCP=(sshpass -e scp -o PubkeyAuthentication=no -o PreferredAuthentications=password)
else
    SSH=(ssh)
    SCP=(scp)
fi

for file in "$BINARY" "$DESKTOP" "$ICON"; do
    if [[ ! -f "$file" ]]; then
        echo "Missing deploy artifact: $file" >&2
        exit 1
    fi
done

if ! file "$BINARY" | grep -q 'ARM aarch64'; then
    echo "Refusing to deploy a non-AArch64 binary: $BINARY" >&2
    exit 1
fi

stage=$("${SSH[@]}" "$REMOTE" 'mktemp -d /tmp/calculator-deploy.XXXXXX')
cleanup() {
    "${SSH[@]}" "$REMOTE" "rm -rf '$stage'" >/dev/null 2>&1 || true
}
trap cleanup EXIT

"${SCP[@]}" -q "$BINARY" "$REMOTE:$stage/Calculator"
"${SCP[@]}" -q "$DESKTOP" "$REMOTE:$stage/calculator.desktop"
"${SCP[@]}" -q "$ICON" "$REMOTE:$stage/calculator.png"

"${SSH[@]}" "$REMOTE" "bash -s" <<REMOTE_SCRIPT
set -euo pipefail
stage='$stage'
root=/usr/share/APPLaunch
backup=\$root/backups/calculator-\$(date +%Y%m%d-%H%M%S)

if ! sudo -n true 2>/dev/null; then
    echo 'Passwordless sudo is required to install into /usr/share/APPLaunch' >&2
    exit 1
fi

sudo -n install -d "\$root/bin" "\$root/applications" "\$root/share/images" "\$root/backups"
sudo -n mkdir -p "\$backup"

for rel in bin/Calculator applications/calculator.desktop share/images/calculator.png; do
    if [[ -e "\$root/\$rel" ]]; then
        sudo -n cp -a --parents "\$root/\$rel" "\$backup/"
    fi
done

sudo -n install -m 0755 "\$stage/Calculator" "\$root/bin/Calculator"
sudo -n install -m 0644 "\$stage/calculator.desktop" "\$root/applications/calculator.desktop"
sudo -n install -m 0644 "\$stage/calculator.png" "\$root/share/images/calculator.png"
sudo -n rm -rf "\$stage"

systemctl --user restart APPLaunch.service
sleep 1
status=\$(systemctl --user is-active APPLaunch.service)
printf 'APPLaunch.service: %s\n' "\$status"
[[ "\$status" == active ]]
printf 'binary sha256: '
sha256sum "\$root/bin/Calculator"
printf 'backup: %s\n' "\$backup"
REMOTE_SCRIPT
