#include "SinkMonitorSubsystem.h"

#include "Engine/World.h"
#include "FGPlayerController.h"
#include "FGRecipeManager.h"
#include "FGResourceSinkSubsystem.h"
#include "FGSchematicManager.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Registry/ModContentRegistry.h"
#include "SinkMonitorContent.h"
#include "SinkMonitorModule.h"
#include "SinkMonitorRemoteCallObject.h"
#include "Subsystem/SubsystemActorManager.h"
#include "TimerManager.h"

namespace
{
TAtomic<ASinkMonitorSubsystem*> GAuthoritySinkMonitorSubsystem{nullptr};
TAtomic<const UWorld*> GAuthoritySinkMonitorWorld{nullptr};

constexpr float SinkMonitorReplicationIntervalSeconds = 0.5f;
constexpr float SinkMonitorMaxSimDeltaSeconds = 0.25f;
constexpr float SinkMonitorSourceExpirySeconds = 60.0f;
constexpr int32 SinkMonitorMaxSourcesPerItem = 8;

const TCHAR* Tier2ResourceSinkSchematicPath =
	TEXT("/Game/FactoryGame/Schematics/Progression/Schematic_2-2.Schematic_2-2_C");
const TCHAR* VanillaResourceSinkRecipePath =
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ResourceSink.Recipe_ResourceSink_C");
} // namespace

ASinkMonitorSubsystem::ASinkMonitorSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.0f);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
}

ASinkMonitorSubsystem* ASinkMonitorSubsystem::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	if (USubsystemActorManager* SubsystemManager = World->GetSubsystem<USubsystemActorManager>())
	{
		return SubsystemManager->GetSubsystemActor<ASinkMonitorSubsystem>();
	}
	return nullptr;
}

ASinkMonitorSubsystem* ASinkMonitorSubsystem::GetAuthorityInstanceThreadSafe(const UWorld* ExpectedWorld)
{
	ASinkMonitorSubsystem* Sub = GAuthoritySinkMonitorSubsystem.Load();
	if (!Sub)
	{
		return nullptr;
	}
	if (ExpectedWorld && GAuthoritySinkMonitorWorld.Load() != ExpectedWorld)
	{
		return nullptr;
	}
	return Sub;
}

ASinkMonitorSubsystem* ASinkMonitorSubsystem::GetSinkMonitorSubsystem(UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	return Get(WorldContext->GetWorld());
}

void ASinkMonitorSubsystem::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GAuthoritySinkMonitorWorld.Store(GetWorld());
		GAuthoritySinkMonitorSubsystem.Store(this);
	}

	RefreshSinkableCatalog();

	if (HasAuthority())
	{
		if (bPendingSaveRestore)
		{
			RestoreSavedRecords();
		}
		BindSchematicDelegate();
		SyncTerminalSchematicUnlock();
		bReplicationDirty = true;
		RebuildReplicatedSnapshot();
	}
	else
	{
		RebuildClientLookupCaches();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DelayedSyncTimerHandle,
			this,
			&ASinkMonitorSubsystem::HandleDelayedInitSync,
			1.5f,
			false);
	}
}

void ASinkMonitorSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DelayedSyncTimerHandle);

		if (bSchematicDelegateBound)
		{
			if (AFGSchematicManager* SchematicMgr = AFGSchematicManager::Get(World))
			{
				SchematicMgr->PurchasedSchematicDelegate.RemoveDynamic(
					this, &ASinkMonitorSubsystem::HandleSchematicPurchased);
			}
			bSchematicDelegateBound = false;
		}
	}

	if (HasAuthority() && GAuthoritySinkMonitorSubsystem.Load() == this)
	{
		GAuthoritySinkMonitorSubsystem.Store(nullptr);
		GAuthoritySinkMonitorWorld.Store(nullptr);
	}

	Super::EndPlay(EndPlayReason);
}

void ASinkMonitorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		return;
	}

	DrainPendingSinkEvents();
	TickTrackersAndPruneSources(FMath::Clamp(DeltaTime, 0.0f, SinkMonitorMaxSimDeltaSeconds));

	ReplicationAccumulatorSeconds += DeltaTime;
	if (ReplicationAccumulatorSeconds >= SinkMonitorReplicationIntervalSeconds || bReplicationDirty)
	{
		ReplicationAccumulatorSeconds = 0.0f;
		bReplicationDirty = false;
		RebuildReplicatedSnapshot();
	}
}

void ASinkMonitorSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASinkMonitorSubsystem, ReplicatedItemStats);
	DOREPLIFETIME(ASinkMonitorSubsystem, ReplicatedActiveSources);
}

void ASinkMonitorSubsystem::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	SavedItemRecords.Reset();
	for (const TSubclassOf<UFGItemDescriptor>& ItemClass : RootedItemClasses)
	{
		UClass* RawClass = ItemClass.Get();
		if (!RawClass)
		{
			continue;
		}
		const SinkMonitorCore::FItemRateRingBuffer* Buffer = ItemTracker.Find(RawClass);
		const int64 Lifetime = Buffer ? Buffer->GetLifetimeTotal() : 0;
		const bool bPinned = PinnedItems.Contains(ItemClass);
		if (Lifetime > 0 || bPinned)
		{
			FSinkSavedItemRecord Record;
			Record.ItemClass = ItemClass;
			Record.LifetimeTotalSunk = Lifetime;
			Record.bIsPinned = bPinned;
			SavedItemRecords.Add(MoveTemp(Record));
		}
	}
}

void ASinkMonitorSubsystem::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
}

void ASinkMonitorSubsystem::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
}

void ASinkMonitorSubsystem::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	bPendingSaveRestore = true;
}

void ASinkMonitorSubsystem::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects)
{
}

bool ASinkMonitorSubsystem::NeedTransform_Implementation()
{
	return false;
}

bool ASinkMonitorSubsystem::ShouldSave_Implementation() const
{
	return true;
}

void ASinkMonitorSubsystem::EnqueueSunkItemFromFactoryThread(
	TSubclassOf<UFGItemDescriptor> ItemClass,
	uint64 SinkUniqueId,
	const FVector& SinkWorldLocation)
{
	if (!ItemClass)
	{
		return;
	}

	FSinkQueuedEvent Event;
	Event.ItemClass = ItemClass;
	Event.SinkUniqueId = SinkUniqueId;
	Event.SinkWorldLocation = SinkWorldLocation;
	PendingSinkEvents.Enqueue(Event);
}

void ASinkMonitorSubsystem::RefreshSinkableCatalog()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AFGResourceSinkSubsystem* SinkSubsystem = AFGResourceSinkSubsystem::Get(World);
	if (!SinkSubsystem)
	{
		return;
	}

	// 1. Read all points already cached in AFGResourceSinkSubsystem (via AccessTransformers).
	for (const TPair<TSubclassOf<UFGItemDescriptor>, FResourceSinkValuePair32>& Pair : SinkSubsystem->mCachedResourceSinkPoints)
	{
		RegisterCatalogItem(Pair.Key, Pair.Value.Value, Pair.Value.TrackType);
	}

	// 2. Cross-check ModContentRegistry to catch late-registered or modded descriptors.
	if (UModContentRegistry* Registry = UModContentRegistry::Get(World))
	{
		const TArray<FGameObjectRegistration> LoadedItems = Registry->GetLoadedItemDescriptors();
		for (const FGameObjectRegistration& Reg : LoadedItems)
		{
			if (Reg.HasAnyFlags(EGameObjectRegistrationFlags::Removed))
			{
				continue;
			}
			UClass* ObjClass = Cast<UClass>(Reg.RegisteredObject.Get());
			if (!ObjClass || !ObjClass->IsChildOf(UFGItemDescriptor::StaticClass()))
			{
				continue;
			}

			TSubclassOf<UFGItemDescriptor> ItemClass(ObjClass);
			if (CatalogIndexByClass.Contains(ObjClass))
			{
				continue;
			}

			int32 Points = 0;
			EResourceSinkTrack Track = EResourceSinkTrack::RST_Default;
			if (SinkSubsystem->FindResourceSinkPointsForItem(ItemClass, Points, Track))
			{
				RegisterCatalogItem(ItemClass, Points, Track);
			}
		}
	}

	CatalogList.Sort([](const FCatalogItemMeta& A, const FCatalogItemMeta& B)
	{
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});

	CatalogIndexByClass.Reset();
	for (int32 Idx = 0; Idx < CatalogList.Num(); ++Idx)
	{
		if (CatalogList[Idx].ItemClass)
		{
			CatalogIndexByClass.Add(CatalogList[Idx].ItemClass.Get(), Idx);
		}
	}

	UE_LOG(LogSinkMonitor, Log, TEXT("Sinkable item catalog refreshed (%d valid items)."), CatalogList.Num());
}

