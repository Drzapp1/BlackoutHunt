#!/usr/bin/env bash
set -euo pipefail

preferred_root="${1:-${UE_ROOT:-/run/media/adamrosta/T7/UE_5.7}}"

candidates=(
	"$preferred_root"
	"$HOME/UE_5.7"
	"$HOME/Epic Games/UE_5.7"
	"$HOME/UnrealEngine"
	"/opt/UE_5.7"
	"/opt/UnrealEngine"
)

for root in "${candidates[@]}"; do
	if [[ -f "$root/Engine/Build/BatchFiles/RunUAT.sh" ]]; then
		printf '%s\n' "$root"
		exit 0
	fi
done

cat >&2 <<'EOF'
Unreal Engine 5.7 was not found.
Set UE_ROOT to a native Linux Unreal Engine install, then rerun.
EOF
exit 1
