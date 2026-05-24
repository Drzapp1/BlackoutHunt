#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"
project="$project_root/BlackoutHunt.uproject"
archive="$project_root/Builds/Linux"
engine_root="$("$script_dir/Find-Unreal.sh")"
run_uat="$engine_root/Engine/Build/BatchFiles/RunUAT.sh"
configuration="${CONFIGURATION:-Development}"
uat_extra_args=()

if [[ "${CLASSROOM:-0}" == "1" ]]; then
	configuration="${CONFIGURATION:-Shipping}"
	uat_extra_args+=("-distribution")
	uat_extra_args+=("-nodebuginfo")
	case "$(realpath "$archive")" in
		"$(realpath "$project_root")"/*) rm -rf "$archive" ;;
		*) echo "Refusing to clean archive outside project root: $archive" >&2; exit 1 ;;
	esac
fi

if [[ ! -f "$engine_root/Engine/Build/BatchFiles/Linux/SetupEnvironment.sh" ]]; then
	cat >&2 <<EOF
This Unreal Engine install is missing native Linux host build files:
  $engine_root

Install a native Linux Unreal Engine 5.7 build, or run Tools\\Package-Linux.ps1
from Windows after enabling the Linux optional component in the Epic Games Launcher.
EOF
	exit 1
fi

mkdir -p "$archive"

pushd "$(dirname "$run_uat")" >/dev/null
bash "$run_uat" BuildCookRun \
	"-project=$project" \
	-notinstalledengine \
	-noP4 \
	-platform=Linux \
	-clientconfig="$configuration" \
	-serverconfig="$configuration" \
	-cook \
	"-map=/Engine/Maps/Entry" \
	-build \
	-noxge \
	"-ubtargs=-NoXGE -MaxParallelActions=2" \
	-stage \
	-pak \
	-archive \
	"-archivedirectory=$archive" \
	"${uat_extra_args[@]}"
popd >/dev/null
