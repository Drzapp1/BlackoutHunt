// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHRoofStarfield.h"
#include "BHPropVisuals.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABHRoofStarfield::ABHRoofStarfield()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.0f);
	// No changing replicated state -- the dome is rebuilt deterministically in BeginPlay on every machine, so the
	// actor only needs its initial spawn to reach clients.
	NetDormancy = DORM_DormantAll;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Stars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stars"));
	Stars->SetupAttachment(SceneRoot);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCastShadow(false);
	Stars->SetMobility(EComponentMobility::Static);

	FairyLights = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FairyLights"));
	FairyLights->SetupAttachment(SceneRoot);
	FairyLights->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FairyLights->SetCastShadow(false);
	FairyLights->SetMobility(EComponentMobility::Static);

	if (SphereMesh.Succeeded())
	{
		Stars->SetStaticMesh(SphereMesh.Object);
		FairyLights->SetStaticMesh(SphereMesh.Object);
	}
}

void ABHRoofStarfield::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer || bBuilt)
	{
		// Nothing to render on a dedicated server, and never rebuild twice.
		return;
	}
	bBuilt = true;

	BuildStars();
	BuildFairyLights();
}

void ABHRoofStarfield::BuildStars()
{
	if (!Stars)
	{
		return;
	}

	// Effectively UNLIT: a black base colour means the flashlight (and every scene light) has nothing to diffusely
	// reflect, so the stars read purely from their cranked emissive -- they never "light up" under the torch. Very
	// bright so they still pop from far away.
	if (UMaterialInstanceDynamic* StarMaterial = Stars->CreateDynamicMaterialInstance(0, BHPropVisuals::BasicMaterial()))
	{
		const FLinearColor StarColor(0.85f, 0.92f, 1.0f, 1.0f);
		StarMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::Black);
		StarMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor::Black);
		// Kill any flashlight specular glint too, where the base material exposes these (harmless no-op otherwise).
		StarMaterial->SetScalarParameterValue(TEXT("Specular"), 0.0f);
		StarMaterial->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
		StarMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), StarColor * 26.0f);
	}

	// Deterministic dome (fixed seed) so every client renders the same sky with no replicated state. Pushed FAR
	// out and high up (well beyond flashlight range) so it reads as a real night sky; bigger per-star so the
	// distant points still register. The opaque roof slab hides it from inside the carriage.
	FRandomStream Rng(1337);
	const int32 StarCount = 230;
	for (int32 Index = 0; Index < StarCount; ++Index)
	{
		const float X = Rng.FRandRange(-12000.0f, 12000.0f);
		const float Y = Rng.FRandRange(-6000.0f, 6000.0f);
		const float Z = Rng.FRandRange(3200.0f, 7400.0f);
		const float StarScale = Rng.FRandRange(0.5f, 1.7f);
		FTransform StarTransform;
		StarTransform.SetLocation(FVector(X, Y, Z));
		StarTransform.SetScale3D(FVector(StarScale));
		Stars->AddInstance(StarTransform);
	}
}

void ABHRoofStarfield::BuildFairyLights()
{
	if (!FairyLights)
	{
		return;
	}

	// Warm festoon glow strung along the -Y roof edge (clear of the centreline hatch and the +Y breaker side).
	if (UMaterialInstanceDynamic* FairyMaterial = FairyLights->CreateDynamicMaterialInstance(0, BHPropVisuals::BasicMaterial()))
	{
		const FLinearColor FairyColor(1.0f, 0.80f, 0.45f, 1.0f);
		// Black base = unlit read (the flashlight never washes the bulbs); the warm glow is all emissive.
		FairyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::Black);
		FairyMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor::Black);
		FairyMaterial->SetScalarParameterValue(TEXT("Specular"), 0.0f);
		FairyMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), FairyColor * 7.0f);
	}

	const int32 BeadCount = 26;
	const float StartX = -2200.0f;
	const float EndX = 2200.0f;
	const float EdgeY = -250.0f;
	for (int32 Index = 0; Index < BeadCount; ++Index)
	{
		const float Alpha = BeadCount > 1 ? static_cast<float>(Index) / static_cast<float>(BeadCount - 1) : 0.0f;
		const float X = FMath::Lerp(StartX, EndX, Alpha);
		// Gentle catenary sag between supports so it reads as a hung string, not a rigid bar.
		const float Sag = 10.0f * FMath::Sin(Alpha * PI * 5.0f);
		FTransform BeadTransform;
		BeadTransform.SetLocation(FVector(X, EdgeY, 340.0f - Sag));
		BeadTransform.SetScale3D(FVector(0.10f));
		FairyLights->AddInstance(BeadTransform);
	}
}
