#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "FGResourceSinkSubsystem.h"
#include "Resources/FGItemDescriptor.h"
#include "SinkMonitorTypes.generated.h"

/**
 * Track filter modes exposed in the AWESOME Sink Monitor terminal UI.
 */
UENUM(BlueprintType)
enum class ESinkMonitorTrackFilter : uint8
{
	All UMETA(DisplayName = "All Tracks"),
	DefaultOnly UMETA(DisplayName = "Standard Track"),
	ExplorationOnly UMETA(DisplayName = "Exploration / DNA")
};

/**
 * Sorting options exposed in the AWESOME Sink Monitor terminal UI.
 */
UENUM(BlueprintType)
enum class ESinkMonitorSortMode : uint8
{
	RateDesc UMETA(DisplayName = "Sinking Rate (High -> Low)"),
	PointsPerMinDesc UMETA(DisplayName = "Points/Min (High -> Low)"),
	PointValueDesc UMETA(DisplayName = "Point Value (High -> Low)"),
	NameAsc UMETA(DisplayName = "Name (A -> Z)"),
	TotalSunkDesc UMETA(DisplayName = "Total Sunk (High -> Low)")
};

/**
 * Telemetry snapshot for an individual AWESOME Sink currently consuming a selected item.
 */
USTRUCT(BlueprintType)
struct SINKMONITOR_API FSinkActiveSourceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	float ItemsPerMin = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	float SecondsSinceLastSunk = 0.0f;
};

/**
 * Complete catalog + live rate record for a single sinkable item descriptor.
 */
USTRUCT(BlueprintType)
struct SINKMONITOR_API FSinkableItemEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	TSubclassOf<UFGItemDescriptor> ItemClass;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	int32 PointValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	EResourceSinkTrack SinkTrack = EResourceSinkTrack::RST_Default;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	float CurrentItemsPerMin = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	float CurrentPointsPerMin = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	int64 SessionTotalSunk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	int64 LifetimeTotalSunk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	int32 ActiveSinkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	bool bIsPinned = false;
};

/**
 * Compact network replication payload sent from server to connected clients.
 */
USTRUCT(BlueprintType)
struct SINKMONITOR_API FSinkReplicatedItemStat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	TSubclassOf<UFGItemDescriptor> ItemClass;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	float CurrentItemsPerMin = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	int64 SessionTotalSunk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	int64 LifetimeTotalSunk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	int32 ActiveSinkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	bool bIsPinned = false;
};

/**
 * Replicated active sink source entry linked to a specific item class.
 */
USTRUCT(BlueprintType)
struct SINKMONITOR_API FSinkReplicatedSourceEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	TSubclassOf<UFGItemDescriptor> ItemClass;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	FVector_NetQuantize WorldLocation = FVector_NetQuantize::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	float ItemsPerMin = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "SinkMonitor")
	float SecondsSinceLastSunk = 0.0f;
};

/**
 * SaveGame-persisted per-item lifetime total and pin flag using soft class references
 * so uninstalling third-party content mods never corrupts save files.
 */
USTRUCT()
struct SINKMONITOR_API FSinkSavedItemRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftClassPtr<UFGItemDescriptor> ItemClass;

	UPROPERTY(SaveGame)
	int64 LifetimeTotalSunk = 0;

	UPROPERTY(SaveGame)
	bool bIsPinned = false;
};
