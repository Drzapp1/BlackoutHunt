#include "BHSecurityShutter.h"
#include "Net/UnrealNetwork.h"

ABHSecurityShutter::ABHSecurityShutter()
{
	bOpen = false;
	CircuitId = 0;
	InteractionLabel = FText::FromString(TEXT("Security Shutter"));
	SetActorScale3D(FVector(0.18f, 2.4f, 2.6f));
}

void ABHSecurityShutter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHSecurityShutter, bOpen);
	DOREPLIFETIME(ABHSecurityShutter, CircuitId);
}

void ABHSecurityShutter::Configure(int32 NewCircuitId)
{
	CircuitId = NewCircuitId;
}

void ABHSecurityShutter::SetOpen(bool bNewOpen)
{
	bOpen = bNewOpen;
	ApplyOpenState();
}

int32 ABHSecurityShutter::GetCircuitId() const
{
	return CircuitId;
}

void ABHSecurityShutter::OnRep_Open()
{
	ApplyOpenState();
}

void ABHSecurityShutter::ApplyOpenState()
{
	SetActorHiddenInGame(bOpen);
	SetActorEnableCollision(!bOpen);
}
