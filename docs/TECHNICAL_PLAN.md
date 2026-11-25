# Technical Implementation Plan: AWESOME Sink Monitor (`SinkMonitor`)

## 1. Verified Target Stack & Header Contract (`CL517554` / `SML 3.12.0`)

- **Game / Engine:** Satisfactory `1.0+` (`CL517554`), Unreal Engine `5.6.1-CSS` (`/mnt/Workspace/UnrealEngineCSS`), **SML `^3.12.0`** (`/mnt/Workspace/Antigravity/SatisfactorySpaceElevatorMOD/tools/SatisfactoryModLoader`).
- **Module Language & Targets:** C++20 (`SinkMonitor.Build.cs`), Shipping packaging for `Windows`, `WindowsServer`, and `LinuxServer` (`Alpakit.ini`), zero `ContentLib` dependency.
- **Asset Strategy:** Pure-C++ classes for all runtime logic, buildable registration, and Slate UI (`USinkMonitorInteractWidget`), combined with automated Blender -> headless `UnrealEditor-Cmd` Python cooking for `SM_SinkMonitorTerminal`, 5 PBR materials, textures, and 2D icons inside `SinkMonitor/Content/`.

### Verified `FactoryGame` Signatures (`Source/FactoryGame/Public/`)

1. **`AFGResourceSinkSubsystem` (`FGResourceSinkSubsystem.h`):**
   - `enum class EResourceSinkTrack : uint8 { RST_Default = 0, RST_Exploration = 1, RST_MAX = 2 };`
   - `struct FResourceSinkValuePair32 { EResourceSinkTrack TrackType; int32 Value; };`
   - `bool AddPoints_ThreadSafe(TSubclassOf<UFGItemDescriptor> item);` — invoked on **Factory Tick worker threads**; returns `true` only when `item` can be discarded and is worth `>= 1` sink point.
   - `int32 GetResourceSinkPointsForItem(TSubclassOf<UFGItemDescriptor> itemDescriptor);`
   - `bool FindResourceSinkPointsForItem(TSubclassOf<UFGItemDescriptor> itemDescriptor, int32& out_numPoints, EResourceSinkTrack& out_itemTrack);`
   - `private: TMap<TSubclassOf<UFGItemDescriptor>, FResourceSinkValuePair32> mCachedResourceSinkPoints;` — exposed to `ASinkMonitorSubsystem` via `AccessTransformers.ini`.
   - `FOnItemsSunkDelegate mOnItemsSunk;` (`TSet<TSubclassOf<UFGItemDescriptor>>`) — deduplicated per tick flush, therefore **unsuitable for exact item counts**; `AddPoints_ThreadSafe` is the exact hook target.

2. **`AFGBuildableResourceSink` (`Buildables/FGBuildableResourceSink.h`):**
   - `virtual void Factory_CollectInput_Implementation() override;` — worker-thread input collection; paired with a `thread_local` sink pointer so `AddPoints_ThreadSafe` knows which `AFGBuildableResourceSink` instance (and world location) consumed the item.

3. **`AFGBuildable` (`Buildables/FGBuildable.h`) & `IFGUseableInterface` (`FGUseableInterface.h`):**
   - `private: TSoftClassPtr<UFGInteractWidget> mInteractWidgetSoftClass;` and `uint8 mIsUseable:1;` — exposed via `Friend=(Class="FGBuildable", FriendClass="ASinkMonitorBuildable")` in `AccessTransformers.ini`.
   - `mHologramClass = AFGBuildableHologram::StaticClass()`, `mClearanceData`, `mManagedByLightweightBuildableSubsystem = false`.
   - Standard interaction via `AFGHUD::OpenInteractUI` (`FGHUD.h`) opening `USinkMonitorInteractWidget`.

