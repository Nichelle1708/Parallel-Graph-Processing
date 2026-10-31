#!/usr/bin/env bash
# Downloads the SNAP datasets used by the batch into data/ and decompresses them.
# Dataset facts below were taken from the SNAP dataset pages (snap.stanford.edu/data).
#
#   name          nodes        edges (as listed by SNAP)   type        diameter (SNAP)
#   ca-GrQc          5,242          14,496                 undirected   17
#   email-Enron     36,692         183,831                 undirected   11
#   roadNet-CA   1,965,206       2,766,607                 undirected  849
#   web-Google     875,713       5,105,039                 directed     21   (optional, mainly for PageRank)
#
# Usage: scripts/download_data.sh            (the three undirected graphs)
#        scripts/download_data.sh web-Google (any additional SNAP name from the list above)
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p data
NAMES=("$@")
if [ ${#NAMES[@]} -eq 0 ]; then NAMES=(ca-GrQc email-Enron roadNet-CA); fi

for n in "${NAMES[@]}"; do
  if [ -f "data/${n}.txt" ]; then echo "data/${n}.txt already present"; continue; fi
  echo "downloading ${n} ..."
  curl -fL --retry 3 -o "data/${n}.txt.gz" "https://snap.stanford.edu/data/${n}.txt.gz"
  gunzip -f "data/${n}.txt.gz"
  echo "  -> data/${n}.txt ($(wc -l < "data/${n}.txt") lines incl. comments)"
done
