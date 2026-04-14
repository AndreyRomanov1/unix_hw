#!/bin/bash
set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <output_file>" >&2
    exit 1
fi

OUTFILE="$1"
SIZE=$(( 4 * 1024 * 1024 + 1 ))
LAST=$(( SIZE - 1 ))

dd if=/dev/zero of="$OUTFILE" bs=1M count=4 2>/dev/null
dd if=/dev/zero of="$OUTFILE" bs=1 count=1 seek=$LAST conv=notrunc 2>/dev/null

printf '\x01' | dd of="$OUTFILE" bs=1 seek=0     count=1 conv=notrunc 2>/dev/null
printf '\x01' | dd of="$OUTFILE" bs=1 seek=10000 count=1 conv=notrunc 2>/dev/null
printf '\x01' | dd of="$OUTFILE" bs=1 seek=$LAST count=1 conv=notrunc 2>/dev/null

echo "Created '$OUTFILE': $SIZE bytes, non-zero at offsets 0, 10000, $LAST"