4. **SML Content Registration (`ModContentRegistry.cpp` & `BlueprintAssetHelperLibrary.cpp`):**
   - `/Script/SinkMonitor` native classes (`UGameWorldModule` with `bRootModule = true`, `UGameInstanceModule` with `bRootModule = true`, `UFGSchematic`, `UFGRecipe`, `UFGBuildingDescriptor`, `AFGBuildable`) are natively resolved by `UModContentRegistry::FindContentOwnerFast` and `FindPluginNameByObjectPath`.

### Verified Vanilla Asset Paths (`Content/FactoryGame/`)

- **Build Category (Special):** `/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/BC_TradingPost.BC_TradingPost_C`
- **Build SubCategory (AWESOME):** `/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/Sub_Special/SC_AWESOME.SC_AWESOME_C`
- **AWESOME Shop Category:** `/Game/FactoryGame/Schematics/ResourceSinkShopCategories/SC_RSS_Management.SC_RSS_Management_C`
- **Prerequisite Tier 2 Milestone (Resource Sink Bonus Program):** `/Game/FactoryGame/Schematics/Progression/Schematic_2-2.Schematic_2-2_C`
- **Recipe Producer (Build Gun):** `/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C`
- **Vanilla Ingredients / Cost:**
  - `/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C`
  - `/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C`
  - `/Game/FactoryGame/Resource/Parts/CopperSheet/Desc_CopperSheet.Desc_CopperSheet_C`
  - `/Game/FactoryGame/Resource/Parts/QuartzCrystal/Desc_QuartzCrystal.Desc_QuartzCrystal_C`
  - `/Game/FactoryGame/Resource/Parts/ResourceSinkCoupon/Desc_ResourceSinkCoupon.Desc_ResourceSinkCoupon_C`

---

## 2. Phase-by-Phase Architecture

### Phase 1 — Plugin Scaffold & Pure-C++20 Rate Core (Current Readiness Milestone)
- `SinkMonitorCore::FItemRateRingBuffer` & `TSinkRateTracker<KeyType>` in `SinkRateTracker.h`:
  - 60-bucket (1 bucket = 1 second) zero-drift rolling window, session totals, and persistent lifetime totals.
  - Zero Unreal headers so `Tests/test_rate_tracker.cpp` runs directly with `g++ -std=c++20 -Wall -Wextra -Werror`.
- `SinkMonitorTypes.h`: `ESinkMonitorSortMode`, `FSinkActiveSourceInfo`, `FSinkableItemEntry`, `FSinkReplicatedItemStat`.
- `SinkMonitorModule.h/.cpp`, `SinkMonitor.uplugin`, `SinkMonitor.Build.cs`, `Config/AccessTransformers.ini`, `Config/Alpakit.ini`, `Config/PluginSettings.ini`.

### 3D Model Generation & UE5 Cook Pipeline
- `assets/blender/generate_sink_monitor_terminal.py` -> `.blend`, `.fbx` (with 3 `UCX_` convex collision hulls), `T_SinkMonitor_Screen_Emissive.png`, `T_SinkMonitor_Hazard.png`, `T_Icon_SinkMonitor_256.png`, `T_Icon_SinkMonitor_512.png`, `Icon128.png`, `docs/SMR_Logo.png`, and `docs/renders/terminal_turnaround.png` (see `docs/MODEL_DESIGN.md`).
- `tools/ue_import_assets.py` -> headless `UnrealEditor-Cmd` import into `SinkMonitor/Content/` with 5 compiled PBR `UMaterial` assets and 3 LODs.

### Phase 2 — Sink Tracking Subsystem (`ASinkMonitorSubsystem`)
- Inherits `AModSubsystem` (`ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate`, `bWithTick = true`).
- **Native SML Hooks (in `FSinkMonitorModule`, guarded for runtime only):**
  1. `SUBSCRIBE_UOBJECT_METHOD(AFGBuildableResourceSink, Factory_CollectInput_Implementation, ...)`: sets/clears a `thread_local` active `AFGBuildableResourceSink*` around the original call.
  2. `SUBSCRIBE_UOBJECT_METHOD_AFTER(AFGResourceSinkSubsystem, AddPoints_ThreadSafe, ...)`: when `ReturnValue == true`, pushes `{ItemClass, ActiveSinkWeakPtrOrKey, SinkLocation}` into a lock-free `TQueue<FSinkEvent, EQueueMode::Mpsc>`.
