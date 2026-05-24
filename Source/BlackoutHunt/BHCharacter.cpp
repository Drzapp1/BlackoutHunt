#include "BHCharacter.h"
#include "BHAlarmTrap.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHInteractableInterface.h"
#include "BHLocker.h"
#include "BHNoiseDecoy.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

namespace
{
constexpr float BHKeyboardTurnInputPerSecond = 48.0f;
constexpr float BHKeyboardLookInputPerSecond = 36.0f;
constexpr float BHHorrorThreatRange = 3600.0f;
constexpr float BHHorrorCloseThreatRange = 950.0f;

FString BHCompassFromDelta(const FVector& Delta)
{
	if (Delta.IsNearlyZero())
	{
		return TEXT("nearby");
	}

	const float AbsX = FMath::Abs(Delta.X);
	const float AbsY = FMath::Abs(Delta.Y);
	if (AbsX > AbsY * 1.65f)
	{
		return Delta.X >= 0.0f ? TEXT("east") : TEXT("west");
	}
	if (AbsY > AbsX * 1.65f)
	{
		return Delta.Y >= 0.0f ? TEXT("north") : TEXT("south");
	}

	const FString NS = Delta.Y >= 0.0f ? TEXT("north") : TEXT("south");
	const FString EW = Delta.X >= 0.0f ? TEXT("east") : TEXT("west");
	return FString::Printf(TEXT("%s-%s"), *NS, *EW);
}

void BHConfigureAvatarPart(
	UStaticMeshComponent* Part,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& Location,
	const FRotator& Rotation,
	const FVector& Scale)
{
	if (!Part)
	{
		return;
	}

	BHPropVisuals::ConfigurePart(Part, Mesh, Material, Location, Rotation, Scale, false);
	Part->SetOwnerNoSee(true);
	Part->SetOnlyOwnerSee(false);
	Part->SetCastShadow(true);
	Part->SetReceivesDecals(false);
	Part->SetMobility(EComponentMobility::Movable);
}

FLinearColor BHAvatarColorLerp(const FLinearColor& A, const FLinearColor& B, float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return FLinearColor(
		FMath::Lerp(A.R, B.R, ClampedAlpha),
		FMath::Lerp(A.G, B.G, ClampedAlpha),
		FMath::Lerp(A.B, B.B, ClampedAlpha),
		1.0f);
}

FLinearColor BHAvatarColorScale(const FLinearColor& Color, float Scale)
{
	return FLinearColor(
		FMath::Clamp(Color.R * Scale, 0.0f, 1.0f),
		FMath::Clamp(Color.G * Scale, 0.0f, 1.0f),
		FMath::Clamp(Color.B * Scale, 0.0f, 1.0f),
		1.0f);
}

void BHPrepareRoleMeshComponent(UMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetOwnerNoSee(true);
	Component->SetOnlyOwnerSee(false);
	Component->SetCastShadow(true);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetHiddenInGame(true);
	Component->SetVisibility(false, true);
}

void BHPrepareFlashlightBeamComponent(UStaticMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
	Component->SetReceivesDecals(false);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetTranslucentSortPriority(32);
	Component->SetHiddenInGame(true);
	Component->SetVisibility(false, true);
}

void BHSetRoleMeshVisible(UMeshComponent* Component, bool bVisible)
{
	if (!Component)
	{
		return;
	}

	Component->SetHiddenInGame(!bVisible);
	Component->SetVisibility(bVisible, true);
}

FVector BHRoleModelFeetAtCapsuleBaseOffset(const UCapsuleComponent* Capsule, const USceneComponent* ScaledRoot)
{
	const float CapsuleHalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 96.0f;
	const float RootScaleZ = ScaledRoot ? FMath::Max(0.01f, FMath::Abs(ScaledRoot->GetRelativeScale3D().Z)) : 1.0f;
	return FVector(0.0f, 0.0f, -CapsuleHalfHeight / RootScaleZ);
}

void BHApplyRoleModelMaterial(UMeshComponent* Component, UMaterialInterface* Material, const FLinearColor& Color, float EmissiveStrength)
{
	if (!Component)
	{
		return;
	}

	const int32 MaterialCount = FMath::Max(1, Component->GetNumMaterials());
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		if (Material)
		{
			Component->SetMaterial(MaterialIndex, Material);
		}

		UMaterialInstanceDynamic* DynamicMaterial = Component->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
			DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
			DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), Color);
			DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * EmissiveStrength);
			DynamicMaterial->SetVectorParameterValue(TEXT("Emissive"), Color * EmissiveStrength);
		}
	}
}

FLinearColor BHAvatarPaletteColor(int32 Index)
{
	static const FLinearColor Palette[] = {
		FLinearColor(0.22f, 0.58f, 0.74f, 1.0f),
		FLinearColor(0.78f, 0.46f, 0.18f, 1.0f),
		FLinearColor(0.46f, 0.72f, 0.28f, 1.0f),
		FLinearColor(0.70f, 0.30f, 0.42f, 1.0f),
		FLinearColor(0.52f, 0.44f, 0.86f, 1.0f),
		FLinearColor(0.84f, 0.75f, 0.24f, 1.0f),
		FLinearColor(0.28f, 0.68f, 0.62f, 1.0f),
		FLinearColor(0.76f, 0.76f, 0.80f, 1.0f)
	};

	return Palette[FMath::Abs(Index) % UE_ARRAY_COUNT(Palette)];
}

int32 BHNearestAvatarColorIndex(const FLinearColor& Color)
{
	int32 BestIndex = 0;
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FLinearColor Candidate = BHAvatarPaletteColor(Index);
		const float Distance =
			FMath::Square(Color.R - Candidate.R)
			+ FMath::Square(Color.G - Candidate.G)
			+ FMath::Square(Color.B - Candidate.B);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestIndex = Index;
		}
	}

	return BestIndex;
}

UMaterialInterface* BHQuaterniusPaletteMaterial(const FLinearColor& Color)
{
	static const TCHAR* MaterialPaths[] = {
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/LightBlue.LightBlue"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Brown2.Brown2"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Green.Green"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Red.Red"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Purple.Purple"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Gold.Gold"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/LightGreen.LightGreen"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/White.White")
	};

	return LoadObject<UMaterialInterface>(nullptr, MaterialPaths[BHNearestAvatarColorIndex(Color) % UE_ARRAY_COUNT(MaterialPaths)]);
}

bool BHShouldPreserveQuaterniusMaterial(UMaterialInterface* Material)
{
	const FString Name = Material ? Material->GetName() : FString();
	return Name.Contains(TEXT("Skin"))
		|| Name.Contains(TEXT("Hair"))
		|| Name.Contains(TEXT("Eye"))
		|| Name.Contains(TEXT("Eyebrow"))
		|| Name.Contains(TEXT("Moustache"));
}

void BHApplyQuaterniusPalette(UMeshComponent* Component, const FLinearColor& Color)
{
	if (!Component)
	{
		return;
	}

	UMaterialInterface* PaletteMaterial = BHQuaterniusPaletteMaterial(Color);
	if (!PaletteMaterial)
	{
		return;
	}

	const int32 MaterialCount = Component->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		if (!BHShouldPreserveQuaterniusMaterial(Component->GetMaterial(MaterialIndex)))
		{
			Component->SetMaterial(MaterialIndex, PaletteMaterial);
		}
	}
}

UMaterialInterface* BHQuaterniusMaterial(const TCHAR* AssetName)
{
	const FString Name(AssetName);
	const FString Path = FString::Printf(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/%s.%s"), *Name, *Name);
	return LoadObject<UMaterialInterface>(nullptr, *Path);
}

void BHSetAccessoryPiece(
	UStaticMeshComponent* Part,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& Location,
	const FRotator& Rotation,
	const FVector& Scale,
	bool bVisible)
{
	if (!Part)
	{
		return;
	}

	if (Mesh)
	{
		Part->SetStaticMesh(Mesh);
	}
	if (Material)
	{
		Part->SetMaterial(0, Material);
	}

	Part->SetRelativeLocation(Location);
	Part->SetRelativeRotation(Rotation);
	Part->SetRelativeScale3D(Scale);
	BHPropVisuals::SetPartVisible(Part, bVisible);
}

const TCHAR* BHSelectQuaterniusMeshPath(const ABHPlayerState* BHPS)
{
	static const TCHAR* SurvivorMeshes[] = {
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Casual_2.SK_BH_Q_Casual_2"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Worker.SK_BH_Q_Worker"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Adventurer.SK_BH_Q_Adventurer"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Farmer.SK_BH_Q_Farmer"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Beach.SK_BH_Q_Beach"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Punk.SK_BH_Q_Punk"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Suit.SK_BH_Q_Suit"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Spacesuit.SK_BH_Q_Spacesuit")
	};
	static const TCHAR* HunterMeshes[] = {
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Swat.SK_BH_Q_Swat"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Suit.SK_BH_Q_Suit"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Spacesuit.SK_BH_Q_Spacesuit"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_King.SK_BH_Q_King")
	};

	const int32 AvatarIndex = BHPS ? FMath::Abs(BHPS->AvatarIndex) : 0;
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		return HunterMeshes[AvatarIndex % UE_ARRAY_COUNT(HunterMeshes)];
	}
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Suit.SK_BH_Q_Suit");
	}
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Adventurer.SK_BH_Q_Adventurer");
	}

	return SurvivorMeshes[AvatarIndex % UE_ARRAY_COUNT(SurvivorMeshes)];
}

