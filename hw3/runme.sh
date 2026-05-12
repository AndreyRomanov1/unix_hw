#!/bin/bash

RESULT="result.txt"
LOG="/tmp/myinit.log"
PIDFILE="/tmp/myinit.pid"

log() { echo "$*" | tee -a "$RESULT"; }
sep() { log "------------------------------------------------------------"; }

cd "$(dirname "$0")"
DIR="$(pwd)"

rm -f "$RESULT" "$LOG" "$PIDFILE"

log "myinit test — $(date)"
sep

log ""
log "[BUILD]"
make clean 2>&1 | tee -a "$RESULT"
make       2>&1 | tee -a "$RESULT"
sep

cat > "$DIR/config1.txt" <<EOF
/bin/sleep 1111 /dev/null /dev/null
/bin/sleep 2222 /dev/null /dev/null
/bin/sleep 3333 /dev/null /dev/null
EOF

cat > "$DIR/config2.txt" <<EOF
/bin/sleep 9999 /dev/null /dev/null
EOF

log ""
log "[TEST 1] Start myinit with 3 processes"
log "Expected: 3 child processes (sleep 1111, sleep 2222, sleep 3333)"
"$DIR/myinit" "$DIR/config1.txt"
sleep 1

MYINIT=$(cat "$PIDFILE")
log "myinit PID: $MYINIT"
log "Children:"
ps --ppid "$MYINIT" -o pid,cmd --no-headers 2>&1 | tee -a "$RESULT"
COUNT=$(ps --ppid "$MYINIT" --no-headers 2>/dev/null | wc -l)
log "Count: $COUNT (expected 3)"
[ "$COUNT" -eq 3 ] && log "PASS" || log "FAIL"
sep

log ""
log "[TEST 2] Kill process #2 (sleep 2222), wait 1 second, check restart"
log "Expected: 3 child processes again"
pkill -f "sleep 2222"
sleep 1
log "Children after restart:"
ps --ppid "$MYINIT" -o pid,cmd --no-headers 2>&1 | tee -a "$RESULT"
COUNT=$(ps --ppid "$MYINIT" --no-headers 2>/dev/null | wc -l)
log "Count: $COUNT (expected 3)"
[ "$COUNT" -eq 3 ] && log "PASS" || log "FAIL"
sep

log ""
log "[TEST 3] Replace config with 1 process, send SIGHUP"
log "Expected: 1 child process (sleep 9999)"
cp "$DIR/config2.txt" "$DIR/config1.txt"
kill -HUP "$MYINIT"
sleep 1
log "Children after SIGHUP:"
ps --ppid "$MYINIT" -o pid,cmd --no-headers 2>&1 | tee -a "$RESULT"
COUNT=$(ps --ppid "$MYINIT" --no-headers 2>/dev/null | wc -l)
log "Count: $COUNT (expected 1)"
[ "$COUNT" -eq 1 ] && log "PASS" || log "FAIL"
sep

log ""
log "[LOG FILE: $LOG]"
cat "$LOG" | tee -a "$RESULT"
sep

log ""
log "[LOG VERIFICATION]"
NSTARTED=$(grep -c "started pid=" "$LOG")
NTERM=$(grep -c "terminated" "$LOG")
NKILLED=$(grep -c "killed" "$LOG")
NHUP=$(grep -c "SIGHUP" "$LOG")
log "start events   : $NSTARTED (expected >=5: 3 initial + 1 restart + 1 after SIGHUP)"
log "terminated     : $NTERM    (expected 3: old processes on SIGHUP)"
log "killed by signal: $NKILLED (expected 1: process #2 killed externally)"
log "SIGHUP events  : $NHUP     (expected 1)"
sep

kill "$MYINIT" 2>/dev/null
