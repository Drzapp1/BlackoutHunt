// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHPowerSwitch.h"
#include "BHPropVisuals.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "BHCharacter.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"

ABHPowerSwitch::ABHPowerSwitch()
{
	CircuitId = 0;
	SwitchLabel = FText::FromString(TEXT("Toggle Lights"));
	InteractionLabel = SwitchLabel;
	SetActorScale3D(FVector::OneVector);

	SwitchPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchPlate"));
	SwitchPlate->SetupAttachment(RootComponent);
	SwitchLever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchLever"));
	SwitchLever->SetupAttachment(RootComponent);
	CircuitLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CircuitLight"));
	CircuitLight->SetupAttachment(RootComponent);
	WarningLabel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningLabel"));
	WarningLabel->SetupAttachment(RootComponent);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.48f, 0.13f, 0.70f), true);
	BHPropVisuals::ConfigurePart(SwitchPlate, BHPropVisuals::CubeMesh(), BHPropVisuals::DiamondPlateMaterial(), FVector(26.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.025f, 0.105f, 0.52f));
	BHPropVisuals::ConfigurePart(SwitchLever, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(30.0f, 0.0f, -5.0f), FRotator(0.0f, 0.0f, -26.0f), FVector(0.035f, 0.040f, 0.35f));
	BHPropVisuals::ConfigurePart(CircuitLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(31.0f, -9.0f, 24.0f), FRotator::ZeroRotator, FVector(0.050f));
	BHPropVisuals::ConfigurePart(WarningLabel, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(31.0f, 8.0f, -29.0f), FRotator::ZeroRotator, FVector(0.018f, 0.070f, 0.045f));

	BHPropVisuals::TintPart(SwitchLever, FLinearColor(0.08f, 0.085f, 0.09f, 1.0f));
	BHPropVisuals::TintPart(CircuitLight, FLinearColor(0.18f, 0.86f, 0.62f, 1.0f), 1.8f);
	BHPropVisuals::TintPart(WarningLabel, FLinearColor(0.88f, 0.62f, 0.10f, 1.0f));
}

void ABHPowerSwitch::Configure(int32 NewCircuitId, const FText& NewLabel)
{
	CircuitId = NewCircuitId;
	SwitchLabel = NewLabel;
	InteractionLabel = SwitchLabel;
}

bool ABHPowerSwitch::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}
	// Power switches are an in-round environmental control; disallow toggling in the lobby or on the
	// results screen so a griefing student can't strobe lights while the class is gathering/reviewing.
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHGS
		&& BHGS->RoundPhase != EBHRoundPhase::Lobby
		&& BHGS->RoundPhase != EBHRoundPhase::SurvivorsWin
		&& BHGS->RoundPhase != EBHRoundPhase::HunterWin;
}

void ABHPowerSwitch::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !CanInteract_Implementation(Character))
	{
		return;
	}

	// Anti-grief throttle: at most one circuit toggle per second per switch, so a student cannot rapidly
	// strobe a whole area's lights on/off for all 32 players. ToggleLightCircuit itself has no rate limit.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastToggleServerTime < 1.0f)
	{
		return;
	}
	LastToggleServerTime = Now;

	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->ToggleLightCircuit(CircuitId);
	}
}

FText ABHPowerSwitch::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return SwitchLabel;
}
