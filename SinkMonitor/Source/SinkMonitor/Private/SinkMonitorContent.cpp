#include "SinkMonitorContent.h"

#include "FGCategory.h"
#include "ItemAmount.h"
#include "Resources/FGItemDescriptor.h"
#include "SinkMonitorBuildable.h"
#include "SinkMonitorModule.h"
#include "SinkMonitorRemoteCallObject.h"
#include "SinkMonitorSubsystem.h"

#if !UE_SERVER
#include "Engine/Texture2D.h"
#endif

namespace SinkMonitorContentAssets
{
	constexpr const TCHAR* Icon256Path = TEXT("/SinkMonitor/UI/Icons/T_Icon_SinkMonitor_256.T_Icon_SinkMonitor_256");
	constexpr const TCHAR* Icon512Path = TEXT("/SinkMonitor/UI/Icons/T_Icon_SinkMonitor_512.T_Icon_SinkMonitor_512");

	constexpr const TCHAR* BuildCategoryPath = TEXT("/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/BC_TradingPost.BC_TradingPost_C");
	constexpr const TCHAR* BuildSubCategoryPath = TEXT("/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/Sub_Special/SC_AWESOME.SC_AWESOME_C");
	constexpr const TCHAR* BuildGunProducerPath = TEXT("/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C");

	struct FRecipeIngredientSpec
	{
		const TCHAR* AssetPath;
		int32 Amount;
	};

	constexpr FRecipeIngredientSpec Ingredients[] = {
		{TEXT("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C"), 4},
		{TEXT("/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C"), 8},
		{TEXT("/Game/FactoryGame/Resource/Parts/CopperSheet/Desc_CopperSheet.Desc_CopperSheet_C"), 4},
		{TEXT("/Game/FactoryGame/Resource/Parts/QuartzCrystal/Desc_QuartzCrystal.Desc_QuartzCrystal_C"), 2},
	};

#if !UE_SERVER
	UTexture2D* LoadIconTexture(const TCHAR* AssetPath)
	{
		if (IsRunningDedicatedServer())
		{
			return nullptr;
		}
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath);
		if (!Texture)
		{
			UE_LOG(LogSinkMonitor, Warning, TEXT("SinkMonitor content failed to load icon texture: %s"), AssetPath);
		}
		return Texture;
	}
#endif
}

USinkMonitorBuildingDescriptor::USinkMonitorBuildingDescriptor()
{
	mBuildableClass = ASinkMonitorBuildable::StaticClass();
	mUsesDistanceForZooping = false;
	mUseDisplayNameAndDescription = true;
	mDisplayName = NSLOCTEXT("SinkMonitor", "DescriptorName", "AWESOME Sink Monitor Terminal");
	mDescription = NSLOCTEXT(
		"SinkMonitor",
		"DescriptorDesc",
		"Connects to the AWESOME Sink telemetry network to monitor live Items/min, Points/min, session/lifetime totals, and active sinks across all vanilla and modded sinkable items.");
	mForm = EResourceForm::RF_SOLID;
	mStackSize = EStackSize::SS_ONE;
	mCanBeDiscarded = true;
	mRememberPickUp = false;
	mMenuPriority = 25.0f;
}

void USinkMonitorBuildingDescriptor::EnsureInitialized()
{
	mBuildableClass = ASinkMonitorBuildable::StaticClass();

	if (UClass* CategoryClass = LoadClass<UFGCategory>(nullptr, SinkMonitorContentAssets::BuildCategoryPath))
	{
		mCategory = CategoryClass;
	}
	else
	{
		UE_LOG(LogSinkMonitor, Warning, TEXT("SinkMonitor failed to load build category: %s"), SinkMonitorContentAssets::BuildCategoryPath);
	}

	mSubCategories.Reset();
	if (UClass* SubCategoryClass = LoadClass<UFGCategory>(nullptr, SinkMonitorContentAssets::BuildSubCategoryPath))
	{
		mSubCategories.Add(SubCategoryClass);
	}
	else
	{
		UE_LOG(LogSinkMonitor, Warning, TEXT("SinkMonitor failed to load build subcategory: %s"), SinkMonitorContentAssets::BuildSubCategoryPath);
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (UTexture2D* SmallTex = SinkMonitorContentAssets::LoadIconTexture(SinkMonitorContentAssets::Icon256Path))
		{
			mSmallIcon = SmallTex;
		}
		if (UTexture2D* BigTex = SinkMonitorContentAssets::LoadIconTexture(SinkMonitorContentAssets::Icon512Path))
		{
			mPersistentBigIcon = BigTex;
		}
	}
#endif
}

USinkMonitorRecipe::USinkMonitorRecipe()
{
	mDisplayNameOverride = false;
	mDisplayName = NSLOCTEXT("SinkMonitor", "RecipeName", "AWESOME Sink Monitor Terminal");
	mManufactoringDuration = 1.0f;
	mManualManufacturingMultiplier = 1.0f;
	mManufacturingMenuPriority = 25.0f;

	mProduct.Add(FItemAmount(USinkMonitorBuildingDescriptor::StaticClass(), 1));
	mProducedIn.Add(TSoftClassPtr<UObject>(FSoftObjectPath(SinkMonitorContentAssets::BuildGunProducerPath)));
}

