// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainRoofParkourGate.h"

#include "BHCharacter.h"
#include "BHPropVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ABHTrainRoofParkourGate::ABHTrainRoofParkourGate()
{
	InteractionLabel = FText::FromString(TEXT("Parkour Finish"));

	// A bright little finish "flag" post.
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.14f, 0.14f, 1.4f));
	}
}

void ABHTrainRoofParkourGate::BeginPlay()
{
	Super::BeginPlay();

	// Glowing green finish marker (emissive so it stands out at the top of the climb). Runtime-only material work.
	if (Mesh && GetNetMode() != NM_DedicatedServer)
	{
		if (UMaterialInstanceDynamic* FlagMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			const FLinearColor FinishColor(0.20f, 1.0f, 0.45f, 1.0f);
			FlagMaterial->SetVectorParameterValue(TEXT("Color"), FinishColor);
			FlagMaterial->SetVectorParameterValue(TEXT("BaseColor"), FinishColor);
			FlagMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), FinishColor * 3.5f);
		}
	}
}

bool ABHTrainRoofParkourGate::CanInteract_Implementation(ABHCharacter* Character) const
{
	return IsValid(Character);
}

void ABHTrainRoofParkourGate::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character))
	{
		return;
	}

	// Cosmetic only: ask the owning client to unlock the achievement (which also awards its XP). Account progress
	// is client-local, so the server can't write it directly -- it asks the owning client to, here.
	Character->ClientGrantAchievement(FName(TEXT("roof_parkour")), TEXT("Roof parkour cleared! Nice moves."));
}

FText ABHTrainRoofParkourGate::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return InteractionLabel;
}
