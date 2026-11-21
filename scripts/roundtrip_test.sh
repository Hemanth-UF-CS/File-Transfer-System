#!/usr/bin/env bash
# Upload a random 5 MB file to a local server, download it back, and compare bytes.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'kill "${server_pid:-}" 2>/dev/null || true; rm -rf "$tmp"' EXIT

mkdir -p "$tmp/server" "$tmp/up" "$tmp/down"
head -c 5000000 /dev/urandom > "$tmp/up/blob.bin"
(cd "$tmp/server" && exec "$root/server") > "$tmp/server.log" 2>&1 &
server_pid=$!
sleep 1

(cd "$tmp/up" && "$root/client" 127.0.0.1 upload blob.bin) > /dev/null
(cd "$tmp/down" && "$root/client" 127.0.0.1 download blob.bin) > /dev/null
cmp "$tmp/up/blob.bin" "$tmp/server/uploads/blob.bin"
cmp "$tmp/up/blob.bin" "$tmp/down/blob.bin"
echo "ok: 5 MB upload and download are byte-identical"
