# AWESOME Sink Monitor (`SinkMonitor`) — Workspace Rules & Context

This repository implements **AWESOME Sink Monitor (`SinkMonitor`)**, a Satisfactory `1.0+` mod built on **Satisfactory Mod Loader (`SML ^3.12.0`)** and **Unreal Engine `5.6.1-CSS`** (C++20). It adds the **AWESOME Sink Monitor Terminal**, an interactive buildable that opens a scrollable, searchable, sortable, and clickable telemetry interface tracking live `Items/min`, `Points/min`, session/lifetime totals, sink tracks, and active AWESOME Sinks across all vanilla and modded sinkable items.

*(Note: Keep `awesome_sink_monitor_antigravity_project_layout.md` intact as the original user specification.)*

---

## 1. Verified Local Environment & Toolchain

- **Workspace Root:** `/mnt/Workspace/Antigravity/SatisfactorySinkMonitor`
- **Plugin Root:** `/mnt/Workspace/Antigravity/SatisfactorySinkMonitor/SinkMonitor`
- **Unreal Engine `5.6.1-CSS`:** `/mnt/Workspace/UnrealEngineCSS` (includes `UnrealEditor-Cmd` with `PythonScriptPlugin` and `EditorScriptingUtilities` enabled in `FactoryGame.uproject`)
- **Shared SML Starter Project (`SML 3.12.0`, `CL517554`):** `/mnt/Workspace/Antigravity/SatisfactorySpaceElevatorMOD/tools/SatisfactoryModLoader`
  - Plugin symlinked at `tools/SatisfactoryModLoader/Mods/SinkMonitor` -> `/mnt/Workspace/Antigravity/SatisfactorySinkMonitor/SinkMonitor`
  - Read-only `FactoryGame` headers: `tools/SatisfactoryModLoader/Source/FactoryGame/Public/`
  - Read-only `SML` headers: `tools/SatisfactoryModLoader/Mods/SML/Source/SML/Public/`
- **Windows Cross-Compilation (`msvc-wine`):** `UE_WINE_MSVC=/home/drakkhis/.local/share/msvc-wine` with Wine `s:` drive mapped to `tools/SatisfactoryModLoader` for multi-target Shipping packaging (`Windows`, `WindowsServer`, `LinuxServer`).
- **3D Modeling & Asset Tooling:** Blender `5.3.0` (`/usr/bin/blender`, plus `blender` MCP server).

---

## 2. Architecture Strategy

- **No `ContentLib` Dependency:** All subsystem logic, native hooks, buildable actor, descriptors, recipes, schematics, root modules (`UGameWorldModule` / `UGameInstanceModule`), replication (`UFGRemoteCallObject`), and Slate UI (`USinkMonitorInteractWidget : public UFGInteractWidget` via `RebuildWidget()`) are written in **C++20**.
- **Automated Binary Asset Pipeline:** Custom 3D mesh (`SM_SinkMonitorTerminal`), 5 PBR materials, emissive screen texture, hazard stripe texture, and studio-rendered 2D icons are generated via Blender Python (`assets/blender/generate_sink_monitor_terminal.py`) and imported/cooked into `SinkMonitor/Content/` via headless `UnrealEditor-Cmd` Python (`tools/ue_import_assets.py`).
- **Never Hand-Edit `.uasset` Files & Never Pollute `SinkMonitor/Content/`:** Only valid `.uasset` / `.umap` files may reside under `SinkMonitor/Content/` (no `.gitkeep`, `.fbx`, `.png`, or markdown files inside `Content/`).

---

## 3. Strict Coding Rules for Antigravity Agents

1. **Always Inspect Local Headers First:**
   - Before writing any `#include`, hook, or call to Satisfactory/SML APIs (`AFGResourceSinkSubsystem`, `AFGBuildableResourceSink`, `AFGBuildable`, `UFGInteractWidget`, `UFGItemDescriptor`, `UFGBuildingDescriptor`, `UFGRecipe`, `UFGSchematic`), read the actual headers under `tools/SatisfactoryModLoader/Source/FactoryGame/Public/` and `Mods/SML/Source/SML/Public/`. Never guess signatures or asset paths.
