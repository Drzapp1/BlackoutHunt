#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"
project="$project_root/BlackoutHunt.uproject"
archive="$project_root/Builds/Linux"
engine_root="${UE_ROOT:-/run/media/adamrosta/T7/UE_5.7}"
run_uat="$engine_root/Engine/Build/BatchFiles/RunUAT.bat"
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

if ! command -v wine >/dev/null 2>&1; then
	echo "wine was not found on PATH." >&2
	exit 1
fi

if [[ ! -f "$run_uat" ]]; then
	cat >&2 <<EOF
RunUAT.bat was not found:
  $run_uat

Set UE_ROOT to the Windows Unreal Engine 5.7 install and rerun.
EOF
	exit 1
fi

if [[ ! -d "$engine_root/Engine/Binaries/Linux" ]]; then
	cat >&2 <<EOF
This Unreal Engine install is missing the platform_Linux optional component:
  $engine_root

Install it with Tools/Install-Unreal-Linux-Platform.sh, or enable Linux in the
Epic Games Launcher options for UE 5.7, then rerun.
EOF
	exit 1
fi

mkdir -p "$archive"

project_win="$(winepath -w "$project")"
archive_win="$(winepath -w "$archive")"

pushd "$(dirname "$run_uat")" >/dev/null
DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 wine cmd /c RunUAT.bat BuildCookRun \
	"-project=$project_win" \
	-notinstalledengine \
	-noP4 \
	-platform=Linux \
	-clientconfig="$configuration" \
	-serverconfig="$configuration" \
	-cook \
	"-map=/Engine/Maps/Entry" \
	-build \
	-nocompileeditor \
	-noxge \
	"-ubtargs=-NoXGE -MaxParallelActions=2" \
	-stage \
	-pak \
	-archive \
	"-archivedirectory=$archive_win" \
	"${uat_extra_args[@]}"
popd >/dev/null
