#include "BHJumpscareMonster.h"
#include "BHAmbientEmitter.h"
#include "BHCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void ConfigurePart(UStaticMeshComponent* Part, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale)
{
	if (!Part)
	{
		return;
	}

	Part->SetStaticMesh(Mesh);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCastShadow(true);
	Part->SetHiddenInGame(false);
	Part->SetVisibility(true, true);
	Part->SetMobility(EComponentMobility::Movable);
}

void TintPart(UStaticMeshComponent* Part, const FLinearColor& Color)
{
	if (!Part)
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = Part->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
		DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 2.5f);
		DynamicMaterial->SetVectorParameterValue(TEXT("Emissive"), Color * 2.5f);
	}
}
}

ABHJumpscareMonster::ABHJumpscareMonster()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Root);

	MonsterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMonsterMesh"));
	MonsterMesh->SetupAttachment(VisualRoot);
	MonsterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MonsterMesh->SetCastShadow(true);
	MonsterMesh->SetMobility(EComponentMobility::Movable);
	MonsterMesh->SetHiddenInGame(true);
	MonsterMesh->SetVisibility(false, true);

	SkeletalMonsterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PrototypeMonsterSkeletalMesh"));
	SkeletalMonsterMesh->SetupAttachment(VisualRoot);
	SkeletalMonsterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMonsterMesh->SetCastShadow(true);
	SkeletalMonsterMesh->SetMobility(EComponentMobility::Movable);
	SkeletalMonsterMesh->SetHiddenInGame(true);
	SkeletalMonsterMesh->SetVisibility(false, true);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(VisualRoot);
	Chest = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chest"));
	Chest->SetupAttachment(VisualRoot);
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(VisualRoot);
	LeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftArm"));
	LeftArm->SetupAttachment(VisualRoot);
	RightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightArm"));
	RightArm->SetupAttachment(VisualRoot);
	LeftLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLeg"));
	LeftLeg->SetupAttachment(VisualRoot);
	RightLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLeg"));
	RightLeg->SetupAttachment(VisualRoot);
	LeftEye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftEye"));
	LeftEye->SetupAttachment(VisualRoot);
	RightEye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightEye"));
	RightEye->SetupAttachment(VisualRoot);
	Mouth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mouth"));
	Mouth->SetupAttachment(VisualRoot);
	EyeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLight"));
	EyeLight->SetupAttachment(VisualRoot);
	CoreLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CoreLight"));
	CoreLight->SetupAttachment(VisualRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ConfigurePart(Body, CubeMesh.Object, FVector(0.0f, 0.0f, 190.0f), FVector(1.20f, 0.72f, 2.30f));
		ConfigurePart(Chest, CubeMesh.Object, FVector(66.0f, 0.0f, 205.0f), FVector(0.07f, 0.56f, 0.82f));
		ConfigurePart(Head, CubeMesh.Object, FVector(16.0f, 0.0f, 365.0f), FVector(1.05f, 0.84f, 0.95f));
		ConfigurePart(LeftArm, CubeMesh.Object, FVector(10.0f, -100.0f, 205.0f), FVector(0.30f, 0.34f, 2.75f));
		ConfigurePart(RightArm, CubeMesh.Object, FVector(10.0f, 100.0f, 205.0f), FVector(0.30f, 0.34f, 2.75f));
		ConfigurePart(LeftLeg, CubeMesh.Object, FVector(-14.0f, -38.0f, 54.0f), FVector(0.36f, 0.32f, 1.12f));
		ConfigurePart(RightLeg, CubeMesh.Object, FVector(-14.0f, 38.0f, 54.0f), FVector(0.36f, 0.32f, 1.12f));
		ConfigurePart(LeftEye, CubeMesh.Object, FVector(76.0f, -34.0f, 380.0f), FVector(0.08f, 0.18f, 0.08f));
		ConfigurePart(RightEye, CubeMesh.Object, FVector(76.0f, 34.0f, 380.0f), FVector(0.08f, 0.18f, 0.08f));
		ConfigurePart(Mouth, CubeMesh.Object, FVector(78.0f, 0.0f, 338.0f), FVector(0.055f, 0.44f, 0.055f));
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Body->SetMaterial(0, ShapeMaterial.Object);
		Chest->SetMaterial(0, ShapeMaterial.Object);
		Head->SetMaterial(0, ShapeMaterial.Object);
		LeftArm->SetMaterial(0, ShapeMaterial.Object);
		RightArm->SetMaterial(0, ShapeMaterial.Object);
		LeftLeg->SetMaterial(0, ShapeMaterial.Object);
		RightLeg->SetMaterial(0, ShapeMaterial.Object);
		LeftEye->SetMaterial(0, ShapeMaterial.Object);
		RightEye->SetMaterial(0, ShapeMaterial.Object);
		Mouth->SetMaterial(0, ShapeMaterial.Object);
	}

	MonsterMesh->SetHiddenInGame(true);
	MonsterMesh->SetVisibility(false, true);
	SkeletalMonsterMesh->SetHiddenInGame(true);
	SkeletalMonsterMesh->SetVisibility(false, true);

	UStaticMeshComponent* ProxyParts[] = {
		Body,
		Chest,
		Head,
		LeftArm,
		RightArm,
		LeftLeg,
		RightLeg,
		LeftEye,
		RightEye,
		Mouth
	};
	for (UStaticMeshComponent* Part : ProxyParts)
	{
		if (Part)
		{
			Part->SetHiddenInGame(false);
			Part->SetVisibility(true, true);
		}
	}

	EyeLight->SetRelativeLocation(FVector(92.0f, 0.0f, 375.0f));
	EyeLight->SetLightColor(FLinearColor(1.0f, 0.02f, 0.0f));
	EyeLight->SetIntensity(18000.0f);
	EyeLight->SetAttenuationRadius(1650.0f);
	EyeLight->SetCastShadows(false);
	EyeLight->SetMobility(EComponentMobility::Movable);

	CoreLight->SetRelativeLocation(FVector(82.0f, 0.0f, 218.0f));
	CoreLight->SetLightColor(FLinearColor(1.0f, 0.02f, 0.0f));
	CoreLight->SetIntensity(9000.0f);
	CoreLight->SetAttenuationRadius(1300.0f);
	CoreLight->SetCastShadows(false);
	CoreLight->SetMobility(EComponentMobility::Movable);

	ChargeSpeed = 2200.0f;
	MaxLifetime = 5.0f;
	HoldSeconds = 0.0f;
	SpawnTime = -1.0f;
	bChargeStarted = false;
	SetActorScale3D(FVector(1.7f, 1.7f, 1.7f));
}

