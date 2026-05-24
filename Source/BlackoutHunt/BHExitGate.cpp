#include "BHExitGate.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Net/UnrealNetwork.h"

ABHExitGate::ABHExitGate()
{
	InteractionLabel = FText::FromString(TEXT("Exit"));
	bDirectorActive = true;
	SetActorScale3D(FVector(2.0f, 0.2f, 2.5f));
}

void ABHExitGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHExitGate, bDirectorActive);
}

bool ABHExitGate::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return bDirectorActive && BHPS && BHGS && BHPS->IsAliveSurvivor() && BHGS->bExitUnlocked;
}

void ABHExitGate::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority() && CanInteract_Implementation(Character))
	{
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->NotifySurvivorEscaped(Character);
		}
	}
}

FText ABHExitGate::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	if (!bDirectorActive)
	{
		return FText::FromString(TEXT("Inactive Exit"));
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return (BHGS && BHGS->bExitUnlocked) ? FText::FromString(TEXT("Escape")) : FText::FromString(TEXT("Exit Locked"));
}

void ABHExitGate::SetDirectorActive(bool bNewActive)
{
	bDirectorActive = bNewActive;
}

bool ABHExitGate::IsDirectorActive() const
{
	return bDirectorActive;
}