const TCHAR* BHQuaterniusAnimationPath(FName AnimationName)
{
	if (AnimationName == FName(TEXT("Run")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Run.A_BH_Q_Run");
	}
	if (AnimationName == FName(TEXT("Walk")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Walk.A_BH_Q_Walk");
	}
	if (AnimationName == FName(TEXT("Death")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Death.A_BH_Q_Death");
	}

	return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Idle.A_BH_Q_Idle");
}

bool BHUsesHunterMovementProfile(const ABHPlayerState* BHPS)
{
	return BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter;
}

float BHRoleWalkSpeed(const ABHPlayerState* BHPS, float DefaultWalkSpeed)
{
	return BHUsesHunterMovementProfile(BHPS) ? 315.0f : DefaultWalkSpeed;
}

float BHRoleSprintSpeed(const ABHPlayerState* BHPS, float DefaultSprintSpeed)
{
	return BHUsesHunterMovementProfile(BHPS) ? 1150.0f : DefaultSprintSpeed;
}

float BHRoleSprintDrainMultiplier(const ABHPlayerState* BHPS)
{
	return BHUsesHunterMovementProfile(BHPS) ? 1.75f : 1.0f;
}
}

ABHCharacter::ABHCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	Camera->bUsePawnControlRotation = true;

	AvatarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AvatarRoot"));
	AvatarRoot->SetupAttachment(GetCapsuleComponent());

	RoleModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RoleModelRoot"));
	RoleModelRoot->SetupAttachment(AvatarRoot);
	RoleModelRoot->SetMobility(EComponentMobility::Movable);

	RoleStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleStaticMesh"));
	RoleStaticMesh->SetupAttachment(RoleModelRoot);
	BHPrepareRoleMeshComponent(RoleStaticMesh);

	RoleSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RoleSkeletalMesh"));
	RoleSkeletalMesh->SetupAttachment(RoleModelRoot);
	RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	BHPrepareRoleMeshComponent(RoleSkeletalMesh);

	const auto CreateJoint = [this](FName Name, USceneComponent* Parent)
	{
		USceneComponent* Joint = CreateDefaultSubobject<USceneComponent>(Name);
		Joint->SetupAttachment(Parent);
		Joint->SetMobility(EComponentMobility::Movable);
		return Joint;
	};

	const auto CreateAvatarMesh = [this](FName Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetOwnerNoSee(true);
		Part->SetOnlyOwnerSee(false);
		Part->SetCastShadow(true);
		Part->SetMobility(EComponentMobility::Movable);
		return Part;
	};

	HeadJoint = CreateJoint(TEXT("HeadJoint"), AvatarRoot);
	LeftShoulderJoint = CreateJoint(TEXT("LeftShoulderJoint"), AvatarRoot);
	RightShoulderJoint = CreateJoint(TEXT("RightShoulderJoint"), AvatarRoot);
	LeftElbowJoint = CreateJoint(TEXT("LeftElbowJoint"), LeftShoulderJoint);
	RightElbowJoint = CreateJoint(TEXT("RightElbowJoint"), RightShoulderJoint);
	LeftHipJoint = CreateJoint(TEXT("LeftHipJoint"), AvatarRoot);
	RightHipJoint = CreateJoint(TEXT("RightHipJoint"), AvatarRoot);
	LeftKneeJoint = CreateJoint(TEXT("LeftKneeJoint"), LeftHipJoint);
	RightKneeJoint = CreateJoint(TEXT("RightKneeJoint"), RightHipJoint);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(AvatarRoot);
	ChestMesh = CreateAvatarMesh(TEXT("ChestMesh"), AvatarRoot);
	WaistMesh = CreateAvatarMesh(TEXT("WaistMesh"), AvatarRoot);
	HeadMesh = CreateAvatarMesh(TEXT("HeadMesh"), HeadJoint);
	HairMesh = CreateAvatarMesh(TEXT("HairMesh"), HeadJoint);
	LeftEyeMesh = CreateAvatarMesh(TEXT("LeftEyeMesh"), HeadJoint);
	RightEyeMesh = CreateAvatarMesh(TEXT("RightEyeMesh"), HeadJoint);
	MouthMesh = CreateAvatarMesh(TEXT("MouthMesh"), HeadJoint);
	LeftUpperArmMesh = CreateAvatarMesh(TEXT("LeftUpperArmMesh"), LeftShoulderJoint);
	LeftLowerArmMesh = CreateAvatarMesh(TEXT("LeftLowerArmMesh"), LeftElbowJoint);
	LeftHandMesh = CreateAvatarMesh(TEXT("LeftHandMesh"), LeftElbowJoint);
	RightUpperArmMesh = CreateAvatarMesh(TEXT("RightUpperArmMesh"), RightShoulderJoint);
	RightLowerArmMesh = CreateAvatarMesh(TEXT("RightLowerArmMesh"), RightElbowJoint);
	RightHandMesh = CreateAvatarMesh(TEXT("RightHandMesh"), RightElbowJoint);
	LeftUpperLegMesh = CreateAvatarMesh(TEXT("LeftUpperLegMesh"), LeftHipJoint);
	LeftLowerLegMesh = CreateAvatarMesh(TEXT("LeftLowerLegMesh"), LeftKneeJoint);
	LeftFootMesh = CreateAvatarMesh(TEXT("LeftFootMesh"), LeftKneeJoint);
	RightUpperLegMesh = CreateAvatarMesh(TEXT("RightUpperLegMesh"), RightHipJoint);
	RightLowerLegMesh = CreateAvatarMesh(TEXT("RightLowerLegMesh"), RightKneeJoint);
	RightFootMesh = CreateAvatarMesh(TEXT("RightFootMesh"), RightKneeJoint);
	BackpackMesh = CreateAvatarMesh(TEXT("BackpackMesh"), AvatarRoot);
	RoleBadgeMesh = CreateAvatarMesh(TEXT("RoleBadgeMesh"), AvatarRoot);
	RoleHeadwearMesh = CreateAvatarMesh(TEXT("RoleHeadwearMesh"), RoleModelRoot);
	RoleHeadwearAccentMesh = CreateAvatarMesh(TEXT("RoleHeadwearAccentMesh"), RoleModelRoot);
	RoleHeadwearDetailMesh = CreateAvatarMesh(TEXT("RoleHeadwearDetailMesh"), RoleModelRoot);
	RoleGearMesh = CreateAvatarMesh(TEXT("RoleGearMesh"), RoleModelRoot);
	RoleGearAccentMesh = CreateAvatarMesh(TEXT("RoleGearAccentMesh"), RoleModelRoot);
	RoleGearLeftStrapMesh = CreateAvatarMesh(TEXT("RoleGearLeftStrapMesh"), RoleModelRoot);
	RoleGearRightStrapMesh = CreateAvatarMesh(TEXT("RoleGearRightStrapMesh"), RoleModelRoot);
	RoleGearDetailMesh = CreateAvatarMesh(TEXT("RoleGearDetailMesh"), RoleModelRoot);

	ConfigureLowPolyAvatar();

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Camera);
	Flashlight->SetIntensity(9000.0f);
	Flashlight->SetAttenuationRadius(2200.0f);
	Flashlight->SetInnerConeAngle(18.0f);
	Flashlight->SetOuterConeAngle(34.0f);
	Flashlight->SetVolumetricScatteringIntensity(5.0f);
	Flashlight->SetVisibility(false);

	UStaticMesh* BeamMeshOuter = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineVolumetrics/LightBeam/Mesh/S_EV_SimpleLightBeam_03_cross.S_EV_SimpleLightBeam_03_cross"));
	UStaticMesh* BeamMeshCore = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineVolumetrics/LightBeam/Mesh/S_EV_SimpleLightBeam_01.S_EV_SimpleLightBeam_01"));
	if (!BeamMeshOuter)
	{
		BeamMeshOuter = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}
	if (!BeamMeshCore)
	{
		BeamMeshCore = BeamMeshOuter;
	}

	UMaterialInterface* BeamMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineVolumetrics/LightBeam/Materials/M_EV_Lightbeam_Master_01_Inst.M_EV_Lightbeam_Master_01_Inst"));
	if (!BeamMaterial)
	{
		BeamMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineVolumetrics/LightBeam/Materials/M_EV_Lightbeam_Master_01.M_EV_Lightbeam_Master_01"));
	}

	FlashlightBeamOuter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightBeamOuter"));
	FlashlightBeamOuter->SetupAttachment(Flashlight);
	if (BeamMeshOuter)
	{
		FlashlightBeamOuter->SetStaticMesh(BeamMeshOuter);
	}
	if (BeamMaterial)
	{
		FlashlightBeamOuter->SetMaterial(0, BeamMaterial);
	}
	BHPrepareFlashlightBeamComponent(FlashlightBeamOuter);

	FlashlightBeamCore = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightBeamCore"));
	FlashlightBeamCore->SetupAttachment(Flashlight);
	if (BeamMeshCore)
	{
		FlashlightBeamCore->SetStaticMesh(BeamMeshCore);
	}
	if (BeamMaterial)
	{
		FlashlightBeamCore->SetMaterial(0, BeamMaterial);
	}
	BHPrepareFlashlightBeamComponent(FlashlightBeamCore);

	WalkSpeed = 360.0f;
	SprintSpeed = 900.0f;
	MaxStamina = 200.0f;
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	InteractDistance = FMath::Max(150.0f, Settings->InteractDistance);
	CaptureDistance = FMath::Max(100.0f, Settings->CaptureDistance);
	FlashlightDrainPerSecond = FMath::Max(0.0f, Settings->FlashlightDrainPerSecond);
	ScanCooldownSeconds = FMath::Max(0.0f, Settings->ScanCooldownSeconds);
	DecoyCooldownSeconds = FMath::Max(0.0f, Settings->DecoyCooldownSeconds);
	StaminaDrainPerSecond = 20.0f;
	StaminaRecoveryPerSecond = 14.0f;
	bFlashlightOn = false;
	FlashlightBattery = 100.0f;
	Stamina = MaxStamina;
	Fear = 0.0f;
	Dread = 0.0f;
	DetentionMarkRemaining = 0.0f;
	bHiddenInLocker = false;
	bOutOfPlay = false;
	LastScanTime = -999.0f;
	LastHunterPowerTime = -999.0f;
	LastDecoyTime = -999.0f;
	LastSprintNoiseTime = -999.0f;
	LastStaminaWarningTime = -999.0f;
	LastHidingPanicMessageTime = -999.0f;
	LastLockerNoiseTime = -999.0f;
	LastForcedBreathNoiseTime = -999.0f;
	LastDetentionNoiseTime = -999.0f;
	HiddenSeconds = 0.0f;
	DefaultCameraFOV = 90.0f;
	DefaultCameraLocation = FVector(0.0f, 0.0f, 64.0f);
	CameraBobTime = 0.0f;
	AvatarAnimTime = 0.0f;
	AvatarBreathTime = 0.0f;
	AvatarMoveAlpha = 0.0f;
	AvatarSprintAlpha = 0.0f;
	SmoothedMoveAlpha = 0.0f;
	SmoothedStrafeAlpha = 0.0f;
	SmoothedSprintAlpha = 0.0f;
	FlashlightPulseTime = 0.0f;
	bKeyboardLookLeft = false;
	bKeyboardLookRight = false;
	bKeyboardLookUp = false;
	bKeyboardLookDown = false;
	bBHopJumpHeld = false;
	bUsingRoleModel = false;
	LastRoleAnimationName = NAME_None;
	LastAppliedAvatarIndex = INDEX_NONE;
	LastAppliedAvatarRole = EBHPlayerRole::Unassigned;
	LastAppliedAvatarColor = FLinearColor::Transparent;
	LastAppliedAvatarHeadwearIndex = INDEX_NONE;
	LastAppliedAvatarGearIndex = INDEX_NONE;

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 205.0f;
	GetCharacterMovement()->MaxAcceleration = 4200.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 3200.0f;
	GetCharacterMovement()->BrakingFrictionFactor = 0.75f;
	GetCharacterMovement()->GroundFriction = 7.25f;
	GetCharacterMovement()->JumpZVelocity = 520.0f;
	GetCharacterMovement()->AirControl = 0.58f;
	GetCharacterMovement()->BrakingDecelerationFalling = 192.0f;
	GetCharacterMovement()->FallingLateralFriction = 0.05f;
	GetCharacterMovement()->SetCrouchedHalfHeight(62.0f);
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
}

void ABHCharacter::BeginPlay()
{
	Super::BeginPlay();
	ConfigureLowPolyAvatar();
	if (Camera)
	{
		DefaultCameraFOV = Camera->FieldOfView;
		DefaultCameraLocation = Camera->GetRelativeLocation();
	}
	if (FlashlightBeamOuter)
	{
		FlashlightBeamOuterMaterial = FlashlightBeamOuter->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (FlashlightBeamCore)
	{
		FlashlightBeamCoreMaterial = FlashlightBeamCore->CreateAndSetMaterialInstanceDynamic(0);
	}
	ApplyFlashlightState();
	ApplyHiddenState();
	ApplyAvatarStyle();
}

void ABHCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyAvatarStyle();
}

void ABHCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyAvatarStyle();
}

ABHPlayerState* ABHCharacter::GetBHPlayerState() const
{
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		return BHPS;
	}

	const AController* OwnerController = GetController();
	return OwnerController ? OwnerController->GetPlayerState<ABHPlayerState>() : nullptr;
}

void ABHCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyKeyboardLook(DeltaSeconds);

	if (HasAuthority() && bFlashlightOn)
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		const bool bInfiniteFlashlight = (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester) || (BHGS && BHGS->bTestMode);
		if (bInfiniteFlashlight)
		{
			FlashlightBattery = 100.0f;
		}
		else
		{
			FlashlightBattery = FMath::Max(0.0f, FlashlightBattery - FlashlightDrainPerSecond * DeltaSeconds);
			if (FlashlightBattery <= 0.0f)
			{
				bFlashlightOn = false;
				ApplyFlashlightState();
			}
		}
	}

	if (HasAuthority())
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		if (BHPS && BHPS->IsAliveSurvivor() && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt)
		{
			if (bHiddenInLocker)
			{
				HiddenSeconds += DeltaSeconds;
			}
			else
			{
				HiddenSeconds = 0.0f;
			}

			float NearestHunterDistSq = TNumericLimits<float>::Max();
			bool bHunterHasSight = false;
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				ABHCharacter* Other = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
				const ABHPlayerState* OtherPS = Other ? Other->GetPlayerState<ABHPlayerState>() : nullptr;
				if (Other && Other != this && OtherPS && OtherPS->IsAliveHunter())
				{
					const float DistSq = FVector::DistSquared2D(Other->GetActorLocation(), GetActorLocation());
					if (DistSq < NearestHunterDistSq)
					{
						NearestHunterDistSq = DistSq;
						bHunterHasSight = Other->GetController() ? Other->GetController()->LineOfSightTo(this) : false;
					}
				}
			}

			if (NearestHunterDistSq <= FMath::Square(BHHorrorThreatRange))
			{
				const float NearestHunterDistance = FMath::Sqrt(NearestHunterDistSq);
				const float DistanceAlpha = 1.0f - FMath::Clamp(NearestHunterDistance / BHHorrorThreatRange, 0.0f, 1.0f);
				const float HiddenMultiplier = bHiddenInLocker ? 1.62f : 1.0f;
				const float SightMultiplier = bHunterHasSight && !bHiddenInLocker ? 1.75f : 1.0f;
				Fear = FMath::Clamp(Fear + (3.5f + 14.0f * DistanceAlpha) * HiddenMultiplier * SightMultiplier * DeltaSeconds, 0.0f, 100.0f);

				const float HiddenDreadBonus = bHiddenInLocker ? 12.0f : 0.0f;
				const float DarknessDreadBonus = bFlashlightOn ? 0.0f : 4.5f;
				Dread = FMath::Clamp(Dread + (5.0f + 18.0f * DistanceAlpha + HiddenDreadBonus + DarknessDreadBonus) * DeltaSeconds, 0.0f, 100.0f);

				if (bHiddenInLocker && NearestHunterDistance <= BHHorrorCloseThreatRange)
				{
					const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
					if (Now - LastHidingPanicMessageTime > 9.0f)
					{
						LastHidingPanicMessageTime = Now;
						SendStatusMessage(TEXT("Something is outside the door. Stay still."));
					}
				}
			}
			else
			{
				Fear = FMath::Max(0.0f, Fear - 4.5f * DeltaSeconds);
				const float HiddenDecay = bHiddenInLocker ? 1.2f : 6.5f;
				Dread = FMath::Max(0.0f, Dread - HiddenDecay * DeltaSeconds);
			}

			if (bHiddenInLocker && HiddenSeconds > 12.0f)
			{
				const float LongHideAlpha = FMath::Clamp((HiddenSeconds - 12.0f) / 34.0f, 0.0f, 1.0f);
				Dread = FMath::Clamp(Dread + (1.2f + LongHideAlpha * 7.5f) * DeltaSeconds, 0.0f, 100.0f);
				const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				if (Dread >= 86.0f && Now - LastForcedBreathNoiseTime > 16.0f)
				{
					LastForcedBreathNoiseTime = Now;
					SendStatusMessage(TEXT("You breathed too loudly."));
					if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
					{
						BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("panicked breathing"));
					}
				}
			}

			if (DetentionMarkRemaining > 0.0f)
			{
				DetentionMarkRemaining = FMath::Max(0.0f, DetentionMarkRemaining - DeltaSeconds);
				Dread = FMath::Clamp(Dread + 3.8f * DeltaSeconds, 0.0f, 100.0f);

				const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(45.0f);
				const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				const float NoiseCooldown = bHiddenInLocker ? 9.0f : 5.5f;
				if ((bMoving || bHiddenInLocker) && Now - LastDetentionNoiseTime > NoiseCooldown)
				{
					LastDetentionNoiseTime = Now;
					if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
					{
						BHGM->NotifyLoudNoise(GetActorLocation(), bHiddenInLocker ? TEXT("detention whisper") : TEXT("detention mark"));
					}
				}
			}
		}
		else
		{
			HiddenSeconds = 0.0f;
			Fear = FMath::Max(0.0f, Fear - 8.0f * DeltaSeconds);
			Dread = FMath::Max(0.0f, Dread - 10.0f * DeltaSeconds);
			DetentionMarkRemaining = 0.0f;
		}
	}

	UpdateViewFeel(DeltaSeconds);
	UpdateFlashlightFeel(DeltaSeconds);
	UpdateLowPolyAvatar(DeltaSeconds);

	if (bBHopJumpHeld)
	{
		TryBHopJump();
	}

	if (CanAct())
	{
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const float CurrentWalkSpeed = BHRoleWalkSpeed(BHPS, WalkSpeed);
		const float CurrentSprintSpeed = BHRoleSprintSpeed(BHPS, SprintSpeed);
		if (Movement && IsPlayerControlled() && Movement->MaxWalkSpeed <= FMath::Max(WalkSpeed, CurrentWalkSpeed) + 1.0f)
		{
			Movement->MaxWalkSpeed = CurrentWalkSpeed;
		}

		const bool bTryingToSprint = Movement
			&& Movement->MaxWalkSpeed >= CurrentSprintSpeed - 1.0f
			&& GetVelocity().SizeSquared2D() > FMath::Square(30.0f);
		if (HasAuthority())
		{
			if (bTryingToSprint)
			{
				const float DrainMultiplier = BHRoleSprintDrainMultiplier(BHPS);
				Stamina = FMath::Max(0.0f, Stamina - StaminaDrainPerSecond * DrainMultiplier * DeltaSeconds);
				if (Stamina <= 0.0f)
				{
					if (Movement)
					{
						Movement->MaxWalkSpeed = CurrentWalkSpeed;
					}
					const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
					if (Now - LastStaminaWarningTime > 4.0f)
					{
						LastStaminaWarningTime = Now;
						SendStatusMessage(TEXT("You are exhausted."));
					}
				}
			}
			else
			{
				const float RecoveryMultiplier = FMath::Lerp(1.0f, 0.65f, FMath::Clamp(Fear / 100.0f, 0.0f, 1.0f));
				Stamina = FMath::Min(MaxStamina, Stamina + StaminaRecoveryPerSecond * RecoveryMultiplier * DeltaSeconds);
			}
		}
		else if (Stamina <= 0.0f && Movement && Movement->MaxWalkSpeed >= CurrentSprintSpeed - 1.0f)
		{
			Movement->MaxWalkSpeed = CurrentWalkSpeed;
		}
	}

	if (const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		constexpr int32 ForcedHeadwearIndex = 0;
		constexpr int32 ForcedGearIndex = 0;
		const bool bAvatarChanged = BHPS->AvatarIndex != LastAppliedAvatarIndex
			|| BHPS->PlayerRole != LastAppliedAvatarRole
			|| !BHPS->AvatarColor.Equals(LastAppliedAvatarColor, 0.005f)
			|| ForcedHeadwearIndex != LastAppliedAvatarHeadwearIndex
			|| ForcedGearIndex != LastAppliedAvatarGearIndex;
		if (bAvatarChanged)
		{
			ApplyAvatarStyle();
		}
	}
}

void ABHCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ABHCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ABHCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ABHCharacter::Turn);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ABHCharacter::LookUp);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ABHCharacter::StartKeyboardTurnLeft);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Released, this, &ABHCharacter::StopKeyboardTurnLeft);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ABHCharacter::StartKeyboardTurnRight);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Released, this, &ABHCharacter::StopKeyboardTurnRight);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ABHCharacter::StartKeyboardLookUp);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Released, this, &ABHCharacter::StopKeyboardLookUp);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ABHCharacter::StartKeyboardLookDown);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Released, this, &ABHCharacter::StopKeyboardLookDown);
	PlayerInputComponent->BindAction(TEXT("Ready"), IE_Pressed, this, &ABHCharacter::ToggleReady);
	PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ABHCharacter::StartInteract);
	PlayerInputComponent->BindAction(TEXT("Interact"), IE_Released, this, &ABHCharacter::StopInteract);
	PlayerInputComponent->BindAction(TEXT("Flashlight"), IE_Pressed, this, &ABHCharacter::ToggleFlashlight);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ABHCharacter::StartJump);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ABHCharacter::StopJump);
	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &ABHCharacter::StartSprint);
	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &ABHCharacter::StopSprint);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &ABHCharacter::StartCrouch);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Released, this, &ABHCharacter::StopCrouch);
	PlayerInputComponent->BindAction(TEXT("Capture"), IE_Pressed, this, &ABHCharacter::TryCapture);
	PlayerInputComponent->BindAction(TEXT("Scan"), IE_Pressed, this, &ABHCharacter::UseScan);
	PlayerInputComponent->BindAction(TEXT("HunterPower"), IE_Pressed, this, &ABHCharacter::UseHunterPower);
	PlayerInputComponent->BindAction(TEXT("Decoy"), IE_Pressed, this, &ABHCharacter::DropDecoy);
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ABHCharacter::SubmitAnswerOne);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ABHCharacter::SubmitAnswerTwo);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ABHCharacter::SubmitAnswerThree);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ABHCharacter::SubmitAnswerFour);
	PlayerInputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &ABHCharacter::SubmitAnswerOne);
	PlayerInputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &ABHCharacter::SubmitAnswerTwo);
	PlayerInputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &ABHCharacter::SubmitAnswerThree);
	PlayerInputComponent->BindKey(EKeys::NumPadFour, IE_Pressed, this, &ABHCharacter::SubmitAnswerFour);
}

void ABHCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABHCharacter, bFlashlightOn);
	DOREPLIFETIME(ABHCharacter, FlashlightBattery);
	DOREPLIFETIME(ABHCharacter, Stamina);
	DOREPLIFETIME(ABHCharacter, Fear);
	DOREPLIFETIME(ABHCharacter, Dread);
	DOREPLIFETIME(ABHCharacter, DetentionMarkRemaining);
	DOREPLIFETIME(ABHCharacter, bHiddenInLocker);
	DOREPLIFETIME(ABHCharacter, bOutOfPlay);
}

void ABHCharacter::EnterLocker(ABHLocker* Locker)
{
	if (!HasAuthority() || !Locker)
	{
		return;
	}

	CurrentLocker = Locker;
	bHiddenInLocker = true;
	bFlashlightOn = false;
	HiddenSeconds = 0.0f;
	Dread = FMath::Clamp(Dread + 8.0f, 0.0f, 100.0f);

	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetHiddenInLocker(true);
	}

	SetActorLocation(Locker->GetActorLocation());
	ApplyFlashlightState();
	ApplyHiddenState();
}

void ABHCharacter::ExitLocker()
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentLocker)
	{
		CurrentLocker->ClearOccupant(this);
		const FVector ExitLocation = CurrentLocker->GetActorLocation() + CurrentLocker->GetActorForwardVector() * 120.0f;
		SetActorLocation(ExitLocation);
		CurrentLocker = nullptr;

		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if ((Fear >= 55.0f || Dread >= 55.0f) && Now - LastLockerNoiseTime > 6.0f)
		{
			LastLockerNoiseTime = Now;
			if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
			{
				BHGM->NotifyLoudNoise(ExitLocation, TEXT("locker door"));
			}
		}
	}

	bHiddenInLocker = false;
	HiddenSeconds = 0.0f;

	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetHiddenInLocker(false);
	}

	ApplyHiddenState();
}

void ABHCharacter::MarkCaptured()
{
	if (!HasAuthority())
	{
		return;
	}

	ExitLocker();

	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetLifeState(EBHPlayerLifeState::Captured);
	}

	bFlashlightOn = false;
	bOutOfPlay = true;
	SetActorHiddenInGame(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	ApplyFlashlightState();
	ApplyHiddenState();
}

void ABHCharacter::MarkEscaped()
{
	if (!HasAuthority())
	{
		return;
	}

	ExitLocker();

	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetLifeState(EBHPlayerLifeState::Escaped);
	}

	bFlashlightOn = false;
	bOutOfPlay = true;
	SetActorHiddenInGame(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	ApplyFlashlightState();
	ApplyHiddenState();
}

void ABHCharacter::RefillFlashlight(float Amount)
{
	if (!HasAuthority())
	{
		return;
	}

	FlashlightBattery = FMath::Clamp(FlashlightBattery + Amount, 0.0f, 100.0f);
}

void ABHCharacter::RecoverStamina(float Amount)
{
	if (HasAuthority())
	{
		Stamina = FMath::Clamp(Stamina + Amount, 0.0f, MaxStamina);
	}
}

void ABHCharacter::AddFear(float Amount)
{
	if (HasAuthority())
	{
		Fear = FMath::Clamp(Fear + Amount, 0.0f, 100.0f);
	}
}

void ABHCharacter::AddDread(float Amount)
{
	if (HasAuthority())
	{
		Dread = FMath::Clamp(Dread + Amount, 0.0f, 100.0f);
	}
}

void ABHCharacter::ApplyDetentionMark(float DurationSeconds)
{
	if (HasAuthority())
	{
		DetentionMarkRemaining = FMath::Max(DetentionMarkRemaining, FMath::Max(0.0f, DurationSeconds));
		LastDetentionNoiseTime = GetWorld() ? GetWorld()->GetTimeSeconds() - 3.0f : LastDetentionNoiseTime;
	}
}

void ABHCharacter::ClearDetentionMark()
{
	if (HasAuthority())
	{
		DetentionMarkRemaining = 0.0f;
	}
}

bool ABHCharacter::IsHiddenInLocker() const
{
	return bHiddenInLocker;
}

float ABHCharacter::GetFlashlightBattery() const
{
	return FlashlightBattery;
}

float ABHCharacter::GetStamina() const
{
	return Stamina;
}

float ABHCharacter::GetStaminaPercent() const
{
	return MaxStamina > 0.0f ? FMath::Clamp((Stamina / MaxStamina) * 100.0f, 0.0f, 100.0f) : 0.0f;
}

float ABHCharacter::GetFear() const
{
	return Fear;
}

float ABHCharacter::GetDread() const
{
	return Dread;
}

bool ABHCharacter::IsDetentionMarked() const
{
	return DetentionMarkRemaining > 0.0f;
}

float ABHCharacter::GetDetentionMarkRemaining() const
{
	return DetentionMarkRemaining;
}

void ABHCharacter::MoveForward(float Value)
{
	if (Value != 0.0f && CanAct())
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ABHCharacter::MoveRight(float Value)
{
	if (Value != 0.0f && CanAct())
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ABHCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void ABHCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void ABHCharacter::StartKeyboardTurnLeft()
{
	bKeyboardLookLeft = true;
}

void ABHCharacter::StopKeyboardTurnLeft()
{
	bKeyboardLookLeft = false;
}

void ABHCharacter::StartKeyboardTurnRight()
{
	bKeyboardLookRight = true;
}

void ABHCharacter::StopKeyboardTurnRight()
{
	bKeyboardLookRight = false;
}

void ABHCharacter::StartKeyboardLookUp()
{
	bKeyboardLookUp = true;
}

void ABHCharacter::StopKeyboardLookUp()
{
	bKeyboardLookUp = false;
}

void ABHCharacter::StartKeyboardLookDown()
{
	bKeyboardLookDown = true;
}

void ABHCharacter::StopKeyboardLookDown()
{
	bKeyboardLookDown = false;
}

void ABHCharacter::ApplyKeyboardLook(float DeltaSeconds)
{
	const float YawValue = (bKeyboardLookRight ? 1.0f : 0.0f) - (bKeyboardLookLeft ? 1.0f : 0.0f);
	if (YawValue != 0.0f)
	{
		AddControllerYawInput(YawValue * BHKeyboardTurnInputPerSecond * DeltaSeconds);
	}

	const float PitchValue = (bKeyboardLookUp ? 1.0f : 0.0f) - (bKeyboardLookDown ? 1.0f : 0.0f);
	if (PitchValue != 0.0f)
	{
		AddControllerPitchInput(PitchValue * BHKeyboardLookInputPerSecond * DeltaSeconds);
	}
}

void ABHCharacter::TryBHopJump()
{
	if (!CanAct())
	{
		StopJumping();
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement && Movement->IsMovingOnGround())
	{
		Jump();
	}
}

void ABHCharacter::ToggleReady()
{
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(GetController()))
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		BHPC->ServerSetReady(!(BHPS && BHPS->bReady));
	}
}

void ABHCharacter::StartInteract()
{
	if (bHiddenInLocker)
	{
		ServerExitCurrentLocker();
		return;
	}

	AActor* Target = nullptr;
	TraceForInteractable(Target);
	CurrentInteractTarget = Target;
	ServerBeginInteract(Target);
}

void ABHCharacter::StopInteract()
{
	ServerEndInteract(CurrentInteractTarget);
	CurrentInteractTarget = nullptr;
}

void ABHCharacter::ToggleFlashlight()
{
	if (!CanAct())
	{
		return;
	}

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bInfiniteFlashlight = (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester) || (BHGS && BHGS->bTestMode);
	if (bInfiniteFlashlight)
	{
		FlashlightBattery = 100.0f;
	}

	if (!bFlashlightOn && FlashlightBattery <= 1.0f)
	{
		SendStatusMessage(TEXT("Flashlight battery is dead."));
		return;
	}

	if (!bFlashlightOn && FlashlightBattery <= 20.0f)
	{
		SendStatusMessage(TEXT("Flashlight battery is almost gone."));
	}

	ServerSetFlashlight(!bFlashlightOn);
}

void ABHCharacter::StartJump()
{
	bBHopJumpHeld = true;
	TryBHopJump();
}

void ABHCharacter::StopJump()
{
	bBHopJumpHeld = false;
	StopJumping();
}

void ABHCharacter::StartSprint()
{
	if (!CanAct())
	{
		return;
	}

	if (Stamina > MaxStamina * 0.08f)
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		GetCharacterMovement()->MaxWalkSpeed = BHRoleSprintSpeed(BHPS, SprintSpeed);
		ServerSetSprinting(true);
	}
	else
	{
		SendStatusMessage(TEXT("Too exhausted to sprint."));
	}
}

void ABHCharacter::StopSprint()
{
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	GetCharacterMovement()->MaxWalkSpeed = BHRoleWalkSpeed(BHPS, WalkSpeed);
	ServerSetSprinting(false);
}

void ABHCharacter::StartCrouch()
{
	if (CanAct())
	{
		Crouch();
	}
}

void ABHCharacter::StopCrouch()
{
	UnCrouch();
}

void ABHCharacter::TryCapture()
{
	ServerTryCapture();
}

void ABHCharacter::UseScan()
{
	ServerUseScan();
}

void ABHCharacter::UseHunterPower()
{
	ServerUseHunterPower();
}

void ABHCharacter::DropDecoy()
{
	ServerDropDecoy();
}

void ABHCharacter::SubmitAnswer(int32 AnswerIndex)
{
	if (!CanAct())
	{
		SendStatusMessage(bHiddenInLocker ? TEXT("You cannot answer while hiding.") : TEXT("You cannot answer right now."));
		return;
	}

	ServerSubmitAnswer(AnswerIndex);
}

void ABHCharacter::SubmitAnswerOne()
{
	SubmitAnswer(0);
}

void ABHCharacter::SubmitAnswerTwo()
{
	SubmitAnswer(1);
}

void ABHCharacter::SubmitAnswerThree()
{
	SubmitAnswer(2);
}

void ABHCharacter::SubmitAnswerFour()
{
	SubmitAnswer(3);
}

bool ABHCharacter::CanAct() const
{
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	return !bHiddenInLocker && !bOutOfPlay && (!BHPS || BHPS->LifeState == EBHPlayerLifeState::Alive);
}

bool ABHCharacter::TraceForInteractable(AActor*& OutActor) const
{
	return FindInteractableFromView(OutActor);
}