void ABHJumpscareMonster::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SetLifeSpan(MaxLifetime + 0.35f);
	SetActorHiddenInGame(false);
	ApplyVisuals();
}

void ABHJumpscareMonster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetWorld())
	{
		const float Age = SpawnTime >= 0.0f ? GetWorld()->GetTimeSeconds() - SpawnTime : 0.0f;
		const bool bHolding = HoldSeconds > 0.0f && Age < HoldSeconds;
		const float Pulse = 0.65f + FMath::Abs(FMath::Sin(Age * (bHolding ? 19.0f : 13.0f))) * 0.35f;
		if (!bHolding && !bChargeStarted)
		{
			StartChargeEffects();
		}

		if (VisualRoot)
		{
			const float RunAlpha = bHolding ? 0.0f : 1.0f;
			const float Bob = FMath::Sin(Age * 42.0f) * 8.0f * RunAlpha;
			const float Roll = FMath::Sin(Age * 38.0f) * 4.5f * RunAlpha;
			VisualRoot->SetRelativeLocation(FVector(0.0f, Bob * 0.35f, FMath::Abs(Bob) * 0.35f));
			VisualRoot->SetRelativeRotation(FRotator(-11.0f * RunAlpha, 0.0f, Roll));
			VisualRoot->SetRelativeScale3D(FVector::OneVector);
		}
		if (EyeLight)
		{
			EyeLight->SetIntensity((15000.0f + 11000.0f * Pulse) * (bHolding ? 1.25f : 1.0f));
		}
		if (CoreLight)
		{
			CoreLight->SetIntensity((6500.0f + 7000.0f * Pulse) * (bHolding ? 1.2f : 1.0f));
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	if (!Target.IsValid() || !GetWorld())
	{
		Destroy();
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (SpawnTime >= 0.0f && Now - SpawnTime > MaxLifetime)
	{
		Destroy();
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 110.0f);
	FVector Delta = TargetLocation - GetActorLocation();
	Delta.Z = 0.0f;
	const FVector Direction = Delta.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	SetActorRotation(Direction.Rotation());
	if (SpawnTime >= 0.0f && Now - SpawnTime < HoldSeconds)
	{
		return;
	}

	if (Delta.SizeSquared() <= FMath::Square(175.0f))
	{
		Destroy();
		return;
	}

	const float ChargeAge = SpawnTime >= 0.0f ? FMath::Max(0.0f, Now - SpawnTime - HoldSeconds) : MaxLifetime;
	const float LaunchMultiplier = ChargeAge < 0.35f ? FMath::Lerp(1.35f, 1.0f, ChargeAge / 0.35f) : 1.0f;
	SetActorLocation(GetActorLocation() + Direction * ChargeSpeed * LaunchMultiplier * DeltaSeconds);
}

void ABHJumpscareMonster::StartChargeEffects()
{
	bChargeStarted = true;

	if (HasAuthority())
	{
		SpawnLaunchScream();
	}
}

void ABHJumpscareMonster::SpawnLaunchScream()
{
	if (!GetWorld())
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector ScreamLocation = Target.IsValid()
		? Target->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f)
		: GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
	if (ABHAmbientEmitter* Scream = GetWorld()->SpawnActor<ABHAmbientEmitter>(ScreamLocation, FRotator::ZeroRotator, Params))
	{
		Scream->Configure(1040.0f, 1.15f, 1.45f, 31.0f);
		Scream->SetLifeSpan(1.25f);
	}
}

void ABHJumpscareMonster::Configure(ABHCharacter* NewTarget, float NewSpeed, float NewLifetime, float NewHoldSeconds)
{
	Target = NewTarget;
	ChargeSpeed = FMath::Max(400.0f, NewSpeed);
	HoldSeconds = FMath::Clamp(NewHoldSeconds, 0.0f, 5.0f);
	MaxLifetime = FMath::Clamp(NewLifetime, FMath::Max(1.0f, HoldSeconds + 0.8f), 12.0f);
	SetLifeSpan(MaxLifetime + 0.35f);
}

void ABHJumpscareMonster::ApplyVisuals()
{
	const FLinearColor BodyColor(0.16f, 0.015f, 0.012f, 1.0f);
	const FLinearColor ArmColor(0.22f, 0.018f, 0.015f, 1.0f);
	const FLinearColor EyeColor(1.0f, 0.04f, 0.02f, 1.0f);
	const FLinearColor ChestColor(0.85f, 0.02f, 0.01f, 1.0f);
	TintPart(Body, BodyColor);
	TintPart(Chest, ChestColor);
	TintPart(Head, BodyColor);
	TintPart(LeftArm, ArmColor);
	TintPart(RightArm, ArmColor);
	TintPart(LeftLeg, ArmColor);
	TintPart(RightLeg, ArmColor);
	TintPart(LeftEye, EyeColor);
	TintPart(RightEye, EyeColor);
	TintPart(Mouth, EyeColor);
}
