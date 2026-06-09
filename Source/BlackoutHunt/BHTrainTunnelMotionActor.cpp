// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainTunnelMotionActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// How many sweeping exterior lights each window-side actor carries. Kept small (unshadowed, short radius) so a
	// busy lobby of these stays cheap; spread evenly along the loop so a light is almost always mid-pass.
	constexpr int32 BHNumPassingLights = 2;
}

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
	// parallax between the two layers is what makes the windows read as a real moving tunnel. Enough of them
	// that, spread over the FULL loop (see Tick), every window pane has a pillar sweeping past rather than
	// only the middle ones.
	for (int32 Index = 0; Index < 14; ++Index)
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

	// Sweeping exterior lights: movable, unshadowed point lights that scroll along the window outside and bloom as
	// they pass. They start dark (intensity 0) and are driven entirely in Tick. Parked just outside the glass so the
	// wash reaches the interior.
	for (int32 Index = 0; Index < BHNumPassingLights; ++Index)
	{
		UPointLightComponent* PassLight = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("PassingLight_%02d"), Index));
		PassLight->SetupAttachment(SceneRoot);
		PassLight->SetMobility(EComponentMobility::Movable);
		PassLight->SetCastShadows(false);
		PassLight->SetIntensity(0.0f);
		PassLight->SetAttenuationRadius(520.0f);
		PassLight->SetSourceRadius(12.0f);
		PassLight->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
		PassingLights.Add(PassLight);
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

	// One-time per-light tint so passes vary (warm lamps vs cool platform light) between cars and lights. Position
	// and intensity are animated in Tick; the colour only needs setting once and depends on the now-final location.
	for (int32 Index = 0; Index < PassingLights.Num(); ++Index)
	{
		if (!PassingLights[Index])
		{
			continue;
		}
		const float Hue = FMath::Frac(0.37f * static_cast<float>(Index) + GetActorLocation().X * 0.0007f);
		const FLinearColor Warm(1.0f, 0.86f, 0.66f, 1.0f);
		const FLinearColor Cool(0.72f, 0.86f, 1.0f, 1.0f);
		PassingLights[Index]->SetLightColor(FMath::Lerp(Warm, Cool, Hue));
	}
}

void ABHTrainTunnelMotionActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMoving)
	{
		MotionOffset = FMath::Fmod(MotionOffset + MotionSpeed * DeltaSeconds, FMath::Max(1.0f, LoopLength));
		// Near layer sweeps ~1.7x faster for parallax. It shares the FULL loop length so the pillars tile the
		// whole window band (a shorter loop bunched them into the middle panes and left the ends bare); the
		// parallax comes purely from the faster scroll speed, not from a shorter loop.
		const float PillarLoop = FMath::Max(1.0f, LoopLength);
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
	// and only "lit" while moving so a parked train looks parked. Full-loop spread so every pane gets a pillar.
	const float PillarLoop = FMath::Max(1.0f, LoopLength);
	const float PillarStep = PillarLoop / FMath::Max(1, Pillars.Num());
	// The near layer must sit ~110cm TOWARD the carriage interior from the strips so it reads as closer to the
	// glass than the far layer. The actor is parked just outside one window wall (y=+/-430), so "toward the
	// interior" is -Y for the +Y wall and +Y for the -Y wall. A fixed -110 offset only worked on the +Y wall and
	// shoved the -Y wall's pillars back against the backdrop (y-540), which is why the bars showed on one side
	// only. Derive the sign from the actor's own Y so both walls get prominent, glass-hugging pillars.
	const float NearOffset = (GetActorLocation().Y >= 0.0f) ? -110.0f : 110.0f;
	for (int32 Index = 0; Index < Pillars.Num(); ++Index)
	{
		if (!Pillars[Index])
		{
			continue;
		}

		const float LocalX = FMath::Fmod(Index * PillarStep - PillarOffset + PillarLoop, PillarLoop) - PillarLoop * 0.5f;
		Pillars[Index]->SetRelativeLocation(FVector(LocalX, NearOffset, 0.0f));
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

	// Sweeping exterior lights: scroll along the window like the strips, bloom as they cross the pane centre and
	// fade to dark toward the ends, so the carriage interior gets a moving wash that reads as lit things passing
	// outside. Sit just inside the strips (toward the glass) so the cast reaches the interior; dark while parked.
	const float LightLoop = FMath::Max(1.0f, LoopLength);
	const float LightStep = LightLoop / FMath::Max(1, PassingLights.Num());
	const float LightY = (GetActorLocation().Y >= 0.0f) ? -95.0f : 95.0f;
	for (int32 Index = 0; Index < PassingLights.Num(); ++Index)
	{
		if (!PassingLights[Index])
		{
			continue;
		}
		const float LocalX = FMath::Fmod(Index * LightStep - MotionOffset + LightLoop, LightLoop) - LightLoop * 0.5f;
		PassingLights[Index]->SetRelativeLocation(FVector(LocalX, LightY, 0.0f));

		// Bloom near the pane centre (LocalX ~ 0), dark toward the ends -> a discrete lamp sweeping by, squared so
		// it reads as a pass rather than a constant glow.
		const float Span = FMath::Max(1.0f, LightLoop * 0.30f);
		const float Through = FMath::Clamp(1.0f - FMath::Abs(LocalX) / Span, 0.0f, 1.0f);
		PassingLights[Index]->SetIntensity(bMoving ? 260.0f * Through * Through : 0.0f);
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
