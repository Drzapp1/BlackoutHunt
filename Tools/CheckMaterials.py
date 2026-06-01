"""Quick diagnostic: which materials the authoring bake references actually load."""
import unreal

def log(m):
    unreal.log("[BH MatCheck] " + str(m))

PATHS = [
    "/Game/BlackoutHunt/Art/Materials/M_BH_Concrete.M_BH_Concrete",
    "/Game/BlackoutHunt/Art/Materials/M_BH_Plaster.M_BH_Plaster",
    "/Game/BlackoutHunt/Art/Materials/M_BH_RustedMetal.M_BH_RustedMetal",
    "/Game/BlackoutHunt/Art/Materials/M_BH_DiamondPlate.M_BH_DiamondPlate",
    "/Game/BlackoutHunt/Art/Materials/M_BH_PaintedMetal.M_BH_PaintedMetal",
    "/Game/BlackoutHunt/Art/Materials/M_BH_Tiles.M_BH_Tiles",
    "/Game/BlackoutHunt/Art/Materials/M_BH_WarningSign.M_BH_WarningSign",
    "/Game/ContainersHouseCH/Materials/MI_CH_Walls_Metal.MI_CH_Walls_Metal",
    "/Game/ContainersHouseCH/StaticMesh/SM_Wall_Metal_6m.SM_Wall_Metal_6m",
]
for p in PATHS:
    exists = unreal.EditorAssetLibrary.does_asset_exist(p.split(".")[0])
    obj = unreal.load_asset(p)
    log("{:65} exists={} load={}".format(p, exists, (obj.get_class().get_name() if obj else "NULL")))

# For the wall mesh, report its material slots.
mesh = unreal.load_asset("/Game/ContainersHouseCH/StaticMesh/SM_Wall_Metal_6m.SM_Wall_Metal_6m")
if mesh:
    try:
        mats = mesh.get_editor_property("static_materials")
        for i, sm in enumerate(mats):
            mi = sm.get_editor_property("material_interface")
            log("SM_Wall_Metal_6m slot {} -> {}".format(i, (mi.get_name() if mi else "NULL")))
    except Exception as exc:
        log("mesh slot read warn: {}".format(exc))

unreal.SystemLibrary.quit_editor()