void USinkMonitorRecipe::EnsureInitialized()
{
	mProduct.Reset();
	mProduct.Add(FItemAmount(USinkMonitorBuildingDescriptor::StaticClass(), 1));

	mProducedIn.Reset();
	mProducedIn.Add(TSoftClassPtr<UObject>(FSoftObjectPath(SinkMonitorContentAssets::BuildGunProducerPath)));

	mIngredients.Reset();
	for (const SinkMonitorContentAssets::FRecipeIngredientSpec& Spec : SinkMonitorContentAssets::Ingredients)
	{
		UClass* ItemClass = LoadClass<UFGItemDescriptor>(nullptr, Spec.AssetPath);
		if (ItemClass)
		{
			mIngredients.Add(FItemAmount(ItemClass, Spec.Amount));
		}
		else
		{
			UE_LOG(LogSinkMonitor, Error, TEXT("SinkMonitor recipe failed to load ingredient: %s"), Spec.AssetPath);
		}
	}
}

USinkMonitorUnlockRecipe::USinkMonitorUnlockRecipe()
{
	mRecipes.AddUnique(USinkMonitorRecipe::StaticClass());
}

void USinkMonitorUnlockRecipe::EnsureInitialized()
{
	mRecipes.Reset();
	mRecipes.Add(USinkMonitorRecipe::StaticClass());
}

USinkMonitorSchematic::USinkMonitorSchematic()
{
	mType = ESchematicType::EST_Custom;
	mDisplayName = NSLOCTEXT("SinkMonitor", "SchematicName", "AWESOME Sink Monitor");
	mDescription = NSLOCTEXT(
		"SinkMonitor",
		"SchematicDesc",
		"Unlocks the AWESOME Sink Monitor Terminal buildable for real-time tracking of sinked items, point rates, and active AWESOME Sinks.");
	mTechTier = 2;
	mMenuPriority = 25.0f;
	mTimeToComplete = 0.0f;
	mIsPlayerSpecific = false;
	mDependenciesBlocksSchematicAccess = false;
	mHiddenUntilDependenciesMet = false;

#if WITH_EDITORONLY_DATA
	mIncludeInBuilds = EIncludeInBuilds::IIB_PublicBuilds;
#endif

	UnlockRecipe = CreateDefaultSubobject<USinkMonitorUnlockRecipe>(TEXT("UnlockRecipe"));
	if (UnlockRecipe)
	{
		mUnlocks.Add(UnlockRecipe);
	}
}

void USinkMonitorSchematic::EnsureInitialized()
{
	if (!UnlockRecipe)
	{
		UnlockRecipe = NewObject<USinkMonitorUnlockRecipe>(this, TEXT("UnlockRecipe"));
	}
	if (UnlockRecipe)
	{
		UnlockRecipe->EnsureInitialized();
	}

	mUnlocks.Reset();
	if (UnlockRecipe)
	{
		mUnlocks.Add(UnlockRecipe);
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (UTexture2D* SmallTex = SinkMonitorContentAssets::LoadIconTexture(SinkMonitorContentAssets::Icon256Path))
		{
			mSmallSchematicIcon = SmallTex;
		}
		if (UTexture2D* BigTex = SinkMonitorContentAssets::LoadIconTexture(SinkMonitorContentAssets::Icon512Path))
		{
			mSchematicIcon.SetResourceObject(BigTex);
			mSchematicIcon.SetImageSize(FVector2f(512.0f, 512.0f));
			mSchematicIcon.DrawAs = ESlateBrushDrawType::Image;
		}
	}
#endif
}

USinkMonitorGameWorldModule::USinkMonitorGameWorldModule()
{
	bRootModule = true;
	ModSubsystems.AddUnique(ASinkMonitorSubsystem::StaticClass());
	mSchematics.AddUnique(USinkMonitorSchematic::StaticClass());
}

void USinkMonitorGameWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	if (Phase == ELifecyclePhase::CONSTRUCTION || Phase == ELifecyclePhase::INITIALIZATION)
	{
		GetMutableDefault<ASinkMonitorBuildable>()->EnsureMeshLoaded();
		GetMutableDefault<USinkMonitorBuildingDescriptor>()->EnsureInitialized();
		GetMutableDefault<USinkMonitorRecipe>()->EnsureInitialized();
		GetMutableDefault<USinkMonitorSchematic>()->EnsureInitialized();
	}

	Super::DispatchLifecycleEvent(Phase);
}

USinkMonitorGameInstanceModule::USinkMonitorGameInstanceModule()
{
	bRootModule = true;
	RemoteCallObjects.AddUnique(USinkMonitorRemoteCallObject::StaticClass());
}
