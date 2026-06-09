// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainHandStrap.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 BHMaxStraps = 8;
}

ABHTrainHandStrap::ABHTrainHandStrap()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;                 // exist on clients (spawned by the server gamemode); sway is local/cosmetic
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.0f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 Index = 0; Index < BHMaxStraps; ++Index)
	{
		USceneComponent* Pivot = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Pivot_%d"), Index));
		Pivot->SetupAttachment(SceneRoot);

		UStaticMeshComponent* Strap = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Strap_%d"), Index));
		Strap->SetupAttachment(Pivot);
		Strap->SetRelativeLocation(FVector(0.0f, 0.0f, -18.0f));   // hangs below the pivot, so it swings like a pendulum
		Strap->SetRelativeScale3D(FVector(0.025f, 0.05f, 0.36f));
		Strap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Strap->SetCastShadow(false);
		if (CubeMesh.Succeeded())
		{
			Strap->SetStaticMesh(CubeMesh.Object);
		}

		Pivots.Add(Pivot);
		Straps.Add(Strap);
	}

	ConfigureRow(7, 520.0f);
}

void ABHTrainHandStrap::ConfigureRow(int32 InCount, float InHalfSpanX)
{
	const int32 Count = FMath::Clamp(InCount, 1, Pivots.Num());
	for (int32 Index = 0; Index < Pivots.Num(); ++Index)
	{
		const bool bActive = Index < Count;
		if (Straps[Index])
		{
			Straps[Index]->SetVisibility(bActive);
		}
		if (Pivots[Index] && bActive)
		{
			const float Frac = (Count > 1) ? (static_cast<float>(Index) / static_cast<float>(Count - 1)) : 0.5f;
			const float X = -InHalfSpanX + 2.0f * InHalfSpanX * Frac;
			Pivots[Index]->SetRelativeLocation(FVector(X, 0.0f, 0.0f));
		}
	}
}

void ABHTrainHandStrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;   // purely cosmetic
	}

	ElapsedSeconds += DeltaSeconds;
	const float T = ElapsedSeconds;
	for (int32 Index = 0; Index < Pivots.Num(); ++Index)
	{
		if (!Pivots[Index] || (Straps[Index] && !Straps[Index]->IsVisible()))
		{
			continue;
		}
		// Pendulum swing: a dominant lateral rock + a fainter fore/aft nod, staggered per strap so the row doesn't
		// swing as one rigid bar. Degrees.
		const float Phase = static_cast<float>(Index) * 0.7f;
		const float Roll = 7.5f * FMath::Sin(T * 1.1f + Phase) + 2.4f * FMath::Sin(T * 1.9f + Phase * 1.7f);
		const float Pitch = 3.0f * FMath::Sin(T * 0.8f + Phase * 1.3f);
		Pivots[Index]->SetRelativeRotation(FRotator(Pitch, 0.0f, Roll));
	}
}
