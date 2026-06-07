// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHJumpscareMonster.h"
#include "BHAmbientEmitter.h"
#include "BHCharacter.h"
#include "BHGameSettings.h"
#include "BHJumpscarePresentation.h"
#include "BHJumpscareVariantLibrary.h"
#include "BHPlayerController.h"
#include "Animation/AnimSequence.h"
#include "Components/ChildActorComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float BHJumpscareProxyVisualRootScale = 0.62f;
constexpr float BHJumpscareProxyCloseVisualRootScale = 0.40f;
constexpr float BHJumpscareWorldTargetVisualHeight = 265.0f;
constexpr float BHJumpscareCloseTargetVisualHeight = 165.0f;
// A full-body close-up (the in-your-face slam, the super-chain finale) shows the creature head to toe, so it
// is fit to a taller height than the upper-body framing — otherwise the whole mesh shrinks to fit a short
// torso target and the complete monster reads as too small.
constexpr float BHJumpscareCloseFullBodyTargetVisualHeight = 250.0f;
constexpr float BHJumpscareMinVisualScaleAxis = 0.05f;
// High ceiling so a per-variant scale can compensate for a mesh imported at the wrong unit scale (e.g. the
// SCP-096 FBX authored in metres needs ~100x). Normal creatures sit far below this; it only caps garbage.
constexpr float BHJumpscareMaxVisualScaleAxis = 150.0f;

FVector BHSanitizeJumpscareScale(const FVector& RawScale)
{
	FVector Scale = RawScale.GetAbs();
	if (Scale.IsNearlyZero())
	{
		return FVector::OneVector;
	}

	Scale.X = FMath::Clamp(Scale.X, BHJumpscareMinVisualScaleAxis, BHJumpscareMaxVisualScaleAxis);
	Scale.Y = FMath::Clamp(Scale.Y, BHJumpscareMinVisualScaleAxis, BHJumpscareMaxVisualScaleAxis);
	Scale.Z = FMath::Clamp(Scale.Z, BHJumpscareMinVisualScaleAxis, BHJumpscareMaxVisualScaleAxis);
	return Scale;
}

float BHMeshVisualHeight(const USkeletalMesh* Mesh)
{
	return Mesh ? Mesh->GetBounds().BoxExtent.Z * 2.0f : 0.0f;
}

float BHMeshVisualHeight(const UStaticMesh* Mesh)
{
	return Mesh ? Mesh->GetBounds().BoxExtent.Z * 2.0f : 0.0f;
}

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

bool FindJumpscareVariantById(FName VariantId, FBHJumpscareVariant& OutVariant)
{
	return FindResolvedJumpscareVariantById(VariantId, OutVariant);
}

bool BHSoftObjectPathExists(const FSoftObjectPath& ObjectPath)
{
	if (ObjectPath.IsNull())
	{
		return false;
	}

	const FString PackageName = ObjectPath.GetLongPackageName();
	return !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
}

bool BHJumpscareNameLooksLowerBody(const FString& RawName)
{
	FString Name = RawName.ToLower();
	return Name.Contains(TEXT("leg"))
		|| Name.Contains(TEXT("thigh"))
		|| Name.Contains(TEXT("calf"))
		|| Name.Contains(TEXT("shin"))
		|| Name.Contains(TEXT("knee"))
		|| Name.Contains(TEXT("foot"))
		|| Name.Contains(TEXT("toe"));
}

void BHHideLowerBodyForCloseup(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TArray<USkeletalMeshComponent*> SkeletalComponents;
	Actor->GetComponents<USkeletalMeshComponent>(SkeletalComponents);
	for (USkeletalMeshComponent* SkeletalComponent : SkeletalComponents)
	{
		if (!SkeletalComponent)
		{
			continue;
		}

		const int32 BoneCount = SkeletalComponent->GetNumBones();
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			const FName BoneName = SkeletalComponent->GetBoneName(BoneIndex);
			if (!BoneName.IsNone() && BHJumpscareNameLooksLowerBody(BoneName.ToString()))
			{
				SkeletalComponent->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
			}
		}
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && BHJumpscareNameLooksLowerBody(PrimitiveComponent->GetName()))
		{
			PrimitiveComponent->SetHiddenInGame(true);
			PrimitiveComponent->SetVisibility(false, true);
		}
	}
}
}

ABHJumpscareMonster::ABHJumpscareMonster()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	// A jumpscare is only seen up close, so far clients never need its fast movement updates.
	// Cull beyond ~60 m and cap the movement cadence so a full class does not pay for every
	// monster moving every tick to every student. Nearby clients still get smooth motion.
	SetNetCullDistanceSquared(6000.0f * 6000.0f);
	SetNetUpdateFrequency(30.0f);

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

	VariantVisualActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("VariantVisualActor"));
	VariantVisualActor->SetupAttachment(VisualRoot);
	VariantVisualActor->SetMobility(EComponentMobility::Movable);

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
	bContactJumpscareTriggered = false;
	bUseScriptedPath = false;
	bPeekMode = false;
	bChargeDescend = false;
	bPlayChargeEffects = true;
	bScriptedFaceLookAtTarget = false;
	bScriptedPlayChargeEffects = true;
	ScriptedPathIndex = 1;
	bUsingImportedVisual = false;
	bCloseupPresentation = false;
	CloseupStartTime = -1.0f;
	CameraFocusHeight = 145.0f;
	PresentationLightColor = FLinearColor(1.0f, 0.02f, 0.0f, 1.0f);
	PresentationVisualOffset = FVector(-20.0f, 0.0f, -88.0f);
	PresentationVisualRotation = FRotator(0.0f, -90.0f, 0.0f);
	PresentationVisualScale = FVector::OneVector;
	BaseVisualRootScale = FVector(BHJumpscareProxyVisualRootScale);
	JumpscareVariantId = TEXT("ProxyRed");
	LaunchSound = nullptr;
	SetActorScale3D(FVector::OneVector);

	UseProxyFallbackVisual();

	ApplyVisualRootScale();
}

