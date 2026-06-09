import unreal
lines = []
for name in ["SM_BH_Telescope","SM_BH_RoofAntenna","SM_BH_RoofRocks","SM_BH_RoofMeteor","SM_BH_RoofStool"]:
    p = "/Game/BlackoutHunt/Art/Roof/{0}.{0}".format(name)
    a = unreal.load_asset(p)
    if not a:
        lines.append(name + ": MISSING"); continue
    try:
        bb = a.get_bounding_box()
        sz = bb.max - bb.min
        lines.append("{0}: {1:.1f} x {2:.1f} x {3:.1f} cm".format(name, sz.x, sz.y, sz.z))
    except Exception as e:
        lines.append("{0}: ERR {1}".format(name, e))
with open(r"D:\BlackoutHunt\Saved\roof_bounds.txt", "w") as f:
    f.write("\n".join(lines))
unreal.SystemLibrary.quit_editor()
