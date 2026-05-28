#include "BHModuleReceiverRelay.h"

ABHModuleReceiverRelay::ABHModuleReceiverRelay()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	LastModuleId = NAME_None;
	bLastActiveState = false;
	ActivationCount = 0;
}

void ABHModuleReceiverRelay::OnModuleActivated_Implementation(ABHCharacter* Character, FName ModuleId, bool bActive)
{
	LastModuleId = ModuleId;
	bLastActiveState = bActive;
	++ActivationCount;
	OnRelayActivated.Broadcast(Character, ModuleId, bActive);
}

FName ABHModuleReceiverRelay::GetLastModuleId() const
{
	return LastModuleId;
}

bool ABHModuleReceiverRelay::GetLastActiveState() const
{
	return bLastActiveState;
}

int32 ABHModuleReceiverRelay::GetActivationCount() const
{
	return ActivationCount;
}

