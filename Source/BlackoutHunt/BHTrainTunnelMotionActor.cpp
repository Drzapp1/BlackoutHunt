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
	bMoving = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 Index = 0; Index < 12; ++Index)
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
}

void ABHTrainTunnelMotionActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMoving)
	{
		MotionOffset = FMath::Fmod(MotionOffset + MotionSpeed * DeltaSeconds, FMath::Max(1.0f, LoopLength));
	}

	const float Step = LoopLength / FMath::Max(1, LightStrips.Num());
	for (int32 Index = 0; Index < LightStrips.Num(); ++Index)
	{
		if (!LightStrips[Index])
		{
			continue;
		}

		float LocalX = FMath::Fmod(Index * Step - MotionOffset + LoopLength, LoopLength) - LoopLength * 0.5f;
		LightStrips[Index]->SetRelativeLocation(FVector(LocalX, 0.0f, 0.0f));
		const float Pulse = 0.5f + 0.5f * FMath::Sin((MotionOffset + Index * 37.0f) * 0.02f);
		UMaterialInstanceDynamic* DynamicMaterial = LightStrips[Index]->CreateAndSetMaterialInstanceDynamic(0);
		if (DynamicMaterial)
		{
			const FLinearColor Color = FLinearColor(0.10f, 0.62f + Pulse * 0.25f, 0.78f + Pulse * 0.18f, 1.0f);
			DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
			DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
			DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * (bMoving ? 3.0f : 0.6f));
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