2. **SML Hooking Conventions:**
   - Guard runtime hooks cleanly with `if (!WITH_EDITOR && !GIsEditor && !IsRunningCommandlet())` (or `#if !WITH_EDITOR` paired with `!GIsEditor && !IsRunningCommandlet()`, matching `FullBeltPhaseLock`) so Editor builds still compile-check hook signatures without patching live Editor memory.
   - Always terminate `SUBSCRIBE_UOBJECT_METHOD(...);` and `UNSUBSCRIBE_UOBJECT_METHOD(...);` with a trailing semicolon.
   - Never edit `FactoryGame` headers directly. Use `SinkMonitor/Config/AccessTransformers.ini` when private/protected access is required.
3. **`AccessTransformers.ini` Rules:**
   - Strip the Unreal `U`/`A`/`F` prefix in `Class="..."` (e.g., `Class="FGResourceSinkSubsystem"`, `Class="FGBuildable"`, `Class="FGBuildableResourceSink"`).
   - Keep the full C++ class name in `FriendClass="..."` (e.g., `FriendClass="ASinkMonitorSubsystem"`, `FriendClass="FSinkMonitorModule"`, `FriendClass="ASinkMonitorBuildable"`).
   - If UBT/UHT skips regenerating headers after editing `Config/AccessTransformers.ini`, touch/update a comment line in `AccessTransformers.ini`.
4. **Factory Tick Thread Safety:**
   - `AFGBuildableResourceSink::Factory_CollectInput_Implementation` and `AFGResourceSinkSubsystem::AddPoints_ThreadSafe` execute on **Factory Tick worker threads**.
   - Never mutate `UObject` state, `TMap`s owned by the Game Thread, replicated arrays, or Slate widgets on worker threads.
   - Capture `{ItemClass, SourceSinkObjectKey, SinkWorldLocation}` in a lock-free MPSC `TQueue<..., EQueueMode::Mpsc>` (or `FCriticalSection`-guarded queue) and drain strictly on the Game Thread in `ASinkMonitorSubsystem::Tick()`.
5. **UObject & Pointer Safety:**
   - Never store raw `UFGItemDescriptor*` or `AFGBuildableResourceSink*` without `UPROPERTY()`, `TSubclassOf<>`, or `TWeakObjectPtr<>`.
   - Always null-check `GetWorld()`, `AFGResourceSinkSubsystem::Get(World)`, and Slate widget pointers.
6. **Linux Dedicated Server Portability:**
   - Use exact case-sensitive `#include` paths matching disk files.
   - Never include Windows-only headers (`Windows.h`, etc.).
   - Guard all Slate/UMG/Texture UI and cosmetic code behind `#if !UE_SERVER` and runtime `!IsRunningDedicatedServer()`.
7. **Pure-C++20 Core & Standalone Unit Tests:**
   - Keep `SinkMonitor/Source/SinkMonitor/Public/SinkRateTracker.h` decoupled from Unreal headers so `SinkMonitor/Tests/test_rate_tracker.cpp` compiles and runs directly on Linux with `g++ -std=c++20 -Wall -Wextra -Werror`.

---

## 4. Phased Implementation Plan

- **Phase 1 — Discovery, 3D Model Plan & Plugin Scaffold (Current Readiness Milestone):**
  - Workspace layout, `.gitignore`, `AGENTS.md`, `docs/MODEL_DESIGN.md`, `docs/TECHNICAL_PLAN.md`.
  - `SinkMonitor.uplugin`, `Config/` (`AccessTransformers.ini`, `Alpakit.ini`, `PluginSettings.ini`), `SinkMonitor.Build.cs`, `SinkRateTracker.h`, `SinkMonitorTypes.h`, `SinkMonitorModule.h/.cpp`, `Tests/test_rate_tracker.cpp`.
  - Verified standalone C++20 unit tests + `FactoryEditor Linux Development` UBT build.
- **3D Asset Generation & UE5 Import:**
  - Generate `SM_SinkMonitorTerminal` (FBX, textures, icons, turnaround renders) in Blender 5.3.0 and import/cook into `SinkMonitor/Content/` via headless `UnrealEditor-Cmd` Python.