void ABHJumpscareMonster::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHJumpscareMonster, JumpscareVariantId);
}

float ABHJumpscareMonster::ComputeLocalReducedFlashScale() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1.0f;
	}

	if (const ABHPlayerController* LocalBHPC = Cast<ABHPlayerController>(World->GetFirstPlayerController()))
	{
		// Mirror the 0.25x factor used for horror-cue and flicker-burst flash.
		return LocalBHPC->IsReducedFlashEnabled() ? 0.25f : 1.0f;
	}

	return 1.0f;
}

bool ABHJumpscareMonster::LocalViewerWantsProxyJumpscare() const
{
	// True when the local viewer has Reduced Jumpscares on, so this monster should render as the gentle
	// procedural proxy instead of an imported photoreal creature. Evaluated against the local player only,
	// so it is correct per-client even when several players share the same replicated monster actor.
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (const ABHPlayerController* LocalBHPC = Cast<ABHPlayerController>(World->GetFirstPlayerController()))
	{
		return LocalBHPC->IsReducedJumpscaresEnabled();
	}
	return false;
}

void ABHJumpscareMonster::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	CachedReducedFlashScale = ComputeLocalReducedFlashScale();
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
		const bool bCanStartChargeEffects = bUseScriptedPath ? bScriptedPlayChargeEffects : bPlayChargeEffects;
		if (!bHolding && !bChargeStarted && bCanStartChargeEffects)
		{
			StartChargeEffects();
		}

		if (VisualRoot)
		{
			if (bCloseupPresentation)
			{
				// Close-up "lunge": the entity slams toward the camera (+X is forward, toward the view)
				// from a slightly pulled-back, smaller pose, then the face twitches as it settles.
				constexpr float LungeDuration = 0.14f;
				const float CloseAge = CloseupStartTime >= 0.0f ? FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - CloseupStartTime) : LungeDuration;
				const float LungeAlpha = FMath::Clamp(CloseAge / LungeDuration, 0.0f, 1.0f);
				const float Eased = 1.0f - FMath::Square(1.0f - LungeAlpha); // ease-out: fast in, settle
				const FVector StartOffset(-72.0f, 0.0f, -26.0f);
				const FVector LungeLoc = FMath::Lerp(StartOffset, FVector::ZeroVector, Eased);

				// Head twitch decays over ~0.6s after the lunge lands.
				const float TwitchDecay = 1.0f - FMath::Clamp((CloseAge - LungeDuration) / 0.6f, 0.0f, 1.0f);
				const float TwitchYaw = FMath::Sin(CloseAge * 46.0f) * 3.4f * TwitchDecay;
				const float TwitchPitch = FMath::Sin(CloseAge * 61.0f + 0.7f) * 2.6f * TwitchDecay;
				VisualRoot->SetRelativeLocation(LungeLoc);
				VisualRoot->SetRelativeRotation(FRotator(TwitchPitch, TwitchYaw, TwitchYaw * 0.4f));
				VisualRoot->SetRelativeScale3D(BaseVisualRootScale * FMath::Lerp(0.78f, 1.0f, Eased));
			}
			else
			{
				const float RunAlpha = (bHolding || bPeekMode) ? 0.0f : 1.0f;
				const float Bob = FMath::Sin(Age * 42.0f) * 8.0f * RunAlpha;
				const float Roll = FMath::Sin(Age * 38.0f) * 4.5f * RunAlpha;
				VisualRoot->SetRelativeLocation(FVector(0.0f, Bob * 0.35f, FMath::Abs(Bob) * 0.35f));
				VisualRoot->SetRelativeRotation(FRotator(-11.0f * RunAlpha, 0.0f, Roll));
				ApplyVisualRootScale();
			}
		}
		// Mute the pulse oscillation toward its steady midpoint for a local Reduced-flash player.
		// The base glow (and the sustained hold boost) stays, so the monster is still clearly
		// visible; only the rapid strobe amplitude is reduced.
		const float SteadyPulse = 0.825f;
		const float EffectivePulse = SteadyPulse + (Pulse - SteadyPulse) * CachedReducedFlashScale;
		if (EyeLight)
		{
			EyeLight->SetIntensity((15000.0f + 11000.0f * EffectivePulse) * (bHolding ? 1.25f : 1.0f));
		}
		if (CoreLight)
		{
			CoreLight->SetIntensity((6500.0f + 7000.0f * EffectivePulse) * (bHolding ? 1.2f : 1.0f));
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	if (bContactJumpscareTriggered)
	{
		return;
	}

	if (bPeekMode)
	{
		if (!Target.IsValid() || !GetWorld())
		{
			Destroy();
			return;
		}

		const float PeekNow = GetWorld()->GetTimeSeconds();
		if (SpawnTime >= 0.0f && PeekNow - SpawnTime > MaxLifetime)
		{
			Destroy();
			return;
		}

		// Keep facing the player while peeking out from the wall edge.
		FVector PeekFaceDir = Target->GetActorLocation() - GetActorLocation();
		PeekFaceDir.Z = 0.0f;
		if (!PeekFaceDir.IsNearlyZero())
		{
			SetActorRotation(PeekFaceDir.Rotation());
		}

		// "Caught looking": once the player turns their view onto the peeker it ducks back out of sight.
		// A short grace (>=0.4s alive) guarantees it is seen for a beat before it can retreat.
		if (SpawnTime >= 0.0f && PeekNow - SpawnTime >= 0.4f)
		{
			if (const AController* TargetController = Target->GetController())
			{
				const FVector ViewForward = TargetController->GetControlRotation().Vector();
				const FVector EyeLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
				const FVector DirToPeeker = (GetActorLocation() + FVector(0.0f, 0.0f, 60.0f) - EyeLocation).GetSafeNormal();
				if (!DirToPeeker.IsNearlyZero() && FVector::DotProduct(ViewForward, DirToPeeker) > 0.96f)
				{
					Destroy();
					return;
				}
			}
		}
		return;
	}

	if (bUseScriptedPath)
	{
		TickScriptedPath(DeltaSeconds);
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
	if (!bChargeDescend)
	{
		// Default homing charge stays on the horizontal plane; the ceiling-drop approach keeps
		// the vertical component so the entity descends onto the target.
		Delta.Z = 0.0f;
	}
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

	constexpr float ContactDistance = 190.0f;
	const float DistanceToTarget = Delta.Size();
	if (DistanceToTarget <= ContactDistance)
	{
		TriggerContactJumpscare();
		return;
	}

	const float ChargeAge = SpawnTime >= 0.0f ? FMath::Max(0.0f, Now - SpawnTime - HoldSeconds) : MaxLifetime;
	const float LaunchMultiplier = ChargeAge < 0.35f ? FMath::Lerp(1.35f, 1.0f, ChargeAge / 0.35f) : 1.0f;
	const float MoveDistance = ChargeSpeed * LaunchMultiplier * DeltaSeconds;
	if (DistanceToTarget - MoveDistance <= ContactDistance)
	{
		SetActorLocation(GetActorLocation() + Direction * FMath::Max(0.0f, DistanceToTarget - ContactDistance));
		TriggerContactJumpscare();
		return;
	}

	SetActorLocation(GetActorLocation() + Direction * MoveDistance);
}

void ABHJumpscareMonster::TickScriptedPath(float DeltaSeconds)
{
	if (!GetWorld() || ScriptedPathPoints.Num() < 2 || ScriptedPathIndex >= ScriptedPathPoints.Num())
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

	FVector CurrentLocation = GetActorLocation();
	FVector GoalLocation = ScriptedPathPoints[ScriptedPathIndex];
	FVector Delta = GoalLocation - CurrentLocation;
	Delta.Z = 0.0f;

	while (Delta.SizeSquared2D() <= FMath::Square(36.0f))
	{
		++ScriptedPathIndex;
		if (ScriptedPathIndex >= ScriptedPathPoints.Num())
		{
			Destroy();
			return;
		}

		GoalLocation = ScriptedPathPoints[ScriptedPathIndex];
		Delta = GoalLocation - CurrentLocation;
		Delta.Z = 0.0f;
	}

	const FVector Direction = Delta.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	if (bScriptedFaceLookAtTarget && ScriptedLookAtTarget.IsValid())
	{
		FVector LookDelta = ScriptedLookAtTarget->GetActorLocation() + FVector(0.0f, 0.0f, 95.0f) - CurrentLocation;
		LookDelta.Z = 0.0f;
		const FVector LookDirection = LookDelta.GetSafeNormal2D();
		if (!LookDirection.IsNearlyZero())
		{
			SetActorRotation(LookDirection.Rotation());
		}
	}
	else
	{
		SetActorRotation(Direction.Rotation());
	}
	const float MoveDistance = FMath::Max(120.0f, ChargeSpeed) * DeltaSeconds;
	const float Distance = Delta.Size2D();
	const FVector NewLocation = CurrentLocation + Direction * FMath::Min(MoveDistance, Distance);
	SetActorLocation(FVector(NewLocation.X, NewLocation.Y, GoalLocation.Z));
}

void ABHJumpscareMonster::StartChargeEffects()
{
	bChargeStarted = true;

	if (LaunchSound && GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation(), 1.0f, 1.0f, 0.0f);
	}

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

void ABHJumpscareMonster::SetCloseupUpperBodyOnly()
{
	if (LeftLeg)
	{
		LeftLeg->SetHiddenInGame(true);
		LeftLeg->SetVisibility(false, true);
	}
	if (RightLeg)
	{
		RightLeg->SetHiddenInGame(true);
		RightLeg->SetVisibility(false, true);
	}

	BHHideLowerBodyForCloseup(this);
	if (VariantVisualActor && VariantVisualActor->GetChildActor())
	{
		BHHideLowerBodyForCloseup(VariantVisualActor->GetChildActor());
	}
}

void ABHJumpscareMonster::TriggerContactJumpscare()
{
	if (bContactJumpscareTriggered || !Target.IsValid() || !GetWorld())
	{
		return;
	}

	bContactJumpscareTriggered = true;
	ABHCharacter* TargetCharacter = Target.Get();
	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(TargetCharacter ? TargetCharacter->GetController() : nullptr);

	FVector ViewLocation = TargetCharacter ? TargetCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 110.0f) : GetActorLocation();
	FRotator ViewRotation = TargetCharacter ? TargetCharacter->GetActorRotation() : GetActorRotation();
	if (TargetPC)
	{
		TargetPC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	FVector Forward = ViewRotation.Vector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = TargetCharacter ? TargetCharacter->GetActorForwardVector().GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D();
	}
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector CloseLocation = ViewLocation + Forward * 88.0f - FVector::UpVector * 135.0f;
	SetActorLocation(CloseLocation);
	SetActorRotation((ViewLocation - CloseLocation).Rotation());
	bCloseupPresentation = true;
	// Show the full towering creature on the contact slam (and thus the super-chain finale) so it never reads
	// as a stumpy legless torso. The behind-you payoff is a separate path and keeps its tight upper-body framing.
	bCloseupUpperBodyOnly = false;
	CloseupStartTime = GetWorld()->GetTimeSeconds();
	if (!ActiveVariant.VariantId.IsNone() || !JumpscareVariantId.IsNone())
	{
		ApplyConfiguredVariant();
	}
	else
	{
		UseProxyFallbackVisual();
	}
	SetActorScale3D(FVector::OneVector);
	ForceNetUpdate();

	// Randomize timing slightly so repeated scares never feel mechanically identical.
	const float CloseDuration = FMath::FRandRange(1.30f, 1.60f);
	const float CloseLock = FMath::FRandRange(0.86f, 1.04f);
	SetLifeSpan(CloseDuration);

	if (TargetPC)
	{
		const FLinearColor ImpactColor = ActiveVariant.LightColor.A > 0.0f ? ActiveVariant.LightColor : PresentationLightColor;
		const FVector FocusLocation = BHResolveJumpscareCloseFocusLocation(ViewLocation, Forward, ActiveVariant);
		TargetPC->ClientSnapViewToFlatFocus(FocusLocation);

		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = FocusLocation;
		Cue.DurationSeconds = CloseDuration;
		Cue.LockSeconds = CloseLock;
		Cue.ShakeIntensity = FMath::Clamp(FMath::Max(ActiveVariant.CameraShakeIntensity, 0.96f), 0.0f, 1.0f);
		Cue.CameraJitterDuration = FMath::Clamp(FMath::Max(ActiveVariant.CameraJitterDuration, 1.25f), 0.0f, 3.0f);
		Cue.CameraJitterFrequency = 52.0f;
		Cue.FlashIntensity = FMath::Clamp(FMath::Max(ActiveVariant.FlashIntensity, 0.92f), 0.0f, 1.0f);
		Cue.FlashColor = ImpactColor;
		Cue.AudioAsset = ActiveVariant.LaunchSound;
		Cue.AudioVolume = 1.35f;
		Cue.VisualActorClass = ActiveVariant.VisualActorClass;
		Cue.CloseVisualOffset = ActiveVariant.CloseVisualOffset;
		Cue.CloseVisualRotation = ActiveVariant.CloseVisualRotation;
		Cue.CloseVisualScale = ActiveVariant.CloseVisualScale;
		Cue.VariantId = ActiveVariant.VariantId;
		Cue.bSnapToFocus = true;
		Cue.bLockInput = true;
		Cue.bCloseRangeFocus = true;
		Cue.bUpperBodyCloseVisual = false;  // full-body slam: show the complete, towering creature
		Cue.FOVPunch = FMath::Clamp(ActiveVariant.ImpactFOVPunch, 0.0f, 30.0f);
		Cue.HitStopSeconds = FMath::Clamp(ActiveVariant.ImpactHitStopSeconds, 0.0f, 0.25f);
		Cue.RumbleIntensity = FMath::Clamp(ActiveVariant.ImpactRumbleIntensity, 0.0f, 1.0f);
		Cue.ImpactStinger = ActiveVariant.ImpactStinger;
		Cue.bLayeredImpactAudio = true;
		TargetPC->ClientPlayHorrorCue(Cue);
	}
}

