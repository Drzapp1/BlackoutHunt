#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"
project="$project_root/BlackoutHunt.uproject"
archive="$project_root/Builds/Linux"
# Persistent local DDC shared with the Windows package flow. Keeping cooked
# shaders on disk between runs turns a cold ~250s shader compile into a warm
# cook, which is the single biggest repeat-cook speedup on this machine.
package_ddc="${BLACKOUTHUNT_DDC:-$project_root/Builds/DerivedDataCache}"
engine_root="${UE_ROOT:-/run/media/adamrosta/T7/UE_5.7}"
run_uat="$engine_root/Engine/Build/BatchFiles/RunUAT.bat"
configuration="${CONFIGURATION:-Development}"
toolchain_root="${LINUX_MULTIARCH_ROOT:-/run/media/adamrosta/T7/UnrealToolchains/v26_clang-20.1.8-rockylinux8}"
# Compile parallelism for UnrealBuildTool. Defaults to the core count; lower it
# (BUILD_PARALLEL=2) on memory-constrained hosts where parallel clang actions
# risk OOM, or raise it on bigger machines.
build_parallel="${BUILD_PARALLEL:-$(nproc)}"
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

# Opt-in iterative cook: re-cooks only assets that changed since the last cook
# in this DDC/cooked output. Big speedup for repeat dev cooks; leave off for
# release/distribution packages where a clean cook is preferred.
if [[ "${ITERATE:-0}" == "1" ]]; then
	uat_extra_args+=("-iterate")
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

if [[ ! -f "$toolchain_root/ToolchainVersion.txt" ]]; then
	cat >&2 <<EOF
The Unreal Linux cross-toolchain was not found:
  $toolchain_root

Install the Unreal Linux toolchain, or set LINUX_MULTIARCH_ROOT to the v26
toolchain root, then rerun.
EOF
	exit 1
fi

mkdir -p "$archive"
mkdir -p "$package_ddc"

project_win="$(winepath -w "$project")"
archive_win="$(winepath -w "$archive")"
toolchain_win="$(winepath -w "$toolchain_root")"
ddc_win="$(winepath -w "$package_ddc")"

pushd "$(dirname "$run_uat")" >/dev/null
env \
	LINUX_MULTIARCH_ROOT="$toolchain_win" \
	DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 \
	"UE-LocalDataCachePath=$ddc_win" \
	wine cmd /c RunUAT.bat BuildCookRun \
	"-project=$project_win" \
	-notinstalledengine \
	-noP4 \
	-platform=Linux \
	-clientconfig="$configuration" \
	-serverconfig="$configuration" \
	-cook \
	-ddc=InstalledNoZenLocalFallback \
	"-map=/Engine/Maps/Entry+/Game/BlackoutHunt/Maps/Facility+/Game/BlackoutHunt/Maps/Substation+/Game/BlackoutHunt/Maps/Foggrounds+/Game/BlackoutHunt/Maps/Tutorial" \
	-build \
	-nocompileeditor \
	-noxge \
	"-ubtargs=-NoXGE -MaxParallelActions=$build_parallel" \
	-stage \
	-pak \
	-archive \
	"-archivedirectory=$archive_win" \
	"${uat_extra_args[@]}"
popd >/dev/null
