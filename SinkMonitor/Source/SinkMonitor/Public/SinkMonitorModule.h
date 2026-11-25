#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

SINKMONITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogSinkMonitor, Log, All);

/**
 * Runtime module interface for AWESOME Sink Monitor.
 * Installs worker-thread-safe SML native hooks on AFGBuildableResourceSink and AFGResourceSinkSubsystem.
 */
class FSinkMonitorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FDelegateHandle CollectInputHookHandle;
	FDelegateHandle AddPointsAfterHookHandle;
};