void ABHJumpscareMonster::Configure(ABHCharacter* NewTarget, float NewSpeed, float NewLifetime, float NewHoldSeconds, bool bNewPlayChargeEffects)
{
	Target = NewTarget;
	ChargeSpeed = FMath::Max(400.0f, NewSpeed);
	HoldSeconds = FMath::Clamp(NewHoldSeconds, 0.0f, 5.0f);
	MaxLifetime = FMath::Clamp(NewLifetime, FMath::Max(1.0f, HoldSeconds + 0.8f), 12.0f);
	bContactJumpscareTriggered = false;
	bUseScriptedPath = false;
	bPlayChargeEffects = bNewPlayChargeEffects;
	bScriptedFaceLookAtTarget = false;
	bScriptedPlayChargeEffects = true;
	bCloseupPresentation = false;
	ScriptedLookAtTarget.Reset();
	ScriptedPathPoints.Reset();
	ScriptedPathIndex = 1;
	SetActorScale3D(FVector::OneVector);
	SetLifeSpan(MaxLifetime + 0.35f);
}

void ABHJumpscareMonster::ConfigureScriptedPath(const TArray<FVector>& NewPathPoints, float NewSpeed, float NewLifetime, AActor* NewLookAtTarget, bool bNewFaceLookAtTarget, bool bNewPlayChargeEffects)
{
	Target.Reset();
	ScriptedPathPoints = NewPathPoints;
	ScriptedPathIndex = 1;
	ScriptedLookAtTarget = NewLookAtTarget;
	ChargeSpeed = FMath::Max(120.0f, NewSpeed);
	HoldSeconds = 0.0f;
	MaxLifetime = FMath::Clamp(NewLifetime, 0.4f, 8.0f);
	bContactJumpscareTriggered = false;
	bUseScriptedPath = ScriptedPathPoints.Num() >= 2;
	bPlayChargeEffects = true;
	bScriptedFaceLookAtTarget = bUseScriptedPath && bNewFaceLookAtTarget;
	bScriptedPlayChargeEffects = bNewPlayChargeEffects;
	bCloseupPresentation = false;
	SetActorScale3D(FVector::OneVector);
	if (bUseScriptedPath)
	{
		SetActorLocation(ScriptedPathPoints[0]);
		const FVector InitialDelta = (ScriptedPathPoints[1] - ScriptedPathPoints[0]).GetSafeNormal2D();
		if (bScriptedFaceLookAtTarget && ScriptedLookAtTarget.IsValid())
		{
			FVector LookDelta = ScriptedLookAtTarget->GetActorLocation() + FVector(0.0f, 0.0f, 95.0f) - ScriptedPathPoints[0];
			LookDelta.Z = 0.0f;
			const FVector LookDirection = LookDelta.GetSafeNormal2D();
			if (!LookDirection.IsNearlyZero())
			{
				SetActorRotation(LookDirection.Rotation());
			}
		}
		else if (!InitialDelta.IsNearlyZero())
		{
			SetActorRotation(InitialDelta.Rotation());
		}
	}
	SetLifeSpan(MaxLifetime + 0.35f);
}