- **Game Thread Drain & Aggregation (`ASinkMonitorSubsystem::Tick`):**
  - Drains `Mpsc` queue, updates `TSinkRateTracker<TSubclassOf<UFGItemDescriptor>>` and per-source active sink trackers (keeping sinks that consumed an item within the last 10–60s).
  - Advances ring buffer once per accumulated second (`Tick(DeltaSeconds)`).
- **Hybrid Sinkable Catalog Discovery (`RefreshSinkableCatalog()`):**
  - Reads `AFGResourceSinkSubsystem::mCachedResourceSinkPoints` (via `AccessTransformers.ini` friend access) and cross-checks `UModContentRegistry::GetObtainableItemDescriptors(...)` + `FindResourceSinkPointsForItem(...)` for all `Points > 0` solid/sinkable items (including custom/modded items).
  - Excludes `UFGResourceSinkSettings::mExcludedDescriptors` (coupons/abstract classes) and fluid/gas descriptors (`EResourceForm::RF_LIQUID` / `RF_GAS`).
- **Diagnostics:** Verbose `LogSinkMonitor` output + optional console command / chat diagnostic hooks.

### Phase 3 — Interactable Buildable & Pure-C++ Content Registration
- `ASinkMonitorBuildable` (`AFGBuildable`):
  - Sets `mIsUseable = true`, `mInteractWidgetSoftClass = USinkMonitorInteractWidget::StaticClass()`, `mHologramClass = AFGBuildableHologram::StaticClass()`, `mManagedByLightweightBuildableSubsystem = false`, `mDisplayName`, `mDescription`, and `mClearanceData` (`130 × 180 × 230 cm`).
  - Static mesh component loading `/SinkMonitor/Buildable/SM_SinkMonitorTerminal.SM_SinkMonitorTerminal` (`TSoftObjectPtr<UStaticMesh>`) with customizer mesh proxy support (`UFGColoredInstanceMeshProxy`).
- Pure-C++ `USinkMonitorDescriptor` (`UFGBuildingDescriptor`), `USinkMonitorRecipe` (`UFGRecipe`), `USinkMonitorSchematic` (`UFGSchematic`), `USinkMonitorGameWorldModule` (`bRootModule = true`), and `USinkMonitorGameInstanceModule` (`bRootModule = true`).

### Phase 4 — Pure-C++ Slate Interactive Terminal UI (`USinkMonitorInteractWidget`)
- Inherits `UFGInteractWidget` (`mUseKeyboard = true`, `mUseMouse = true`, `mRestoreFocusWhenLost = true`, `mSupportsStacking = false`) and constructs a full-featured FICSIT-styled Slate hierarchy in `RebuildWidget()` (guarded `#if !UE_SERVER`), so **no manual UMG `.uasset` creation is required**.
- Features:
  - Live periodic UI refresh (`0.5s` timer / event-driven snapshot updates).
  - Search `SEditableTextBox` filtering by item display name.
  - "Currently Sinking Only (`> 0/min`)" `SCheckBox` toggle.
  - Sort dropdown (`Rate (High -> Low)`, `Points/min`, `Point Value`, `Name (A -> Z)`, `Total Sunk`), with Pinned/Favorite items always floating to the top.
  - Scrollable virtualized/reused `SScrollBox` rows (`ItemIcon`, `DisplayName`, `PointValue`, `Items/min` green when active / muted gray when idle, `Points/min`, and click selection highlight).
  - Detail side-panel displaying high-res icon, item name, track badge (`Standard` vs `Exploration / DNA`), points per item, live `Items/min`, live `Points/min`, session & lifetime totals, **Pin/Favorite** toggle button, and active AWESOME Sinks list (with world coordinates / per-sink rate).

