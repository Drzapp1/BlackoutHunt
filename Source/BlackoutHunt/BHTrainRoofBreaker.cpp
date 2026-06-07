// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainRoofBreaker.h"

#include "BHCharacter.h"
#include "BHPropVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

ABHTrainRoofBreaker::ABHTrainRoofBreaker()
{
	InteractionLabel = FText::FromString(TEXT("Roof Light Breaker"));

	// A small, dark junction-box panel: easy to miss until you go looking (or ping it with the N node marker).
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.22f, 0.30f, 0.34f));
	}
}

void ABHTrainRoofBreaker::BeginPlay()
{
	Super::BeginPlay();

	// Runtime material (not constructor-safe), applied here so the box reads as a grimy electrical panel on
	// every machine. BeginPlay runs on the server and on each client for this replicated actor.
	if (Mesh)
	{
		if (UMaterialInterface* PanelMaterial = BHPropVisuals::RustedMetalMaterial())
		{
			Mesh->SetMaterial(0, PanelMaterial);
		}
	}
}

bool ABHTrainRoofBreaker::CanInteract_Implementation(ABHCharacter* Character) const
{
	return IsValid(Character);
}

void ABHTrainRoofBreaker::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character))
	{
		return;
	}

	// Per-player lighting: ask the interacting player's OWN client to toggle ITS roof service lights. Nothing is
	// shared, so each passenger lights (or darkens) the roof for themselves.
	Character->ClientToggleRoofServiceLights();
}

FText ABHTrainRoofBreaker::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return InteractionLabel;
}
