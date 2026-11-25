#!/usr/bin/env python3
"""
Procedurally builds SM_SinkMonitorTerminal (mesh, 5 PBR materials, UVs, 3 UCX_ convex collisions),
exports SM_SinkMonitorTerminal.fbx, saves SM_SinkMonitorTerminal.blend, and renders studio icons
and a 4-view turnaround sheet via Cycles in Blender 5.3.0.
"""

from __future__ import annotations

import math
from pathlib import Path

import bpy
import bmesh
from mathutils import Euler, Matrix, Vector
from PIL import Image, ImageDraw, ImageFont


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
DOCS_DIR = PROJECT_ROOT / "docs"
RENDERS_DIR = DOCS_DIR / "renders"
RESOURCES_DIR = PROJECT_ROOT / "SinkMonitor" / "Resources"

TEX_SCREEN_PATH = SCRIPT_DIR / "T_SinkMonitor_Screen_Emissive.png"
TEX_HAZARD_PATH = SCRIPT_DIR / "T_SinkMonitor_Hazard.png"
BLEND_PATH = SCRIPT_DIR / "SM_SinkMonitorTerminal.blend"
FBX_PATH = SCRIPT_DIR / "SM_SinkMonitorTerminal.fbx"
ICON_512_PATH = SCRIPT_DIR / "T_Icon_SinkMonitor_512.png"
ICON_256_PATH = SCRIPT_DIR / "T_Icon_SinkMonitor_256.png"
ICON_128_PATH = RESOURCES_DIR / "Icon128.png"
SMR_LOGO_PATH = DOCS_DIR / "SMR_Logo.png"
TURNAROUND_PATH = RENDERS_DIR / "terminal_turnaround.png"
FONT_BOLD = "/usr/share/fonts/liberation/LiberationSans-Bold.ttf"


def srgb_to_linear(hex_str: str) -> tuple[float, float, float, float]:
    h = hex_str.lstrip("#")
    vals = [int(h[i : i + 2], 16) / 255.0 for i in (0, 2, 4)]
    lin = [pow(c, 2.2) for c in vals]
    return (lin[0], lin[1], lin[2], 1.0)


