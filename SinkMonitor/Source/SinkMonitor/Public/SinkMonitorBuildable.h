#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "SinkMonitorBuildable.generated.h"

class UFGMaterialEffect_Build;
class UStaticMeshComponent;

/**
 * Placeable AWESOME Sink Monitor Terminal buildable.
 * Opens USinkMonitorInteractWidget when interacted with by a pioneer.
 */
UCLASS()
class SINKMONITOR_API ASinkMonitorBuildable : public AFGBuildable
{
	GENERATED_BODY()

public:
	ASinkMonitorBuildable(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	virtual TSoftClassPtr<UFGMaterialEffect_Build> GetBuildEffectTemplate_Implementation() const override;
	virtual TSoftClassPtr<UFGMaterialEffect_Build> GetDismantleEffectTemplate_Implementation() const override;

	/** Ensures MainMesh has SM_SinkMonitorTerminal loaded and assigned (safe to call on CDO and instances). */
	void EnsureMeshLoaded();

	static inline const TCHAR* MeshAssetPath = TEXT("/SinkMonitor/Buildable/Mesh/SM_SinkMonitorTerminal.SM_SinkMonitorTerminal");
	static inline const TCHAR* BuildEffectAssetPath = TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BP_MaterialEffect_Build.BP_MaterialEffect_Build_C");
	static inline const TCHAR* DismantleEffectAssetPath = TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BP_MaterialEffect_Dismantle.BP_MaterialEffect_Dismantle_C");

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SinkMonitor")
	TObjectPtr<UStaticMeshComponent> MainMesh;
};
