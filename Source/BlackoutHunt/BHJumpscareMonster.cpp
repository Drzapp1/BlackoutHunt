#include "BHJumpscareMonster.h"
#include "BHAmbientEmitter.h"
#include "BHCharacter.h"
#include "BHGameSettings.h"
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
constexpr float BHJumpscareMinVisualScaleAxis = 0.05f;
constexpr float BHJumpscareMaxVisualScaleAxis = 2.0f;

bool BHVariantIdIsScp096(FName VariantId)
{
	return VariantId.ToString().Equals(TEXT("SCP096"), ESearchCase::IgnoreCase);
}

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

FVector BHJumpscareCloseFaceFocusLocation(const FVector& ViewLocation, const FVector& ViewForward, const FBHJumpscareVariant& Variant)
{
	FVector Forward = ViewForward;
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const float FocusHeight = FMath::Clamp(Variant.FocusHeight, 80.0f, 320.0f);
	const FVector CloseOffset = Variant.CloseVisualOffset;
	const float HeightFactor = CloseOffset.Z < -80.0f ? 0.98f : 0.50f;
	const float FaceLift = FMath::Clamp(CloseOffset.Z + FocusHeight * HeightFactor, 22.0f, 54.0f);
	const float FaceDistance = FMath::Max(88.0f, CloseOffset.X + FMath::Clamp(FocusHeight * 0.08f, 8.0f, 18.0f));
	return ViewLocation + Forward * FaceDistance + FVector::UpVector * FaceLift;
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
	bScriptedFaceLookAtTarget = false;
	bScriptedPlayChargeEffects = true;
	ScriptedPathIndex = 1;
	bUsingScpVisual = false;
	bCloseupPresentation = false;
	CameraFocusHeight = 145.0f;
	PresentationLightColor = FLinearColor(1.0f, 0.02f, 0.0f, 1.0f);
	PresentationVisualOffset = FVector(-20.0f, 0.0f, -88.0f);
	PresentationVisualRotation = FRotator(0.0f, -90.0f, 0.0f);
	PresentationVisualScale = FVector::OneVector;
	BaseVisualRootScale = FVector(BHJumpscareProxyVisualRootScale);
	JumpscareVariantId = TEXT("SCP096");
	LaunchSound = nullptr;
	SetActorScale3D(FVector::OneVector);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ScpSkeletalMesh(TEXT("/Game/BlackoutHunt/Art/SCP096/Skeletal/SK_SCP096.SK_SCP096"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ScpRunAnim(TEXT("/Game/BlackoutHunt/Art/SCP096/Skeletal/A_SCP096_Run.A_SCP096_Run"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ScpMaterial(TEXT("/Game/BlackoutHunt/Art/SCP096/M_SCP096.M_SCP096"));
	if (ScpSkeletalMesh.Succeeded() && SkeletalMonsterMesh)
	{
		SkeletalMonsterMesh->SetSkeletalMesh(ScpSkeletalMesh.Object);
		if (ScpMaterial.Succeeded())
		{
			SkeletalMonsterMesh->SetMaterial(0, ScpMaterial.Object);
		}
		if (ScpRunAnim.Succeeded())
		{
			SkeletalMonsterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			SkeletalMonsterMesh->SetAnimation(ScpRunAnim.Object);
			SkeletalMonsterMesh->Play(true);
		}
		SkeletalMonsterMesh->SetRelativeLocation(FVector(-20.0f, 0.0f, -88.0f));
		SkeletalMonsterMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		SkeletalMonsterMesh->SetRelativeScale3D(FitVisualScaleToTargetHeight(FVector::OneVector, BHMeshVisualHeight(ScpSkeletalMesh.Object)));
		SkeletalMonsterMesh->SetHiddenInGame(false);
		SkeletalMonsterMesh->SetVisibility(true, true);
		BaseVisualRootScale = FVector::OneVector;
		bUsingScpVisual = true;
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ScpStaticMesh(TEXT("/Game/BlackoutHunt/Art/SCP096/SM_SCP096.SM_SCP096"));
		if (ScpStaticMesh.Succeeded() && MonsterMesh)
		{
			MonsterMesh->SetStaticMesh(ScpStaticMesh.Object);
			if (ScpMaterial.Succeeded())
			{
				MonsterMesh->SetMaterial(0, ScpMaterial.Object);
			}
			MonsterMesh->SetRelativeLocation(FVector(-20.0f, 0.0f, -88.0f));
			MonsterMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
			MonsterMesh->SetRelativeScale3D(FitVisualScaleToTargetHeight(FVector::OneVector, BHMeshVisualHeight(ScpStaticMesh.Object)));
			MonsterMesh->SetHiddenInGame(false);
			MonsterMesh->SetVisibility(true, true);
			BaseVisualRootScale = FVector::OneVector;
			bUsingScpVisual = true;
		}
	}

	if (bUsingScpVisual)
	{
		MonsterMesh->SetHiddenInGame(MonsterMesh->GetStaticMesh() == nullptr);
		MonsterMesh->SetVisibility(MonsterMesh->GetStaticMesh() != nullptr, true);
		SkeletalMonsterMesh->SetHiddenInGame(SkeletalMonsterMesh->GetSkeletalMeshAsset() == nullptr);
		SkeletalMonsterMesh->SetVisibility(SkeletalMonsterMesh->GetSkeletalMeshAsset() != nullptr, true);

		UStaticMeshComponent* ScpProxyParts[] = {
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
		for (UStaticMeshComponent* Part : ScpProxyParts)
		{
			if (Part)
			{
				Part->SetHiddenInGame(true);
				Part->SetVisibility(false, true);
			}
		}
	}
	else
	{
		UseProxyFallbackVisual();
	}

	ApplyVisualRootScale();
}

void ABHJumpscareMonster::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHJumpscareMonster, JumpscareVariantId);
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
		const bool bCanStartChargeEffects = !bUseScriptedPath || bScriptedPlayChargeEffects;
		if (!bHolding && !bChargeStarted && bCanStartChargeEffects)
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
			ApplyVisualRootScale();
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

	if (bContactJumpscareTriggered)
	{
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
	if (!ActiveVariant.VariantId.IsNone() || !JumpscareVariantId.IsNone())
	{
		ApplyConfiguredVariant();
	}
	else
	{
		UseProxyFallbackVisual();
	}
	SetActorScale3D(FVector::OneVector);
	SetCloseupUpperBodyOnly();
	ForceNetUpdate();
	SetLifeSpan(1.15f);

	if (TargetPC)
	{
		const FLinearColor ImpactColor = ActiveVariant.LightColor.A > 0.0f ? ActiveVariant.LightColor : PresentationLightColor;
		const FVector FocusLocation = BHJumpscareCloseFaceFocusLocation(ViewLocation, Forward, ActiveVariant);
		TargetPC->ClientSnapViewToFlatFocus(FocusLocation);

		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = FocusLocation;
		Cue.DurationSeconds = 1.35f;
		Cue.LockSeconds = 0.92f;
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
		Cue.bUpperBodyCloseVisual = true;
		TargetPC->ClientPlayHorrorCue(Cue);
	}
}

void ABHJumpscareMonster::Configure(ABHCharacter* NewTarget, float NewSpeed, float NewLifetime, float NewHoldSeconds)
{
	Target = NewTarget;
	ChargeSpeed = FMath::Max(400.0f, NewSpeed);
	HoldSeconds = FMath::Clamp(NewHoldSeconds, 0.0f, 5.0f);
	MaxLifetime = FMath::Clamp(NewLifetime, FMath::Max(1.0f, HoldSeconds + 0.8f), 12.0f);
	bContactJumpscareTriggered = false;
	bUseScriptedPath = false;
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
		bUsingScpVisual = true;
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
		bUsingScpVisual = true;
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
	bScriptedFaceLookAtTarget = false;
	bScriptedPlayChargeEffects = false;
	bContactJumpscareTriggered = true;
	bCloseupPresentation = true;
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
	const float TargetHeight = bCloseupPresentation ? BHJumpscareCloseTargetVisualHeight : BHJumpscareWorldTargetVisualHeight;
	const float RequestedHeight = UnscaledHeight * FMath::Max(Scale.Z, BHJumpscareMinVisualScaleAxis);
	if (UnscaledHeight > KINDA_SMALL_NUMBER && RequestedHeight > TargetHeight)
	{
		Scale *= TargetHeight / RequestedHeight;
	}
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

	bUsingScpVisual = BHVariantIdIsScp096(JumpscareVariantId) || BHVariantIdIsScp096(ActiveVariant.VariantId);
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
				if (!ActiveVariant.RunAnimation.IsNull() && BHSoftObjectPathExists(ActiveVariant.RunAnimation.ToSoftObjectPath()))
				{
					if (UAnimSequence* ResolvedAnimation = ActiveVariant.RunAnimation.LoadSynchronous())
					{
						SkeletalMonsterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						SkeletalMonsterMesh->SetAnimation(ResolvedAnimation);
						SkeletalMonsterMesh->Play(true);
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
					const float TargetHeight = bCloseupPresentation ? BHJumpscareCloseTargetVisualHeight : BHJumpscareWorldTargetVisualHeight;
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