def clear_scene() -> None:
    bpy.ops.wm.read_homefile(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0


def create_materials() -> dict[str, bpy.types.Material]:
    mats: dict[str, bpy.types.Material] = {}

    def make_pbr(name: str, hex_color: str, metallic: float, roughness: float) -> bpy.types.Material:
        mat = bpy.data.materials.new(name=name)
        if mat.node_tree is None:
            mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = srgb_to_linear(hex_color)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        mats[name] = mat
        return mat

    make_pbr("M_SinkMonitor_FicsitOrange", "#E67E22", metallic=0.15, roughness=0.42)
    make_pbr("M_SinkMonitor_Gunmetal", "#252A30", metallic=0.85, roughness=0.48)
    make_pbr("M_SinkMonitor_BrushedMetal", "#9BA3AB", metallic=0.92, roughness=0.30)

    # Hazard stripe material
    mat_hazard = bpy.data.materials.new(name="M_SinkMonitor_Hazard")
    if mat_hazard.node_tree is None:
        mat_hazard.use_nodes = True
    nt = mat_hazard.node_tree
    bsdf_h = nt.nodes.get("Principled BSDF")
    bsdf_h.inputs["Metallic"].default_value = 0.10
    bsdf_h.inputs["Roughness"].default_value = 0.45
    tex_h = nt.nodes.new("ShaderNodeTexImage")
    tex_h.image = bpy.data.images.load(str(TEX_HAZARD_PATH), check_existing=True)
    tex_h.extension = "REPEAT"
    nt.links.new(tex_h.outputs["Color"], bsdf_h.inputs["Base Color"])
    mats["M_SinkMonitor_Hazard"] = mat_hazard

    # Emissive telemetry screen material
    mat_screen = bpy.data.materials.new(name="M_SinkMonitor_Screen")
    if mat_screen.node_tree is None:
        mat_screen.use_nodes = True
    nt_s = mat_screen.node_tree
    bsdf_s = nt_s.nodes.get("Principled BSDF")
    bsdf_s.inputs["Base Color"].default_value = srgb_to_linear("#05080D")
    bsdf_s.inputs["Metallic"].default_value = 0.05
    bsdf_s.inputs["Roughness"].default_value = 0.12
    tex_s = nt_s.nodes.new("ShaderNodeTexImage")
    tex_s.image = bpy.data.images.load(str(TEX_SCREEN_PATH), check_existing=True)
    tex_s.extension = "EXTEND"
    nt_s.links.new(tex_s.outputs["Color"], bsdf_s.inputs["Emission Color"])
    bsdf_s.inputs["Emission Strength"].default_value = 1.65
    mats["M_SinkMonitor_Screen"] = mat_screen

    return mats


class MeshBuilder:
    """Accumulates clean beveled hard-surface primitives into one BMesh with material slots and UVs."""

    def __init__(self, mat_order: list[str]) -> None:
        self.bm = bmesh.new()
        self.uv_layer = self.bm.loops.layers.uv.new("UVMap")
        self.mat_index = {name: idx for idx, name in enumerate(mat_order)}
        self.custom_uv_faces: set[bmesh.types.BMFace] = set()

    def _apply_xform(self, geom: list, xform: Matrix | None) -> None:
        if xform is not None:
            bmesh.ops.transform(self.bm, matrix=xform, verts=[g for g in geom if isinstance(g, bmesh.types.BMVert)])

    def _assign_mat(self, faces: list[bmesh.types.BMFace], mat_name: str, smooth: bool = False) -> None:
        idx = self.mat_index[mat_name]
        for f in faces:
            f.material_index = idx
            f.smooth = smooth

    def add_box(
        self,
        size: tuple[float, float, float],
        center: tuple[float, float, float] = (0.0, 0.0, 0.0),
        rot_deg: tuple[float, float, float] = (0.0, 0.0, 0.0),
        mat: str = "M_SinkMonitor_Gunmetal",
        bevel: float = 0.0,
        bevel_segments: int = 1,
        parent_xform: Matrix | None = None,
    ) -> list[bmesh.types.BMFace]:
        sx, sy, sz = size
        scale_mat = Matrix.Diagonal((sx, sy, sz, 1.0))
        res = bmesh.ops.create_cube(self.bm, size=1.0, matrix=scale_mat)
        verts = res["verts"]

        faces = list({f for v in verts for f in v.link_faces})
        edges = list({e for v in verts for e in v.link_edges})

        if bevel > 0.0:
            bev = bmesh.ops.bevel(
                self.bm,
                geom=edges,
                offset=bevel,
                offset_type="OFFSET",
                segments=bevel_segments,
                profile=0.5,
                affect="EDGES",
                clamp_overlap=True,
            )
            verts = list({v for g in bev["verts"] for v in [g]} | set(verts))
            # Filter still-valid verts and rebuild face list
            verts = [v for v in verts if v.is_valid]
            faces = list({f for v in verts for f in v.link_faces})

        local_mat = Matrix.Translation(Vector(center)) @ Euler(
            tuple(math.radians(a) for a in rot_deg), "XYZ"
        ).to_matrix().to_4x4()
        total_mat = (parent_xform @ local_mat) if parent_xform is not None else local_mat
        bmesh.ops.transform(self.bm, matrix=total_mat, verts=verts)
        bmesh.ops.recalc_face_normals(self.bm, faces=faces)
        self._assign_mat(faces, mat, smooth=False)
        return faces

    def add_cylinder(
        self,
        radius: float,
        depth: float,
        segments: int = 16,
        center: tuple[float, float, float] = (0.0, 0.0, 0.0),
        rot_deg: tuple[float, float, float] = (0.0, 0.0, 0.0),
        mat: str = "M_SinkMonitor_BrushedMetal",
        bevel: float = 0.0,
        parent_xform: Matrix | None = None,
        smooth: bool = True,
    ) -> list[bmesh.types.BMFace]:
        res = bmesh.ops.create_cone(
            self.bm,
            cap_ends=True,
            cap_tris=False,
            segments=segments,
            radius1=radius,
            radius2=radius,
            depth=depth,
        )
        verts = res["verts"]
        faces = list({f for v in verts for f in v.link_faces})

        if bevel > 0.0:
            cap_edges = [
                e for e in {e for v in verts for e in v.link_edges}
                if abs(e.verts[0].co.z - e.verts[1].co.z) < 1e-5
            ]
            bev = bmesh.ops.bevel(
                self.bm,
                geom=cap_edges,
                offset=bevel,
                offset_type="OFFSET",
                segments=1,
                profile=0.5,
                affect="EDGES",
                clamp_overlap=True,
            )
            verts = [v for v in (set(verts) | set(bev["verts"])) if v.is_valid]
            faces = list({f for v in verts for f in v.link_faces})

        local_mat = Matrix.Translation(Vector(center)) @ Euler(
            tuple(math.radians(a) for a in rot_deg), "XYZ"
        ).to_matrix().to_4x4()
        total_mat = (parent_xform @ local_mat) if parent_xform is not None else local_mat
        bmesh.ops.transform(self.bm, matrix=total_mat, verts=verts)
        bmesh.ops.recalc_face_normals(self.bm, faces=faces)
        self._assign_mat(faces, mat, smooth=smooth)
        return faces

    def add_uv_quad(
        self,
        width_y: float,
        height_z: float,
        center: tuple[float, float, float],
        rot_deg: tuple[float, float, float],
        mat: str,
        uv_rect: tuple[float, float, float, float] = (0.0, 0.0, 1.0, 1.0),
        parent_xform: Matrix | None = None,
    ) -> bmesh.types.BMFace:
        """
        Adds a quad facing +X in its local space with exact non-mirrored UVs:
        Looking from +X toward -X (+Z up, +Y right):
        Bottom-Left (-Y, -Z) -> (u0, v0)
        Bottom-Right (+Y, -Z) -> (u1, v0)
        Top-Right (+Y, +Z) -> (u1, v1)
        Top-Left (-Y, +Z) -> (u0, v1)
        """
        hy = width_y * 0.5
        hz = height_z * 0.5
        u0, v0, u1, v1 = uv_rect

        coords = [
            Vector((0.0, -hy, -hz)),
            Vector((0.0, hy, -hz)),
            Vector((0.0, hy, hz)),
            Vector((0.0, -hy, hz)),
        ]
        uvs = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]

        local_mat = Matrix.Translation(Vector(center)) @ Euler(
            tuple(math.radians(a) for a in rot_deg), "XYZ"
        ).to_matrix().to_4x4()
        total_mat = (parent_xform @ local_mat) if parent_xform is not None else local_mat

        verts = [self.bm.verts.new(total_mat @ c) for c in coords]
        face = self.bm.faces.new(verts)
        face.material_index = self.mat_index[mat]
        face.smooth = False
        for loop, uv in zip(face.loops, uvs):
            loop[self.uv_layer].uv = uv
        self.custom_uv_faces.add(face)
        return face

    def assign_box_uvs(self, scale: float = 1.0) -> None:
        """Assigns clean cubic/triplanar UVs to every non-custom-UV face."""
        for face in self.bm.faces:
            if face in self.custom_uv_faces:
                continue
            n = face.normal
            ax, ay, az = abs(n.x), abs(n.y), abs(n.z)
            for loop in face.loops:
                co = loop.vert.co
                if az >= ax and az >= ay:
                    u, v = co.x * scale, co.y * scale
                elif ax >= ay:
                    u, v = co.y * scale, co.z * scale
                else:
                    u, v = co.x * scale, co.z * scale
                loop[self.uv_layer].uv = (u, v)


