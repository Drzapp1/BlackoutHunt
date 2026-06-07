// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainTunnelMotionActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABHTrainTunnelMotionActor::ABHTrainTunnelMotionActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.0f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	LoopLength = 2400.0f;
	MotionSpeed = 840.0f;
	MotionOffset = 0.0f;
	PillarOffset = 0.0f;
	bMoving = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 Index = 0; Index < 16; ++Index)
	{
		UStaticMeshComponent* Strip = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("LightStrip_%02d"), Index));
		Strip->SetupAttachment(SceneRoot);
		Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Strip->SetCastShadow(false);
		// Strips live in the service gap between the carriage window glass (y=+/-300) and the opaque
		// tunnel backdrop (y=+/-580). Keep them short enough in Y that they never poke through the glass
		// into the interior nor clip the backdrop: centred at y=+/-430, half-length 110cm -> y[320,540].
		Strip->SetRelativeScale3D(FVector(0.06f, 2.2f, 0.08f));
		if (CubeMesh.Succeeded())
		{
			Strip->SetStaticMesh(CubeMesh.Object);
		}
		LightStrips.Add(Strip);
	}

	// Near layer: tall, thin vertical pillars (tunnel supports) that scroll faster than the light bars -- the
	// parallax between the two layers is what makes the windows read as a real moving tunnel.
	for (int32 Index = 0; Index < 10; ++Index)
	{
		UStaticMeshComponent* Pillar = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Pillar_%02d"), Index));
		Pillar->SetupAttachment(SceneRoot);
		Pillar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pillar->SetCastShadow(false);
		// Slightly nearer the glass than the light bars, full-height and thin.
		Pillar->SetRelativeScale3D(FVector(0.10f, 0.55f, 3.0f));
		if (CubeMesh.Succeeded())
		{
			Pillar->SetStaticMesh(CubeMesh.Object);
		}
		Pillars.Add(Pillar);
	}
}

void ABHTrainTunnelMotionActor::BeginPlay()
{
	Super::BeginPlay();

	// Build the dynamic materials ONCE here (the old code recreated them every tick). Skip on a dedicated server,
	// which renders nothing.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	StripMaterials.Reset();
	for (UStaticMeshComponent* Strip : LightStrips)
	{
		StripMaterials.Add(Strip ? Strip->CreateAndSetMaterialInstanceDynamic(0) : nullptr);
	}
	PillarMaterials.Reset();
	for (UStaticMeshComponent* Pillar : Pillars)
	{
		PillarMaterials.Add(Pillar ? Pillar->CreateAndSetMaterialInstanceDynamic(0) : nullptr);
	}
}

void ABHTrainTunnelMotionActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMoving)
	{
		MotionOffset = FMath::Fmod(MotionOffset + MotionSpeed * DeltaSeconds, FMath::Max(1.0f, LoopLength));
		// Near layer sweeps ~1.7x faster for parallax. Its loop is a fraction of the far loop so pillars recur
		// more often than the wall lights.
		const float PillarLoop = FMath::Max(1.0f, LoopLength * 0.6f);
		PillarOffset = FMath::Fmod(PillarOffset + MotionSpeed * 1.7f * DeltaSeconds, PillarLoop);
	}

	// Far layer: horizontal wall lights, gently pulsing.
	const float Step = LoopLength / FMath::Max(1, LightStrips.Num());
	for (int32 Index = 0; Index < LightStrips.Num(); ++Index)
	{
		if (!LightStrips[Index])
		{
			continue;
		}

		const float LocalX = FMath::Fmod(Index * Step - MotionOffset + LoopLength, LoopLength) - LoopLength * 0.5f;
		LightStrips[Index]->SetRelativeLocation(FVector(LocalX, 0.0f, 0.0f));
		if (StripMaterials.IsValidIndex(Index))
		{
			if (UMaterialInstanceDynamic* StripMaterial = StripMaterials[Index])
			{
				const float Pulse = 0.5f + 0.5f * FMath::Sin((MotionOffset + Index * 37.0f) * 0.02f);
				const FLinearColor Color = FLinearColor(0.10f, 0.62f + Pulse * 0.25f, 0.78f + Pulse * 0.18f, 1.0f);
				StripMaterial->SetVectorParameterValue(TEXT("Color"), Color);
				StripMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
				StripMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * (bMoving ? 3.0f : 0.6f));
			}
		}
	}

	// Near layer: vertical pillars rushing past faster (the parallax that makes it feel like motion). Dim, cool,
	// and only "lit" while moving so a parked train looks parked.
	const float PillarLoop = FMath::Max(1.0f, LoopLength * 0.6f);
	const float PillarStep = PillarLoop / FMath::Max(1, Pillars.Num());
	for (int32 Index = 0; Index < Pillars.Num(); ++Index)
	{
		if (!Pillars[Index])
		{
			continue;
		}

		const float LocalX = FMath::Fmod(Index * PillarStep - PillarOffset + PillarLoop, PillarLoop) - PillarLoop * 0.5f;
		Pillars[Index]->SetRelativeLocation(FVector(LocalX, -110.0f, 0.0f));
		if (PillarMaterials.IsValidIndex(Index))
		{
			if (UMaterialInstanceDynamic* PillarMaterial = PillarMaterials[Index])
			{
				const FLinearColor Color(0.05f, 0.07f, 0.09f, 1.0f);
				PillarMaterial->SetVectorParameterValue(TEXT("Color"), Color);
				PillarMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
				PillarMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * (bMoving ? 2.2f : 0.4f));
			}
		}
	}
}

void ABHTrainTunnelMotionActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainTunnelMotionActor, bMoving);
}

void ABHTrainTunnelMotionActor::SetMoving(bool bNewMoving)
{
	bMoving = bNewMoving;
}

void ABHTrainTunnelMotionActor::ConfigureMotion(float NewLoopLength, float NewSpeed)
{
	LoopLength = FMath::Max(500.0f, NewLoopLength);
	MotionSpeed = FMath::Max(0.0f, NewSpeed);
}
