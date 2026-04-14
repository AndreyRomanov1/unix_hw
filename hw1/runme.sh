#!/bin/bash
set -e

RESULT="result.txt"

log() { echo "$*" | tee -a "$RESULT"; }
sep()  { log "------------------------------------------------------------"; }

rm -f "$RESULT" fileA fileA.gz fileB fileB.gz fileC fileD

log "Sparse file program — test report"
log "Date: $(date)"
sep

log ""
log "[BUILD] make"
make clean 2>&1 | tee -a "$RESULT"
make       2>&1 | tee -a "$RESULT"
log "[BUILD] OK"
sep

log ""
log "[STEP 1] Creating test file fileA"
bash create_test_file.sh fileA 2>&1 | tee -a "$RESULT"
sep

log ""
log "[TEST 2] ./myprogram fileA fileB  (default 4096-byte blocks)"
log "Expected: same logical size, far fewer disk blocks than fileA"
./myprogram fileA fileB
log "fileA: $(stat -c 'logical=%s bytes  blocks=%b×512B' fileA)"
log "fileB: $(stat -c 'logical=%s bytes  blocks=%b×512B' fileB)"
sep

log ""
log "[TEST 3] gzip -k fileA fileB"
log "Expected: fileB.gz << fileA.gz (holes compress better)"
gzip -k fileA fileB
log "fileA.gz: $(stat -c '%s bytes' fileA.gz)"
log "fileB.gz: $(stat -c '%s bytes' fileB.gz)"
sep

log ""
log "[TEST 4] gzip -cd fileB.gz | ./myprogram fileC"
log "Expected: fileC sparse, byte-for-byte identical to fileA"
gzip -cd fileB.gz | ./myprogram fileC
log "fileC: $(stat -c 'logical=%s bytes  blocks=%b×512B' fileC)"
diff -q fileA fileC > /dev/null 2>&1 && log "PASS: identical to fileA" || log "FAIL: differs from fileA"
sep

log ""
log "[TEST 5] ./myprogram -b 100 fileA fileD"
log "Expected: same content, more blocks than fileB (100-byte block straddles 4KB boundary)"
./myprogram -b 100 fileA fileD
log "fileD: $(stat -c 'logical=%s bytes  blocks=%b×512B' fileD)"
diff -q fileA fileD > /dev/null 2>&1 && log "PASS: identical to fileA" || log "FAIL: differs from fileA"
sep

log ""
log "[STAT SUMMARY]"
stat -c '%n  logical=%s bytes  blocks=%b×512B' fileA fileA.gz fileB fileB.gz fileC fileD \
    2>&1 | tee -a "$RESULT"
sep

log ""
log "Done. Results saved to $RESULT"
