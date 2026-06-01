"""Create world-aligned (triplanar) variants of the concrete/plaster blockout materials.

The plain M_BH_Concrete / M_BH_Plaster use a fixed TextureCoordinate node (24x / 10x UV tiling). The
blockout cubes have 0..1 UVs per face regardless of actor scale, so a full-cell wall (13m x 2.7m) or a
huge floor/ceiling slab stretches each tile into thin smears -> reads as flat / untextured. These WA
variants project the textures in WORLD space (triplanar), so tiling stays consistent no matter how the
cube is scaled or rotated. New asset names (M_BH_ConcreteWA / M_BH_PlasterWA) so the existing materials
and every map that uses them are left untouched; only the backrooms maze opts in via EBHBlockMaterial.

Run headless:
  & 'D:\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe' "<proj>\\BlackoutHunt.uproject" \
      -ExecutePythonScript="<proj>\\Tools\\CreateWorldAlignedWallMaterials.py" -unattended -nop4 -nosplash -stdout -nopause
"""
import unreal

MATERIAL_DEST = "/Game/BlackoutHunt/Art/Materials"
TEXTURE_DIR = "/Game/BlackoutHunt/Art/Textures"
WORLD_ALIGNED_FUNC = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture.WorldAlignedTexture"

# Each WA material: source texture asset-id prefix + world tile size (Unreal units per texture tile).
SPECS = [
    {
        "name": "M_BH_ConcreteWA",
        "asset_id": "Concrete048",
        "tile_size": 256.0,    # ~2.5 m concrete tile -> reads as a real wall, not stretched
        "roughness": 0.90,
        "metallic": 0.0,
        # Keep the world-aligned concrete texture albedo (cracks/variation) but multiply it down to a dark,
        # grimy red so it's textured-not-monotone and not the original bright red.
        "tint": (0.52, 0.22, 0.20),
    },
    {
        "name": "M_BH_PlasterWA",
        "asset_id": "Plaster001",
        "tile_size": 320.0,    # slightly larger so the ceiling plaster is smooth, not busy
        "roughness": 0.85,
        "metallic": 0.0,
        "tint": (0.20, 0.20, 0.23),   # textured but cold and near-black ceiling
    },
    {
        "name": "M_BH_ConcreteWA_Cool",
        "asset_id": "Concrete048",
        "tile_size": 256.0,    # same concrete tiling as ConcreteWA -> reads as real concrete, not stretched
        "roughness": 0.92,
        "metallic": 0.0,
        # Same world-aligned concrete albedo (cracks/variation) but a cold gray-teal cast instead of the
        # backrooms dark-red: keeps the Substation's cold industrial palette while still being textured.
        "tint": (0.34, 0.40, 0.44),
    },
]

MAP_KEYS = ("color", "normal", "roughness", "metallic", "ao")


def log(m):
    unreal.log("[BH WA Mat] " + str(m))


def fail(m):
    unreal.log_error("[BH WA Mat] " + m)
    raise RuntimeError(m)


def tex_path(asset_id, map_key):
    name = "T_BH_{}_{}".format(asset_id, map_key)
    return "{}/{}.{}".format(TEXTURE_DIR, name, name)


def load_textures(asset_id):
    out = {}
    for key in MAP_KEYS:
        path = tex_path(asset_id, key)
        if unreal.EditorAssetLibrary.does_asset_exist(path.split(".")[0]):
            out[key] = unreal.load_asset(path)
    return out


def new_expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def connect(a, a_pin, b, b_pin):
    return unreal.MaterialEditingLibrary.connect_material_expressions(a, a_pin, b, b_pin)


def _input_names(call):
    """Exact input pin names of a MaterialFunctionCall (discovered, not guessed)."""
    names = []
    try:
        for fei in call.get_editor_property("function_inputs"):
            ei = fei.get_editor_property("expression_input")
            if ei:
                names.append(str(ei.get_editor_property("input_name")))
    except Exception as exc:
        log("input-name introspection warn: {}".format(exc))
    return names


def _output_names(call):
    names = []
    try:
        for feo in call.get_editor_property("function_outputs"):
            eo = feo.get_editor_property("expression_output")
            if eo:
                names.append(str(eo.get_editor_property("output_name")))
    except Exception as exc:
        log("output-name introspection warn: {}".format(exc))
    return names


def _pick(names, *must_have_any, **kw):
    avoid = kw.get("avoid", ())
    for n in names:
        low = n.lower()
        if any(m in low for m in must_have_any) and not any(a in low for a in avoid):
            return n
    return None