def build_terminal_geometry(builder: MeshBuilder) -> None:
    orange = "M_SinkMonitor_FicsitOrange"
    gunmetal = "M_SinkMonitor_Gunmetal"
    brushed = "M_SinkMonitor_BrushedMetal"
    hazard = "M_SinkMonitor_Hazard"
    screen = "M_SinkMonitor_Screen"

    # Exact UV coordinates sampling solid color swatches inside T_SinkMonitor_Screen_Emissive.png
    uv_cyan = (0.12, 0.145, 0.14, 0.151)     # Solid cyan DNA bar
    uv_mint = (0.825, 0.928, 0.829, 0.934)   # Solid mint status dot
    uv_amber = (0.68, 0.562, 0.72, 0.570)    # Solid orange/amber coupon bar

    # -------------------------------------------------------------------------
    # PART 1: Base Plinth (Z = 0.00 to 0.22 m)
    # -------------------------------------------------------------------------
    # Lower cast-iron slab (flush at Z = 0.00)
    builder.add_box((1.00, 1.50, 0.13), center=(0.0, 0.0, 0.065), mat=gunmetal, bevel=0.022)
    # Upper stepped plinth deck
    builder.add_box((0.88, 1.38, 0.09), center=(-0.01, 0.0, 0.175), mat=gunmetal, bevel=0.018)

    # 4 corner anchor flanges + 8 hex anchor bolts
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            bx = sx * 0.44
            by = sy * 0.69
            builder.add_box((0.20, 0.22, 0.055), center=(bx, by, 0.0275), mat=gunmetal, bevel=0.010)
            builder.add_box((0.16, 0.18, 0.015), center=(bx, by, 0.0625), mat=brushed, bevel=0.003)
            for off in (-0.045, 0.045):
                builder.add_cylinder(0.018, 0.024, segments=6, center=(bx, by + off, 0.080), mat=brushed, bevel=0.003, smooth=False)

    # Front angled bumper + hazard stripe panel
    builder.add_box((0.10, 1.36, 0.13), center=(0.48, 0.0, 0.095), rot_deg=(0.0, 18.0, 0.0), mat=gunmetal, bevel=0.012)
    builder.add_uv_quad(
        width_y=1.24,
        height_z=0.115,
        center=(0.534, 0.0, 0.098),
        rot_deg=(0.0, 18.0, 0.0),
        mat=hazard,
        uv_rect=(0.0, 0.0, 0.5, 1.0),
    )
    # Orange bumper corner caps
    for sy in (-1.0, 1.0):
        builder.add_box((0.115, 0.075, 0.135), center=(0.48, sy * 0.66, 0.095), rot_deg=(0.0, 18.0, 0.0), mat=orange, bevel=0.010)

    # Rear cable conduit collar
    builder.add_cylinder(0.065, 0.16, segments=8, center=(-0.47, 0.0, 0.10), rot_deg=(0.0, 90.0, 0.0), mat=brushed, bevel=0.008, smooth=False)

    # -------------------------------------------------------------------------
    # PART 2: Pedestal Server Core & Side Armor Buttresses (Z = 0.22 to 1.05 m)
    # -------------------------------------------------------------------------
    # Main gunmetal server tower
    builder.add_box((0.74, 1.14, 0.84), center=(-0.03, 0.0, 0.635), mat=gunmetal, bevel=0.025)

    # Front service bay recess frame + twin vertical handles + FICSIT badge bar
    builder.add_box((0.04, 0.88, 0.56), center=(0.345, 0.0, 0.56), mat=gunmetal, bevel=0.010)
    builder.add_box((0.025, 0.82, 0.50), center=(0.365, 0.0, 0.56), mat=brushed, bevel=0.005)
    builder.add_box((0.035, 0.76, 0.44), center=(0.372, 0.0, 0.56), mat=gunmetal, bevel=0.006)
    for sy in (-1.0, 1.0):
        builder.add_box((0.035, 0.024, 0.22), center=(0.402, sy * 0.30, 0.56), mat=brushed, bevel=0.005)
    builder.add_box((0.02, 0.28, 0.055), center=(0.392, 0.0, 0.72), mat=orange, bevel=0.005)

    # Twin FICSIT-Orange side armor buttresses (tapered look via stacked beveled sections)
    for sy in (-1.0, 1.0):
        cy = sy * 0.65
        builder.add_box((0.78, 0.19, 0.44), center=(-0.02, cy, 0.43), mat=orange, bevel=0.025)
        builder.add_box((0.70, 0.18, 0.45), center=(-0.03, sy * 0.64, 0.84), mat=orange, bevel=0.025)
        # Side vent bezel + 5 angled louver slats
        vy = sy * 0.742
        builder.add_box((0.40, 0.025, 0.46), center=(-0.03, vy, 0.60), mat=gunmetal, bevel=0.006)
        for i in range(5):
            lz = 0.44 + i * 0.08
            builder.add_box((0.32, 0.030, 0.028), center=(-0.03, vy + sy * 0.006, lz), rot_deg=(sy * 28.0, 0.0, 0.0), mat=brushed, bevel=0.003)

    # -------------------------------------------------------------------------
    # PART 3: Ergonomic Control Console Deck (Z = 0.92 to 1.15 m, tilted 22 deg)
    # -------------------------------------------------------------------------
    deck_xform = Matrix.Translation(Vector((0.31, 0.0, 1.03))) @ Euler((0.0, math.radians(22.0), 0.0), "XYZ").to_matrix().to_4x4()

    # Support gussets under console
    for sy in (-1.0, 1.0):
        builder.add_box((0.28, 0.07, 0.22), center=(0.30, sy * 0.46, 0.90), rot_deg=(0.0, -30.0, 0.0), mat=gunmetal, bevel=0.008)

    # Main sloped deck body + orange side cheeks + brushed front palm rail
    builder.add_box((0.48, 1.38, 0.09), center=(0.0, 0.0, 0.0), mat=gunmetal, bevel=0.016, parent_xform=deck_xform)
    for sy in (-1.0, 1.0):
        builder.add_box((0.50, 0.075, 0.105), center=(0.0, sy * 0.72, 0.005), mat=orange, bevel=0.014, parent_xform=deck_xform)
    builder.add_cylinder(0.022, 1.36, segments=12, center=(0.245, 0.0, 0.025), rot_deg=(90.0, 0.0, 0.0), mat=brushed, bevel=0.004, parent_xform=deck_xform)

    # Left-center tactile keypad cluster (3x5 keys + wide execute bar)
    builder.add_box((0.28, 0.46, 0.025), center=(0.01, -0.32, 0.048), mat=brushed, bevel=0.005, parent_xform=deck_xform)
    for r in range(3):
        for c in range(5):
            kx = -0.07 + r * 0.055
            ky = -0.46 + c * 0.070
            builder.add_box((0.036, 0.048, 0.022), center=(kx, ky, 0.062), mat=gunmetal, bevel=0.004, parent_xform=deck_xform)
    builder.add_box((0.040, 0.24, 0.022), center=(0.095, -0.32, 0.062), mat=orange, bevel=0.005, parent_xform=deck_xform)

    # Illuminated diagnostic pushbuttons (mint, cyan, amber)
    button_uvs = [uv_mint, uv_cyan, uv_amber]
    for idx, buv in enumerate(button_uvs):
        by = 0.02 + idx * 0.095
        builder.add_box((0.068, 0.068, 0.020), center=(0.03, by, 0.052), mat=brushed, bevel=0.004, parent_xform=deck_xform)
        builder.add_uv_quad(
            width_y=0.048,
            height_z=0.048,
            center=(0.03, by, 0.064),
            rot_deg=(0.0, -90.0, 0.0),
            mat=screen,
            uv_rect=buv,
            parent_xform=deck_xform,
        )

    # Knurled rotary encoder dial
    builder.add_cylinder(0.052, 0.020, segments=16, center=(0.02, 0.34, 0.052), mat=gunmetal, bevel=0.003, parent_xform=deck_xform)
    builder.add_cylinder(0.040, 0.032, segments=16, center=(0.02, 0.34, 0.072), mat=brushed, bevel=0.004, parent_xform=deck_xform)

    # Decorative AWESOME Coupon reader bezel on right end of console
    builder.add_box((0.20, 0.16, 0.060), center=(0.01, 0.54, 0.068), mat=orange, bevel=0.010, parent_xform=deck_xform)
    builder.add_box((0.15, 0.11, 0.028), center=(0.01, 0.54, 0.094), mat=brushed, bevel=0.004, parent_xform=deck_xform)
    builder.add_box((0.016, 0.082, 0.020), center=(0.01, 0.54, 0.104), mat=gunmetal, bevel=0.002, parent_xform=deck_xform)
    builder.add_uv_quad(
        width_y=0.080,
        height_z=0.016,
        center=(-0.035, 0.54, 0.110),
        rot_deg=(0.0, -90.0, 0.0),
        mat=screen,
        uv_rect=uv_cyan,
        parent_xform=deck_xform,
    )

    # -------------------------------------------------------------------------
    # PART 4: Main Display Hood & Wide Telemetry Screen (Z = 1.15 to 1.96 m)
    # -------------------------------------------------------------------------
    # Heavy gunmetal neck collar bridging pedestal (Z=1.05) cleanly into tilted hood
    builder.add_box((0.50, 1.06, 0.24), center=(-0.05, 0.0, 1.12), mat=gunmetal, bevel=0.020)

    # Tilted back 12 deg around Y axis -> top leans toward -X, screen faces +X & slightly up.
    screen_pivot = Matrix.Translation(Vector((-0.02, 0.0, 1.55))) @ Euler((0.0, math.radians(-12.0), 0.0), "XYZ").to_matrix().to_4x4()

    # Main FICSIT-Orange armored display shell (front face sits at local_X = +0.08)
    builder.add_box((0.36, 1.50, 0.80), center=(-0.10, 0.0, 0.0), mat=orange, bevel=0.030, parent_xform=screen_pivot)
    # Top overhanging glare brow
    builder.add_box((0.24, 1.46, 0.09), center=(0.11, 0.0, 0.365), mat=orange, bevel=0.016, parent_xform=screen_pivot)
    # Recessed dark gunmetal inner glare bezel frame (protruding to local_X = +0.15 around 1.24 x 0.62 aperture)
    builder.add_box((0.12, 1.34, 0.065), center=(0.090, 0.0, 0.342), mat=gunmetal, bevel=0.008, parent_xform=screen_pivot)
    builder.add_box((0.12, 1.34, 0.065), center=(0.090, 0.0, -0.342), mat=gunmetal, bevel=0.008, parent_xform=screen_pivot)
    for sy in (-1.0, 1.0):
        builder.add_box((0.12, 0.065, 0.64), center=(0.090, sy * 0.652, 0.0), mat=gunmetal, bevel=0.008, parent_xform=screen_pivot)

    # Wide 1.24 m x 0.62 m emissive telemetry glass screen (proud of +0.08 shell, recessed inside +0.15 bezel)
    builder.add_uv_quad(
        width_y=1.24,
        height_z=0.62,
        center=(0.095, 0.0, 0.0),
        rot_deg=(0.0, 0.0, 0.0),
        mat=screen,
        uv_rect=(0.0, 0.0, 1.0, 1.0),
        parent_xform=screen_pivot,
    )

    # Twin brushed-metal side grab bars (within Y in [-0.80, +0.80])
    for sy in (-1.0, 1.0):
        gy = sy * 0.778
        builder.add_cylinder(0.019, 0.56, segments=12, center=(0.02, gy, 0.0), mat=brushed, bevel=0.003, parent_xform=screen_pivot)
        for sz in (-0.25, 0.25):
            builder.add_cylinder(0.017, 0.075, segments=12, center=(0.02, gy - sy * 0.032, sz), rot_deg=(90.0, 0.0, 0.0), mat=brushed, bevel=0.003, parent_xform=screen_pivot)

    # -------------------------------------------------------------------------
    # PART 5: Top Status Visor & Wireless Transceiver Mast (Z = 1.95 to 2.25 m)
    # -------------------------------------------------------------------------
    # Brushed steel roof rack base
    builder.add_box((0.38, 1.28, 0.065), center=(-0.13, 0.0, 1.975), mat=brushed, bevel=0.010)
    # Forward-facing status visor housing + glowing cyan strip (clearly visible above display brow)
    builder.add_box((0.14, 0.94, 0.068), center=(0.07, 0.0, 2.005), mat=gunmetal, bevel=0.008)
    builder.add_uv_quad(
        width_y=0.84,
        height_z=0.032,
        center=(0.143, 0.0, 2.005),
        rot_deg=(0.0, 0.0, 0.0),
        mat=screen,
        uv_rect=uv_cyan,
    )

    # Left optical diagnostic pod
    builder.add_box((0.20, 0.16, 0.095), center=(-0.18, -0.48, 2.045), mat=gunmetal, bevel=0.010)
    builder.add_cylinder(0.026, 0.04, segments=12, center=(-0.08, -0.48, 2.045), rot_deg=(0.0, 90.0, 0.0), mat=brushed, bevel=0.004)

    # Right wireless transceiver pod + dual vertical antenna rods (tip at Z = 2.25 m)
    builder.add_box((0.24, 0.22, 0.105), center=(-0.18, 0.48, 2.05), mat=orange, bevel=0.014)
    builder.add_box((0.20, 0.18, 0.035), center=(-0.18, 0.48, 2.105), mat=gunmetal, bevel=0.006)

    # Primary antenna rod: Z = 2.10 to 2.24 m + tip cap to Z = 2.25 m
    builder.add_cylinder(0.014, 0.13, segments=10, center=(-0.21, 0.44, 2.175), mat=brushed, bevel=0.002)
    builder.add_cylinder(0.018, 0.020, segments=10, center=(-0.21, 0.44, 2.240), mat=orange, bevel=0.003)
    # Secondary antenna rod: Z = 2.10 to 2.19 m
    builder.add_cylinder(0.011, 0.085, segments=10, center=(-0.15, 0.53, 2.15), mat=brushed, bevel=0.002)
    builder.add_cylinder(0.015, 0.016, segments=10, center=(-0.15, 0.53, 2.195), mat=orange, bevel=0.002)

    # -------------------------------------------------------------------------
    # PART 6: 360° Rear Detailing (-X face)
    # -------------------------------------------------------------------------
    # 11 vertical extruded cooling fins on lower pedestal rear
    for i in range(11):
        fy = -0.45 + i * 0.09
        builder.add_box((0.08, 0.022, 0.56), center=(-0.42, fy, 0.62), mat=brushed, bevel=0.003)

    # Rear junction box & serial plate
    builder.add_box((0.10, 0.46, 0.22), center=(-0.42, 0.0, 1.06), mat=gunmetal, bevel=0.012)
    builder.add_box((0.02, 0.24, 0.09), center=(-0.472, 0.0, 1.06), mat=brushed, bevel=0.003)
    builder.add_box((0.03, 0.06, 0.06), center=(-0.472, 0.17, 1.06), mat=orange, bevel=0.004)

    # Dual circular fan shrouds & cross grilles on rear of tilted display hood
    for sy in (-1.0, 1.0):
        fy = sy * 0.34
        builder.add_cylinder(0.18, 0.055, segments=20, center=(-0.295, fy, 0.0), rot_deg=(0.0, 90.0, 0.0), mat=gunmetal, bevel=0.008, parent_xform=screen_pivot)
        builder.add_cylinder(0.14, 0.045, segments=20, center=(-0.312, fy, 0.0), rot_deg=(0.0, 90.0, 0.0), mat=brushed, bevel=0.004, parent_xform=screen_pivot)
        builder.add_cylinder(0.048, 0.060, segments=12, center=(-0.320, fy, 0.0), rot_deg=(0.0, 90.0, 0.0), mat=gunmetal, bevel=0.004, parent_xform=screen_pivot)
        # Cross guard bars
        builder.add_box((0.025, 0.27, 0.022), center=(-0.335, fy, 0.0), mat=brushed, bevel=0.002, parent_xform=screen_pivot)
        builder.add_box((0.025, 0.022, 0.27), center=(-0.335, fy, 0.0), mat=brushed, bevel=0.002, parent_xform=screen_pivot)


