// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainGhost.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float BHGhostVisibleSeconds = 4.5f;
	constexpr float BHGhostFadeRate = 0.8f;     // alpha per second (slow materialise)
	const FLinearColor GhostTint(0.62f, 0.78f, 0.95f, 1.0f);
}

ABHTrainGhost::ABHTrainGhost()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	SetReplicateMovement(true);
	InteractionLabel = FText::FromString(TEXT(""));

	Robe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Robe"));
	Robe->SetupAttachment(SceneRoot);
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(SceneRoot);

	Aura = CreateDefaultSubobject<UPointLightComponent>(TEXT("Aura"));
	Aura->SetupAttachment(SceneRoot);
	Aura->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	Aura->SetIntensity(0.0f);
	Aura->SetAttenuationRadius(420.0f);
	Aura->SetLightColor(GhostTint);
	Aura->SetCastShadows(false);

	ApplyVisuals();
	SetActorScale3D(FVector(0.001f));
	SetActorHiddenInGame(true);
}

void ABHTrainGhost::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		HomeLocation = GetActorLocation();
		bHomeSet = true;
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		NextEventTime = Now + FMath::FRandRange(25.0f, 70.0f);   // first haunting after a while
	}
}

void ABHTrainGhost::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainGhost, bShowing);
}

void ABHTrainGhost::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (HasAuthority())
	{
		if (!bHomeSet)
		{
			HomeLocation = GetActorLocation();
			bHomeSet = true;
		}
		if (!bShowing && Now >= NextEventTime)
		{
			const FVector Spot = HomeLocation + FVector(FMath::FRandRange(-550.0f, 550.0f), FMath::FRandRange(-180.0f, 180.0f), 0.0f);
			SetActorLocation(Spot);
			SetActorRotation(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
			bShowing = true;
			NextEventTime = Now + BHGhostVisibleSeconds;
			ForceNetUpdate();
		}
		else if (bShowing && Now >= NextEventTime)
		{
			bShowing = false;
			NextEventTime = Now + FMath::FRandRange(120.0f, 240.0f);
			ForceNetUpdate();
		}
	}

	// Local fade (all clients + host): materialise/dissolve via scale + emissive aura.
	const float Target = bShowing ? 1.0f : 0.0f;
	Alpha = FMath::FInterpConstantTo(Alpha, Target, DeltaSeconds, BHGhostFadeRate);
	const bool bVisible = Alpha > 0.02f;
	SetActorHiddenInGame(!bVisible);
	if (bVisible)
	{
		SetActorScale3D(FVector(FMath::Max(0.05f, Alpha)));
		BHPropVisuals::TintPart(Robe, GhostTint, 0.3f + 0.6f * Alpha);
		BHPropVisuals::TintPart(Head, GhostTint, 0.3f + 0.6f * Alpha);
	}
	if (Aura)
	{
		Aura->SetIntensity(Alpha * 45.0f);
	}
}

bool ABHTrainGhost::CanInteract_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return false;   // you can't touch a ghost
}

void ABHTrainGhost::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	// A tapering robe + a faint head. No collision (it drifts through the world).
	BHPropVisuals::ConfigurePart(Robe, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 70.0f), FRotator::ZeroRotator, FVector(0.42f, 0.42f, 1.4f), false);
	BHPropVisuals::ConfigurePart(Head, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 150.0f), FRotator::ZeroRotator, FVector(0.30f, 0.30f, 0.32f), false);
	BHPropVisuals::TintPart(Robe, GhostTint, 0.5f);
	BHPropVisuals::TintPart(Head, GhostTint, 0.5f);
}
