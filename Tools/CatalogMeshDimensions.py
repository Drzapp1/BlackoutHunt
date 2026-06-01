"""Catalog static-mesh dimensions/pivots for the BlackoutHunt authored-map kits.

Runs headless in the editor and dumps a JSON catalog of every UStaticMesh under the
candidate facility kits (ContainersHouseCH modular shell + SmartBasicInterfaces props),
so the C++ authoring-bake can tile modular wall/floor/ceiling panels edge-to-edge using
real dimensions + pivot offsets instead of guessing.

Run:
  & "D:\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "D:\\BlackoutHunt\\BlackoutHunt.uproject" `
      -ExecutePythonScript="D:\\BlackoutHunt\\Tools\\CatalogMeshDimensions.py" `
      -unattended -nop4 -nosplash -stdout -nopause

Output: Tools/MeshCatalog.json
"""

import json
import os

import unreal


# Roots to introspect. SmartBasicInterfaces is already cooked + wired; ContainersHouseCH is
# the modular industrial shell candidate (not yet cooked). Engine basic shapes included so the
# authoring-bake knows the exact cube size it scales blockout geometry from.
ROOTS = [
    "/Game/ContainersHouseCH/StaticMesh",
    "/Game/SmartBasicInterfaces/Meshes",
    "/Game/SmartBasicInterfaces/Demo/SCContent/Props",
    "/Engine/BasicShapes",
]

OUT_PATH = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
    "Tools",
    "MeshCatalog.json",
)


def log(msg):
    unreal.log("[BH MeshCatalog] " + str(msg))


def fail(msg):
    unreal.log_error("[BH MeshCatalog] " + str(msg))
    raise RuntimeError(msg)


def catalog_root(eal, root, entries):
    if not eal.does_directory_exist(root):
        log("SKIP (missing): " + root)
        return
    paths = eal.list_assets(root, recursive=True, include_folder=False)
    found = 0
    for path in paths:
        # list_assets returns "/Game/.../Name.Name"; load_asset wants the object path.
        asset = eal.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        try:
            b = asset.get_bounds()              # unreal.BoxSphereBounds
            ext = b.box_extent                  # HALF-extent, cm
            org = b.origin                       # geometric center relative to pivot
            aabb = asset.get_bounding_box()     # unreal.Box -> .min/.max in local space
            bmin, bmax = aabb.min, aabb.max
            entries.append({
                "asset_path": path,
                "name": asset.get_name(),
                "root": root,
                "size_cm": [round(ext.x * 2.0, 2), round(ext.y * 2.0, 2), round(ext.z * 2.0, 2)],
                "half_extent": [round(ext.x, 2), round(ext.y, 2), round(ext.z, 2)],
                "origin": [round(org.x, 2), round(org.y, 2), round(org.z, 2)],
                "bbox_min": [round(bmin.x, 2), round(bmin.y, 2), round(bmin.z, 2)],
                "bbox_max": [round(bmax.x, 2), round(bmax.y, 2), round(bmax.z, 2)],
                "radius_cm": round(b.sphere_radius, 2),
                # Z offset to seat the mesh flush on a floor plane at Z=0 (spawn at this Z).
                "floor_seat_z": round(-bmin.z, 2),
            })
            found += 1
        except Exception as exc:  # noqa: BLE001 - want to keep cataloguing the rest
            unreal.log_warning("[BH MeshCatalog] bounds failed for {}: {}".format(path, exc))
    log("{}: {} static meshes".format(root, found))


def main():
    eal = unreal.EditorAssetLibrary
    entries = []
    for root in ROOTS:
        catalog_root(eal, root, entries)
    entries.sort(key=lambda e: e["asset_path"])
    with open(OUT_PATH, "w", encoding="utf-8") as f:
        json.dump({"mesh_count": len(entries), "meshes": entries}, f, indent=2)
    log("Wrote {} meshes -> {}".format(len(entries), OUT_PATH))


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("[BH MeshCatalog] FAILED: " + str(exc))
finally:
    unreal.SystemLibrary.quit_editor()
