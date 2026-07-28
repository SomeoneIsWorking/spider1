#!/usr/bin/env bash
# ghidra_import.sh — (re)build the headless Ghidra project from the RAM image.
set -eu
cd "$(dirname "$0")/.."
[ -f scratch/bin/spiderman/ram.bin ] || { echo "run tools/redump_ram.py first" >&2; exit 1; }
rm -rf scratch/ghidra && mkdir -p scratch/ghidra
analyzeHeadless scratch/ghidra spider1 -import scratch/bin/spiderman/ram.bin \
  -processor MIPS:LE:32:default -loader BinaryLoader -loader-baseAddr 0x80000000