void ASinkMonitorSubsystem::RegisterCatalogItem(
	TSubclassOf<UFGItemDescriptor> ItemClass,
	int32 PointValue,
	EResourceSinkTrack SinkTrack)
{
	UClass* RawClass = ItemClass.Get();
	if (!RawClass || RawClass->HasAnyClassFlags(CLASS_Abstract) || PointValue <= 0)
	{
		return;
	}

	if (UFGItemDescriptor::GetForm(ItemClass) != EResourceForm::RF_SOLID)
	{
		return;
	}

	if (const int32* ExistingIdx = CatalogIndexByClass.Find(RawClass))
	{
		CatalogList[*ExistingIdx].PointValue = PointValue;
		CatalogList[*ExistingIdx].SinkTrack = SinkTrack;
		return;
	}

	FText DisplayName = UFGItemDescriptor::GetItemName(ItemClass);
	if (DisplayName.IsEmpty())
	{
		DisplayName = FText::FromString(RawClass->GetName());
	}

	FCatalogItemMeta Meta;
	Meta.ItemClass = ItemClass;
	Meta.DisplayName = MoveTemp(DisplayName);
	Meta.PointValue = PointValue;
	Meta.SinkTrack = SinkTrack;

	const int32 NewIndex = CatalogList.Add(MoveTemp(Meta));
	CatalogIndexByClass.Add(RawClass, NewIndex);
	RootedItemClasses.AddUnique(ItemClass);
}

void ASinkMonitorSubsystem::DrainPendingSinkEvents()
{
	UWorld* World = GetWorld();
	const double NowWorldTime = World ? World->GetTimeSeconds() : 0.0;
	AFGResourceSinkSubsystem* SinkSubsystem = World ? AFGResourceSinkSubsystem::Get(World) : nullptr;

	FSinkQueuedEvent Event;
	bool bAnyDrained = false;
	while (PendingSinkEvents.Dequeue(Event))
	{
		UClass* RawClass = Event.ItemClass.Get();
		if (!RawClass)
		{
			continue;
		}

		bAnyDrained = true;
		RootedItemClasses.AddUnique(Event.ItemClass);

		// Lazy-discover dynamically registered items if they were not in initial catalog.
		if (!CatalogIndexByClass.Contains(RawClass) && SinkSubsystem)
		{
			int32 Points = 0;
			EResourceSinkTrack Track = EResourceSinkTrack::RST_Default;
			if (SinkSubsystem->FindResourceSinkPointsForItem(Event.ItemClass, Points, Track))
			{
				RegisterCatalogItem(Event.ItemClass, Points, Track);
			}
		}

		ItemTracker.RecordSunk(RawClass, 1);

		if (Event.SinkUniqueId != 0)
		{
			const FSinkSourceKey SourceKey{RawClass, Event.SinkUniqueId};
			SourceTracker.RecordSunk(SourceKey, 1);

			TArray<uint64>& SinkIds = ActiveSinkIdsByItem.FindOrAdd(RawClass);
			SinkIds.AddUnique(Event.SinkUniqueId);

			FSinkSourceRuntimeMeta& SourceMeta = SourceMetaMap.FindOrAdd(TPair<UClass*, uint64>(RawClass, Event.SinkUniqueId));
			SourceMeta.WorldLocation = Event.SinkWorldLocation;
			SourceMeta.LastSunkWorldTime = NowWorldTime;
		}
	}

	if (bAnyDrained)
	{
		bReplicationDirty = true;
	}
}