def world_aligned_sample(material, texture, tile_size, x, y):
    """A WorldAlignedTexture function call wired to `texture`; returns (call_expr, output_pin_name).

    Pin names are DISCOVERED from the loaded function (logged), not hardcoded, so a spelling like
    'TextureObject (T2d)' vs 'TextureObject' can't silently leave the required input unconnected.
    """
    func = unreal.load_asset(WORLD_ALIGNED_FUNC)
    if not func:
        return None, None

    tex_obj = new_expr(material, unreal.MaterialExpressionTextureObject, x - 320, y)
    tex_obj.set_editor_property("texture", texture)

    size = new_expr(material, unreal.MaterialExpressionConstant, x - 320, y + 150)
    size.set_editor_property("r", tile_size)

    call = new_expr(material, unreal.MaterialExpressionMaterialFunctionCall, x, y)
    call.set_editor_property("material_function", func)

    in_names = _input_names(call)
    out_names = _output_names(call)
    log("WAT inputs={} outputs={}".format(in_names, out_names))

    tex_in = _pick(in_names, "texture", avoid=("size",)) or "TextureObject"
    size_in = _pick(in_names, "size")
    out_pin = _pick(out_names, "xyz") or (out_names[0] if out_names else "")

    ok_tex = connect(tex_obj, "", call, tex_in)
    log("connect TextureObject->'{}' ok={}".format(tex_in, ok_tex))
    if size_in:
        connect(size, "", call, size_in)
    return call, out_pin


def create_constant(material, value, x, y):
    c = new_expr(material, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", value)
    return c


def build(spec):
    name = spec["name"]
    path = "{}/{}".format(MATERIAL_DEST, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.delete_asset(path):
            fail("could not delete existing " + path)

    textures = load_textures(spec["asset_id"])
    if not textures.get("color"):
        fail("{}: no color texture (T_BH_{}_color) on disk".format(name, spec["asset_id"]))

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew()
    )
    if not material:
        fail("could not create " + name)
    material.set_editor_property("use_material_attributes", False)

    if not unreal.load_asset(WORLD_ALIGNED_FUNC):
        fail("engine WorldAlignedTexture function missing at " + WORLD_ALIGNED_FUNC)

    ts = spec["tile_size"]

    # Base colour. Two modes:
    #   "tint": world-aligned texture albedo (keeps the concrete cracks/variation) * a dark colour -> textured,
    #           dark and grimy (used for the dark-red concrete).
    #   "base": a flat constant colour (used for the near-black ceiling, which needs no detail).
    tint = spec.get("tint")
    if tint:
        color_call, color_out = world_aligned_sample(material, textures["color"], ts, -340, -220)
        tint_c = new_expr(material, unreal.MaterialExpressionConstant3Vector, -340, -60)
        tint_c.set_editor_property("constant", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        mul = new_expr(material, unreal.MaterialExpressionMultiply, -120, -200)
        connect(color_call, color_out, mul, "A")
        connect(tint_c, "", mul, "B")
        unreal.MaterialEditingLibrary.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        base = spec.get("base", (0.09, 0.09, 0.10))
        base_c = new_expr(material, unreal.MaterialExpressionConstant3Vector, -150, -220)
        base_c.set_editor_property("constant", unreal.LinearColor(base[0], base[1], base[2], 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(base_c, "", unreal.MaterialProperty.MP_BASE_COLOR)

    if textures.get("normal"):
        n_call, n_out = world_aligned_sample(material, textures["normal"], ts, -150, 120)
        unreal.MaterialEditingLibrary.connect_material_property(n_call, n_out, unreal.MaterialProperty.MP_NORMAL)

    if textures.get("roughness"):
        r_call, r_out = world_aligned_sample(material, textures["roughness"], ts, -150, 420)
        unreal.MaterialEditingLibrary.connect_material_property(r_call, r_out, unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        r_const = create_constant(material, spec["roughness"], -150, 420)
        unreal.MaterialEditingLibrary.connect_material_property(r_const, "", unreal.MaterialProperty.MP_ROUGHNESS)

    if textures.get("metallic"):
        m_call, m_out = world_aligned_sample(material, textures["metallic"], ts, -150, 620)
        unreal.MaterialEditingLibrary.connect_material_property(m_call, m_out, unreal.MaterialProperty.MP_METALLIC)
    else:
        m_const = create_constant(material, spec["metallic"], -150, 620)
        unreal.MaterialEditingLibrary.connect_material_property(m_const, "", unreal.MaterialProperty.MP_METALLIC)

    if textures.get("ao"):
        ao_call, ao_out = world_aligned_sample(material, textures["ao"], ts, -150, 820)
        unreal.MaterialEditingLibrary.connect_material_property(ao_call, ao_out, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    log("built {} (tile {}u, maps: {})".format(name, ts, ",".join(sorted(textures.keys()))))


def main():
    for spec in SPECS:
        build(spec)
    unreal.EditorAssetLibrary.save_directory("/Game/BlackoutHunt/Art/Materials", only_if_is_dirty=False, recursive=False)
    log("done")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error("[BH WA Mat] FAILED: " + str(exc))
        raise
