#include "SinkMonitorModule.h"

#include "Buildables/FGBuildableResourceSink.h"
#include "Engine/World.h"
#include "FGResourceSinkSubsystem.h"
#include "Misc/CoreMiscDefines.h"
#include "Patching/NativeHookManager.h"
#include "Resources/FGItemDescriptor.h"
#include "SinkMonitorSubsystem.h"
#include "UObject/GCObject.h"

DEFINE_LOG_CATEGORY(LogSinkMonitor);

namespace
{
class FSinkMonitorAssetRooter final : public FGCObject
{
public:
	void Add(UObject* Object)
	{
		if (IsValid(Object))
		{
			KeepAliveObjects.AddUnique(Object);
		}
	}

	void Reset()
	{
		KeepAliveObjects.Reset();
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObjects(KeepAliveObjects);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("FSinkMonitorAssetRooter");
	}

private:
	TArray<TObjectPtr<UObject>> KeepAliveObjects;
};

TUniquePtr<FSinkMonitorAssetRooter> GSinkMonitorAssetRooter;

struct FThreadActiveSinkContext
{
	uint64 SinkUniqueId = 0;
	FVector SinkWorldLocation = FVector::ZeroVector;
};

thread_local FThreadActiveSinkContext GThreadActiveSinkContext;

struct FScopedActiveSinkGuard
{
	FThreadActiveSinkContext Previous;

	explicit FScopedActiveSinkGuard(const AFGBuildableResourceSink* SinkActor)
		: Previous(GThreadActiveSinkContext)
	{
		if (SinkActor)
		{
			GThreadActiveSinkContext.SinkUniqueId = static_cast<uint64>(SinkActor->GetUniqueID());
			GThreadActiveSinkContext.SinkWorldLocation = SinkActor->GetActorLocation();
		}
		else
		{
			GThreadActiveSinkContext = FThreadActiveSinkContext{};
		}
	}

	~FScopedActiveSinkGuard()
	{
		GThreadActiveSinkContext = Previous;
	}
};

void HandleResourceSinkCollectInput(
	TCallScope<void(*)(AFGBuildableResourceSink*)>& Scope,
	AFGBuildableResourceSink* SinkActor)
{
	const FScopedActiveSinkGuard Guard(SinkActor);
	Scope(SinkActor);
}

void HandleResourceSinkAddPointsAfter(
	const bool& bWasSunk,
	AFGResourceSinkSubsystem* SinkSubsystem,
	TSubclassOf<UFGItemDescriptor> ItemClass)
{
	if (!bWasSunk || !SinkSubsystem || !ItemClass || !SinkSubsystem->HasAuthority())
	{
		return;
	}

	if (ASinkMonitorSubsystem* MonitorSub = ASinkMonitorSubsystem::GetAuthorityInstanceThreadSafe(SinkSubsystem->GetWorld()))
	{
		MonitorSub->EnqueueSunkItemFromFactoryThread(
			ItemClass,
			GThreadActiveSinkContext.SinkUniqueId,
			GThreadActiveSinkContext.SinkWorldLocation);
	}
}
} // namespace

void FSinkMonitorModule::StartupModule()
{
	constexpr bool bIsEditorBuild = (WITH_EDITOR != 0);
	if (!bIsEditorBuild && !GIsEditor && !IsRunningCommandlet())
	{
		CollectInputHookHandle = SUBSCRIBE_UOBJECT_METHOD(
			AFGBuildableResourceSink,
			Factory_CollectInput_Implementation,
			&HandleResourceSinkCollectInput);

		AddPointsAfterHookHandle = SUBSCRIBE_UOBJECT_METHOD_AFTER(
			AFGResourceSinkSubsystem,
			AddPoints_ThreadSafe,
			&HandleResourceSinkAddPointsAfter);

		UE_LOG(LogSinkMonitor, Log, TEXT("SinkMonitor native AWESOME Sink telemetry hooks installed."));
	}
	else
	{
		UE_LOG(LogSinkMonitor, Log, TEXT("SinkMonitor module initialized (Editor/Commandlet; runtime hooks skipped)."));
	}
}

void FSinkMonitorModule::KeepAssetAlive(UObject* Object)
{
	if (!IsValid(Object) || IsRunningCommandlet() || IsEngineExitRequested() || !IsInGameThread())
	{
		return;
	}
	if (!GSinkMonitorAssetRooter)
	{
		GSinkMonitorAssetRooter = MakeUnique<FSinkMonitorAssetRooter>();
	}
	GSinkMonitorAssetRooter->Add(Object);
}

void FSinkMonitorModule::ShutdownModule()
{
	if (GSinkMonitorAssetRooter)
	{
		GSinkMonitorAssetRooter->Reset();
		GSinkMonitorAssetRooter.Reset();
	}

	constexpr bool bIsEditorBuild = (WITH_EDITOR != 0);
	if (!bIsEditorBuild)
	{
		if (CollectInputHookHandle.IsValid())
		{
			UNSUBSCRIBE_UOBJECT_METHOD(
				AFGBuildableResourceSink,
				Factory_CollectInput_Implementation,
				CollectInputHookHandle);
		}
		if (AddPointsAfterHookHandle.IsValid())
		{
			UNSUBSCRIBE_UOBJECT_METHOD(
				AFGResourceSinkSubsystem,
				AddPoints_ThreadSafe,
				AddPointsAfterHookHandle);
		}
	}
}

IMPLEMENT_MODULE(FSinkMonitorModule, SinkMonitor)