def create_mesh_object(name: str, bm: bmesh.types.BMesh, materials: list[bpy.types.Material]) -> bpy.types.Object:
    # Triangulate >4-gon faces (e.g., cylinder caps) so FBX exports tangent space cleanly while keeping screen quads intact.
    ngons = [f for f in bm.faces if len(f.verts) > 4]
    if ngons:
        bmesh.ops.triangulate(bm, faces=ngons)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for mat in materials:
        mesh.materials.append(mat)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def generate_lightmap_uvs(obj: bpy.types.Object) -> None:
    """Packs non-overlapping LightmapUVs into channel 1."""
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    lm_layer = obj.data.uv_layers.new(name="LightmapUV")
    obj.data.uv_layers.active = lm_layer

    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=0.02, scale_to_bounds=True)
    bpy.ops.object.mode_set(mode="OBJECT")

    # Keep UVMap (channel 0) active for rendering & FBX preview.
    obj.data.uv_layers.active_index = 0


def create_ucx_convex_hulls() -> list[bpy.types.Object]:
    """Creates 3 simple convex collision meshes following UE5 UCX_ naming."""
    specs = [
        ("UCX_SM_SinkMonitorTerminal_01", (1.02, 1.52, 1.04), (-0.01, 0.0, 0.52)),
        ("UCX_SM_SinkMonitorTerminal_02", (0.56, 1.48, 0.28), (0.27, 0.0, 1.04)),
        ("UCX_SM_SinkMonitorTerminal_03", (0.64, 1.58, 1.06), (-0.07, 0.0, 1.69)),
    ]
    hulls: list[bpy.types.Object] = []
    for name, size, center in specs:
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Diagonal((size[0], size[1], size[2], 1.0)))
        bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(center)), verts=bm.verts)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new(name, mesh)
        obj.display_type = "WIRE"
        obj.hide_render = True
        bpy.context.collection.objects.link(obj)
        hulls.append(obj)
    return hulls