void ASinkMonitorSubsystem::TickTrackersAndPruneSources(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	ItemTracker.Tick(static_cast<double>(DeltaSeconds));
	SourceTracker.Tick(static_cast<double>(DeltaSeconds));

	const double NowWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (auto It = ActiveSinkIdsByItem.CreateIterator(); It; ++It)
	{
		UClass* RawClass = It->Key;
		TArray<uint64>& SinkIds = It->Value;

		for (int32 i = SinkIds.Num() - 1; i >= 0; --i)
		{
			const uint64 SinkId = SinkIds[i];
			const FSinkSourceKey SourceKey{RawClass, SinkId};
			const SinkMonitorCore::FItemRateRingBuffer* Buffer = SourceTracker.Find(SourceKey);
			const FSinkSourceRuntimeMeta* Meta = SourceMetaMap.Find(TPair<UClass*, uint64>(RawClass, SinkId));

			const bool bRateZero = !Buffer || Buffer->GetItemsPerMinute() <= 0.0;
			const bool bExpired = !Meta || (NowWorldTime - Meta->LastSunkWorldTime) > SinkMonitorSourceExpirySeconds;
			if (bRateZero && bExpired)
			{
				SourceMetaMap.Remove(TPair<UClass*, uint64>(RawClass, SinkId));
				SinkIds.RemoveAtSwap(i);
			}
		}

		if (SinkIds.IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

void ASinkMonitorSubsystem::RebuildReplicatedSnapshot()
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedItemStats.Reset();
	ReplicatedActiveSources.Reset();

	for (const TSubclassOf<UFGItemDescriptor>& ItemClass : RootedItemClasses)
	{
		UClass* RawClass = ItemClass.Get();
		if (!RawClass)
		{
			continue;
		}

		const SinkMonitorCore::FItemRateRingBuffer* Buffer = ItemTracker.Find(RawClass);
		const float RatePerMin = Buffer ? static_cast<float>(Buffer->GetItemsPerMinute()) : 0.0f;
		const int64 SessionTotal = Buffer ? Buffer->GetSessionTotal() : 0;
		const int64 LifetimeTotal = Buffer ? Buffer->GetLifetimeTotal() : 0;
		const bool bPinned = PinnedItems.Contains(ItemClass);

		TArray<FSinkActiveSourceInfo> ItemSources = GetActiveSinksForItem(ItemClass);
		const int32 ActiveSinkCount = ItemSources.Num();

		if (RatePerMin > 0.0f || SessionTotal > 0 || LifetimeTotal > 0 || bPinned || ActiveSinkCount > 0)
		{
			FSinkReplicatedItemStat Stat;
			Stat.ItemClass = ItemClass;
			Stat.CurrentItemsPerMin = RatePerMin;
			Stat.SessionTotalSunk = SessionTotal;
			Stat.LifetimeTotalSunk = LifetimeTotal;
			Stat.ActiveSinkCount = ActiveSinkCount;
			Stat.bIsPinned = bPinned;
			ReplicatedItemStats.Add(Stat);

			const int32 SourceLimit = FMath::Min(ItemSources.Num(), SinkMonitorMaxSourcesPerItem);
			for (int32 i = 0; i < SourceLimit; ++i)
			{
				FSinkReplicatedSourceEntry Entry;
				Entry.ItemClass = ItemClass;
				Entry.WorldLocation = FVector_NetQuantize(ItemSources[i].WorldLocation);
				Entry.ItemsPerMin = ItemSources[i].ItemsPerMin;
				Entry.SecondsSinceLastSunk = ItemSources[i].SecondsSinceLastSunk;
				ReplicatedActiveSources.Add(Entry);
			}
		}
	}

	ForceNetUpdate();
	OnSinkStatsUpdated.Broadcast();
}

void ASinkMonitorSubsystem::RebuildClientLookupCaches()
{
	ClientStatsCache.Reset();
	for (const FSinkReplicatedItemStat& Stat : ReplicatedItemStats)
	{
		if (Stat.ItemClass)
		{
			ClientStatsCache.Add(Stat.ItemClass.Get(), Stat);
		}
	}

	ClientSourcesCache.Reset();
	for (const FSinkReplicatedSourceEntry& Entry : ReplicatedActiveSources)
	{
		if (!Entry.ItemClass)
		{
			continue;
		}
		FSinkActiveSourceInfo Info;
		Info.WorldLocation = FVector(Entry.WorldLocation);
		Info.ItemsPerMin = Entry.ItemsPerMin;
		Info.SecondsSinceLastSunk = Entry.SecondsSinceLastSunk;
		ClientSourcesCache.FindOrAdd(Entry.ItemClass.Get()).Add(Info);
	}
}

void ASinkMonitorSubsystem::OnRep_ReplicatedStats()
{
	RebuildClientLookupCaches();
	OnSinkStatsUpdated.Broadcast();
}

void ASinkMonitorSubsystem::OnRep_ReplicatedSources()
{
	RebuildClientLookupCaches();
	OnSinkStatsUpdated.Broadcast();
}

TArray<FSinkableItemEntry> ASinkMonitorSubsystem::GetAllSinkableItems() const
{
	TArray<FSinkableItemEntry> Result;
	Result.Reserve(CatalogList.Num());

	for (const FCatalogItemMeta& Meta : CatalogList)
	{
		FSinkableItemEntry Entry;
		Entry.ItemClass = Meta.ItemClass;
		Entry.DisplayName = Meta.DisplayName;
		Entry.PointValue = Meta.PointValue;
		Entry.SinkTrack = Meta.SinkTrack;

		UClass* RawClass = Meta.ItemClass.Get();
		if (HasAuthority())
		{
			if (const SinkMonitorCore::FItemRateRingBuffer* Buffer = ItemTracker.Find(RawClass))
			{
				Entry.CurrentItemsPerMin = static_cast<float>(Buffer->GetItemsPerMinute());
				Entry.SessionTotalSunk = Buffer->GetSessionTotal();
				Entry.LifetimeTotalSunk = Buffer->GetLifetimeTotal();
			}
			Entry.bIsPinned = PinnedItems.Contains(Meta.ItemClass);
			if (const TArray<uint64>* SinkIds = ActiveSinkIdsByItem.Find(RawClass))
			{
				int32 ActiveCount = 0;
				for (uint64 SinkId : *SinkIds)
				{
					const SinkMonitorCore::FItemRateRingBuffer* SrcBuf = SourceTracker.Find(FSinkSourceKey{RawClass, SinkId});
					if (SrcBuf && SrcBuf->GetItemsPerMinute() > 0.0)
					{
						++ActiveCount;
					}
				}
				Entry.ActiveSinkCount = ActiveCount;
			}
		}
		else if (const FSinkReplicatedItemStat* Stat = ClientStatsCache.Find(RawClass))
		{
			Entry.CurrentItemsPerMin = Stat->CurrentItemsPerMin;
			Entry.SessionTotalSunk = Stat->SessionTotalSunk;
			Entry.LifetimeTotalSunk = Stat->LifetimeTotalSunk;
			Entry.ActiveSinkCount = Stat->ActiveSinkCount;
			Entry.bIsPinned = Stat->bIsPinned;
		}

		Entry.CurrentPointsPerMin = Entry.CurrentItemsPerMin * static_cast<float>(Entry.PointValue);
		Result.Add(MoveTemp(Entry));
	}

	return Result;
}

bool ASinkMonitorSubsystem::GetStatsForItem(
	TSubclassOf<UFGItemDescriptor> ItemClass,
	FSinkableItemEntry& OutEntry) const
{
	UClass* RawClass = ItemClass.Get();
	if (!RawClass)
	{
		return false;
	}

	const int32* IndexPtr = CatalogIndexByClass.Find(RawClass);
	if (!IndexPtr || !CatalogList.IsValidIndex(*IndexPtr))
	{
		return false;
	}

	const FCatalogItemMeta& Meta = CatalogList[*IndexPtr];
	OutEntry.ItemClass = Meta.ItemClass;
	OutEntry.DisplayName = Meta.DisplayName;
	OutEntry.PointValue = Meta.PointValue;
	OutEntry.SinkTrack = Meta.SinkTrack;

	if (HasAuthority())
	{
		if (const SinkMonitorCore::FItemRateRingBuffer* Buffer = ItemTracker.Find(RawClass))
		{
			OutEntry.CurrentItemsPerMin = static_cast<float>(Buffer->GetItemsPerMinute());
			OutEntry.SessionTotalSunk = Buffer->GetSessionTotal();
			OutEntry.LifetimeTotalSunk = Buffer->GetLifetimeTotal();
		}
		else
		{
			OutEntry.CurrentItemsPerMin = 0.0f;
			OutEntry.SessionTotalSunk = 0;
			OutEntry.LifetimeTotalSunk = 0;
		}
		OutEntry.bIsPinned = PinnedItems.Contains(ItemClass);
		OutEntry.ActiveSinkCount = GetActiveSinksForItem(ItemClass).Num();
	}
	else if (const FSinkReplicatedItemStat* Stat = ClientStatsCache.Find(RawClass))
	{
		OutEntry.CurrentItemsPerMin = Stat->CurrentItemsPerMin;
		OutEntry.SessionTotalSunk = Stat->SessionTotalSunk;
		OutEntry.LifetimeTotalSunk = Stat->LifetimeTotalSunk;
		OutEntry.ActiveSinkCount = Stat->ActiveSinkCount;
		OutEntry.bIsPinned = Stat->bIsPinned;
	}
	else
	{
		OutEntry.CurrentItemsPerMin = 0.0f;
		OutEntry.SessionTotalSunk = 0;
		OutEntry.LifetimeTotalSunk = 0;
		OutEntry.ActiveSinkCount = 0;
		OutEntry.bIsPinned = false;
	}

	OutEntry.CurrentPointsPerMin = OutEntry.CurrentItemsPerMin * static_cast<float>(OutEntry.PointValue);
	return true;
}

TArray<FSinkActiveSourceInfo> ASinkMonitorSubsystem::GetActiveSinksForItem(
	TSubclassOf<UFGItemDescriptor> ItemClass) const
{
	TArray<FSinkActiveSourceInfo> Results;
	UClass* RawClass = ItemClass.Get();
	if (!RawClass)
	{
		return Results;
	}

	if (!HasAuthority())
	{
		if (const TArray<FSinkActiveSourceInfo>* Cached = ClientSourcesCache.Find(RawClass))
		{
			Results = *Cached;
		}
		return Results;
	}

	const TArray<uint64>* SinkIds = ActiveSinkIdsByItem.Find(RawClass);
	if (!SinkIds)
	{
		return Results;
	}

	const double NowWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (uint64 SinkId : *SinkIds)
	{
		const SinkMonitorCore::FItemRateRingBuffer* Buffer = SourceTracker.Find(FSinkSourceKey{RawClass, SinkId});
		if (!Buffer || Buffer->GetItemsPerMinute() <= 0.0)
		{
			continue;
		}

		const FSinkSourceRuntimeMeta* Meta = SourceMetaMap.Find(TPair<UClass*, uint64>(RawClass, SinkId));
		FSinkActiveSourceInfo Info;
		Info.WorldLocation = Meta ? Meta->WorldLocation : FVector::ZeroVector;
		Info.ItemsPerMin = static_cast<float>(Buffer->GetItemsPerMinute());
		Info.SecondsSinceLastSunk = Meta ? static_cast<float>(FMath::Max(0.0, NowWorldTime - Meta->LastSunkWorldTime)) : 0.0f;
		Results.Add(Info);
	}

	Results.Sort([](const FSinkActiveSourceInfo& A, const FSinkActiveSourceInfo& B)
	{
		return A.ItemsPerMin > B.ItemsPerMin;
	});
	return Results;
}

void ASinkMonitorSubsystem::GetGlobalSummary(
	float& OutTotalPointsPerMin,
	int32& OutActiveItemCount,
	int32& OutTotalActiveSinkCount) const
{
	OutTotalPointsPerMin = 0.0f;
	OutActiveItemCount = 0;
	OutTotalActiveSinkCount = 0;

	const TArray<FSinkableItemEntry> Entries = GetAllSinkableItems();
	for (const FSinkableItemEntry& Entry : Entries)
	{
		if (Entry.CurrentItemsPerMin > 0.0f)
		{
			++OutActiveItemCount;
			OutTotalPointsPerMin += Entry.CurrentPointsPerMin;
		}
	}

	if (HasAuthority())
	{
		TSet<uint64> UniqueActiveSinks;
		for (const TPair<UClass*, TArray<uint64>>& Pair : ActiveSinkIdsByItem)
		{
			UClass* RawClass = Pair.Key;
			for (uint64 SinkId : Pair.Value)
			{
				const SinkMonitorCore::FItemRateRingBuffer* SrcBuf = SourceTracker.Find(FSinkSourceKey{RawClass, SinkId});
				if (SrcBuf && SrcBuf->GetItemsPerMinute() > 0.0)
				{
					UniqueActiveSinks.Add(SinkId);
				}
			}
		}
		OutTotalActiveSinkCount = UniqueActiveSinks.Num();
	}
	else
	{
		TSet<FIntVector> UniqueLocations;
		for (const TPair<UClass*, TArray<FSinkActiveSourceInfo>>& Pair : ClientSourcesCache)
		{
			for (const FSinkActiveSourceInfo& Info : Pair.Value)
			{
				if (Info.ItemsPerMin > 0.0f)
				{
					UniqueLocations.Add(FIntVector(
						FMath::RoundToInt(Info.WorldLocation.X),
						FMath::RoundToInt(Info.WorldLocation.Y),
						FMath::RoundToInt(Info.WorldLocation.Z)));
				}
			}
		}
		OutTotalActiveSinkCount = UniqueLocations.Num();
	}
}

void ASinkMonitorSubsystem::TogglePinItem(TSubclassOf<UFGItemDescriptor> ItemClass)
{
	if (!ItemClass)
	{
		return;
	}

	if (HasAuthority())
	{
		TogglePinItemAuthoritative(ItemClass);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AFGPlayerController* LocalPC = Cast<AFGPlayerController>(World->GetFirstPlayerController()))
		{
			if (USinkMonitorRemoteCallObject* RCO = LocalPC->GetRemoteCallObjectOfClass<USinkMonitorRemoteCallObject>())
			{
				RCO->Server_TogglePinItem(ItemClass);
			}
		}
	}
}

void ASinkMonitorSubsystem::RequestResetSessionStats()
{
	if (HasAuthority())
	{
		ResetSessionStatsAuthoritative();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AFGPlayerController* LocalPC = Cast<AFGPlayerController>(World->GetFirstPlayerController()))
		{
			if (USinkMonitorRemoteCallObject* RCO = LocalPC->GetRemoteCallObjectOfClass<USinkMonitorRemoteCallObject>())
			{
				RCO->Server_ResetSessionStats();
			}
		}
	}
}

