// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHSlidingGate.h"
#include "BHCharacter.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Engine/World.h"
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
	if (!HasAuthority() || !CanInteract_Implementation(Character))
	{
		return;
	}

	// Anti-grief throttle: at most one toggle per second per gate, matching the power/security controls,
	// so a student can't rapidly flap a gate to block a doorway or spam movement noise for everyone.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastToggleServerTime < 1.0f)
	{
		return;
	}
	LastToggleServerTime = Now;

	SetOpen(!bOpen);
}

bool ABHSlidingGate::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}
	// Gates are an in-round traversal control; disallow toggling in the lobby or on the results screen
	// so a griefing student can't block the gathering area or flap gates while the class is reviewing.
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHGS
		&& BHGS->RoundPhase != EBHRoundPhase::Lobby
		&& BHGS->RoundPhase != EBHRoundPhase::SurvivorsWin
		&& BHGS->RoundPhase != EBHRoundPhase::HunterWin;
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
