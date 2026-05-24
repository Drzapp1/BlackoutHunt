#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"
build_root="$project_root/Builds/Windows"
game_exe="$build_root/BlackoutHunt/Binaries/Win64/BlackoutHunt.exe"
wine_prefix="${BLACKOUTHUNT_WINEPREFIX:-$HOME/.local/share/wineprefixes/blackouthunt}"
heroic_dxvk_root="$HOME/.var/app/com.heroicgameslauncher.hgl/config/heroic/tools/dxvk"

if ! command -v wine >/dev/null 2>&1; then
	echo "wine was not found on PATH." >&2
	exit 1
fi

if [[ ! -f "$game_exe" ]]; then
	cat >&2 <<EOF
Packaged game executable was not found:
  $game_exe

Package the Windows build first, or check that Builds/Windows is present.
EOF
	exit 1
fi

dxvk_dir="${DXVK_DIR:-}"
if [[ -z "$dxvk_dir" && -f "$heroic_dxvk_root/latest_dxvk" ]]; then
	dxvk_version="$(<"$heroic_dxvk_root/latest_dxvk")"
	dxvk_dir="$heroic_dxvk_root/$dxvk_version"
fi

if [[ -z "$dxvk_dir" && -d "$heroic_dxvk_root" ]]; then
	dxvk_dir="$(find "$heroic_dxvk_root" -maxdepth 1 -type d -name 'dxvk-*' | sort -V | tail -n 1)"
fi

if [[ -z "$dxvk_dir" || ! -f "$dxvk_dir/x64/d3d11.dll" || ! -f "$dxvk_dir/x64/dxgi.dll" ]]; then
	cat >&2 <<EOF
DXVK was not found.

Install DXVK through Heroic/Lutris, or set DXVK_DIR to a DXVK directory with x64/d3d11.dll and x64/dxgi.dll.
EOF
	exit 1
fi

mkdir -p "$wine_prefix"
if [[ ! -d "$wine_prefix/drive_c/windows/system32" ]]; then
	env WINEPREFIX="$wine_prefix" WINEARCH=win64 WINEDEBUG="${WINEDEBUG:--all}" wineboot -u
fi

install -m 0644 "$dxvk_dir/x64/d3d11.dll" "$wine_prefix/drive_c/windows/system32/d3d11.dll"
install -m 0644 "$dxvk_dir/x64/dxgi.dll" "$wine_prefix/drive_c/windows/system32/dxgi.dll"
if [[ -f "$dxvk_dir/x64/d3d10core.dll" ]]; then
	install -m 0644 "$dxvk_dir/x64/d3d10core.dll" "$wine_prefix/drive_c/windows/system32/d3d10core.dll"
fi

if [[ -d "$wine_prefix/drive_c/windows/syswow64" && -f "$dxvk_dir/x32/d3d11.dll" && -f "$dxvk_dir/x32/dxgi.dll" ]]; then
	install -m 0644 "$dxvk_dir/x32/d3d11.dll" "$wine_prefix/drive_c/windows/syswow64/d3d11.dll"
	install -m 0644 "$dxvk_dir/x32/dxgi.dll" "$wine_prefix/drive_c/windows/syswow64/dxgi.dll"
fi

runtime_input_dir="$build_root/BlackoutHunt/Config"
saved_input_dir="$build_root/BlackoutHunt/Saved/Config/Windows"
mkdir -p "$runtime_input_dir" "$saved_input_dir"

arrow_input_override='[/Script/Engine.InputSettings]
+AxisMappings=(AxisName="Turn",Scale=0.650000,Key=Right)
+AxisMappings=(AxisName="Turn",Scale=-0.650000,Key=Left)
+AxisMappings=(AxisName="LookUp",Scale=0.650000,Key=Up)
+AxisMappings=(AxisName="LookUp",Scale=-0.650000,Key=Down)
'

printf '%s\n' "$arrow_input_override" > "$runtime_input_dir/DefaultInput.ini"
printf ';METADATA=(Diff=true, UseCommands=true)\n%s\n' "$arrow_input_override" > "$saved_input_dir/Input.ini"

arrow_helper_pid=""
if [[ "${BLACKOUTHUNT_ARROW_MOUSE:-1}" != "0" && -n "${DISPLAY:-}" && -x "$script_dir/WineArrowLook.py" ]]; then
	"$script_dir/WineArrowLook.py" --parent-pid "$$" 2>"$build_root/BlackoutHunt/Saved/Logs/WineArrowLook.log" &
	arrow_helper_pid="$!"
fi

cleanup_arrow_helper() {
	if [[ -n "$arrow_helper_pid" ]]; then
		kill "$arrow_helper_pid" >/dev/null 2>&1 || true
	fi
}
trap cleanup_arrow_helper EXIT

rhi_args=(-d3d11)
for arg in "$@"; do
	case "${arg,,}" in
		-d3d11|-d3d12|-vulkan)
			rhi_args=()
			;;
	esac
done

cd "$build_root"
env \
	WINEPREFIX="$wine_prefix" \
	WINEDEBUG="${WINEDEBUG:--all}" \
	WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-d3d11,dxgi,d3d10core=n,b}" \
	DXVK_LOG_LEVEL="${DXVK_LOG_LEVEL:-warn}" \
	DXVK_LOG_PATH="${DXVK_LOG_PATH:-$build_root/BlackoutHunt/Saved/Logs}" \
	wine "$game_exe" "${rhi_args[@]}" "$@"
