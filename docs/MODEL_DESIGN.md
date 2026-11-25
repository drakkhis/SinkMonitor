# 3D Model Design Plan: AWESOME Sink Monitor Terminal (`SM_SinkMonitorTerminal`)

## 1. Concept & Visual Identity

The **AWESOME Sink Monitor Terminal** is a free-standing industrial FICSIT telemetry kiosk designed to match the visual language of Satisfactory's **AWESOME Sink**, **AWESOME Shop**, and **HUB Terminal**:
- Heavy beveled cast-iron plinth with corner anchor bolts and front hazard bumper.
- Chamfered FICSIT-orange (`#E67E22`) armored housing and display shroud paired with matte charcoal gunmetal (`#252A30`) and machined brushed metal (`#9BA3AB`) accents.
- Ergonomic angled operator console deck with tactile keypad, LED status indicators, rotary dial, and decorative AWESOME Coupon card-reader slot.
- High-contrast wide telemetry screen (`2048×1024` custom emissive artwork showing live-style bar graphs, FICSIT header banner, coupon telemetry emblem, and scanline grid).
- Wireless top-rear telemetry transceiver mast + cyan/amber diagnostic status visor, visually explaining why the terminal needs **no conveyor belt inputs or power wiring** to aggregate sink rates across the entire factory.
- Full 360° detailing including rear extruded heat-sink cooling fins, dual circular fan grilles, armored junction box, and serial nameplate.

---

## 2. Dimensions, Scale & Coordinate Conventions

- **Blender Scene Units:** Metric (`1.0 m` in Blender = `100.0 cm` in Unreal Engine 5).
- **FBX Axis Mapping:** Blender `-Y` forward / `+Z` up exported (`global_scale=1.0`, `apply_unit_scale=True`) cleanly into UE5 `+X` operator-facing front, `+Z` up, `+Y` right.
- **Actor Pivot / Origin:** `(0, 0, 0)` at dead-center of the floor footprint base (`Z = 0` flush on foundations and terrain).
- **Overall Envelope:**
  - **Depth (`X`):** `110 cm` (`-55 cm` rear to `+55 cm` front console overhang)
  - **Width (`Y`):** `160 cm` (`-80 cm` left to `+80 cm` right)
  - **Height (`Z`):** `225 cm` (`0 cm` floor to `+225 cm` antenna tip)
- **Pioneer Ergonomics:**
  - Pioneer camera eye-line sits around `Z ≈ 160 cm`.
  - Main display spans `Z = 118–188 cm`, tilted back `12°` for glare-free close-up readability.
  - Control console deck spans `Z = 95–112 cm`, angled `22°` toward the pioneer.
- **Buildable Clearance (`FFGClearanceData`):**
  - `FBox(FVector(-65.0f, -90.0f, 0.0f), FVector(65.0f, 90.0f, 230.0f))` (`130 × 180 × 230 cm`), `EClearanceType::CT_Default`.

---

## 3. 6-Part Hard-Surface Geometry Breakdown

Target polygon budget: **6,000 – 12,000 triangles (LOD0)** with clean beveled edges and crisp shading (auto-smooth / weighted normals).

| Part | Z Range (cm) | Structural Details |
| --- | --- | --- |
| **1. Base Plinth** | `0 – 22` | Wide beveled cast-iron footplate (`110 × 160 cm` envelope), 4 corner anchor bolt flanges with hex bolts, recessed rubberized leveling pads, rear conduit stub, front angled hazard bumper strip (`M_SinkMonitor_Hazard`). |
| **2. Pedestal Server Core & Side Armor Buttresses** | `22 – 105` | Tapered dark gunmetal server chassis flanked by chamfered FICSIT-orange side cowling fins, recessed side ventilation louvers, and bolted service access doors. |
| **3. Ergonomic Control Console Deck** | `95 – 115` | Cantilevered sloped operator ledge extending to `X = +55 cm`, featuring a recessed tactile keypad cluster, illuminated cyan/amber LED status pushbuttons, knurled rotary encoder dial, and a decorative AWESOME Coupon reader slot bezel. |
| **4. Main Display Hood & Wide Telemetry Screen** | `115 – 195` | Heavy beveled FICSIT-orange outer monitor shroud with a dark gunmetal recessed glare hood framing a `120 × 64 cm` dark glass telemetry screen (`12°` backward tilt) showing the custom `2048×1024` FICSIT telemetry display graphic. |
| **5. Top Status Visor & Wireless Transceiver Mast** | `195 – 225` | Brushed steel roof rack housing a horizontal cyan/amber emissive status bar, optical diagnostic sensor pod, and a compact dual-rod industrial telemetry antenna mounted on the rear-right corner. |
| **6. 360° Rear Chassis Detailing (`-X`)** | `22 – 195` | Vertical extruded aluminum heat-sink cooling fins, dual recessed circular fan shrouds with protective grilles, armored junction box, and brushed metal FICSIT serial nameplate. |

---

## 4. PBR Material Slots & Generated Textures

Exactly **5 clean material slots** mapped consistently across Blender and UE5:

1. **`M_SinkMonitor_FicsitOrange`**
   - Signature FICSIT safety orange (`#E67E22`, `BaseColor`), `Metallic = 0.15`, `Roughness = 0.42`.
   - Used on outer display shroud, side armor buttresses, and top trim accents.
2. **`M_SinkMonitor_Gunmetal`**
   - Matte industrial charcoal steel (`#252A30`), `Metallic = 0.85`, `Roughness = 0.48`.
   - Used on base plinth, server core, glare hood, keypad housing, and fan shrouds.
3. **`M_SinkMonitor_BrushedMetal`**
   - Machined brushed steel/aluminum (`#9BA3AB`), `Metallic = 0.92`, `Roughness = 0.30`.
   - Used on hex anchor bolts, rear heat-sink fins, rotary dial, coupon bezel, roof rack, and antenna rods.
4. **`M_SinkMonitor_Hazard`**
   - Clean `2048×512` procedural diagonal yellow/black hazard stripe texture (`T_SinkMonitor_Hazard.png`), `Metallic = 0.10`, `Roughness = 0.45`.
   - Used on front plinth bumper strip and upper warning edge trim.
5. **`M_SinkMonitor_Screen`**
   - Glossy dark glass (`BaseColor = #05080D`, `Metallic = 0.05`, `Roughness = 0.12`, `Specular = 0.70`) driven by a custom `2048×1024` emissive telemetry artwork texture (`T_SinkMonitor_Screen_Emissive.png`) featuring FICSIT AWESOME Sink header banner, throughput bar charts, coupon readout emblem, and crisp cyan/orange/green status indicators.

---

## 5. UVs, Collision & LOD Strategy

- **UV Channel 0 (`UVMap`):** Non-overlapping islands; the main screen quad fills `0..1` cleanly for pixel-sharp telemetry art; hazard strip UVs aligned squarely along the stripe axis.
- **UV Channel 1 (`LightmapUV`):** Non-overlapping packed lightmap UVs with clean texel padding.
- **Convex Collision Hulls (UE5 `UCX_` convention inside the same FBX):**
  - `UCX_SM_SinkMonitorTerminal_01`: Base plinth & pedestal tower volume.
  - `UCX_SM_SinkMonitorTerminal_02`: Cantilevered control deck volume.
  - `UCX_SM_SinkMonitorTerminal_03`: Display hood & top visor volume.
- **LODs:** Auto-generate `LOD1` and `LOD2` (`num_lods = 3`) inside UE5 on FBX import.

---

## 6. Automated Blender + Headless UE5 Import Pipeline

1. **Blender Script (`assets/blender/generate_sink_monitor_terminal.py`):**
   - Run headlessly via `/usr/bin/blender -b -P assets/blender/generate_sink_monitor_terminal.py` (or inspected interactively via `blender` MCP).
   - Procedurally builds the 6-part terminal mesh + 3 `UCX_` convex collision hulls and saves `assets/blender/SM_SinkMonitorTerminal.blend`.
   - Generates `T_SinkMonitor_Screen_Emissive.png` (`2048×1024`) and `T_SinkMonitor_Hazard.png` (`2048×512`).
   - Exports `assets/blender/SM_SinkMonitorTerminal.fbx` (mesh + `UCX_` collisions + tangents/smoothing).
   - Renders studio-lit transparent 3/4 icons & showcase views:
     - `SinkMonitor/Resources/Icon128.png` (`128×128` plugin icon)
     - `assets/blender/T_Icon_SinkMonitor_256.png` (`256×256` small buildable icon)
     - `assets/blender/T_Icon_SinkMonitor_512.png` (`512×512` big buildable / schematic icon)
     - `docs/SMR_Logo.png` (`512×512` ficsit.app mod logo)
     - `docs/renders/terminal_turnaround.png` (multi-angle preview render for visual inspection)
2. **Headless UE5 Import Script (`tools/ue_import_assets.py`):**
   - Executed via `/mnt/Workspace/UnrealEngineCSS/Engine/Binaries/Linux/UnrealEditor-Cmd` against `/mnt/Workspace/Antigravity/SatisfactorySpaceElevatorMOD/tools/SatisfactoryModLoader/FactoryGame.uproject` (`-ExecutePythonScript=... -NullRHI -NoSound -Unattended -NoSplash`).
   - Imports `SM_SinkMonitorTerminal.fbx` (with `UCX_` hulls & `num_lods = 3`), `T_SinkMonitor_Screen_Emissive.png`, `T_SinkMonitor_Hazard.png`, `T_Icon_SinkMonitor_256.png`, and `T_Icon_SinkMonitor_512.png` into `/SinkMonitor/...`.
   - Programmatically constructs and compiles the 5 `UMaterial` assets (`M_SinkMonitor_FicsitOrange`, `M_SinkMonitor_Gunmetal`, `M_SinkMonitor_BrushedMetal`, `M_SinkMonitor_Hazard`, `M_SinkMonitor_Screen`), binds them to the 5 material slots on `SM_SinkMonitorTerminal`, configures `TextureGroup::TEXTUREGROUP_UI` + `sRGB` on icons, and saves every `.uasset` cleanly into `SinkMonitor/Content/`.
