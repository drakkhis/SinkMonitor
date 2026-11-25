# Project: AWESOME Sink Monitor (`SinkMonitor`)

> **Antigravity Workspace Context File (`AGENTS.md`)**
> Place this file in the root of your mod plugin directory (or SML project root) so Google Antigravity automatically discovers and loads these architectural rules, constraints, and implementation phases.

---

## 1. Project Overview

**SinkMonitor** is a Satisfactory mod built on the **Satisfactory Mod Loader (SML 3.x / Unreal Engine 5)** that adds a custom interactive buildable—the **AWESOME Sink Monitor Terminal**.

When a pioneer clicks/interacts with the buildable, it opens a **scrollable, searchable, and clickable UMG interface** that:
1. **Lists every sinkable item** in the game (vanilla + modded items dynamically discovered from the game's Resource Sink system).
2. **Displays real-time sinking metrics** for each item:
   - Current live sinking rate (**Items / min**, calculated over a rolling 60-second window).
   - Live points contributed (**Points / min**).
   - Cumulative count sunk (session and/or saved total).
3. **Supports clicking any item row** to inspect full details in a side panel (item icon, sink track—Standard vs. DNA/Exploration, point value per item, active AWESOME Sinks currently consuming it, and a quick Pin/Favorite toggle).

---

## 2. Core Strategy for Developing in Antigravity

Satisfactory mods combine **C++ source code** (text files that Antigravity can write, refactor, and compile) with **Unreal Engine `.uasset` files** (binary Blueprints, UMG layouts, and meshes that must be created in the Unreal Editor).

To maximize velocity in Antigravity, this project follows a **C++-First, Thin-Blueprint Architecture**:
- **90% in C++ (Managed by Antigravity):** All item discovery, SML native hooks, rolling-window rate tracking, multiplayer replication, buildable interaction logic, and UMG widget behavior (`BindWidget` C++ base classes).
- **10% in Unreal Editor (Manual Visual Wiring):** The `.uasset` wrappers (assigning the Static Mesh, Recipe, Schematic, Build Menu category, and dragging UMG visual widgets with matching variable names).

---

## 3. Antigravity Workspace & Folder Layout

Configure your Antigravity Project to include the **Satisfactory Mod Loader (SML) Starter Project** root so the agent can inspect Satisfactory's generated C++ headers (`FactoryGame/Source/FactoryGame/Public/...`) alongside your plugin.

```text
SatisfactoryModLoader/                      # SML Unreal Project Root (Add to Antigravity Project)
├── Source/FactoryGame/Public/              # READ-ONLY reference headers (DO NOT EDIT)
│   ├── FGResourceSinkSubsystem.h           # Reference for sink point lookup & point addition
│   ├── Buildables/FGBuildableResourceSink.h# Reference for AWESOME Sink consumption logic
│   └── Resources/FGItemDescriptor.h        # Reference for item metadata & icons
└── Mods/
    └── SinkMonitor/                        # <-- YOUR MOD PLUGIN ROOT
        ├── AGENTS.md                       # This file (Antigravity auto-discovered rules)
        ├── SinkMonitor.uplugin             # Plugin descriptor (depends on SML)
        ├── Config/
        │   └── AccessTransformers.ini      # SML friend/publicizer declarations if needed
        ├── Content/                        # Unreal Editor binary assets (.uasset)
        │   ├── GameWorldModule_SinkMonitor.uasset
        │   ├── Buildable/
        │   │   ├── Build_SinkMonitor.uasset          # Child of ASinkMonitorBuildable
        │   │   ├── Desc_SinkMonitor.uasset           # UFGBuildingDescriptor
        │   │   ├── Recipe_SinkMonitor.uasset         # UFGRecipe
        │   │   └── Schematic_SinkMonitor.uasset      # Unlock schematic (e.g., AWESOME Shop / Tier)
        │   └── UI/
        │       ├── WBP_SinkMonitorMain.uasset        # Child of USinkMonitorInteractWidget
        │       └── WBP_SinkMonitorItemRow.uasset     # Child of USinkMonitorItemRowWidget
        └── Source/
            └── SinkMonitor/
                ├── SinkMonitor.Build.cs
                ├── Public/
                │   ├── SinkMonitorModule.h
                │   ├── SinkMonitorTypes.h            # Structs (FSinkableItemData, FSinkItemStats)
                │   ├── SinkMonitorSubsystem.h        # AModSubsystem + SML hooks + rate tracker
                │   ├── SinkMonitorBuildable.h        # AFGBuildable interactable terminal
                │   └── UI/
                │       ├── SinkMonitorInteractWidget.h   # UFGInteractWidget main UI logic
                │       └── SinkMonitorItemRowWidget.h    # Clickable row widget logic
                └── Private/
                    ├── SinkMonitorModule.cpp
                    ├── SinkMonitorSubsystem.cpp
                    ├── SinkMonitorBuildable.cpp
                    └── UI/
                        ├── SinkMonitorInteractWidget.cpp
                        └── SinkMonitorItemRowWidget.cpp
```

---

## 4. Technical Architecture

### 4.1 Data Structures (`SinkMonitorTypes.h`)
- `FSinkableItemEntry` (`USTRUCT(BlueprintType)`):
  - `TSubclassOf<UFGItemDescriptor> ItemClass`
  - `FText DisplayName`
  - `int32 PointValue` (points per single item)
  - `EResourceSinkTrack SinkTrack` (Standard Resources vs. Exploration/DNA)
  - `float CurrentItemsPerMin` (rolling 60-second rate)
  - `float CurrentPointsPerMin` (`CurrentItemsPerMin * PointValue`)
  - `int64 SessionTotalSunk`
  - `bool bIsPinned`
- `FSinkItemRateBucket`:
  - Fixed-size ring buffer of **60 integer buckets** (1 bucket = 1 second).
  - On each 1-second timer tick, advance the ring index, clear the new bucket, and sum the 60 buckets to get the exact **Items/min** sinking rate with zero drift.

### 4.2 Tracking & Discovery (`ASinkMonitorSubsystem`)
Inherits from `AModSubsystem` (registered in `GameWorldModule_SinkMonitor` with `ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate`).

1. **Dynamic Sinkable Item Discovery:**
   - Query `AFGResourceSinkSubsystem::Get(GetWorld())`.
   - Iterate all loaded `UFGItemDescriptor` subclasses (or inspect the subsystem's internal points map via SML Access Transformer if private).
   - Keep every item where `GetResourceSinkPointsForItem(ItemClass) > 0` (excluding non-sinkable items, nuclear waste, and fluids/abstract base classes).
   - Cache the sorted master catalog once the world finishes initializing.
2. **Real-Time Sink Interception (SML Hooking):**
   - Register a native SML hook using `SUBSCRIBE_METHOD` (wrapped in `if (!WITH_EDITOR)`) on the game's item-sinking method (verify exact signature in local `FGResourceSinkSubsystem.h` or `FGBuildableResourceSink.h`, typically `AFGResourceSinkSubsystem::AddPoints_ThreadSafe` or the sink's item consumption handler).
   - Whenever an item is consumed by any AWESOME Sink:
     - Increment the current 1-second bucket for that `TSubclassOf<UFGItemDescriptor>`.
     - Increment `SessionTotalSunk`.
     - Record the timestamp and optional source `AFGBuildableResourceSink` reference/location for the detail view.
3. **Multiplayer Support:**
   - Hooks execute on the server (`HasAuthority()`).
   - Replicate active item rates (only non-zero or changed entries to minimize bandwidth) using a replicated `TArray` or `FFastArraySerializer` / periodic snapshot RPC via a custom `UFGRemoteCallObject` (RCO) when a client opens the terminal.

### 4.3 The Buildable (`ASinkMonitorBuildable`)
Inherits from `AFGBuildable` (implements `IFGUseableInterface`).
- `mIsUseable = true`
- `mInteractWidgetClass` exposed to Blueprints (`EditDefaultsOnly`) so `Build_SinkMonitor` can point to `WBP_SinkMonitorMain`.
- Uses a clean terminal/kiosk footprint (can reuse a vanilla Satisfactory monitor/console mesh during prototyping, then swap in a custom FBX mesh later).
- Requires no conveyor inputs or power (or optional minimal power like `1 MW` if desired).

### 4.4 Scrollable & Clickable UI (`USinkMonitorInteractWidget` & `USinkMonitorItemRowWidget`)
Uses `UPROPERTY(meta = (BindWidget))` so **all logic stays in C++**:

- **`USinkMonitorInteractWidget` (inherits `UFGInteractWidget`):**
  - `UScrollBox* ItemListScrollBox` — Holds one `USinkMonitorItemRowWidget` per sinkable item (spawned once on `NativeConstruct`, updated in place every `0.5s–1.0s` for smooth performance).
  - `UEditableTextBox* SearchInputBox` — Live text filtering by item display name.
  - `UCheckBox* ActiveOnlyCheckBox` — Toggle between *All Sinkable Items* and *Currently Sinking (> 0/min)*.
  - `UComboBoxString* SortModeComboBox` — Sort by **Sinking Rate (High → Low)**, **Points/Min**, **Point Value**, or **Name (A → Z)** (Pinned items always float to the top).
  - **Detail Side-Panel** (updates when any row is clicked):
    - `UImage* DetailItemIcon`
    - `UTextBlock* DetailItemNameText`
    - `UTextBlock* DetailPointValueText`
    - `UTextBlock* DetailRatePerMinText`
    - `UTextBlock* DetailPointsPerMinText`
    - `UTextBlock* DetailTotalSunkText`
    - `UTextBlock* DetailTrackText` (Standard vs. DNA)
    - `UButton* PinItemButton`
- **`USinkMonitorItemRowWidget` (inherits `UUserWidget`):**
  - `UButton* RowClickButton` — Broadcasts `FOnSinkItemRowClicked(TSubclassOf<UFGItemDescriptor>)` to the parent widget and updates selected highlight state.
  - `UImage* ItemIconImage`
  - `UTextBlock* ItemNameText`
  - `UTextBlock* PointValueText`
  - `UTextBlock* CurrentRateText` (e.g., `120 / min`, highlighted green when `> 0`, muted gray when `0`)

---

## 5. Strict Coding Rules for Antigravity Agents

1. **Always Inspect Local Headers First:**
   - Before writing any `#include`, hook, or call to Satisfactory classes (`AFGResourceSinkSubsystem`, `AFGBuildableResourceSink`, `UFGItemDescriptor`, `UFGInteractWidget`), **read the actual header files** inside `Source/FactoryGame/Public/` in this workspace. Never guess UE5/Satisfactory method names or parameter lists.
2. **SML Hooking Conventions:**
   - Always wrap `SUBSCRIBE_METHOD` / `SUBSCRIBE_METHOD_VIRTUAL` calls inside `if (!WITH_EDITOR) { ... }` (do NOT use `#if !WITH_EDITOR` preprocessor blocks, so compilation errors are caught in Editor builds).
   - Always terminate `SUBSCRIBE_METHOD(...);` macros with a trailing semicolon (required in SML 3.6+).
   - Never modify `FactoryGame` headers directly. If private/protected members (e.g., internal sink point maps) are needed, expose them via `Config/AccessTransformers.ini`.
3. **Thread Safety:**
   - `AddPoints_ThreadSafe` (and factory ticks) can run on worker threads during Satisfactory's parallel factory tick. Protect any shared maps/buckets with `FCriticalSection` (`FScopeLock`) or marshal increments cleanly to the Game Thread via `AsyncTask(ENamedThreads::GameThread, ...)` / lock-free atomics before touching UI or UObjects.
4. **UObject & Pointer Safety:**
   - Never store raw `UFGItemDescriptor*` or `AFGBuildableResourceSink*` without `UPROPERTY()`, `TSubclassOf<>`, or `TWeakObjectPtr<>`.
   - Always null-check `GetWorld()`, `AFGResourceSinkSubsystem::Get(World)`, and bound UMG widgets.
5. **No Binary Asset Guessing:**
   - Never attempt to generate or hand-edit `.uasset` or `.umap` binary files. Whenever an Unreal Editor step is required, output a concise, exact checklist of asset names, parent classes, and `BindWidget` names for the user to create.

---

## 6. Phased Implementation Plan

Execute this project **one phase at a time** using Antigravity's `PLAN → EXECUTE → VERIFY` workflow.

### Phase 1: Plugin Scaffold & Header Reconnaissance
- **Agent Tasks:**
  1. Read `FGResourceSinkSubsystem.h`, `FGBuildableResourceSink.h`, and `FGInteractWidget.h` in the local SML workspace and document the exact signatures for:
     - Fetching all sinkable items and their point values.
     - Intercepting individual items being sunk (method name, parameters, thread context).
  2. Create `SinkMonitor.uplugin`, `SinkMonitor.Build.cs`, `SinkMonitorModule.h/.cpp`, and `SinkMonitorTypes.h`.
  3. Add any required `AccessTransformers.ini` entries.
- **Verification:** Plugin compiles cleanly for `FactoryGameEditor` (Development Editor) with zero errors.

### Phase 2: Sink Tracking Subsystem (`ASinkMonitorSubsystem`)
- **Agent Tasks:**
  1. Implement `ASinkMonitorSubsystem` inheriting `AModSubsystem`.
  2. Implement `RefreshSinkableCatalog()` to populate every item with `Points > 0` (supporting both Standard and DNA tracks + modded items).
  3. Implement thread-safe SML hook(s) to record every sunk item into the 60-second rolling ring buffer and session total.
  4. Expose Blueprint-callable query functions (`GetAllSinkableItems()`, `GetStatsForItem()`, `TogglePinItem()`).
  5. Add verbose debug logging (`LogSinkMonitor`) toggled via console command or config.
- **Verification:** Launch game in Development mode, feed items into an AWESOME Sink, and verify logs output accurate item names, point values, and live items/min rates.

### Phase 3: Interactable Buildable (`ASinkMonitorBuildable`)
- **Agent Tasks:**
  1. Implement `ASinkMonitorBuildable` inheriting `AFGBuildable`.
  2. Configure `mIsUseable`, display text, and `mInteractWidgetClass`.
  3. Generate the exact Unreal Editor checklist for creating `GameWorldModule_SinkMonitor`, `Build_SinkMonitor`, `Desc_SinkMonitor`, `Recipe_SinkMonitor`, and `Schematic_SinkMonitor`.
- **Verification:** Build the terminal in-game via the Build Gun and confirm pressing `E` triggers interaction without crashing.

### Phase 4: Scrollable & Clickable UI (`USinkMonitorInteractWidget` & Row Widget)
- **Agent Tasks:**
  1. Implement `USinkMonitorItemRowWidget` with `BindWidget` properties and click delegate.
  2. Implement `USinkMonitorInteractWidget` with:
     - Scrollable list creation & live periodic refresh (`FTimerHandle` every `0.5s`).
     - Search box filtering, "Currently Sinking Only" toggle, and Sort dropdown.
     - Row click handler that highlights the selected item and populates the Detail Side-Panel (including Pin toggle).
  3. Generate the exact Unreal Editor UMG Hierarchy checklist (exact widget types and names required for `BindWidget` compilation).
- **Verification:** Open the terminal in-game; verify all sinkable items appear in the scrollable list, searching/sorting works, clicking any row updates the detail panel, and feeding an AWESOME Sink updates the live `Items/min` counter in real time.

### Phase 5: Polish, Multiplayer & Packaging
- **Agent Tasks:**
  1. Verify and harden multiplayer replication (server-to-client stats sync for dedicated servers and co-op clients).
  2. Add optional `IFGSaveInterface` persistence for lifetime sunk totals and pinned items.
  3. Add optional "Active Sinks" locator info in the detail panel (shows how many AWESOME Sinks are currently receiving the selected item).
  4. Verify Shipping build (`Alpakit` packaging) compiles cleanly without editor-only code leaks.
- **Verification:** Package mod via Alpakit, test in a standard Satisfactory session (and multiplayer if applicable), and confirm clean save/load behavior.

---

## 7. Quick-Start Prompts for Antigravity

Copy and paste these into the Antigravity Agent panel to kick off each step:

- **Start Phase 1:**
  > `Read @AGENTS.md. Begin Phase 1: inspect the local FactoryGame headers for AFGResourceSinkSubsystem and AFGBuildableResourceSink, report the exact method signatures we should use for item discovery and sink hooking, and scaffold the SinkMonitor plugin files.`
- **Start Phase 2:**
  > `Implement Phase 2 from @AGENTS.md: build ASinkMonitorSubsystem with dynamic sinkable item discovery, thread-safe 60-second rolling rate tracking, and SML hooks. Include a verification checklist.`
- **Start Phase 3 & 4:**
  > `Implement Phase 3 and Phase 4 from @AGENTS.md: create ASinkMonitorBuildable, USinkMonitorInteractWidget, and USinkMonitorItemRowWidget using BindWidget, and give me the exact Unreal Editor asset + UMG widget naming checklist.`

---

## 8. Learnings & Notes Log

*(Antigravity Agent: Append discovered SDK quirks, header paths, Access Transformer rules, or bug fixes here after each milestone so mistakes are never repeated.)*

- **[Pending Phase 1]:** Record exact Satisfactory CL# / SML version and verified hook target method signature here.