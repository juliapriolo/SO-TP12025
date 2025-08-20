#!/usr/bin/env bash
set -euo pipefail
for name in /game_state /game_sync; do
  if [ -e "/dev/shm${name}" ]; then
    echo "Removing /dev/shm${name}"
    rm -f "/dev/shm${name}"
  fi
done