void ABHJumpscareMonster::ConfigurePresentation(USkeletalMesh* NewSkeletalMesh, UAnimSequence* NewRunAnimation, UStaticMesh* NewStaticMesh, UMaterialInterface* NewMaterial, USoundBase* NewLaunchSound, const FLinearColor& NewLightColor, float NewFocusHeight)
{
	LaunchSound = NewLaunchSound;
	PresentationLightColor = NewLightColor;
	CameraFocusHeight = FMath::Clamp(NewFocusHeight, 80.0f, 260.0f);
	BaseVisualRootScale = FVector::OneVector;

	if (NewSkeletalMesh && SkeletalMonsterMesh)
	{
		SkeletalMonsterMesh->SetSkeletalMesh(NewSkeletalMesh);
		if (NewRunAnimation)
		{
			SkeletalMonsterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			SkeletalMonsterMesh->SetAnimation(NewRunAnimation);
			SkeletalMonsterMesh->Play(true);
		}
		if (NewMaterial)
		{
			SkeletalMonsterMesh->SetMaterial(0, NewMaterial);
		}
		SkeletalMonsterMesh->SetRelativeScale3D(FitVisualScaleToTargetHeight(GetEffectiveVariantVisualScale(), BHMeshVisualHeight(NewSkeletalMesh)));
		SkeletalMonsterMesh->SetHiddenInGame(false);
		SkeletalMonsterMesh->SetVisibility(true, true);
		if (MonsterMesh)
		{
			MonsterMesh->SetHiddenInGame(true);
			MonsterMesh->SetVisibility(false, true);
		}
		bUsingImportedVisual = true;
		SetProxyPartsVisible(false);
	}
	else if (NewStaticMesh && MonsterMesh)
	{
		MonsterMesh->SetStaticMesh(NewStaticMesh);
		if (NewMaterial)
		{
			MonsterMesh->SetMaterial(0, NewMaterial);
		}
		MonsterMesh->SetRelativeScale3D(FitVisualScaleToTargetHeight(GetEffectiveVariantVisualScale(), BHMeshVisualHeight(NewStaticMesh)));
		MonsterMesh->SetHiddenInGame(false);
		MonsterMesh->SetVisibility(true, true);
		if (SkeletalMonsterMesh)
		{
			SkeletalMonsterMesh->SetHiddenInGame(true);
			SkeletalMonsterMesh->SetVisibility(false, true);
		}
		bUsingImportedVisual = true;
		SetProxyPartsVisible(false);
	}

	ApplyConfiguredPresentation();
}

