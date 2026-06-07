// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainServiceLight.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABHTrainServiceLight::ABHTrainServiceLight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.0f);
	// The boot animation runs entirely client-side off BeginPlay time, so the actor has no changing replicated
	// state -- it only needs its initial spawn to reach clients. Go dormant and cull far connections so a long
	// run of these small fixtures never pays a per-light heartbeat.
	SetNetCullDistanceSquared(7000.0f * 7000.0f);
	NetDormancy = DORM_DormantAll;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FixtureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureMesh"));
	FixtureMesh->SetupAttachment(SceneRoot);
	FixtureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FixtureMesh->SetCastShadow(false);
	// A small, thin, flush ceiling panel -- the visible "module".
	FixtureMesh->SetRelativeScale3D(FVector(0.34f, 0.16f, 0.05f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FixtureMesh->SetStaticMesh(CubeMesh.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(SceneRoot);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, -12.0f));
	// Many small fill lights: keep them shadowless and short-range so they read as a row of modules rather than
	// a few big floods, and so a full carriage of them stays cheap.
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(520.0f);

	StartupColor = FLinearColor(0.85f, 0.05f, 0.03f);   // red "backup power"
	PoweredColor = FLinearColor(1.0f, 0.92f, 0.78f);    // warm, relaxing "mains power"
	StartupIntensity = 240.0f;
	PoweredIntensity = 900.0f;                            // "half on": comfortable, no flashlight needed, not perfect
	BootDelaySeconds = 3.5f;
	BootFadeSeconds = 1.5f;
	bFlickerDuringStartup = true;

	BootStartTime = 0.0f;
	bBootComplete = false;
}

void ABHTrainServiceLight::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		// Nothing to render on a dedicated server; skip the per-tick boot animation entirely.
		SetActorTickEnabled(false);
		return;
	}

	if (FixtureMesh && !FixtureMaterial)
	{
		FixtureMaterial = FixtureMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	BootStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bBootComplete = false;
	ApplyLightLevel(StartupColor, StartupIntensity);
}

void ABHTrainServiceLight::SetRoofModeImmediate()
{
	// Cooler "service white" for the exposed roof, with no red backup phase: fade straight up from a dim version
	// of the powered colour. These roof lights are spawned client-locally (per-player), so this never replicates.
	PoweredColor = FLinearColor(0.78f, 0.90f, 1.0f);
	StartupColor = PoweredColor * 0.25f;
	StartupIntensity = 60.0f;
	PoweredIntensity = 1100.0f;
	if (Light)
	{
		Light->SetAttenuationRadius(720.0f);
	}
	BootDelaySeconds = 0.0f;     // skip the red "backup power" phase
	BootFadeSeconds = 1.2f;      // quick fade-in when the breaker is thrown
	bFlickerDuringStartup = false;
	// On the open roof the visible fixture box reads as ugly floating geometry, so only the illumination should
	// show -- hide the module mesh and keep just the point light.
	if (FixtureMesh)
	{
		FixtureMesh->SetVisibility(false);
		FixtureMesh->SetHiddenInGame(true);
	}
	BootStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bBootComplete = false;
	ApplyLightLevel(StartupColor, StartupIntensity);
}

void ABHTrainServiceLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Light)
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Elapsed = Now - BootStartTime;

	if (Elapsed < BootDelaySeconds)
	{
		// Backup power: dim, red, and slightly unstable until the mains kick in.
		float Intensity = StartupIntensity;
		if (bFlickerDuringStartup)
		{
			const float Flick = 0.70f + 0.30f * FMath::Sin(Now * 22.0f + GetUniqueID() * 0.13f);
			Intensity *= Flick;
		}
		ApplyLightLevel(StartupColor, Intensity);
		return;
	}

	const float FadeAlpha = FMath::Clamp((Elapsed - BootDelaySeconds) / FMath::Max(0.05f, BootFadeSeconds), 0.0f, 1.0f);
	if (FadeAlpha >= 1.0f)
	{
		bBootComplete = true;
		// Steady, comfortable mains power with a barely-there breathe so it feels alive rather than flat.
		const float Breathe = 1.0f + 0.02f * FMath::Sin(Now * 1.3f + GetUniqueID() * 0.07f);
		ApplyLightLevel(PoweredColor, PoweredIntensity * Breathe);
		return;
	}

	// "Power on": ramp from backup red up to the warm mains white.
	const FLinearColor Color = FMath::Lerp(StartupColor, PoweredColor, FadeAlpha);
	const float Intensity = FMath::Lerp(StartupIntensity, PoweredIntensity, FadeAlpha);
	ApplyLightLevel(Color, Intensity);
}

void ABHTrainServiceLight::ApplyLightLevel(const FLinearColor& Color, float Intensity)
{
	if (Light)
	{
		Light->SetLightColor(Color);
		Light->SetIntensity(FMath::Max(0.0f, Intensity));
	}

	if (FixtureMaterial)
	{
		// The visible fixture glows the same colour, scaled brighter so it reads as the actual source.
		const float IntensityAlpha = FMath::Clamp(Intensity / FMath::Max(1.0f, PoweredIntensity), 0.0f, 1.0f);
		const float Emissive = FMath::Lerp(0.6f, 4.0f, IntensityAlpha);
		FixtureMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		FixtureMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
		FixtureMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * Emissive);
	}
}
