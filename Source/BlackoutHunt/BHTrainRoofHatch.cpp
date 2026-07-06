// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainRoofHatch.h"

#include "BHCharacter.h"
#include "BHPropVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

ABHTrainRoofHatch::ABHTrainRoofHatch()
{
	InteractionLabel = FText::FromString(TEXT("Maintenance Hatch"));

	// A small, dark wall panel flush against the carriage wall -- visible if you go looking, easy to miss.
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.5f, 0.12f, 0.7f));
	}
}

void ABHTrainRoofHatch::BeginPlay()
{
	Super::BeginPlay();

	// The material is a runtime LoadObject (not constructor-safe), so apply it here. BeginPlay runs on the
	// server and on every client for this replicated actor, so the panel looks right everywhere.
	if (Mesh)
	{
		if (UMaterialInterface* PanelMaterial = BHPropVisuals::RustedMetalMaterial())
		{
			Mesh->SetMaterial(0, PanelMaterial);
		}
	}
}

bool ABHTrainRoofHatch::CanInteract_Implementation(ABHCharacter* Character) const
{
	return IsValid(Character);
}

void ABHTrainRoofHatch::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character))
	{
		return;
	}

	// Lift the passenger onto the train roof. Cosmetic only -- the intermission runs on its own timer, so being
	// on the roof never blocks boarding or affects scoring. bSweep=false so geometry can't snag the teleport.
	Character->SetActorLocation(RoofTeleportLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Ask the owning client to record the cosmetic achievement + toast (account progress is client-local, so the
	// server cannot write it directly).
	Character->ClientGrantAchievement(FName(TEXT("roof_rider")), TEXT("You climbed out onto the roof. Mind the tunnel."));
}

void ABHTrainRoofHatch::ConfigureHatch(const FVector& InRoofTeleportLocation)
{
	RoofTeleportLocation = InRoofTeleportLocation;
}
