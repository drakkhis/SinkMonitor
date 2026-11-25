#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "FGSaveInterface.h"
#include "SinkMonitorTypes.h"
#include "SinkRateTracker.h"
#include "Subsystem/ModSubsystem.h"
#include "Templates/Atomic.h"
#include "SinkMonitorSubsystem.generated.h"

class AFGBuildableResourceSink;
class UFGItemDescriptor;
class UFGSchematic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSinkMonitorStatsUpdated);

/**
 * Server-authoritative mod subsystem aggregating real-time AWESOME Sink telemetry,
 * discovering vanilla/modded sinkable items, persisting lifetime totals, replicating
 * compact deltas to clients, and managing Tier 2 schematic auto-unlocks.
 */
UCLASS(Blueprintable)
class SINKMONITOR_API ASinkMonitorSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	ASinkMonitorSubsystem();

	/** Returns the active ASinkMonitorSubsystem for World, or nullptr if unavailable. */
	static ASinkMonitorSubsystem* Get(UWorld* World);

	/** Worker-thread-safe lookup for the authoritative subsystem on ExpectedWorld. */
	static ASinkMonitorSubsystem* GetAuthorityInstanceThreadSafe(const UWorld* ExpectedWorld);

	/** Blueprint-accessible world-context accessor. */
	UFUNCTION(BlueprintPure, Category = "SinkMonitor", Meta = (DefaultToSelf = "WorldContext"))
	static ASinkMonitorSubsystem* GetSinkMonitorSubsystem(UObject* WorldContext);

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor interface

	//~ Begin IFGSaveInterface
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override;
	virtual bool NeedTransform_Implementation() override;
	virtual bool ShouldSave_Implementation() const override;
	//~ End IFGSaveInterface

	/** Enqueues one sunk item event from a Factory Tick worker thread without touching UObjects. */
	void EnqueueSunkItemFromFactoryThread(
		TSubclassOf<UFGItemDescriptor> ItemClass,
		uint64 SinkUniqueId,
		const FVector& SinkWorldLocation);

	/** Refreshes the catalog of all valid solid sinkable items (vanilla + modded). */
	UFUNCTION(BlueprintCallable, Category = "SinkMonitor")
	void RefreshSinkableCatalog();

	/** Returns all sinkable items merged with current live rates, totals, and pin flags. */
	UFUNCTION(BlueprintCallable, Category = "SinkMonitor")
	TArray<FSinkableItemEntry> GetAllSinkableItems() const;

	/** Queries telemetry for a single sinkable item class. Returns false if unknown. */
	UFUNCTION(BlueprintCallable, Category = "SinkMonitor")
	bool GetStatsForItem(TSubclassOf<UFGItemDescriptor> ItemClass, FSinkableItemEntry& OutEntry) const;

	/** Returns active AWESOME Sinks currently consuming ItemClass, sorted by rate descending. */
	UFUNCTION(BlueprintCallable, Category = "SinkMonitor")
	TArray<FSinkActiveSourceInfo> GetActiveSinksForItem(TSubclassOf<UFGItemDescriptor> ItemClass) const;

	/** Computes aggregate Points/min, active item count, and active sink count. */
	UFUNCTION(BlueprintPure, Category = "SinkMonitor")
	void GetGlobalSummary(
		float& OutTotalPointsPerMin,
		int32& OutActiveItemCount,
		int32& OutTotalActiveSinkCount) const;

	/** Toggles pinned/favorite state for ItemClass (routes via RCO on remote clients). */
	UFUNCTION(BlueprintCallable, Category = "SinkMonitor")
	void TogglePinItem(TSubclassOf<UFGItemDescriptor> ItemClass);

	/** Requests resetting current session counters while preserving lifetime totals. */
	UFUNCTION(BlueprintCallable, Category = "SinkMonitor")
	void RequestResetSessionStats();

	/** Requests an immediate server-to-client replication sync (no-op on server). */
	UFUNCTION(BlueprintCallable, Category = "SinkMonitor")
	void RequestStatsSync();

	// Server-authority mutation helpers invoked directly or via USinkMonitorRemoteCallObject.
	void TogglePinItemAuthoritative(TSubclassOf<UFGItemDescriptor> ItemClass);
	void ResetSessionStatsAuthoritative();
	void ForceReplicationSync();

	UPROPERTY(BlueprintAssignable, Category = "SinkMonitor")
	FOnSinkMonitorStatsUpdated OnSinkStatsUpdated;

