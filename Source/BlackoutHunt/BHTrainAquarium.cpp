// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainAquarium.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr int32 BHAquariumFish = 6;
	const FLinearColor FishTints[BHAquariumFish] = {
		FLinearColor(0.95f, 0.5f, 0.15f, 1.0f), FLinearColor(0.95f, 0.85f, 0.25f, 1.0f),
		FLinearColor(0.3f, 0.7f, 0.95f, 1.0f), FLinearColor(0.85f, 0.35f, 0.5f, 1.0f),
		FLinearColor(0.55f, 0.9f, 0.6f, 1.0f), FLinearColor(0.9f, 0.6f, 0.85f, 1.0f),
	};
}

ABHTrainAquarium::ABHTrainAquarium()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;
	bReplicates = true;   // exists on all clients; the fish motion itself is local/time-driven
	InteractionLabel = FText::FromString(TEXT("Aquarium"));

	Stand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stand"));
	Stand->SetupAttachment(SceneRoot);
	Water = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Water"));
	Water->SetupAttachment(SceneRoot);
	Glass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackPanel"));
	Glass->SetupAttachment(SceneRoot);

	Fish.Reserve(BHAquariumFish);
	for (int32 Index = 0; Index < BHAquariumFish; ++Index)
	{
		UStaticMeshComponent* F = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Fish_%d"), Index));
		F->SetupAttachment(SceneRoot);
		Fish.Add(F);
	}

	TankLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TankLight"));
	TankLight->SetupAttachment(SceneRoot);
	TankLight->SetRelativeLocation(FVector(0.0f, -30.0f, 120.0f));
	TankLight->SetIntensity(70.0f);
	TankLight->SetAttenuationRadius(320.0f);
	TankLight->SetLightColor(FLinearColor(0.35f, 0.7f, 1.0f, 1.0f));
	TankLight->SetCastShadows(false);

	ApplyVisuals();
}

void ABHTrainAquarium::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainAquarium, StartleStartServerTime);
}

void ABHTrainAquarium::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = GS ? GS->GetServerWorldTimeSeconds() : Time;
	const float Dart = (Now - StartleStartServerTime < 1.6f) ? 3.0f : 1.0f;

	for (int32 Index = 0; Index < Fish.Num(); ++Index)
	{
		if (!Fish[Index])
		{
			continue;
		}
		const float Phase = Index * 1.4f;
		const float SpeedX = (0.45f + 0.12f * Index) * Dart;
		const float X = FMath::Sin(Time * SpeedX + Phase) * 56.0f;
		const float Z = 120.0f + FMath::Sin(Time * 0.6f + Phase * 2.0f) * 18.0f;
		const float Y = -6.0f + FMath::Sin(Time * 0.4f + Phase) * 8.0f;
		const float Heading = FMath::Cos(Time * SpeedX + Phase) >= 0.0f ? 0.0f : 180.0f;
		Fish[Index]->SetRelativeLocation(FVector(X, Y, Z));
		Fish[Index]->SetRelativeRotation(FRotator(0.0f, Heading, 0.0f));
	}
}

bool ABHTrainAquarium::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainAquarium::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	StartleStartServerTime = GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	ForceNetUpdate();
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(TEXT("You tap the glass. The fish dart away."), 2.0f);
	}
}

FText ABHTrainAquarium::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Tap the glass"));
}

void ABHTrainAquarium::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetRelativeScale3D(FVector(1.5f, 0.55f, 1.6f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	}

	BHPropVisuals::ConfigurePart(Stand, BHPropVisuals::CubeMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator, FVector(1.5f, 0.55f, 0.80f), true);
	BHPropVisuals::ConfigurePart(Glass, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 22.0f, 120.0f), FRotator::ZeroRotator, FVector(1.5f, 0.05f, 0.82f), false);
	BHPropVisuals::ConfigurePart(Water, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 88.0f), FRotator::ZeroRotator, FVector(1.42f, 0.46f, 0.06f), false);

	BHPropVisuals::TintPart(Stand, FLinearColor(0.2f, 0.13f, 0.08f, 1.0f));
	BHPropVisuals::TintPart(Glass, FLinearColor(0.03f, 0.10f, 0.20f, 1.0f), 0.15f);
	BHPropVisuals::TintPart(Water, FLinearColor(0.10f, 0.40f, 0.75f, 1.0f), 0.35f);

	for (int32 Index = 0; Index < Fish.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(Fish[Index], BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, FVector(0.10f, 0.05f, 0.06f), false);
		BHPropVisuals::TintPart(Fish[Index], FishTints[Index % BHAquariumFish], 0.4f);
	}
}
