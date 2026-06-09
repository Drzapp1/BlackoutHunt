// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainStopCord.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor BHStopIdle(0.16f, 0.015f, 0.015f, 1.0f);   // unlit sign: dark red
	const FLinearColor BHStopHot(1.0f, 0.10f, 0.06f, 1.0f);       // requested: hot red
}

ABHTrainStopCord::ABHTrainStopCord()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// The base 'Mesh' becomes the small button/housing you actually tap (keeps the base block-all collision so the
	// look-trace finds it). Authored for a +Y wall mount facing -Y into the carriage (placed without rotation).
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.22f, 0.10f, 0.28f));
	}

	// Hanging pull cord above the button (cosmetic), drooping slightly into the car.
	Cord = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cord"));
	Cord->SetupAttachment(SceneRoot);
	Cord->SetRelativeLocation(FVector(0.0f, -10.0f, 55.0f));
	Cord->SetRelativeScale3D(FVector(0.03f, 0.03f, 1.1f));
	Cord->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded())
	{
		Cord->SetStaticMesh(CubeMesh.Object);
	}

	// Lit "stop requested" sign panel up near the ceiling, facing into the car.
	Sign = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sign"));
	Sign->SetupAttachment(SceneRoot);
	Sign->SetRelativeLocation(FVector(0.0f, -8.0f, 128.0f));
	Sign->SetRelativeScale3D(FVector(0.90f, 0.05f, 0.26f));
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded())
	{
		Sign->SetStaticMesh(CubeMesh.Object);
	}

	SignLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SignLight"));
	SignLight->SetupAttachment(SceneRoot);
	SignLight->SetRelativeLocation(FVector(0.0f, -28.0f, 128.0f));
	SignLight->SetIntensity(0.0f);
	SignLight->SetAttenuationRadius(360.0f);
	SignLight->SetCastShadows(false);
	SignLight->SetLightColor(BHStopHot);

	InteractionLabel = FText::FromString(TEXT("Request stop"));
}

void ABHTrainStopCord::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer && Sign)
	{
		SignMID = Sign->CreateAndSetMaterialInstanceDynamic(0);
	}
	ApplyVisuals();
}

void ABHTrainStopCord::ApplyVisuals()
{
	const FLinearColor Base = bRequested ? BHStopHot : BHStopIdle;
	if (SignMID)
	{
		SignMID->SetVectorParameterValue(TEXT("Color"), Base);
		SignMID->SetVectorParameterValue(TEXT("BaseColor"), Base);
		SignMID->SetVectorParameterValue(TEXT("EmissiveColor"), Base * (bRequested ? 3.0f : 0.15f));
	}
	if (SignLight)
	{
		SignLight->SetIntensity(bRequested ? 220.0f : 0.0f);
	}
}

void ABHTrainStopCord::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Gentle attention pulse while a stop is requested (client cosmetic only).
	if (!bRequested || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	PulseTime += DeltaSeconds;
	const float Pulse = 0.72f + 0.28f * FMath::Sin(PulseTime * 6.0f);
	if (SignMID)
	{
		SignMID->SetVectorParameterValue(TEXT("EmissiveColor"), BHStopHot * (3.0f * Pulse));
	}
	if (SignLight)
	{
		SignLight->SetIntensity(220.0f * Pulse);
	}
}

void ABHTrainStopCord::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainStopCord, bRequested);
}

void ABHTrainStopCord::OnRep_Requested()
{
	PulseTime = 0.0f;
	ApplyVisuals();
}

bool ABHTrainStopCord::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainStopCord::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	bRequested = !bRequested;
	PulseTime = 0.0f;
	ForceNetUpdate();
	ApplyVisuals();   // listen-server host updates immediately; remote clients via OnRep.
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(bRequested ? TEXT("Stop requested -- the sign is lit.") : TEXT("Stop request cancelled."), 2.0f);
	}
}

FText ABHTrainStopCord::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(bRequested ? TEXT("Cancel stop request") : TEXT("Request stop"));
}