protected:
	UFUNCTION()
	void OnRep_ReplicatedStats();

	UFUNCTION()
	void OnRep_ReplicatedSources();

	UFUNCTION()
	void HandleSchematicPurchased(TSubclassOf<UFGSchematic> PurchasedSchematic);

private:
	struct FSinkQueuedEvent
	{
		TSubclassOf<UFGItemDescriptor> ItemClass;
		uint64 SinkUniqueId = 0;
		FVector SinkWorldLocation = FVector::ZeroVector;
	};

	struct FCatalogItemMeta
	{
		TSubclassOf<UFGItemDescriptor> ItemClass;
		FText DisplayName;
		int32 PointValue = 0;
		EResourceSinkTrack SinkTrack = EResourceSinkTrack::RST_Default;
	};

	struct FSinkSourceKey
	{
		UClass* ItemClass = nullptr;
		uint64 SinkUniqueId = 0;

		bool operator==(const FSinkSourceKey& Other) const
		{
			return ItemClass == Other.ItemClass && SinkUniqueId == Other.SinkUniqueId;
		}
	};

	struct FSinkSourceKeyHash
	{
		std::size_t operator()(const FSinkSourceKey& Key) const noexcept
		{
			const std::size_t H1 = std::hash<const void*>{}(Key.ItemClass);
			const std::size_t H2 = std::hash<uint64>{}(Key.SinkUniqueId);
			return H1 ^ (H2 + 0x9e3779b97f4a7c15ULL + (H1 << 6) + (H1 >> 2));
		}
	};

	struct FSinkSourceRuntimeMeta
	{
		FVector WorldLocation = FVector::ZeroVector;
		double LastSunkWorldTime = 0.0;
	};

	void DrainPendingSinkEvents();
	void TickTrackersAndPruneSources(float DeltaSeconds);
	void RebuildReplicatedSnapshot();
	void RebuildClientLookupCaches();
	void RegisterCatalogItem(
		TSubclassOf<UFGItemDescriptor> ItemClass,
		int32 PointValue,
		EResourceSinkTrack SinkTrack);
	void RestoreSavedRecords();
	void BindSchematicDelegate();
	void SyncTerminalSchematicUnlock();
	void HandleDelayedInitSync();

	TQueue<FSinkQueuedEvent, EQueueMode::Mpsc> PendingSinkEvents;

	TArray<FCatalogItemMeta> CatalogList;
	TMap<UClass*, int32> CatalogIndexByClass;

	SinkMonitorCore::TSinkRateTracker<UClass*> ItemTracker;
	SinkMonitorCore::TSinkRateTracker<FSinkSourceKey, FSinkSourceKeyHash> SourceTracker;
	TMap<UClass*, TArray<uint64>> ActiveSinkIdsByItem;
	TMap<TPair<UClass*, uint64>, FSinkSourceRuntimeMeta> SourceMetaMap;

	/** Strong GC-rooted references for every tracked/cataloged descriptor class. */
	UPROPERTY(Transient)
	TArray<TSubclassOf<UFGItemDescriptor>> RootedItemClasses;

	UPROPERTY(Transient)
	TSet<TSubclassOf<UFGItemDescriptor>> PinnedItems;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedStats)
	TArray<FSinkReplicatedItemStat> ReplicatedItemStats;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedSources)
	TArray<FSinkReplicatedSourceEntry> ReplicatedActiveSources;

	TMap<UClass*, FSinkReplicatedItemStat> ClientStatsCache;
	TMap<UClass*, TArray<FSinkActiveSourceInfo>> ClientSourcesCache;

	UPROPERTY(SaveGame)
	int32 SaveSchemaVersion = 1;

	UPROPERTY(SaveGame)
	TArray<FSinkSavedItemRecord> SavedItemRecords;

	FTimerHandle DelayedSyncTimerHandle;
	float ReplicationAccumulatorSeconds = 0.0f;
	bool bReplicationDirty = false;
	bool bPendingSaveRestore = false;
	bool bSchematicDelegateBound = false;
};
