// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainSeat.h"
#include "BHCharacter.h"
#include "BHPlayerState.h"
#include "BHTypes.h"
#include "Components/StaticMeshComponent.h"

ABHTrainSeat::ABHTrainSeat()
{
	// Reuse the base 'Mesh' cube as an INVISIBLE interaction box: hidden in game, blocks only the ECC_Visibility
	// view trace, ignores every other channel (so it never blocks walking nor ejects a sitter when they stand).
	// Default size suits a lobby wall bench; ConfigureSeat resizes it per seat. Sits axis-aligned over the bench.
	if (Mesh)
	{
		Mesh->SetHiddenInGame(true);
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 41.0f));
		Mesh->SetRelativeScale3D(FVector(1.55f, 0.62f, 0.92f));
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	InteractionLabel = FText::FromString(TEXT("Sit down"));
}

void ABHTrainSeat::ConfigureSeat(const FVector& InBoxScale, float InFacingYaw)
{
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(InBoxScale);
	}
	SeatFacingYaw = InFacingYaw;
}

bool ABHTrainSeat::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}
	// Can't sit if already seated (you stand via Jump, not by re-interacting).
	return !Character->IsSeated();
}

void ABHTrainSeat::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character || Character->IsSeated())
	{
		return;
	}
	// Teleport onto the seat and lock the player there facing the configured direction.
	const FVector SeatTarget = GetActorLocation() + FVector(0.0f, 0.0f, SeatCapsuleRiseZ);
	Character->SitOnSeatAuthority(SeatTarget, SeatFacingYaw);
}

FText ABHTrainSeat::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Sit down"));
}

FBHInteractionPromptInfo ABHTrainSeat::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	// Explain the seat in the prompt itself so a player knows how to get up before they sit.
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.Label = FText::FromString(TEXT("Sit down  (press Jump to stand)"));
	if (!Info.bCanInteract)
	{
		Info.DisabledReason = FText::FromString(
			(Character && Character->IsSeated()) ? TEXT("ALREADY SEATED") : TEXT("UNAVAILABLE"));
	}
	return Info;
}
