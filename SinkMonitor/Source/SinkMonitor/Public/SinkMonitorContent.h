#pragma once

#include "CoreMinimal.h"
#include "FGRecipe.h"
#include "FGSchematic.h"
#include "Module/GameInstanceModule.h"
#include "Module/GameWorldModule.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "SinkMonitorContent.generated.h"

/**
 * Building descriptor for ASinkMonitorBuildable under Special -> AWESOME.
 */
UCLASS()
class SINKMONITOR_API USinkMonitorBuildingDescriptor : public UFGBuildingDescriptor
{
	GENERATED_BODY()

public:
	USinkMonitorBuildingDescriptor();

	/** Resolves Build Menu category, subcategory, and UI icons onto the CDO idempotently. */
	void EnsureInitialized();
};

/**
 * Build Gun recipe for crafting the AWESOME Sink Monitor Terminal.
 */
UCLASS()
class SINKMONITOR_API USinkMonitorRecipe : public UFGRecipe
{
	GENERATED_BODY()

public:
	USinkMonitorRecipe();

	/** Resolves ingredient item classes onto the CDO idempotently. */
	void EnsureInitialized();
};

/**
 * Concrete UFGUnlockRecipe subclass (base UFGUnlockRecipe is Abstract) unlocking USinkMonitorRecipe.
 */
UCLASS()
class SINKMONITOR_API USinkMonitorUnlockRecipe : public UFGUnlockRecipe
{
	GENERATED_BODY()

public:
	USinkMonitorUnlockRecipe();

	/** Ensures USinkMonitorRecipe is listed in mRecipes. */
	void EnsureInitialized();
};

/**
 * Custom schematic registered by USinkMonitorGameWorldModule and unlocked with Tier 2 AWESOME Sink.
 */
UCLASS()
class SINKMONITOR_API USinkMonitorSchematic : public UFGSchematic
{
	GENERATED_BODY()

public:
	USinkMonitorSchematic();

	/** Resolves schematic unlock subobject and UI icons onto the CDO idempotently. */
	void EnsureInitialized();

protected:
	UPROPERTY()
	TObjectPtr<USinkMonitorUnlockRecipe> UnlockRecipe;
};

/**
 * Root GameWorld module registering ASinkMonitorSubsystem and USinkMonitorSchematic.
 */
UCLASS()
class SINKMONITOR_API USinkMonitorGameWorldModule : public UGameWorldModule
{
	GENERATED_BODY()

public:
	USinkMonitorGameWorldModule();

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};

/**
 * Root GameInstance module registering USinkMonitorRemoteCallObject for multiplayer RPCs.
 */
UCLASS()
class SINKMONITOR_API USinkMonitorGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	USinkMonitorGameInstanceModule();
};
