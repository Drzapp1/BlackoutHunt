#!/usr/bin/env bash
set -euo pipefail

engine_root="${UE_ROOT:-/run/media/adamrosta/T7/UE_5.7}"
drive_root="$(cd "$engine_root/.." && pwd)"
manifest="$engine_root/.egstore/163F87FE4D191FF7548452B8422DBE57.manifest"

if [[ ! -f "$manifest" ]]; then
	cat >&2 <<EOF
UE_5.7 Epic manifest was not found:
  $manifest

Set UE_ROOT to the Epic-installed Unreal Engine 5.7 directory and rerun.
EOF
	exit 1
fi

if command -v legendary >/dev/null 2>&1; then
	legendary_cmd=(legendary)
elif flatpak info com.heroicgameslauncher.hgl >/dev/null 2>&1; then
	legendary_cmd=(
		flatpak run
		"--filesystem=$drive_root"
		--command=/app/bin/heroic/resources/app.asar.unpacked/build/bin/x64/linux/legendary
		com.heroicgameslauncher.hgl
	)
else
	cat >&2 <<'EOF'
Legendary was not found, and Heroic Games Launcher is not installed as a Flatpak.
Install Heroic or legendary to inspect the Epic manifest.
EOF
	exit 1
fi

file_list="$(mktemp)"
missing_list="$(mktemp)"
trap 'rm -f "$file_list" "$missing_list"' EXIT

"${legendary_cmd[@]}" list-files \
	--manifest "$manifest" \
	--install-tag platform_Linux \
	--tsv > "$file_list"

total_files=0
present_files=0
missing_files=0
total_bytes=0
missing_bytes=0

while IFS=$'\t' read -r path _hash size _tags; do
	[[ "$path" == "path" ]] && continue
	[[ -z "${path:-}" ]] && continue
	total_files=$((total_files + 1))
	total_bytes=$((total_bytes + size))
	if [[ -e "$engine_root/$path" ]]; then
		present_files=$((present_files + 1))
	else
		missing_files=$((missing_files + 1))
		missing_bytes=$((missing_bytes + size))
		printf '%s\n' "$path" >> "$missing_list"
	fi
done < "$file_list"

printf 'UE root: %s\n' "$engine_root"
printf 'Manifest: %s\n' "$manifest"
printf 'platform_Linux files: %d total, %d present, %d missing\n' "$total_files" "$present_files" "$missing_files"
printf 'platform_Linux size: %.2f GiB total, %.2f GiB missing\n' \
	"$(awk "BEGIN { print $total_bytes / 1024 / 1024 / 1024 }")" \
	"$(awk "BEGIN { print $missing_bytes / 1024 / 1024 / 1024 }")"

if [[ -d "$engine_root/Engine/Binaries/Linux" ]]; then
	echo "Windows UE Linux target binaries: present"
else
	echo "Windows UE Linux target binaries: missing"
fi

if [[ -f "$engine_root/Engine/Build/BatchFiles/Linux/SetupEnvironment.sh" ]]; then
	echo "Native Linux host build files: present"
else
	echo "Native Linux host build files: missing"
fi

if (( missing_files > 0 )); then
	echo
	echo "First missing platform_Linux files:"
	sed -n '1,20p' "$missing_list"
	echo
	echo "Next step when Epic access is available:"
	echo "  ./Tools/Install-Unreal-Linux-Platform.sh"
	exit 1
fi

echo
echo "platform_Linux is installed. You can package through Wine with:"
echo "  ./Tools/Package-Linux-Wine.sh"