void ABHJumpscareMonster::ConfigureVariant(const FBHJumpscareVariant& NewVariant)
{
	ActiveVariant = NewVariant;
	if (!NewVariant.VariantId.IsNone())
	{
		JumpscareVariantId = NewVariant.VariantId;
	}
	ApplyConfiguredVariant();
}

void ABHJumpscareMonster::ConfigureCloseupPresentation(const FBHJumpscareVariant& NewVariant, float NewLifetime, bool bUpperBodyOnly)
{
	Target.Reset();
	ScriptedLookAtTarget.Reset();
	ScriptedPathPoints.Reset();
	bUseScriptedPath = false;
	bPlayChargeEffects = false;
	bScriptedFaceLookAtTarget = false;
	bScriptedPlayChargeEffects = false;
	bContactJumpscareTriggered = true;
	bCloseupPresentation = true;
	bCloseupUpperBodyOnly = bUpperBodyOnly;
	CloseupStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	MaxLifetime = FMath::Clamp(NewLifetime, 0.2f, 5.0f);
	HoldSeconds = 0.0f;

	ConfigureVariant(NewVariant);
	SetActorEnableCollision(false);
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent)
		{
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComponent->SetGenerateOverlapEvents(false);
		}
	}

	SetActorScale3D(FVector::OneVector);
	ApplyVisualRootScale();
	if (bUpperBodyOnly)
	{
		SetCloseupUpperBodyOnly();
	}
	SetLifeSpan(MaxLifetime);
	ForceNetUpdate();
}