### Phase 5 — Multiplayer Replication, Save Persistence & Packaging
- **Multiplayer (`HasAuthority()` + `USinkMonitorRemoteCallObject`):**
  - Server runs hooks and authoritative `TSinkRateTracker`.
  - Server replicates compact `FSinkReplicatedItemStat` deltas / active rates and responds to client `Server_RequestFullSnapshot()` & `Server_TogglePinItem(ItemClass)` RPCs via `USinkMonitorRemoteCallObject` registered in `USinkMonitorGameInstanceModule::RemoteCallObjects`.
- **Save Persistence (`IFGSaveInterface` on `ASinkMonitorSubsystem`):**
  - Persists `UPROPERTY(SaveGame)` lifetime sunk totals per item (`TMap` or `TArray<FSinkSavedItemRecord>`) and pinned item classes safely across save/load (even if a mod providing an item is temporarily unloaded).
- **Packaging & Verification:**
  - Linux Editor UBT build:
    ```bash
    /mnt/Workspace/UnrealEngineCSS/Engine/Build/BatchFiles/Linux/Build.sh \
      FactoryEditor Linux Development \
      -Project="/mnt/Workspace/Antigravity/SatisfactorySpaceElevatorMOD/tools/SatisfactoryModLoader/FactoryGame.uproject" \
      -WaitMutex -FromMsBuild
    ```
  - Multi-target Shipping packaging (`Windows`, `WindowsServer`, `LinuxServer`):
    ```bash
    UE_WINE_MSVC=/home/drakkhis/.local/share/msvc-wine \
    WINE_MSVC_SHORT_PREFIX="/mnt/Workspace/Antigravity/SatisfactorySpaceElevatorMOD/tools/SatisfactoryModLoader" \
    WINE_MSVC_SHORT_DRIVE="s" \
    /mnt/Workspace/UnrealEngineCSS/Engine/Build/BatchFiles/RunUAT.sh \
      -ScriptsForProject="/mnt/Workspace/Antigravity/SatisfactorySpaceElevatorMOD/tools/SatisfactoryModLoader/FactoryGame.uproject" \
      PackagePlugin \
      -project="/mnt/Workspace/Antigravity/SatisfactorySpaceElevatorMOD/tools/SatisfactoryModLoader/FactoryGame.uproject" \
      -clientconfig=Shipping \
      -serverconfig=Shipping \
      -utf8output \
      -DLCName=SinkMonitor \
      -build \
      -platform=Win64 \
      -server \
      -serverplatform=Win64+Linux \
      -nocompileeditor \
      -installed \
      -merge
    ```

---

## 3. Confirmed Design Decisions

1. **3D Model (`docs/MODEL_DESIGN.md`):** Confirmed `110 × 160 × 225 cm` free-standing wireless FICSIT telemetry kiosk silhouette, 5 PBR materials, `2048×1024` emissive screen, `2048×512` hazard strip, 3 `UCX_` collision hulls, and automated headless Blender + UE5 `.uasset` cooking pipeline.
2. **Unlock Path (`USinkMonitorSchematic`):** Automatically unlocks with Tier 2 *Resource Sink Bonus Program* (`Schematic_2-2`). On existing saves (or AGS / Creative saves) where `Schematic_2-2` is already researched/purchased, `ASinkMonitorSubsystem` detects `SchematicManager->IsSchematicPurchased(Schematic_2-2)` on load and automatically grants `USinkMonitorSchematic` silently (`ESchematicUnlockFlags::Force | ESchematicUnlockFlags::SuppressNarrativeMessages`) so the **AWESOME Sink Monitor Terminal** is immediately added to the player's Build Menu library (`Special -> AWESOME`). On new saves, completing `Schematic_2-2` triggers `PurchasedSchematicDelegate` and unlocks it automatically.
3. **Build Recipe (`USinkMonitorRecipe`):** `4× Reinforced Iron Plate`, `8× Cable`, `4× Copper Sheet`, `2× Quartz Crystal`.
