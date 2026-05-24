#include "BHSlidingGate.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ABHSlidingGate::ABHSlidingGate()
{
	bOpen = false;
	OpenLiftHeight = 265.0f;
	ClosedMeshLocation = FVector::ZeroVector;
	InteractionLabel = FText::FromString(TEXT("Gate"));
	SetActorScale3D(FVector::OneVector);
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.16f, 1.55f, 2.55f));
	}
}

void ABHSlidingGate::BeginPlay()
{
	Super::BeginPlay();
	if (Mesh)
	{
		ClosedMeshLocation = Mesh->GetRelativeLocation();
	}
	ApplyGateState();
}

void ABHSlidingGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHSlidingGate, bOpen);
}

void ABHSlidingGate::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority())
	{
		return;
	}

	SetOpen(!bOpen);
}

FText ABHSlidingGate::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return bOpen ? FText::FromString(TEXT("Lower Gate")) : FText::FromString(TEXT("Raise Gate"));
}

void ABHSlidingGate::SetOpen(bool bNewOpen)
{
	bOpen = bNewOpen;
	ApplyGateState();
}

void ABHSlidingGate::OnRep_Open()
{
	ApplyGateState();
}

void ABHSlidingGate::ApplyGateState()
{
	if (Mesh)
	{
		Mesh->SetRelativeLocation(ClosedMeshLocation + FVector(0.0f, 0.0f, bOpen ? OpenLiftHeight : 0.0f));
	}
}
