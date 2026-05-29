#!/usr/bin/env bash
set -euo pipefail

configuration="${1:-Shipping}"
case "$configuration" in
	Development|Shipping) ;;
	*)
		echo "Usage: $0 [Development|Shipping]" >&2
		exit 2
		;;
esac

if [[ "$(uname -s)" != "Darwin" ]]; then
	echo "Mac packaging must run on macOS with Xcode installed." >&2
	exit 1
fi

if [[ -z "${UE_ROOT:-}" ]]; then
	echo "UE_ROOT must point to a macOS Unreal Engine 5.7 install." >&2
	echo "Example: UE_ROOT=\"/Users/Shared/Epic Games/UE_5.7\" $0 $configuration" >&2
	exit 1
fi

if ! developer_dir="$(xcode-select -p 2>/dev/null)"; then
	echo "Xcode command line tools are not configured. Install Xcode, then run: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
	exit 1
fi

if ! xcode_version_output="$(xcodebuild -version 2>/dev/null)"; then
	echo "xcodebuild is unavailable. Install Xcode 15.4 or newer and accept the license." >&2
	exit 1
fi

xcode_version="$(printf '%s\n' "$xcode_version_output" | awk '/^Xcode / { print $2; exit }')"
xcode_major="${xcode_version%%.*}"
xcode_minor="${xcode_version#*.}"
xcode_minor="${xcode_minor%%.*}"
if [[ -z "$xcode_version" ]]; then
	echo "Could not determine Xcode version from xcodebuild output." >&2
	exit 1
fi
if (( xcode_major < 15 || (xcode_major == 15 && xcode_minor < 4) )); then
	echo "Xcode 15.4 or newer is required; found: ${xcode_version:-unknown}" >&2
	exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"
project="$project_root/BlackoutHunt.uproject"
archive="$project_root/Builds/Mac"
package_ddc="$project_root/Builds/DerivedDataCache-Mac"
log_dir="$archive/Logs"
timestamp="$(date -u +%Y%m%d-%H%M%S)"
log_path="$log_dir/package-mac-$configuration-$timestamp.log"
ue_root="$UE_ROOT"
run_uat="$ue_root/Engine/Build/BatchFiles/RunUAT.sh"

if [[ ! -x "$run_uat" ]]; then
	echo "RunUAT.sh was not found at: $run_uat" >&2
	echo "Set UE_ROOT to a macOS Unreal Engine 5.7 install and rerun." >&2
	exit 1
fi

mkdir -p "$archive" "$package_ddc" "$log_dir"

echo "Using Xcode $xcode_version at $developer_dir"
echo "Using Unreal Engine at $ue_root"
echo "Writing package log to $log_path"

uat_args=(
	BuildCookRun
	-project="$project"
	-notinstalledengine
	-noP4
	-platform=Mac
	-clientconfig="$configuration"
	-serverconfig="$configuration"
	-cook
	-ddc=InstalledNoZenLocalFallback
	-map=/Engine/Maps/Entry
	-build
	-stage
	-pak
	-createappbundle
	-archive
	-archivedirectory="$archive"
)

env "UE-LocalDataCachePath=$package_ddc" "$run_uat" "${uat_args[@]}" 2>&1 | tee "$log_path"

app_path="$(find "$archive" -maxdepth 5 -type d -name 'BlackoutHunt.app' | head -n 1)"
if [[ -z "$app_path" ]]; then
	echo "Mac package completed, but BlackoutHunt.app was not found under: $archive" >&2
	exit 1
fi

zip_path="$archive/BlackoutHunt-Mac-$configuration-$timestamp.zip"
ditto -c -k --sequesterRsrc --keepParent "$app_path" "$zip_path"
shasum -a 256 "$zip_path" > "$zip_path.sha256"

echo "Mac package written to: $archive"
echo "Mac app: $app_path"
echo "Archive: $zip_path"
echo "SHA-256: $zip_path.sha256"
