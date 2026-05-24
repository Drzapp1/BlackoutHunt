#!/usr/bin/env bash
set -euo pipefail

engine_root="${UE_ROOT:-/run/media/adamrosta/T7/UE_5.7}"
drive_root="$(cd "$engine_root/.." && pwd)"
manifest="$engine_root/.egstore/163F87FE4D191FF7548452B8422DBE57.manifest"
legendary_config="${LEGENDARY_CONFIG_PATH:-$HOME/.var/app/com.heroicgameslauncher.hgl/config/heroic/legendaryConfig/legendary}"

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
		"--env=LEGENDARY_CONFIG_PATH=$legendary_config"
		--command=/app/bin/heroic/resources/app.asar.unpacked/build/bin/x64/linux/legendary
		com.heroicgameslauncher.hgl
	)
else
	cat >&2 <<'EOF'
Legendary was not found, and Heroic Games Launcher is not installed as a Flatpak.
Install Heroic or legendary, then rerun.
EOF
	exit 1
fi

status="$("${legendary_cmd[@]}" status 2>&1 || true)"
if grep -q "Epic account: <not logged in>" <<<"$status"; then
	cat >&2 <<EOF
Legendary is not logged in to Epic.

Run this, complete the Epic login, then rerun this script:
  flatpak run --filesystem=$drive_root --env=LEGENDARY_CONFIG_PATH=$legendary_config --command=/app/bin/heroic/resources/app.asar.unpacked/build/bin/x64/linux/legendary com.heroicgameslauncher.hgl auth --disable-webview

No Epic password is needed by this script; Legendary stores the auth token locally.
EOF
	exit 1
fi

"${legendary_cmd[@]}" import UE_5.7 "$engine_root" --disable-check --platform Windows || true
"${legendary_cmd[@]}" install UE_5.7 \
	--repair \
	--manifest "$manifest" \
	--install-tag platform_Linux \
	--base-path "$drive_root" \
	--game-folder "$(basename "$engine_root")" \
	--platform Windows \
	--yes

if [[ ! -d "$engine_root/Engine/Binaries/Linux" ]]; then
	echo "platform_Linux install completed, but Engine/Binaries/Linux is still missing." >&2
	exit 1
fi

echo "Unreal Engine platform_Linux component is installed."