def export_fbx(objs: list[bpy.types.Object], fbx_path: Path) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objs:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(
        filepath=str(fbx_path),
        use_selection=True,
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        axis_forward="-Y",
        axis_up="Z",
        object_types={"MESH"},
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        use_tspace=True,
        bake_space_transform=False,
        path_mode="AUTO",
    )
    print(f"Exported FBX: {fbx_path}")


def setup_studio_lighting() -> bpy.types.Object:
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 48
    scene.cycles.use_denoising = True
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"

    world = bpy.data.worlds.new("StudioWorld")
    scene.world = world
    if world.node_tree is None:
        world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.045, 0.055, 0.072, 1.0)
    bg.inputs["Strength"].default_value = 0.75

    def add_area_light(name: str, loc: tuple[float, float, float], target: tuple[float, float, float], energy: float, size: float, color: tuple[float, float, float]) -> None:
        ld = bpy.data.lights.new(name=name, type="AREA")
        ld.energy = energy
        ld.size = size
        ld.color = color
        lo = bpy.data.objects.new(name, ld)
        lo.location = Vector(loc)
        direction = Vector(target) - Vector(loc)
        lo.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
        bpy.context.collection.objects.link(lo)

    target = (0.0, 0.0, 1.15)
    add_area_light("KeyLight", (3.2, -2.4, 2.8), target, energy=360.0, size=2.2, color=(1.0, 0.94, 0.88))
    add_area_light("FillLight", (2.6, 2.8, 2.0), target, energy=150.0, size=2.8, color=(0.82, 0.90, 1.0))
    add_area_light("RimLight", (-3.0, 1.8, 2.6), target, energy=260.0, size=1.8, color=(0.75, 0.92, 1.0))
    add_area_light("TopFill", (0.4, 0.0, 4.2), target, energy=140.0, size=3.0, color=(0.95, 0.97, 1.0))

    cam_data = bpy.data.cameras.new("StudioCamera")
    cam_obj = bpy.data.objects.new("StudioCamera", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    scene.camera = cam_obj
    return cam_obj


def render_camera(
    cam_obj: bpy.types.Object,
    loc: tuple[float, float, float],
    target: tuple[float, float, float],
    lens: float,
    res_x: int,
    res_y: int,
    out_path: Path,
    transparent: bool = False,
) -> None:
    scene = bpy.context.scene
    cam_obj.location = Vector(loc)
    direction = Vector(target) - Vector(loc)
    cam_obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam_obj.data.lens = lens
    scene.render.resolution_x = res_x
    scene.render.resolution_y = res_y
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = transparent
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA" if transparent else "RGB"
    scene.render.filepath = str(out_path)
    bpy.ops.render.render(write_still=True)


def create_turnaround_sheet(cam_obj: bpy.types.Object) -> None:
    RENDERS_DIR.mkdir(parents=True, exist_ok=True)
    # Add dark studio floor plane just for turnaround renders
    bm = bmesh.new()
    bmesh.ops.create_circle(bm, cap_ends=True, radius=6.0, segments=48)
    floor_mesh = bpy.data.meshes.new("StudioFloor")
    bm.to_mesh(floor_mesh)
    bm.free()
    floor_mat = bpy.data.materials.new("StudioFloorMat")
    if floor_mat.node_tree is None:
        floor_mat.use_nodes = True
    bsdf = floor_mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (0.04, 0.045, 0.055, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.65
    floor_mesh.materials.append(floor_mat)
    floor_obj = bpy.data.objects.new("StudioFloor", floor_mesh)
    floor_obj.location = (0.0, 0.0, -0.002)
    bpy.context.collection.objects.link(floor_obj)

    views = [
        ("FRONT 3/4 HERO", (3.35, -2.75, 1.85), (0.0, 0.0, 1.08), 35.0, RENDERS_DIR / "_tmp_v1.png"),
        ("REAR 3/4 DETAIL (COOLING & ANTENNA)", (-3.45, 2.75, 1.85), (-0.05, 0.0, 1.08), 35.0, RENDERS_DIR / "_tmp_v2.png"),
        ("FRONT EYE-LINE CLOSE-UP", (2.15, 0.0, 1.56), (0.05, 0.0, 1.48), 40.0, RENDERS_DIR / "_tmp_v3.png"),
        ("SIDE PROFILE (110 x 160 x 225 cm)", (0.05, -4.65, 1.15), (0.0, 0.0, 1.12), 32.0, RENDERS_DIR / "_tmp_v4.png"),
    ]
    for _, loc, tgt, lens, path in views:
        render_camera(cam_obj, loc, tgt, lens, 960, 540, path, transparent=False)

    # Remove temporary studio floor so .blend stays clean
    bpy.data.objects.remove(floor_obj, do_unlink=True)
    bpy.data.meshes.remove(floor_mesh)
    bpy.data.materials.remove(floor_mat)

    sheet = Image.new("RGB", (1920, 1080), (14, 18, 24))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype(FONT_BOLD, 24)
    except OSError:
        font = ImageFont.load_default()

    offsets = [(0, 0), (960, 0), (0, 540), (960, 540)]
    for (label, _, _, _, path), (ox, oy) in zip(views, offsets):
        tile = Image.open(path)
        sheet.paste(tile, (ox, oy))
        draw.rectangle([ox + 16, oy + 14, ox + 560, oy + 52], fill=(12, 16, 22))
        draw.rectangle([ox + 16, oy + 14, ox + 22, oy + 52], fill=(250, 149, 73))
        draw.text((ox + 34, oy + 20), label, font=font, fill=(238, 242, 248))
        path.unlink(missing_ok=True)

    draw.line([(960, 0), (960, 1080)], fill=(40, 50, 65), width=3)
    draw.line([(0, 540), (1920, 540)], fill=(40, 50, 65), width=3)
    sheet.save(TURNAROUND_PATH, format="PNG")
    print(f"Saved turnaround render: {TURNAROUND_PATH}")


def create_icons_and_smr_logo(cam_obj: bpy.types.Object) -> None:
    RESOURCES_DIR.mkdir(parents=True, exist_ok=True)
    DOCS_DIR.mkdir(parents=True, exist_ok=True)

    # Render clean transparent 512x512 studio icon (front 3/4 slightly elevated)
    render_camera(
        cam_obj,
        loc=(2.75, -2.25, 1.82),
        target=(0.0, 0.0, 1.10),
        lens=37.0,
        res_x=512,
        res_y=512,
        out_path=ICON_512_PATH,
        transparent=True,
    )

    icon_512 = Image.open(ICON_512_PATH).convert("RGBA")
    icon_256 = icon_512.resize((256, 256), Image.Resampling.LANCZOS)
    icon_256.save(ICON_256_PATH, format="PNG")
    icon_128 = icon_512.resize((128, 128), Image.Resampling.LANCZOS)
    icon_128.save(ICON_128_PATH, format="PNG")
    print(f"Saved studio icons: {ICON_512_PATH}, {ICON_256_PATH}, {ICON_128_PATH}")

    # Composite 512x512 SMR_Logo.png over sleek dark FICSIT hex badge
    size = 512
    logo = Image.new("RGBA", (size, size), (14, 18, 24, 255))
    draw = ImageDraw.Draw(logo)

    # Subtle radial glow + octagonal/chamfered frame
    draw.rounded_rectangle([14, 14, size - 15, size - 15], radius=38, fill=(18, 24, 34, 255), outline=(250, 149, 73, 255), width=6)
    draw.rounded_rectangle([28, 28, size - 29, size - 29], radius=28, fill=(22, 30, 42, 255), outline=(50, 213, 245, 160), width=2)

    # Subtle horizontal grid accent lines
    for y in range(64, size - 64, 32):
        draw.line([(36, y), (size - 36, y)], fill=(28, 40, 56, 120), width=1)

    # Paste centered terminal render above bottom banner
    scaled = icon_512.resize((410, 410), Image.Resampling.LANCZOS)
    logo.alpha_composite(scaled, (51, 24))

    # Bottom banner badge
    draw.rounded_rectangle([42, 434, size - 42, 480], radius=10, fill=(12, 16, 22, 235), outline=(250, 149, 73, 255), width=2)
    try:
        font = ImageFont.truetype(FONT_BOLD, 24)
    except OSError:
        font = ImageFont.load_default()
    title = "AWESOME SINK MONITOR"
    bbox = draw.textbbox((0, 0), title, font=font)
    tw = bbox[2] - bbox[0]
    draw.text(((size - tw) // 2, 444), title, font=font, fill=(242, 246, 250, 255))

    logo.save(SMR_LOGO_PATH, format="PNG")
    print(f"Saved SMR logo: {SMR_LOGO_PATH}")


def main() -> None:
    clear_scene()
    mats_dict = create_materials()
    mat_order = [
        "M_SinkMonitor_FicsitOrange",
        "M_SinkMonitor_Gunmetal",
        "M_SinkMonitor_BrushedMetal",
        "M_SinkMonitor_Hazard",
        "M_SinkMonitor_Screen",
    ]
    mat_list = [mats_dict[name] for name in mat_order]

    builder = MeshBuilder(mat_order)
    build_terminal_geometry(builder)
    builder.assign_box_uvs(scale=1.0)

    terminal_obj = create_mesh_object("SM_SinkMonitorTerminal", builder.bm, mat_list)
    generate_lightmap_uvs(terminal_obj)

    hulls = create_ucx_convex_hulls()

    # Log exact Blender bounding box and triangle count
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj = terminal_obj.evaluated_get(depsgraph)
    eval_mesh = eval_obj.to_mesh()
    eval_mesh.calc_loop_triangles()
    tri_count = len(eval_mesh.loop_triangles)
    coords = [v.co for v in eval_mesh.vertices]
    min_co = Vector((min(c.x for c in coords), min(c.y for c in coords), min(c.z for c in coords)))
    max_co = Vector((max(c.x for c in coords), max(c.y for c in coords), max(c.z for c in coords)))
    size_co = max_co - min_co
    eval_obj.to_mesh_clear()

    print(
        f"SM_SinkMonitorTerminal stats: tris={tri_count}, "
        f"min=({min_co.x:.3f}, {min_co.y:.3f}, {min_co.z:.3f}) m, "
        f"max=({max_co.x:.3f}, {max_co.y:.3f}, {max_co.z:.3f}) m, "
        f"size=({size_co.x:.3f} x {size_co.y:.3f} x {size_co.z:.3f}) m"
    )

    cam_obj = setup_studio_lighting()
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    print(f"Saved blend: {BLEND_PATH}")

    export_fbx([terminal_obj] + hulls, FBX_PATH)
    create_turnaround_sheet(cam_obj)
    create_icons_and_smr_logo(cam_obj)


if __name__ == "__main__":
    main()