bool ABHCharacter::FindInteractableFromView(AActor*& OutActor, float ExtraDistance) const
{
	OutActor = nullptr;

	if (!GetWorld())
	{
		return false;
	}

	FVector Start = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (Controller)
	{
		Controller->GetPlayerViewPoint(Start, ViewRotation);
	}
	else if (Camera)
	{
		Start = Camera->GetComponentLocation();
		ViewRotation = Camera->GetComponentRotation();
	}
	else
	{
		return false;
	}

	const float TraceDistance = InteractDistance + ExtraDistance;
	const FVector ViewDirection = ViewRotation.Vector();
	const FVector End = Start + ViewDirection * TraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHInteractTrace), false, this);
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, Params);

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
		{
			OutActor = HitActor;
			return true;
		}

		if (Hit.bBlockingHit)
		{
			break;
		}
	}

	float BestScore = 0.0f;
	AActor* BestActor = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - Start;
		const float Distance = ToCandidate.Size();
		if (Distance > TraceDistance + 125.0f)
		{
			continue;
		}

		const float Dot = FVector::DotProduct(ViewDirection, ToCandidate.GetSafeNormal());
		if (Dot < 0.86f)
		{
			continue;
		}

		FHitResult VisibilityHit;
		GetWorld()->LineTraceSingleByChannel(VisibilityHit, Start, Candidate->GetActorLocation(), ECC_Visibility, Params);
		if (VisibilityHit.bBlockingHit && VisibilityHit.GetActor() != Candidate)
		{
			continue;
		}

		const float Score = Dot / FMath::Max(100.0f, Distance);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestActor = Candidate;
		}
	}

	if (BestActor)
	{
		OutActor = BestActor;
		return true;
	}

	return false;
}

bool ABHCharacter::IsValidInteractionTarget(AActor* Target) const
{
	if (!Target || !Target->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
	{
		return false;
	}

	return FVector::DistSquared(Target->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractDistance + 175.0f);
}

FString ABHCharacter::GetInteractionFailureReason(AActor* Target) const
{
	const ABHPlayerState* BHPS = GetBHPlayerState();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;

	if (!BHPS)
	{
		return TEXT("Interaction blocked: player state is not ready yet.");
	}

	if (BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return TEXT("Interaction blocked: you are out of play.");
	}

	if (BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		return TEXT("Press Enter on every player to ready up. Host can kick blockers from the roster.");
	}

	if (BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby)
	{
		return TEXT("Round is still in the lobby. Press Enter on every player to start.");
	}

	if (BHGS && BHGS->RoundPhase == EBHRoundPhase::Prep)
	{
		return TEXT("Prep phase: hiding works for survivors. Breakers, scans, and captures start when Hunt begins.");
	}

	if (const ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target))
	{
		if (Station->IsDirectorActive() && !Station->IsQuestionSolved() && BHPS->IsAliveSurvivor())
		{
			return TEXT("Answer the station question with 1-4 before touching the task.");
		}
	}

	const FString Label = Target && Target->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass())
		? IBHInteractableInterface::Execute_GetInteractionLabel(Target, const_cast<ABHCharacter*>(this)).ToString()
		: FString(TEXT("that"));

	return FString::Printf(TEXT("Cannot use %s with your current role or phase."), *Label);
}

void ABHCharacter::SendStatusMessage(const FString& Message) const
{
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(GetController()))
	{
		BHPC->ClientShowStatusMessage(Message, 3.25f);
	}
}

void ABHCharacter::SendFakeHunterHint(bool bRealHint)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const ABHPlayerState* FakePS = GetPlayerState<ABHPlayerState>();
	FVector HintLocation = FVector::ZeroVector;
	bool bFoundSignal = false;

	if (bRealHint)
	{
		float BestDistSq = TNumericLimits<float>::Max();
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			ABHCharacter* Target = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
			const ABHPlayerState* TargetPS = Target ? Target->GetPlayerState<ABHPlayerState>() : nullptr;
			if (!Target || !TargetPS || !TargetPS->IsAliveSurvivor())
			{
				continue;
			}

			const float DistSq = FVector::DistSquared2D(Target->GetActorLocation(), GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				HintLocation = Target->GetActorLocation();
				bFoundSignal = true;
			}
		}

		if (!bFoundSignal)
		{
			SendStatusMessage(TEXT("No survivor signal is available to hint."));
			return;
		}
	}
	else
	{
		const float Angle = FMath::FRandRange(-PI, PI);
		const float Radius = FMath::FRandRange(1200.0f, 3600.0f);
		HintLocation = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;
		bFoundSignal = true;
	}

	int32 HuntersNotified = 0;
	const FString SenderName = FakePS && !FakePS->GetPlayerName().IsEmpty() ? FakePS->GetPlayerName() : FString(TEXT("Hall monitor"));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const ABHPlayerState* HunterPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!PC || !Pawn || !HunterPS || !HunterPS->IsAliveHunter())
		{
			continue;
		}

		const FVector Delta = HintLocation - Pawn->GetActorLocation();
		const float DistanceMeters = Delta.Size2D() / 100.0f;
		PC->ClientShowStatusMessage(FString::Printf(TEXT("%s hint: movement %s, %.0fm away."), *SenderName, *BHCompassFromDelta(Delta), DistanceMeters), 4.0f);
		++HuntersNotified;
	}

	SendStatusMessage(bRealHint
		? FString::Printf(TEXT("Real hint sent to %d Teacher(s)."), HuntersNotified)
		: FString::Printf(TEXT("False hint sent to %d Teacher(s)."), HuntersNotified));
}

void ABHCharacter::ConfigureLowPolyAvatar()
{
	UStaticMesh* Cube = BHPropVisuals::CubeMesh();
	UStaticMesh* Cylinder = BHPropVisuals::CylinderMesh();
	UMaterialInterface* Material = BHPropVisuals::BasicMaterial();

	if (AvatarRoot)
	{
		AvatarRoot->SetRelativeLocation(FVector::ZeroVector);
		AvatarRoot->SetRelativeRotation(FRotator::ZeroRotator);
		AvatarRoot->SetRelativeScale3D(FVector::OneVector);
	}
	if (RoleModelRoot)
	{
		RoleModelRoot->SetRelativeLocation(FVector::ZeroVector);
		RoleModelRoot->SetRelativeRotation(FRotator::ZeroRotator);
		RoleModelRoot->SetRelativeScale3D(FVector::OneVector);
	}

	if (HeadJoint)
	{
		HeadJoint->SetRelativeLocation(FVector(4.0f, 0.0f, 42.0f));
	}
	if (LeftShoulderJoint)
	{
		LeftShoulderJoint->SetRelativeLocation(FVector(0.0f, -28.0f, 24.0f));
	}
	if (RightShoulderJoint)
	{
		RightShoulderJoint->SetRelativeLocation(FVector(0.0f, 28.0f, 24.0f));
	}
	if (LeftElbowJoint)
	{
		LeftElbowJoint->SetRelativeLocation(FVector(0.0f, 0.0f, -32.0f));
	}
	if (RightElbowJoint)
	{
		RightElbowJoint->SetRelativeLocation(FVector(0.0f, 0.0f, -32.0f));
	}
	if (LeftHipJoint)
	{
		LeftHipJoint->SetRelativeLocation(FVector(0.0f, -12.0f, -36.0f));
	}
	if (RightHipJoint)
	{
		RightHipJoint->SetRelativeLocation(FVector(0.0f, 12.0f, -36.0f));
	}
	if (LeftKneeJoint)
	{
		LeftKneeJoint->SetRelativeLocation(FVector(0.0f, 0.0f, -28.0f));
	}
	if (RightKneeJoint)
	{
		RightKneeJoint->SetRelativeLocation(FVector(0.0f, 0.0f, -28.0f));
	}

	BHConfigureAvatarPart(BodyMesh, Cube, Material, FVector(0.0f, 0.0f, -6.0f), FRotator::ZeroRotator, FVector(0.42f, 0.30f, 0.72f));
	BHConfigureAvatarPart(ChestMesh, Cube, Material, FVector(23.0f, 0.0f, 6.0f), FRotator::ZeroRotator, FVector(0.035f, 0.26f, 0.42f));
	BHConfigureAvatarPart(WaistMesh, Cube, Material, FVector(0.0f, 0.0f, -44.0f), FRotator::ZeroRotator, FVector(0.34f, 0.27f, 0.12f));
	BHConfigureAvatarPart(HeadMesh, Cube, Material, FVector(7.0f, 0.0f, 15.0f), FRotator::ZeroRotator, FVector(0.28f, 0.25f, 0.28f));
	BHConfigureAvatarPart(HairMesh, Cube, Material, FVector(1.0f, 0.0f, 31.0f), FRotator::ZeroRotator, FVector(0.29f, 0.26f, 0.09f));
	BHConfigureAvatarPart(LeftEyeMesh, Cube, Material, FVector(22.0f, -7.0f, 18.0f), FRotator::ZeroRotator, FVector(0.018f, 0.036f, 0.020f));
	BHConfigureAvatarPart(RightEyeMesh, Cube, Material, FVector(22.0f, 7.0f, 18.0f), FRotator::ZeroRotator, FVector(0.018f, 0.036f, 0.020f));
	BHConfigureAvatarPart(MouthMesh, Cube, Material, FVector(22.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(0.016f, 0.10f, 0.014f));
	BHConfigureAvatarPart(LeftUpperArmMesh, Cube, Material, FVector(0.0f, 0.0f, -16.0f), FRotator::ZeroRotator, FVector(0.14f, 0.12f, 0.32f));
	BHConfigureAvatarPart(LeftLowerArmMesh, Cube, Material, FVector(0.0f, 0.0f, -15.0f), FRotator::ZeroRotator, FVector(0.12f, 0.10f, 0.30f));
	BHConfigureAvatarPart(LeftHandMesh, Cube, Material, FVector(1.0f, 0.0f, -31.0f), FRotator::ZeroRotator, FVector(0.14f, 0.12f, 0.09f));
	BHConfigureAvatarPart(RightUpperArmMesh, Cube, Material, FVector(0.0f, 0.0f, -16.0f), FRotator::ZeroRotator, FVector(0.14f, 0.12f, 0.32f));
	BHConfigureAvatarPart(RightLowerArmMesh, Cube, Material, FVector(0.0f, 0.0f, -15.0f), FRotator::ZeroRotator, FVector(0.12f, 0.10f, 0.30f));
	BHConfigureAvatarPart(RightHandMesh, Cube, Material, FVector(1.0f, 0.0f, -31.0f), FRotator::ZeroRotator, FVector(0.14f, 0.12f, 0.09f));
	BHConfigureAvatarPart(LeftUpperLegMesh, Cube, Material, FVector(0.0f, 0.0f, -14.0f), FRotator::ZeroRotator, FVector(0.18f, 0.13f, 0.28f));
	BHConfigureAvatarPart(LeftLowerLegMesh, Cube, Material, FVector(0.0f, 0.0f, -14.0f), FRotator::ZeroRotator, FVector(0.16f, 0.12f, 0.28f));
	BHConfigureAvatarPart(LeftFootMesh, Cube, Material, FVector(10.0f, 0.0f, -29.0f), FRotator::ZeroRotator, FVector(0.28f, 0.13f, 0.08f));
	BHConfigureAvatarPart(RightUpperLegMesh, Cube, Material, FVector(0.0f, 0.0f, -14.0f), FRotator::ZeroRotator, FVector(0.18f, 0.13f, 0.28f));
	BHConfigureAvatarPart(RightLowerLegMesh, Cube, Material, FVector(0.0f, 0.0f, -14.0f), FRotator::ZeroRotator, FVector(0.16f, 0.12f, 0.28f));
	BHConfigureAvatarPart(RightFootMesh, Cube, Material, FVector(10.0f, 0.0f, -29.0f), FRotator::ZeroRotator, FVector(0.28f, 0.13f, 0.08f));
	BHConfigureAvatarPart(BackpackMesh, Cube, Material, FVector(-25.0f, 0.0f, -4.0f), FRotator::ZeroRotator, FVector(0.10f, 0.24f, 0.50f));
	BHConfigureAvatarPart(RoleBadgeMesh, Cube, Material, FVector(24.0f, -11.0f, 15.0f), FRotator::ZeroRotator, FVector(0.016f, 0.06f, 0.07f));
	BHConfigureAvatarPart(RoleHeadwearMesh, Cylinder, Material, FVector(7.0f, 0.0f, 76.0f), FRotator::ZeroRotator, FVector(0.28f, 0.28f, 0.08f));
	BHConfigureAvatarPart(RoleHeadwearAccentMesh, Cube, Material, FVector(22.0f, 0.0f, 68.0f), FRotator::ZeroRotator, FVector(0.11f, 0.28f, 0.025f));
	BHConfigureAvatarPart(RoleHeadwearDetailMesh, Cube, Material, FVector(6.0f, 0.0f, 78.0f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 0.025f));
	BHConfigureAvatarPart(RoleGearMesh, Cube, Material, FVector(-26.0f, 0.0f, 10.0f), FRotator::ZeroRotator, FVector(0.17f, 0.31f, 0.42f));
	BHConfigureAvatarPart(RoleGearAccentMesh, Cube, Material, FVector(-10.0f, 0.0f, 34.0f), FRotator::ZeroRotator, FVector(0.055f, 0.32f, 0.075f));
	BHConfigureAvatarPart(RoleGearLeftStrapMesh, Cube, Material, FVector(9.0f, -15.0f, 18.0f), FRotator::ZeroRotator, FVector(0.035f, 0.022f, 0.32f));
	BHConfigureAvatarPart(RoleGearRightStrapMesh, Cube, Material, FVector(9.0f, 15.0f, 18.0f), FRotator::ZeroRotator, FVector(0.035f, 0.022f, 0.32f));
	BHConfigureAvatarPart(RoleGearDetailMesh, Cube, Material, FVector(14.0f, 0.0f, 16.0f), FRotator::ZeroRotator, FVector(0.02f, 0.09f, 0.10f));

	UStaticMeshComponent* RoleAccessoryParts[] = {
		RoleHeadwearMesh,
		RoleHeadwearAccentMesh,
		RoleHeadwearDetailMesh,
		RoleGearMesh,
		RoleGearAccentMesh,
		RoleGearLeftStrapMesh,
		RoleGearRightStrapMesh,
		RoleGearDetailMesh
	};
	for (UStaticMeshComponent* Part : RoleAccessoryParts)
	{
		BHPropVisuals::SetPartVisible(Part, false);
	}
}

void ABHCharacter::UpdateLowPolyAvatar(float DeltaSeconds)
{
	if (!AvatarRoot)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const float CurrentWalkSpeed = BHRoleWalkSpeed(BHPS, WalkSpeed);
	const float Speed2D = GetVelocity().Size2D();
	const float MaxSpeed = Movement ? FMath::Max(1.0f, Movement->MaxWalkSpeed) : FMath::Max(1.0f, CurrentWalkSpeed);
	const bool bGrounded = Movement && Movement->IsMovingOnGround();
	const float MoveTarget = (CanAct() && bGrounded) ? FMath::Clamp(Speed2D / MaxSpeed, 0.0f, 1.0f) : 0.0f;
	const float SprintTarget = (CanAct() && MaxSpeed > CurrentWalkSpeed + 25.0f && Speed2D > CurrentWalkSpeed * 0.55f) ? 1.0f : 0.0f;
	const float HorrorAlpha = FMath::Clamp(FMath::Max(Fear, Dread) / 100.0f, 0.0f, 1.0f);

	AvatarMoveAlpha = FMath::FInterpTo(AvatarMoveAlpha, MoveTarget, DeltaSeconds, 9.0f);
	AvatarSprintAlpha = FMath::FInterpTo(AvatarSprintAlpha, SprintTarget, DeltaSeconds, 7.5f);
	AvatarBreathTime += DeltaSeconds * FMath::Lerp(1.25f, 2.25f, HorrorAlpha);
	UpdateRoleSkeletalAnimation(Speed2D, AvatarMoveAlpha, AvatarSprintAlpha, bGrounded);
	if (AvatarMoveAlpha > 0.025f && bGrounded)
	{
		const float StrideFrequency = FMath::Lerp(5.4f, 9.6f, AvatarSprintAlpha);
		AvatarAnimTime += DeltaSeconds * StrideFrequency * FMath::Lerp(0.65f, 1.12f, AvatarMoveAlpha);
	}

	float Strafe = 0.0f;
	if (Speed2D > 4.0f)
	{
		Strafe = FVector::DotProduct(GetVelocity().GetSafeNormal2D(), GetActorRightVector()) * AvatarMoveAlpha;
	}

	const float CrouchAlpha = bIsCrouched ? 1.0f : 0.0f;
	const float Step = FMath::Sin(AvatarAnimTime);
	const float CounterStep = -Step;
	const float MoveAlpha = FMath::Clamp(AvatarMoveAlpha, 0.0f, 1.0f);
	const float SprintAlpha = FMath::Clamp(AvatarSprintAlpha, 0.0f, 1.0f);
	const float BobZ = FMath::Abs(FMath::Sin(AvatarAnimTime * 2.0f)) * FMath::Lerp(0.6f, 2.8f, SprintAlpha) * MoveAlpha;
	const float Breath = FMath::Sin(AvatarBreathTime) * FMath::Lerp(0.9f, 1.8f, HorrorAlpha);

	AvatarRoot->SetRelativeLocation(FVector(0.0f, 0.0f, BobZ - CrouchAlpha * 14.0f));

	const FRotator TorsoRotation(
		-FMath::Lerp(0.0f, 7.0f, SprintAlpha) * MoveAlpha - CrouchAlpha * 7.0f + Breath * 0.45f,
		0.0f,
		Strafe * -5.5f);
	if (RoleModelRoot)
	{
		RoleModelRoot->SetRelativeLocation(FVector(0.0f, 0.0f, -CrouchAlpha * 4.0f));
		RoleModelRoot->SetRelativeRotation(FRotator(
			TorsoRotation.Pitch * 0.72f,
			Step * MoveAlpha * FMath::Lerp(1.2f, 2.8f, SprintAlpha),
			TorsoRotation.Roll * 0.62f));
	}
	if (BodyMesh)
	{
		BodyMesh->SetRelativeRotation(TorsoRotation);
	}
	if (ChestMesh)
	{
		ChestMesh->SetRelativeRotation(TorsoRotation);
	}
	if (WaistMesh)
	{
		WaistMesh->SetRelativeRotation(FRotator(CrouchAlpha * 4.0f, 0.0f, Strafe * -3.0f));
	}
	if (BackpackMesh)
	{
		BackpackMesh->SetRelativeRotation(TorsoRotation);
	}
	if (RoleBadgeMesh)
	{
		RoleBadgeMesh->SetRelativeRotation(TorsoRotation);
	}

	const FRotator AimRotation = GetBaseAimRotation();
	const float AimPitch = FMath::Clamp(FRotator::NormalizeAxis(AimRotation.Pitch), -42.0f, 42.0f);
	const float AimYaw = FMath::Clamp(FRotator::NormalizeAxis(AimRotation.Yaw - GetActorRotation().Yaw), -55.0f, 55.0f);
	if (HeadJoint)
	{
		HeadJoint->SetRelativeRotation(FRotator(
			AimPitch * 0.42f + Breath * 0.35f - CrouchAlpha * 3.0f,
			AimYaw * 0.55f,
			Step * MoveAlpha * 1.8f));
	}

	const float ArmSwing = FMath::Lerp(18.0f, 48.0f, SprintAlpha) * MoveAlpha;
	const float ArmRoll = FMath::Lerp(5.0f, 13.0f, SprintAlpha) * MoveAlpha;
	const float ElbowSwing = FMath::Lerp(8.0f, 24.0f, SprintAlpha) * MoveAlpha;
	if (LeftShoulderJoint)
	{
		LeftShoulderJoint->SetRelativeRotation(FRotator(CounterStep * ArmSwing - CrouchAlpha * 10.0f, 0.0f, -ArmRoll));
	}
	if (RightShoulderJoint)
	{
		RightShoulderJoint->SetRelativeRotation(FRotator(Step * ArmSwing - CrouchAlpha * 10.0f, 0.0f, ArmRoll));
	}
	if (LeftElbowJoint)
	{
		LeftElbowJoint->SetRelativeRotation(FRotator(6.0f + FMath::Max(0.0f, Step) * ElbowSwing + CrouchAlpha * 8.0f, 0.0f, 0.0f));
	}
	if (RightElbowJoint)
	{
		RightElbowJoint->SetRelativeRotation(FRotator(6.0f + FMath::Max(0.0f, CounterStep) * ElbowSwing + CrouchAlpha * 8.0f, 0.0f, 0.0f));
	}

	const float LegSwing = FMath::Lerp(15.0f, 34.0f, SprintAlpha) * MoveAlpha;
	const float KneeSwing = FMath::Lerp(10.0f, 32.0f, SprintAlpha) * MoveAlpha;
	if (LeftHipJoint)
	{
		LeftHipJoint->SetRelativeRotation(FRotator(Step * LegSwing + CrouchAlpha * 23.0f, 0.0f, -Strafe * 4.0f));
	}
	if (RightHipJoint)
	{
		RightHipJoint->SetRelativeRotation(FRotator(CounterStep * LegSwing + CrouchAlpha * 23.0f, 0.0f, -Strafe * 4.0f));
	}
	if (LeftKneeJoint)
	{
		LeftKneeJoint->SetRelativeRotation(FRotator(FMath::Max(0.0f, CounterStep) * KneeSwing + CrouchAlpha * 18.0f, 0.0f, 0.0f));
	}
	if (RightKneeJoint)
	{
		RightKneeJoint->SetRelativeRotation(FRotator(FMath::Max(0.0f, Step) * KneeSwing + CrouchAlpha * 18.0f, 0.0f, 0.0f));
	}
	if (LeftFootMesh)
	{
		LeftFootMesh->SetRelativeRotation(FRotator(FMath::Clamp(CounterStep * 12.0f * MoveAlpha, -12.0f, 14.0f) - CrouchAlpha * 6.0f, 0.0f, 0.0f));
	}
	if (RightFootMesh)
	{
		RightFootMesh->SetRelativeRotation(FRotator(FMath::Clamp(Step * 12.0f * MoveAlpha, -12.0f, 14.0f) - CrouchAlpha * 6.0f, 0.0f, 0.0f));
	}
}

void ABHCharacter::UpdateRoleSkeletalAnimation(float Speed2D, float MoveAlpha, float SprintAlpha, bool bGrounded)
{
	if (!bUsingRoleModel || !RoleSkeletalMesh || !RoleSkeletalMesh->IsVisible())
	{
		return;
	}

	FName DesiredAnimation(TEXT("Idle"));
	float DesiredPlayRate = 1.0f;
	if (CanAct() && bGrounded && Speed2D > 35.0f && MoveAlpha > 0.08f)
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const float CurrentWalkSpeed = BHRoleWalkSpeed(BHPS, WalkSpeed);
		const float CurrentSprintSpeed = BHRoleSprintSpeed(BHPS, SprintSpeed);
		const bool bRunning = SprintAlpha > 0.35f || Speed2D > CurrentWalkSpeed * 1.08f;
		DesiredAnimation = bRunning ? FName(TEXT("Run")) : FName(TEXT("Walk"));
		DesiredPlayRate = bRunning
			? FMath::GetMappedRangeValueClamped(FVector2D(CurrentWalkSpeed, CurrentSprintSpeed), FVector2D(0.9f, 1.2f), Speed2D)
			: FMath::GetMappedRangeValueClamped(FVector2D(70.0f, CurrentWalkSpeed), FVector2D(0.75f, 1.08f), Speed2D);
	}

	if (DesiredAnimation != LastRoleAnimationName)
	{
		if (UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, BHQuaterniusAnimationPath(DesiredAnimation)))
		{
			RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			RoleSkeletalMesh->PlayAnimation(Animation, true);
			LastRoleAnimationName = DesiredAnimation;
		}
	}
	RoleSkeletalMesh->SetPlayRate(DesiredPlayRate);
}

