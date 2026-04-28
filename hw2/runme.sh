#!/bin/bash

RESULT="result.txt"
SHARED="shared_file"
STATS="stats.txt"

log() { echo "$*" | tee -a "$RESULT"; }
sep() { log "------------------------------------------------------------"; }

rm -f "$RESULT" "$STATS" "$SHARED" "$SHARED.lck"

log "File lock test — $(date)"
sep

log ""
log "[BUILD]"
make clean 2>&1 | tee -a "$RESULT"
make 2>&1 | tee -a "$RESULT"
sep

touch "$SHARED"

log ""
log "[TEST] Starting 10 parallel processes, running for 5 minutes"
log "Each process locks $SHARED for 1 second in an infinite loop."
log "On SIGINT each process writes its lock count to $STATS."

PIDS=()
for i in $(seq 1 10); do
    ./mylock "$SHARED" &
    PIDS+=($!)
done
log "PIDs: ${PIDS[*]}"

sleep 300

log "Sending SIGINT..."
kill -SIGINT "${PIDS[@]}"
wait "${PIDS[@]}" 2>/dev/null
sep

log ""
log "[RESULTS]"
log "Contents of $STATS:"
cat "$STATS" | tee -a "$RESULT"

log ""
LINE_COUNT=$(wc -l < "$STATS")
log "Lines in stats.txt: $LINE_COUNT (expected 10)"
[ "$LINE_COUNT" -eq 10 ] && log "PASS: one line per process" || log "FAIL: expected 10 lines"

log ""
MIN=$(awk '{print $3}' "$STATS" | sort -n | head -1)
MAX=$(awk '{print $3}' "$STATS" | sort -n | tail -1)
log "Min locks: $MIN, Max locks: $MAX (roughly equal = no starvation)"
sep