void ABHJumpscareMonster::ConfigurePeek(ABHCharacter* NewTarget, const FBHJumpscareVariant& NewVariant, float LingerSeconds)
{
	Target = NewTarget;
	ChargeSpeed = 0.0f;
	HoldSeconds = 0.0f;
	MaxLifetime = FMath::Clamp(LingerSeconds, 0.6f, 8.0f);
	bContactJumpscareTriggered = false;
	bUseScriptedPath = false;
	bPeekMode = true;
	bPlayChargeEffects = false; // passive: no charge scream or launch VFX
	bScriptedFaceLookAtTarget = false;
	bScriptedPlayChargeEffects = false;
	bCloseupPresentation = false;
	ScriptedLookAtTarget.Reset();
	ScriptedPathPoints.Reset();
	SetActorScale3D(FVector::OneVector);

	ConfigureVariant(NewVariant);

	// Passive prop: never block, trap, or overlap the player.
	SetActorEnableCollision(false);
	TArray<UPrimitiveComponent*> PeekPrimitives;
	GetComponents<UPrimitiveComponent>(PeekPrimitives);
	for (UPrimitiveComponent* PrimitiveComponent : PeekPrimitives)
	{
		if (PrimitiveComponent)
		{
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComponent->SetGenerateOverlapEvents(false);
		}
	}

	// Face the target so the figure is looking right at the player as it peeks out.
	if (Target.IsValid())
	{
		FVector FaceDir = Target->GetActorLocation() - GetActorLocation();
		FaceDir.Z = 0.0f;
		if (!FaceDir.IsNearlyZero())
		{
			SetActorRotation(FaceDir.Rotation());
		}
	}

	SetLifeSpan(MaxLifetime + 0.35f);
	ForceNetUpdate();
}

void ABHJumpscareMonster::OnRep_JumpscareVariantId()
{
	if (FindJumpscareVariantById(JumpscareVariantId, ActiveVariant))
	{
		ApplyConfiguredVariant();
	}
}

FVector ABHJumpscareMonster::GetEffectiveVariantVisualScale() const
{
	FVector Scale = BHSanitizeJumpscareScale(PresentationVisualScale);
	if (bCloseupPresentation)
	{
		const FVector CloseScale = ActiveVariant.CloseVisualScale.GetAbs().IsNearlyZero()
			? FVector(1.0f)
			: BHSanitizeJumpscareScale(ActiveVariant.CloseVisualScale);
		Scale *= CloseScale;
	}
	return BHSanitizeJumpscareScale(Scale);
}

FVector ABHJumpscareMonster::FitVisualScaleToTargetHeight(const FVector& RequestedScale, float UnscaledHeight) const
{
	FVector Scale = BHSanitizeJumpscareScale(RequestedScale);
	const float RequestedHeight = UnscaledHeight * FMath::Max(Scale.Z, BHJumpscareMinVisualScaleAxis);
	if (UnscaledHeight <= KINDA_SMALL_NUMBER || RequestedHeight <= KINDA_SMALL_NUMBER)
	{
		return Scale;
	}

	const float CloseTargetHeight = bCloseupUpperBodyOnly ? BHJumpscareCloseTargetVisualHeight : BHJumpscareCloseFullBodyTargetVisualHeight;
	const float TargetHeight = bCloseupPresentation ? CloseTargetHeight : BHJumpscareWorldTargetVisualHeight;
	// Shrink-only in every case: a mesh taller than the target is scaled down to fit, but a shorter one is left
	// at its authored size. Upscaling world monsters toward a "looming" target broke the super-jumpscare chain
	// (the inflated, foot-offset meshes sank/clipped at the tight peek/cross/corner spawn spots and vanished),
	// so we never grow them now. A mesh imported at the wrong unit scale must fix that via its variant scale.
	const float Fit = FMath::Min(TargetHeight / RequestedHeight, 1.0f);
	Scale *= Fit;
	return BHSanitizeJumpscareScale(Scale);
}

void ABHJumpscareMonster::ApplyVisualRootScale()
{
	if (VisualRoot)
	{
		VisualRoot->SetRelativeScale3D(BaseVisualRootScale);
	}
}

