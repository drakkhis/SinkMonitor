"""
Headless Unreal Editor 5.6.1 Python script that imports SM_SinkMonitorTerminal.fbx,
textures, and 2D studio icons into /SinkMonitor/..., constructs the 5 PBR materials,
assigns material slots + lightmap UVs + LODs, verifies bounds/collisions/materials,
and saves every .uasset under SinkMonitor/Content/.
"""

import sys
from pathlib import Path
import unreal


PROJECT_ROOT = Path("/mnt/Workspace/Antigravity/SatisfactorySinkMonitor")
BLENDER_DIR = PROJECT_ROOT / "assets" / "blender"

FBX_PATH = BLENDER_DIR / "SM_SinkMonitorTerminal.fbx"
TEX_SCREEN_PATH = BLENDER_DIR / "T_SinkMonitor_Screen_Emissive.png"
TEX_HAZARD_PATH = BLENDER_DIR / "T_SinkMonitor_Hazard.png"
ICON_256_PATH = BLENDER_DIR / "T_Icon_SinkMonitor_256.png"
ICON_512_PATH = BLENDER_DIR / "T_Icon_SinkMonitor_512.png"


def srgb_hex_to_linear_color(hex_str: str) -> unreal.LinearColor:
    h = hex_str.lstrip("#")
    r, g, b = (int(h[i : i + 2], 16) / 255.0 for i in (0, 2, 4))
    return unreal.LinearColor(pow(r, 2.2), pow(g, 2.2), pow(b, 2.2), 1.0)


def import_texture(src_file: Path, dest_folder: str, asset_name: str, is_ui_icon: bool = False) -> unreal.Texture2D:
    if not src_file.exists():
        raise FileNotFoundError(f"Missing source texture: {src_file}")

    task = unreal.AssetImportTask()
    task.filename = str(src_file)
    task.destination_path = dest_folder
    task.destination_name = asset_name
    task.replace_existing = True
    task.replace_existing_settings = True
    task.automated = True
    task.save = False

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset_path = f"{dest_folder}/{asset_name}"
    imported = task.get_objects()
    tex = imported[0] if imported else unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(tex, unreal.Texture2D):
        raise RuntimeError(f"Failed to load imported Texture2D at {asset_path}")

    tex.set_editor_property("srgb", True)
    if is_ui_icon:
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)

    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    unreal.log(f"[SinkMonitor] Imported Texture2D: {asset_path} (is_ui_icon={is_ui_icon})")
    return tex


def get_or_create_material(dest_folder: str, mat_name: str) -> unreal.Material:
    asset_path = f"{dest_folder}/{mat_name}.{mat_name}"
    mat = unreal.load_asset(asset_path)
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name=mat_name,
            package_path=dest_folder,
            asset_class=unreal.Material,
            factory=unreal.MaterialFactoryNew(),
        )
    if not isinstance(mat, unreal.Material):
        raise RuntimeError(f"Failed to create/load Material: {asset_path}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    return mat


def make_scalar_const(mat: unreal.Material, value: float, x: int, y: int) -> unreal.MaterialExpressionConstant:
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant, x, y
    )
    expr.set_editor_property("r", float(value))
    return expr


def make_color_const(mat: unreal.Material, hex_str: str, x: int, y: int) -> unreal.MaterialExpressionConstant3Vector:
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, x, y
    )
    expr.set_editor_property("constant", srgb_hex_to_linear_color(hex_str))
    return expr


def finalize_material(mat: unreal.Material) -> unreal.Material:
    unreal.MaterialEditingLibrary.layout_material_expressions(mat)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"[SinkMonitor] Compiled & saved Material: {mat.get_path_name()}")
    return mat


