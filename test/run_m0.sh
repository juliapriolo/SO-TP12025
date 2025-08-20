#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

make all
echo "== writer =="
bin/writer_shm -w 12 -h 10 -s 123
echo "== reader (3 filas) =="
bin/reader_shm -r 3
