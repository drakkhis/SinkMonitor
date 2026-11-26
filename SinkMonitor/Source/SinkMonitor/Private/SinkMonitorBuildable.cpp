#include "SinkMonitorBuildable.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "FGClearanceData.h"
#include "Hologram/FGBuildableHologram.h"
#include "Materials/MaterialInterface.h"
#include "SinkMonitorModule.h"
#include "UI/SinkMonitorInteractWidget.h"
#include "UObject/ConstructorHelpers.h"

ASinkMonitorBuildable::ASinkMonitorBuildable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	MainMesh = CreateDefaultSubobject<UStaticMeshComponent>(FName(*MAIN_MESH_NAME));
	if (RootComponent)
	{
		MainMesh->SetupAttachment(RootComponent);
		MainMesh->SetMobility(RootComponent->Mobility);
	}
	else
	{
		RootComponent = MainMesh;
	}
	MainMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> TerminalMeshFinder(MeshAssetPath);
	if (TerminalMeshFinder.Succeeded())
	{
		MainMesh->SetStaticMesh(TerminalMeshFinder.Get());
	}

	mDisplayName = NSLOCTEXT("SinkMonitor", "BuildableName", "AWESOME Sink Monitor Terminal");
	mDescription = NSLOCTEXT(
		"SinkMonitor",
		"BuildableDesc",
		"Connects to the AWESOME Sink telemetry network to monitor live Items/min, Points/min, session/lifetime totals, and active sinks across all vanilla and modded sinkable items.");

	mHologramClass = AFGBuildableHologram::StaticClass();
	mIsUseable = true;

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		mInteractWidgetSoftClass = TSoftClassPtr<UFGInteractWidget>(USinkMonitorInteractWidget::StaticClass());
	}
#endif

	mManagedByLightweightBuildableSubsystem = false;
	mCanContainLightweightInstances = false;
	mAllowColoring = false;
	mAllowPatterning = false;
	mShouldApplyCustomizationData = false;
	mToggleDormancyOnInteraction = false;

	FFGClearanceData Clearance;
	Clearance.Type = EClearanceType::CT_Default;
	Clearance.ClearanceBox = FBox(FVector(-65.0f, -90.0f, 0.0f), FVector(65.0f, 90.0f, 230.0f));
	Clearance.RelativeTransform = FTransform::Identity;
	mClearanceData.Add(Clearance);
}

void ASinkMonitorBuildable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureMeshLoaded();
}

void ASinkMonitorBuildable::BeginPlay()
{
	Super::BeginPlay();
	EnsureMeshLoaded();
}

void ASinkMonitorBuildable::EnsureMeshLoaded()
{
	if (!MainMesh)
	{
		return;
	}

	UStaticMesh* Mesh = MainMesh->GetStaticMesh();
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, MeshAssetPath);
		if (Mesh)
		{
			MainMesh->SetStaticMesh(Mesh);
		}
	}

	if (Mesh)
	{
		FSinkMonitorModule::KeepAssetAlive(Mesh);
		for (const FStaticMaterial& StaticMat : Mesh->GetStaticMaterials())
		{
			if (StaticMat.MaterialInterface)
			{
				FSinkMonitorModule::KeepAssetAlive(StaticMat.MaterialInterface);
			}
		}
	}
}

TSoftClassPtr<UFGMaterialEffect_Build> ASinkMonitorBuildable::GetBuildEffectTemplate_Implementation() const
{
	return TSoftClassPtr<UFGMaterialEffect_Build>(FSoftObjectPath(BuildEffectAssetPath));
}

TSoftClassPtr<UFGMaterialEffect_Build> ASinkMonitorBuildable::GetDismantleEffectTemplate_Implementation() const
{
	return TSoftClassPtr<UFGMaterialEffect_Build>(FSoftObjectPath(DismantleEffectAssetPath));
}
