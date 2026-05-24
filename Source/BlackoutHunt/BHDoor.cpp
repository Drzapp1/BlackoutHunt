#include "BHDoor.h"
#include "Net/UnrealNetwork.h"

ABHDoor::ABHDoor()
{
	bOpen = false;
	InteractionLabel = FText::FromString(TEXT("Door"));
	SetActorScale3D(FVector(0.15f, 1.35f, 2.6f));
}

void ABHDoor::BeginPlay()
{
	Super::BeginPlay();
	ClosedRotation = GetActorRotation();
	ApplyDoorState();
}

void ABHDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHDoor, bOpen);
}

void ABHDoor::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority())
	{
		return;
	}

	SetOpen(!bOpen);
}

FText ABHDoor::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return bOpen ? FText::FromString(TEXT("Close Door")) : FText::FromString(TEXT("Open Door"));
}

void ABHDoor::SetOpen(bool bNewOpen)
{
	bOpen = bNewOpen;
	ApplyDoorState();
}

bool ABHDoor::IsOpen() const
{
	return bOpen;
}

void ABHDoor::OnRep_Open()
{
	ApplyDoorState();
}

void ABHDoor::ApplyDoorState()
{
	SetActorRotation(bOpen ? ClosedRotation + FRotator(0.0f, 90.0f, 0.0f) : ClosedRotation);
}
