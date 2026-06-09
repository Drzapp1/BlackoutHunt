// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainCat.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float BHCatSpeed = 110.0f;        // cm/s amble
	constexpr float BHCatRoamX = 1000.0f;       // wander box half-extents around home
	constexpr float BHCatRoamY = 160.0f;

	const FLinearColor CatCoats[4] = {
		FLinearColor(0.85f, 0.45f, 0.15f, 1.0f),   // orange tabby
		FLinearColor(0.06f, 0.06f, 0.07f, 1.0f),   // black
		FLinearColor(0.50f, 0.50f, 0.52f, 1.0f),   // grey
		FLinearColor(0.90f, 0.90f, 0.86f, 1.0f),   // white
	};
}

ABHTrainCat::ABHTrainCat()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	SetReplicateMovement(true);
	InteractionLabel = FText::FromString(TEXT("Cat"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(SceneRoot);
	EarL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EarL"));
	EarL->SetupAttachment(SceneRoot);
	EarR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EarR"));
	EarR->SetupAttachment(SceneRoot);
	Tail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tail"));
	Tail->SetupAttachment(SceneRoot);
}

void ABHTrainCat::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		CoatIndex = static_cast<uint8>(FMath::RandRange(0, 3));
		HomeLocation = GetActorLocation();
		bHomeSet = true;
		PickNewTarget();
		ForceNetUpdate();
	}
	ApplyCatVisuals();
}

void ABHTrainCat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainCat, CoatIndex);
	DOREPLIFETIME(ABHTrainCat, PetCount);
}

void ABHTrainCat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (!bHomeSet)
	{
		HomeLocation = GetActorLocation();
		bHomeSet = true;
		PickNewTarget();
	}

	if (SitTimer > 0.0f)
	{
		SitTimer -= DeltaSeconds;
		return;   // sitting still while being petted
	}
	if (IdleTimer > 0.0f)
	{
		IdleTimer -= DeltaSeconds;
		return;
	}

	const FVector Loc = GetActorLocation();
	FVector ToTarget = TargetLocation - Loc;
	ToTarget.Z = 0.0f;
	const float Dist = ToTarget.Size();
	if (Dist < 16.0f)
	{
		if (FMath::FRand() < 0.4f)
		{
			IdleTimer = FMath::FRandRange(1.5f, 4.0f);   // pause and look around
		}
		else
		{
			PickNewTarget();
		}
		return;
	}

	const FVector Step = ToTarget.GetSafeNormal() * BHCatSpeed * DeltaSeconds;
	SetActorLocation(FVector(Loc.X + Step.X, Loc.Y + Step.Y, Loc.Z));
	SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
}

void ABHTrainCat::PickNewTarget()
{
	TargetLocation = HomeLocation + FVector(FMath::FRandRange(-BHCatRoamX, BHCatRoamX), FMath::FRandRange(-BHCatRoamY, BHCatRoamY), 0.0f);
}

bool ABHTrainCat::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainCat::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	SitTimer = 4.0f;   // sit and enjoy the attention
	IdleTimer = 0.0f;
	++PetCount;
	ForceNetUpdate();

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		static const TCHAR* Purrs[] = { TEXT("The cat purrs."), TEXT("The cat headbutts your hand."), TEXT("The cat blinks slowly at you."), TEXT("The cat flops over, content.") };
		PC->ClientShowStatusMessage(Purrs[PetCount % 4], 2.5f);
	}
}

FText ABHTrainCat::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Pet the cat"));
}

FBHInteractionPromptInfo ABHTrainCat::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("PET"));
	}
	return Info;
}

void ABHTrainCat::OnRep_Coat()
{
	ApplyCatVisuals();
}

void ABHTrainCat::ApplyCatVisuals()
{
	// Inherited collision cube is the interaction target but invisible; keep it small.
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetRelativeScale3D(FVector(0.55f, 0.30f, 0.40f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
	}

	const FLinearColor Coat = CatCoats[FMath::Clamp(static_cast<int32>(CoatIndex), 0, 3)];
	const FLinearColor Ear = Coat * 0.8f;
	const FLinearColor Pink(0.95f, 0.6f, 0.62f, 1.0f);

	BHPropVisuals::ConfigurePart(Body, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, FVector(0.52f, 0.26f, 0.22f), false);
	BHPropVisuals::ConfigurePart(Head, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(30.0f, 0.0f, 30.0f), FRotator::ZeroRotator, FVector(0.20f, 0.20f, 0.18f), false);
	BHPropVisuals::ConfigurePart(EarL, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(32.0f, -7.0f, 42.0f), FRotator(0.0f, 0.0f, 45.0f), FVector(0.05f, 0.05f, 0.07f), false);
	BHPropVisuals::ConfigurePart(EarR, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(32.0f, 7.0f, 42.0f), FRotator(0.0f, 0.0f, 45.0f), FVector(0.05f, 0.05f, 0.07f), false);
	BHPropVisuals::ConfigurePart(Tail, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(-28.0f, 0.0f, 30.0f), FRotator(50.0f, 0.0f, 0.0f), FVector(0.04f, 0.04f, 0.22f), false);

	BHPropVisuals::TintPart(Body, Coat);
	BHPropVisuals::TintPart(Head, Coat);
	BHPropVisuals::TintPart(EarL, Ear);
	BHPropVisuals::TintPart(EarR, Ear);
	BHPropVisuals::TintPart(Tail, Coat);
	(void)Pink;
}