- **Phase 2 — Sink Tracking Subsystem (`ASinkMonitorSubsystem`):**
  - Native hooks on `AFGBuildableResourceSink::Factory_CollectInput_Implementation` + `AFGResourceSinkSubsystem::AddPoints_ThreadSafe`, Game Thread MPSC drain, 60-second ring buffer updates, hybrid catalog discovery (`mCachedResourceSinkPoints` + `UModContentRegistry`), active sink location tracking, and Blueprint/UI query methods.
- **Phase 3 — Interactable Buildable (`ASinkMonitorBuildable` & Pure-C++ Content Registration):**
  - `ASinkMonitorBuildable`, `USinkMonitorDescriptor`, `USinkMonitorRecipe` (`4× Reinforced Iron Plate`, `8× Cable`, `4× Copper Sheet`, `2× Quartz Crystal`), `USinkMonitorSchematic` (auto-unlocks with Tier 2 `Schematic_2-2` via `PurchasedSchematicDelegate`, and retro-grants silently via `ESchematicUnlockFlags::Force | ESchematicUnlockFlags::SuppressNarrativeMessages` on saves where `Schematic_2-2` is already unlocked), `USinkMonitorGameWorldModule`, `USinkMonitorGameInstanceModule`.
- **Phase 4 — Scrollable, Searchable & Clickable Slate UI (`USinkMonitorInteractWidget`):**
  - Search filter, "Currently Sinking Only" toggle, Sort modes, Pin/Favorite sorting, detail side-panel with icon, sink track, points, live rates, totals, and active sink telemetry.
- **Phase 5 — Multiplayer Replication, Save Persistence & Multi-Target Packaging:**
  - Server-to-client replication / `USinkMonitorRemoteCallObject` RPCs, `IFGSaveInterface` persistence for lifetime totals & pinned items, optional `USinkMonitorModConfiguration`, and `Windows` + `WindowsServer` + `LinuxServer` Shipping packaging via Alpakit (`RunUAT.sh PackagePlugin`).

---

## 5. Learnings & Notes Log

- **[Phase 1 — CL517554 / SML 3.12.0 Discovery]:**
  - `AFGResourceSinkSubsystem::AddPoints_ThreadSafe(TSubclassOf<UFGItemDescriptor> item)` returns `bool` (`true` iff the item is sinkable and worth `>= 1` point) and runs on Factory Tick worker threads.
  - `AFGResourceSinkSubsystem::mOnItemsSunk` is `TSet<TSubclassOf<UFGItemDescriptor>>` (deduplicated per flush), so it **cannot** be used to count exact items/min; hook `AddPoints_ThreadSafe` instead, and optionally wrap `AFGBuildableResourceSink::Factory_CollectInput_Implementation` with a `thread_local` sink pointer to capture which AWESOME Sink consumed each item.
  - `AFGResourceSinkSubsystem::mCachedResourceSinkPoints` is `private: TMap<TSubclassOf<UFGItemDescriptor>, FResourceSinkValuePair32>` (`EResourceSinkTrack TrackType; int32 Value;`), exposed via `Friend=(Class="FGResourceSinkSubsystem", FriendClass="ASinkMonitorSubsystem")` in `AccessTransformers.ini`.
  - `AFGBuildable` stores `mInteractWidgetSoftClass` (`TSoftClassPtr<UFGInteractWidget>`) and `mIsUseable` (`uint8:1`) in `private:`, exposed via `Friend=(Class="FGBuildable", FriendClass="ASinkMonitorBuildable")`.
  - Verified vanilla paths in `Content/FactoryGame/`:
    - Build Category (Special): `/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/BC_TradingPost.BC_TradingPost_C`
    - Build SubCategory (AWESOME): `/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/Sub_Special/SC_AWESOME.SC_AWESOME_C`
    - AWESOME Shop Category: `/Game/FactoryGame/Schematics/ResourceSinkShopCategories/SC_RSS_Management.SC_RSS_Management_C`
    - Tier 2 Resource Sink Bonus Program Prerequisite: `/Game/FactoryGame/Schematics/Progression/Schematic_2-2.Schematic_2-2_C`
    - Build Gun Producer: `/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C`
