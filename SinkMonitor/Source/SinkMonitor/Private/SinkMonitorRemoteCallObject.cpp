#include "SinkMonitorRemoteCallObject.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "SinkMonitorSubsystem.h"

void USinkMonitorRemoteCallObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USinkMonitorRemoteCallObject, mForceNetField_USinkMonitorRemoteCallObject);
}

void USinkMonitorRemoteCallObject::Server_TogglePinItem_Implementation(TSubclassOf<UFGItemDescriptor> ItemClass)
{
	if (ASinkMonitorSubsystem* Sub = ASinkMonitorSubsystem::Get(GetWorld()))
	{
		Sub->TogglePinItemAuthoritative(ItemClass);
	}
}

void USinkMonitorRemoteCallObject::Server_RequestStatsSync_Implementation()
{
	if (ASinkMonitorSubsystem* Sub = ASinkMonitorSubsystem::Get(GetWorld()))
	{
		Sub->ForceReplicationSync();
	}
}

void USinkMonitorRemoteCallObject::Server_ResetSessionStats_Implementation()
{
	if (ASinkMonitorSubsystem* Sub = ASinkMonitorSubsystem::Get(GetWorld()))
	{
		Sub->ResetSessionStatsAuthoritative();
	}
}
