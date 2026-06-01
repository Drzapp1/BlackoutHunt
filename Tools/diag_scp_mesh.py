"""Measure the imported SCP-096 skeletal mesh so we can pick a correct display scale.

'Nothing renders' on a mesh that loads + has its material usually means it imported at a
tiny (or gigantic) scale. Print its bounds/height and material slot count so we can set the
variant's VisualScale to land it at a sane size.
"""
import unreal

MESH = "/Game/BlackoutHunt/Art/SCP096/Skeletal/SK_SCP096"


def main():
    m = unreal.load_asset(MESH)
    if m is None:
        unreal.log_error("SCP diag: could not load %s" % MESH)
        return
    unreal.log("SCP diag: class=%s" % type(m).__name__)

    # Bounds (try a few APIs across versions).
    b = None
    for getter in ("get_bounds",):
        try:
            b = getattr(m, getter)()
            break
        except Exception as exc:
            unreal.log_warning("SCP diag: %s() failed: %s" % (getter, exc))
    if b is None:
        try:
            b = m.get_editor_property("imported_bounds")
        except Exception as exc:
            unreal.log_warning("SCP diag: imported_bounds failed: %s" % exc)

    if b is not None:
        try:
            ext = b.box_extent
            org = b.origin
            unreal.log("SCP diag: box_extent=(%.2f, %.2f, %.2f)  height(Z*2)=%.2f  sphere=%.2f  origin=(%.2f,%.2f,%.2f)"
                       % (ext.x, ext.y, ext.z, ext.z * 2.0, b.sphere_radius, org.x, org.y, org.z))
        except Exception as exc:
            unreal.log_error("SCP diag: reading bounds fields failed: %s" % exc)

    # Material slots.
    try:
        mats = m.get_editor_property("materials")
        unreal.log("SCP diag: material_slots=%d" % len(mats))
    except Exception as exc:
        unreal.log_warning("SCP diag: materials read failed: %s" % exc)


main()