void ABHCharacter::SetLowPolyAvatarVisible(bool bVisible)
{
	UStaticMeshComponent* Parts[] = {
		BodyMesh,
		ChestMesh,
		WaistMesh,
		HeadMesh,
		HairMesh,
		LeftEyeMesh,
		RightEyeMesh,
		MouthMesh,
		LeftUpperArmMesh,
		LeftLowerArmMesh,
		LeftHandMesh,
		RightUpperArmMesh,
		RightLowerArmMesh,
		RightHandMesh,
		LeftUpperLegMesh,
		LeftLowerLegMesh,
		LeftFootMesh,
		RightUpperLegMesh,
		RightLowerLegMesh,
		RightFootMesh,
		BackpackMesh,
		RoleBadgeMesh
	};

	for (UStaticMeshComponent* Part : Parts)
	{
		BHPropVisuals::SetPartVisible(Part, bVisible);
	}
}

void ABHCharacter::ApplyRoleModelVisuals(const ABHPlayerState* BHPS, const FLinearColor& ShirtColor, const FLinearColor& SkinColor)
{
	const bool bUseHunterModel = BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter;
	bool bAppliedRoleModel = false;
	bool bAppliedSkeletalModel = false;
	const FVector RoleModelFeetOffset = BHRoleModelFeetAtCapsuleBaseOffset(GetCapsuleComponent(), AvatarRoot);

	if (RoleModelRoot)
	{
		RoleModelRoot->SetRelativeScale3D(FVector::OneVector);
	}

	if (RoleSkeletalMesh)
	{
		if (USkeletalMesh* RoleMesh = LoadObject<USkeletalMesh>(nullptr, BHSelectQuaterniusMeshPath(BHPS)))
		{
			RoleSkeletalMesh->SetSkeletalMeshAsset(RoleMesh);
			RoleSkeletalMesh->EmptyOverrideMaterials();
			RoleSkeletalMesh->SetRelativeLocation(RoleModelFeetOffset);
			RoleSkeletalMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
			RoleSkeletalMesh->SetRelativeScale3D(FVector(1.0f));
			RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			BHApplyQuaterniusPalette(RoleSkeletalMesh, ShirtColor);
			LastRoleAnimationName = NAME_None;
			bAppliedRoleModel = true;
			bAppliedSkeletalModel = true;
		}
	}

	if (!bAppliedRoleModel && bUseHunterModel && RoleSkeletalMesh)
	{
		if (USkeletalMesh* HunterMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/BlackoutHunt/Art/Characters/Hunter/SK_BH_Hunter.SK_BH_Hunter")))
		{
			RoleSkeletalMesh->SetSkeletalMeshAsset(HunterMesh);
			RoleSkeletalMesh->EmptyOverrideMaterials();
			RoleSkeletalMesh->SetRelativeLocation(RoleModelFeetOffset);
			RoleSkeletalMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
			RoleSkeletalMesh->SetRelativeScale3D(FVector(0.11f));
			RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (UMaterialInterface* HunterMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BlackoutHunt/Art/Characters/Hunter/M_BH_Hunter.M_BH_Hunter")))
			{
				BHApplyRoleModelMaterial(RoleSkeletalMesh, HunterMaterial, FLinearColor(0.27f, 0.026f, 0.020f, 1.0f), 0.18f);
			}
			bAppliedRoleModel = true;
			bAppliedSkeletalModel = true;
			LastRoleAnimationName = NAME_None;
		}
	}
	else if (!bAppliedRoleModel && RoleStaticMesh)
	{
		if (UStaticMesh* HiderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/BlackoutHunt/Art/Characters/Hider/SM_BH_Hider.SM_BH_Hider")))
		{
			RoleStaticMesh->SetStaticMesh(HiderMesh);
			RoleStaticMesh->SetRelativeLocation(RoleModelFeetOffset);
			RoleStaticMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
			RoleStaticMesh->SetRelativeScale3D(FVector(0.43f));
			const FLinearColor HiderTint = BHAvatarColorLerp(SkinColor, ShirtColor, 0.18f);
			if (UMaterialInterface* HiderMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BlackoutHunt/Art/Characters/Hider/M_BH_Hider.M_BH_Hider")))
			{
				BHApplyRoleModelMaterial(RoleStaticMesh, HiderMaterial, HiderTint, 0.0f);
			}
			bAppliedRoleModel = true;
		}
	}

	BHSetRoleMeshVisible(RoleSkeletalMesh, bAppliedRoleModel && bAppliedSkeletalModel);
	BHSetRoleMeshVisible(RoleStaticMesh, bAppliedRoleModel && !bAppliedSkeletalModel);
	bUsingRoleModel = bAppliedRoleModel;
	SetLowPolyAvatarVisible(!bAppliedRoleModel);
}

void ABHCharacter::UpdateViewFeel(float DeltaSeconds)
{
	if (!Camera)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const float CurrentWalkSpeed = BHRoleWalkSpeed(BHPS, WalkSpeed);
	const float Speed2D = GetVelocity().Size2D();
	const float MaxSpeed = Movement ? FMath::Max(1.0f, Movement->MaxWalkSpeed) : FMath::Max(1.0f, CurrentWalkSpeed);
	const bool bGrounded = Movement && Movement->IsMovingOnGround();
	const float MoveTarget = (CanAct() && bGrounded) ? FMath::Clamp(Speed2D / MaxSpeed, 0.0f, 1.0f) : 0.0f;
	const float SprintTarget = (CanAct() && MaxSpeed > CurrentWalkSpeed + 25.0f && Speed2D > CurrentWalkSpeed * 0.55f) ? 1.0f : 0.0f;

	SmoothedMoveAlpha = FMath::FInterpTo(SmoothedMoveAlpha, MoveTarget, DeltaSeconds, 8.5f);
	SmoothedSprintAlpha = FMath::FInterpTo(SmoothedSprintAlpha, SprintTarget, DeltaSeconds, 7.0f);

	float StrafeTarget = 0.0f;
	if (Speed2D > 4.0f)
	{
		StrafeTarget = FVector::DotProduct(GetVelocity().GetSafeNormal2D(), GetActorRightVector()) * MoveTarget;
	}
	SmoothedStrafeAlpha = FMath::FInterpTo(SmoothedStrafeAlpha, StrafeTarget, DeltaSeconds, 7.5f);

	if (SmoothedMoveAlpha > 0.025f && bGrounded)
	{
		const float BobFrequency = FMath::Lerp(6.4f, 10.8f, SmoothedSprintAlpha);
		CameraBobTime += DeltaSeconds * BobFrequency * FMath::Lerp(0.7f, 1.15f, SmoothedMoveAlpha);
	}

	const float HorrorAlpha = FMath::Clamp(FMath::Max(Fear, Dread) / 100.0f, 0.0f, 1.0f);
	const float BobScale = SmoothedMoveAlpha * (bIsCrouched ? 0.44f : 1.0f);
	const float BobZ = FMath::Sin(CameraBobTime * 2.0f) * FMath::Lerp(0.8f, 1.85f, SmoothedSprintAlpha) * BobScale;
	const float BobY = FMath::Sin(CameraBobTime) * FMath::Lerp(0.45f, 1.05f, SmoothedSprintAlpha) * BobScale;
	const float StressTremor = (FMath::Sin(FlashlightPulseTime * 13.0f) + FMath::Sin(FlashlightPulseTime * 19.7f) * 0.45f) * HorrorAlpha * 0.42f;
	const float CrouchOffset = bIsCrouched ? -14.0f : 0.0f;
	const FVector TargetCameraLocation = DefaultCameraLocation
		+ FVector(0.0f, BobY - SmoothedStrafeAlpha * 1.65f, CrouchOffset + BobZ + StressTremor);

	Camera->SetRelativeLocation(FMath::VInterpTo(Camera->GetRelativeLocation(), TargetCameraLocation, DeltaSeconds, bHiddenInLocker ? 4.5f : 11.5f));

	const float HiddenFOVPenalty = bHiddenInLocker ? 3.5f : 0.0f;
	const float ExhaustionThreshold = MaxStamina * 0.22f;
	const float ExhaustionAlpha = ExhaustionThreshold > 0.0f ? FMath::Clamp((ExhaustionThreshold - Stamina) / ExhaustionThreshold, 0.0f, 1.0f) : 0.0f;
	const float DesiredFOV = DefaultCameraFOV + SmoothedSprintAlpha * 4.8f - HiddenFOVPenalty - HorrorAlpha * 6.8f - ExhaustionAlpha * 1.4f;
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, DesiredFOV, DeltaSeconds, 4.2f));
}

void ABHCharacter::UpdateFlashlightFeel(float DeltaSeconds)
{
	if (!Flashlight)
	{
		return;
	}

	FlashlightPulseTime += DeltaSeconds;
	const bool bShouldBeVisible = bFlashlightOn && FlashlightBattery > 0.0f && !bHiddenInLocker && !bOutOfPlay;
	const auto SetBeamEnabled = [](UMeshComponent* Beam, bool bVisible)
	{
		if (!Beam)
		{
			return;
		}

		Beam->SetHiddenInGame(!bVisible);
		Beam->SetVisibility(bVisible, true);
	};

	Flashlight->SetVisibility(bShouldBeVisible);
	SetBeamEnabled(FlashlightBeamOuter, bShouldBeVisible);
	SetBeamEnabled(FlashlightBeamCore, bShouldBeVisible);
	if (!bShouldBeVisible)
	{
		return;
	}

	const float BatteryAlpha = FMath::Clamp(FlashlightBattery / 100.0f, 0.0f, 1.0f);
	const float LowBatteryAlpha = FMath::Clamp((32.0f - FlashlightBattery) / 32.0f, 0.0f, 1.0f);
	const float HorrorAlpha = FMath::Clamp(FMath::Max(Fear, Dread) / 100.0f, 0.0f, 1.0f);
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const EBHFogPreset ActiveFogPreset = BHGS ? BHGS->ActiveFogPreset : EBHFogPreset::Heavy;
	const bool bFoggroundsActive = BHGS && BHGS->ActiveLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	const bool bExtremeFog = bFoggroundsActive && ActiveFogPreset == EBHFogPreset::Extreme;
	const bool bHeavyFog = bFoggroundsActive && ActiveFogPreset == EBHFogPreset::Heavy;

	const FVector StableEyeLocation = GetActorLocation() + FVector(0.0f, 0.0f, DefaultCameraLocation.Z + (bIsCrouched ? -14.0f : 0.0f));
	const FRotator StableViewRotation = Controller ? Controller->GetControlRotation() : (Camera ? Camera->GetComponentRotation() : GetActorRotation());
	Flashlight->SetWorldLocationAndRotation(StableEyeLocation, StableViewRotation);

	const float UnevenPulse = 1.0f
		+ FMath::Sin(FlashlightPulseTime * FMath::Lerp(5.8f, 13.0f, LowBatteryAlpha)) * (0.018f + LowBatteryAlpha * 0.045f)
		+ FMath::Sin(FlashlightPulseTime * 23.0f) * (HorrorAlpha * 0.018f);
	const float Cutout = 1.0f - LowBatteryAlpha * (0.09f + 0.12f * FMath::Square(FMath::Max(0.0f, FMath::Sin(FlashlightPulseTime * 37.0f))));
	const float NormalIntensity = FMath::Clamp(9200.0f * FMath::Lerp(0.52f, 1.0f, BatteryAlpha) * UnevenPulse * Cutout, 1800.0f, 11500.0f);
	const float NormalRadius = FMath::Lerp(1300.0f, 2200.0f, BatteryAlpha) * FMath::Lerp(1.0f, 0.94f, HorrorAlpha);

	const float EffectiveIntensity = bExtremeFog ? FMath::Min(NormalIntensity, 9500.0f) : (bHeavyFog ? FMath::Min(NormalIntensity, 8600.0f) : NormalIntensity);
	const float EffectiveRadius = bExtremeFog ? FMath::Min(NormalRadius, 650.0f) : (bHeavyFog ? FMath::Min(NormalRadius, 950.0f) : NormalRadius);
	const float EffectiveInnerConeAngle = bExtremeFog ? 16.0f : (bHeavyFog ? 16.0f : 18.0f);
	const float EffectiveOuterConeAngle = bExtremeFog ? 30.0f : (bHeavyFog ? 32.0f : (34.0f + LowBatteryAlpha * 2.5f + HorrorAlpha * 1.8f));

	Flashlight->SetIntensity(EffectiveIntensity);
	Flashlight->SetAttenuationRadius(EffectiveRadius);
	Flashlight->SetInnerConeAngle(EffectiveInnerConeAngle);
	Flashlight->SetOuterConeAngle(EffectiveOuterConeAngle);
	Flashlight->SetVolumetricScatteringIntensity(bExtremeFog ? 20.0f : (bHeavyFog ? 18.0f : 8.0f));

	const float FogScatterAlpha =
		ActiveFogPreset == EBHFogPreset::Extreme ? 0.80f :
		ActiveFogPreset == EBHFogPreset::Heavy ? 0.72f :
		0.42f;
	const float BeamLength = bExtremeFog ? 620.0f : (bHeavyFog ? 850.0f : FMath::Clamp(EffectiveRadius * 0.74f, 760.0f, 1380.0f));
	const float BeamPulse = FMath::Clamp(UnevenPulse * Cutout, 0.55f, 1.28f);
	const FLinearColor BeamTint(0.76f, 0.86f, 0.80f, 1.0f);

	const auto ConfigureBeam = [BeamTint](UMeshComponent* Beam, UMaterialInstanceDynamic* Material, float Length, float ConeAngle, float RadiusScale, float Opacity, float Brightness)
	{
		if (!Beam)
		{
			return;
		}

		const float BaseRadius = FMath::Max(8.0f, FMath::Tan(FMath::DegreesToRadians(ConeAngle)) * Length * RadiusScale);
		Beam->SetRelativeLocation(FVector(Length * 0.5f + 14.0f, 0.0f, 0.0f));
		Beam->SetRelativeRotation(FRotator::ZeroRotator);
		Beam->SetRelativeScale3D(FVector(Length / 100.0f, BaseRadius / 85.0f, BaseRadius / 85.0f));

		if (Material)
		{
			const FLinearColor BeamColor(BeamTint.R, BeamTint.G, BeamTint.B, Opacity);
			Material->SetVectorParameterValue(TEXT("Color"), BeamColor);
			Material->SetVectorParameterValue(TEXT("BaseColor"), BeamColor);
			Material->SetVectorParameterValue(TEXT("Tint"), BeamColor);
			Material->SetVectorParameterValue(TEXT("TintColor"), BeamColor);
			Material->SetVectorParameterValue(TEXT("FogColor"), BeamColor);
			Material->SetVectorParameterValue(TEXT("BeamColor"), BeamColor);
			Material->SetVectorParameterValue(TEXT("LightColor"), BeamColor);
			Material->SetVectorParameterValue(TEXT("EmissiveColor"), BeamTint * Brightness);
			Material->SetVectorParameterValue(TEXT("Emissive"), BeamTint * Brightness);
			Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			Material->SetScalarParameterValue(TEXT("Alpha"), Opacity);
			Material->SetScalarParameterValue(TEXT("Density"), Opacity * 0.85f);
			Material->SetScalarParameterValue(TEXT("Brightness"), Brightness);
			Material->SetScalarParameterValue(TEXT("Intensity"), Brightness);
			Material->SetScalarParameterValue(TEXT("Falloff"), 3.4f);
			Material->SetScalarParameterValue(TEXT("Softness"), 0.72f);
		}
	};

	const float OuterOpacity = FMath::Clamp((0.014f + FogScatterAlpha * 0.032f + LowBatteryAlpha * 0.006f) * BeamPulse, 0.012f, bExtremeFog ? 0.060f : (bHeavyFog ? 0.065f : 0.105f));
	const float CoreOpacity = FMath::Clamp((0.010f + FogScatterAlpha * 0.021f) * BeamPulse, 0.009f, bExtremeFog ? 0.040f : (bHeavyFog ? 0.048f : 0.072f));
	ConfigureBeam(FlashlightBeamOuter, FlashlightBeamOuterMaterial, BeamLength, EffectiveOuterConeAngle, 1.0f, OuterOpacity, bExtremeFog ? 1.20f : (bHeavyFog ? 1.25f : 0.75f));
	ConfigureBeam(FlashlightBeamCore, FlashlightBeamCoreMaterial, BeamLength * 0.82f, EffectiveInnerConeAngle, 0.50f, CoreOpacity, bExtremeFog ? 1.45f : (bHeavyFog ? 1.55f : 1.00f));
}

void ABHCharacter::ApplyFlashlightState()
{
	const bool bShouldBeVisible = bFlashlightOn && FlashlightBattery > 0.0f && !bHiddenInLocker && !bOutOfPlay;
	if (Flashlight)
	{
		Flashlight->SetVisibility(bShouldBeVisible);
	}
	if (FlashlightBeamOuter)
	{
		FlashlightBeamOuter->SetHiddenInGame(!bShouldBeVisible);
		FlashlightBeamOuter->SetVisibility(bShouldBeVisible, true);
	}
	if (FlashlightBeamCore)
	{
		FlashlightBeamCore->SetHiddenInGame(!bShouldBeVisible);
		FlashlightBeamCore->SetVisibility(bShouldBeVisible, true);
	}
}

void ABHCharacter::ApplyHiddenState()
{
	if (bHiddenInLocker || bOutOfPlay)
	{
		SetActorHiddenInGame(true);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetCharacterMovement()->DisableMovement();
	}
	else
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const bool bAlive = !BHPS || BHPS->LifeState == EBHPlayerLifeState::Alive;
		SetActorHiddenInGame(!bAlive);
		GetCapsuleComponent()->SetCollisionEnabled(bAlive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bAlive)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}
}

void ABHCharacter::ApplyAvatarStyle()
{
	if (!AvatarRoot || !BodyMesh)
	{
		return;
	}

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const int32 AvatarIndex = BHPS ? FMath::Abs(BHPS->AvatarIndex) : 0;
	FLinearColor ShirtColor = BHPS ? BHPS->AvatarColor : FLinearColor(0.22f, 0.58f, 0.74f, 1.0f);
	static const FVector AvatarScales[] = {
		FVector(1.00f, 1.00f, 1.00f),
		FVector(0.94f, 1.05f, 0.96f),
		FVector(1.08f, 0.96f, 1.06f),
		FVector(1.00f, 1.00f, 0.93f),
		FVector(0.95f, 0.95f, 1.12f),
		FVector(1.11f, 1.06f, 1.01f),
		FVector(0.91f, 1.02f, 1.07f),
		FVector(1.04f, 0.93f, 0.98f)
	};

	static const FLinearColor SkinTones[] = {
		FLinearColor(0.82f, 0.58f, 0.42f, 1.0f),
		FLinearColor(0.92f, 0.72f, 0.56f, 1.0f),
		FLinearColor(0.58f, 0.38f, 0.28f, 1.0f),
		FLinearColor(0.74f, 0.49f, 0.34f, 1.0f),
		FLinearColor(0.96f, 0.80f, 0.64f, 1.0f),
		FLinearColor(0.47f, 0.30f, 0.22f, 1.0f),
		FLinearColor(0.88f, 0.65f, 0.48f, 1.0f),
		FLinearColor(0.67f, 0.44f, 0.32f, 1.0f)
	};
	static const FLinearColor HairTones[] = {
		FLinearColor(0.06f, 0.045f, 0.035f, 1.0f),
		FLinearColor(0.16f, 0.10f, 0.055f, 1.0f),
		FLinearColor(0.32f, 0.22f, 0.12f, 1.0f),
		FLinearColor(0.08f, 0.075f, 0.07f, 1.0f),
		FLinearColor(0.42f, 0.35f, 0.24f, 1.0f),
		FLinearColor(0.18f, 0.12f, 0.09f, 1.0f),
		FLinearColor(0.55f, 0.46f, 0.34f, 1.0f),
		FLinearColor(0.10f, 0.09f, 0.12f, 1.0f)
	};

	FLinearColor SkinColor = SkinTones[AvatarIndex % UE_ARRAY_COUNT(SkinTones)];
	FLinearColor HairColor = HairTones[AvatarIndex % UE_ARRAY_COUNT(HairTones)];
	FLinearColor PantsColor = BHAvatarColorLerp(ShirtColor, FLinearColor(0.035f, 0.04f, 0.048f, 1.0f), 0.62f);
	FLinearColor ChestColor = BHAvatarColorScale(ShirtColor, 1.12f);
	FLinearColor ShoeColor(0.025f, 0.027f, 0.030f, 1.0f);
	FLinearColor BackpackColor(0.08f, 0.09f, 0.10f, 1.0f);
	FLinearColor BadgeColor = BHAvatarColorLerp(ShirtColor, FLinearColor::White, 0.34f);
	float BadgeEmissive = 0.05f;
	FVector AvatarScale = AvatarScales[AvatarIndex % UE_ARRAY_COUNT(AvatarScales)];

	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		ShirtColor = FLinearColor(0.36f, 0.045f, 0.035f, 1.0f);
		ChestColor = FLinearColor(0.62f, 0.10f, 0.08f, 1.0f);
		PantsColor = FLinearColor(0.025f, 0.025f, 0.030f, 1.0f);
		SkinColor = BHAvatarColorLerp(SkinColor, FLinearColor(0.82f, 0.80f, 0.76f, 1.0f), 0.42f);
		HairColor = FLinearColor(0.025f, 0.020f, 0.018f, 1.0f);
		BackpackColor = FLinearColor(0.09f, 0.025f, 0.025f, 1.0f);
		BadgeColor = FLinearColor(1.0f, 0.08f, 0.04f, 1.0f);
		BadgeEmissive = 1.1f;
		AvatarScale *= FVector(1.06f, 1.06f, 1.08f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		ShirtColor = FLinearColor(0.45f, 0.19f, 0.06f, 1.0f);
		ChestColor = FLinearColor(0.72f, 0.34f, 0.10f, 1.0f);
		PantsColor = FLinearColor(0.06f, 0.055f, 0.045f, 1.0f);
		BackpackColor = FLinearColor(0.13f, 0.08f, 0.045f, 1.0f);
		BadgeColor = FLinearColor(1.0f, 0.52f, 0.08f, 1.0f);
		BadgeEmissive = 0.55f;
		AvatarScale *= FVector(1.03f, 1.03f, 1.02f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		ShirtColor = FLinearColor(0.08f, 0.42f, 0.48f, 1.0f);
		ChestColor = FLinearColor(0.12f, 0.72f, 0.78f, 1.0f);
		PantsColor = FLinearColor(0.025f, 0.075f, 0.09f, 1.0f);
		SkinColor = BHAvatarColorLerp(SkinColor, FLinearColor(0.78f, 0.96f, 0.98f, 1.0f), 0.16f);
		BackpackColor = FLinearColor(0.03f, 0.12f, 0.14f, 1.0f);
		BadgeColor = FLinearColor(0.45f, 1.0f, 0.95f, 1.0f);
		BadgeEmissive = 1.35f;
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		ShirtColor = FLinearColor(0.14f, 0.14f, 0.16f, 1.0f);
		ChestColor = FLinearColor(0.20f, 0.20f, 0.23f, 1.0f);
		PantsColor = FLinearColor(0.07f, 0.07f, 0.08f, 1.0f);
		SkinColor = FLinearColor(0.28f, 0.28f, 0.30f, 1.0f);
		HairColor = FLinearColor(0.08f, 0.08f, 0.09f, 1.0f);
		BackpackColor = FLinearColor(0.05f, 0.05f, 0.055f, 1.0f);
		BadgeColor = FLinearColor(0.24f, 0.24f, 0.28f, 1.0f);
		AvatarScale *= FVector(0.98f, 0.98f, 0.98f);
	}

	AvatarRoot->SetRelativeScale3D(AvatarScale);

	BHPropVisuals::TintPart(BodyMesh, ShirtColor, 0.0f);
	BHPropVisuals::TintPart(ChestMesh, ChestColor, 0.0f);
	BHPropVisuals::TintPart(WaistMesh, PantsColor, 0.0f);
	BHPropVisuals::TintPart(HeadMesh, SkinColor, 0.0f);
	BHPropVisuals::TintPart(HairMesh, HairColor, 0.0f);
	BHPropVisuals::TintPart(LeftEyeMesh, FLinearColor(0.015f, 0.018f, 0.020f, 1.0f), 0.0f);
	BHPropVisuals::TintPart(RightEyeMesh, FLinearColor(0.015f, 0.018f, 0.020f, 1.0f), 0.0f);
	BHPropVisuals::TintPart(MouthMesh, FLinearColor(0.08f, 0.035f, 0.030f, 1.0f), 0.0f);
	BHPropVisuals::TintPart(LeftUpperArmMesh, ShirtColor, 0.0f);
	BHPropVisuals::TintPart(RightUpperArmMesh, ShirtColor, 0.0f);
	BHPropVisuals::TintPart(LeftLowerArmMesh, SkinColor, 0.0f);
	BHPropVisuals::TintPart(RightLowerArmMesh, SkinColor, 0.0f);
	BHPropVisuals::TintPart(LeftHandMesh, SkinColor, 0.0f);
	BHPropVisuals::TintPart(RightHandMesh, SkinColor, 0.0f);
	BHPropVisuals::TintPart(LeftUpperLegMesh, PantsColor, 0.0f);
	BHPropVisuals::TintPart(RightUpperLegMesh, PantsColor, 0.0f);
	BHPropVisuals::TintPart(LeftLowerLegMesh, BHAvatarColorScale(PantsColor, 0.82f), 0.0f);
	BHPropVisuals::TintPart(RightLowerLegMesh, BHAvatarColorScale(PantsColor, 0.82f), 0.0f);
	BHPropVisuals::TintPart(LeftFootMesh, ShoeColor, 0.0f);
	BHPropVisuals::TintPart(RightFootMesh, ShoeColor, 0.0f);
	BHPropVisuals::TintPart(BackpackMesh, BackpackColor, 0.0f);
	BHPropVisuals::TintPart(RoleBadgeMesh, BadgeColor, BadgeEmissive);
	ApplyRoleModelVisuals(BHPS, ShirtColor, SkinColor);

	constexpr int32 HeadwearIndex = 0;
	constexpr int32 GearIndex = 0;
	const bool bShowRoleAccessories = bUsingRoleModel;
	UStaticMesh* AccessoryCube = BHPropVisuals::CubeMesh();
	UStaticMesh* AccessoryCylinder = BHPropVisuals::CylinderMesh();
	UStaticMesh* AccessorySphere = BHPropVisuals::SphereMesh();
	UMaterialInterface* AccessoryBlack = BHQuaterniusMaterial(TEXT("Black")) ? BHQuaterniusMaterial(TEXT("Black")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* AccessoryBrown = BHQuaterniusMaterial(TEXT("Brown2")) ? BHQuaterniusMaterial(TEXT("Brown2")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* AccessoryDarkBrown = BHQuaterniusMaterial(TEXT("DarkBrown")) ? BHQuaterniusMaterial(TEXT("DarkBrown")) : AccessoryBrown;
	UMaterialInterface* AccessoryMetal = BHQuaterniusMaterial(TEXT("Metal")) ? BHQuaterniusMaterial(TEXT("Metal")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* AccessoryGold = BHQuaterniusMaterial(TEXT("Gold")) ? BHQuaterniusMaterial(TEXT("Gold")) : AccessoryMetal;
	UMaterialInterface* AccessoryLight = BHQuaterniusMaterial(TEXT("LightBlue")) ? BHQuaterniusMaterial(TEXT("LightBlue")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* AccessoryWhite = BHQuaterniusMaterial(TEXT("White")) ? BHQuaterniusMaterial(TEXT("White")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* AccessoryColor = BHQuaterniusPaletteMaterial(ShirtColor) ? BHQuaterniusPaletteMaterial(ShirtColor) : AccessoryLight;
	UStaticMeshComponent* RoleHeadwearParts[] = {
		RoleHeadwearMesh,
		RoleHeadwearAccentMesh,
		RoleHeadwearDetailMesh
	};
	UStaticMeshComponent* RoleGearParts[] = {
		RoleGearMesh,
		RoleGearAccentMesh,
		RoleGearLeftStrapMesh,
		RoleGearRightStrapMesh,
		RoleGearDetailMesh
	};
	for (UStaticMeshComponent* Part : RoleHeadwearParts)
	{
		BHPropVisuals::SetPartVisible(Part, false);
	}
	for (UStaticMeshComponent* Part : RoleGearParts)
	{
		BHPropVisuals::SetPartVisible(Part, false);
	}

	if (bShowRoleAccessories && HeadwearIndex > 0)
	{
		if (HeadwearIndex == 1)
		{
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCylinder, AccessoryColor, FVector(7.0f, 0.0f, 74.0f), FRotator::ZeroRotator, FVector(0.30f, 0.30f, 0.075f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryColor, FVector(23.0f, 0.0f, 68.0f), FRotator::ZeroRotator, FVector(0.12f, 0.28f, 0.025f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCube, AccessoryWhite, FVector(6.0f, 0.0f, 78.0f), FRotator::ZeroRotator, FVector(0.055f, 0.055f, 0.018f), true);
		}
		else if (HeadwearIndex == 2)
		{
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCube, AccessoryBlack, FVector(23.0f, -8.0f, 60.0f), FRotator::ZeroRotator, FVector(0.025f, 0.070f, 0.035f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryBlack, FVector(23.0f, 8.0f, 60.0f), FRotator::ZeroRotator, FVector(0.025f, 0.070f, 0.035f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCube, AccessoryMetal, FVector(24.0f, 0.0f, 60.0f), FRotator::ZeroRotator, FVector(0.018f, 0.050f, 0.014f), true);
		}
		else if (HeadwearIndex == 3)
		{
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCylinder, AccessoryColor, FVector(6.0f, 0.0f, 77.0f), FRotator::ZeroRotator, FVector(0.32f, 0.32f, 0.095f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryDarkBrown, FVector(13.0f, 0.0f, 68.0f), FRotator::ZeroRotator, FVector(0.050f, 0.32f, 0.035f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessorySphere, AccessoryColor, FVector(3.0f, 0.0f, 85.0f), FRotator::ZeroRotator, FVector(0.055f, 0.055f, 0.055f), true);
		}
		else if (HeadwearIndex == 4)
		{
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCube, AccessoryBlack, FVector(11.0f, 0.0f, 67.0f), FRotator::ZeroRotator, FVector(0.045f, 0.40f, 0.080f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryLight, FVector(23.0f, 0.0f, 64.0f), FRotator::ZeroRotator, FVector(0.055f, 0.42f, 0.115f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCube, AccessoryMetal, FVector(24.0f, 0.0f, 57.0f), FRotator::ZeroRotator, FVector(0.018f, 0.36f, 0.012f), true);
		}
	}

	if (bShowRoleAccessories && GearIndex > 0)
	{
		if (GearIndex == 1)
		{
			BHSetAccessoryPiece(RoleGearMesh, AccessoryCube, AccessoryBrown, FVector(-26.0f, 0.0f, 12.0f), FRotator::ZeroRotator, FVector(0.17f, 0.31f, 0.42f), true);
			BHSetAccessoryPiece(RoleGearAccentMesh, AccessoryCube, AccessoryDarkBrown, FVector(-10.0f, 0.0f, 34.0f), FRotator::ZeroRotator, FVector(0.055f, 0.32f, 0.075f), true);
			BHSetAccessoryPiece(RoleGearLeftStrapMesh, AccessoryCube, AccessoryBlack, FVector(9.0f, -15.0f, 18.0f), FRotator::ZeroRotator, FVector(0.035f, 0.022f, 0.32f), true);
			BHSetAccessoryPiece(RoleGearRightStrapMesh, AccessoryCube, AccessoryBlack, FVector(9.0f, 15.0f, 18.0f), FRotator::ZeroRotator, FVector(0.035f, 0.022f, 0.32f), true);
			BHSetAccessoryPiece(RoleGearDetailMesh, AccessoryCube, AccessoryGold, FVector(14.0f, 0.0f, 16.0f), FRotator::ZeroRotator, FVector(0.020f, 0.090f, 0.10f), true);
		}
		else if (GearIndex == 2)
		{
			BHSetAccessoryPiece(RoleGearMesh, AccessoryCylinder, AccessoryMetal, FVector(30.0f, 16.0f, 16.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.055f, 0.055f, 0.24f), true);
			BHSetAccessoryPiece(RoleGearAccentMesh, AccessoryCylinder, AccessoryLight, FVector(42.0f, 16.0f, 16.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.060f, 0.060f, 0.035f), true);
			BHSetAccessoryPiece(RoleGearLeftStrapMesh, AccessoryCube, AccessoryBlack, FVector(24.0f, 16.0f, 5.0f), FRotator::ZeroRotator, FVector(0.030f, 0.050f, 0.090f), true);
		}
		else if (GearIndex == 3)
		{
			BHSetAccessoryPiece(RoleGearMesh, AccessoryCylinder, AccessoryGold, FVector(28.0f, -16.0f, 25.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.075f, 0.075f, 0.020f), true);
			BHSetAccessoryPiece(RoleGearAccentMesh, AccessoryCube, AccessoryWhite, FVector(30.0f, -16.0f, 25.0f), FRotator::ZeroRotator, FVector(0.018f, 0.045f, 0.055f), true);
			BHSetAccessoryPiece(RoleGearDetailMesh, AccessoryCube, AccessoryBlack, FVector(23.0f, -16.0f, 25.0f), FRotator::ZeroRotator, FVector(0.018f, 0.085f, 0.095f), true);
		}
		else if (GearIndex == 4)
		{
			BHSetAccessoryPiece(RoleGearMesh, AccessoryCube, AccessoryBlack, FVector(-20.0f, 20.0f, 26.0f), FRotator::ZeroRotator, FVector(0.060f, 0.14f, 0.20f), true);
			BHSetAccessoryPiece(RoleGearAccentMesh, AccessoryCube, AccessoryMetal, FVector(-12.0f, 20.0f, 33.0f), FRotator::ZeroRotator, FVector(0.020f, 0.12f, 0.025f), true);
			BHSetAccessoryPiece(RoleGearLeftStrapMesh, AccessoryCube, AccessoryMetal, FVector(-8.0f, 20.0f, 45.0f), FRotator(0.0f, 0.0f, -12.0f), FVector(0.012f, 0.018f, 0.18f), true);
			BHSetAccessoryPiece(RoleGearDetailMesh, AccessoryCube, AccessoryLight, FVector(-9.0f, 20.0f, 24.0f), FRotator::ZeroRotator, FVector(0.012f, 0.10f, 0.040f), true);
		}
	}

	BodyMaterialInstance = Cast<UMaterialInstanceDynamic>(BodyMesh->GetMaterial(0));

	if (BHPS)
	{
		LastAppliedAvatarIndex = BHPS->AvatarIndex;
		LastAppliedAvatarRole = BHPS->PlayerRole;
		LastAppliedAvatarColor = BHPS->AvatarColor;
		LastAppliedAvatarHeadwearIndex = HeadwearIndex;
		LastAppliedAvatarGearIndex = GearIndex;
	}
}

bool ABHCharacter::BotBeginInteract(AActor* Target)
{
	return BeginInteractAuthority(Target, false, false);
}

void ABHCharacter::BotEndInteract(AActor* Target)
{
	EndInteractAuthority(Target);
}

void ABHCharacter::BotExitCurrentLocker()
{
	ExitCurrentLockerAuthority();
}

bool ABHCharacter::BotTryCapture()
{
	return TryCaptureAuthority(false);
}

bool ABHCharacter::BotUseScan()
{
	return UseScanAuthority(false);
}

bool ABHCharacter::BotUseHunterPower()
{
	return UseHunterPowerAuthority(false);
}

bool ABHCharacter::BotDropDecoyOrTrap()
{
	return DropDecoyAuthority(false);
}

bool ABHCharacter::BotSubmitAnswer(ABHObjectiveStation* Station, int32 AnswerIndex)
{
	return SubmitAnswerAuthority(Station, AnswerIndex, false, false);
}

void ABHCharacter::ServerSetFlashlight_Implementation(bool bNewOn)
{
	if (CanAct())
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		const bool bInfiniteFlashlight = (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester) || (BHGS && BHGS->bTestMode);
		if (bInfiniteFlashlight)
		{
			FlashlightBattery = 100.0f;
		}
		bFlashlightOn = bNewOn && (bInfiniteFlashlight || FlashlightBattery > 0.0f);
		ApplyFlashlightState();
	}
}

void ABHCharacter::ServerBeginInteract_Implementation(AActor* Target)
{
	BeginInteractAuthority(Target, true, true);
}

bool ABHCharacter::BeginInteractAuthority(AActor* Target, bool bUseViewFallback, bool bShowFailureMessages)
{
	if (bHiddenInLocker)
	{
		ExitLocker();
		return true;
	}

	if (!CanAct())
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("You cannot interact right now."));
		}
		return false;
	}

	AActor* ResolvedTarget = Target;
	if (bUseViewFallback && (!ResolvedTarget || !IsValidInteractionTarget(ResolvedTarget)))
	{
		FindInteractableFromView(ResolvedTarget, 175.0f);
	}

	if (!ResolvedTarget)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("No usable interactable in reach. Aim at the object and press E."));
		}
		return false;
	}

	if (!IsValidInteractionTarget(ResolvedTarget))
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Interactable is too far away."));
		}
		return false;
	}

	if (!IBHInteractableInterface::Execute_CanInteract(ResolvedTarget, this))
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(GetInteractionFailureReason(ResolvedTarget));
		}
		return false;
	}

	if (IBHInteractableInterface::Execute_CanInteract(ResolvedTarget, this))
	{
		CurrentServerInteractTarget = ResolvedTarget;
		IBHInteractableInterface::Execute_BeginInteract(ResolvedTarget, this);
		return true;
	}

	return false;
}