void ABHJumpscareMonster::UseProxyFallbackVisual()
{
	const FVector CloseScale = bCloseupPresentation && !ActiveVariant.CloseVisualScale.GetAbs().IsNearlyZero()
		? BHSanitizeJumpscareScale(ActiveVariant.CloseVisualScale)
		: FVector::OneVector;
	const float CloseMultiplier = bCloseupPresentation ? FMath::Clamp(CloseScale.GetMin(), 0.90f, 1.08f) : 1.0f;
	BaseVisualRootScale = FVector((bCloseupPresentation ? BHJumpscareProxyCloseVisualRootScale : BHJumpscareProxyVisualRootScale) * CloseMultiplier);
	SetProxyPartsVisible(true);

	if (MonsterMesh)
	{
		MonsterMesh->SetHiddenInGame(true);
		MonsterMesh->SetVisibility(false, true);
	}
	if (SkeletalMonsterMesh)
	{
		SkeletalMonsterMesh->SetHiddenInGame(true);
		SkeletalMonsterMesh->SetVisibility(false, true);
	}
	if (VariantVisualActor)
	{
		VariantVisualActor->DestroyChildActor();
	}

	bUsingImportedVisual = false;
	ApplyVisualRootScale();
}

void ABHJumpscareMonster::SetProxyPartsVisible(bool bVisible)
{
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
			Part->SetHiddenInGame(!bVisible);
			Part->SetVisibility(bVisible, true);
		}
	}
}