def build_solid_pbr_material(
    dest_folder: str,
    mat_name: str,
    hex_color: str,
    metallic: float,
    roughness: float,
) -> unreal.Material:
    mat = get_or_create_material(dest_folder, mat_name)
    c_node = make_color_const(mat, hex_color, -320, -160)
    m_node = make_scalar_const(mat, metallic, -320, 40)
    r_node = make_scalar_const(mat, roughness, -320, 160)

    mel = unreal.MaterialEditingLibrary
    mel.connect_material_property(c_node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(m_node, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(r_node, "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finalize_material(mat)


def build_hazard_material(dest_folder: str, hazard_tex: unreal.Texture2D) -> unreal.Material:
    mat = get_or_create_material(dest_folder, "M_SinkMonitor_Hazard")
    mel = unreal.MaterialEditingLibrary

    tex_node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, -160)
    tex_node.set_editor_property("texture", hazard_tex)
    m_node = make_scalar_const(mat, 0.10, -320, 80)
    r_node = make_scalar_const(mat, 0.45, -320, 200)

    mel.connect_material_property(tex_node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(m_node, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(r_node, "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finalize_material(mat)


def build_screen_material(dest_folder: str, screen_tex: unreal.Texture2D) -> unreal.Material:
    mat = get_or_create_material(dest_folder, "M_SinkMonitor_Screen")
    mel = unreal.MaterialEditingLibrary

    c_node = make_color_const(mat, "#05080D", -320, -240)
    m_node = make_scalar_const(mat, 0.05, -320, -40)
    s_node = make_scalar_const(mat, 0.70, -320, 80)
    r_node = make_scalar_const(mat, 0.12, -320, 200)

    tex_node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -560, 340)
    tex_node.set_editor_property("texture", screen_tex)
    mul_node = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -280, 340)
    mul_node.set_editor_property("const_b", 1.8)

    mel.connect_material_expressions(tex_node, "", mul_node, "A")
    mel.connect_material_property(c_node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(m_node, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(s_node, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(r_node, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(mul_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finalize_material(mat)


def import_terminal_mesh(materials_by_name: dict[str, unreal.Material]) -> unreal.StaticMesh:
    if not FBX_PATH.exists():
        raise FileNotFoundError(f"Missing FBX: {FBX_PATH}")

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)

    sm_data = options.get_editor_property("static_mesh_import_data")
    sm_data.set_editor_property("static_mesh_lod_group", "LargeProp")
    sm_data.set_editor_property("combine_meshes", True)
    sm_data.set_editor_property("auto_generate_collision", False)
    sm_data.set_editor_property("one_convex_hull_per_ucx", True)
    sm_data.set_editor_property("generate_lightmap_u_vs", False)
    sm_data.set_editor_property("convert_scene_unit", False)
    sm_data.set_editor_property("import_uniform_scale", 100.0)
    sm_data.set_editor_property("remove_degenerates", True)

    dest_folder = "/SinkMonitor/Buildable/Mesh"
    asset_name = "SM_SinkMonitorTerminal"

    task = unreal.AssetImportTask()
    task.filename = str(FBX_PATH)
    task.destination_path = dest_folder
    task.destination_name = asset_name
    task.replace_existing = True
    task.replace_existing_settings = True
    task.automated = True
    task.save = False
    task.options = options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{dest_folder}/{asset_name}"
    mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"Failed to load imported StaticMesh at {asset_path}")

    # Assign our 5 compiled PBR materials matching each FBX material slot name.
    static_materials = mesh.get_editor_property("static_materials")
    unreal.log(f"[SinkMonitor] Found {len(static_materials)} static material slots on {asset_name}:")
    for idx, slot in enumerate(static_materials):
        slot_name = str(slot.material_slot_name)
        matched_mat = materials_by_name.get(slot_name)
        if matched_mat is None:
            for key, mat_candidate in materials_by_name.items():
                if key.lower() in slot_name.lower() or slot_name.lower() in key.lower():
                    matched_mat = mat_candidate
                    break
        if matched_mat is None:
            raise RuntimeError(f"Unmatched FBX material slot '{slot_name}' at index {idx}")
        mesh.set_material(idx, matched_mat)
        unreal.log(f"  slot[{idx}] '{slot_name}' -> {matched_mat.get_path_name()}")

    mesh.set_editor_property("light_map_coordinate_index", 1)
    mesh.set_editor_property("light_map_resolution", 128)

    # Configure LODs if StaticMeshEditorSubsystem is active (full Editor), otherwise LargeProp LODGroup builds 4 LODs automatically.
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if sm_sub is not None:
        lod_settings = []
        for pct, screen_sz in ((1.0, 1.0), (0.5, 0.35), (0.2, 0.12)):
            s = unreal.StaticMeshReductionSettings()
            s.percent_triangles = pct
            s.screen_size = screen_sz
            lod_settings.append(s)

        red_opts = unreal.StaticMeshReductionOptions()
        red_opts.auto_compute_lod_screen_size = True
        red_opts.reduction_settings = lod_settings
        lod_count = sm_sub.set_lods(mesh, red_opts)
        unreal.log(f"[SinkMonitor] StaticMeshEditorSubsystem set_lods -> {lod_count}")
    else:
        unreal.log(f"[SinkMonitor] Commandlet mode: LargeProp LODGroup produced {mesh.get_num_lods()} LOD(s)")

    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh


def verify_imported_mesh(mesh: unreal.StaticMesh) -> None:
    bbox = mesh.get_bounding_box()
    size = bbox.max - bbox.min
    lod_count = mesh.get_num_lods()
    static_materials = mesh.get_editor_property("static_materials")

    body_setup = mesh.get_editor_property("body_setup")
    agg_geom = body_setup.get_editor_property("agg_geom") if body_setup else None
    convex_cols = len(agg_geom.get_editor_property("convex_elems")) if agg_geom else 0
    box_cols = len(agg_geom.get_editor_property("box_elems")) if agg_geom else 0
    sphere_cols = len(agg_geom.get_editor_property("sphere_elems")) if agg_geom else 0
    capsule_cols = len(agg_geom.get_editor_property("sphyl_elems")) if agg_geom else 0
    simple_cols = convex_cols + box_cols + sphere_cols + capsule_cols

    unreal.log(
        f"[SinkMonitor] VERIFY StaticMesh '{mesh.get_name()}': "
        f"BoxMin=({bbox.min.x:.2f}, {bbox.min.y:.2f}, {bbox.min.z:.2f}) cm, "
        f"BoxMax=({bbox.max.x:.2f}, {bbox.max.y:.2f}, {bbox.max.z:.2f}) cm, "
        f"Size=({size.x:.2f} x {size.y:.2f} x {size.z:.2f}) cm, "
        f"LODs={lod_count}, SimpleCols={simple_cols}, ConvexCols={convex_cols}, "
        f"MaterialSlots={len(static_materials)}"
    )

    if len(static_materials) != 5:
        raise RuntimeError(f"Expected 5 material slots, found {len(static_materials)}")
    for idx, slot in enumerate(static_materials):
        if slot.material_interface is None:
            raise RuntimeError(f"Material slot {idx} ('{slot.material_slot_name}') is None!")
    if simple_cols < 3 or convex_cols < 3:
        raise RuntimeError(f"Expected 3 UCX convex collisions, got simple={simple_cols}, convex={convex_cols}")
    if lod_count < 3:
        raise RuntimeError(f"Expected >= 3 LODs, got {lod_count}")
    if size.z < 200.0 or size.z > 250.0:
        raise RuntimeError(f"Unexpected UE mesh height {size.z:.2f} cm (expected ~225 cm)")


def main() -> None:
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/SinkMonitor"], True)
    tex_dest = "/SinkMonitor/Buildable/Textures"
    mat_dest = "/SinkMonitor/Buildable/Materials"
    icon_dest = "/SinkMonitor/UI/Icons"

    tex_screen = import_texture(TEX_SCREEN_PATH, tex_dest, "T_SinkMonitor_Screen_Emissive", is_ui_icon=False)
    tex_hazard = import_texture(TEX_HAZARD_PATH, tex_dest, "T_SinkMonitor_Hazard", is_ui_icon=False)
    import_texture(ICON_256_PATH, icon_dest, "T_Icon_SinkMonitor_256", is_ui_icon=True)
    import_texture(ICON_512_PATH, icon_dest, "T_Icon_SinkMonitor_512", is_ui_icon=True)

    materials = {
        "M_SinkMonitor_FicsitOrange": build_solid_pbr_material(
            mat_dest, "M_SinkMonitor_FicsitOrange", "#E67E22", metallic=0.15, roughness=0.42
        ),
        "M_SinkMonitor_Gunmetal": build_solid_pbr_material(
            mat_dest, "M_SinkMonitor_Gunmetal", "#252A30", metallic=0.85, roughness=0.48
        ),
        "M_SinkMonitor_BrushedMetal": build_solid_pbr_material(
            mat_dest, "M_SinkMonitor_BrushedMetal", "#9BA3AB", metallic=0.92, roughness=0.30
        ),
        "M_SinkMonitor_Hazard": build_hazard_material(mat_dest, tex_hazard),
        "M_SinkMonitor_Screen": build_screen_material(mat_dest, tex_screen),
    }

    mesh = import_terminal_mesh(materials)
    verify_imported_mesh(mesh)
    unreal.EditorAssetLibrary.save_directory("/SinkMonitor", only_if_is_dirty=False, recursive=True)
    unreal.log("[SinkMonitor] All UE5 assets imported, compiled, verified, and saved cleanly.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"[SinkMonitor] Asset import failed: {exc}")
        sys.exit(1)