void ABHCharacter::ServerEndInteract_Implementation(AActor* Target)
{
	EndInteractAuthority(Target);
}

void ABHCharacter::EndInteractAuthority(AActor* Target)
{
	AActor* ResolvedTarget = Target ? Target : CurrentServerInteractTarget.Get();
	if (ResolvedTarget && ResolvedTarget->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
	{
		IBHInteractableInterface::Execute_EndInteract(ResolvedTarget, this);
	}

	if (!Target || Target == CurrentServerInteractTarget)
	{
		CurrentServerInteractTarget = nullptr;
	}
}

void ABHCharacter::ServerExitCurrentLocker_Implementation()
{
	ExitCurrentLockerAuthority();
}

void ABHCharacter::ExitCurrentLockerAuthority()
{
	ExitLocker();
}

void ABHCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const float CurrentWalkSpeed = BHRoleWalkSpeed(BHPS, WalkSpeed);
	const float CurrentSprintSpeed = BHRoleSprintSpeed(BHPS, SprintSpeed);
	if (bNewSprinting && Stamina <= MaxStamina * 0.05f)
	{
		GetCharacterMovement()->MaxWalkSpeed = CurrentWalkSpeed;
		return;
	}

	GetCharacterMovement()->MaxWalkSpeed = bNewSprinting ? CurrentSprintSpeed : CurrentWalkSpeed;

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (bNewSprinting && BHPS && BHPS->IsAliveSurvivor() && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && Now - LastSprintNoiseTime > 3.5f)
	{
		LastSprintNoiseTime = Now;
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("running footsteps"));
		}
	}
}

