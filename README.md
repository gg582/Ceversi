# Ceversi

> C-crafted Othello that anyone can spin up, feather-light, and unapologetically slick.

Ceversi is a minimalist, SSL-ready Reversi/Othello server written in straight-up C. It ships with a tiny deployment footprint, a dockerized distro, and zero-nonsense scripts so you can run games on your own turf without wrestling yet another web stack.

## Why You’ll Vibe
- Pure C core that stays lean and predictable.
- SQLite-backed stats so every flip counts.
- SSL out of the box; no mystery meat configs.
- One-touch deploy script for the “I just wanna play” crowd.

## Spin It Up
### Easiest path (recommended)
```bash
chmod +x scripts/deploy/quick-deploy.sh
./scripts/deploy/quick-deploy.sh
```
This handles key generation, dependency setup, Docker build, and boots the service at `https://localhost:31744` (self-signed cert, so allow it once).

### Bare-metal build
```bash
./scripts/deploy/setup_libs.sh      # install native deps once
make                 # builds ./server
./server             # launches the C backend
```
Feel free to customize `docker-compose.yml` or `Makefile` if you’re targeting something exotic.

### Systemd install (apt-based distros)
```bash
sudo ./scripts/deploy/install.sh
```
This script mirrors the Docker build steps on the host, installs the binary + assets into `/opt/ceversi`, and drops a `ceversi.service` unit so `systemd` keeps it alive. Manage it with `systemctl status|restart ceversi.service` and tail logs via `journalctl -u ceversi.service -f`.

## Project Map
- `src/app`, `src/http`, `src/data`, `src/game`, `src/core` – backend code split by responsibility.
- `public/` – browser assets served under `/static`.
- `templates/` – server-rendered HTML templates.
- `scripts/deploy/` – install and bootstrap scripts.
- `tests/` – harness for verifying move logic.

## Contribute Your Spice
1. Fork, branch, hack.
2. Keep it lightweight; comment only when future-you would be confused.
3. Send a PR with screenshots or logs if you tweaked visuals/networking.

Made with competitive energy and a can of Dr. Pepper. Have fun flipping.