void ABHJumpscareMonster::ApplyConfiguredVariant()
{
	if (ActiveVariant.VariantId.IsNone() && !FindJumpscareVariantById(JumpscareVariantId, ActiveVariant))
	{
		return;
	}

	// Dedicated server has no local viewer: skip the synchronous skeletal-mesh / anim / material loads
	// and the rest of the purely-cosmetic presentation setup below. (The shipped classroom host is a
	// listen-server, NM_ListenServer, so it still renders the creature for the host player.)
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	PresentationLightColor = ActiveVariant.LightColor;
	CameraFocusHeight = FMath::Clamp(ActiveVariant.FocusHeight, 80.0f, 320.0f);
	PresentationVisualOffset = ActiveVariant.VisualOffset;
	PresentationVisualRotation = ActiveVariant.VisualRotation;
	PresentationVisualScale = BHSanitizeJumpscareScale(ActiveVariant.VisualScale);
	if (PresentationVisualScale.IsNearlyZero())
	{
		PresentationVisualScale = FVector::OneVector;
	}

	USoundBase* ResolvedLaunchSound = !ActiveVariant.LaunchSound.IsNull() && BHSoftObjectPathExists(ActiveVariant.LaunchSound.ToSoftObjectPath())
		? ActiveVariant.LaunchSound.LoadSynchronous()
		: nullptr;
	if (ResolvedLaunchSound)
	{
		LaunchSound = ResolvedLaunchSound;
	}

	// Comfort / "safe mode": a viewer with Reduced Jumpscares enabled sees the abstract procedural proxy
	// instead of the realistic creature. ApplyConfiguredVariant runs per-client (via OnRep_JumpscareVariantId),
	// so each player independently gets the proxy or the real monster based on their OWN setting — the scary
	// audio/flash are still toned down elsewhere, but the menacing photoreal mesh is never shown to them.
	if (LocalViewerWantsProxyJumpscare())
	{
		// Mirror the normal proxy-fallback path (UseProxyFallbackVisual + ApplyConfiguredPresentation) so the
		// proxy still gets its light/eye presentation; just skip ever loading the realistic creature.
		UseProxyFallbackVisual();
		ApplyConfiguredPresentation();
		return;
	}

	UMaterialInterface* ResolvedMaterial = !ActiveVariant.Material.IsNull() && BHSoftObjectPathExists(ActiveVariant.Material.ToSoftObjectPath())
		? ActiveVariant.Material.LoadSynchronous()
		: nullptr;
	bool bAppliedImportedVisual = false;
	bool bAppliedSkeletalVisual = false;
	bool bAppliedStaticVisual = false;

	if (!ActiveVariant.SkeletalMesh.IsNull() && BHSoftObjectPathExists(ActiveVariant.SkeletalMesh.ToSoftObjectPath()))
	{
		if (USkeletalMesh* ResolvedSkeletalMesh = ActiveVariant.SkeletalMesh.LoadSynchronous())
		{
			if (SkeletalMonsterMesh)
			{
				SkeletalMonsterMesh->SetSkeletalMesh(ResolvedSkeletalMesh);
				// Tight presentations (the close-up slam, the behind-you payoff, corner peeks) play the shared
				// lunge-at-camera clip so every creature faces the player aggressively instead of holding a
				// neutral idle/walk pose that reads as turned the wrong way. The charge across the room keeps
				// its own locomotion clip.
				const bool bWantsCloseUpPose = (bCloseupPresentation || bPeekMode)
					&& !ActiveVariant.CloseUpAnimation.IsNull()
					&& BHSoftObjectPathExists(ActiveVariant.CloseUpAnimation.ToSoftObjectPath());
				const TSoftObjectPtr<UAnimSequence>& AnimationToPlay = bWantsCloseUpPose ? ActiveVariant.CloseUpAnimation : ActiveVariant.RunAnimation;
				if (!AnimationToPlay.IsNull() && BHSoftObjectPathExists(AnimationToPlay.ToSoftObjectPath()))
				{
					if (UAnimSequence* ResolvedAnimation = AnimationToPlay.LoadSynchronous())
					{
						SkeletalMonsterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						SkeletalMonsterMesh->SetAnimation(ResolvedAnimation);
						// The lunge is a one-shot that holds its final, scariest frame; locomotion loops.
						SkeletalMonsterMesh->Play(!bWantsCloseUpPose);
					}
				}
				if (ResolvedMaterial)
				{
					SkeletalMonsterMesh->SetMaterial(0, ResolvedMaterial);
				}
				SkeletalMonsterMesh->SetRelativeLocation(PresentationVisualOffset);
				SkeletalMonsterMesh->SetRelativeRotation(PresentationVisualRotation);
				SkeletalMonsterMesh->SetRelativeScale3D(FitVisualScaleToTargetHeight(GetEffectiveVariantVisualScale(), BHMeshVisualHeight(ResolvedSkeletalMesh)));
				SkeletalMonsterMesh->SetHiddenInGame(false);
				SkeletalMonsterMesh->SetVisibility(true, true);
				BaseVisualRootScale = FVector::OneVector;
				bAppliedImportedVisual = true;
				bAppliedSkeletalVisual = true;
			}
		}
	}

	if (!bAppliedImportedVisual && !ActiveVariant.StaticMesh.IsNull() && BHSoftObjectPathExists(ActiveVariant.StaticMesh.ToSoftObjectPath()))
	{
		if (UStaticMesh* ResolvedStaticMesh = ActiveVariant.StaticMesh.LoadSynchronous())
		{
			if (MonsterMesh)
			{
				MonsterMesh->SetStaticMesh(ResolvedStaticMesh);
				if (ResolvedMaterial)
				{
					MonsterMesh->SetMaterial(0, ResolvedMaterial);
				}
				MonsterMesh->SetRelativeLocation(PresentationVisualOffset);
				MonsterMesh->SetRelativeRotation(PresentationVisualRotation);
				MonsterMesh->SetRelativeScale3D(FitVisualScaleToTargetHeight(GetEffectiveVariantVisualScale(), BHMeshVisualHeight(ResolvedStaticMesh)));
				MonsterMesh->SetHiddenInGame(false);
				MonsterMesh->SetVisibility(true, true);
				BaseVisualRootScale = FVector::OneVector;
				bAppliedImportedVisual = true;
				bAppliedStaticVisual = true;
			}
		}
	}

	bool bAppliedVisualActor = false;
	if (VariantVisualActor)
	{
		VariantVisualActor->SetRelativeLocation(PresentationVisualOffset);
		VariantVisualActor->SetRelativeRotation(PresentationVisualRotation);
		VariantVisualActor->SetRelativeScale3D(GetEffectiveVariantVisualScale());
		VariantVisualActor->DestroyChildActor();
		if (!ActiveVariant.VisualActorClass.IsNull() && BHSoftObjectPathExists(ActiveVariant.VisualActorClass.ToSoftObjectPath()))
		{
			if (UClass* VisualClass = ActiveVariant.VisualActorClass.LoadSynchronous())
			{
				VariantVisualActor->SetChildActorClass(VisualClass);
				if (AActor* ChildActor = VariantVisualActor->GetChildActor())
				{
					ChildActor->SetActorEnableCollision(false);
					TArray<UPrimitiveComponent*> PrimitiveComponents;
					ChildActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
					for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
					{
						if (PrimitiveComponent)
						{
							PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}
					}

					const FBox Bounds = ChildActor->GetComponentsBoundingBox(true);
					const float Height = Bounds.IsValid ? Bounds.GetSize().Z : 0.0f;
					// Mirror the skeletal/static fit: a full-body close-up fits to the taller full-body target so the
					// whole creature (legs included) fills the frame; only the opt-in upper-body framing crops to the
					// short torso target.
					const float CloseTargetHeight = bCloseupUpperBodyOnly ? BHJumpscareCloseTargetVisualHeight : BHJumpscareCloseFullBodyTargetVisualHeight;
					const float TargetHeight = bCloseupPresentation ? CloseTargetHeight : BHJumpscareWorldTargetVisualHeight;
					if (Height > TargetHeight && TargetHeight > 0.0f)
					{
						VariantVisualActor->SetRelativeScale3D(VariantVisualActor->GetRelativeScale3D() * (TargetHeight / Height));
					}
				}
				BaseVisualRootScale = FVector::OneVector;
				bAppliedVisualActor = true;
			}
		}
	}

	if (bAppliedImportedVisual || bAppliedVisualActor)
	{
		SetProxyPartsVisible(false);
		if (MonsterMesh)
		{
			const bool bShowStatic = bAppliedStaticVisual && !bAppliedVisualActor;
			MonsterMesh->SetHiddenInGame(!bShowStatic);
			MonsterMesh->SetVisibility(bShowStatic, true);
		}
		if (SkeletalMonsterMesh)
		{
			const bool bShowSkeletal = bAppliedSkeletalVisual && !bAppliedVisualActor;
			SkeletalMonsterMesh->SetHiddenInGame(!bShowSkeletal);
			SkeletalMonsterMesh->SetVisibility(bShowSkeletal, true);
		}
	}
	else
	{
		UseProxyFallbackVisual();
	}

	ApplyConfiguredPresentation();
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
	ApplyConfiguredPresentation();
}

void ABHJumpscareMonster::ApplyConfiguredPresentation()
{
	if (EyeLight)
	{
		EyeLight->SetLightColor(PresentationLightColor);
	}
	if (CoreLight)
	{
		CoreLight->SetLightColor(PresentationLightColor);
	}
}
