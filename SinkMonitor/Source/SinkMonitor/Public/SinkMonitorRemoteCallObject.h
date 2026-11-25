#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "Resources/FGItemDescriptor.h"
#include "SinkMonitorRemoteCallObject.generated.h"

/**
 * Per-player Remote Call Object forwarding client terminal interactions
 * (pinning items, requesting immediate snapshots, resetting session counters) to the server.
 */
UCLASS()
class SINKMONITOR_API USinkMonitorRemoteCallObject : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_TogglePinItem(TSubclassOf<UFGItemDescriptor> ItemClass);

	UFUNCTION(Server, Reliable)
	void Server_RequestStatsSync();

	UFUNCTION(Server, Reliable)
	void Server_ResetSessionStats();

	UPROPERTY(Replicated, Meta = (NoAutoJson))
	bool mForceNetField_USinkMonitorRemoteCallObject = false;
};