void ASinkMonitorSubsystem::RequestStatsSync()
{
	if (HasAuthority())
	{
		RebuildReplicatedSnapshot();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AFGPlayerController* LocalPC = Cast<AFGPlayerController>(World->GetFirstPlayerController()))
		{
			if (USinkMonitorRemoteCallObject* RCO = LocalPC->GetRemoteCallObjectOfClass<USinkMonitorRemoteCallObject>())
			{
				RCO->Server_RequestStatsSync();
			}
		}
	}
}

void ASinkMonitorSubsystem::TogglePinItemAuthoritative(TSubclassOf<UFGItemDescriptor> ItemClass)
{
	if (!HasAuthority() || !ItemClass)
	{
		return;
	}

	RootedItemClasses.AddUnique(ItemClass);
	if (PinnedItems.Contains(ItemClass))
	{
		PinnedItems.Remove(ItemClass);
	}
	else
	{
		PinnedItems.Add(ItemClass);
	}

	bReplicationDirty = true;
	RebuildReplicatedSnapshot();
}

void ASinkMonitorSubsystem::ResetSessionStatsAuthoritative()
{
	if (!HasAuthority())
	{
		return;
	}

	ItemTracker.ResetSession();
	SourceTracker.ResetSession();
	ActiveSinkIdsByItem.Reset();
	SourceMetaMap.Reset();
	bReplicationDirty = true;
	RebuildReplicatedSnapshot();
}