void ABHCharacter::ServerTryCapture_Implementation()
{
	TryCaptureAuthority(true);
}

bool ABHCharacter::TryCaptureAuthority(bool bShowFailureMessages)
{
	const ABHPlayerState* HunterPS = GetPlayerState<ABHPlayerState>();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!HunterPS || !HunterPS->IsAliveHunter())
	{
		if (bShowFailureMessages && HunterPS && HunterPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			SendStatusMessage(TEXT("Hall monitors cannot capture. Use G to place traps and Q/R to hint."));
			return false;
		}
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Capture is Teacher-only. Press Enter on all players to assign roles."));
		}
		return false;
	}

	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Capture unlocks when the Hunt phase begins."));
		}
		return false;
	}

	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Target = *It;
		ABHPlayerState* TargetPS = Target ? Target->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Target || Target == this || !TargetPS || !TargetPS->IsAliveSurvivor() || Target->IsHiddenInLocker())
		{
			continue;
		}

		const bool bInRange = FVector::DistSquared(Target->GetActorLocation(), GetActorLocation()) <= FMath::Square(CaptureDistance);
		const bool bVisible = Controller ? Controller->LineOfSightTo(Target) : true;
		if (bInRange && bVisible)
		{
			if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
			{
				BHGM->NotifySurvivorCaptured(Target);
			}
			return true;
		}
	}

	if (bShowFailureMessages)
	{
		SendStatusMessage(TEXT("No visible survivor close enough to capture."));
	}
	return false;
}

