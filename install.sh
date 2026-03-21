#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="ceversi.service"
APP_USER="ceversi"
INSTALL_DIR="/opt/ceversi"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}"
LIBTTAK_REPO="${LIBTTAK_REPO:-https://github.com/gg582/libttak.git}"
LIBTTAK_REF="${LIBTTAK_REF:-main}"
CWIST_REPO="${CWIST_REPO:-https://github.com/gg582/cwist.git}"
CWIST_REF="${CWIST_REF:-main}"
REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"

require_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "This installer must be run as root." >&2
        exit 1
    fi
}

require_root

if ! command -v systemctl >/dev/null; then
    echo "systemd is required but not available on this system." >&2
    exit 1
fi

install_packages() {
    local packages=(build-essential clang lld tcc pkg-config git sqlite3 libsqlite3-dev libssl-dev libcjson-dev liburiparser-dev ca-certificates openssl)
    if command -v apt-get >/dev/null; then
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y "${packages[@]}"
    else
        cat <<MSG >&2
Unsupported package manager. Install these packages manually and rerun:
${packages[*]}
MSG
        exit 1
    fi
}

install_packages

ensure_user() {
    if ! id -u "$APP_USER" >/dev/null 2>&1; then
        useradd --system --home "$INSTALL_DIR" --shell /usr/sbin/nologin "$APP_USER"
    fi
}

ensure_user

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

build_libttak() {
    git clone --depth 1 --branch "$LIBTTAK_REF" "$LIBTTAK_REPO" "$TMP_DIR/libttak"
    pushd "$TMP_DIR/libttak" >/dev/null
    make clean >/dev/null 2>&1 || true
    CC=tcc LDFLAGS="${LDFLAGS:-} -Wl,--no-eh-frame-hdr -fuse-ld=lld" make
    CC=tcc make install
    popd >/dev/null
}

build_cwist() {
    git clone --depth 1 --branch "$CWIST_REF" "$CWIST_REPO" "$TMP_DIR/cwist"
    git -C "$TMP_DIR/cwist" submodule update --init --recursive

    mkdir -p "$TMP_DIR/bin"
    cat <<'WRAP' > "$TMP_DIR/bin/tcc"
#!/bin/sh
/usr/bin/tcc -std=gnu17 "$@"
WRAP
    chmod +x "$TMP_DIR/bin/tcc"

    pushd "$TMP_DIR/cwist" >/dev/null
    rm -rf lib/libttak
    mkdir -p lib
    cp -r "$TMP_DIR/libttak" lib/libttak
    PATH="$TMP_DIR/bin:$PATH" LDFLAGS="${LDFLAGS:-} -fuse-ld=lld -Wl,--no-eh-frame-hdr" CC=tcc make
    PATH="$TMP_DIR/bin:$PATH" CC=tcc make install
    popd >/dev/null
}

build_libttak
build_cwist

pushd "$REPO_ROOT" >/dev/null
if [[ -x ./keygen.sh ]]; then
    ./keygen.sh
fi
make clean
make
popd >/dev/null

prepare_install_dir() {
    if systemctl list-unit-files | grep -q "^${SERVICE_NAME}"; then
        systemctl stop "$SERVICE_NAME" >/dev/null 2>&1 || true
    fi

    mkdir -p "$INSTALL_DIR"
    install -m 755 "$REPO_ROOT/server" "$INSTALL_DIR/ceversi"

    rm -rf "$INSTALL_DIR/public"
    cp -a "$REPO_ROOT/public" "$INSTALL_DIR/public"

    install -m 644 "$REPO_ROOT/index.html.tmpl" "$INSTALL_DIR/index.html.tmpl"
    install -m 644 "$REPO_ROOT/style.css" "$INSTALL_DIR/style.css"
    install -m 644 "$REPO_ROOT/script.js" "$INSTALL_DIR/script.js"

    install -m 644 "$REPO_ROOT/server.crt" "$INSTALL_DIR/server.crt"
    install -m 600 "$REPO_ROOT/server.key" "$INSTALL_DIR/server.key"

    if [[ ! -f "$INSTALL_DIR/othello.db" ]]; then
        touch "$INSTALL_DIR/othello.db"
    fi
    touch "$INSTALL_DIR/server.log"

    chown -R "$APP_USER":"$APP_USER" "$INSTALL_DIR"
}

prepare_install_dir

cat <<EOF > "$SERVICE_FILE"
[Unit]
Description=Ceversi C Othello Server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$APP_USER
Group=$APP_USER
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/ceversi
Restart=on-failure
RestartSec=5
Environment=PORT=31744
StandardOutput=append:$INSTALL_DIR/server.log
StandardError=append:$INSTALL_DIR/server.log

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now "$SERVICE_NAME"

printf "\nCeversi is installed and managed by systemd.\n"
printf 'Check status via: systemctl status %s\n' "$SERVICE_NAME"
printf 'Logs stream via: journalctl -u %s -f\n' "$SERVICE_NAME"