void ASinkMonitorSubsystem::ForceReplicationSync()
{
	if (HasAuthority())
	{
		RebuildReplicatedSnapshot();
	}
}

void ASinkMonitorSubsystem::RestoreSavedRecords()
{
	bPendingSaveRestore = false;

	for (const FSinkSavedItemRecord& Record : SavedItemRecords)
	{
		UClass* LoadedClass = Record.ItemClass.LoadSynchronous();
		if (!LoadedClass || !LoadedClass->IsChildOf(UFGItemDescriptor::StaticClass()))
		{
			continue;
		}

		TSubclassOf<UFGItemDescriptor> ItemClass(LoadedClass);
		RootedItemClasses.AddUnique(ItemClass);
		if (Record.LifetimeTotalSunk > 0)
		{
			ItemTracker.GetOrAdd(LoadedClass).SetLifetimeTotal(Record.LifetimeTotalSunk);
		}
		if (Record.bIsPinned)
		{
			PinnedItems.Add(ItemClass);
		}
	}

	bReplicationDirty = true;
}

void ASinkMonitorSubsystem::BindSchematicDelegate()
{
	if (bSchematicDelegateBound || !HasAuthority())
	{
		return;
	}

	if (AFGSchematicManager* SchematicMgr = AFGSchematicManager::Get(GetWorld()))
	{
		SchematicMgr->PurchasedSchematicDelegate.AddDynamic(
			this, &ASinkMonitorSubsystem::HandleSchematicPurchased);
		bSchematicDelegateBound = true;
	}
}

