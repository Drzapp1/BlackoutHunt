#!/usr/bin/env bash
# Live progress bar for the Linux Wine cook. Phase is read from the real build
# log (+ the side UBT/UBA log for compile [N/M]); the bar creeps on per-phase
# elapsed time, clamped to the reached phase. Exits when the build writes EXIT=.
set -u
LOG="${1:-/run/media/adamrosta/T7/BlackoutHunt/build-linux-0.8.0.log}"
UBA="$HOME/.wine/drive_c/users/adamrosta/AppData/Roaming/Unreal Engine/AutomationTool/Logs/D+UE_5.7/UBA-BlackoutHunt-Linux-Shipping.txt"
STATE="/tmp/bhbar.phase"
WIDTH=28

# phase -> "floor ceil est_seconds label detail". Anchored on UAT's
# "***** XXX COMMAND STARTED *****" banners (never in the command-line echo).
detect() {
  if grep -q '^EXIT=' "$LOG" 2>/dev/null; then echo "100 100 1 DONE done"; return; fi
  if grep -qE 'AutomationTool exiting|BUILD SUCCESSFUL' "$LOG" 2>/dev/null; then echo "99 100 30 FINALIZE wrapping_up"; return; fi
  if grep -qE 'ARCHIVE COMMAND STARTED' "$LOG" 2>/dev/null; then echo "97 99 120 ARCHIVE copying_to_Builds/Linux"; return; fi
  if grep -qE 'STAGE COMMAND STARTED' "$LOG" 2>/dev/null; then
    # pak runs inside the STAGE command on UE
    if grep -qiE 'RunUnrealPak|Creating pak|Generating .*\.pak' "$LOG" 2>/dev/null; then echo "90 97 240 PAK packing_paks"; return; fi
    echo "82 90 180 STAGE staging_files"; return
  fi
  if grep -qE 'COOK COMMAND STARTED|LogCook' "$LOG" 2>/dev/null; then echo "30 82 1200 COOK cooking_content+shaders"; return; fi
  if grep -qE 'BUILD COMMAND STARTED' "$LOG" 2>/dev/null; then
    nm=$(grep -oE '\[[0-9]+/[0-9]+\]' "$UBA" "$LOG" 2>/dev/null | grep -oE '[0-9]+/[0-9]+' | tail -1)
    [ -n "$nm" ] && echo "12 30 900 BUILD compiling[$nm]" || echo "12 30 900 BUILD compiling"
    return
  fi
  if grep -qE 'Creating makefile|Total script module initialization' "$LOG" 2>/dev/null; then echo "10 14 240 BUILD makefile/UBT"; return; fi
  echo "0 12 720 INIT UAT_bootstrap"
}

start_epoch="${START_EPOCH:-$(stat -c %Y "$LOG" 2>/dev/null || date +%s)}"
last=""
prev_phase=""
while true; do
  read -r floor ceil est label detail < <(detect)
  now=$(date +%s)
  # per-phase start tracking
  if [ "$label" != "$prev_phase" ]; then echo "$now" > "$STATE"; prev_phase="$label"; fi
  pstart=$(cat "$STATE" 2>/dev/null || echo "$now")
  ein=$(( now - pstart )); total=$(( now - start_epoch ))
  # sub-progress within band
  nm=$(echo "$detail" | grep -oE '[0-9]+/[0-9]+' | head -1)
  if [ -n "$nm" ]; then
    n=${nm%/*}; m=${nm#*/}; [ "$m" -gt 0 ] && frac=$(( 100*n/m )) || frac=0
  else
    frac=$(( 100*ein/est )); [ "$frac" -gt 92 ] && frac=92
  fi
  pct=$(( floor + (ceil-floor)*frac/100 ))
  [ "$label" = "DONE" ] && pct=100
  [ "$pct" -lt 0 ] && pct=0; [ "$pct" -gt 100 ] && pct=100
  filled=$(( pct*WIDTH/100 )); empty=$(( WIDTH-filled ))
  bar=""; i=0; while [ $i -lt $filled ]; do bar="$bar#"; i=$((i+1)); done
  i=0; while [ $i -lt $empty ]; do bar="$bar."; i=$((i+1)); done
  render=$(printf '[%s] %3d%%  %-8s %s  • %dm%02ds' "$bar" "$pct" "$label" "$detail" "$((total/60))" "$((total%60))")
  [ "$render" != "$last" ] && { echo "$render"; last="$render"; }
  if grep -q '^EXIT=' "$LOG" 2>/dev/null; then
    code=$(grep -oE 'EXIT=[0-9]+' "$LOG" | tail -1 | cut -d= -f2)
    [ "$code" = "0" ] && echo "✅ cook finished OK (EXIT=0)" || echo "❌ cook FAILED (EXIT=$code) — see build-linux-0.8.0.log"
    exit 0
  fi
  sleep 90
done
