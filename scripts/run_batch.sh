#!/usr/bin/env bash
set -euo pipefail

cd /home/joglio/Quantas

runs=(
  "quantas/ExamplePeer/ExampleInput.json"
  "quantas/AltBitPeer/AltBitInput.json"
)

for f in "${runs[@]}"; do
  echo "[$(date -Is)] START $f"
  make run INPUTFILE="$f"
  echo "[$(date -Is)] END   $f"
done