void ASinkMonitorSubsystem::HandleSchematicPurchased(TSubclassOf<UFGSchematic> PurchasedSchematic)
{
	if (HasAuthority())
	{
		SyncTerminalSchematicUnlock();
	}
}

void ASinkMonitorSubsystem::SyncTerminalSchematicUnlock()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AFGSchematicManager* SchematicMgr = AFGSchematicManager::Get(World);
	AFGRecipeManager* RecipeMgr = AFGRecipeManager::Get(World);
	if (!SchematicMgr)
	{
		return;
	}

	const TSubclassOf<UFGSchematic> MonitorSchematic = USinkMonitorSchematic::StaticClass();
	const TSubclassOf<UFGRecipe> MonitorRecipe = USinkMonitorRecipe::StaticClass();
	const bool bHasSchematic = SchematicMgr->IsSchematicPurchased(MonitorSchematic);
	const bool bHasRecipe = RecipeMgr && RecipeMgr->IsRecipeAvailable(MonitorRecipe);
	if (bHasSchematic && bHasRecipe)
	{
		return;
	}

	const TSubclassOf<UFGSchematic> Tier2Schematic =
		LoadClass<UFGSchematic>(nullptr, Tier2ResourceSinkSchematicPath);
	const bool bTier2Unlocked = Tier2Schematic && SchematicMgr->IsSchematicPurchased(Tier2Schematic);

	const TSubclassOf<UFGRecipe> VanillaSinkRecipe =
		LoadClass<UFGRecipe>(nullptr, VanillaResourceSinkRecipePath);
	const bool bSinkRecipeAvailable = RecipeMgr && VanillaSinkRecipe && RecipeMgr->IsRecipeAvailable(VanillaSinkRecipe);

	if (!bTier2Unlocked && !bSinkRecipeAvailable)
	{
		return;
	}

	if (!bHasSchematic)
	{
		SchematicMgr->GiveAccessToSchematic(
			MonitorSchematic,
			nullptr,
			ESchematicUnlockFlags::Force | ESchematicUnlockFlags::SuppressNarrativeMessages);
		UE_LOG(LogSinkMonitor, Log,
			TEXT("Granted USinkMonitorSchematic automatically (Tier2Unlocked=%d, SinkRecipeAvailable=%d)."),
			bTier2Unlocked ? 1 : 0,
			bSinkRecipeAvailable ? 1 : 0);
	}

	if (RecipeMgr && !RecipeMgr->IsRecipeAvailable(MonitorRecipe))
	{
		RecipeMgr->AddAvailableRecipe(MonitorRecipe);
		UE_LOG(LogSinkMonitor, Log, TEXT("Added USinkMonitorRecipe to RecipeManager."));
	}
}

void ASinkMonitorSubsystem::HandleDelayedInitSync()
{
	RefreshSinkableCatalog();

	if (HasAuthority())
	{
		if (bPendingSaveRestore)
		{
			RestoreSavedRecords();
		}
		BindSchematicDelegate();
		SyncTerminalSchematicUnlock();
		bReplicationDirty = true;
		RebuildReplicatedSnapshot();
	}
	else
	{
		RequestStatsSync();
	}
}
