// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainToyTrain.h"
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
	constexpr float BHToyA = 70.0f;        // oval semi-axis X
	constexpr float BHToyB = 42.0f;        // oval semi-axis Y
	constexpr float BHToyZ = 92.0f;        // tabletop height
	constexpr float BHToySpeed = 0.8f;     // rad/s
	constexpr float BHToySpacing = 0.55f;  // theta gap between cars
	constexpr int32 BHToyTrackTies = 16;

	FVector OvalLocal(float Theta, float Z)
	{
		return FVector(BHToyA * FMath::Cos(Theta), BHToyB * FMath::Sin(Theta), Z);
	}

	float OvalYaw(float Theta)
	{
		// Tangent of (A cosθ, B sinθ) is (-A sinθ, B cosθ).
		return FMath::RadiansToDegrees(FMath::Atan2(BHToyB * FMath::Cos(Theta), -BHToyA * FMath::Sin(Theta)));
	}
}

ABHTrainToyTrain::ABHTrainToyTrain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Model Train"));

	Table = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Table"));
	Table->SetupAttachment(SceneRoot);

	TrackSegments.Reserve(BHToyTrackTies);
	for (int32 Index = 0; Index < BHToyTrackTies; ++Index)
	{
		UStaticMeshComponent* Tie = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Tie_%d"), Index));
		Tie->SetupAttachment(SceneRoot);
		TrackSegments.Add(Tie);
	}

	Engine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Engine"));
	Engine->SetupAttachment(SceneRoot);
	Funnel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Funnel"));
	Funnel->SetupAttachment(SceneRoot);
	Car1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Car1"));
	Car1->SetupAttachment(SceneRoot);
	Car2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Car2"));
	Car2->SetupAttachment(SceneRoot);

	Headlight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Headlight"));
	Headlight->SetupAttachment(SceneRoot);
	Headlight->SetIntensity(8.0f);
	Headlight->SetAttenuationRadius(140.0f);
	Headlight->SetLightColor(FLinearColor(1.0f, 0.9f, 0.6f, 1.0f));
	Headlight->SetCastShadows(false);

	ApplyVisuals();
}

void ABHTrainToyTrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainToyTrain, TootTime);
}

void ABHTrainToyTrain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Base = T * BHToySpeed;

	auto Place = [&](UStaticMeshComponent* Comp, float Theta, float ZExtra)
	{
		if (Comp)
		{
			Comp->SetRelativeLocation(OvalLocal(Theta, BHToyZ + ZExtra));
			Comp->SetRelativeRotation(FRotator(0.0f, OvalYaw(Theta), 0.0f));
		}
	};

	Place(Engine, Base, 0.0f);
	Place(Funnel, Base, 9.0f);
	Place(Car1, Base - BHToySpacing, 0.0f);
	Place(Car2, Base - 2.0f * BHToySpacing, 0.0f);

	if (Headlight)
	{
		Headlight->SetRelativeLocation(OvalLocal(Base + 0.08f, BHToyZ + 4.0f));
		// Toot test runs off the synchronized server clock so the flash window matches on every client.
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : T;
		const bool bToot = (ServerNow - TootTime) < 0.6f;
		Headlight->SetIntensity(bToot ? 40.0f : 9.0f);
	}
}

bool ABHTrainToyTrain::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainToyTrain::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	TootTime = GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	ForceNetUpdate();
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(TEXT("Toot toot! The little train chugs on."), 2.0f);
	}
}

FText ABHTrainToyTrain::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Toot the whistle"));
}

void ABHTrainToyTrain::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetRelativeScale3D(FVector(1.7f, 1.1f, 0.85f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 44.0f));
	}
	// Tabletop the layout sits on.
	BHPropVisuals::ConfigurePart(Table, BHPropVisuals::CubeMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 44.0f), FRotator::ZeroRotator, FVector(1.7f, 1.1f, 0.85f), true);
	BHPropVisuals::TintPart(Table, FLinearColor(0.22f, 0.14f, 0.08f, 1.0f));

	// Oval of track ties (placed once).
	for (int32 Index = 0; Index < TrackSegments.Num(); ++Index)
	{
		const float Theta = (static_cast<float>(Index) / BHToyTrackTies) * 2.0f * PI;
		if (TrackSegments[Index])
		{
			BHPropVisuals::ConfigurePart(TrackSegments[Index], BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), OvalLocal(Theta, BHToyZ - 3.0f), FRotator(0.0f, OvalYaw(Theta) + 90.0f, 0.0f), FVector(0.03f, 0.10f, 0.01f), false);
			BHPropVisuals::TintPart(TrackSegments[Index], FLinearColor(0.18f, 0.16f, 0.14f, 1.0f));
		}
	}

	BHPropVisuals::ConfigurePart(Engine, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), OvalLocal(0.0f, BHToyZ), FRotator::ZeroRotator, FVector(0.13f, 0.07f, 0.08f), false);
	BHPropVisuals::ConfigurePart(Funnel, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), OvalLocal(0.0f, BHToyZ + 9.0f), FRotator::ZeroRotator, FVector(0.025f, 0.025f, 0.05f), false);
	BHPropVisuals::ConfigurePart(Car1, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), OvalLocal(-BHToySpacing, BHToyZ), FRotator::ZeroRotator, FVector(0.11f, 0.065f, 0.06f), false);
	BHPropVisuals::ConfigurePart(Car2, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), OvalLocal(-2.0f * BHToySpacing, BHToyZ), FRotator::ZeroRotator, FVector(0.11f, 0.065f, 0.06f), false);

	BHPropVisuals::TintPart(Engine, FLinearColor(0.7f, 0.12f, 0.12f, 1.0f), 0.1f);
	BHPropVisuals::TintPart(Funnel, FLinearColor(0.05f, 0.05f, 0.06f, 1.0f));
	BHPropVisuals::TintPart(Car1, FLinearColor(0.15f, 0.45f, 0.7f, 1.0f), 0.1f);
	BHPropVisuals::TintPart(Car2, FLinearColor(0.2f, 0.6f, 0.3f, 1.0f), 0.1f);
}