void ABHCharacter::ServerUseScan_Implementation()
{
	UseScanAuthority(true);
}

bool ABHCharacter::UseScanAuthority(bool bShowFailureMessages)
{
	const ABHPlayerState* HunterPS = GetPlayerState<ABHPlayerState>();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (HunterPS && HunterPS->PlayerRole == EBHPlayerRole::FakeHunter && HunterPS->LifeState == EBHPlayerLifeState::Alive)
	{
		if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
		{
			if (bShowFailureMessages)
			{
				SendStatusMessage(TEXT("Hall monitor hints unlock when the Hunt phase begins."));
			}
			return false;
		}

		const float Now = GetWorld()->GetTimeSeconds();
		const float HintCooldown = FMath::Max(12.0f, ScanCooldownSeconds * 0.75f);
		if (Now - LastScanTime < HintCooldown)
		{
			if (bShowFailureMessages)
			{
				SendStatusMessage(FString::Printf(TEXT("Real hint cooling down: %ds."), FMath::CeilToInt(HintCooldown - (Now - LastScanTime))));
			}
			return false;
		}

		LastScanTime = Now;
		SendFakeHunterHint(true);
		return true;
	}

	if (!HunterPS || !HunterPS->IsAliveHunter())
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Heartbeat scan is Teacher-only. Press Enter on all players to assign roles."));
		}
		return false;
	}

	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Heartbeat scan unlocks when the Hunt phase begins."));
		}
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastScanTime < ScanCooldownSeconds)
	{
		if (bShowFailureMessages)
		{
			const int32 RemainingCooldown = FMath::CeilToInt(ScanCooldownSeconds - (Now - LastScanTime));
			SendStatusMessage(FString::Printf(TEXT("Heartbeat scan cooling down: %ds."), RemainingCooldown));
		}
		return false;
	}

	LastScanTime = Now;
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("heartbeat scan"));
	}

	ABHCharacter* Nearest = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Target = *It;
		ABHPlayerState* TargetPS = Target ? Target->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Target || Target == this || !TargetPS || !TargetPS->IsAliveSurvivor())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Target->GetActorLocation(), GetActorLocation());
		if (DistSq <= FMath::Square(3000.0f))
		{
			Target->AddFear(Target->IsDetentionMarked() ? 24.0f : 16.0f);
			if (ABHPlayerController* SurvivorPC = Cast<ABHPlayerController>(Target->GetController()))
			{
				SurvivorPC->ClientShowStatusMessage(Target->IsDetentionMarked()
					? TEXT("The detention mark burns. The Teacher scan found you.")
					: TEXT("Your heartbeat spikes. The Teacher scanned nearby."), 2.5f);
			}
		}
		const float ScanScore = Target->IsDetentionMarked() ? DistSq * 0.35f : DistSq;
		if (ScanScore < BestDistSq)
		{
			BestDistSq = ScanScore;
			Nearest = Target;
		}
	}

	if (bShowFailureMessages)
	{
		ClientReceiveScanResult(Nearest ? Nearest->GetActorLocation() : FVector::ZeroVector, Nearest ? Nearest->IsHiddenInLocker() : false, Nearest != nullptr);
	}
	return Nearest != nullptr;
}

void ABHCharacter::ServerUseHunterPower_Implementation()
{
	UseHunterPowerAuthority(true);
}

bool ABHCharacter::UseHunterPowerAuthority(bool bShowFailureMessages)
{
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}

	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Role powers unlock when the Hunt phase begins."));
		}
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (BHPS->PlayerRole == EBHPlayerRole::Hunter || BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		const float BlackoutCooldown = 34.0f;
		if (Now - LastHunterPowerTime < BlackoutCooldown)
		{
			if (bShowFailureMessages)
			{
				SendStatusMessage(FString::Printf(TEXT("Blackout cooling down: %ds."), FMath::CeilToInt(BlackoutCooldown - (Now - LastHunterPowerTime))));
			}
			return false;
		}

		LastHunterPowerTime = Now;
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->TriggerHunterBlackout(GetActorLocation());
			BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("blackout surge"));
		}
		return true;
	}

	if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		const float FakeHintCooldown = 18.0f;
		if (Now - LastHunterPowerTime < FakeHintCooldown)
		{
			if (bShowFailureMessages)
			{
				SendStatusMessage(FString::Printf(TEXT("False hint cooling down: %ds."), FMath::CeilToInt(FakeHintCooldown - (Now - LastHunterPowerTime))));
			}
			return false;
		}

		LastHunterPowerTime = Now;
		SendFakeHunterHint(false);
		return true;
	}

	if (bShowFailureMessages)
	{
		SendStatusMessage(TEXT("No role power is available for your current role."));
	}
	return false;
}

void ABHCharacter::ServerDropDecoy_Implementation()
{
	DropDecoyAuthority(true);
}

bool ABHCharacter::DropDecoyAuthority(bool bShowFailureMessages)
{
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastDecoyTime < DecoyCooldownSeconds)
	{
		return false;
	}

	LastDecoyTime = Now;
	const FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 115.0f + FVector(0.0f, 0.0f, 20.0f);
	if (BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		GetWorld()->SpawnActor<ABHNoiseDecoy>(SpawnLocation, GetActorRotation());
		GetWorld()->SpawnActor<ABHAlarmTrap>(SpawnLocation + GetActorRightVector() * 70.0f, GetActorRotation());
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->NotifyLoudNoise(SpawnLocation, TEXT("test decoy"));
		}
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Tester decoy and trap placed."));
		}
		return true;
	}

	if (BHPS->IsAliveSurvivor())
	{
		GetWorld()->SpawnActor<ABHNoiseDecoy>(SpawnLocation, GetActorRotation());
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->NotifyLoudNoise(SpawnLocation, TEXT("decoy"));
		}
		return true;
	}

	if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		GetWorld()->SpawnActor<ABHAlarmTrap>(SpawnLocation, GetActorRotation());
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Hall monitor trap placed."));
		}
		return true;
	}

	return false;
}

void ABHCharacter::ServerSubmitAnswer_Implementation(int32 AnswerIndex)
{
	SubmitAnswerAuthority(nullptr, AnswerIndex, true, true);
}

bool ABHCharacter::SubmitAnswerAuthority(ABHObjectiveStation* Station, int32 AnswerIndex, bool bUseViewFallback, bool bShowFailureMessages)
{
	if (!CanAct())
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("You cannot answer right now."));
		}
		return false;
	}

	AActor* Target = nullptr;
	if (!Station && bUseViewFallback)
	{
		FindInteractableFromView(Target, 225.0f);
		Station = Cast<ABHObjectiveStation>(Target);
	}
	if (!Station)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Aim at a question station and press 1-4."));
		}
		return false;
	}

	if (!IsValidInteractionTarget(Station))
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Question station is too far away."));
		}
		return false;
	}

	return Station->SubmitAnswer(this, AnswerIndex);
}

void ABHCharacter::ClientReceiveScanResult_Implementation(const FVector& TargetLocation, bool bTargetHidden, bool bFoundTarget)
{
	ABHPlayerController* PC = Cast<ABHPlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (!bFoundTarget)
	{
		PC->ShowLocalStatusMessage(TEXT("Heartbeat scan found no survivor."), 2.5f);
		return;
	}

	const FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	const FString HiddenText = bTargetHidden ? TEXT(" Hidden.") : TEXT("");
	PC->ShowLocalStatusMessage(FString::Printf(TEXT("Heartbeat detected: direction %.0f, %.0f.%s"), Direction.X, Direction.Y, *HiddenText), 3.0f);
}

void ABHCharacter::OnRep_FlashlightOn()
{
	ApplyFlashlightState();
}

void ABHCharacter::OnRep_HiddenInLocker()
{
	ApplyHiddenState();
}

void ABHCharacter::OnRep_OutOfPlay()
{
	ApplyHiddenState();
	ApplyFlashlightState();
}
