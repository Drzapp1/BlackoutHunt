// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainDanceFloor.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr int32 BHFloorCols = 8;
	constexpr int32 BHFloorRows = 6;
	constexpr int32 BHFloorTiles = BHFloorCols * BHFloorRows;
	constexpr float BHFloorTile = 44.0f;

	FLinearColor HSV(float HueDeg, float Sat, float Val)
	{
		return FLinearColor(FMath::Fmod(HueDeg < 0.0f ? HueDeg + 360.0f : HueDeg, 360.0f), Sat, Val).HSVToLinearRGB();
	}
}

ABHTrainDanceFloor::ABHTrainDanceFloor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.06f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Dance Floor"));

	Tiles.Reserve(BHFloorTiles);
	for (int32 Index = 0; Index < BHFloorTiles; ++Index)
	{
		UStaticMeshComponent* Tile = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Tile_%d"), Index));
		Tile->SetupAttachment(SceneRoot);
		Tiles.Add(Tile);
	}

	FloorGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("FloorGlow"));
	FloorGlow->SetupAttachment(SceneRoot);
	FloorGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 140.0f));
	FloorGlow->SetIntensity(60.0f);
	FloorGlow->SetAttenuationRadius(520.0f);
	FloorGlow->SetCastShadows(false);

	BuildTiles();
}

void ABHTrainDanceFloor::BuildTiles()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		const int32 Col = Index % BHFloorCols;
		const int32 Row = Index / BHFloorCols;
		const FVector Loc((Col - (BHFloorCols - 1) * 0.5f) * BHFloorTile, (Row - (BHFloorRows - 1) * 0.5f) * BHFloorTile, 3.0f);
		BHPropVisuals::ConfigurePart(Tiles[Index], BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), Loc, FRotator::ZeroRotator, FVector(BHFloorTile / 100.0f * 0.94f, BHFloorTile / 100.0f * 0.94f, 0.04f), false);
	}
}

void ABHTrainDanceFloor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainDanceFloor, Pattern);
}

void ABHTrainDanceFloor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		if (!Tiles[Index])
		{
			continue;
		}
		const int32 Col = Index % BHFloorCols;
		const int32 Row = Index / BHFloorCols;
		FLinearColor Color;
		switch (Pattern)
		{
		case 1:   // checker flash
		{
			const int32 Phase = static_cast<int32>(T * 4.0f);
			const bool bOn = (((Col + Row + Phase) & 1) == 0);
			Color = bOn ? HSV(T * 70.0f, 0.9f, 1.0f) : HSV(T * 70.0f + 180.0f, 0.9f, 0.25f);
			break;
		}
		case 2:   // sparkle
		{
			const float Seed = Col * 7.0f + Row * 13.0f;
			const float Flick = 0.5f + 0.5f * FMath::Sin(T * 6.0f + Seed);
			Color = HSV(Seed * 11.0f + T * 30.0f, 0.85f, 0.25f + 0.75f * Flick);
			break;
		}
		default:  // rainbow sweep
			Color = HSV((Col + Row) * 25.0f + T * 130.0f, 0.95f, 1.0f);
			break;
		}
		BHPropVisuals::TintPart(Tiles[Index], Color, 1.0f);
	}

	if (FloorGlow)
	{
		FloorGlow->SetLightColor(HSV(T * 130.0f, 0.7f, 1.0f));
	}
}

bool ABHTrainDanceFloor::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainDanceFloor::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	Pattern = (Pattern + 1) % 3;
	ForceNetUpdate();
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		static const TCHAR* Names[3] = { TEXT("Rainbow sweep"), TEXT("Checker flash"), TEXT("Sparkle") };
		PC->ClientShowStatusMessage(FString::Printf(TEXT("Dance floor: %s"), Names[Pattern % 3]), 2.0f);
	}
}

FText ABHTrainDanceFloor::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Change the lights"));
}
