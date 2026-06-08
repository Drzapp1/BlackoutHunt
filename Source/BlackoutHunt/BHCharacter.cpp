// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHCharacter.h"
#include "BHCosmeticUnlocks.h"
#include "BHAccountSubsystem.h"
#include "BHAlarmTrap.h"
#include "BHBlockActor.h"
#include "BHCrawlSpaceVolume.h"
#include "BHEscapeStationManager.h"
#include "BHFootstepSurfaceComponent.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHInteractableInterface.h"
#include "BHLocker.h"
#include "BHMovementAnimInstance.h"
#include "BHMovementTuningAsset.h"
#include "BHNoiseDecoy.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHHUD.h"
#include "BHQuestionInteraction.h"
#include "BHPlayerState.h"
#include "BHPowerupComponent.h"
#include "BHPropVisuals.h"
#include "BHTrainBonusQuestionTerminal.h"
#include "BHTrainRoofBreaker.h"
#include "BHTrainServiceLight.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "HAL/IConsoleManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Perception/AISense_Hearing.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"

namespace
{
constexpr float BHKeyboardTurnInputPerSecond = 48.0f;
constexpr float BHKeyboardLookInputPerSecond = 36.0f;
constexpr float BHHorrorThreatRange = 3600.0f;
constexpr float BHHorrorCloseThreatRange = 950.0f;
constexpr float BHHunterHueLightIntensity = 185.0f;
constexpr float BHHunterHueLightRadius = 320.0f;
constexpr float BHBHopJumpBufferSeconds = 0.16f;
constexpr float BHPostLandingStaminaRecoveryLockSeconds = 0.18f;
constexpr float BHProneCapsuleHalfHeight = 34.0f;
constexpr float BHProneCameraOffsetZ = -48.0f;
constexpr float BHSlideCameraOffsetZ = -32.0f;
constexpr float BHDiveCameraOffsetZ = -38.0f;
constexpr float BHProneStaminaRecoveryMultiplier = 0.55f;
constexpr float BHTeacherMeleeSwingDuration = 0.54f;
constexpr float BHTeacherMeleeSwingMinRestartSeconds = 0.18f;
constexpr float BHTeacherMeleeHitFlashDuration = 0.22f;
constexpr float BHTeacherCaptureWindupSeconds = 0.30f;
constexpr float BHTeacherCaptureAttackSeconds = 0.62f;
constexpr float BHTeacherCaptureSuccessRecoverySeconds = 0.62f;
constexpr float BHTeacherCaptureMissRecoverySeconds = 1.05f;
constexpr float BHTeacherCaptureDoorSlamRecoverySeconds = 1.20f;
constexpr float BHTeacherCaptureFlashlightStaggerRecoverySeconds = 1.35f;
constexpr float BHHunterDefaultSprintDrainMultiplierMax = 0.85f;
constexpr float BHHunterDefaultStaminaRecoveryMultiplier = 1.75f;
constexpr float BHTeacherDefaultCaptureStaminaCost = 5.0f;
constexpr float BHTeacherDefaultCaptureMinStamina = 2.0f;
// Stamina charged per jump (before the per-map stamina scaling). Tuned so a bunny-hop is a touch cheaper than
// the sprint-time it replaces (rewards skill) yet still drains the bar so it can't be spammed for free speed.
constexpr float BHJumpStaminaCost = 12.0f;
constexpr float BHTeacherCaptureRangeForgiveness = 28.0f;
constexpr float BHTeacherCaptureVerticalTolerance = 140.0f;
constexpr float BHTeacherCaptureArcMinDot = 0.05f;
constexpr float BHTeacherCaptureEvasionGraceSeconds = 0.64f;
constexpr float BHLockerExitGraceSeconds = 2.0f;
constexpr float BHTeacherFlashlightStaggerRange = 980.0f;
constexpr float BHTeacherFlashlightStaggerDot = 0.74f;
constexpr float BHTeacherFlashlightStaggerBatteryCost = 22.0f;
constexpr float BHTeacherCaptureMoveMultiplier = 0.54f;
const FVector BHTeacherWeaponIdleLocation(18.0f, 36.0f, -22.0f);
const FRotator BHTeacherWeaponIdleRotation(5.0f, 0.0f, -23.0f);

FBHMovementAnimationProfile BHResolveMovementAnimationProfile();

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

bool BHIsRoleWarmupPhase(const ABHGameState* GameState)
{
	return GameState
		&& GameState->RoundPhase == EBHRoundPhase::Prep
		&& !GameState->bPracticeMode
		&& !GameState->bTestMode;
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
		FLinearColor(0.76f, 0.76f, 0.80f, 1.0f),
		// Hidden prestige tints (indices 8..17) -- must match the ShirtColor entries in BHCosmeticUnlocks.cpp
		// and the identical palettes in BHGameMode/BHPlayerController.
		FLinearColor(0.86f, 0.90f, 0.82f, 1.0f), // Chalk
		FLinearColor(0.95f, 0.22f, 0.62f, 1.0f), // Arcade
		FLinearColor(0.18f, 0.92f, 0.45f, 1.0f), // Exit Sign
		FLinearColor(0.42f, 0.86f, 1.00f, 1.0f), // Afterimage
		FLinearColor(0.64f, 0.44f, 0.22f, 1.0f), // Veteran
		FLinearColor(0.40f, 0.12f, 0.20f, 1.0f), // Faculty
		FLinearColor(0.55f, 0.86f, 0.92f, 1.0f), // Slipstream
		FLinearColor(0.80f, 0.20f, 0.18f, 1.0f), // Detention
		FLinearColor(0.52f, 0.10f, 0.14f, 1.0f), // Apex
		FLinearColor(0.40f, 0.54f, 0.56f, 1.0f), // Commuter
		FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)  // Black (index 18; near-black routes to the Black material)
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
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/White.White"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Black.Black")
	};

	// Black has no near match among the 8 base palette colours, so route near-black recolours straight to the
	// dedicated Black material (the nearest-of-8 scan can never return this appended last index).
	if (Color.R < 0.08f && Color.G < 0.08f && Color.B < 0.08f)
	{
		return LoadObject<UMaterialInterface>(nullptr, MaterialPaths[UE_ARRAY_COUNT(MaterialPaths) - 1]);
	}

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

void BHApplyQuaterniusPalette(UMeshComponent* Component, const TArray<uint8>& SlotColors)
{
	if (!Component)
	{
		return;
	}

	// Per-part recolour. Each clothing material slot keeps its AUTHORED colour unless the player chose an override
	// for it (SlotColors[registryIndex] = palette colour index+1; 0 = authored). Face/skin/hair slots are always
	// left alone. Leaving authored colours by default is what gives each skin its designed multi-colour look
	// instead of a single flat tint. Call EmptyOverrideMaterials() before this so GetMaterial() reads authored.
	const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
	const int32 MaterialCount = Component->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		UMaterialInterface* OriginalMaterial = Component->GetMaterial(MaterialIndex);
		if (BHShouldPreserveQuaterniusMaterial(OriginalMaterial))
		{
			continue;
		}
		// Match by the mesh's authored SLOT NAME (what the recolour UI enumerates), falling back to the material name.
		FName SlotName = SlotNames.IsValidIndex(MaterialIndex) ? SlotNames[MaterialIndex] : NAME_None;
		int32 RegistryIndex = BHColorableMaterialIndex(SlotName);
		if (RegistryIndex == INDEX_NONE && OriginalMaterial)
		{
			RegistryIndex = BHColorableMaterialIndex(FName(*OriginalMaterial->GetName()));
		}
		const uint8 ColorPlusOne = (RegistryIndex != INDEX_NONE && SlotColors.IsValidIndex(RegistryIndex))
			? SlotColors[RegistryIndex]
			: 0;
		if (ColorPlusOne >= 1)
		{
			if (UMaterialInterface* PaletteMaterial = BHQuaterniusPaletteMaterial(BHAvatarPaletteColor(ColorPlusOne - 1)))
			{
				Component->SetMaterial(MaterialIndex, PaletteMaterial);
			}
		}
	}
}

UMaterialInterface* BHQuaterniusMaterial(const TCHAR* AssetName)
{
	const FString Name(AssetName);
	const FString Path = FString::Printf(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/%s.%s"), *Name, *Name);
	return LoadObject<UMaterialInterface>(nullptr, *Path);
}

UStaticMesh* BHTeacherWeaponImportedMesh()
{
	static const TCHAR* CandidatePaths[] = {
		TEXT("/Game/BlackoutHunt/Art/Weapons/KayKit/SM_BH_Axe_1H.SM_BH_Axe_1H"),
		TEXT("/Game/BlackoutHunt/Art/Weapons/KayKit/axe_1handed.axe_1handed")
	};

	for (const TCHAR* Path : CandidatePaths)
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path))
		{
			return Mesh;
		}
	}

	return nullptr;
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

// Finds a skeletal mesh's head bone for headwear attachment (common names first, then any bone containing
// "head"). Returns NAME_None when there's no head bone (e.g. a static role mesh) so callers fall back to root.
static FName BHFindHeadBone(const USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return NAME_None;
	}
	static const TCHAR* Candidates[] = { TEXT("head"), TEXT("Head"), TEXT("B-head"), TEXT("Bip001 Head"), TEXT("mixamorig:Head"), TEXT("Head_M"), TEXT("spine_05") };
	for (const TCHAR* Name : Candidates)
	{
		if (Mesh->GetBoneIndex(FName(Name)) != INDEX_NONE)
		{
			return FName(Name);
		}
	}
	if (const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& RefSkel = Asset->GetRefSkeleton();
		const int32 BoneCount = RefSkel.GetNum();
		for (int32 BoneIdx = 0; BoneIdx < BoneCount; ++BoneIdx)
		{
			const FName BoneName = RefSkel.GetBoneName(BoneIdx);
			if (BoneName.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase))
			{
				return BoneName;
			}
		}
	}
	return NAME_None;
}

// Collect the leg bones (and their descendants are hidden automatically) so a seated avatar can drop the
// "don't render the lower body" trick on any skeleton. Matches by keyword so it works across Quaternius /
// UE-Mannequin / Mixamo naming without hard-coding a single rig.
static TArray<FName> BHCollectLegBones(const USkeletalMeshComponent* Mesh)
{
	TArray<FName> Result;
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		return Result;
	}
	static const TCHAR* Keys[] = { TEXT("thigh"), TEXT("upleg"), TEXT("calf"), TEXT("shin"), TEXT("foot"), TEXT("toe"), TEXT("knee"), TEXT("leg") };
	const FReferenceSkeleton& RefSkel = Asset->GetRefSkeleton();
	const int32 BoneCount = RefSkel.GetNum();
	for (int32 BoneIdx = 0; BoneIdx < BoneCount; ++BoneIdx)
	{
		const FName BoneName = RefSkel.GetBoneName(BoneIdx);
		const FString BoneString = BoneName.ToString();
		for (const TCHAR* Key : Keys)
		{
			if (BoneString.Contains(Key, ESearchCase::IgnoreCase))
			{
				Result.Add(BoneName);
				break;
			}
		}
	}
	return Result;
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
	if (AnimationName == FName(TEXT("IdleWeapon")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/BH_Q_AnimationsCharacterArmature_Idle_Sword.BH_Q_AnimationsCharacterArmature_Idle_Sword");
	}
	if (AnimationName == FName(TEXT("MeleeSwing")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/BH_Q_AnimationsCharacterArmature_Sword_Slash.BH_Q_AnimationsCharacterArmature_Sword_Slash");
	}

	return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Idle.A_BH_Q_Idle");
}

const TCHAR* BHFreeAnimationLibraryPath(FName AnimationName)
{
	if (AnimationName == FName(TEXT("Roll")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Roll.A_BH_FAL_Roll");
	}
	if (AnimationName == FName(TEXT("Slide")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Slide.A_BH_FAL_Slide");
	}
	if (AnimationName == FName(TEXT("ProneIdle")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Prone_Idle.A_BH_FAL_Prone_Idle");
	}
	if (AnimationName == FName(TEXT("ProneCrawl")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Prone_Crawl.A_BH_FAL_Prone_Crawl");
	}
	if (AnimationName == FName(TEXT("Dive")))
	{
		return TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Dive.A_BH_FAL_Dive");
	}

	return nullptr;
}

const TSoftObjectPtr<UAnimSequence>* BHConfiguredMovementAnimation(const FBHMovementAnimationSet& AnimationSet, FName AnimationName)
{
	if (AnimationName == FName(TEXT("Idle")))
	{
		return &AnimationSet.Idle;
	}
	if (AnimationName == FName(TEXT("Walk")))
	{
		return &AnimationSet.Walk;
	}
	if (AnimationName == FName(TEXT("Run")))
	{
		return &AnimationSet.Run;
	}
	if (AnimationName == FName(TEXT("Roll")))
	{
		return &AnimationSet.Roll;
	}
	if (AnimationName == FName(TEXT("Slide")))
	{
		return &AnimationSet.Slide;
	}
	if (AnimationName == FName(TEXT("Dive")))
	{
		return &AnimationSet.Dive;
	}
	if (AnimationName == FName(TEXT("ProneIdle")))
	{
		return &AnimationSet.ProneIdle;
	}
	if (AnimationName == FName(TEXT("ProneCrawl")))
	{
		return &AnimationSet.ProneCrawl;
	}
	return nullptr;
}

UAnimSequence* BHLoadConfiguredMovementAnimation(FName AnimationName)
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings || !Settings->bUseImportedMovementAnimations)
	{
		return nullptr;
	}

	const FBHMovementAnimationProfile AnimationProfile = BHResolveMovementAnimationProfile();
	const TSoftObjectPtr<UAnimSequence>* ConfiguredAnimation = BHConfiguredMovementAnimation(AnimationProfile.Animations, AnimationName);
	return ConfiguredAnimation && !ConfiguredAnimation->IsNull()
		? ConfiguredAnimation->LoadSynchronous()
		: nullptr;
}

UAnimSequence* BHLoadRoleMovementAnimation(FName AnimationName)
{
	if (UAnimSequence* ConfiguredAnimation = BHLoadConfiguredMovementAnimation(AnimationName))
	{
		return ConfiguredAnimation;
	}

	return LoadObject<UAnimSequence>(nullptr, BHQuaterniusAnimationPath(AnimationName));
}

bool BHIsTransientSpecialMove(EBHMovementSpecialState State)
{
	return State == EBHMovementSpecialState::Rolling
		|| State == EBHMovementSpecialState::Sliding
		|| State == EBHMovementSpecialState::Diving;
}

FName BHAnimationNameForSpecialState(EBHMovementSpecialState State, bool bMoving)
{
	switch (State)
	{
	case EBHMovementSpecialState::Rolling:
		return FName(TEXT("Roll"));
	case EBHMovementSpecialState::Sliding:
		return FName(TEXT("Slide"));
	case EBHMovementSpecialState::Diving:
		return FName(TEXT("Dive"));
	case EBHMovementSpecialState::Prone:
		return bMoving ? FName(TEXT("ProneCrawl")) : FName(TEXT("ProneIdle"));
	default:
		return NAME_None;
	}
}

UAnimSequence* BHLoadSpecialAnimation(EBHMovementSpecialState State, bool bMoving)
{
	const FName AnimationName = BHAnimationNameForSpecialState(State, bMoving);
	if (AnimationName == NAME_None)
	{
		return nullptr;
	}

	if (UAnimSequence* ConfiguredAnimation = BHLoadConfiguredMovementAnimation(AnimationName))
	{
		return ConfiguredAnimation;
	}

	if (const TCHAR* FreeLibraryPath = BHFreeAnimationLibraryPath(AnimationName))
	{
		if (UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, FreeLibraryPath))
		{
			return Animation;
		}
	}

	// The FreeAnimationLibrary clips this normally wants aren't present on disk, so without these Quaternius fallbacks
	// only Rolling animated and Slide/Dive/Prone froze the third-person mesh on its prior pose (a sliding/crawling
	// body that visibly didn't move its limbs). Reuse the closest existing Quaternius clip per state.
	switch (State)
	{
	case EBHMovementSpecialState::Rolling:
	case EBHMovementSpecialState::Diving:
		// No dedicated dive clip; the roll reads as a committed forward lunge for a dive too.
		return LoadObject<UAnimSequence>(nullptr, TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/BH_Q_AnimationsCharacterArmature_Roll.BH_Q_AnimationsCharacterArmature_Roll"));
	case EBHMovementSpecialState::Sliding:
		// No slide clip; a run clip (the caller can slow the play rate) beats a frozen pose while the body slides.
		return LoadObject<UAnimSequence>(nullptr, TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Run.A_BH_Q_Run"));
	case EBHMovementSpecialState::Prone:
		// No prone clip; idle (still) / walk (crawling) keep the body posed rather than frozen mid-stand.
		return LoadObject<UAnimSequence>(nullptr, bMoving
			? TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Walk.A_BH_Q_Walk")
			: TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Idle.A_BH_Q_Idle"));
	default:
		break;
	}

	return nullptr;
}

bool BHUsesHunterMovementProfile(const ABHPlayerState* BHPS)
{
	return BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter;
}

FBHMovementSpecialTuning BHMakeDefaultMovementSpecialTuning(EBHMovementSpecialState State)
{
	FBHMovementSpecialTuning Tuning;
	Tuning.State = State;
	Tuning.LowStaminaText = FText::FromString(TEXT("Too exhausted for that move."));
	Tuning.CooldownText = FText::FromString(TEXT("Movement cooling down."));
	Tuning.BlockedText = FText::FromString(TEXT("Not enough room for that move."));

	switch (State)
	{
	case EBHMovementSpecialState::Rolling:
		Tuning.DurationSeconds = 0.55f;
		Tuning.StaminaCost = 12.0f;
		Tuning.CooldownSeconds = 1.25f;
		Tuning.NoiseStrength = 0.72f;
		Tuning.Curve.Distance = 420.0f;
		Tuning.Curve.MinForwardClearance = 210.0f;
		Tuning.Curve.MaxDropHeight = 64.0f;
		break;
	case EBHMovementSpecialState::Sliding:
		Tuning.DurationSeconds = 0.75f;
		Tuning.StaminaCost = 16.0f;
		Tuning.CooldownSeconds = 1.60f;
		Tuning.NoiseStrength = 1.06f;
		Tuning.Curve.Distance = 620.0f;
		Tuning.Curve.MinForwardClearance = 310.0f;
		Tuning.Curve.MaxDropHeight = 58.0f;
		break;
	case EBHMovementSpecialState::Diving:
		Tuning.DurationSeconds = 0.65f;
		Tuning.StaminaCost = 22.0f;
		Tuning.CooldownSeconds = 2.40f;
		Tuning.NoiseStrength = 1.24f;
		Tuning.Curve.Distance = 510.0f;
		Tuning.Curve.VerticalImpulse = 115.0f;
		Tuning.Curve.MinForwardClearance = 285.0f;
		Tuning.Curve.MaxDropHeight = 86.0f;
		break;
	default:
		break;
	}

	return Tuning;
}

FBHMovementRoleTuning BHMakeDefaultMovementRoleTuning(EBHPlayerRole Role)
{
	FBHMovementRoleTuning Tuning;
	Tuning.Role = Role;
	Tuning.WalkSpeed = 530.0f;   // brisk exploration pace (360 -> 470 -> 530)
	Tuning.SprintSpeed = 900.0f;
	Tuning.SprintDrainMultiplier = 1.0f;
	Tuning.ProneSpeed = 120.0f;
	Tuning.StaminaCostMultiplier = 1.0f;
	Tuning.CooldownMultiplier = 1.0f;
	Tuning.NoiseMultiplier = 1.0f;
	Tuning.ProneNoiseMultiplier = 0.38f;
	Tuning.ProneVisibilityMultiplier = 0.55f;
	if (Role == EBHPlayerRole::Hunter)
	{
		Tuning.WalkSpeed = 460.0f;   // faster hunter walk (315 -> 410 -> 460), keeps the ~same gap to survivors
		Tuning.SprintSpeed = 1150.0f;
		Tuning.SprintDrainMultiplier = BHHunterDefaultSprintDrainMultiplierMax;
		Tuning.ProneSpeed = 95.0f;
		Tuning.StaminaCostMultiplier = 1.40f;
		Tuning.CooldownMultiplier = 1.30f;
		Tuning.NoiseMultiplier = 1.35f;
		Tuning.ProneNoiseMultiplier = 0.54f;
		Tuning.ProneVisibilityMultiplier = 0.82f;
	}
	else if (Role == EBHPlayerRole::FakeHunter)
	{
		Tuning.ProneSpeed = 110.0f;
		Tuning.StaminaCostMultiplier = 1.20f;
		Tuning.NoiseMultiplier = 1.25f;
		Tuning.ProneNoiseMultiplier = 0.48f;
		Tuning.ProneVisibilityMultiplier = 0.68f;
	}
	Tuning.SpecialMoves = {
		BHMakeDefaultMovementSpecialTuning(EBHMovementSpecialState::Rolling),
		BHMakeDefaultMovementSpecialTuning(EBHMovementSpecialState::Sliding),
		BHMakeDefaultMovementSpecialTuning(EBHMovementSpecialState::Diving)
	};
	return Tuning;
}

EBHPlayerRole BHEffectiveMovementRole(const ABHPlayerState* BHPS)
{
	if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned || BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		return EBHPlayerRole::Survivor;
	}
	return BHPS->PlayerRole;
}

const FBHMovementRoleTuning* BHFindRoleTuning(const TArray<FBHMovementRoleTuning>& Tunings, EBHPlayerRole Role)
{
	for (const FBHMovementRoleTuning& Tuning : Tunings)
	{
		if (Tuning.Role == Role)
		{
			return &Tuning;
		}
	}
	return nullptr;
}

FBHMovementRoleTuning BHResolveMovementRoleTuning(const ABHPlayerState* BHPS)
{
	const EBHPlayerRole Role = BHEffectiveMovementRole(BHPS);
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings)
	{
		if (!Settings->DefaultMovementTuningAsset.IsNull())
		{
			if (const UBHMovementTuningAsset* Asset = Settings->DefaultMovementTuningAsset.LoadSynchronous())
			{
				return Asset->ResolveRoleTuning(Role);
			}
		}

		if (const FBHMovementRoleTuning* ConfigTuning = BHFindRoleTuning(Settings->MovementRoleTunings, Role))
		{
			return *ConfigTuning;
		}
	}
	return BHMakeDefaultMovementRoleTuning(Role);
}

FBHMovementSpecialTuning BHGetSpecialMoveTuning(const ABHPlayerState* BHPS, EBHMovementSpecialState State)
{
	const FBHMovementRoleTuning RoleTuning = BHResolveMovementRoleTuning(BHPS);
	for (const FBHMovementSpecialTuning& Tuning : RoleTuning.SpecialMoves)
	{
		if (Tuning.State == State)
		{
			return Tuning;
		}
	}
	return BHMakeDefaultMovementSpecialTuning(State);
}

FBHMovementAnimationProfile BHResolveMovementAnimationProfile()
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings)
	{
		if (!Settings->DefaultMovementTuningAsset.IsNull())
		{
			if (const UBHMovementTuningAsset* Asset = Settings->DefaultMovementTuningAsset.LoadSynchronous())
			{
				return Asset->AnimationProfile;
			}
		}
		return Settings->MovementAnimationProfile;
	}
	return FBHMovementAnimationProfile();
}

FBHPOVAnimationTuning BHResolvePOVAnimationTuning()
{
	if (const UBHGameSettings* Settings = GetDefault<UBHGameSettings>())
	{
		return Settings->POVAnimationTuning;
	}
	return FBHPOVAnimationTuning();
}

float BHPOVIntensityScale(EBHPOVAnimIntensity Intensity)
{
	switch (Intensity)
	{
	case EBHPOVAnimIntensity::Subtle:
		return 0.55f;
	case EBHPOVAnimIntensity::Punchy:
		return 1.6f;
	case EBHPOVAnimIntensity::Moderate:
	default:
		return 1.0f;
	}
}

bool BHDoesSoftClassPackageExist(const TSoftClassPtr<UAnimInstance>& AnimClass)
{
	if (AnimClass.IsNull())
	{
		return false;
	}

	const FSoftObjectPath SoftPath = AnimClass.ToSoftObjectPath();
	const FString PackageName = SoftPath.GetLongPackageName().IsEmpty()
		? FPackageName::ObjectPathToPackageName(SoftPath.ToString())
		: SoftPath.GetLongPackageName();
	return !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
}

UClass* BHLoadAnimInstanceClassIfAvailable(const TSoftClassPtr<UAnimInstance>& AnimClass)
{
	return BHDoesSoftClassPackageExist(AnimClass)
		? AnimClass.LoadSynchronous()
		: nullptr;
}

UClass* BHLoadMovementAnimInstanceClassInternal(const ABHPlayerState* BHPS, bool bAllowHunterFallback)
{
	const FBHMovementAnimationProfile Profile = BHResolveMovementAnimationProfile();
	if (!Profile.bPreferAnimBlueprint)
	{
		return nullptr;
	}

	const bool bHunterRole = BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter;
	if (bHunterRole)
	{
		if (UClass* HunterAnimClass = BHLoadAnimInstanceClassIfAvailable(Profile.HunterAnimInstanceClass))
		{
			return HunterAnimClass;
		}
		if (!bAllowHunterFallback)
		{
			return nullptr;
		}
	}

	return BHLoadAnimInstanceClassIfAvailable(Profile.QuaterniusAnimInstanceClass);
}

UClass* BHLoadMovementAnimInstanceClass(const ABHPlayerState* BHPS)
{
	return BHLoadMovementAnimInstanceClassInternal(BHPS, true);
}

UClass* BHLoadNativeHunterAnimInstanceClass(const ABHPlayerState* BHPS)
{
	return BHLoadMovementAnimInstanceClassInternal(BHPS, false);
}

// Per-map movement feel. The three maps form a deliberate arc: Facility is the hiding map (slowest,
// most tense), Substation sits in between, Foggrounds is the running-from-the-teacher map (fastest).
// A single scalar is applied to BOTH walk and sprint for every role, so the survivor<->hunter chase
// gap (and the walk:sprint ratio) is preserved while each map gets its own distinct walk AND run pace.
// Reads the replicated ActiveLevelName off the GameState, so it works identically on client and server.
float BHLevelMovementScale(const ABHPlayerState* BHPS)
{
	if (!BHPS)
	{
		return 1.0f;
	}
	const UWorld* World = BHPS->GetWorld();
	const ABHGameState* GameState = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!GameState)
	{
		return 1.0f;
	}
	const FString& Level = GameState->ActiveLevelName;
	if (Level.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		return 1.13f;   // running map: fastest pace, keep the chase open
	}
	if (Level.Equals(TEXT("Facility"), ESearchCase::IgnoreCase))
	{
		return 0.90f;   // hiding map: slowest, most deliberate
	}
	return 1.0f;        // Substation / Tutorial / default: in between
}

// Per-map sprint economy. The running-focused maps give considerably more usable stamina: sprint drains
// slower AND recovers faster, both scaled by this factor. Facility (the hiding map) stays baseline. This runs
// server-side (drain/recovery are authority-only), which already knows the replicated ActiveLevelName.
float BHLevelStaminaScale(const ABHPlayerState* BHPS)
{
	if (!BHPS)
	{
		return 1.0f;
	}
	const UWorld* World = BHPS->GetWorld();
	const ABHGameState* GameState = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!GameState)
	{
		return 1.0f;
	}
	const FString& Level = GameState->ActiveLevelName;
	if (Level.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		return 1.70f;   // the run map: sprint lasts much longer and refills fast
	}
	if (Level.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
	{
		return 1.30f;   // mid map: a meaningful boost over the hiding map
	}
	return 1.0f;        // Facility / Tutorial / default: baseline (hiding, not running)
}

float BHRoleWalkSpeed(const ABHPlayerState* BHPS, float DefaultWalkSpeed)
{
	const FBHMovementRoleTuning Tuning = BHResolveMovementRoleTuning(BHPS);
	const float Base = Tuning.WalkSpeed > 0.0f ? Tuning.WalkSpeed : DefaultWalkSpeed;
	return Base * BHLevelMovementScale(BHPS);
}

float BHRoleSprintSpeed(const ABHPlayerState* BHPS, float DefaultSprintSpeed)
{
	const FBHMovementRoleTuning Tuning = BHResolveMovementRoleTuning(BHPS);
	const float Base = Tuning.SprintSpeed > 0.0f ? Tuning.SprintSpeed : DefaultSprintSpeed;
	return Base * BHLevelMovementScale(BHPS);
}

float BHRoleSprintDrainMultiplier(const ABHPlayerState* BHPS)
{
	const float RoleMultiplier = BHResolveMovementRoleTuning(BHPS).SprintDrainMultiplier;
	if (!BHPS || !BHPS->IsAliveHunter())
	{
		return RoleMultiplier;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const float HunterDrainCap = Settings
		? FMath::Max(0.0f, Settings->HunterSprintDrainMultiplierMax)
		: BHHunterDefaultSprintDrainMultiplierMax;
	return FMath::Min(RoleMultiplier, HunterDrainCap);
}

float BHRoleStaminaRecoveryMultiplier(const ABHPlayerState* BHPS)
{
	if (!BHPS || !BHPS->IsAliveHunter())
	{
		return 1.0f;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	return Settings
		? FMath::Max(0.0f, Settings->HunterStaminaRecoveryMultiplier)
		: BHHunterDefaultStaminaRecoveryMultiplier;
}

float BHTeacherCaptureStaminaCost()
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	return Settings
		? FMath::Max(0.0f, Settings->TeacherAxeStaminaCost)
		: BHTeacherDefaultCaptureStaminaCost;
}

float BHTeacherCaptureMinStamina()
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	return Settings
		? FMath::Max(0.0f, Settings->TeacherAxeMinStamina)
		: BHTeacherDefaultCaptureMinStamina;
}

float BHRoleProneSpeed(const ABHPlayerState* BHPS)
{
	return BHResolveMovementRoleTuning(BHPS).ProneSpeed;
}

float BHRoleSpecialStaminaMultiplier(const ABHPlayerState* BHPS)
{
	return BHResolveMovementRoleTuning(BHPS).StaminaCostMultiplier;
}

float BHRoleSpecialCooldownMultiplier(const ABHPlayerState* BHPS)
{
	return BHResolveMovementRoleTuning(BHPS).CooldownMultiplier;
}

float BHRoleSpecialNoiseMultiplier(const ABHPlayerState* BHPS)
{
	return BHResolveMovementRoleTuning(BHPS).NoiseMultiplier;
}

float BHRoleProneNoiseMultiplier(const ABHPlayerState* BHPS)
{
	return BHResolveMovementRoleTuning(BHPS).ProneNoiseMultiplier;
}

float BHMovementCurveAlpha(const FBHMovementSpecialTuning& Tuning, float NormalizedTime)
{
	const float ClampedTime = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
	if (!Tuning.Curve.DistanceCurve.IsNull())
	{
		if (const UCurveFloat* Curve = Tuning.Curve.DistanceCurve.LoadSynchronous())
		{
			return FMath::Clamp(Curve->GetFloatValue(ClampedTime), 0.0f, 1.0f);
		}
	}

	return ClampedTime * ClampedTime * (3.0f - 2.0f * ClampedTime);
}

const TCHAR* BHFootstepSurfaceToken(EBHFootstepSurface Surface)
{
	switch (Surface)
	{
	case EBHFootstepSurface::Concrete:
		return TEXT("concrete");
	case EBHFootstepSurface::Tile:
		return TEXT("tile");
	case EBHFootstepSurface::Metal:
		return TEXT("metal");
	case EBHFootstepSurface::Wet:
		return TEXT("wet");
	case EBHFootstepSurface::Gravel:
		return TEXT("gravel");
	case EBHFootstepSurface::Glass:
		return TEXT("glass");
	case EBHFootstepSurface::Soft:
		return TEXT("soft");
	default:
		return TEXT("default");
	}
}

FBHFootstepSurfaceProfile BHDefaultFootstepProfile(EBHFootstepSurface Surface)
{
	FBHFootstepSurfaceProfile Profile;
	Profile.Surface = Surface;
	switch (Surface)
	{
	case EBHFootstepSurface::Concrete:
		Profile.NoiseMultiplier = 1.05f;
		Profile.HearingRadiusBase = 1700.0f;
		Profile.HearingRadiusScale = 1850.0f;
		Profile.AtmosphereMultiplier = 1.05f;
		break;
	case EBHFootstepSurface::Tile:
		Profile.NoiseMultiplier = 1.24f;
		Profile.HearingRadiusBase = 1850.0f;
		Profile.HearingRadiusScale = 2050.0f;
		Profile.AtmosphereMultiplier = 1.14f;
		break;
	case EBHFootstepSurface::Metal:
		Profile.NoiseMultiplier = 1.42f;
		Profile.HearingRadiusBase = 2050.0f;
		Profile.HearingRadiusScale = 2300.0f;
		Profile.AtmosphereMultiplier = 1.22f;
		break;
	case EBHFootstepSurface::Wet:
		Profile.NoiseMultiplier = 1.18f;
		Profile.HearingRadiusBase = 1900.0f;
		Profile.HearingRadiusScale = 2150.0f;
		Profile.AtmosphereMultiplier = 1.24f;
		break;
	case EBHFootstepSurface::Gravel:
		Profile.NoiseMultiplier = 1.34f;
		Profile.HearingRadiusBase = 1950.0f;
		Profile.HearingRadiusScale = 2200.0f;
		Profile.AtmosphereMultiplier = 1.18f;
		break;
	case EBHFootstepSurface::Glass:
		Profile.NoiseMultiplier = 1.58f;
		Profile.HearingRadiusBase = 2150.0f;
		Profile.HearingRadiusScale = 2500.0f;
		Profile.AtmosphereMultiplier = 1.32f;
		break;
	case EBHFootstepSurface::Soft:
		Profile.NoiseMultiplier = 0.62f;
		Profile.HearingRadiusBase = 1100.0f;
		Profile.HearingRadiusScale = 1250.0f;
		Profile.AtmosphereMultiplier = 0.74f;
		break;
	default:
		Profile.NoiseMultiplier = 1.0f;
		Profile.HearingRadiusBase = 1600.0f;
		Profile.HearingRadiusScale = 1700.0f;
		Profile.AtmosphereMultiplier = 1.0f;
		break;
	}
	return Profile;
}

EBHFootstepSurface BHSurfaceFromName(const FString& Name)
{
	if (Name.Contains(TEXT("Glass"), ESearchCase::IgnoreCase))
	{
		return EBHFootstepSurface::Glass;
	}
	if (Name.Contains(TEXT("Metal"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Steel"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Plate"), ESearchCase::IgnoreCase))
	{
		return EBHFootstepSurface::Metal;
	}
	if (Name.Contains(TEXT("Tile"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Ceramic"), ESearchCase::IgnoreCase))
	{
		return EBHFootstepSurface::Tile;
	}
	if (Name.Contains(TEXT("Water"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Wet"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Puddle"), ESearchCase::IgnoreCase))
	{
		return EBHFootstepSurface::Wet;
	}
	if (Name.Contains(TEXT("Gravel"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Dirt"), ESearchCase::IgnoreCase))
	{
		return EBHFootstepSurface::Gravel;
	}
	if (Name.Contains(TEXT("Snow"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Grass"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Vegetation"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Carpet"), ESearchCase::IgnoreCase))
	{
		return EBHFootstepSurface::Soft;
	}
	if (Name.Contains(TEXT("Concrete"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Plaster"), ESearchCase::IgnoreCase))
	{
		return EBHFootstepSurface::Concrete;
	}
	return EBHFootstepSurface::Default;
}

EBHFootstepSurface BHSurfaceFromActorTags(const AActor* Actor)
{
	if (!Actor)
	{
		return EBHFootstepSurface::Default;
	}

	static const TPair<FName, EBHFootstepSurface> SurfaceTags[] = {
		{TEXT("Surface_Glass"), EBHFootstepSurface::Glass},
		{TEXT("Surface_Metal"), EBHFootstepSurface::Metal},
		{TEXT("Surface_Tile"), EBHFootstepSurface::Tile},
		{TEXT("Surface_Wet"), EBHFootstepSurface::Wet},
		{TEXT("Surface_Gravel"), EBHFootstepSurface::Gravel},
		{TEXT("Surface_Soft"), EBHFootstepSurface::Soft},
		{TEXT("Surface_Concrete"), EBHFootstepSurface::Concrete}
	};

	for (const TPair<FName, EBHFootstepSurface>& SurfaceTag : SurfaceTags)
	{
		if (Actor->ActorHasTag(SurfaceTag.Key))
		{
			return SurfaceTag.Value;
		}
	}
	return BHSurfaceFromName(Actor->GetName());
}

bool BHCharacterSoftObjectPathExists(const FSoftObjectPath& ObjectPath)
{
	if (ObjectPath.IsNull())
	{
		return false;
	}

	static TMap<FSoftObjectPath, bool> OptionalAssetPathCache;
	if (const bool* bCachedExists = OptionalAssetPathCache.Find(ObjectPath))
	{
		return *bCachedExists;
	}

	const FString PackageName = ObjectPath.GetLongPackageName();
	const bool bExists = !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
	OptionalAssetPathCache.Add(ObjectPath, bExists);
	return bExists;
}

void BHLogMissingOptionalCharacterAssetOnce(const FSoftObjectPath& ObjectPath, const TCHAR* Context)
{
	if (ObjectPath.IsNull())
	{
		return;
	}

	static TSet<FSoftObjectPath> MissingOptionalAssets;
	if (MissingOptionalAssets.Contains(ObjectPath))
	{
		return;
	}

	MissingOptionalAssets.Add(ObjectPath);
	UE_LOG(LogTemp, Verbose, TEXT("BlackoutHunt %s missing, using silent fallback: %s"), Context, *ObjectPath.ToString());
}

FVector BHFootstepCueLocation(const ABHCharacter* Character)
{
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	FVector Location = Character->GetActorLocation();
	if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Location.Z -= FMath::Max(0.0f, Capsule->GetScaledCapsuleHalfHeight() - 8.0f);
	}
	return Location;
}

float BHHorrorThresholdAlpha(float Value, float Threshold, float FullValue)
{
	return FMath::Clamp((Value - Threshold) / FMath::Max(1.0f, FullValue - Threshold), 0.0f, 1.0f);
}

float BHFearPanicAlpha(const ABHCharacter* Character)
{
	return Character ? BHHorrorThresholdAlpha(Character->GetFear(), 55.0f, 100.0f) : 0.0f;
}

float BHDreadStrainAlpha(const ABHCharacter* Character)
{
	return Character ? BHHorrorThresholdAlpha(Character->GetDread(), 62.0f, 100.0f) : 0.0f;
}

float BHHorrorWalkSpeedMultiplier(const ABHCharacter* Character, const ABHPlayerState* BHPS)
{
	if (!Character || !BHPS || !BHPS->IsAliveSurvivor())
	{
		return 1.0f;
	}

	const float FearPenalty = BHHorrorThresholdAlpha(Character->GetFear(), 78.0f, 100.0f) * 0.07f;
	const float DreadPenalty = BHDreadStrainAlpha(Character) * 0.10f;
	return FMath::Clamp(1.0f - FearPenalty - DreadPenalty, 0.82f, 1.0f);
}

float BHHorrorSprintSpeedMultiplier(const ABHCharacter* Character, const ABHPlayerState* BHPS)
{
	if (!Character || !BHPS || !BHPS->IsAliveSurvivor())
	{
		return 1.0f;
	}

	const float FearPenalty = BHFearPanicAlpha(Character) * 0.16f;
	const float DreadPenalty = BHDreadStrainAlpha(Character) * 0.08f;
	return FMath::Clamp(1.0f - FearPenalty - DreadPenalty, 0.76f, 1.0f);
}

float BHHorrorStaminaDrainMultiplier(const ABHCharacter* Character, const ABHPlayerState* BHPS)
{
	if (!Character || !BHPS || !BHPS->IsAliveSurvivor())
	{
		return 1.0f;
	}

	return 1.0f + BHFearPanicAlpha(Character) * 0.36f + BHDreadStrainAlpha(Character) * 0.22f;
}

float BHHorrorStaminaRecoveryMultiplier(const ABHCharacter* Character, const ABHPlayerState* BHPS)
{
	if (!Character || !BHPS || !BHPS->IsAliveSurvivor())
	{
		return 1.0f;
	}

	const float FearAlpha = FMath::Clamp(Character->GetFear() / 100.0f, 0.0f, 1.0f);
	return FMath::Lerp(1.0f, 0.55f, FearAlpha) * FMath::Lerp(1.0f, 0.78f, BHDreadStrainAlpha(Character));
}

float BHHorrorNoiseMultiplier(const ABHCharacter* Character, const ABHPlayerState* BHPS)
{
	if (!Character || !BHPS || !BHPS->IsAliveSurvivor())
	{
		return 1.0f;
	}

	return 1.0f + BHHorrorThresholdAlpha(Character->GetFear(), 45.0f, 100.0f) * 0.42f + BHHorrorThresholdAlpha(Character->GetDread(), 70.0f, 100.0f) * 0.28f;
}

bool BHFinalEscapeHunterPenaltyActive(UWorld* World, const ABHCharacter* Character)
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!World || !BHPS || !BHPS->IsAliveHunter())
	{
		return false;
	}

	for (TActorIterator<ABHEscapeStationManager> It(World); It; ++It)
	{
		const ABHEscapeStationManager* Manager = *It;
		if (Manager && Manager->IsHunterPenaltyActive(Character))
		{
			return true;
		}
	}
	return false;
}
}

ABHCharacter::ABHCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Standing radius (was 42 -> 34): a normal human footprint. The auto-squeeze in UpdateAutoSqueeze shrinks
	// this down to ~14 when threading a narrow slot, so tight crevices are passable without making the player
	// permanently skinny.
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

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
	TeacherWeaponRoot = CreateJoint(TEXT("TeacherWeaponRoot"), RoleModelRoot);
	TeacherWeaponMesh = CreateAvatarMesh(TEXT("TeacherWeaponMesh"), TeacherWeaponRoot);
	TeacherWeaponHandleMesh = CreateAvatarMesh(TEXT("TeacherWeaponHandleMesh"), TeacherWeaponRoot);
	TeacherWeaponHeadMesh = CreateAvatarMesh(TEXT("TeacherWeaponHeadMesh"), TeacherWeaponRoot);
	TeacherWeaponBladeMesh = CreateAvatarMesh(TEXT("TeacherWeaponBladeMesh"), TeacherWeaponRoot);
	TeacherWeaponGripMesh = CreateAvatarMesh(TEXT("TeacherWeaponGripMesh"), TeacherWeaponRoot);
	UStaticMeshComponent* WeaponParts[] = {
		TeacherWeaponMesh,
		TeacherWeaponHandleMesh,
		TeacherWeaponHeadMesh,
		TeacherWeaponBladeMesh,
		TeacherWeaponGripMesh,
		TeacherWeaponTrailMesh
	};
	for (UStaticMeshComponent* WeaponPart : WeaponParts)
	{
		if (WeaponPart)
		{
			WeaponPart->SetOwnerNoSee(false);
		}
	}

	ConfigureLowPolyAvatar();

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Camera);
	Flashlight->SetIntensity(9000.0f);
	Flashlight->SetAttenuationRadius(2200.0f);
	Flashlight->SetInnerConeAngle(18.0f);
	Flashlight->SetOuterConeAngle(34.0f);
	Flashlight->SetVolumetricScatteringIntensity(5.0f);
	Flashlight->SetVisibility(false);

	HunterHueLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HunterHueLight"));
	HunterHueLight->SetupAttachment(GetCapsuleComponent());
	HunterHueLight->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));
	HunterHueLight->SetLightColor(FLinearColor(1.0f, 0.12f, 0.055f, 1.0f));
	HunterHueLight->SetIntensity(0.0f);
	HunterHueLight->SetAttenuationRadius(BHHunterHueLightRadius);
	HunterHueLight->SetSourceRadius(96.0f);
	HunterHueLight->SetSoftSourceRadius(180.0f);
	HunterHueLight->SetCastShadows(false);
	HunterHueLight->SetVolumetricScatteringIntensity(0.22f);
	HunterHueLight->SetVisibility(false);

	PowerupComponent = CreateDefaultSubobject<UBHPowerupComponent>(TEXT("PowerupComponent"));

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

	WalkSpeed = 530.0f;   // brisk exploration pace (360 -> 470 -> 530); scales per-role so hunter/survivor balance holds
	SprintSpeed = 900.0f;
	MaxStamina = 100.0f;
	InteractDistance = 150.0f;
	CaptureDistance = 100.0f;
	FlashlightDrainPerSecond = 0.0f;
	ScanCooldownSeconds = 0.0f;
	DecoyCooldownSeconds = 0.0f;
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings)
	{
		InteractDistance = FMath::Max(150.0f, Settings->InteractDistance);
		CaptureDistance = FMath::Max(100.0f, Settings->CaptureDistance);
		FlashlightDrainPerSecond = FMath::Max(0.0f, Settings->FlashlightDrainPerSecond);
		ScanCooldownSeconds = FMath::Max(0.0f, Settings->ScanCooldownSeconds);
		DecoyCooldownSeconds = FMath::Max(0.0f, Settings->DecoyCooldownSeconds);
	}
	StaminaDrainPerSecond = 20.0f;
	StaminaRecoveryPerSecond = 14.0f;
	MovementSpecialState = EBHMovementSpecialState::None;
	bTeacherCaptureAttackActive = false;
	TeacherCaptureAttackStartServerTime = -999.0f;
	TeacherCaptureAttackResolveServerTime = -999.0f;
	TeacherCaptureAttackEndServerTime = -999.0f;
	TeacherCaptureNextAllowedServerTime = -999.0f;
	bFlashlightOn = false;
	FlashlightBattery = 100.0f;
	bFlashlightEmptyTelemetryReported = false;
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
	LastFootstepStimulusTime = -999.0f;
	LastFlashlightAudioCueTime = -999.0f;
	LastTeacherProximityAudioTime = -999.0f;
	FootstepStimulusDistanceAccumulator = 0.0f;
	StaminaRecoveryLockedUntil = -999.0f;
	LastStaminaWarningTime = -999.0f;
	LastHidingPanicMessageTime = -999.0f;
	LastLockerNoiseTime = -999.0f;
	LockerExitTime = -999.0f;
	LastForcedBreathNoiseTime = -999.0f;
	LastPanicBreathNoiseTime = -999.0f;
	LastDetentionNoiseTime = -999.0f;
	AntiCampMoveBurstSeconds = 0.0f;
	AntiCampMoveBurstDistance = 0.0f;
	AntiCampLastSatisfiedTime = -999.0f;
	AntiCampLastMeaningfulMoveTime = -999.0f;
	AntiCampLastSampleLocation = FVector::ZeroVector;
	LastAntiCampWarningTime = -999.0f;
	LastAntiCampNoiseTime = -999.0f;
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
	ViewFeelCameraLocation = DefaultCameraLocation;
	POVAnimRotationCurrent = FRotator::ZeroRotator;
	POVAnimLocationCurrent = FVector::ZeroVector;
	LocalSpecialAnimStartTime = -999.0f;
	LocalSpecialAnimState = EBHMovementSpecialState::None;
	FlashlightPulseTime = 0.0f;
	LastBHopJumpInputTime = -999.0f;
	SpecialMoveStartTime = -999.0f;
	SpecialMoveEndTime = -999.0f;
	SpecialMoveCooldownEndTime = -999.0f;
	SpecialMoveDistanceTravelled = 0.0f;
	SpecialMoveDirection = FVector::ForwardVector;
	LastCaptureEvasionTime = -999.0f;
	LastTeacherCounterplayHintTime = -999.0f;
	DefaultCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	DefaultCapsuleRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	MovementFailurePulse = 0.0f;
	LastMovementFailureTime = -999.0f;
	LastMovementFailureReason = FString();
	bKeyboardLookLeft = false;
	bKeyboardLookRight = false;
	bKeyboardLookUp = false;
	bKeyboardLookDown = false;
	bBHopJumpQueued = false;
	bSprintInputHeld = false;
	bProneInputHeld = false;
	bSpecialMoveEndsProne = false;
	bSpecialMoveEndProneRequiresInput = false;
	bProneCollisionApplied = false;
	bTeacherCaptureAttackResolved = false;
	SpecialMoveNoiseEventMask = 0;
	CosmeticMovementSpecialState = EBHMovementSpecialState::None;
	bUsingRoleModel = false;
	LastRoleAnimationName = NAME_None;
	TeacherMeleeSwingStartTime = -999.0f;
	TeacherMeleeSwingEndTime = -999.0f;
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
	JumpMaxHoldTime = 0.0f;
	GetCharacterMovement()->JumpZVelocity = 395.0f;
	GetCharacterMovement()->AirControl = 0.16f;
	GetCharacterMovement()->BrakingDecelerationFalling = 80.0f;
	GetCharacterMovement()->FallingLateralFriction = 0.12f;
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
		SmoothedBaseFOV = DefaultCameraFOV;
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

void ABHCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Tear down any per-player roof service lights this client spawned (e.g. on death/respawn or level travel).
	SetRoofServiceLightsLocal(false);
	Super::EndPlay(EndPlayReason);
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
	MovementFailurePulse = FMath::FInterpTo(MovementFailurePulse, 0.0f, DeltaSeconds, 5.5f);

	// Track the question station the local player is aiming at so typed numeric entry knows
	// when a Calculation question is active. Owning-client/host human pawns only.
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		AActor* FocusActor = nullptr;
		FindInteractableFromView(FocusActor, 225.0f);
		SetClientFocusedQuestionStation(Cast<ABHObjectiveStation>(FocusActor));
		UpdateQuestionInteractionState();
	}

	// Comfort (local viewer only): a remote body that crowds right up against this client's camera fades out so
	// it doesn't block the view. Each character evaluates itself against the local viewpoint, so it self-corrects
	// when the setting changes or the local pawn swaps. Dedicated servers have no local view, so skip them.
	if (!IsLocallyControlled() && GetNetMode() != NM_DedicatedServer)
	{
		UpdateProximityPlayerHideFromLocalView();
	}

	if (HasAuthority() && IsSpecialMoveActive())
	{
		UpdateSpecialMoveAuthority(DeltaSeconds);
		if (GetWorld() && GetWorld()->GetTimeSeconds() >= SpecialMoveEndTime)
		{
			FinishSpecialMoveAuthority();
		}
	}

	if (HasAuthority())
	{
		UpdateTeacherCaptureAttackAuthority(DeltaSeconds);
		EndStaleHeldInteractionAuthority();
	}

	if (HasAuthority())
	{
		// Enforce the final-escape / jumpscare input lock on the server, not just via CanAct() (which
		// only gates *local* input). The replicated freeze flags otherwise let a modified client keep
		// walking through the scripted cutscene and jumpscares; stop its movement component the same
		// way capture does. Captured/escaped (bOutOfPlay) and locker-hidden pawns own their own
		// movement state, so leave those alone and only hand movement back to a normal in-play pawn.
		const ABHPlayerState* FreezePS = GetPlayerState<ABHPlayerState>();
		const ABHGameState* FreezeGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		const bool bFrozenByLock = FreezeGS && (FreezeGS->bPlayerInputFrozen || (FreezeGS->bHunterInputFrozen && FreezePS && FreezePS->IsAliveHunter()));
		if (bFrozenByLock && !bOutOfPlay && !bHiddenInLocker)
		{
			bMovementFrozenByServer = true;
			// Re-assert every tick rather than only on the edge: if anything else hands movement back
			// while the lock is still on (e.g. an overlapping jumpscare's own restore timer), clamp it
			// again next frame. Cheap — only touches the component when the mode has drifted off None.
			if (UCharacterMovementComponent* Movement = GetCharacterMovement())
			{
				if (Movement->MovementMode != MOVE_None)
				{
					Movement->StopMovementImmediately();
					Movement->DisableMovement();
				}
			}
		}
		else if (bMovementFrozenByServer)
		{
			bMovementFrozenByServer = false;
			// Only restore if we're back to a normal, controllable pawn. If capture/locker took over
			// while frozen, they disabled movement deliberately — don't re-enable underneath them.
			if (CanAct())
			{
				if (UCharacterMovementComponent* Movement = GetCharacterMovement())
				{
					if (Movement->MovementMode == MOVE_None)
					{
						Movement->SetMovementMode(MOVE_Walking);
					}
				}
				ApplyMovementSpecialState();
			}
		}
	}

	if (HasAuthority() && bFlashlightOn)
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		const bool bInfiniteFlashlight = (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester) || (BHGS && BHGS->bTestMode);
		// The intermission train dims the flashlight to near-uselessness (see UpdateFlashlightFeel); do
		// not also burn battery while parked on the train so players board the next round with charge.
		const bool bTrainIntermission = BHGS && (BHGS->RoundPhase == EBHRoundPhase::Intermission || BHGS->ActiveLevelName.Equals(TEXT("TrainIntermission"), ESearchCase::IgnoreCase));
		if (bInfiniteFlashlight)
		{
			FlashlightBattery = 100.0f;
			bFlashlightEmptyTelemetryReported = false;
		}
		else if (!bTrainIntermission)
		{
			const float PreviousBattery = FlashlightBattery;
			FlashlightBattery = FMath::Max(0.0f, FlashlightBattery - GetEffectiveFlashlightDrainPerSecond() * DeltaSeconds);
			if (FlashlightBattery <= 0.0f)
			{
				if (PreviousBattery > 0.0f && !bFlashlightEmptyTelemetryReported)
				{
					if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
					{
						BHGM->RecordPlaytestTelemetryMarker(TEXT("battery_starved"), GetActorLocation(), TEXT("flashlight_empty"), GetBHPlayerState());
					}
					bFlashlightEmptyTelemetryReported = true;
				}
				bFlashlightOn = false;
				ApplyFlashlightState();
				BroadcastFlashlightAudioCue(false, true);
			}

			// A student caught in the Teacher's blackout: the dark keeps choking the beam, so emit a
			// stuttering "struggling light" cue (the call rate-limits itself to an erratic cadence).
			if (bFlashlightOn && IsInTeacherBlackout())
			{
				BroadcastFlashlightStruggleAudioCue();
			}
		}
	}

	if (HasAuthority())
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		if (BHPS && BHPS->IsAliveSurvivor() && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt)
		{
			const float Speed2D = GetVelocity().Size2D();
			const float StressNoiseAlpha = FMath::Max(BHFearPanicAlpha(this), BHDreadStrainAlpha(this));
			if (!bHiddenInLocker && GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround() && Speed2D > 120.0f)
			{
				FootstepStimulusDistanceAccumulator += Speed2D * DeltaSeconds;
				const bool bProneMove = IsProne();
				const float RoleWalk = BHRoleWalkSpeed(BHPS, WalkSpeed);
				const bool bLikelySprinting = !bProneMove && Speed2D > RoleWalk * 1.18f;
				const float StepDistance = (bProneMove ? 245.0f : (bLikelySprinting ? 275.0f : 420.0f)) / FMath::Lerp(1.0f, 1.22f, StressNoiseAlpha);
				const float StepCooldown = (bProneMove ? 1.35f : (bLikelySprinting ? 0.72f : 1.45f)) / FMath::Lerp(1.0f, 1.18f, StressNoiseAlpha);
				const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				if (FootstepStimulusDistanceAccumulator >= StepDistance && Now - LastFootstepStimulusTime >= StepCooldown)
				{
					FootstepStimulusDistanceAccumulator = 0.0f;
					LastFootstepStimulusTime = Now;
					const EBHFootstepSurface Surface = ResolveFootstepSurface();
					const float StepStrength = (bProneMove ? 0.30f * BHRoleProneNoiseMultiplier(BHPS) : (bLikelySprinting ? 1.0f : 0.42f)) * BHHorrorNoiseMultiplier(this, BHPS);
					const bool bPanickedStep = StressNoiseAlpha >= 0.55f;
					EmitFootstepStimulus(
						StepStrength,
						bProneMove
							? TEXT("prone crawl")
							: (bPanickedStep
							? (bLikelySprinting ? TEXT("panicked running footstep") : TEXT("unsteady footstep"))
							: (bLikelySprinting ? TEXT("running footstep") : TEXT("careful footstep"))),
						Surface);
				}
			}
			else
			{
				FootstepStimulusDistanceAccumulator = 0.0f;
			}

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

				if (!bHiddenInLocker && NearestHunterDistance <= 1550.0f)
				{
					SendTeacherProximityAudioCue(DistanceAlpha, bHunterHasSight);
				}

				if (bHiddenInLocker && NearestHunterDistance <= BHHorrorCloseThreatRange)
				{
					const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
					if (Now - LastHidingPanicMessageTime > 9.0f)
					{
						LastHidingPanicMessageTime = Now;
						SendStatusMessage(TEXT("Something is outside the door. Stay still."));
						if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
						{
							const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
							FBHScareEventSpec Spec;
							Spec.EventType = EBHScareEventType::LockerKnock;
							Spec.Target = this;
							Spec.Origin = CurrentLocker ? CurrentLocker->GetActorLocation() : GetActorLocation();
							Spec.Intensity = FMath::Clamp(0.46f + DistanceAlpha * 0.38f, 0.0f, 1.0f);
							Spec.AudioAsset = Settings ? Settings->LockerKnockSound : TSoftObjectPtr<USoundBase>();
							Spec.Message = TEXT("The locker knocks from outside. Stay still.");
							Spec.FlashIntensity = 0.02f;
							BHGM->TriggerAtmosphereCue(Spec);
						}
					}
				}
			}
			else
			{
				Fear = FMath::Max(0.0f, Fear - 4.5f * DeltaSeconds);
				const float HiddenDecay = bHiddenInLocker ? 1.2f : 6.5f;
				Dread = FMath::Max(0.0f, Dread - HiddenDecay * DeltaSeconds);
			}

			const int32 ObjectiveRequired = FMath::Max(1, BHGS->BreakersRequired + BHGS->SideObjectivesRequired);
			const int32 ObjectiveCompleted = BHGS->BreakersCompleted + BHGS->SideObjectivesCompleted;
			const float ObjectivePressureAlpha = FMath::Clamp(static_cast<float>(ObjectiveCompleted) / static_cast<float>(ObjectiveRequired), 0.0f, 1.0f);
			const float PresenceStressAlpha = BHHorrorThresholdAlpha(BHGS->PresenceLevel, 42.0f, 100.0f);
			const float TimerStressAlpha = BHGS->RemainingTime > 0 ? BHHorrorThresholdAlpha(180.0f - static_cast<float>(BHGS->RemainingTime), 0.0f, 180.0f) : 0.0f;
			const float DarknessStress = (!bFlashlightOn && !bHiddenInLocker) ? 0.95f : 0.0f;
			const float ExitStress = BHGS->bExitUnlocked ? 2.2f : 0.0f;
			const float AmbientDreadPerSecond =
				PresenceStressAlpha * 2.7f
				+ ObjectivePressureAlpha * 0.75f
				+ TimerStressAlpha * 1.45f
				+ DarknessStress
				+ ExitStress;
			if (AmbientDreadPerSecond > 0.0f)
			{
				Dread = FMath::Clamp(Dread + AmbientDreadPerSecond * DeltaSeconds, 0.0f, 100.0f);
			}

			UpdateAntiCampPressureAuthority(DeltaSeconds, Speed2D, BHGS, BHPS);

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

			const float PanicBreathAlpha = FMath::Max(BHHorrorThresholdAlpha(Fear, 74.0f, 100.0f), BHHorrorThresholdAlpha(Dread, 82.0f, 100.0f));
			if (!bHiddenInLocker && Speed2D > 150.0f && PanicBreathAlpha > 0.0f)
			{
				const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				const float BreathCooldown = FMath::Lerp(18.0f, 9.0f, PanicBreathAlpha);
				if (Now - LastPanicBreathNoiseTime > BreathCooldown)
				{
					LastPanicBreathNoiseTime = Now;
					SendStatusMessage(TEXT("Your breathing gives you away."));
					if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
					{
						BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("panicked breathing"));
					}
				}
			}
		}
		else
		{
			HiddenSeconds = 0.0f;
			ResetAntiCampTrackingAuthority();
			Fear = FMath::Max(0.0f, Fear - 8.0f * DeltaSeconds);
			Dread = FMath::Max(0.0f, Dread - 10.0f * DeltaSeconds);
			DetentionMarkRemaining = 0.0f;
		}
	}

	UpdateViewFeel(DeltaSeconds);
	UpdateFlashlightFeel(DeltaSeconds);
	UpdateLowPolyAvatar(DeltaSeconds);

	if (bBHopJumpQueued)
	{
		TryBHopJump();
	}

	if (CanAct())
	{
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const float HunterPenaltyMultiplier = BHFinalEscapeHunterPenaltyActive(GetWorld(), this) ? 0.72f : 1.0f;
		const float HorrorWalkMultiplier = BHHorrorWalkSpeedMultiplier(this, BHPS);
		const float TeacherCaptureMoveMultiplier = (BHPS && BHPS->IsAliveHunter() && IsTeacherCaptureAttackActive()) ? BHTeacherCaptureMoveMultiplier : 1.0f;
		const float CurrentWalkSpeed = BHRoleWalkSpeed(BHPS, WalkSpeed) * HunterPenaltyMultiplier * TeacherCaptureMoveMultiplier * HorrorWalkMultiplier;
		const float CurrentSprintSpeed = BHRoleSprintSpeed(BHPS, SprintSpeed)
			* (PowerupComponent ? PowerupComponent->GetSprintSpeedMultiplier() : 1.0f)
			* HunterPenaltyMultiplier
			* TeacherCaptureMoveMultiplier
			* BHHorrorSprintSpeedMultiplier(this, BHPS);
		if (Movement && IsProne())
		{
			Movement->MaxWalkSpeed = BHRoleProneSpeed(BHPS) * BHHorrorWalkSpeedMultiplier(this, BHPS);
			Movement->MaxWalkSpeedCrouched = Movement->MaxWalkSpeed;
		}
		else if (Movement && IsSpecialMoveActive())
		{
			Movement->MaxWalkSpeedCrouched = 205.0f * HorrorWalkMultiplier;
		}
		else if (Movement)
		{
			Movement->MaxWalkSpeedCrouched = 205.0f * HorrorWalkMultiplier;
		}
		if (!IsProne() && !IsSpecialMoveActive() && Movement && IsPlayerControlled() && Movement->MaxWalkSpeed <= FMath::Max(WalkSpeed, CurrentWalkSpeed) + 1.0f)
		{
			Movement->MaxWalkSpeed = CurrentWalkSpeed;
		}
		else if (!IsProne() && !IsSpecialMoveActive() && Movement && IsPlayerControlled() && Movement->MaxWalkSpeed > CurrentWalkSpeed + 1.0f && Movement->MaxWalkSpeed > CurrentSprintSpeed + 1.0f)
		{
			Movement->MaxWalkSpeed = CurrentSprintSpeed;
		}

		// The per-second sprint drain only bites while RUNNING ON THE GROUND. Once airborne it stops (and so does
		// regen, see below), so a clean bunny-hop chain -- tap sprint to reach sprint speed, then chain jumps --
		// holds top speed paying only the flat per-jump cost instead of the continuous ground drain. That makes a
		// well-timed chain a little cheaper than holding sprint, which is exactly what the Advanced Movement guide
		// promises. Without the IsMovingOnGround() gate the drain kept billing mid-air, making hops strictly MORE
		// expensive than the help claimed (see the bunny-hop discrepancy fix).
		const bool bTryingToSprint = Movement
			&& Movement->IsMovingOnGround()
			&& !IsProne()
			&& !IsSpecialMoveActive()
			&& Movement->MaxWalkSpeed >= CurrentSprintSpeed - 1.0f
			&& GetVelocity().SizeSquared2D() > FMath::Square(30.0f);
		if (HasAuthority())
		{
			if (bTryingToSprint)
			{
				MarkTutorialAction(TutorialActSprintBit); // movement-tutorial: actually running at sprint speed
				const float DrainMultiplier = BHRoleSprintDrainMultiplier(BHPS)
					* (PowerupComponent ? PowerupComponent->GetStaminaDrainMultiplier() : 1.0f)
					* BHHorrorStaminaDrainMultiplier(this, BHPS);
				Stamina = FMath::Max(0.0f, Stamina - (StaminaDrainPerSecond / BHLevelStaminaScale(BHPS)) * DrainMultiplier * DeltaSeconds);
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
			else if (Movement && Movement->IsMovingOnGround())
			{
				const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				if (Now >= StaminaRecoveryLockedUntil)
				{
					const float RecoveryMultiplier = BHHorrorStaminaRecoveryMultiplier(this, BHPS)
						* BHRoleStaminaRecoveryMultiplier(BHPS)
						* (PowerupComponent ? PowerupComponent->GetStaminaRecoveryMultiplier() : 1.0f)
						* (IsProne() ? BHProneStaminaRecoveryMultiplier : 1.0f);
					Stamina = FMath::Min(MaxStamina, Stamina + StaminaRecoveryPerSecond * BHLevelStaminaScale(BHPS) * RecoveryMultiplier * DeltaSeconds);
				}
			}
		}
		else if (Stamina <= 0.0f && Movement && Movement->MaxWalkSpeed >= CurrentSprintSpeed - 1.0f)
		{
			Movement->MaxWalkSpeed = CurrentWalkSpeed;
		}
	}

	if (const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		const int32 HeadwearIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Headwear, BHPS->AvatarHeadwearIndex);
		const int32 GearIndex = 0;
		const bool bAvatarChanged = BHPS->AvatarIndex != LastAppliedAvatarIndex
			|| BHPS->PlayerRole != LastAppliedAvatarRole
			|| !BHPS->AvatarColor.Equals(LastAppliedAvatarColor, 0.005f)
			|| HeadwearIndex != LastAppliedAvatarHeadwearIndex
			|| GearIndex != LastAppliedAvatarGearIndex;
		if (bAvatarChanged)
		{
			ApplyAvatarStyle();
		}
	}

	// Auto-squeeze runs last so its walk-speed penalty isn't overwritten by the speed logic above.
	UpdateAutoSqueeze(DeltaSeconds);

	UpdateTrainPawnCollision();
}

void ABHCharacter::UpdateTrainPawnCollision()
{
	// Disable player<->player (and player<->teacher) blocking while the train intermission is running, so the
	// crowded carriage never gets people stuck on each other. Driven by the replicated train phase, so it works
	// on clients too; only touched on the boundary (entering/leaving the intermission), not every tick.
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bInTrain = BHGS && BHGS->TrainPhase != EBHTrainPhase::Inactive;
	if (bInTrain == bTrainPawnCollisionOff)
	{
		return;
	}
	bTrainPawnCollisionOff = bInTrain;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, bInTrain ? ECR_Ignore : ECR_Block);
	}
}

void ABHCharacter::UpdateAutoSqueeze(float DeltaSeconds)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UWorld* World = GetWorld();
	if (!Capsule || !World)
	{
		return;
	}

	constexpr float SqueezeRadius = 14.0f;
	const float StandRadius = DefaultCapsuleRadius;   // 34

	// Only the authority and the owning client simulate this pawn's movement; both must run identically so the
	// shrink stays networked-consistent. Skip while prone/crouched/special-move (they own the capsule) and for
	// AI/hunter pawns (only human survivors squeeze). When it can't apply, restore the standing radius.
	const bool bCanSqueeze = (HasAuthority() || IsLocallyControlled())
		&& IsPlayerControlled() && !IsProne() && !bIsCrouched && !IsSpecialMoveActive();
	if (!bCanSqueeze)
	{
		bAutoSqueezing = false;
		const float CurRadius = Capsule->GetUnscaledCapsuleRadius();
		if (CurRadius < StandRadius - 0.5f)
		{
			Capsule->SetCapsuleSize(FMath::FInterpTo(CurRadius, StandRadius, DeltaSeconds, 12.0f), Capsule->GetUnscaledCapsuleHalfHeight(), false);
		}
		return;
	}

	const float Probe = StandRadius + 30.0f;
	const FVector Loc = GetActorLocation();
	const FVector Right = GetActorRightVector();
	const FVector Fwd = GetActorForwardVector();
	FCollisionQueryParams Params(FName(TEXT("BHAutoSqueeze")), false, this);
	auto SidesBlocked = [&](const FVector& Origin)
	{
		FHitResult H;
		const bool bR = World->LineTraceSingleByChannel(H, Origin, Origin + Right * Probe, ECC_WorldStatic, Params);
		const bool bL = World->LineTraceSingleByChannel(H, Origin, Origin - Right * Probe, ECC_WorldStatic, Params);
		return bR && bL;   // a wall close on BOTH sides => a narrow slot
	};

	// Sample at the capsule centre and a little ahead, so the slot is detected just before you reach it.
	const bool bNarrow = SidesBlocked(Loc) || SidesBlocked(Loc + Fwd * (StandRadius + 12.0f));
	bAutoSqueezing = bNarrow;

	const float TargetRadius = bNarrow ? SqueezeRadius : StandRadius;
	const float CurRadius = Capsule->GetUnscaledCapsuleRadius();
	if (!FMath::IsNearlyEqual(CurRadius, TargetRadius, 0.4f))
	{
		Capsule->SetCapsuleSize(FMath::FInterpTo(CurRadius, TargetRadius, DeltaSeconds, 12.0f), Capsule->GetUnscaledCapsuleHalfHeight(), false);
	}

	if (bNarrow)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MaxWalkSpeed = FMath::Min(Move->MaxWalkSpeed, 170.0f);   // slow, deliberate squeeze
		}
	}
}

// --- Advanced movement link tuning (see Docs/MOVEMENT.md). Declared here (above Landed/ExitLocker, the earliest
// users) so every consuming function below can read them. The frame-perfect flow-chain cvars live further down with
// TryStartSpecialMoveAuthority; these are the newer noise/traversal knobs. ---------------------------------------
// Quiet-roll discipline: a clean, full-distance roll emits its impact stimulus at this fraction of the normal
// loudness (a stealth reward for good spacing); a roll that bonks a wall stays at the full 1.0. 1.0 disables it.
static TAutoConsoleVariable<float> CVarBHQuietRollCleanScale(
	TEXT("bh.QuietRollCleanScale"),
	0.65f,
	TEXT("Roll-impact noise multiplier for a clean (no wall contact) roll; bonked rolls stay 1.0. 1.0 = off. (Default 0.65)"),
	ECVF_Default);
// Flow-chain noise amnesty: each chained link reduces that move's noise by this much per link (a clean operator
// moves quietly), but reaching the chain cap fires one loud "overextension" burst. 0.0 disables the amnesty.
static TAutoConsoleVariable<float> CVarBHMomentumChainNoiseScale(
	TEXT("bh.MomentumChainNoiseScale"),
	0.18f,
	TEXT("Per-link noise reduction for a chained special move (noise *= 1 - k*chainDepth, floored). 0 = off. (Default 0.18)"),
	ECVF_Default);
// Dodge-roll camera style override (motion sickness). -1 = use the player's GConfig setting [BlackoutHunt.Comfort]
// RollCamStyle (default Dip); 0=Off, 1=Subtle, 2=Dip, 3=Full somersault. Reduced-camera-shake comfort clamps to Subtle.
static TAutoConsoleVariable<int32> CVarBHRollCamStyle(
	TEXT("bh.RollCamStyle"),
	-1,
	TEXT("Dodge-roll camera: -1 = use the player's comfort setting; 0=Off, 1=Subtle, 2=Dip, 3=Full forward somersault."),
	ECVF_Default);
// Silent slide-stop: releasing Prone within this early fraction of a Slide brakes into a quiet crouch instead of
// standing/proning (trades distance for silence). 0 disables the early-brake outcome.
static TAutoConsoleVariable<float> CVarBHSlideStopWindow(
	TEXT("bh.SlideStopWindow"),
	0.45f,
	TEXT("Release Prone before this normalized slide time to brake into a quiet crouch. 0 = off. (Default 0.45)"),
	ECVF_Default);
// Drop-roll: a roll requested this many seconds before landing is buffered and fired on touchdown, suppressing the
// hard-landing noise (a skill-timed silent landing). 0 disables drop-roll buffering.
static TAutoConsoleVariable<float> CVarBHDropRollWindow(
	TEXT("bh.DropRollWindow"),
	0.45f,
	TEXT("Buffer a roll requested within this many seconds of landing and fire it on touchdown (silent landing). 0 = off. (Default 0.45)"),
	ECVF_Default);
// Roll-out-of-locker: holding Sprint while leaving a locker fires a capture-immune forward roll instead of the
// exposed standing pop. 1 = on.
static TAutoConsoleVariable<int32> CVarBHLockerRollExit(
	TEXT("bh.LockerRollExit"),
	1,
	TEXT("1 (default) = holding Sprint on locker exit fires a forward roll; 0 = always a plain stand-up exit."),
	ECVF_Default);
// Crawl-gap dash: a dive started FROM prone uses this noise multiplier (a quieter, localizable scrape) so threading
// a crawl gap costs information but not a full loud standing dive. 1.0 = identical to a standing dive.
static TAutoConsoleVariable<float> CVarBHCrawlDashNoiseScale(
	TEXT("bh.CrawlDashNoiseScale"),
	0.55f,
	TEXT("Noise multiplier for a dive started from prone (crawl-gap dash). 1.0 = same as a standing dive. (Default 0.55)"),
	ECVF_Default);
// Vault / mantle: lets a roll or slide whose forward path is blocked by a LOW, vaultable ledge convert into a short
// up-and-over hop that finally consumes FBHMovementCurveTuning.VerticalImpulse. Ships DARK (0) -- it adds vertical
// motion to the otherwise horizontal special-move mover and wants playtest + collision review before going live.
static TAutoConsoleVariable<int32> CVarBHVaultTech(
	TEXT("bh.VaultTech"),
	0,
	TEXT("1 = roll/slide into a low ledge vaults over it (consumes Curve.VerticalImpulse); 0 (default) = dead-stop at the obstacle as before."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHVaultMaxLedgeHeight(
	TEXT("bh.VaultMaxLedgeHeight"),
	95.0f,
	TEXT("Maximum obstacle top height (cm above the floor) that bh.VaultTech will vault rather than dead-stop. (Default 95)"),
	ECVF_Default);

void ABHCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (!HasAuthority())
	{
		return;
	}

	StaminaRecoveryLockedUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + BHPostLandingStaminaRecoveryLockSeconds;

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;

	// Drop-roll: if the player buffered a roll just before touchdown (held Sprint + tapped Crouch while falling),
	// convert the landing into a silent forward roll instead of the loud "hard landing" thud. The roll starts only
	// AFTER touchdown, so it gets its normal i-frame window -- not a perpetual airborne capture shield.
	const float DropRollWindow = CVarBHDropRollWindow.GetValueOnGameThread();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	// Drop-roll engages two ways: the player is HOLDING Sprint+Crouch as they touch down (the reliable path -- no
	// timing needed, just hold both through the fall), OR they tapped Sprint+Crouch within the buffer window just
	// before landing. Either converts the landing into a silent roll. Works for any role that can roll; the roll's
	// own gates (TryStartSpecialMoveAuthority) still apply.
	const bool bHeldDropRoll = bSprintInputHeld && bCrouchInputHeld;
	const bool bBufferedDropRoll = DropRollWindow > 0.0f
		&& (Now - LastDropRollInputTime) >= 0.0f && (Now - LastDropRollInputTime) <= DropRollWindow;
	if ((bHeldDropRoll || bBufferedDropRoll) && !IsSpecialMoveActive() && !IsProne())
	{
		LastDropRollInputTime = -999.0f;
		if (TryStartSpecialMoveAuthority(EBHMovementSpecialState::Rolling, false, false))
		{
			MarkTutorialAction(TutorialActDropRollBit); // movement-tutorial: a drop-roll specifically fired
			return; // silent landing: the roll-start stimulus replaces the hard-landing thud
		}
	}

	if (BHPS && BHPS->IsAliveSurvivor() && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt)
	{
		EmitFootstepStimulus(0.95f * BHHorrorNoiseMultiplier(this, BHPS), TEXT("hard landing"), ResolveFootstepSurface(&Hit));
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
	PlayerInputComponent->BindAction(TEXT("Prone"), IE_Pressed, this, &ABHCharacter::StartProne);
	PlayerInputComponent->BindAction(TEXT("Prone"), IE_Released, this, &ABHCharacter::StopProne);
	PlayerInputComponent->BindAction(TEXT("Capture"), IE_Pressed, this, &ABHCharacter::TryCapture);
	PlayerInputComponent->BindAction(TEXT("Scan"), IE_Pressed, this, &ABHCharacter::UseScan);
	PlayerInputComponent->BindAction(TEXT("HunterPower"), IE_Pressed, this, &ABHCharacter::UseHunterPower);
	PlayerInputComponent->BindAction(TEXT("Decoy"), IE_Pressed, this, &ABHCharacter::DropDecoy);
	PlayerInputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ABHCharacter::UsePowerupSlotOne);
	PlayerInputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ABHCharacter::UsePowerupSlotTwo);
	PlayerInputComponent->BindKey(EKeys::F3, IE_Pressed, this, &ABHCharacter::UsePowerupSlotThree);
	PlayerInputComponent->BindKey(EKeys::F4, IE_Pressed, this, &ABHCharacter::UsePowerupSlotFour);
	PlayerInputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ABHCharacter::UsePowerupSlotFive);
	PlayerInputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ABHCharacter::UsePowerupSlotSix);
	PlayerInputComponent->BindKey(EKeys::Insert, IE_Pressed, this, &ABHCharacter::TesterGrantTrainResources);
	PlayerInputComponent->BindKey(EKeys::Home, IE_Pressed, this, &ABHCharacter::TesterOpenTrainIntermission);
	PlayerInputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &ABHCharacter::TesterAdvanceTrainPhase);
	PlayerInputComponent->BindKey(EKeys::End, IE_Pressed, this, &ABHCharacter::TesterLoadFinalStation);
	PlayerInputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &ABHCharacter::TesterTriggerFinalEscape);
	PlayerInputComponent->BindKey(EKeys::Delete, IE_Pressed, this, &ABHCharacter::TesterForceFinalRecap);
	PlayerInputComponent->BindKey(EKeys::NumPadFive, IE_Pressed, this, &ABHCharacter::TesterGrantTrainResources);
	PlayerInputComponent->BindKey(EKeys::NumPadSix, IE_Pressed, this, &ABHCharacter::TesterOpenTrainIntermission);
	PlayerInputComponent->BindKey(EKeys::NumPadSeven, IE_Pressed, this, &ABHCharacter::TesterAdvanceTrainPhase);
	PlayerInputComponent->BindKey(EKeys::NumPadEight, IE_Pressed, this, &ABHCharacter::TesterLoadFinalStation);
	PlayerInputComponent->BindKey(EKeys::NumPadNine, IE_Pressed, this, &ABHCharacter::TesterTriggerFinalEscape);
	PlayerInputComponent->BindKey(EKeys::NumPadZero, IE_Pressed, this, &ABHCharacter::TesterForceFinalRecap);
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ABHCharacter::SubmitAnswerOne);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ABHCharacter::SubmitAnswerTwo);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ABHCharacter::SubmitAnswerThree);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ABHCharacter::SubmitAnswerFour);
	PlayerInputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &ABHCharacter::SubmitAnswerOne);
	PlayerInputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &ABHCharacter::SubmitAnswerTwo);
	PlayerInputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &ABHCharacter::SubmitAnswerThree);
	PlayerInputComponent->BindKey(EKeys::NumPadFour, IE_Pressed, this, &ABHCharacter::SubmitAnswerFour);
	// Typed numeric entry for Calculation questions. These top-row keys are otherwise unused;
	// keys 1-4 reuse the answer handlers (routed to digit entry when a calc question is focused).
	PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ABHCharacter::NumericEntryFive);
	PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ABHCharacter::NumericEntrySix);
	PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ABHCharacter::NumericEntrySeven);
	PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ABHCharacter::NumericEntryEight);
	PlayerInputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ABHCharacter::NumericEntryNine);
	PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ABHCharacter::NumericEntryZero);
	PlayerInputComponent->BindKey(EKeys::Period, IE_Pressed, this, &ABHCharacter::NumericEntryDecimal);
	PlayerInputComponent->BindKey(EKeys::Decimal, IE_Pressed, this, &ABHCharacter::NumericEntryDecimal);
	PlayerInputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &ABHCharacter::NumericEntryMinus);
	PlayerInputComponent->BindKey(EKeys::Subtract, IE_Pressed, this, &ABHCharacter::NumericEntryMinus);
	PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &ABHCharacter::NumericEntryBackspace);
	// Mouse-driven question answering. Tab toggles the cursor over a focused question panel; the left
	// mouse button (otherwise the Capture key, gated in TryCapture while the cursor is up) clicks
	// choices / drags arrangement pieces. Number keys 1-4 still work, so keyboard/bots are unaffected.
	PlayerInputComponent->BindAction(TEXT("NodeMarker"), IE_Pressed, this, &ABHCharacter::UseNodeMarker);
	PlayerInputComponent->BindAction(TEXT("ResetToTrain"), IE_Pressed, this, &ABHCharacter::RequestResetToTrainInterior);
	// Functional sit toggle (C). Free key -- Crouch is LeftCtrl. Pressing a movement key stands you back up.
	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &ABHCharacter::ToggleSit);
	// Social emote (X): cycles through a few text emotes shown over your head to nearby players.
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &ABHCharacter::CycleEmote);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ABHCharacter::ToggleQuestionCursor);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ABHCharacter::OnQuestionPointerDown);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ABHCharacter::OnQuestionPointerUp);
	// Enter is the existing "Ready" action; ToggleReady() submits a typed numeric answer
	// when a calculation question is focused, otherwise it toggles ready as before.
}

void ABHCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABHCharacter, bFlashlightOn);
	// FlashlightBattery stays replicated to everyone: ApplyFlashlightState dims the spotlight beam by
	// battery level, and that beam is rendered on remote clients. DetentionMarkRemaining likewise drives a
	// shared cue and changes infrequently, so keep it broadcast.
	DOREPLIFETIME(ABHCharacter, FlashlightBattery);
	// Stamina/Fear/Dread are read only by the owning client's HUD and by the server (AI/objective logic);
	// no remote client renders another pawn's meters. Replicating them to all 10 clients every net tick is
	// pure O(n^2) waste, so restrict them to the owner. The server keeps its authoritative copy regardless.
	DOREPLIFETIME_CONDITION(ABHCharacter, Stamina, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABHCharacter, Fear, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABHCharacter, Dread, COND_OwnerOnly);
	DOREPLIFETIME(ABHCharacter, DetentionMarkRemaining);
	DOREPLIFETIME(ABHCharacter, MovementSpecialState);
	DOREPLIFETIME(ABHCharacter, bTeacherCaptureAttackActive);
	DOREPLIFETIME(ABHCharacter, TeacherCaptureAttackStartServerTime);
	DOREPLIFETIME(ABHCharacter, TeacherCaptureAttackResolveServerTime);
	DOREPLIFETIME(ABHCharacter, TeacherCaptureAttackEndServerTime);
	DOREPLIFETIME(ABHCharacter, TeacherCaptureNextAllowedServerTime);
	DOREPLIFETIME(ABHCharacter, bHiddenInLocker);
	DOREPLIFETIME(ABHCharacter, bOutOfPlay);
	DOREPLIFETIME(ABHCharacter, bSeated);
	DOREPLIFETIME(ABHCharacter, EmoteId);
	DOREPLIFETIME(ABHCharacter, EmoteStartServerTime);
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
		BHPS->MarkWarmupStep(EBHWarmupStep::Hide);
	}

	SetActorLocation(Locker->GetActorLocation());
	ApplyFlashlightState();
	ApplyHiddenState();
}

static FVector ResolveLockerExitLocation(UWorld* World, const FVector& Origin, const FVector& Forward)
{
	// Try candidates scattered across the front semicircle at various radii.
	// Random start index gives a different spread each exit so the teacher can't
	// predict exactly where the player will appear.
	struct FLockerCandidate { float AngleDeg; float Radius; };
	static const FLockerCandidate Candidates[] = {
		{   0.0f, 180.0f },
		{  30.0f, 220.0f },
		{ -30.0f, 220.0f },
		{  60.0f, 180.0f },
		{ -60.0f, 180.0f },
		{   0.0f, 280.0f },
		{  45.0f, 260.0f },
		{ -45.0f, 260.0f },
		{  90.0f, 200.0f },
		{ -90.0f, 200.0f },
		{   0.0f, 120.0f },
	};

	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(42.0f, 98.0f);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const int32 StartIdx = FMath::RandRange(0, UE_ARRAY_COUNT(Candidates) - 1);

	for (int32 i = 0; i < UE_ARRAY_COUNT(Candidates); ++i)
	{
		const FLockerCandidate& C = Candidates[(StartIdx + i) % UE_ARRAY_COUNT(Candidates)];
		const float Rad = FMath::DegreesToRadians(C.AngleDeg);
		const FVector Dir = Forward * FMath::Cos(Rad) + Right * FMath::Sin(Rad);
		const FVector Candidate = Origin + Dir * C.Radius;
		if (World && !World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Capsule))
		{
			return Candidate;
		}
	}

	// Every fanned candidate was blocked (a very tight alcove). Returning an UNTESTED offset here is what caused the
	// roll-out soft-lock: a standing capsule teleported into a wall can't depenetrate and jams in place. Probe straight
	// out the door at shrinking distances and return the first that actually fits; the farthest is the least-bad last
	// resort (the caller teleports with TeleportPhysics, which resolves a shallow overlap rather than wedging).
	for (const float Dist : { 240.0f, 190.0f, 140.0f, 100.0f })
	{
		const FVector Probe = Origin + Forward * Dist;
		if (World && !World->OverlapBlockingTestByChannel(Probe, FQuat::Identity, ECC_Pawn, Capsule))
		{
			return Probe;
		}
	}

	return Origin + Forward * 240.0f;
}

void ABHCharacter::ExitLocker(bool bAllowMovementExit)
{
	if (!HasAuthority())
	{
		return;
	}

	FVector LockerExitDir = FVector::ZeroVector;
	if (CurrentLocker)
	{
		CurrentLocker->ClearOccupant(this);
		// Exit through the DOORS: panels/vents are on the locker's local -Y face, so the outward door normal is
		// -RightVector (Right = +Y). The old code passed ForwardVector (+X) -- the locker's SIDE -- and since lockers
		// stand flush against walls that ejected the player INTO the wall; a non-swept SetActorLocation then embedded
		// the standing capsule in geometry (CharacterMovement can't depenetrate -> the "no space" + can't-move soft-lock).
		LockerExitDir = (-CurrentLocker->GetActorRightVector()).GetSafeNormal2D();
		const FVector ExitLocation = ResolveLockerExitLocation(GetWorld(), CurrentLocker->GetActorLocation(), LockerExitDir);
		SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);
		CurrentLocker = nullptr;

		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		LockerExitTime = Now;
		if ((Fear >= 55.0f || Dread >= 55.0f) && Now - LastLockerNoiseTime > 6.0f)
		{
			LastLockerNoiseTime = Now;
			if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
			{
				BHGM->NotifyLoudNoise(ExitLocation, TEXT("locker door"));
				BHGM->ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Locker, ExitLocation, this, this, 0.9f, TEXT("locker door"));
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

	// Roll-out-of-locker (bh.LockerRollExit): on a VOLUNTARY exit with Sprint held, convert the exposed standing pop
	// into a capture-immune forward roll in the direction the player is facing. The roll's own space check rejects a
	// roll into a wall (falling back to the plain exit just performed), and its i-frames cover the catchable instant.
	if (bAllowMovementExit && CVarBHLockerRollExit.GetValueOnGameThread() != 0 && bSprintInputHeld
		&& !IsProne() && !IsSpecialMoveActive() && CanAct()
		&& GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround())
	{
		// Orient OUT the door before the roll. The player was facing INTO the locker (they looked at it to open it),
		// so without this the roll would dash back toward the locker/wall and get rejected for space. Turning the pawn
		// sets the roll direction (SpecialMoveDirection = actor forward, captured in TryStartSpecialMoveAuthority) so
		// "roll out" actually carries them away from the locker. Authority-only here, so it replicates to clients; the
		// pawn orients to movement (not controller yaw) so this turns the body without snapping the player's camera.
		if (!LockerExitDir.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.0f, LockerExitDir.Rotation().Yaw, 0.0f));
		}
		if (TryStartSpecialMoveAuthority(EBHMovementSpecialState::Rolling, false, false))
		{
			MarkTutorialAction(TutorialActLockerRollBit); // movement-tutorial: a roll-out-of-locker specifically fired
		}
	}
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
	if (FlashlightBattery > 5.0f)
	{
		bFlashlightEmptyTelemetryReported = false;
	}
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

void ABHCharacter::ResetRoleWarmupStateForRoundStart()
{
	if (!HasAuthority())
	{
		return;
	}

	ExitLocker();
	EndInteractAuthority(CurrentServerInteractTarget);
	CurrentInteractTarget = nullptr;
	CurrentServerInteractTarget = nullptr;
	LastScanTime = -999.0f;
	LastHunterPowerTime = -999.0f;
	LastDecoyTime = -999.0f;
	LastSprintNoiseTime = -999.0f;
	LastStaminaWarningTime = -999.0f;
	LastHidingPanicMessageTime = -999.0f;
	LastLockerNoiseTime = -999.0f;
	LockerExitTime = -999.0f;
	LastForcedBreathNoiseTime = -999.0f;
	LastPanicBreathNoiseTime = -999.0f;
	LastDetentionNoiseTime = -999.0f;
	LastCaptureEvasionTime = -999.0f;
	LastTeacherCounterplayHintTime = -999.0f;
	ResetAntiCampTrackingAuthority();
	bTeacherCaptureAttackActive = false;
	bTeacherCaptureAttackResolved = false;
	TeacherCaptureAttackStartServerTime = -999.0f;
	TeacherCaptureAttackResolveServerTime = -999.0f;
	TeacherCaptureAttackEndServerTime = -999.0f;
	TeacherCaptureNextAllowedServerTime = -999.0f;
	MovementSpecialState = EBHMovementSpecialState::None;
	CosmeticMovementSpecialState = EBHMovementSpecialState::None;
	bBHopJumpQueued = false;
	bSprintInputHeld = false;
	bProneInputHeld = false;
	bMovementFrozenByServer = false;
	bSpecialMoveEndsProne = false;
	bSpecialMoveEndProneRequiresInput = false;
	SpecialMoveStartTime = -999.0f;
	SpecialMoveEndTime = -999.0f;
	SpecialMoveCooldownEndTime = -999.0f;
	SpecialMoveDistanceTravelled = 0.0f;
	SpecialMoveDirection = FVector::ForwardVector;
	SpecialMoveNoiseEventMask = 0;
	FlashlightBattery = 100.0f;
	bFlashlightEmptyTelemetryReported = false;
	Stamina = MaxStamina;
	Fear = 0.0f;
	Dread = 0.0f;
	DetentionMarkRemaining = 0.0f;
	HiddenSeconds = 0.0f;
	bFlashlightOn = false;
	bHiddenInLocker = false;
	bOutOfPlay = false;
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
	}
	SetActorHiddenInGame(false);
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}
	ApplyMovementSpecialState();
	ApplyFlashlightState();
	ApplyHiddenState();
}

bool ABHCharacter::IsHiddenInLocker() const
{
	return bHiddenInLocker;
}

float ABHCharacter::GetFlashlightBattery() const
{
	return FlashlightBattery;
}

bool ABHCharacter::IsInTeacherBlackout() const
{
	// Student-only: the Teacher (and dead / non-survivor roles) are never weakened by their own dark.
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHPS || !BHPS->IsAliveSurvivor())
	{
		return false;
	}
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHGS && BHGS->IsPointInTeacherBlackout(GetActorLocation());
}

float ABHCharacter::GetEffectiveFlashlightDrainPerSecond() const
{
	float DrainRate = FlashlightDrainPerSecond;

	// A student caught in the Teacher's blackout burns battery faster while the dark smothers the beam.
	if (IsInTeacherBlackout())
	{
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		const float Multiplier = Settings ? FMath::Max(1.0f, Settings->BlackoutFlashlightDrainMultiplier) : 1.0f;
		DrainRate *= Multiplier;
	}

	return DrainRate;
}

float ABHCharacter::ComputeBlackoutFlashlightFlicker() const
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const float Floor = Settings ? FMath::Clamp(Settings->BlackoutFlashlightStrengthScale, 0.0f, 1.0f) : 0.15f;

	// Irregular high-frequency stutter with periodic near-cutouts, so the beam hovers around the configured
	// near-dead floor while violently fighting to stay alive. Driven by FlashlightPulseTime (advanced per
	// frame in UpdateFlashlightFeel) so no RNG is needed.
	const float T = FlashlightPulseTime;
	const float Stutter = 0.5f + 0.5f * FMath::Sin(T * 51.0f) * FMath::Sin(T * 23.0f + 1.3f);   // [0..1]
	const float Cutout = FMath::Square(FMath::Max(0.0f, FMath::Sin(T * 9.0f + 0.7f)));            // occasional [0..1]
	const float Flicker = FMath::Lerp(0.35f, 1.6f, Stutter) * FMath::Lerp(1.0f, 0.12f, Cutout);
	// Floor the flicker just under the configured near-dead level, not at a fixed 0.02 -- so a config that
	// sets BlackoutFlashlightStrengthScale=0 (the beam should fully die in the blackout) is honoured instead
	// of being clamped back up to 0.02. The default (0.15) still keeps the original ~0.02 minimum.
	return FMath::Clamp(Floor * Flicker, FMath::Min(Floor, 0.02f), 0.55f);
}

float ABHCharacter::GetFlashlightTuningValue(FName ParameterName) const
{
	const FString Parameter = ParameterName.ToString();
	if (Parameter == TEXT("Flashlight.IntensityScale")) { return FlashlightTuningIntensityScale; }
	if (Parameter == TEXT("Flashlight.RadiusScale")) { return FlashlightTuningRadiusScale; }
	if (Parameter == TEXT("Flashlight.VolumetricScale")) { return FlashlightTuningVolumetricScale; }
	if (Parameter == TEXT("Flashlight.BeamLengthScale")) { return FlashlightTuningBeamLengthScale; }
	if (Parameter == TEXT("Flashlight.BeamOpacityScale")) { return FlashlightTuningBeamOpacityScale; }
	if (Parameter == TEXT("Flashlight.BeamBrightnessScale")) { return FlashlightTuningBeamBrightnessScale; }
	if (Parameter == TEXT("Flashlight.ConeScale")) { return FlashlightTuningConeScale; }
	if (Parameter == TEXT("Flashlight.RayVisibilityScale")) { return FlashlightTuningRayVisibilityScale; }
	if (Parameter == TEXT("Flashlight.RayWidthScale")) { return FlashlightTuningRayWidthScale; }
	if (Parameter == TEXT("Flashlight.RayStartOffset")) { return FlashlightTuningRayStartOffset; }
	return 1.0f;
}

void ABHCharacter::SetFlashlightTuningValue(FName ParameterName, float Value)
{
	const FString Parameter = ParameterName.ToString();
	if (Parameter == TEXT("Flashlight.IntensityScale")) { FlashlightTuningIntensityScale = FMath::Clamp(Value, 0.0f, 3.0f); }
	else if (Parameter == TEXT("Flashlight.RadiusScale")) { FlashlightTuningRadiusScale = FMath::Clamp(Value, 0.0f, 3.0f); }
	else if (Parameter == TEXT("Flashlight.VolumetricScale")) { FlashlightTuningVolumetricScale = FMath::Clamp(Value, 0.0f, 3.0f); }
	else if (Parameter == TEXT("Flashlight.BeamLengthScale")) { FlashlightTuningBeamLengthScale = FMath::Clamp(Value, 0.0f, 3.0f); }
	else if (Parameter == TEXT("Flashlight.BeamOpacityScale")) { FlashlightTuningBeamOpacityScale = FMath::Clamp(Value, 0.0f, 3.0f); }
	else if (Parameter == TEXT("Flashlight.BeamBrightnessScale")) { FlashlightTuningBeamBrightnessScale = FMath::Clamp(Value, 0.0f, 3.0f); }
	else if (Parameter == TEXT("Flashlight.ConeScale")) { FlashlightTuningConeScale = FMath::Clamp(Value, 0.1f, 2.0f); }
	else if (Parameter == TEXT("Flashlight.RayVisibilityScale")) { FlashlightTuningRayVisibilityScale = FMath::Clamp(Value, 0.0f, 3.0f); }
	else if (Parameter == TEXT("Flashlight.RayWidthScale")) { FlashlightTuningRayWidthScale = FMath::Clamp(Value, 0.25f, 3.0f); }
	else if (Parameter == TEXT("Flashlight.RayStartOffset")) { FlashlightTuningRayStartOffset = FMath::Clamp(Value, 0.0f, 220.0f); }

	UpdateFlashlightFeel(0.0f);
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

EBHMovementSpecialState ABHCharacter::GetMovementSpecialState() const
{
	return MovementSpecialState;
}

bool ABHCharacter::IsProne() const
{
	return MovementSpecialState == EBHMovementSpecialState::Prone;
}

bool ABHCharacter::IsSpecialMoveActive() const
{
	return BHIsTransientSpecialMove(MovementSpecialState);
}

EBHMovementSpecialState ABHCharacter::GetCosmeticMovementSpecialState() const
{
	return CosmeticMovementSpecialState;
}

float ABHCharacter::GetRemainingSpecialMoveCooldown() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	return FMath::Max(0.0f, SpecialMoveCooldownEndTime - Now);
}

FString ABHCharacter::GetLastMovementFailureReason() const
{
	return LastMovementFailureReason;
}

float ABHCharacter::GetMovementSightProfileMultiplier() const
{
	return IsProne()
		? FMath::Clamp(BHResolveMovementRoleTuning(GetBHPlayerState()).ProneVisibilityMultiplier, 0.25f, 1.0f)
		: 1.0f;
}

bool ABHCharacter::IsTeacherCaptureAttackActive() const
{
	const float Now = GetTeacherCaptureClockSeconds();
	return bTeacherCaptureAttackActive && Now < TeacherCaptureAttackEndServerTime;
}

bool ABHCharacter::IsTeacherCaptureAttackInWindup() const
{
	const float Now = GetTeacherCaptureClockSeconds();
	return IsTeacherCaptureAttackActive() && Now < TeacherCaptureAttackResolveServerTime;
}

float ABHCharacter::GetTeacherCaptureAttackProgress() const
{
	if (!IsTeacherCaptureAttackActive() || TeacherCaptureAttackEndServerTime <= TeacherCaptureAttackStartServerTime)
	{
		return 0.0f;
	}

	const float Now = GetTeacherCaptureClockSeconds();
	return FMath::Clamp((Now - TeacherCaptureAttackStartServerTime) / FMath::Max(0.01f, TeacherCaptureAttackEndServerTime - TeacherCaptureAttackStartServerTime), 0.0f, 1.0f);
}

float ABHCharacter::GetTeacherCaptureCooldownRemaining() const
{
	const float Now = GetTeacherCaptureClockSeconds();
	return FMath::Max(0.0f, TeacherCaptureNextAllowedServerTime - Now);
}

#if WITH_DEV_AUTOMATION_TESTS
bool ABHCharacter::TryStartSpecialMoveForTest(EBHMovementSpecialState RequestedState, bool bEndProne)
{
	bSprintInputHeld = RequestedState == EBHMovementSpecialState::Rolling || RequestedState == EBHMovementSpecialState::Sliding;
	return TryStartSpecialMoveAuthority(RequestedState, bEndProne, false);
}

bool ABHCharacter::TrySetProneForTest(bool bNewProne)
{
	return SetProneAuthority(bNewProne, false);
}

float ABHCharacter::DebugGetAntiCampIdleSecondsForTest() const
{
	return GetWorld() && AntiCampLastSatisfiedTime > -900.0f
		? FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - AntiCampLastSatisfiedTime)
		: 0.0f;
}

bool ABHCharacter::DebugIsTeacherCaptureCandidateForTest(const ABHCharacter* Target) const
{
	float Score = 0.0f;
	return IsTeacherCaptureCandidateAuthority(Target, Score);
}
#endif

void ABHCharacter::MoveForward(float Value)
{
	// A seated player stands up the instant they try to move (see ToggleSit / ServerSetSeated).
	if (bSeated && Value != 0.0f)
	{
		ServerSetSeated(false);
		return;
	}
	if (Value != 0.0f && CanAct() && !IsSpecialMoveActive())
	{
		AddMovementInput(GetActorForwardVector(), Value);
		// Record the direction for the tutorial WASD lesson (negligible cost outside the tutorial).
		TutorialMovementMask |= (Value > 0.0f) ? TutorialMoveForwardBit : TutorialMoveBackBit;
	}
}

void ABHCharacter::MoveRight(float Value)
{
	if (bSeated && Value != 0.0f)
	{
		ServerSetSeated(false);
		return;
	}
	if (Value != 0.0f && CanAct() && !IsSpecialMoveActive())
	{
		AddMovementInput(GetActorRightVector(), Value);
		TutorialMovementMask |= (Value > 0.0f) ? TutorialMoveRightBit : TutorialMoveLeftBit;
	}
}

void ABHCharacter::Turn(float Value)
{
	// While answering a question with the mouse, the cursor owns the mouse: freeze look so moving it
	// only moves the cursor, never the view. Otherwise the camera swings off the station, the focus
	// trace loses it, and the question interaction drops out ("disconnects").
	if (bQuestionCursorActive)
	{
		return;
	}
	AddControllerYawInput(Value);
}

void ABHCharacter::LookUp(float Value)
{
	if (bQuestionCursorActive)
	{
		return;
	}
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
	// Keyboard look is also frozen while the question cursor is up (see Turn()).
	if (bQuestionCursorActive)
	{
		return;
	}
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
	if (!CanAct() || IsProne() || IsSpecialMoveActive())
	{
		StopJumping();
		bBHopJumpQueued = false;
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (!bBHopJumpQueued || Now - LastBHopJumpInputTime > BHBHopJumpBufferSeconds)
	{
		bBHopJumpQueued = false;
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement && Movement->IsMovingOnGround())
	{
		bBHopJumpQueued = false;
		Jump();
	}
}

void ABHCharacter::ToggleReady()
{
	// Enter doubles as "submit typed answer" when a calculation question is focused with a
	// pending numeric entry. Outside that context it toggles lobby readiness as before, so
	// there is no conflict (a focused calc question only exists mid-Hunt, never in the lobby).
	if (IsCalculationEntryActive() && !NumericAnswerEntry.IsEmpty())
	{
		ConfirmNumericAnswer();
		return;
	}

	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(GetController()))
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		BHPC->ServerSetReady(!(BHPS && BHPS->bReady));
	}
}

void ABHCharacter::StartInteract()
{
	if (IsSpecialMoveActive())
	{
		SendStatusMessage(TEXT("Finish the move before interacting."));
		return;
	}

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
		bFlashlightEmptyTelemetryReported = false;
	}

	if (!bFlashlightOn && FlashlightBattery <= 1.0f)
	{
		SendStatusMessage(TEXT("Flashlight battery is dead."));
		return;
	}

	if (!bFlashlightOn && FlashlightBattery <= 20.0f)
	{
		// State that it DID turn on, so this low-battery note doesn't read like the "battery is dead" refusal above
		// it (a player at 8% could otherwise think the light failed to come on).
		SendStatusMessage(TEXT("Flashlight on - battery almost gone."));
	}

	ServerSetFlashlight(!bFlashlightOn);
}

void ABHCharacter::ToggleSit()
{
	// Local input -> server. The server re-validates (CanAct) and owns the authoritative seated state.
	ServerSetSeated(!bSeated);
}

void ABHCharacter::StartJump()
{
	if (IsSpecialMoveActive())
	{
		return;
	}

	// Space stands you up from prone. (Dive is bound to Space-then-Alt: jump here, then press Alt -- see StartProne.)
	if (IsProne())
	{
		ServerSetProne(false);
		return;
	}

	bBHopJumpQueued = true;
	LastBHopJumpInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	TryBHopJump();
}

void ABHCharacter::StopJump()
{
	StopJumping();
}

void ABHCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	// Jumping costs stamina. Without this, bunny-hopping is a free speed hack: tap sprint to reach sprint speed,
	// jump, and coast through the air at that speed where the per-second sprint drain (which only bites while
	// holding sprint on the ground) never applies. Charging each jump makes a skilled hop a little MORE
	// stamina-efficient than just holding sprint (a small reward), but still a real cost -- chained hops drain
	// the bar, and once it empties sprint speed is removed so the airborne coast decays. Server-authoritative,
	// matching the sprint drain; scaled by the per-map stamina economy so the "slightly better than sprint"
	// margin holds on the running maps too.
	if (HasAuthority() && IsPlayerControlled() && !IsProne())
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const float Cost = BHJumpStaminaCost / FMath::Max(0.01f, BHLevelStaminaScale(BHPS));
		Stamina = FMath::Max(0.0f, Stamina - Cost);

		// Movement-tutorial telemetry: count a real bunny-hop chain -- consecutive jumps taken AT SPRINT SPEED within
		// ~1.1s of each other (so a stationary player tapping Space three times does NOT count; they must be carrying
		// speed through the hops). A slow jump, or a gap, resets the chain. Three linked sprint-speed hops latch it.
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const float RoughSprintSpeed = FMath::Max(1.0f, BHRoleSprintSpeed(BHPS, SprintSpeed));
		const bool bAtSprintSpeed = GetVelocity().Size2D() >= 0.8f * RoughSprintSpeed;
		if (bAtSprintSpeed)
		{
			TutorialBhopChain = (Now - LastTutorialJumpServerTime <= 1.1f) ? TutorialBhopChain + 1 : 1;
			if (TutorialBhopChain >= 3)
			{
				MarkTutorialAction(TutorialActBhopBit);
			}
		}
		else
		{
			TutorialBhopChain = 0;
		}
		LastTutorialJumpServerTime = Now;
	}
}

void ABHCharacter::StartSprint()
{
	bSprintInputHeld = true;

	if (!CanAct())
	{
		return;
	}

	if (IsSpecialMoveActive())
	{
		return;
	}

	if (IsProne())
	{
		SendStatusMessage(TEXT("Stand up before sprinting."));
		return;
	}

	if (Stamina > MaxStamina * 0.08f)
	{
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const float HunterPenaltyMultiplier = BHFinalEscapeHunterPenaltyActive(GetWorld(), this) ? 0.72f : 1.0f;
		GetCharacterMovement()->MaxWalkSpeed = BHRoleSprintSpeed(BHPS, SprintSpeed)
			* (PowerupComponent ? PowerupComponent->GetSprintSpeedMultiplier() : 1.0f)
			* HunterPenaltyMultiplier
			* BHHorrorSprintSpeedMultiplier(this, BHPS);
		ServerSetSprinting(true);
	}
	else
	{
		SendStatusMessage(TEXT("Too exhausted to sprint."));
	}
}

void ABHCharacter::StopSprint()
{
	bSprintInputHeld = false;

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const float HunterPenaltyMultiplier = BHFinalEscapeHunterPenaltyActive(GetWorld(), this) ? 0.72f : 1.0f;
	GetCharacterMovement()->MaxWalkSpeed = IsProne()
		? BHRoleProneSpeed(BHPS) * BHHorrorWalkSpeedMultiplier(this, BHPS)
		: BHRoleWalkSpeed(BHPS, WalkSpeed) * HunterPenaltyMultiplier * BHHorrorWalkSpeedMultiplier(this, BHPS);
	ServerSetSprinting(false);
}

void ABHCharacter::StartCrouch()
{
	bCrouchInputHeld = true; // tracked so a drop-roll can fire on landing while Sprint+Crouch are held through a fall
	ServerSetCrouchInputHeld(true); // ...and the SERVER needs it too: Landed()/the drop-roll runs on authority only

	if (IsSpecialMoveActive())
	{
		return;
	}

	if (IsProne())
	{
		ServerSetProne(false);
		return;
	}

	if (bSprintInputHeld)
	{
		StartCosmeticSpecialMove(EBHMovementSpecialState::Rolling);
		ServerStartSpecialMove(EBHMovementSpecialState::Rolling, false, false);
		return;
	}

	if (CanAct())
	{
		Crouch();
	}
}

void ABHCharacter::StopCrouch()
{
	bCrouchInputHeld = false;
	ServerSetCrouchInputHeld(false);
	if (!IsProne())
	{
		UnCrouch();
	}
}

void ABHCharacter::StartProne()
{
	bProneInputHeld = true;
	ServerSetProneInputHeld(true);

	if (!CanAct() || IsSpecialMoveActive())
	{
		return;
	}

	if (bSprintInputHeld)
	{
		StartCosmeticSpecialMove(EBHMovementSpecialState::Sliding);
		ServerStartSpecialMove(EBHMovementSpecialState::Sliding, true, true);
		return;
	}

	// DIVE -- bound to Space-then-Alt: if the player just jumped (or is otherwise airborne) and is moving, Alt
	// launches a forward dive instead of toggling prone. Press Space to leap, then Alt to dive. A grounded or
	// stationary Alt just toggles prone as before.
	if (!IsProne())
	{
		const UCharacterMovementComponent* Move = GetCharacterMovement();
		const bool bAirborne = Move && Move->IsFalling();
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const bool bRecentJump = (Now - LastBHopJumpInputTime) >= 0.0f && (Now - LastBHopJumpInputTime) <= 0.6f;
		const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(30.0f) || GetLastMovementInputVector().SizeSquared2D() > 0.01f;
		if ((bAirborne || bRecentJump) && bMoving)
		{
			StartCosmeticSpecialMove(EBHMovementSpecialState::Diving);
			ServerStartSpecialMove(EBHMovementSpecialState::Diving, true, false);
			return;
		}
	}

	ServerSetProne(!IsProne());
}

void ABHCharacter::StopProne()
{
	bProneInputHeld = false;
	ServerSetProneInputHeld(false);
	if (MovementSpecialState == EBHMovementSpecialState::Sliding && bSpecialMoveEndProneRequiresInput)
	{
		bSpecialMoveEndsProne = false;
	}
}

void ABHCharacter::SetMovementFailureReason(const FString& Reason)
{
	LastMovementFailureReason = Reason;
	LastMovementFailureTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	MovementFailurePulse = 1.0f;
	if (!Reason.IsEmpty())
	{
		SendStatusMessage(Reason);
	}
}

void ABHCharacter::ClientSpecialMoveRejected_Implementation(EBHMovementSpecialState RejectedState, const FString& Reason)
{
	if (CosmeticMovementSpecialState == RejectedState)
	{
		ClearCosmeticSpecialMove();
	}
	SetMovementFailureReason(Reason);
}

bool ABHCharacter::IsSpecialMoveWallBonk() const
{
	return bSpecialMoveHitWall;
}

void ABHCharacter::ClientNotifyPerfectChain_Implementation(int32 ChainCount)
{
	// Cosmetic only: unlock the perfect_chain achievement (-> the Afterimage tint) on the owning client.
	if (UWorld* World = GetWorld())
	{
		if (UBHAccountSubsystem* Account = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
		{
			Account->UnlockAchievement(FName(TEXT("perfect_chain")));
			// A full three-link chain (the cap, BHMomentumChainMaxLinks) earns Flow Master.
			if (ChainCount >= 3)
			{
				Account->UnlockAchievement(FName(TEXT("flow_master")));
			}
		}
	}
	// Brief feel cue for nailing it (the achievement toast handles the first-time fanfare).
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(GetController()))
	{
		BHPC->ShowLocalStatusMessage(ChainCount >= 2 ? FString::Printf(TEXT("Perfect chain x%d"), ChainCount) : FString(TEXT("Perfect chain!")), 1.5f);
	}
}

void ABHCharacter::ClientRecordTrainActivity_Implementation(uint8 ActivityIndex)
{
	// Cosmetic only: record that this client tried a train-intermission activity (-> the Tourist achievement).
	if (UWorld* World = GetWorld())
	{
		if (UBHAccountSubsystem* Account = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
		{
			Account->RecordTrainActivityUse(static_cast<int32>(ActivityIndex));
		}
	}
}

void ABHCharacter::ClientRecordQuestionResult_Implementation(uint8 TopicIndex, bool bCorrect)
{
	// Cosmetic only: record a graded answer on the owning client (-> per-topic mastery + Honor Roll / Polymath).
	if (UWorld* World = GetWorld())
	{
		if (UBHAccountSubsystem* Account = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
		{
			Account->RecordQuestionResult(static_cast<int32>(TopicIndex), bCorrect);
		}
	}
}

void ABHCharacter::ClientGrantAchievement_Implementation(FName AchievementId, const FString& ToastMessage)
{
	// Cosmetic only: unlock the given achievement on the owning client (account progress is client-local, so the
	// server can't write it directly -- it asks the owning client to, here).
	if (UWorld* World = GetWorld())
	{
		if (UBHAccountSubsystem* Account = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
		{
			Account->UnlockAchievement(AchievementId);
		}
	}
	if (!ToastMessage.IsEmpty())
	{
		if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(GetController()))
		{
			BHPC->ShowLocalStatusMessage(ToastMessage, 3.0f);
		}
	}
}

void ABHCharacter::ClientToggleRoofServiceLights_Implementation()
{
	SetRoofServiceLightsLocal(!bRoofServiceLightsOn);
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(GetController()))
	{
		BHPC->ShowLocalStatusMessage(
			bRoofServiceLightsOn ? TEXT("Roof service lights: ON") : TEXT("Roof service lights: OFF"), 2.5f);
	}
}

void ABHCharacter::SetRoofServiceLightsLocal(bool bOn)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bRoofServiceLightsOn = bOn;

	// Always clear any existing local lights first so a toggle never leaks duplicates.
	for (ABHTrainServiceLight* RoofLight : LocalRoofLights)
	{
		if (IsValid(RoofLight))
		{
			RoofLight->Destroy();
		}
	}
	LocalRoofLights.Reset();

	if (!bOn)
	{
		return;
	}

	// Client-local roof service lights along the subway-intermission roof centreline (matches the ABHGameMode
	// subway build: roof slab top ~z316, walkway x[-3750,3750]). Never replicated -- this is per-player lighting,
	// so one passenger throwing the breaker lights the roof only in their own view.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Span wide enough to cover both the intermission roof and the longer 9-car lobby roof. Modules past a
	// shorter roof's end simply illuminate nothing (no visible surface), so over-covering is harmless.
	for (int32 Index = 0; Index < 15; ++Index)
	{
		const FVector RoofLightLocation(-6300.0f + Index * 900.0f, 0.0f, 486.0f);
		if (ABHTrainServiceLight* RoofLight = World->SpawnActor<ABHTrainServiceLight>(RoofLightLocation, FRotator::ZeroRotator, SpawnParams))
		{
			RoofLight->SetRoofModeImmediate();
			LocalRoofLights.Add(RoofLight);
		}
	}
}

void ABHCharacter::UpdateProximityPlayerHideFromLocalView()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The local viewer on this machine drives the comfort option and supplies the camera position.
	const ABHPlayerController* LocalPC = Cast<ABHPlayerController>(World->GetFirstPlayerController());
	const ABHCharacter* LocalPawn = LocalPC ? Cast<ABHCharacter>(LocalPC->GetPawn()) : nullptr;
	const bool bEnabled = LocalPC && LocalPawn && LocalPawn != this && LocalPC->IsHideVeryClosePlayersEnabled();

	bool bWantHidden = false;
	if (bEnabled)
	{
		// Never hide the active threat -- a survivor must always be able to see the Hunter, even point-blank.
		const ABHPlayerState* MyPS = GetPlayerState<ABHPlayerState>();
		const bool bThreat = MyPS && (MyPS->PlayerRole == EBHPlayerRole::Hunter || MyPS->PlayerRole == EBHPlayerRole::FakeHunter);
		if (!bThreat)
		{
			const FVector ViewLocation = LocalPawn->Camera ? LocalPawn->Camera->GetComponentLocation() : LocalPawn->GetActorLocation();
			const float DistSq = FVector::DistSquared(GetActorLocation(), ViewLocation);
			// Hysteresis: hide once within ~1.5m, only reappear past ~1.9m, so a body hovering at the edge of the
			// radius doesn't strobe in and out.
			const float ThresholdSq = bProximityHiddenLocal ? FMath::Square(190.0f) : FMath::Square(150.0f);
			bWantHidden = DistSq <= ThresholdSq;
		}
	}

	SetProximityHiddenLocal(bWantHidden);
}

void ABHCharacter::SetProximityHiddenLocal(bool bShouldHide)
{
	if (bShouldHide == bProximityHiddenLocal)
	{
		return;
	}
	bProximityHiddenLocal = bShouldHide;

	if (bShouldHide)
	{
		// Hide only the mesh components (body parts, role mesh, cosmetics) -- never the lights/audio -- and
		// remember exactly which ones we hid so we can restore that set without forcing already-hidden parts
		// (e.g. an unused role mesh slot) back on. Local visibility only; nothing here replicates.
		ProximityHiddenMeshes.Reset();
		TArray<UMeshComponent*> MeshComponents;
		GetComponents<UMeshComponent>(MeshComponents);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (MeshComponent && MeshComponent->IsVisible())
			{
				MeshComponent->SetVisibility(false);
				ProximityHiddenMeshes.Add(MeshComponent);
			}
		}
	}
	else
	{
		for (const TWeakObjectPtr<UMeshComponent>& WeakMesh : ProximityHiddenMeshes)
		{
			if (UMeshComponent* MeshComponent = WeakMesh.Get())
			{
				MeshComponent->SetVisibility(true);
			}
		}
		ProximityHiddenMeshes.Reset();
	}
}

void ABHCharacter::StartCosmeticSpecialMove(EBHMovementSpecialState State)
{
	if (HasAuthority() || !BHIsTransientSpecialMove(State))
	{
		return;
	}

	CosmeticMovementSpecialState = State;
	LastRoleAnimationName = NAME_None;

	// Client-local clock so the cosmetic-prediction path can drive POV camera progress
	// (SpecialMoveStartTime is authority-only and not replicated).
	if (State != LocalSpecialAnimState)
	{
		LocalSpecialAnimStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		LocalSpecialAnimState = State;
	}
}

void ABHCharacter::ClearCosmeticSpecialMove()
{
	if (CosmeticMovementSpecialState != EBHMovementSpecialState::None)
	{
		CosmeticMovementSpecialState = EBHMovementSpecialState::None;
		LastRoleAnimationName = NAME_None;
	}
}

bool ABHCharacter::ValidateSpecialMoveSpaceAuthority(EBHMovementSpecialState RequestedState, const FBHMovementSpecialTuning& Tuning, FString& OutFailureReason) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!GetWorld() || !Capsule || !Movement)
	{
		return true;
	}
	if (!GetWorld()->GetPhysicsScene())
	{
		return true;
	}

	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		OutFailureReason = TEXT("No movement direction.");
		return false;
	}

	const float HalfHeight = (RequestedState == EBHMovementSpecialState::Sliding || RequestedState == EBHMovementSpecialState::Diving)
		? BHProneCapsuleHalfHeight
		: DefaultCapsuleHalfHeight;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(DefaultCapsuleRadius * 0.92f, HalfHeight);
	const FVector Start = GetActorLocation();
	const float Clearance = FMath::Max(64.0f, Tuning.Curve.MinForwardClearance);
	const FVector End = Start + Forward * Clearance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHSpecialMoveForwardTrace), false, this);
	FHitResult ForwardHit;
	if (GetWorld()->SweepSingleByChannel(ForwardHit, Start, End, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Params) && ForwardHit.bBlockingHit)
	{
		OutFailureReason = Tuning.BlockedText.IsEmpty() ? TEXT("Not enough room for that move.") : Tuning.BlockedText.ToString();
		return false;
	}

	const float ProbeDistance = FMath::Max(64.0f, Tuning.Curve.FloorProbeDistance);
	const FVector FloorStart = End + FVector(0.0f, 0.0f, HalfHeight + 16.0f);
	const FVector FloorEnd = End - FVector(0.0f, 0.0f, ProbeDistance + HalfHeight);
	FHitResult FloorHit;
	if (!GetWorld()->LineTraceSingleByChannel(FloorHit, FloorStart, FloorEnd, Capsule->GetCollisionObjectType(), Params) || !FloorHit.bBlockingHit)
	{
		OutFailureReason = TEXT("No floor ahead.");
		return false;
	}

	const float MinWalkableZ = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Tuning.Curve.MaxSlopeDegrees, 0.0f, 89.0f)));
	if (FloorHit.ImpactNormal.Z < MinWalkableZ)
	{
		OutFailureReason = TEXT("Slope is too steep.");
		return false;
	}

	const float CurrentFloorZ = Start.Z - Capsule->GetScaledCapsuleHalfHeight();
	const float Drop = FMath::Max(0.0f, CurrentFloorZ - FloorHit.ImpactPoint.Z);
	if (Drop > FMath::Max(0.0f, Tuning.Curve.MaxDropHeight))
	{
		OutFailureReason = TEXT("Drop ahead is too high.");
		return false;
	}

	return true;
}

void ABHCharacter::EmitSpecialMoveNoiseAuthority(EBHMovementSpecialState State, FName NoiseEvent, float EventMultiplier)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const FBHMovementSpecialTuning Tuning = BHGetSpecialMoveTuning(BHPS, State);
	const float NoiseStrength = Tuning.NoiseStrength * FMath::Max(0.0f, EventMultiplier) * BHRoleSpecialNoiseMultiplier(BHPS) * BHHorrorNoiseMultiplier(this, BHPS);
	if (NoiseStrength <= 0.0f)
	{
		return;
	}

	const FString NoiseReason = NoiseEvent.IsNone() ? TEXT("special movement") : NoiseEvent.ToString();
	EmitFootstepStimulus(NoiseStrength, NoiseReason, ResolveFootstepSurface());
	if (NoiseStrength >= 1.0f)
	{
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->NotifyLoudNoise(GetActorLocation(), NoiseReason);
		}
	}
}

void ABHCharacter::UpdateSpecialMoveAuthority(float DeltaSeconds)
{
	if (!HasAuthority() || !IsSpecialMoveActive() || !GetWorld())
	{
		return;
	}

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const FBHMovementSpecialTuning Tuning = BHGetSpecialMoveTuning(BHPS, MovementSpecialState);
	const float Duration = FMath::Max(0.01f, Tuning.DurationSeconds);
	const float NormalizedTime = FMath::Clamp((GetWorld()->GetTimeSeconds() - SpecialMoveStartTime) / Duration, 0.0f, 1.0f);
	// Apply the flow-chain momentum bonus to the actual DISTANCE travelled (not just MaxWalkSpeed, which is
	// irrelevant while input is suppressed during the move) so a frame-perfect chain visibly travels farther/faster.
	// The swept AddActorWorldOffset below still stops at any wall, so a longer chained move can't punch through one.
	const float TargetDistance = FMath::Max(0.0f, Tuning.Curve.Distance) * SpecialMoveMomentumScale * BHMovementCurveAlpha(Tuning, NormalizedTime);
	const float DeltaDistance = FMath::Max(0.0f, TargetDistance - SpecialMoveDistanceTravelled);
	if (DeltaDistance > KINDA_SMALL_NUMBER && !SpecialMoveDirection.IsNearlyZero())
	{
		const FVector StepDir = SpecialMoveDirection.GetSafeNormal2D();
		FHitResult MoveHit;
		AddActorWorldOffset(StepDir * DeltaDistance, true, &MoveHit, ETeleportType::None);
		SpecialMoveDistanceTravelled += MoveHit.bBlockingHit ? DeltaDistance * MoveHit.Time : DeltaDistance;
		if (MoveHit.bBlockingHit && !TryVaultOverObstacleAuthority(Tuning, StepDir, MoveHit))
		{
			// Quiet-roll discipline: a bonked roll is "dirty" -> its impact stimulus fires LOUD (full 1.0); a clean,
			// uninterrupted roll fires quiet. Tracked here because the impact event below fires before full distance.
			bSpecialMoveHitWall = true;
			SpecialMoveEndTime = GetWorld()->GetTimeSeconds();
		}
	}

	if (MovementSpecialState == EBHMovementSpecialState::Rolling && NormalizedTime >= 0.34f && !(SpecialMoveNoiseEventMask & 0x01))
	{
		SpecialMoveNoiseEventMask |= 0x01;
		const float CleanScale = FMath::Clamp(CVarBHQuietRollCleanScale.GetValueOnGameThread(), 0.0f, 1.0f);
		const float RollImpactMult = (bSpecialMoveHitWall ? 1.0f : CleanScale) * SpecialMoveNoiseScale;
		EmitSpecialMoveNoiseAuthority(MovementSpecialState, FName(TEXT("roll impact")), RollImpactMult);
		if (!bSpecialMoveHitWall)
		{
			MarkTutorialAction(TutorialActQuietRollBit); // movement-tutorial: a clean (quiet) roll, no wall bonk
		}
	}
	else if (MovementSpecialState == EBHMovementSpecialState::Sliding && NormalizedTime >= 0.42f && !(SpecialMoveNoiseEventMask & 0x02))
	{
		SpecialMoveNoiseEventMask |= 0x02;
		EmitSpecialMoveNoiseAuthority(MovementSpecialState, FName(TEXT("slide scrape")), 0.65f * SpecialMoveNoiseScale);
	}
	else if (MovementSpecialState == EBHMovementSpecialState::Diving && NormalizedTime >= 0.82f && !(SpecialMoveNoiseEventMask & 0x04))
	{
		SpecialMoveNoiseEventMask |= 0x04;
		const float DiveLandMult = bSpecialMoveFromProne ? FMath::Clamp(CVarBHCrawlDashNoiseScale.GetValueOnGameThread(), 0.0f, 1.0f) : 1.0f;
		EmitSpecialMoveNoiseAuthority(MovementSpecialState, FName(TEXT("dive landing")), DiveLandMult * SpecialMoveNoiseScale);
	}
}

// Vault / mantle (ships dark behind bh.VaultTech): when a roll/slide's forward sweep is blocked by a LOW, vaultable
// ledge with clear headroom and floor on the far side, hop up-and-over it instead of dead-stopping -- the one place
// the otherwise-unused FBHMovementCurveTuning.VerticalImpulse is consumed. Returns true if a vault was performed.
bool ABHCharacter::TryVaultOverObstacleAuthority(const FBHMovementSpecialTuning& Tuning, const FVector& StepDir, const FHitResult& WallHit)
{
	if (CVarBHVaultTech.GetValueOnGameThread() == 0 || Tuning.Curve.VerticalImpulse <= 0.0f || (SpecialMoveNoiseEventMask & 0x08))
	{
		return false;
	}
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule || !GetWorld() || !GetWorld()->GetPhysicsScene() || StepDir.IsNearlyZero())
	{
		return false;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float FloorZ = GetActorLocation().Z - HalfHeight;
	const float LedgeHeight = WallHit.ImpactPoint.Z - FloorZ;
	const float MaxLedge = FMath::Max(0.0f, CVarBHVaultMaxLedgeHeight.GetValueOnGameThread());
	if (LedgeHeight <= 8.0f || LedgeHeight > MaxLedge)
	{
		return false; // not a low ledge (a real wall, or just the floor) -> dead-stop as before
	}

	// Target a standing spot just past the obstacle, raised by the vault impulse (clamped so we never launch into
	// the ceiling). Require clear headroom there before committing.
	const float Lift = FMath::Min(Tuning.Curve.VerticalImpulse, LedgeHeight + 28.0f);
	const FVector OverPos = GetActorLocation() + StepDir * (DefaultCapsuleRadius * 2.4f) + FVector(0.0f, 0.0f, Lift);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(DefaultCapsuleRadius * 0.9f, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHVaultProbe), false, this);
	if (GetWorld()->OverlapBlockingTestByChannel(OverPos, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Params))
	{
		return false; // far side is blocked (it's a tall wall, not vaultable cover)
	}

	FHitResult VaultHit;
	AddActorWorldOffset(OverPos - GetActorLocation(), true, &VaultHit, ETeleportType::None);
	SpecialMoveNoiseEventMask |= 0x08; // vault once per move
	EmitSpecialMoveNoiseAuthority(MovementSpecialState, FName(TEXT("vault")), 0.9f * SpecialMoveNoiseScale);
	return true;
}

// Momentum "flow chain" tech. A frame-perfect transient special-move input within this window right as the
// previous move ended bypasses the cooldown once and preserves momentum -- a hard, satisfying speedrun tech.
// Survivor-side only; capped so it can't loop; modest speed scale. Gated by bh.MomentumTech (default on).
// NOTE: this is the one "fun feature" that touches movement, so it WANTS balance playtesting; the cvar plus the
// tight window + cap keep its impact small in the meantime.
static TAutoConsoleVariable<int32> CVarBHMomentumTech(
	TEXT("bh.MomentumTech"),
	1,
	TEXT("1 (default) = survivor frame-perfect momentum-chain tech on; 0 = off (plain special moves)."),
	ECVF_Default);
// Flow-chain tuning knobs -- exposed as cvars so the one balance-sensitive feature can be playtest-tuned live
// (the defaults match the original constants). See Docs/EASTER_EGGS.md.
static TAutoConsoleVariable<float> CVarBHMomentumChainWindow(
	TEXT("bh.MomentumChainWindow"),
	0.12f,
	TEXT("Survivor flow-chain input window in seconds: chain the next special move within this of the previous one ending. Lower = harder. (Default 0.12)"),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarBHMomentumChainMaxLinks(
	TEXT("bh.MomentumChainMaxLinks"),
	3,
	TEXT("Maximum survivor flow-chain links before a real cooldown resets it. (Default 3)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHMomentumChainSpeedScale(
	TEXT("bh.MomentumChainSpeedScale"),
	1.15f,
	TEXT("Speed multiplier applied to a chained special move. (Default 1.15)"),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarBHHatsFollowHead(
	TEXT("bh.HatsFollowHead"),
	1,
	TEXT("1 (default) = procedural headwear attaches to the skeletal head bone so it follows the head through crouch/prone/animation; 0 = anchor to the role mesh's bounds top (the previous static behavior)."),
	ECVF_Default);

bool ABHCharacter::TryStartSpecialMoveAuthority(EBHMovementSpecialState RequestedState, bool bEndProne, bool bEndProneRequiresInput)
{
	if (!HasAuthority() || !CanAct() || !GetWorld())
	{
		ClientSpecialMoveRejected(RequestedState, TEXT("You cannot move right now."));
		return false;
	}

	const bool bRequestedTransient = BHIsTransientSpecialMove(RequestedState);
	const bool bDiveFromProne = RequestedState == EBHMovementSpecialState::Diving && IsProne();
	if (!bRequestedTransient || IsSpecialMoveActive() || (IsProne() && !bDiveFromProne))
	{
		ClientSpecialMoveRejected(RequestedState, TEXT("Finish the current move first."));
		return false;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	// Diving may launch from the AIR (it is bound to Space-then-Alt: jump, then dive). Roll/Slide still require
	// solid ground -- a roll requested in the air is instead buffered for a drop-roll on landing.
	if (!Movement || (!Movement->IsMovingOnGround() && RequestedState != EBHMovementSpecialState::Diving))
	{
		// Drop-roll: a roll requested in the air (Sprint held + Crouch) just before landing is BUFFERED rather than
		// simply rejected, so Landed() can fire it on touchdown and suppress the hard-landing noise. Only the roll,
		// only when the not-grounded gate is the blocker -- a stamina/space failure below still rejects normally.
		if (Movement && RequestedState == EBHMovementSpecialState::Rolling && bSprintInputHeld
			&& CVarBHDropRollWindow.GetValueOnGameThread() > 0.0f)
		{
			LastDropRollInputTime = GetWorld()->GetTimeSeconds();
			// Armed for a drop-roll on landing; clear the predicted cosmetic but show NO failure nag.
			ClientSpecialMoveRejected(RequestedState, FString());
			return false;
		}
		ClientSpecialMoveRejected(RequestedState, TEXT("Get your footing first."));
		return false;
	}

	if ((RequestedState == EBHMovementSpecialState::Rolling || RequestedState == EBHMovementSpecialState::Sliding) && !bSprintInputHeld)
	{
		ClientSpecialMoveRejected(RequestedState, TEXT("Sprint first."));
		return false;
	}

	const bool bHasMoveIntent = GetVelocity().SizeSquared2D() > FMath::Square(25.0f) || GetLastMovementInputVector().SizeSquared2D() > 0.01f || bSprintInputHeld;
	if (RequestedState == EBHMovementSpecialState::Diving && !bHasMoveIntent)
	{
		ClientSpecialMoveRejected(RequestedState, TEXT("Move before diving."));
		return false;
	}

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const FBHMovementSpecialTuning BaseTuning = BHGetSpecialMoveTuning(BHPS, RequestedState);
	const float StaminaCost = BaseTuning.StaminaCost
		* BHRoleSpecialStaminaMultiplier(BHPS)
		* (PowerupComponent ? PowerupComponent->GetStaminaDrainMultiplier() : 1.0f);
	if (Stamina < StaminaCost)
	{
		const FString Reason = BaseTuning.LowStaminaText.IsEmpty() ? TEXT("Too exhausted for that move.") : BaseTuning.LowStaminaText.ToString();
		SetMovementFailureReason(Reason);
		ClientSpecialMoveRejected(RequestedState, Reason);
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Momentum "flow chain" tech: a frame-perfect transient move right as the previous one ended bypasses the
	// cooldown once and keeps the player's momentum. Survivor-side only (never helps a Hunter capture), a tight
	// (hard) window, and capped. SpecialMoveMomentumScale feeds the special-move speed below.
	SpecialMoveMomentumScale = 1.0f;
	bool bPerfectChain = false;
	if (CVarBHMomentumTech.GetValueOnGameThread() != 0 && BHPS && BHPS->IsAliveSurvivor()
		&& Now < SpecialMoveCooldownEndTime && LastSpecialMoveEndedTime > -900.0f
		&& PerfectChainCount < CVarBHMomentumChainMaxLinks.GetValueOnGameThread())
	{
		const float SinceEnded = Now - LastSpecialMoveEndedTime;
		if (SinceEnded >= 0.0f && SinceEnded <= CVarBHMomentumChainWindow.GetValueOnGameThread())
		{
			bPerfectChain = true;
			SpecialMoveMomentumScale = CVarBHMomentumChainSpeedScale.GetValueOnGameThread();
		}
	}

	if (Now < SpecialMoveCooldownEndTime && !bPerfectChain)
	{
		const FString CooldownLabel = BaseTuning.CooldownText.IsEmpty() ? TEXT("Movement cooling down:") : BaseTuning.CooldownText.ToString();
		const FString Reason = FString::Printf(TEXT("%s %ds."), *CooldownLabel, FMath::CeilToInt(SpecialMoveCooldownEndTime - Now));
		SetMovementFailureReason(Reason);
		ClientSpecialMoveRejected(RequestedState, Reason);
		return false;
	}

	FString SpaceFailureReason;
	if (!ValidateSpecialMoveSpaceAuthority(RequestedState, BaseTuning, SpaceFailureReason))
	{
		SetMovementFailureReason(SpaceFailureReason);
		ClientSpecialMoveRejected(RequestedState, SpaceFailureReason);
		return false;
	}

	Stamina = FMath::Max(0.0f, Stamina - StaminaCost);
	StaminaRecoveryLockedUntil = Now + FMath::Max(0.20f, BaseTuning.DurationSeconds);
	SpecialMoveStartTime = Now;
	SpecialMoveEndTime = Now + BaseTuning.DurationSeconds;
	SpecialMoveCooldownEndTime = SpecialMoveEndTime + BaseTuning.CooldownSeconds * BHRoleSpecialCooldownMultiplier(BHPS);
	SpecialMoveDistanceTravelled = 0.0f;
	SpecialMoveDirection = GetActorForwardVector().GetSafeNormal2D();
	LastCaptureEvasionTime = Now;
	SpecialMoveNoiseEventMask = 0;
	bSpecialMoveHitWall = false;
	bSpecialMoveEarlyBrake = false;
	bSpecialMoveFromProne = bDiveFromProne; // crawl-gap dash: a from-prone dive is the quiet variant
	// Flow-chain noise amnesty: a chained link is quieter (a clean operator moves quietly), captured once here so the
	// later Tick noise events use this move's depth even after PerfectChainCount changes. The chain CAP fires a loud
	// "overextension" tell below, so pushing the chain to its limit broadcasts your position -- greed has a cost.
	{
		const int32 ThisMoveChainDepth = bPerfectChain ? PerfectChainCount + 1 : 0;
		const float ChainNoiseK = FMath::Max(0.0f, CVarBHMomentumChainNoiseScale.GetValueOnGameThread());
		SpecialMoveNoiseScale = FMath::Clamp(1.0f - ChainNoiseK * static_cast<float>(ThisMoveChainDepth), 0.35f, 1.0f);
	}
	bSpecialMoveEndsProne = bEndProne;
	bSpecialMoveEndProneRequiresInput = bEndProneRequiresInput;
	bBHopJumpQueued = false;
	ClearCosmeticSpecialMove();

	if (bIsCrouched)
	{
		UnCrouch();
	}

	MovementSpecialState = RequestedState;
	ApplyMovementSpecialState();

	if (RequestedState == EBHMovementSpecialState::Rolling)
	{
		EmitSpecialMoveNoiseAuthority(RequestedState, FName(TEXT("roll start")), 0.45f * SpecialMoveNoiseScale);
	}
	else if (RequestedState == EBHMovementSpecialState::Sliding)
	{
		EmitSpecialMoveNoiseAuthority(RequestedState, FName(TEXT("slide start")), 0.82f * SpecialMoveNoiseScale);
	}
	else if (RequestedState == EBHMovementSpecialState::Diving)
	{
		// A dive started from prone is the quieter "crawl-gap dash": a localizable scrape, not the full loud launch.
		const float DiveStartMult = bDiveFromProne ? 0.78f * FMath::Clamp(CVarBHCrawlDashNoiseScale.GetValueOnGameThread(), 0.0f, 1.0f) : 0.78f;
		EmitSpecialMoveNoiseAuthority(RequestedState, FName(TEXT("dive launch")), DiveStartMult * SpecialMoveNoiseScale);
	}

	if (bPerfectChain)
	{
		++PerfectChainCount;
		ClientNotifyPerfectChain(PerfectChainCount);
		MarkTutorialAction(TutorialActFlowChainBit); // movement-tutorial: a frame-perfect flow-chain link landed
		// Greed tell: the link that reaches the chain cap broadcasts a single loud "overextension" burst, so a master
		// who pushes the chain to its limit pays the noise economy back even though the earlier links were quiet.
		if (PerfectChainCount >= CVarBHMomentumChainMaxLinks.GetValueOnGameThread())
		{
			EmitSpecialMoveNoiseAuthority(RequestedState, FName(TEXT("overextension")), 1.5f);
		}
	}
	else
	{
		PerfectChainCount = 0;
	}

	// Movement-tutorial telemetry: latch which transient link just fired (a 0.5s tutorial poll would miss it).
	switch (RequestedState)
	{
	case EBHMovementSpecialState::Rolling: MarkTutorialAction(TutorialActRollBit); break;
	case EBHMovementSpecialState::Sliding: MarkTutorialAction(TutorialActSlideBit); break;
	case EBHMovementSpecialState::Diving:  MarkTutorialAction(TutorialActDiveBit); break;
	default: break;
	}

	ForceNetUpdate();
	return true;
}

void ABHCharacter::FinishSpecialMoveAuthority()
{
	if (!HasAuthority() || !IsSpecialMoveActive())
	{
		return;
	}

	// Remember when this transient move ended so a frame-perfect follow-up can chain (see the momentum tech).
	if (GetWorld())
	{
		LastSpecialMoveEndedTime = GetWorld()->GetTimeSeconds();
	}

	// Capture the move's exit momentum BEFORE the state fields are reset below. The move drives displacement via
	// AddActorWorldOffset and never writes the CMC velocity, so without re-injecting it the survivor brakes to a
	// dead stop the instant a roll/slide/dive ends -- right when they're fleeing. Seed forward velocity on exit.
	const FVector ExitDir = SpecialMoveDirection.GetSafeNormal2D();
	const float ExitDuration = FMath::Max(0.05f, SpecialMoveEndTime - SpecialMoveStartTime);
	const float ExitSpeed = SpecialMoveDistanceTravelled / ExitDuration;
	// A move that ended because it BONKED a wall has no forward clearance, so don't re-inject velocity into the wall
	// it just hit (it would shove the player into/along the obstacle). A clean move keeps its momentum below.
	const bool bHitWallExit = bSpecialMoveHitWall;

	const bool bShouldEndProne = bSpecialMoveEndsProne && (!bSpecialMoveEndProneRequiresInput || bProneInputHeld);
	const bool bShouldEndCrouched = bSpecialMoveEarlyBrake && !bShouldEndProne; // silent slide-stop
	MovementSpecialState = bShouldEndProne ? EBHMovementSpecialState::Prone : EBHMovementSpecialState::None;
	SpecialMoveStartTime = -999.0f;
	SpecialMoveEndTime = -999.0f;
	SpecialMoveDistanceTravelled = 0.0f;
	SpecialMoveNoiseEventMask = 0;
	bSpecialMoveEndsProne = false;
	bSpecialMoveEndProneRequiresInput = false;
	bSpecialMoveEarlyBrake = false;
	bSpecialMoveHitWall = false;
	bSpecialMoveFromProne = false;
	SpecialMoveNoiseScale = 1.0f;
	ClearCosmeticSpecialMove();
	ApplyMovementSpecialState();
	// Silent slide-stop settles into a low, quiet crouch (not a full stand) when there is headroom to do so.
	if (bShouldEndCrouched && MovementSpecialState == EBHMovementSpecialState::None
		&& GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround() && GetCharacterMovement()->CanEverCrouch())
	{
		Crouch();
	}
	// Re-inject the captured momentum so the move flows into a run instead of dead-stopping. Skip when ending prone
	// or braking to a crouch (those intentionally stop). Cap at the player's current MaxWalkSpeed (which reflects
	// sprint state) so the exit never exceeds normal speed -- the swept move already validated this much clearance.
	if (MovementSpecialState == EBHMovementSpecialState::None && !bShouldEndCrouched && !bHitWallExit && !ExitDir.IsNearlyZero())
	{
		if (UCharacterMovementComponent* ExitMove = GetCharacterMovement())
		{
			if (ExitMove->IsMovingOnGround())
			{
				ExitMove->Velocity = ExitDir * FMath::Clamp(ExitSpeed, 0.0f, ExitMove->MaxWalkSpeed);
			}
		}
	}
	ForceNetUpdate();
}

bool ABHCharacter::SetProneAuthority(bool bNewProne, bool bShowFailureMessages)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (bNewProne)
	{
		if (!CanAct() || IsSpecialMoveActive())
		{
			return false;
		}
		if (IsProne())
		{
			return true;
		}
		if (bIsCrouched)
		{
			UnCrouch();
		}
		// NOTE: do NOT clear bSprintInputHeld here. Prone speed is already enforced by the IsProne() branch in
		// RefreshMovementSpeedFromState, and the owning client never reconciles this flag, so clearing it server-side
		// only made a player who taps Prone-then-stands while holding Sprint rubber-band (client predicts sprint).
		MovementSpecialState = EBHMovementSpecialState::Prone;
		LastCaptureEvasionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastCaptureEvasionTime;
		ApplyMovementSpecialState();
		ForceNetUpdate();
		return true;
	}

	if (!IsProne())
	{
		return true;
	}

	if (!CanStandFromProne())
	{
		if (bShowFailureMessages)
		{
			SetMovementFailureReason(TEXT("No room to stand up."));
		}
		return false;
	}

	MovementSpecialState = EBHMovementSpecialState::None;
	ApplyMovementSpecialState();
	ForceNetUpdate();
	return true;
}

bool ABHCharacter::TryEnterCrawlSpacePose()
{
	if (!HasAuthority())
	{
		return false;
	}

	// Already low-profile (prone/sliding/diving) -> already sheltering; nothing to do.
	const EBHMovementSpecialState State = GetMovementSpecialState();
	if (State == EBHMovementSpecialState::Prone
		|| State == EBHMovementSpecialState::Sliding
		|| State == EBHMovementSpecialState::Diving)
	{
		return true;
	}

	// Auto-drop to prone (bShowFailureMessages=false): lets a survivor sprinting at a crawl mouth flow straight
	// into cover instead of being hard-stopped and shoved back by the volume. Returns false if they can't prone
	// this instant (mid roll / cannot act); the volume then decides whether to wait a tick or eject.
	return SetProneAuthority(true, false);
}

bool ABHCharacter::CanStandFromProne() const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!GetWorld() || !Capsule)
	{
		return true;
	}

	const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const FVector TestLocation = GetActorLocation() + FVector(0.0f, 0.0f, FMath::Max(0.0f, DefaultCapsuleHalfHeight - CurrentHalfHeight));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHProneStandTrace), false, this);
	const FCollisionShape StandingShape = FCollisionShape::MakeCapsule(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
	return !GetWorld()->OverlapBlockingTestByChannel(TestLocation, FQuat::Identity, Capsule->GetCollisionObjectType(), StandingShape, Params);
}

void ABHCharacter::ApplyMovementSpecialState()
{
	// Stamp the client-local POV clock when a replicated transient special move turns on. This
	// covers the listen-server host (authority, no cosmetic prediction) and reconciles a remote
	// client's prediction with the authoritative state. Matches the StartCosmeticSpecialMove path.
	if (BHIsTransientSpecialMove(MovementSpecialState) && MovementSpecialState != LocalSpecialAnimState)
	{
		LocalSpecialAnimStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		LocalSpecialAnimState = MovementSpecialState;
	}

	const bool bNeedsLowCapsule = IsProne()
		|| MovementSpecialState == EBHMovementSpecialState::Sliding
		|| MovementSpecialState == EBHMovementSpecialState::Diving;
	SetProneCollisionApplied(bNeedsLowCapsule);
	RefreshMovementSpeedFromState();
	if (MovementSpecialState != EBHMovementSpecialState::None || CosmeticMovementSpecialState != EBHMovementSpecialState::None)
	{
		ClearCosmeticSpecialMove();
	}
	LastRoleAnimationName = NAME_None;
}

void ABHCharacter::RefreshMovementSpeedFromState()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const float HunterPenaltyMultiplier = BHFinalEscapeHunterPenaltyActive(GetWorld(), this) ? 0.72f : 1.0f;
	const float TeacherCaptureMoveMultiplier = (BHPS && BHPS->IsAliveHunter() && IsTeacherCaptureAttackActive()) ? BHTeacherCaptureMoveMultiplier : 1.0f;
	const float WalkSpeedNow = BHRoleWalkSpeed(BHPS, WalkSpeed) * HunterPenaltyMultiplier * TeacherCaptureMoveMultiplier * BHHorrorWalkSpeedMultiplier(this, BHPS);
	const float SprintSpeedNow = BHRoleSprintSpeed(BHPS, SprintSpeed)
		* (PowerupComponent ? PowerupComponent->GetSprintSpeedMultiplier() : 1.0f)
		* HunterPenaltyMultiplier
		* TeacherCaptureMoveMultiplier
		* BHHorrorSprintSpeedMultiplier(this, BHPS);

	if (IsProne())
	{
		const float ProneSpeed = BHRoleProneSpeed(BHPS) * BHHorrorWalkSpeedMultiplier(this, BHPS);
		Movement->MaxWalkSpeed = ProneSpeed;
		Movement->MaxWalkSpeedCrouched = ProneSpeed;
		return;
	}

	if (IsSpecialMoveActive())
	{
		const FBHMovementSpecialTuning SpecialTuning = BHGetSpecialMoveTuning(BHPS, MovementSpecialState);
		const float SpecialMoveSpeed = (SpecialTuning.DurationSeconds > KINDA_SMALL_NUMBER
			? SpecialTuning.Curve.Distance / SpecialTuning.DurationSeconds
			: WalkSpeedNow) * SpecialMoveMomentumScale;
		Movement->MaxWalkSpeed = FMath::Max(WalkSpeedNow, SpecialMoveSpeed);
		return;
	}

	Movement->MaxWalkSpeed = bSprintInputHeld && Stamina > MaxStamina * 0.08f ? SprintSpeedNow : WalkSpeedNow;
	Movement->MaxWalkSpeedCrouched = 205.0f * BHHorrorWalkSpeedMultiplier(this, BHPS);
}

void ABHCharacter::SetProneCollisionApplied(bool bApplied)
{
	if (bProneCollisionApplied == bApplied)
	{
		return;
	}

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		bProneCollisionApplied = bApplied;
		return;
	}

	const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float TargetHalfHeight = bApplied ? BHProneCapsuleHalfHeight : DefaultCapsuleHalfHeight;
	const FVector TargetLocation = GetActorLocation() + FVector(0.0f, 0.0f, bApplied ? -(CurrentHalfHeight - TargetHalfHeight) : (TargetHalfHeight - CurrentHalfHeight));
	Capsule->SetCapsuleSize(DefaultCapsuleRadius, TargetHalfHeight, true);
	SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
	bProneCollisionApplied = bApplied;
}

void ABHCharacter::TryCapture()
{
	// LeftMouseButton doubles as the capture key; while the question cursor is up a click is meant
	// for the panel, so swallow the capture attempt (OnQuestionPointerDown handles the click).
	if (bQuestionCursorActive)
	{
		return;
	}
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

void ABHCharacter::SubmitAnswer(int32 AnswerIndex, bool bVisual)
{
	// When a calculation question is focused, the 1-4 keys type digits 1-4 into the numeric
	// buffer instead of submitting a multiple-choice answer. All other questions are unchanged.
	if (IsCalculationEntryActive())
	{
		NumericEntryDigit(AnswerIndex + 1);
		return;
	}

	if (!CanAct())
	{
		SendStatusMessage(bHiddenInLocker ? TEXT("You cannot answer while hiding.") : TEXT("You cannot answer right now."));
		return;
	}

	ServerSubmitAnswer(AnswerIndex, bVisual);
}

void ABHCharacter::SetClientFocusedQuestionStation(ABHObjectiveStation* Station)
{
	// Pushed by the local HUD each frame. Clearing the buffer when the focused question
	// changes prevents a value typed at one station leaking into another.
	if (ClientFocusedQuestionStation.Get() != Station)
	{
		ClientFocusedQuestionStation = Station;
		NumericAnswerEntry.Reset();
	}
}

void ABHCharacter::ToggleQuestionCursor()
{
	if (!IsLocallyControlled() || !IsPlayerControlled())
	{
		return;
	}
	if (!bQuestionCursorActive)
	{
		const ABHObjectiveStation* Station = ClientFocusedQuestionStation.Get();
		const bool bAnswerable = Station && Station->IsDirectorActive() && !Station->IsCompleted()
			&& !Station->IsQuestionSolved() && Station->GetQuestionChoiceCount() > 0;
		if (!bAnswerable)
		{
			SendStatusMessage(TEXT("Look at a question checkpoint to answer with the mouse."));
			return;
		}
		bQuestionCursorActive = true;
		QuestionDraggedPiece = INDEX_NONE;
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(GetController()))
		{
			PC->SetQuestionCursorMode(true);
		}
	}
	else
	{
		bQuestionCursorActive = false;
		QuestionDraggedPiece = INDEX_NONE;
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(GetController()))
		{
			PC->SetQuestionCursorMode(false);
		}
	}
}

void ABHCharacter::UseNodeMarker()
{
	if (!IsLocallyControlled() || !IsPlayerControlled() || !GetWorld())
	{
		return;
	}
	ABHPlayerController* PC = Cast<ABHPlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	const FVector Origin = GetActorLocation();
	float BestDistSq = TNumericLimits<float>::Max();
	FVector BestLocation = FVector::ZeroVector;
	bool bFoundNode = false;
	for (TActorIterator<ABHObjectiveStation> It(GetWorld()); It; ++It)
	{
		const ABHObjectiveStation* Station = *It;
		if (!Station || !Station->IsDirectorActive() || Station->IsCompleted() || Station->IsTeacherMirrorTrapNode())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Station->GetActorLocation(), Origin);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestLocation = Station->GetActorLocation();
			bFoundNode = true;
		}
	}
	// The hidden train-roof breaker is also a trackable "node" (the intermission/lobby car has no question
	// stations), so N points a curious passenger to it.
	for (TActorIterator<ABHTrainRoofBreaker> It(GetWorld()); It; ++It)
	{
		const ABHTrainRoofBreaker* Breaker = *It;
		if (!Breaker)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Breaker->GetActorLocation(), Origin);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestLocation = Breaker->GetActorLocation();
			bFoundNode = true;
		}
	}
	if (!bFoundNode)
	{
		SendStatusMessage(TEXT("No trackable nodes found."));
		return;
	}
	PC->NodeMarkerLocation = BestLocation;
	PC->NodeMarkerUntilTime = GetWorld()->GetTimeSeconds() + 8.0f;
}

void ABHCharacter::RequestResetToTrainInterior()
{
	if (!IsLocallyControlled() || !IsPlayerControlled())
	{
		return;
	}
	// Authoritative teleport: ask the server to drop us back on a safe interior spawn (it gates to train levels).
	ServerResetToTrainInterior();
}

void ABHCharacter::ServerResetToTrainInterior_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->ResetPlayerToTrainInterior(GetController());
	}
}

void ABHCharacter::UpdateQuestionInteractionState()
{
	// Owning-client upkeep only (the Tick caller already gates on IsLocallyControlled/IsPlayerControlled).
	ABHObjectiveStation* Station = ClientFocusedQuestionStation.Get();
	const bool bAnswerable = Station && Station->IsDirectorActive() && !Station->IsCompleted()
		&& !Station->IsQuestionSolved() && Station->GetQuestionChoiceCount() > 0;

	// Auto-exit the cursor when the answerable question goes away (solved, completed, looked away).
	if (bQuestionCursorActive && !bAnswerable)
	{
		bQuestionCursorActive = false;
		QuestionDraggedPiece = INDEX_NONE;
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(GetController()))
		{
			PC->SetQuestionCursorMode(false);
		}
	}

	// Rebuild the local drag arrangement whenever the focused arrangement question's piece set
	// changes (a reload after a right/wrong answer, or walking to a different station). Keyed on the
	// station + the shuffled pieces so an identical reload is detected and reset.
	FString NewKey;
	if (bAnswerable && Station->HasInteractiveArrangement())
	{
		NewKey = FString::Printf(TEXT("%s|%d|"), *Station->GetName(), Station->GetInteractiveSlots().Num());
		for (const FString& Piece : Station->GetInteractivePieces())
		{
			NewKey += Piece;
			NewKey += TEXT("\x1f");
		}
	}
	if (NewKey != QuestionArrangementKey)
	{
		QuestionArrangementKey = NewKey;
		QuestionArrangement.Reset();
		if (!NewKey.IsEmpty() && Station)
		{
			QuestionArrangement.Init(INDEX_NONE, Station->GetInteractiveSlots().Num());
		}
		QuestionDraggedPiece = INDEX_NONE;
	}
}

void ABHCharacter::OnQuestionPointerDown()
{
	if (!bQuestionCursorActive || !IsLocallyControlled())
	{
		return;
	}
	ABHPlayerController* PC = Cast<ABHPlayerController>(GetController());
	ABHHUD* HUD = PC ? Cast<ABHHUD>(PC->GetHUD()) : nullptr;
	if (!PC || !HUD)
	{
		return;
	}
	float MX = 0.0f;
	float MY = 0.0f;
	if (!PC->GetMousePosition(MX, MY))
	{
		return;
	}
	FBHQuestionHitRegion Region;
	const bool bHit = HUD->GetQuestionRegionAtCursor(FVector2D(MX, MY), Region);

	// Click-to-pick / click-to-place (no hold-drag): a single click picks up a piece, the next click
	// drops it on a slot. A button is never held, so the mouse stays free to move the cursor and the
	// camera never swings (look is also frozen while the cursor is up; see Turn()).
	if (QuestionDraggedPiece != INDEX_NONE)
	{
		if (bHit && Region.Kind == EBHQuestionRegionKind::Slot && QuestionArrangement.IsValidIndex(Region.IndexA))
		{
			// Place into this slot; any piece already there is displaced back to the tray.
			QuestionArrangement[Region.IndexA] = QuestionDraggedPiece;
			QuestionDraggedPiece = INDEX_NONE;
		}
		else if (bHit && Region.Kind == EBHQuestionRegionKind::Piece)
		{
			// Switch the held piece to the one just clicked (the previous one returns to the tray).
			QuestionDraggedPiece = Region.IndexA;
			for (int32& Slot : QuestionArrangement)
			{
				if (Slot == Region.IndexA)
				{
					Slot = INDEX_NONE;
				}
			}
		}
		else
		{
			// Clicked empty space / a choice / submit while carrying a piece: cancel and return it.
			QuestionDraggedPiece = INDEX_NONE;
		}
		return;
	}

	if (!bHit)
	{
		return;
	}
	switch (Region.Kind)
	{
	case EBHQuestionRegionKind::Piece:
		// Pick up (from the tray or out of a slot it currently occupies).
		QuestionDraggedPiece = Region.IndexA;
		for (int32& Slot : QuestionArrangement)
		{
			if (Slot == Region.IndexA)
			{
				Slot = INDEX_NONE;
			}
		}
		break;
	case EBHQuestionRegionKind::Slot:
		// Clicking a filled slot with nothing held lifts that piece so it can be moved.
		if (QuestionArrangement.IsValidIndex(Region.IndexA) && QuestionArrangement[Region.IndexA] != INDEX_NONE)
		{
			QuestionDraggedPiece = QuestionArrangement[Region.IndexA];
			QuestionArrangement[Region.IndexA] = INDEX_NONE;
		}
		break;
	case EBHQuestionRegionKind::Choice:
		// Picking a text choice row is a plain multiple-choice answer (no visual bonus).
		SubmitAnswer(Region.IndexA, /*bVisual=*/false);
		break;
	case EBHQuestionRegionKind::DiagramChoice:
		// Clicking the answer ON the diagram is a visual answer -> earns the mastery bonus.
		SubmitAnswer(Region.IndexA, /*bVisual=*/true);
		break;
	case EBHQuestionRegionKind::Submit:
		if (QuestionArrangement.Num() > 0 && !QuestionArrangement.Contains(INDEX_NONE))
		{
			ServerSubmitArrangement(QuestionArrangement);
		}
		else
		{
			SendStatusMessage(TEXT("Place a piece on every slot first."));
		}
		break;
	case EBHQuestionRegionKind::KeypadDigit:
		NumericEntryDigit(Region.IndexA);
		break;
	case EBHQuestionRegionKind::KeypadDecimal:
		NumericEntryDecimal();
		break;
	case EBHQuestionRegionKind::KeypadMinus:
		NumericEntryMinus();
		break;
	case EBHQuestionRegionKind::KeypadBackspace:
		NumericEntryBackspace();
		break;
	case EBHQuestionRegionKind::KeypadEnter:
		ConfirmNumericAnswer();
		break;
	default:
		break;
	}
}

void ABHCharacter::OnQuestionPointerUp()
{
	// Intentionally empty: answering uses click-to-pick / click-to-place on press
	// (OnQuestionPointerDown), so there is no hold-drag to finish on release.
}

void ABHCharacter::ServerSubmitArrangement_Implementation(const TArray<int32>& SlotToPiece)
{
	if (!CanAct())
	{
		return;
	}
	// Resolve the station the player is looking at on the server (same view fallback as the
	// multiple-choice / numeric submit paths), then grade the arrangement authoritatively.
	AActor* Target = nullptr;
	FindInteractableFromView(Target, 225.0f);
	if (ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target))
	{
		if (IsValidInteractionTarget(Station))
		{
			Station->SubmitArrangement(this, SlotToPiece);
		}
	}
}

bool ABHCharacter::IsCalculationEntryActive() const
{
	const ABHObjectiveStation* Station = ClientFocusedQuestionStation.Get();
	return Station != nullptr
		&& Station->IsDirectorActive()
		&& !Station->IsCompleted()
		&& !Station->IsQuestionSolved()
		&& Station->GetQuestionType() == EBHQuestionType::Calculation;
}

void ABHCharacter::NumericEntryDigit(int32 Digit)
{
	if (!IsLocallyControlled() || !IsCalculationEntryActive() || NumericAnswerEntry.Len() >= 12)
	{
		return;
	}
	NumericAnswerEntry.AppendChar(static_cast<TCHAR>('0' + FMath::Clamp(Digit, 0, 9)));
}

void ABHCharacter::NumericEntryDecimal()
{
	if (!IsLocallyControlled() || !IsCalculationEntryActive() || NumericAnswerEntry.Len() >= 12 || NumericAnswerEntry.Contains(TEXT(".")))
	{
		return;
	}
	NumericAnswerEntry.AppendChar(TCHAR('.'));
}

void ABHCharacter::NumericEntryMinus()
{
	// A leading minus only; meaningless mid-number.
	if (!IsLocallyControlled() || !IsCalculationEntryActive() || !NumericAnswerEntry.IsEmpty())
	{
		return;
	}
	NumericAnswerEntry.AppendChar(TCHAR('-'));
}

void ABHCharacter::NumericEntryBackspace()
{
	if (!IsLocallyControlled() || !IsCalculationEntryActive() || NumericAnswerEntry.IsEmpty())
	{
		return;
	}
	NumericAnswerEntry.LeftChopInline(1);
}

void ABHCharacter::ConfirmNumericAnswer()
{
	if (!IsLocallyControlled() || !IsCalculationEntryActive() || NumericAnswerEntry.IsEmpty())
	{
		return;
	}
	if (!CanAct())
	{
		SendStatusMessage(bHiddenInLocker ? TEXT("You cannot answer while hiding.") : TEXT("You cannot answer right now."));
		return;
	}
	const float Value = FCString::Atof(*NumericAnswerEntry);
	ServerSubmitNumericAnswer(Value);
	NumericAnswerEntry.Reset();
}

void ABHCharacter::ServerSubmitNumericAnswer_Implementation(float Value)
{
	SubmitNumericAnswerAuthority(Value);
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

bool ABHCharacter::UsePowerupByType(EBHPowerupType Type)
{
	ServerUsePowerup(Type);
	return true;
}

void ABHCharacter::UsePowerupSlotOne()
{
	UsePowerupByType(EBHPowerupType::StaminaBoost);
}

void ABHCharacter::UsePowerupSlotTwo()
{
	UsePowerupByType(EBHPowerupType::SprintBurst);
}

void ABHCharacter::UsePowerupSlotThree()
{
	UsePowerupByType(EBHPowerupType::FlashlightBoost);
}

void ABHCharacter::UsePowerupSlotFour()
{
	UsePowerupByType(EBHPowerupType::QuestionHint);
}

void ABHCharacter::UsePowerupSlotFive()
{
	UsePowerupByType(EBHPowerupType::DecoySound);
}

void ABHCharacter::UsePowerupSlotSix()
{
	UsePowerupByType(EBHPowerupType::DoorRush);
}

void ABHCharacter::TesterGrantTrainResources()
{
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(Controller))
	{
		BHPC->TesterGrantTrainResources();
	}
}

void ABHCharacter::TesterOpenTrainIntermission()
{
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(Controller))
	{
		BHPC->TesterOpenTrainIntermission();
	}
}

void ABHCharacter::TesterAdvanceTrainPhase()
{
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(Controller))
	{
		BHPC->TesterAdvanceTrainPhase();
	}
}

void ABHCharacter::TesterLoadFinalStation()
{
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(Controller))
	{
		BHPC->TesterLoadFinalStation();
	}
}

void ABHCharacter::TesterTriggerFinalEscape()
{
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(Controller))
	{
		BHPC->TesterTriggerFinalEscape();
	}
}

void ABHCharacter::TesterForceFinalRecap()
{
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(Controller))
	{
		BHPC->TesterForceFinalRecap();
	}
}

bool ABHCharacter::CanAct() const
{
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bFrozenByFinalEscape = BHGS && (BHGS->bPlayerInputFrozen || (BHGS->bHunterInputFrozen && BHPS && BHPS->IsAliveHunter()));
	return !bFrozenByFinalEscape && !bHiddenInLocker && !bOutOfPlay && (!BHPS || BHPS->LifeState == EBHPlayerLifeState::Alive);
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

bool ABHCharacter::HasInteractionLineOfSight(AActor* Target) const
{
	// Server-side line-of-sight gate: a client-supplied Target only passes the distance
	// check in IsValidInteractionTarget, which lets a modded client interact through a
	// thin wall. Reuse the same view point and ECC_Visibility channel as
	// FindInteractableFromView and reject if a *different* blocking actor sits between the
	// player's eyes and the target. Self and the target are ignored so legitimate
	// interactions (looking right at the object, no wall in the way) still pass.
	if (!Target || !GetWorld())
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
	}
	else
	{
		// No view point available on the server; fall back to permissive so we never
		// block a legitimate interaction just because the view source is missing.
		return true;
	}

	// Aim the gate at the target's bounds CENTRE, not its pivot. A wall-mounted module (button panel,
	// switch, terminal) commonly has its actor origin ON or just behind the wall it is fixed to, with the
	// interactable face protruding into the room; a trace to the pivot then hits that mounting wall even
	// when the player is looking straight at the object. The bounds centre sits inside the visible body,
	// in front of the wall, so a clear look passes.
	FVector TargetPoint = Target->GetActorLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Target->GetActorBounds(false, TargetPoint, BoundsExtent);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHInteractLoS), false, this);
	Params.AddIgnoredActor(Target);

	// Trace only against world geometry (static + dynamic), NOT pawns: the gate must reject a WALL
	// between the player and the object, but a teammate's body standing in front of an objective in a
	// crowded room must not deny a legitimate interaction.
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult VisibilityHit;
	if (GetWorld()->LineTraceSingleByObjectType(VisibilityHit, Start, TargetPoint, ObjParams, Params))
	{
		// Something world-geometry is in the line. Reject ONLY when it is a *separate* wall sitting
		// between the player and the object (the through-wall exploit). When the hit lands right at the
		// target -- e.g. the wall or frame the module is mounted on, inside its bounds plus a small
		// tolerance -- the player is legitimately looking at the object, so let it through.
		const float MountTolerance = 50.0f;
		const FBox TargetBox = FBox::BuildAABB(TargetPoint, BoundsExtent + FVector(MountTolerance));
		if (!TargetBox.IsInsideOrOn(VisibilityHit.Location))
		{
			return false;
		}
	}

	return true;
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
		return TEXT("Press Enter to ready up before interacting.");
	}

	if (Target && Target->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
	{
		const FBHInteractionPromptInfo PromptInfo = IBHInteractableInterface::Execute_GetInteractionPromptInfo(Target, const_cast<ABHCharacter*>(this));
		if (PromptInfo.bUsePromptInfo && !PromptInfo.DisabledReason.IsEmpty())
		{
			return PromptInfo.DisabledReason.ToString();
		}
	}

	if (BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby)
	{
		return TEXT("Round is still in the lobby. Press Enter on every player to start.");
	}

	if (BHGS && BHGS->RoundPhase == EBHRoundPhase::Prep)
	{
		return TEXT("Role warmup: try flashlight, lockers, questions, decoys, scans, captures, and Hall Monitor tools. The real hunt resets after warmup.");
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

bool ABHCharacter::ResolveHallMonitorMarkerLocation(FVector& OutLocation) const
{
	if (!GetWorld())
	{
		return false;
	}

	FVector ViewLocation = GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
	FRotator ViewRotation = GetActorRotation();
	if (Controller)
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	FVector AimDirection = ViewRotation.Vector();
	if (!AimDirection.Normalize())
	{
		AimDirection = GetActorForwardVector();
	}

	constexpr float MinMarkerDistance = 900.0f;
	constexpr float MaxMarkerDistance = 4200.0f;
	const FVector TraceEnd = ViewLocation + AimDirection * MaxMarkerDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHallMonitorMarkerTrace), false, this);
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	FVector MarkerLocation = TraceEnd;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params) && Hit.bBlockingHit)
	{
		MarkerLocation = Hit.ImpactPoint;
	}

	FVector FlatDirection = MarkerLocation - GetActorLocation();
	FlatDirection.Z = 0.0f;
	if (!FlatDirection.Normalize())
	{
		FlatDirection = AimDirection;
		FlatDirection.Z = 0.0f;
		if (!FlatDirection.Normalize())
		{
			FlatDirection = GetActorForwardVector();
			FlatDirection.Z = 0.0f;
			if (!FlatDirection.Normalize())
			{
				FlatDirection = FVector::ForwardVector;
			}
		}
	}

	const float FlatDistance = FVector::Dist2D(MarkerLocation, GetActorLocation());
	if (FlatDistance < MinMarkerDistance)
	{
		MarkerLocation = GetActorLocation() + FlatDirection * MinMarkerDistance;
	}
	else if (FlatDistance > MaxMarkerDistance)
	{
		MarkerLocation = GetActorLocation() + FlatDirection * MaxMarkerDistance;
	}

	MarkerLocation.Z = GetActorLocation().Z;
	OutLocation = MarkerLocation;
	return true;
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
		// Point at the nearest ALIVE survivor, iterating characters (not just human player controllers) so an AI
		// student counts too: a hall monitor's real hint should be able to surface a bot survivor, and the solo
		// Monitor tutorial's only student IS a bot - the old player-controller-only scan reported "no signal" and
		// made the lesson's Q look broken.
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
		{
			ABHCharacter* Target = *It;
			const ABHPlayerState* TargetPS = Target ? Target->GetPlayerState<ABHPlayerState>() : nullptr;
			if (!Target || Target == this || !TargetPS || !TargetPS->IsAliveSurvivor())
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
		if (!ResolveHallMonitorMarkerLocation(HintLocation))
		{
			const float Angle = FMath::FRandRange(-PI, PI);
			const float Radius = FMath::FRandRange(1200.0f, 3600.0f);
			HintLocation = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;
		}
		bFoundSignal = true;
	}

	int32 HuntersNotified = 0;
	const FString SenderName = FakePS && !FakePS->GetPlayerName().IsEmpty() ? FakePS->GetPlayerName() : FString(TEXT("Hall monitor"));
	if (!bRealHint)
	{
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			HuntersNotified = BHGM->NotifyHallMonitorMisdirection(HintLocation, SenderName);
		}

		SendStatusMessage(FString::Printf(TEXT("False corridor marker sent to %d Teacher(s)."), HuntersNotified));
		return;
	}

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

	SendStatusMessage(FString::Printf(TEXT("Real hint sent to %d Teacher(s)."), HuntersNotified));
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
	if (TeacherWeaponRoot)
	{
		TeacherWeaponRoot->SetRelativeLocation(BHTeacherWeaponIdleLocation);
		TeacherWeaponRoot->SetRelativeRotation(BHTeacherWeaponIdleRotation);
		TeacherWeaponRoot->SetRelativeScale3D(FVector::OneVector);
	}
	BHConfigureAvatarPart(TeacherWeaponMesh, nullptr, Material, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector);
	BHConfigureAvatarPart(TeacherWeaponHandleMesh, Cylinder, Material, FVector(0.0f, 0.0f, 12.0f), FRotator::ZeroRotator, FVector(0.045f, 0.045f, 0.58f));
	BHConfigureAvatarPart(TeacherWeaponGripMesh, Cylinder, Material, FVector(0.0f, 0.0f, -25.0f), FRotator::ZeroRotator, FVector(0.058f, 0.058f, 0.18f));
	BHConfigureAvatarPart(TeacherWeaponHeadMesh, Cube, Material, FVector(0.0f, 0.0f, 50.0f), FRotator::ZeroRotator, FVector(0.11f, 0.045f, 0.08f));
	BHConfigureAvatarPart(TeacherWeaponBladeMesh, Cube, Material, FVector(0.0f, -9.0f, 49.0f), FRotator(0.0f, 0.0f, -8.0f), FVector(0.045f, 0.17f, 0.135f));

	UStaticMeshComponent* RoleAccessoryParts[] = {
		RoleHeadwearMesh,
		RoleHeadwearAccentMesh,
		RoleHeadwearDetailMesh,
		RoleGearMesh,
		RoleGearAccentMesh,
		RoleGearLeftStrapMesh,
		RoleGearRightStrapMesh,
		RoleGearDetailMesh,
		TeacherWeaponMesh,
		TeacherWeaponHandleMesh,
		TeacherWeaponHeadMesh,
		TeacherWeaponBladeMesh,
		TeacherWeaponGripMesh
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

	const EBHMovementSpecialState VisualSpecialState = MovementSpecialState != EBHMovementSpecialState::None ? MovementSpecialState : CosmeticMovementSpecialState;
	const float CrouchAlpha = bIsCrouched ? 1.0f : 0.0f;
	const float ProneAlpha = VisualSpecialState == EBHMovementSpecialState::Prone ? 1.0f : 0.0f;
	const float SlideDiveAlpha = (VisualSpecialState == EBHMovementSpecialState::Sliding || VisualSpecialState == EBHMovementSpecialState::Diving) ? 1.0f : 0.0f;
	const float LowProfileAlpha = FMath::Max(CrouchAlpha, FMath::Max(ProneAlpha, SlideDiveAlpha));
	const float Step = FMath::Sin(AvatarAnimTime);
	const float CounterStep = -Step;
	const float MoveAlpha = FMath::Clamp(AvatarMoveAlpha, 0.0f, 1.0f);
	const float SprintAlpha = FMath::Clamp(AvatarSprintAlpha, 0.0f, 1.0f);
	const float BobZ = FMath::Abs(FMath::Sin(AvatarAnimTime * 2.0f)) * FMath::Lerp(0.6f, 2.8f, SprintAlpha) * MoveAlpha;
	const float Breath = FMath::Sin(AvatarBreathTime) * FMath::Lerp(0.9f, 1.8f, HorrorAlpha);

	AvatarRoot->SetRelativeLocation(FVector(0.0f, 0.0f, BobZ - CrouchAlpha * 14.0f - ProneAlpha * 44.0f - SlideDiveAlpha * 30.0f));

	const FRotator TorsoRotation(
		-FMath::Lerp(0.0f, 7.0f, SprintAlpha) * MoveAlpha - CrouchAlpha * 7.0f - ProneAlpha * 62.0f - SlideDiveAlpha * 36.0f + Breath * 0.45f,
		0.0f,
		Strafe * -5.5f);
	if (RoleModelRoot)
	{
		RoleModelRoot->SetRelativeLocation(FVector(0.0f, 0.0f, -CrouchAlpha * 4.0f - ProneAlpha * 20.0f - SlideDiveAlpha * 12.0f));
		RoleModelRoot->SetRelativeRotation(FRotator(
			TorsoRotation.Pitch * 0.72f,
			Step * MoveAlpha * FMath::Lerp(1.2f, 2.8f, SprintAlpha),
			TorsoRotation.Roll * 0.62f));
	}
	UpdateTeacherWeaponSwingVisuals();
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
		WaistMesh->SetRelativeRotation(FRotator(LowProfileAlpha * 4.0f, 0.0f, Strafe * -3.0f));
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
			AimPitch * 0.42f + Breath * 0.35f - CrouchAlpha * 3.0f + ProneAlpha * 40.0f + SlideDiveAlpha * 18.0f,
			AimYaw * 0.55f,
			Step * MoveAlpha * 1.8f));
	}

	const float ArmSwing = FMath::Lerp(18.0f, 48.0f, SprintAlpha) * MoveAlpha;
	const float ArmRoll = FMath::Lerp(5.0f, 13.0f, SprintAlpha) * MoveAlpha;
	const float ElbowSwing = FMath::Lerp(8.0f, 24.0f, SprintAlpha) * MoveAlpha;
	if (LeftShoulderJoint)
	{
		LeftShoulderJoint->SetRelativeRotation(FRotator(CounterStep * ArmSwing - LowProfileAlpha * 10.0f - ProneAlpha * 28.0f, 0.0f, -ArmRoll));
	}
	if (RightShoulderJoint)
	{
		RightShoulderJoint->SetRelativeRotation(FRotator(Step * ArmSwing - LowProfileAlpha * 10.0f - ProneAlpha * 28.0f, 0.0f, ArmRoll));
	}
	if (LeftElbowJoint)
	{
		LeftElbowJoint->SetRelativeRotation(FRotator(6.0f + FMath::Max(0.0f, Step) * ElbowSwing + LowProfileAlpha * 8.0f + ProneAlpha * 18.0f, 0.0f, 0.0f));
	}
	if (RightElbowJoint)
	{
		RightElbowJoint->SetRelativeRotation(FRotator(6.0f + FMath::Max(0.0f, CounterStep) * ElbowSwing + LowProfileAlpha * 8.0f + ProneAlpha * 18.0f, 0.0f, 0.0f));
	}

	const float LegSwing = FMath::Lerp(15.0f, 34.0f, SprintAlpha) * MoveAlpha;
	const float KneeSwing = FMath::Lerp(10.0f, 32.0f, SprintAlpha) * MoveAlpha;
	if (LeftHipJoint)
	{
		LeftHipJoint->SetRelativeRotation(FRotator(Step * LegSwing + LowProfileAlpha * 23.0f + ProneAlpha * 30.0f, 0.0f, -Strafe * 4.0f));
	}
	if (RightHipJoint)
	{
		RightHipJoint->SetRelativeRotation(FRotator(CounterStep * LegSwing + LowProfileAlpha * 23.0f + ProneAlpha * 30.0f, 0.0f, -Strafe * 4.0f));
	}
	if (LeftKneeJoint)
	{
		LeftKneeJoint->SetRelativeRotation(FRotator(FMath::Max(0.0f, CounterStep) * KneeSwing + LowProfileAlpha * 18.0f + ProneAlpha * 22.0f, 0.0f, 0.0f));
	}
	if (RightKneeJoint)
	{
		RightKneeJoint->SetRelativeRotation(FRotator(FMath::Max(0.0f, Step) * KneeSwing + LowProfileAlpha * 18.0f + ProneAlpha * 22.0f, 0.0f, 0.0f));
	}
	if (LeftFootMesh)
	{
		LeftFootMesh->SetRelativeRotation(FRotator(FMath::Clamp(CounterStep * 12.0f * MoveAlpha, -12.0f, 14.0f) - LowProfileAlpha * 6.0f - ProneAlpha * 16.0f, 0.0f, 0.0f));
	}
	if (RightFootMesh)
	{
		RightFootMesh->SetRelativeRotation(FRotator(FMath::Clamp(Step * 12.0f * MoveAlpha, -12.0f, 14.0f) - LowProfileAlpha * 6.0f - ProneAlpha * 16.0f, 0.0f, 0.0f));
	}

	// Additive Hunter swing lunge: wind the right arm/torso back then drive it forward in lockstep
	// with the procedural weapon swing. Cosmetic and visible in third person on every client (it
	// runs off the multicast-stamped swing timers). Applied after the base joint writes so it is
	// additive to locomotion rather than clobbered by it.
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter && GetWorld())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now < TeacherMeleeSwingEndTime && TeacherMeleeSwingEndTime > TeacherMeleeSwingStartTime)
		{
			const float S = FMath::Clamp((Now - TeacherMeleeSwingStartTime) / FMath::Max(0.01f, BHTeacherMeleeSwingDuration), 0.0f, 1.0f);
			const float LungeScale = BHPOVIntensityScale(BHResolvePOVAnimationTuning().Intensity);

			float ShoulderPitch = 0.0f;   // negative = arm forward
			float ShoulderYaw = 0.0f;
			float ElbowExtend = 0.0f;     // negative = straighten
			float TorsoLean = 0.0f;       // negative = lean forward (matches TorsoRotation convention)
			if (S < 0.28f)
			{
				const float T = S / 0.28f;
				ShoulderPitch = T * 26.0f;    // pull back
				TorsoLean = T * 8.0f;         // lean back
				ElbowExtend = T * 18.0f;      // cock the elbow
			}
			else if (S < 0.68f)
			{
				const float T = (S - 0.28f) / 0.40f;
				ShoulderPitch = FMath::Lerp(26.0f, -30.0f, T);
				ShoulderYaw = FMath::Lerp(0.0f, 22.0f, T);
				ElbowExtend = FMath::Lerp(18.0f, -14.0f, T);
				TorsoLean = FMath::Lerp(8.0f, -10.0f, T);
			}
			else
			{
				const float T = (S - 0.68f) / 0.32f;
				ShoulderPitch = FMath::InterpEaseOut(-30.0f, 0.0f, T, 2.0f);
				ShoulderYaw = FMath::Lerp(22.0f, 0.0f, T);
				ElbowExtend = FMath::InterpEaseOut(-14.0f, 0.0f, T, 2.0f);
				TorsoLean = FMath::InterpEaseOut(-10.0f, 0.0f, T, 2.0f);
			}

			ShoulderPitch *= LungeScale;
			ShoulderYaw *= LungeScale;
			ElbowExtend *= LungeScale;
			TorsoLean *= LungeScale;

			if (RightShoulderJoint)
			{
				RightShoulderJoint->AddRelativeRotation(FRotator(ShoulderPitch, ShoulderYaw, 0.0f));
			}
			if (RightElbowJoint)
			{
				RightElbowJoint->AddRelativeRotation(FRotator(ElbowExtend, 0.0f, 0.0f));
			}
			const FRotator TorsoLunge(TorsoLean, 0.0f, 0.0f);
			if (BodyMesh)
			{
				BodyMesh->AddRelativeRotation(TorsoLunge);
			}
			if (ChestMesh)
			{
				ChestMesh->AddRelativeRotation(TorsoLunge);
			}
			if (RoleModelRoot)
			{
				RoleModelRoot->AddRelativeRotation(FRotator(TorsoLean * 0.72f, 0.0f, 0.0f));
			}
		}
	}
}

void ABHCharacter::UpdateRoleSkeletalAnimation(float Speed2D, float MoveAlpha, float SprintAlpha, bool bGrounded)
{
	if (!bUsingRoleModel || !RoleSkeletalMesh || !RoleSkeletalMesh->IsVisible())
	{
		return;
	}

	// Seated cosmetic pose: hide the legs + drop the torso to the seat (runs on every client).
	ApplySeatedAvatar(bSeated);

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (UBHMovementAnimInstance* MovementAnim = Cast<UBHMovementAnimInstance>(RoleSkeletalMesh->GetAnimInstance()))
	{
		const bool bMovingProneState = MovementSpecialState == EBHMovementSpecialState::Prone && Speed2D > 20.0f;
		MovementAnim->SetBlackoutMovementState(MovementSpecialState, CosmeticMovementSpecialState, Speed2D, bMovingProneState, bGrounded, MovementFailurePulse);
		return;
	}

	FName DesiredAnimation(BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter ? TEXT("IdleWeapon") : TEXT("Idle"));
	float DesiredPlayRate = 1.0f;
	bool bLoopAnimation = true;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bMeleeSwinging = BHPS
		&& BHPS->PlayerRole == EBHPlayerRole::Hunter
		&& Now < TeacherMeleeSwingEndTime;
	if (bMeleeSwinging)
	{
		DesiredAnimation = FName(TEXT("MeleeSwing"));
		DesiredPlayRate = 1.15f;
		bLoopAnimation = false;
	}
	const EBHMovementSpecialState VisualState = MovementSpecialState != EBHMovementSpecialState::None ? MovementSpecialState : CosmeticMovementSpecialState;
	if (!bMeleeSwinging && VisualState != EBHMovementSpecialState::None)
	{
		const bool bMovingProne = VisualState == EBHMovementSpecialState::Prone && Speed2D > 20.0f;
		DesiredAnimation = BHAnimationNameForSpecialState(VisualState, bMovingProne);
		bLoopAnimation = VisualState == EBHMovementSpecialState::Prone;
		if (VisualState == EBHMovementSpecialState::Prone)
		{
			DesiredPlayRate = bMovingProne ? FMath::GetMappedRangeValueClamped(FVector2D(20.0f, BHRoleProneSpeed(GetPlayerState<ABHPlayerState>())), FVector2D(0.65f, 1.05f), Speed2D) : 1.0f;
		}
	}
	else if (!bMeleeSwinging && CanAct() && bGrounded && Speed2D > 35.0f && MoveAlpha > 0.08f)
	{
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
		UAnimSequence* Animation = VisualState != EBHMovementSpecialState::None && !bMeleeSwinging
			? BHLoadSpecialAnimation(VisualState, VisualState == EBHMovementSpecialState::Prone && Speed2D > 20.0f)
			: BHLoadRoleMovementAnimation(DesiredAnimation);
		if (Animation)
		{
			RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			RoleSkeletalMesh->PlayAnimation(Animation, bLoopAnimation);
			LastRoleAnimationName = DesiredAnimation;
		}
	}
	RoleSkeletalMesh->SetPlayRate(DesiredPlayRate);
}

void ABHCharacter::ApplySeatedAvatar(bool bSeatedNow)
{
	if (!RoleSkeletalMesh)
	{
		return;
	}

	// Hide/show the leg bones only on a state change (HideBoneByName also hides their children).
	if (bSeatedNow != bSeatedAvatarApplied)
	{
		bSeatedAvatarApplied = bSeatedNow;
		const TArray<FName> LegBones = BHCollectLegBones(RoleSkeletalMesh);
		for (const FName& BoneName : LegBones)
		{
			if (bSeatedNow)
			{
				RoleSkeletalMesh->HideBoneByName(BoneName, PBO_None);
			}
			else
			{
				RoleSkeletalMesh->UnHideBoneByName(BoneName);
			}
		}
	}

	// Drop the (legless) torso so it rests at seat height; restore the standing offset when up. ~32cm reads as
	// sitting on the lobby stools (seat ~60cm) -- tune this single value if the pose sits high/low.
	if (bRoleMeshStandCaptured)
	{
		const float SeatedDropZ = 32.0f;
		RoleSkeletalMesh->SetRelativeLocation(bSeatedNow ? (RoleMeshStandOffset - FVector(0.0f, 0.0f, SeatedDropZ)) : RoleMeshStandOffset);
	}
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
	const bool bUseNativeHunterModel = bUseHunterModel && BHLoadNativeHunterAnimInstanceClass(BHPS) != nullptr;
	bool bAppliedRoleModel = false;
	bool bAppliedSkeletalModel = false;
	const FVector RoleModelFeetOffset = BHRoleModelFeetAtCapsuleBaseOffset(GetCapsuleComponent(), AvatarRoot);
	// Remember the standing mesh offset so ApplySeatedAvatar can drop to the seat and restore on stand.
	RoleMeshStandOffset = RoleModelFeetOffset;
	bRoleMeshStandCaptured = true;

	if (RoleModelRoot)
	{
		RoleModelRoot->SetRelativeScale3D(FVector::OneVector);
	}

	if (RoleSkeletalMesh && !bUseNativeHunterModel)
	{
		if (USkeletalMesh* RoleMesh = LoadObject<USkeletalMesh>(nullptr, BHSelectQuaterniusMeshPath(BHPS)))
		{
			RoleSkeletalMesh->SetSkeletalMeshAsset(RoleMesh);
			RoleSkeletalMesh->EmptyOverrideMaterials();
			RoleSkeletalMesh->SetRelativeLocation(RoleModelFeetOffset);
			RoleSkeletalMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
			RoleSkeletalMesh->SetRelativeScale3D(FVector(1.0f));
			if (UClass* AnimClass = BHLoadMovementAnimInstanceClass(BHPS))
			{
				RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				RoleSkeletalMesh->SetAnimInstanceClass(AnimClass);
			}
			else
			{
				RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			}
			BHApplyQuaterniusPalette(RoleSkeletalMesh, BHPS ? BHPS->AvatarSlotColors : TArray<uint8>());
			LastRoleAnimationName = NAME_None;
			bAppliedRoleModel = true;
			bAppliedSkeletalModel = true;
		}
	}

	if (!bAppliedRoleModel && bUseNativeHunterModel && RoleSkeletalMesh)
	{
		if (USkeletalMesh* HunterMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/BlackoutHunt/Art/Characters/Hunter/SK_BH_Hunter.SK_BH_Hunter")))
		{
			RoleSkeletalMesh->SetSkeletalMeshAsset(HunterMesh);
			RoleSkeletalMesh->EmptyOverrideMaterials();
			RoleSkeletalMesh->SetRelativeLocation(RoleModelFeetOffset);
			RoleSkeletalMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
			RoleSkeletalMesh->SetRelativeScale3D(FVector(0.11f));
			if (UClass* AnimClass = BHLoadMovementAnimInstanceClass(BHPS))
			{
				RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				RoleSkeletalMesh->SetAnimInstanceClass(AnimClass);
			}
			else
			{
				RoleSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			}
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
	const float FearPanicAlpha = BHFearPanicAlpha(this);
	const float DreadStrainAlpha = BHDreadStrainAlpha(this);
	const float BobScale = SmoothedMoveAlpha * (bIsCrouched ? 0.44f : 1.0f);
	const float BobZ = FMath::Sin(CameraBobTime * 2.0f) * FMath::Lerp(0.8f, 1.85f, SmoothedSprintAlpha) * BobScale;
	const float BobY = FMath::Sin(CameraBobTime) * FMath::Lerp(0.45f, 1.05f, SmoothedSprintAlpha) * BobScale;
	const float StressTremor = (FMath::Sin(FlashlightPulseTime * 13.0f) + FMath::Sin(FlashlightPulseTime * 19.7f) * 0.45f)
		* (FearPanicAlpha * 0.82f + DreadStrainAlpha * 0.34f);
	const float CrouchOffset = bIsCrouched ? -14.0f : 0.0f;
	// Functional sit lowers the first-person eye height to a seated level (no body animation needed). Smoothed
	// into the camera via the interpolation below, so sitting/standing eases the view down/up.
	const float SeatedOffset = bSeated ? -22.0f : 0.0f;
	const EBHMovementSpecialState VisualSpecialState = MovementSpecialState != EBHMovementSpecialState::None ? MovementSpecialState : CosmeticMovementSpecialState;
	const float SpecialOffset = VisualSpecialState == EBHMovementSpecialState::Prone
		? BHProneCameraOffsetZ
		: (VisualSpecialState == EBHMovementSpecialState::Sliding
			? BHSlideCameraOffsetZ
			: (VisualSpecialState == EBHMovementSpecialState::Diving ? BHDiveCameraOffsetZ : 0.0f));
	const FVector TargetCameraLocation = DefaultCameraLocation
		+ FVector(0.0f, BobY - SmoothedStrafeAlpha * 1.65f, CrouchOffset + SeatedOffset + SpecialOffset + BobZ + StressTremor);

	// Interpolate the base view-feel location into a member; UpdatePOVAnimation owns the single
	// final SetRelativeLocation so the additive POV punch doesn't fight the bob interpolation.
	ViewFeelCameraLocation = FMath::VInterpTo(ViewFeelCameraLocation, TargetCameraLocation, DeltaSeconds, bHiddenInLocker ? 4.5f : 11.5f);

	const float HiddenFOVPenalty = bHiddenInLocker ? 3.5f : 0.0f;
	const float ExhaustionThreshold = MaxStamina * 0.22f;
	const float ExhaustionAlpha = ExhaustionThreshold > 0.0f ? FMath::Clamp((ExhaustionThreshold - Stamina) / ExhaustionThreshold, 0.0f, 1.0f) : 0.0f;
	const float DesiredFOV = DefaultCameraFOV + SmoothedSprintAlpha * 4.8f - HiddenFOVPenalty - HorrorAlpha * 6.8f - ExhaustionAlpha * 1.4f;
	// Track the smoothed "base" FOV in a member so the transient jumpscare punch layers on top
	// without feeding back into the interpolation each frame.
	SmoothedBaseFOV = FMath::FInterpTo(SmoothedBaseFOV, DesiredFOV, DeltaSeconds, 4.2f);
	Camera->SetFieldOfView(FMath::Clamp(SmoothedBaseFOV - ComputeJumpscareFOVPunch(), 30.0f, 140.0f));

	UpdatePOVAnimation(DeltaSeconds);
}

bool ABHCharacter::IsReducedCameraShakeLocal() const
{
	if (const ABHPlayerController* PC = Cast<ABHPlayerController>(GetController()))
	{
		return PC->IsComfortOptionEnabledForMenu(FName(TEXT("ReducedCameraShake")));
	}
	return false;
}

EBHRollCamStyle ABHCharacter::ResolveRollCamStyle() const
{
	int32 Style = static_cast<int32>(EBHRollCamStyle::Dip); // comfortable default: a forward nod, no full flip
	if (GConfig)
	{
		GConfig->GetInt(TEXT("BlackoutHunt.Comfort"), TEXT("RollCamStyle"), Style, GGameUserSettingsIni);
	}
	const int32 Override = CVarBHRollCamStyle.GetValueOnGameThread();
	if (Override >= 0)
	{
		Style = Override; // dev/console override
	}
	Style = FMath::Clamp(Style, static_cast<int32>(EBHRollCamStyle::Off), static_cast<int32>(EBHRollCamStyle::Full));
	// The reduced-camera-shake comfort toggle caps the roll camera at a gentle dip, whatever style is chosen.
	if (IsReducedCameraShakeLocal() && Style > static_cast<int32>(EBHRollCamStyle::Subtle))
	{
		Style = static_cast<int32>(EBHRollCamStyle::Subtle);
	}
	return static_cast<EBHRollCamStyle>(Style);
}

void ABHCharacter::UpdatePOVAnimation(float DeltaSeconds)
{
	if (!Camera)
	{
		return;
	}

	const FBHPOVAnimationTuning Tuning = BHResolvePOVAnimationTuning();

	FRotator TargetRot = FRotator::ZeroRotator;
	FVector TargetLoc = FVector::ZeroVector;
	// The roll's barrel spin is driven directly (see the Rolling case) and applied after the interpolation below,
	// because RInterpTo normalizes rotators and would collapse a full-revolution target to a tiny lean.
	float SpecialMoveCameraRollDeg = 0.0f;

	if (Tuning.bEnablePOVAnimation && GetWorld())
	{
		const float Now = GetWorld()->GetTimeSeconds();

		// ---- Special-move layer (local-only progress clock) ----
		const EBHMovementSpecialState Vis = MovementSpecialState != EBHMovementSpecialState::None
			? MovementSpecialState
			: CosmeticMovementSpecialState;
		if (Vis != EBHMovementSpecialState::None)
		{
			const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
			const float Duration = FMath::Max(0.01f, BHGetSpecialMoveTuning(BHPS, Vis).DurationSeconds);
			const float P = FMath::Clamp((Now - LocalSpecialAnimStartTime) / Duration, 0.0f, 1.0f);
			const float Bell = FMath::Sin(P * PI);                            // 0..1..0
			const float Ramp = FMath::Clamp(P / 0.18f, 0.0f, 1.0f);           // fast in
			const float Settle = FMath::Clamp((P - 0.7f) / 0.3f, 0.0f, 1.0f); // tail rise

			switch (Vis)
			{
			case EBHMovementSpecialState::Rolling:
			{
				// Forward roll camera, graded by the player's motion-sickness setting (ResolveRollCamStyle). It is
				// published to ABHPlayerCameraManager, which pitches the view about its local right axis (down -> up)
				// on the final POV -- it can't be done on the camera component here because bUsePawnControlRotation
				// overwrites component rotation each frame. Dip/Subtle use a bell (down then back up, no flip); Full
				// sweeps a clean 0..360 (a complete forward somersault, ending upright).
				switch (ResolveRollCamStyle())
				{
				case EBHRollCamStyle::Full:
					SpecialMoveCameraRollDeg = FMath::InterpEaseInOut(0.0f, 1.0f, P, 1.8f) * Tuning.RollSpinDegrees;
					break;
				case EBHRollCamStyle::Dip:
					SpecialMoveCameraRollDeg = FMath::Sin(P * PI) * 110.0f;
					break;
				case EBHRollCamStyle::Subtle:
					SpecialMoveCameraRollDeg = FMath::Sin(P * PI) * 32.0f;
					break;
				case EBHRollCamStyle::Off:
				default:
					SpecialMoveCameraRollDeg = 0.0f;
					break;
				}
				TargetRot.Pitch = -Bell * Tuning.RollPitchDipDeg;
				TargetLoc.Z = -Bell * Tuning.RollPosPunchCm;
				break;
			}
			case EBHMovementSpecialState::Sliding:
			{
				TargetRot.Pitch = -Ramp * (1.0f - Settle) * Tuning.SlidePitchDipDeg + Settle * Tuning.SlideSettleRiseDeg;
				TargetRot.Roll = Ramp * (1.0f - Settle) * Tuning.SlideRollLeanDeg;
				break;
			}
			case EBHMovementSpecialState::Diving:
			{
				TargetRot.Pitch = -Bell * Tuning.DivePitchDipDeg;
				TargetLoc.Z = -Bell * Tuning.DivePosPunchCm;
				break;
			}
			default:
				break;
			}
		}

		// ---- Swing / capture layer (local controlling client only) ----
		if (IsLocallyControlled() && Now < TeacherMeleeSwingEndTime && TeacherMeleeSwingEndTime > TeacherMeleeSwingStartTime)
		{
			const float S = FMath::Clamp((Now - TeacherMeleeSwingStartTime) / FMath::Max(0.01f, BHTeacherMeleeSwingDuration), 0.0f, 1.0f);
			if (S < 0.28f)
			{
				const float T = S / 0.28f;
				TargetRot.Pitch += T * Tuning.SwingWindupPitchUpDeg;
				TargetRot.Yaw += -T * Tuning.SwingWindupYawDeg;
			}
			else if (S < 0.68f)
			{
				const float T = (S - 0.28f) / 0.40f;
				TargetRot.Pitch += FMath::Lerp(Tuning.SwingWindupPitchUpDeg, -Tuning.SwingPunchPitchDownDeg, T);
				TargetRot.Yaw += FMath::Lerp(-Tuning.SwingWindupYawDeg, Tuning.SwingPunchYawDeg, T);
				TargetLoc.X += FMath::Sin(T * PI) * Tuning.SwingPosPunchCm;
			}
			else
			{
				const float T = (S - 0.68f) / 0.32f;
				TargetRot.Pitch += FMath::InterpEaseOut(-Tuning.SwingPunchPitchDownDeg, 0.0f, T, 2.0f);
				TargetRot.Yaw += FMath::Lerp(Tuning.SwingPunchYawDeg, 0.0f, T);
			}
		}
	}

	// ---- intensity level + reduced-motion accessibility scaling ----
	const float LevelScale = BHPOVIntensityScale(Tuning.Intensity);
	float Scale = LevelScale;
	if (IsReducedCameraShakeLocal())
	{
		Scale *= Tuning.ReducedMotionScale;
	}
	TargetRot *= Scale;
	TargetLoc *= Scale;

	// ---- smooth + apply (cosmetic camera-component transform only; control rotation untouched) ----
	POVAnimRotationCurrent = FMath::RInterpTo(POVAnimRotationCurrent, TargetRot, DeltaSeconds, FMath::Max(0.1f, Tuning.RotInterpSpeed));
	POVAnimLocationCurrent = FMath::VInterpTo(POVAnimLocationCurrent, TargetLoc, DeltaSeconds, FMath::Max(0.1f, Tuning.PosInterpSpeed));
	// The jumpscare flinch is a sharp, damped impulse applied directly (not through the slow
	// POV interpolation) so the recoil reads as an instant jolt rather than a smooth lean.
	// The first-person camera uses bUsePawnControlRotation, which overwrites component-relative ROTATION every frame,
	// so the barrel roll can't be applied on the camera component here -- publish it to ABHPlayerCameraManager, which
	// stamps it onto the view rotation in ProcessViewRotation. (Relative LOCATION still survives, so keep that.)
	// While actively rolling, follow the somersault curve directly. When a roll ENDS mid-flip (e.g. cut short by a
	// wall bonk), don't snap the view upright in one frame -- ease the published offset to the nearest upright
	// orientation (0 or 360) so it settles smoothly. A roll that completes naturally is already at ~0/360, so this
	// is a no-op there.
	const bool bRollingNow = (MovementSpecialState == EBHMovementSpecialState::Rolling || CosmeticMovementSpecialState == EBHMovementSpecialState::Rolling);
	if (bRollingNow)
	{
		ViewRollOffsetDeg = SpecialMoveCameraRollDeg;
	}
	else if (!FMath::IsNearlyZero(ViewRollOffsetDeg, 0.5f))
	{
		// A Full somersault is a monotonic 0..360, so finish it FORWARD to 360 (upright) even if cut short before the
		// midpoint -- easing back to 0 would visibly reverse the flip. The Dip/Subtle bells never exceed ~110, so
		// they always settle to 0 (the way they came), which matches their shape.
		const float SettleTarget = (ResolveRollCamStyle() == EBHRollCamStyle::Full) ? 360.0f : 0.0f;
		ViewRollOffsetDeg = FMath::FInterpTo(ViewRollOffsetDeg, SettleTarget, DeltaSeconds, 12.0f);
		if (FMath::Abs(ViewRollOffsetDeg - SettleTarget) < 0.5f)
		{
			ViewRollOffsetDeg = 0.0f; // 360 == 0 == upright
		}
	}
	else
	{
		ViewRollOffsetDeg = 0.0f;
	}
	Camera->SetRelativeRotation(POVAnimRotationCurrent + ComputeJumpscareCameraFlinch());
	Camera->SetRelativeLocation(ViewFeelCameraLocation + POVAnimLocationCurrent);
}

float ABHCharacter::GetJumpscareImpactEnvelope() const
{
	if (JumpscareImpactIntensity <= 0.0f || JumpscareImpactStartTime < 0.0f || !GetWorld())
	{
		return 0.0f;
	}

	// Total impact window: sharp attack then exponential-ish decay.
	constexpr float AttackSeconds = 0.045f;
	constexpr float TotalSeconds = 0.55f;
	const float Age = GetWorld()->GetTimeSeconds() - JumpscareImpactStartTime;
	if (Age < 0.0f || Age >= TotalSeconds)
	{
		return 0.0f;
	}

	float Env;
	if (Age < AttackSeconds)
	{
		Env = Age / AttackSeconds;
	}
	else
	{
		const float DecayAlpha = FMath::Clamp((Age - AttackSeconds) / FMath::Max(KINDA_SMALL_NUMBER, TotalSeconds - AttackSeconds), 0.0f, 1.0f);
		Env = FMath::Square(1.0f - DecayAlpha); // ease-out tail
	}
	return FMath::Clamp(Env * JumpscareImpactIntensity, 0.0f, 1.0f);
}

float ABHCharacter::ComputeJumpscareFOVPunch() const
{
	const float Env = GetJumpscareImpactEnvelope();
	if (Env <= 0.0f)
	{
		return 0.0f;
	}
	// FOV punch is a zoom-in (positive return is subtracted from the base FOV by the caller).
	return JumpscareImpactFOVPunch * Env;
}

FRotator ABHCharacter::ComputeJumpscareCameraFlinch() const
{
	const float Env = GetJumpscareImpactEnvelope();
	if (Env <= 0.0f || !GetWorld())
	{
		return FRotator::ZeroRotator;
	}

	float Scale = Env;
	if (IsReducedCameraShakeLocal())
	{
		Scale *= 0.35f;
	}

	// Damped high-frequency wobble on pitch + roll for a violent recoil that settles quickly.
	const float Age = GetWorld()->GetTimeSeconds() - JumpscareImpactStartTime;
	const float Wobble = FMath::Sin(Age * 58.0f);
	const float PitchKick = -6.5f * Scale - 3.0f * Scale * Wobble;
	const float RollKick = 4.0f * Scale * FMath::Sin(Age * 47.0f + 1.3f);
	return FRotator(PitchKick, 0.0f, RollKick);
}

void ABHCharacter::PlayJumpscareCameraImpact(float Intensity, float FOVPunchDegrees)
{
	if (!IsLocallyControlled() || !GetWorld())
	{
		return;
	}

	JumpscareImpactIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	JumpscareImpactStartTime = GetWorld()->GetTimeSeconds();
	JumpscareImpactFOVPunch = FOVPunchDegrees >= 0.0f ? FOVPunchDegrees : 14.0f;
	if (IsReducedCameraShakeLocal())
	{
		JumpscareImpactFOVPunch *= 0.4f;
	}
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
	const float FearPanicAlpha = BHFearPanicAlpha(this);
	const float DreadStrainAlpha = BHDreadStrainAlpha(this);
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const EBHFogPreset ActiveFogPreset = BHGS ? BHGS->ActiveFogPreset : EBHFogPreset::Heavy;
	const bool bFoggroundsActive = BHGS && BHGS->ActiveLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	const bool bExtremeFog = bFoggroundsActive && ActiveFogPreset == EBHFogPreset::Extreme;
	const bool bHeavyFog = bFoggroundsActive && ActiveFogPreset == EBHFogPreset::Heavy;
	// On the intermission train the flashlight has no real use and a full-strength beam blows out the
	// carriage's own lighting/mood, so dim it hard (intensity, range, and the volumetric beam) while
	// still letting it toggle. Battery drain is suppressed during the intermission (see Tick) so it is
	// not wasted here. Detection uses the replicated GameState (GameMode::bTrainIntermissionLevel is
	// server-only and unreadable on clients).
	const bool bTrainIntermission = BHGS && (BHGS->RoundPhase == EBHRoundPhase::Intermission || BHGS->ActiveLevelName.Equals(TEXT("TrainIntermission"), ESearchCase::IgnoreCase));
	const float TrainFlashlightIntensityScale = bTrainIntermission ? 0.18f : 1.0f;
	const float TrainFlashlightRadiusScale = bTrainIntermission ? 0.55f : 1.0f;

	const EBHMovementSpecialState FlashlightVisualSpecialState = MovementSpecialState != EBHMovementSpecialState::None ? MovementSpecialState : CosmeticMovementSpecialState;
	const float FlashlightSpecialOffset = FlashlightVisualSpecialState == EBHMovementSpecialState::Prone
		? BHProneCameraOffsetZ
		: (FlashlightVisualSpecialState == EBHMovementSpecialState::Sliding
			? BHSlideCameraOffsetZ
			: (FlashlightVisualSpecialState == EBHMovementSpecialState::Diving ? BHDiveCameraOffsetZ : 0.0f));
	const FVector StableEyeLocation = GetActorLocation() + FVector(0.0f, 0.0f, DefaultCameraLocation.Z + (bIsCrouched ? -14.0f : 0.0f) + FlashlightSpecialOffset);
	const FRotator StableViewRotation = Controller ? Controller->GetControlRotation() : (Camera ? Camera->GetComponentRotation() : GetActorRotation());
	Flashlight->SetWorldLocationAndRotation(StableEyeLocation, StableViewRotation);

	const float UnevenPulse = 1.0f
		+ FMath::Sin(FlashlightPulseTime * FMath::Lerp(5.8f, 13.0f, LowBatteryAlpha)) * (0.018f + LowBatteryAlpha * 0.045f)
		+ FMath::Sin(FlashlightPulseTime * 23.0f) * (HorrorAlpha * 0.018f + FearPanicAlpha * 0.040f + DreadStrainAlpha * 0.026f);
	const float Cutout = 1.0f
		- LowBatteryAlpha * (0.09f + 0.12f * FMath::Square(FMath::Max(0.0f, FMath::Sin(FlashlightPulseTime * 37.0f))))
		- FearPanicAlpha * 0.075f * FMath::Square(FMath::Max(0.0f, FMath::Sin(FlashlightPulseTime * 29.0f)))
		- DreadStrainAlpha * 0.045f * FMath::Square(FMath::Max(0.0f, FMath::Sin(FlashlightPulseTime * 17.0f)));
	const float NormalIntensity = FMath::Clamp(10000.0f * FMath::Lerp(0.56f, 1.0f, BatteryAlpha) * UnevenPulse * Cutout, 2200.0f, 13200.0f);
	const float NormalRadius = FMath::Lerp(1350.0f, 2400.0f, BatteryAlpha)
		* FMath::Lerp(1.0f, 0.94f, HorrorAlpha)
		* FMath::Lerp(1.0f, 0.88f, DreadStrainAlpha)
		* FMath::Lerp(1.0f, 0.95f, FearPanicAlpha);

	const float LightBoostMultiplier = PowerupComponent ? PowerupComponent->GetFlashlightMultiplier() : 1.0f;
	// A student caught in a Teacher blackout: the beam flickers down to a near-dead level (brightness) while
	// its reach collapses to a short, steady stub. Everyone else keeps a scale of 1.0.
	const bool bTeacherBlackout = IsInTeacherBlackout();
	const float BlackoutBeamScale = bTeacherBlackout ? ComputeBlackoutFlashlightFlicker() : 1.0f;
	const float BlackoutRadiusScale = bTeacherBlackout ? 0.55f : 1.0f;
	const float EffectiveIntensity = (bExtremeFog ? FMath::Min(NormalIntensity, 10000.0f) : (bHeavyFog ? FMath::Min(NormalIntensity, 10800.0f) : NormalIntensity)) * LightBoostMultiplier * FlashlightTuningIntensityScale * TrainFlashlightIntensityScale * BlackoutBeamScale;
	const float EffectiveRadius = (bExtremeFog ? FMath::Min(NormalRadius, 2200.0f) : (bHeavyFog ? FMath::Min(NormalRadius, 2400.0f) : NormalRadius)) * FMath::Lerp(1.0f, 1.16f, LightBoostMultiplier - 1.0f) * FlashlightTuningRadiusScale * TrainFlashlightRadiusScale * BlackoutRadiusScale;
	const float EffectiveInnerConeAngle = (bExtremeFog ? 9.0f : (bHeavyFog ? 10.0f : 18.0f)) * FlashlightTuningConeScale;
	const float EffectiveOuterConeAngle = (bExtremeFog ? 18.0f : (bHeavyFog ? 20.0f : (34.0f + LowBatteryAlpha * 2.5f + HorrorAlpha * 1.8f))) * FlashlightTuningConeScale;
	const float RayVisibilityScale = FMath::Clamp(FlashlightTuningRayVisibilityScale, 0.0f, 3.0f);
	const float RayWidthScale = FMath::Clamp(FlashlightTuningRayWidthScale, 0.25f, 3.0f);
	const float RayBoostAlpha = FMath::Clamp(RayVisibilityScale / 1.75f, 0.0f, 1.0f);
	const float RayVisibilityBoost = RayVisibilityScale <= KINDA_SMALL_NUMBER
		? 0.0f
		: FMath::Lerp(0.42f, 1.85f, RayBoostAlpha);
	const float VolumetricRayBoost = RayVisibilityScale <= KINDA_SMALL_NUMBER
		? 0.0f
		: FMath::Lerp(0.55f, 1.22f, RayBoostAlpha);

	Flashlight->SetIntensity(EffectiveIntensity);
	Flashlight->SetAttenuationRadius(EffectiveRadius);
	Flashlight->SetInnerConeAngle(FMath::Clamp(EffectiveInnerConeAngle, 4.0f, 80.0f));
	Flashlight->SetOuterConeAngle(FMath::Clamp(EffectiveOuterConeAngle, 6.0f, 88.0f));
	Flashlight->SetVolumetricScatteringIntensity((bExtremeFog ? 5.6f : (bHeavyFog ? 6.0f : 6.6f)) * FlashlightTuningVolumetricScale * VolumetricRayBoost * TrainFlashlightIntensityScale * BlackoutBeamScale);

	const float FogScatterAlpha =
		ActiveFogPreset == EBHFogPreset::Extreme ? 0.80f :
		ActiveFogPreset == EBHFogPreset::Heavy ? 0.72f :
		0.42f;
	const float BeamLength = (bExtremeFog ? 1550.0f : (bHeavyFog ? 1850.0f : FMath::Clamp(EffectiveRadius * 0.70f, 680.0f, 1260.0f))) * FlashlightTuningBeamLengthScale;
	const float BeamPulse = FMath::Clamp(UnevenPulse * Cutout, 0.55f, 1.28f);
	const FLinearColor BeamTint(0.76f, 0.86f, 0.80f, 1.0f);
	const float BeamStartOffset = FMath::Clamp(FlashlightTuningRayStartOffset, 0.0f, 220.0f);

	const auto ConfigureBeam = [BeamTint, BeamStartOffset, RayVisibilityScale, RayWidthScale, RayBoostAlpha](UMeshComponent* Beam, UMaterialInstanceDynamic* Material, float Length, float ConeAngle, float RadiusScale, float Opacity, float Brightness)
	{
		if (!Beam)
		{
			return;
		}

		const float BaseRadius = FMath::Max(8.0f, FMath::Tan(FMath::DegreesToRadians(ConeAngle)) * Length * RadiusScale);
		Beam->SetRelativeLocation(FVector(Length * 0.5f + BeamStartOffset, 0.0f, 0.0f));
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
			Material->SetScalarParameterValue(TEXT("Falloff"), FMath::Lerp(4.6f, 2.3f, RayBoostAlpha));
			Material->SetScalarParameterValue(TEXT("Softness"), FMath::Lerp(0.62f, 0.90f, RayBoostAlpha));
			Material->SetScalarParameterValue(TEXT("RayVisibility"), RayVisibilityScale);
			Material->SetScalarParameterValue(TEXT("RayWidth"), RayWidthScale);
			Material->SetScalarParameterValue(TEXT("BeamLength"), Length);
			Material->SetScalarParameterValue(TEXT("BeamRadius"), BaseRadius);
			Material->SetScalarParameterValue(TEXT("NearFadeDistance"), BeamStartOffset);
		}
	};

	const float OuterOpacity = FMath::Clamp((0.014f + FogScatterAlpha * 0.028f + LowBatteryAlpha * 0.004f) * BeamPulse * FlashlightTuningBeamOpacityScale * RayVisibilityBoost * TrainFlashlightIntensityScale * BlackoutBeamScale, 0.0f, bExtremeFog ? 0.145f : (bHeavyFog ? 0.158f : 0.180f));
	const float CoreOpacity = FMath::Clamp((0.010f + FogScatterAlpha * 0.020f) * BeamPulse * FlashlightTuningBeamOpacityScale * RayVisibilityBoost * TrainFlashlightIntensityScale * BlackoutBeamScale, 0.0f, bExtremeFog ? 0.110f : (bHeavyFog ? 0.120f : 0.140f));
	ConfigureBeam(FlashlightBeamOuter, FlashlightBeamOuterMaterial, BeamLength, EffectiveOuterConeAngle, 0.92f * RayWidthScale, OuterOpacity, (bExtremeFog ? 0.86f : (bHeavyFog ? 0.92f : 1.00f)) * FlashlightTuningBeamBrightnessScale * FMath::Lerp(0.65f, 1.20f, RayBoostAlpha));
	ConfigureBeam(FlashlightBeamCore, FlashlightBeamCoreMaterial, BeamLength * 0.82f, EffectiveInnerConeAngle, 0.48f * RayWidthScale, CoreOpacity, (bExtremeFog ? 1.05f : (bHeavyFog ? 1.12f : 1.20f)) * FlashlightTuningBeamBrightnessScale * FMath::Lerp(0.70f, 1.18f, RayBoostAlpha));
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
		if (HasAuthority())
		{
			MovementSpecialState = EBHMovementSpecialState::None;
			SpecialMoveEndTime = -999.0f;
			bSpecialMoveEndsProne = false;
			bSpecialMoveEndProneRequiresInput = false;
		}
		ApplyMovementSpecialState();
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
		else
		{
			// A non-alive pawn that didn't take the bOutOfPlay path above (a freshly spawned late-join
			// spectator, or a reconnected captured student) has collision disabled here; without also
			// stopping movement the CharacterMovementComponent stays in MOVE_Walking, finds no floor, and
			// falls through the world — dragging the spectator's attached camera into the void.
			GetCharacterMovement()->StopMovementImmediately();
			GetCharacterMovement()->DisableMovement();
		}
	}

	UpdateHunterVisualCue();
}

void ABHCharacter::UpdateHunterVisualCue()
{
	if (!HunterHueLight)
	{
		return;
	}

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const bool bHunterHueVisible = !bHiddenInLocker
		&& !bOutOfPlay
		&& BHPS
		&& BHPS->PlayerRole == EBHPlayerRole::Hunter
		&& BHPS->LifeState == EBHPlayerLifeState::Alive;
	HunterHueLight->SetVisibility(bHunterHueVisible);
	HunterHueLight->SetIntensity(bHunterHueVisible ? BHHunterHueLightIntensity : 0.0f);
}

void ABHCharacter::ApplyTeacherWeaponVisuals(const ABHPlayerState* BHPS)
{
	const bool bShowWeapon = BHPS
		&& BHPS->PlayerRole == EBHPlayerRole::Hunter
		&& BHPS->LifeState == EBHPlayerLifeState::Alive;
	UStaticMesh* ImportedWeaponMesh = BHTeacherWeaponImportedMesh();
	const bool bUseImportedWeapon = bShowWeapon && ImportedWeaponMesh != nullptr;
	const bool bShowFallbackWeapon = bShowWeapon && !bUseImportedWeapon;

	if (TeacherWeaponRoot)
	{
		TeacherWeaponRoot->SetRelativeLocation(BHTeacherWeaponIdleLocation);
		TeacherWeaponRoot->SetRelativeRotation(BHTeacherWeaponIdleRotation);
		TeacherWeaponRoot->SetRelativeScale3D(FVector::OneVector);
	}

	if (TeacherWeaponMesh)
	{
		if (ImportedWeaponMesh)
		{
			TeacherWeaponMesh->SetStaticMesh(ImportedWeaponMesh);
			TeacherWeaponMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 14.0f));
			TeacherWeaponMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 90.0f));
			TeacherWeaponMesh->SetRelativeScale3D(FVector(0.82f));
		}
		BHPropVisuals::SetPartVisible(TeacherWeaponMesh, bUseImportedWeapon);
	}

	UMaterialInterface* WoodMaterial = BHQuaterniusMaterial(TEXT("DarkBrown")) ? BHQuaterniusMaterial(TEXT("DarkBrown")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* GripMaterial = BHQuaterniusMaterial(TEXT("Black")) ? BHQuaterniusMaterial(TEXT("Black")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* HeadMaterial = BHQuaterniusMaterial(TEXT("Metal_Dark")) ? BHQuaterniusMaterial(TEXT("Metal_Dark")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* BladeMaterial = BHQuaterniusMaterial(TEXT("Metal")) ? BHQuaterniusMaterial(TEXT("Metal")) : HeadMaterial;
	UStaticMesh* Cube = BHPropVisuals::CubeMesh();
	UStaticMesh* Cylinder = BHPropVisuals::CylinderMesh();

	BHSetAccessoryPiece(TeacherWeaponHandleMesh, Cylinder, WoodMaterial, FVector(0.0f, 0.0f, 12.0f), FRotator::ZeroRotator, FVector(0.045f, 0.045f, 0.58f), bShowFallbackWeapon);
	BHSetAccessoryPiece(TeacherWeaponGripMesh, Cylinder, GripMaterial, FVector(0.0f, 0.0f, -25.0f), FRotator::ZeroRotator, FVector(0.058f, 0.058f, 0.18f), bShowFallbackWeapon);
	BHSetAccessoryPiece(TeacherWeaponHeadMesh, Cube, HeadMaterial, FVector(0.0f, 0.0f, 50.0f), FRotator::ZeroRotator, FVector(0.11f, 0.045f, 0.08f), bShowFallbackWeapon);
	BHSetAccessoryPiece(TeacherWeaponBladeMesh, Cube, BladeMaterial, FVector(0.0f, -9.0f, 49.0f), FRotator(0.0f, 0.0f, -8.0f), FVector(0.045f, 0.17f, 0.135f), bShowFallbackWeapon);

	if (TeacherWeaponTrailMesh)
	{
		UStaticMesh* TrailMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (!TrailMesh)
		{
			TrailMesh = Cube;
		}
		UMaterialInterface* TrailMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineVolumetrics/LightBeam/Materials/M_EV_Lightbeam_Master_01_Inst.M_EV_Lightbeam_Master_01_Inst"));
		if (!TrailMaterial)
		{
			TrailMaterial = BHQuaterniusMaterial(TEXT("LightBlue")) ? BHQuaterniusMaterial(TEXT("LightBlue")) : BHPropVisuals::BasicMaterial();
		}

		if (TrailMesh)
		{
			TeacherWeaponTrailMesh->SetStaticMesh(TrailMesh);
		}
		if (!TeacherWeaponTrailMaterialInstance)
		{
			TeacherWeaponTrailMesh->SetMaterial(0, TrailMaterial);
			TeacherWeaponTrailMaterialInstance = TeacherWeaponTrailMesh->CreateAndSetMaterialInstanceDynamic(0);
		}
		TeacherWeaponTrailMesh->SetRelativeLocation(FVector(0.0f, -12.0f, 44.0f));
		TeacherWeaponTrailMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -12.0f));
		TeacherWeaponTrailMesh->SetRelativeScale3D(FVector(0.10f, 0.10f, 1.0f));
		BHPropVisuals::SetPartVisible(TeacherWeaponTrailMesh, false);
	}
}

void ABHCharacter::PlayTeacherMeleeSwingLocal(bool bConfirmedHit)
{
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	// Use IsAliveHunter() (which includes Tester) so the axe swing animates for every role that can actually
	// capture — the capture gameplay in TryCaptureAuthority gates on IsAliveHunter(), so a swinging Tester
	// previously saw no animation/trail/flash for a swing the server accepted.
	if (!BHPS || !BHPS->IsAliveHunter())
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (!bConfirmedHit && Now < TeacherMeleeSwingEndTime && Now - TeacherMeleeSwingStartTime < BHTeacherMeleeSwingMinRestartSeconds)
	{
		return;
	}
	if (bConfirmedHit && Now < TeacherMeleeSwingEndTime)
	{
		bTeacherMeleeSwingHit = true;
		TeacherMeleeHitFlashEndTime = Now + BHTeacherMeleeHitFlashDuration;
		UpdateTeacherWeaponSwingVisuals();
		return;
	}

	TeacherMeleeSwingStartTime = Now;
	TeacherMeleeSwingEndTime = Now + BHTeacherMeleeSwingDuration;
	bTeacherMeleeSwingHit = bConfirmedHit;
	TeacherMeleeHitFlashEndTime = bConfirmedHit ? Now + BHTeacherMeleeHitFlashDuration : -999.0f;
	LastRoleAnimationName = NAME_None;
	UpdateTeacherWeaponSwingVisuals();
}

void ABHCharacter::UpdateTeacherWeaponSwingVisuals()
{
	if (!TeacherWeaponRoot)
	{
		return;
	}

	const auto HideWeaponTrail = [this]()
	{
		if (TeacherWeaponTrailMesh)
		{
			BHPropVisuals::SetPartVisible(TeacherWeaponTrailMesh, false);
		}
		if (HunterHueLight && HunterHueLight->IsVisible())
		{
			HunterHueLight->SetIntensity(BHHunterHueLightIntensity);
		}
	};

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	// IsAliveHunter() includes Tester, matching the capture/swing gameplay predicate (see PlayTeacherMeleeSwingLocal).
	const bool bShowWeapon = BHPS && BHPS->IsAliveHunter();
	if (!bShowWeapon)
	{
		TeacherWeaponRoot->SetRelativeLocation(BHTeacherWeaponIdleLocation);
		TeacherWeaponRoot->SetRelativeRotation(BHTeacherWeaponIdleRotation);
		HideWeaponTrail();
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now >= TeacherMeleeSwingEndTime || TeacherMeleeSwingEndTime <= TeacherMeleeSwingStartTime)
	{
		TeacherWeaponRoot->SetRelativeLocation(BHTeacherWeaponIdleLocation);
		TeacherWeaponRoot->SetRelativeRotation(BHTeacherWeaponIdleRotation);
		bTeacherMeleeSwingHit = false;
		HideWeaponTrail();
		return;
	}

	const auto LerpRotator = [](const FRotator& A, const FRotator& B, float Alpha)
	{
		return FRotator(
			FMath::Lerp(A.Pitch, B.Pitch, Alpha),
			FMath::Lerp(A.Yaw, B.Yaw, Alpha),
			FMath::Lerp(A.Roll, B.Roll, Alpha));
	};

	const float SwingAlpha = FMath::Clamp((Now - TeacherMeleeSwingStartTime) / FMath::Max(0.01f, BHTeacherMeleeSwingDuration), 0.0f, 1.0f);
	const float HitFlashAlpha = bTeacherMeleeSwingHit
		? FMath::Clamp((TeacherMeleeHitFlashEndTime - Now) / FMath::Max(0.01f, BHTeacherMeleeHitFlashDuration), 0.0f, 1.0f)
		: 0.0f;
	const FVector WindupLocation = BHTeacherWeaponIdleLocation + FVector(-7.0f, 6.0f, 8.0f);
	const FVector FollowThroughLocation = BHTeacherWeaponIdleLocation + FVector(16.0f + HitFlashAlpha * 4.0f, -20.0f, -7.0f - HitFlashAlpha * 2.0f);
	const FRotator WindupRotation(-36.0f, 18.0f, -68.0f);
	const FRotator FollowThroughRotation(44.0f + HitFlashAlpha * 5.0f, -20.0f, 48.0f + HitFlashAlpha * 8.0f);

	FVector TargetLocation = BHTeacherWeaponIdleLocation;
	FRotator TargetRotation = BHTeacherWeaponIdleRotation;
	if (SwingAlpha < 0.28f)
	{
		const float T = FMath::InterpEaseInOut(0.0f, 1.0f, SwingAlpha / 0.28f, 2.0f);
		TargetLocation = FMath::Lerp(BHTeacherWeaponIdleLocation, WindupLocation, T);
		TargetRotation = LerpRotator(BHTeacherWeaponIdleRotation, WindupRotation, T);
	}
	else if (SwingAlpha < 0.68f)
	{
		const float T = FMath::InterpEaseIn(0.0f, 1.0f, (SwingAlpha - 0.28f) / 0.40f, 2.6f);
		TargetLocation = FMath::Lerp(WindupLocation, FollowThroughLocation, T);
		TargetRotation = LerpRotator(WindupRotation, FollowThroughRotation, T);
	}
	else
	{
		const float T = FMath::InterpEaseOut(0.0f, 1.0f, (SwingAlpha - 0.68f) / 0.32f, 2.0f);
		TargetLocation = FMath::Lerp(FollowThroughLocation, BHTeacherWeaponIdleLocation, T);
		TargetRotation = LerpRotator(FollowThroughRotation, BHTeacherWeaponIdleRotation, T);
	}

	TeacherWeaponRoot->SetRelativeLocation(TargetLocation);
	TeacherWeaponRoot->SetRelativeRotation(TargetRotation);

	const float TrailAlpha = FMath::Clamp(FMath::Sin(SwingAlpha * PI) * FMath::GetMappedRangeValueClamped(FVector2D(0.18f, 0.38f), FVector2D(0.35f, 1.0f), SwingAlpha), 0.0f, 1.0f);
	if (TeacherWeaponTrailMesh && TrailAlpha > 0.035f)
	{
		const float TrailLength = FMath::Lerp(0.12f, bTeacherMeleeSwingHit ? 0.54f : 0.42f, TrailAlpha);
		const float TrailWidth = FMath::Lerp(0.045f, 0.125f, TrailAlpha);
		TeacherWeaponTrailMesh->SetRelativeLocation(FVector(0.0f, -12.0f - TrailAlpha * 6.0f, 44.0f + HitFlashAlpha * 2.0f));
		TeacherWeaponTrailMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -12.0f - TrailAlpha * 18.0f));
		TeacherWeaponTrailMesh->SetRelativeScale3D(FVector(TrailLength, TrailWidth, 1.0f));
		BHPropVisuals::SetPartVisible(TeacherWeaponTrailMesh, true);

		if (TeacherWeaponTrailMaterialInstance)
		{
			const FLinearColor BaseTrailColor = bTeacherMeleeSwingHit
				? FLinearColor(1.0f, 0.62f, 0.22f, FMath::Clamp(0.10f + TrailAlpha * 0.20f, 0.0f, 0.36f))
				: FLinearColor(0.68f, 0.90f, 1.0f, FMath::Clamp(0.07f + TrailAlpha * 0.14f, 0.0f, 0.26f));
			const float TrailBrightness = bTeacherMeleeSwingHit ? 1.75f + HitFlashAlpha * 1.15f : 1.05f;
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("Color"), BaseTrailColor);
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), BaseTrailColor);
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("Tint"), BaseTrailColor);
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("TintColor"), BaseTrailColor);
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("FogColor"), BaseTrailColor);
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("BeamColor"), BaseTrailColor);
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("LightColor"), BaseTrailColor);
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), BaseTrailColor * TrailBrightness);
			TeacherWeaponTrailMaterialInstance->SetVectorParameterValue(TEXT("Emissive"), BaseTrailColor * TrailBrightness);
			TeacherWeaponTrailMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), BaseTrailColor.A);
			TeacherWeaponTrailMaterialInstance->SetScalarParameterValue(TEXT("Alpha"), BaseTrailColor.A);
			TeacherWeaponTrailMaterialInstance->SetScalarParameterValue(TEXT("Density"), BaseTrailColor.A * 0.75f);
			TeacherWeaponTrailMaterialInstance->SetScalarParameterValue(TEXT("Brightness"), TrailBrightness);
			TeacherWeaponTrailMaterialInstance->SetScalarParameterValue(TEXT("Intensity"), TrailBrightness);
			TeacherWeaponTrailMaterialInstance->SetScalarParameterValue(TEXT("Softness"), 0.82f);
		}
	}
	else
	{
		HideWeaponTrail();
	}

	if (HunterHueLight && HunterHueLight->IsVisible())
	{
		HunterHueLight->SetIntensity(BHHunterHueLightIntensity * (1.0f + TrailAlpha * 0.32f + HitFlashAlpha * 0.48f));
	}
}

void ABHCharacter::ApplyAvatarStyle()
{
	if (!AvatarRoot || !BodyMesh)
	{
		return;
	}

	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	UpdateHunterVisualCue();
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

	const int32 HeadwearIndex = BHPS ? BHCosmeticClampIndex(EBHCosmeticCategory::Headwear, BHPS->AvatarHeadwearIndex) : 0;
	const int32 GearIndex = 0;
	const bool bShowRoleAccessories = bUsingRoleModel;
	FVector RoleAccessoryBaseOffset = FVector::ZeroVector;
	if (bShowRoleAccessories && RoleModelRoot)
	{
		USceneComponent* VisibleRoleMesh = nullptr;
		if (RoleSkeletalMesh && RoleSkeletalMesh->IsVisible())
		{
			VisibleRoleMesh = RoleSkeletalMesh;
		}
		else if (RoleStaticMesh && RoleStaticMesh->IsVisible())
		{
			VisibleRoleMesh = RoleStaticMesh;
		}

		if (VisibleRoleMesh)
		{
			RoleAccessoryBaseOffset = VisibleRoleMesh->GetRelativeLocation();
			// The preview offsets (Z ~67..85) assume a small preview head, but the in-game role mesh varies a
			// lot in size/scale (survivor 1.0, hunter 0.11, hider static), so a fixed Z left the hat down at the
			// torso -- inside the body. Anchor the hat to the TOP of the mesh's runtime bounds (~the head)
			// instead, keeping the preview offsets' relative shape. The preview crown/skull-top is ~78 (the cap
			// crown), so seat that at the mesh's head top; brims/glasses cluster below it and a beanie bobble (~85)
			// pokes just above -- all on the head rather than inside the body.
			const FBoxSphereBounds RoleBounds = VisibleRoleMesh->CalcBounds(VisibleRoleMesh->GetComponentTransform());
			if (RoleBounds.BoxExtent.Z > KINDA_SMALL_NUMBER)
			{
				const float HeadTopWorldZ = RoleBounds.Origin.Z + RoleBounds.BoxExtent.Z;
				const float HeadTopRelZ = HeadTopWorldZ - RoleModelRoot->GetComponentLocation().Z;
				RoleAccessoryBaseOffset.Z = HeadTopRelZ - 78.0f;
			}
		}
	}
	const auto RoleAccessoryLocation = [&RoleAccessoryBaseOffset](const FVector& PreviewLocation)
	{
		return RoleAccessoryBaseOffset + PreviewLocation;
	};
	UStaticMesh* AccessoryCube = BHPropVisuals::CubeMesh();
	UStaticMesh* AccessoryCylinder = BHPropVisuals::CylinderMesh();
	UStaticMesh* AccessorySphere = BHPropVisuals::SphereMesh();
	UMaterialInterface* AccessoryBlack = BHQuaterniusMaterial(TEXT("Black")) ? BHQuaterniusMaterial(TEXT("Black")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* AccessoryBrown = BHQuaterniusMaterial(TEXT("Brown2")) ? BHQuaterniusMaterial(TEXT("Brown2")) : BHPropVisuals::BasicMaterial();
	UMaterialInterface* AccessoryDarkBrown = BHQuaterniusMaterial(TEXT("DarkBrown")) ? BHQuaterniusMaterial(TEXT("DarkBrown")) : AccessoryBrown;
	UMaterialInterface* AccessoryMetal = BHQuaterniusMaterial(TEXT("Metal")) ? BHQuaterniusMaterial(TEXT("Metal")) : BHPropVisuals::BasicMaterial();
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
		// Re-home to RoleModelRoot before re-positioning, in case the previous update parented this piece to the
		// head bone -- so the offsets below stay root-relative. The head-bone re-attach happens after positioning.
		if (Part && RoleModelRoot && Part->GetAttachParent() != RoleModelRoot)
		{
			Part->AttachToComponent(RoleModelRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
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
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCylinder, AccessoryColor, RoleAccessoryLocation(FVector(7.0f, 0.0f, 74.0f)), FRotator::ZeroRotator, FVector(0.30f, 0.30f, 0.075f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryColor, RoleAccessoryLocation(FVector(23.0f, 0.0f, 68.0f)), FRotator::ZeroRotator, FVector(0.12f, 0.28f, 0.025f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCube, AccessoryWhite, RoleAccessoryLocation(FVector(6.0f, 0.0f, 78.0f)), FRotator::ZeroRotator, FVector(0.055f, 0.055f, 0.018f), true);
		}
		else if (HeadwearIndex == 2)
		{
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCube, AccessoryBlack, RoleAccessoryLocation(FVector(23.0f, -8.0f, 72.0f)), FRotator::ZeroRotator, FVector(0.025f, 0.070f, 0.035f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryBlack, RoleAccessoryLocation(FVector(23.0f, 8.0f, 72.0f)), FRotator::ZeroRotator, FVector(0.025f, 0.070f, 0.035f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCube, AccessoryMetal, RoleAccessoryLocation(FVector(24.0f, 0.0f, 72.0f)), FRotator::ZeroRotator, FVector(0.018f, 0.050f, 0.014f), true);
		}
		else if (HeadwearIndex == 3)
		{
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCylinder, AccessoryColor, RoleAccessoryLocation(FVector(6.0f, 0.0f, 77.0f)), FRotator::ZeroRotator, FVector(0.32f, 0.32f, 0.095f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryDarkBrown, RoleAccessoryLocation(FVector(13.0f, 0.0f, 68.0f)), FRotator::ZeroRotator, FVector(0.050f, 0.32f, 0.035f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessorySphere, AccessoryColor, RoleAccessoryLocation(FVector(3.0f, 0.0f, 85.0f)), FRotator::ZeroRotator, FVector(0.055f, 0.055f, 0.055f), true);
		}
		else if (HeadwearIndex == 4)
		{
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCube, AccessoryBlack, RoleAccessoryLocation(FVector(11.0f, 0.0f, 77.0f)), FRotator::ZeroRotator, FVector(0.045f, 0.40f, 0.080f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryLight, RoleAccessoryLocation(FVector(23.0f, 0.0f, 74.0f)), FRotator::ZeroRotator, FVector(0.055f, 0.42f, 0.115f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCube, AccessoryMetal, RoleAccessoryLocation(FVector(24.0f, 0.0f, 67.0f)), FRotator::ZeroRotator, FVector(0.018f, 0.36f, 0.012f), true);
		}
		else if (HeadwearIndex == 5)
		{
			// Top Hat -- the Roof Rider achievement reward. A tall cylinder crown + a wide flat disc brim, with a
			// thin hatband in the player's shirt tint. Offsets use the head-anchored convention (preview-Z 78 ~= head top).
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCylinder, AccessoryBlack, RoleAccessoryLocation(FVector(6.0f, 0.0f, 90.0f)), FRotator::ZeroRotator, FVector(0.26f, 0.26f, 0.22f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCylinder, AccessoryBlack, RoleAccessoryLocation(FVector(6.0f, 0.0f, 75.0f)), FRotator::ZeroRotator, FVector(0.48f, 0.48f, 0.02f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCylinder, AccessoryColor, RoleAccessoryLocation(FVector(6.0f, 0.0f, 80.0f)), FRotator::ZeroRotator, FVector(0.29f, 0.29f, 0.035f), true);
		}
		else if (HeadwearIndex == 6)
		{
			// Crown (On a Roll) -- a circlet in the player's tint with a raised front point + a bright jewel.
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCylinder, AccessoryColor, RoleAccessoryLocation(FVector(6.0f, 0.0f, 79.0f)), FRotator::ZeroRotator, FVector(0.345f, 0.345f, 0.075f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryColor, RoleAccessoryLocation(FVector(20.0f, 0.0f, 90.0f)), FRotator::ZeroRotator, FVector(0.05f, 0.07f, 0.16f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessorySphere, AccessoryLight, RoleAccessoryLocation(FVector(21.0f, 0.0f, 83.0f)), FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.07f), true);
		}
		else if (HeadwearIndex == 7)
		{
			// Halo (Completionist) -- a thin bright disc floating above the head; the accent/detail pieces are hidden.
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCylinder, AccessoryLight, RoleAccessoryLocation(FVector(6.0f, 0.0f, 100.0f)), FRotator::ZeroRotator, FVector(0.36f, 0.36f, 0.012f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCylinder, AccessoryLight, RoleAccessoryLocation(FVector(6.0f, 0.0f, 100.0f)), FRotator::ZeroRotator, FVector(0.01f, 0.01f, 0.01f), false);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCylinder, AccessoryLight, RoleAccessoryLocation(FVector(6.0f, 0.0f, 100.0f)), FRotator::ZeroRotator, FVector(0.01f, 0.01f, 0.01f), false);
		}
		else if (HeadwearIndex == 8)
		{
			// Graduation Cap (Graduate) -- a black skull-cap + a flat square mortarboard + a tassel in the tint.
			BHSetAccessoryPiece(RoleHeadwearMesh, AccessoryCylinder, AccessoryBlack, RoleAccessoryLocation(FVector(6.0f, 0.0f, 77.0f)), FRotator::ZeroRotator, FVector(0.27f, 0.27f, 0.06f), true);
			BHSetAccessoryPiece(RoleHeadwearAccentMesh, AccessoryCube, AccessoryBlack, RoleAccessoryLocation(FVector(6.0f, 0.0f, 82.0f)), FRotator::ZeroRotator, FVector(0.42f, 0.42f, 0.02f), true);
			BHSetAccessoryPiece(RoleHeadwearDetailMesh, AccessoryCube, AccessoryColor, RoleAccessoryLocation(FVector(24.0f, 0.0f, 78.0f)), FRotator::ZeroRotator, FVector(0.02f, 0.02f, 0.10f), true);
		}
	}

	// Pixel-accurate headwear: now that the visible pieces are positioned (root-relative, at the head top),
	// re-attach them to the skeletal HEAD BONE so they follow the head through crouch / prone / animation instead
	// of floating at the last-known bounds top. KeepWorldTransform preserves the position computed above, so there
	// is no placement downside when a head bone is found; a static role mesh (the hider) has no bone, so its pieces
	// just stay on RoleModelRoot as before. Toggle with bh.HatsFollowHead. (Visual change -- worth a playtest.)
	if (bShowRoleAccessories && HeadwearIndex > 0 && CVarBHHatsFollowHead.GetValueOnGameThread() != 0
		&& RoleSkeletalMesh && RoleSkeletalMesh->IsVisible())
	{
		const FName HeadBone = BHFindHeadBone(RoleSkeletalMesh);
		if (!HeadBone.IsNone())
		{
			for (UStaticMeshComponent* Part : RoleHeadwearParts)
			{
				if (Part && Part->IsVisible())
				{
					Part->AttachToComponent(RoleSkeletalMesh, FAttachmentTransformRules::KeepWorldTransform, HeadBone);
				}
			}
		}
	}

	ApplyTeacherWeaponVisuals(BHPS);

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

EBHFootstepSurface ABHCharacter::ResolveFootstepSurface(const FHitResult* KnownGroundHit) const
{
	if (!GetWorld())
	{
		return EBHFootstepSurface::Default;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BHFootstepSurfaceResolve), false, this);
	QueryParams.bReturnPhysicalMaterial = true;

	const FVector FeetLocation = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 12.0f : 84.0f);
	TArray<FOverlapResult> SurfaceOverlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (GetWorld()->OverlapMultiByObjectType(SurfaceOverlaps, FeetLocation, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(44.0f), QueryParams))
	{
		for (const FOverlapResult& Overlap : SurfaceOverlaps)
		{
			const AActor* SurfaceActor = Overlap.GetActor();
			if (const UBHFootstepSurfaceComponent* SurfaceComponent = SurfaceActor ? SurfaceActor->FindComponentByClass<UBHFootstepSurfaceComponent>() : nullptr)
			{
				const EBHFootstepSurface Surface = SurfaceComponent->GetFootstepSurface();
				if (Surface != EBHFootstepSurface::Default)
				{
					return Surface;
				}
			}
		}
	}

	FHitResult GroundHit;
	const FHitResult* HitToUse = KnownGroundHit && KnownGroundHit->bBlockingHit ? KnownGroundHit : nullptr;
	if (!HitToUse)
	{
		const FVector TraceStart = GetActorLocation() + FVector(0.0f, 0.0f, 18.0f);
		const FVector TraceEnd = GetActorLocation() - FVector(0.0f, 0.0f, 170.0f);
		if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			HitToUse = &GroundHit;
		}
	}

	if (!HitToUse)
	{
		return EBHFootstepSurface::Default;
	}

	if (const UPhysicalMaterial* PhysMaterial = HitToUse->PhysMaterial.Get())
	{
		const EBHFootstepSurface PhysSurface = BHSurfaceFromName(PhysMaterial->GetName());
		if (PhysSurface != EBHFootstepSurface::Default)
		{
			return PhysSurface;
		}
	}

	if (const UActorComponent* HitComponent = HitToUse->GetComponent())
	{
		if (const UBHFootstepSurfaceComponent* SurfaceComponent = HitComponent->GetOwner() ? HitComponent->GetOwner()->FindComponentByClass<UBHFootstepSurfaceComponent>() : nullptr)
		{
			const EBHFootstepSurface Surface = SurfaceComponent->GetFootstepSurface();
			if (Surface != EBHFootstepSurface::Default)
			{
				return Surface;
			}
		}
	}

	if (const ABHBlockActor* Block = Cast<ABHBlockActor>(HitToUse->GetActor()))
	{
		const EBHFootstepSurface BlockSurface = UBHFootstepSurfaceComponent::SurfaceForBlockMaterial(Block->GetBlockMaterial());
		if (BlockSurface != EBHFootstepSurface::Default)
		{
			return BlockSurface;
		}
	}

	const EBHFootstepSurface TagSurface = BHSurfaceFromActorTags(HitToUse->GetActor());
	return TagSurface == EBHFootstepSurface::Default ? EBHFootstepSurface::Default : TagSurface;
}

FBHFootstepSurfaceProfile ABHCharacter::GetFootstepSurfaceProfile(EBHFootstepSurface Surface) const
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings)
	{
		for (const FBHFootstepSurfaceProfile& Profile : Settings->FootstepSurfaceProfiles)
		{
			if (Profile.Surface == Surface)
			{
				return Profile;
			}
		}
	}
	return BHDefaultFootstepProfile(Surface);
}

void ABHCharacter::BroadcastFlashlightAudioCue(bool bNewOn, bool bBatteryDied)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFlashlightAudioCueTime < 0.16f)
	{
		return;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings)
	{
		return;
	}

	FBHGameplayAudioCue Cue;
	Cue.AudioAsset = bNewOn ? Settings->FlashlightOnSound : Settings->FlashlightOffSound;
	Cue.Location = GetActorLocation() + FVector(0.0f, 0.0f, 62.0f);
	Cue.Caption = bBatteryDied ? TEXT("Flashlight battery dies.") : (bNewOn ? TEXT("Flashlight clicks on.") : TEXT("Flashlight clicks off."));
	Cue.Volume = bNewOn ? 0.46f : 0.36f;
	Cue.Pitch = bBatteryDied ? 0.84f : (bNewOn ? 1.04f : 0.93f);
	Cue.MaxAudibleDistance = 1800.0f;
	Cue.CaptionSeconds = 1.15f;
	Cue.bSpatial = true;

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		LastFlashlightAudioCueTime = Now;
		BHGM->BroadcastGameplayAudioCue(Cue);
	}
}

void ABHCharacter::BroadcastFlashlightStruggleAudioCue()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	// Erratic cadence: a short floor between clicks plus a gated stutter, so the dying light ticks
	// irregularly instead of like a metronome.
	if (Now - LastFlashlightStruggleAudioTime < 0.18f || FMath::Frac(Now * 5.7f) > 0.5f)
	{
		return;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings)
	{
		return;
	}

	FBHGameplayAudioCue Cue;
	Cue.AudioAsset = Settings->FlashlightOffSound;   // reuse the flashlight click as a dying stutter (no new asset)
	Cue.Location = GetActorLocation() + FVector(0.0f, 0.0f, 62.0f);
	Cue.Caption = TEXT("Flashlight stutters against the dark.");
	Cue.Volume = 0.3f;
	Cue.Pitch = 0.66f + 0.5f * FMath::Frac(Now * 7.3f);   // varied pitch so the stutter does not sound mechanical
	Cue.MaxAudibleDistance = 1500.0f;
	Cue.CaptionSeconds = 0.55f;
	Cue.bSpatial = true;

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		LastFlashlightStruggleAudioTime = Now;
		BHGM->BroadcastGameplayAudioCue(Cue);
	}
}

void ABHCharacter::SendTeacherProximityAudioCue(float DistanceAlpha, bool bHunterHasSight)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const float ClampedAlpha = FMath::Clamp(DistanceAlpha, 0.0f, 1.0f);
	const float CooldownSeconds = FMath::Lerp(14.0f, 7.5f, ClampedAlpha);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastTeacherProximityAudioTime < CooldownSeconds)
	{
		return;
	}

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(GetController()))
	{
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		FBHGameplayAudioCue Cue;
		Cue.AudioAsset = Settings ? Settings->TeacherProximitySound : TSoftObjectPtr<USoundBase>();
		Cue.Location = GetActorLocation();
		Cue.Caption = bHunterHasSight ? TEXT("Heartbeat spike: the Teacher sees you.") : TEXT("Heartbeat spike: the Teacher is close.");
		Cue.Volume = FMath::Lerp(0.28f, 0.72f, ClampedAlpha);
		Cue.Pitch = FMath::Lerp(0.92f, 1.18f, ClampedAlpha);
		Cue.CaptionSeconds = bHunterHasSight ? 1.8f : 1.45f;
		Cue.bSpatial = false;
		Cue.bApplyHorrorComfort = true;
		LastTeacherProximityAudioTime = Now;
		PC->ClientPlayGameplayAudioCue(Cue);
	}
}

void ABHCharacter::ResetAntiCampTrackingAuthority()
{
	AntiCampMoveBurstSeconds = 0.0f;
	AntiCampMoveBurstDistance = 0.0f;
	AntiCampLastSatisfiedTime = -999.0f;
	AntiCampLastMeaningfulMoveTime = -999.0f;
	AntiCampLastSampleLocation = GetActorLocation();
	LastAntiCampWarningTime = -999.0f;
	LastAntiCampNoiseTime = -999.0f;
}

void ABHCharacter::UpdateAntiCampPressureAuthority(float DeltaSeconds, float Speed2D, const ABHGameState* BHGS, const ABHPlayerState* BHPS)
{
	if (!HasAuthority() || !GetWorld() || !BHGS || !BHPS || !BHPS->IsAliveSurvivor() || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const float GraceSeconds = Settings ? FMath::Max(0.0f, Settings->AntiCampGraceSeconds) : 60.0f;
	if (GraceSeconds <= 0.0f)
	{
		ResetAntiCampTrackingAuthority();
		return;
	}

	const float WarningSeconds = Settings ? FMath::Clamp(Settings->AntiCampWarningSeconds, 0.0f, GraceSeconds) : 50.0f;
	const float RequiredMoveSeconds = Settings ? FMath::Max(0.1f, Settings->AntiCampRequiredMoveSeconds) : 5.0f;
	const float RequiredMoveDistance = Settings ? FMath::Max(100.0f, Settings->AntiCampRequiredMoveDistance) : 650.0f;
	const float MovementSpeedThreshold = Settings ? FMath::Max(20.0f, Settings->AntiCampMovementSpeedThreshold) : 150.0f;
	const float PressureDreadPerSecond = Settings ? FMath::Max(0.0f, Settings->AntiCampPressureDreadPerSecond) : 2.6f;
	const float PressureFearPerSecond = Settings ? FMath::Max(0.0f, Settings->AntiCampPressureFearPerSecond) : 0.55f;
	const float AlertDelaySeconds = Settings ? FMath::Max(1.0f, Settings->AntiCampAlertDelaySeconds) : 18.0f;
	const float AlertCooldownSeconds = Settings ? FMath::Max(2.0f, Settings->AntiCampAlertCooldownSeconds) : 18.0f;
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector CurrentLocation = GetActorLocation();

	if (AntiCampLastSatisfiedTime <= -900.0f)
	{
		AntiCampLastSatisfiedTime = Now;
		AntiCampLastSampleLocation = CurrentLocation;
		return;
	}

	const float SampleDistance = FVector::Dist2D(CurrentLocation, AntiCampLastSampleLocation);
	AntiCampLastSampleLocation = CurrentLocation;

	// Require real displacement as well as velocity so wall-pushing or tiny shuffles do not refresh anti-camp safety.
	const float EffectiveMovementSpeedThreshold = IsProne() ? MovementSpeedThreshold * 0.65f : MovementSpeedThreshold;
	const float EffectiveRequiredMoveDistance = IsProne() ? RequiredMoveDistance * 0.60f : RequiredMoveDistance;
	const float MinSampleDistance = FMath::Max(1.5f, EffectiveMovementSpeedThreshold * DeltaSeconds * 0.15f);
	const bool bCanCountMovement = !bHiddenInLocker && !bOutOfPlay && GetCharacterMovement() && GetCharacterMovement()->MovementMode != MOVE_None;
	const bool bMeaningfulMovement = bCanCountMovement && Speed2D >= EffectiveMovementSpeedThreshold && SampleDistance >= MinSampleDistance;
	if (bMeaningfulMovement)
	{
		AntiCampMoveBurstSeconds += DeltaSeconds;
		AntiCampMoveBurstDistance += SampleDistance;
		AntiCampLastMeaningfulMoveTime = Now;
	}
	else if (AntiCampMoveBurstSeconds > 0.0f && Now - AntiCampLastMeaningfulMoveTime > 0.35f)
	{
		AntiCampMoveBurstSeconds = 0.0f;
		AntiCampMoveBurstDistance = 0.0f;
	}

	if (AntiCampMoveBurstSeconds >= RequiredMoveSeconds && AntiCampMoveBurstDistance >= EffectiveRequiredMoveDistance)
	{
		const bool bWasPressured = Now - AntiCampLastSatisfiedTime > GraceSeconds;
		AntiCampLastSatisfiedTime = Now;
		AntiCampMoveBurstSeconds = 0.0f;
		AntiCampMoveBurstDistance = 0.0f;
		if (bWasPressured)
		{
			Dread = FMath::Max(0.0f, Dread - 4.0f);
			SendStatusMessage(TEXT("Moving steadied your nerves."));
		}
	}

	const float IdleSeconds = FMath::Max(0.0f, Now - AntiCampLastSatisfiedTime);
	if (WarningSeconds > 0.0f && IdleSeconds >= WarningSeconds && IdleSeconds < GraceSeconds && Now - LastAntiCampWarningTime > 12.0f)
	{
		LastAntiCampWarningTime = Now;
		SendStatusMessage(TEXT("Keep moving. Staying put is feeding your dread."));
	}

	if (IdleSeconds <= GraceSeconds)
	{
		return;
	}

	const float OverdueSeconds = IdleSeconds - GraceSeconds;
	const float AlertAlpha = FMath::Clamp(OverdueSeconds / AlertDelaySeconds, 0.0f, 1.0f);
	const float HiddenPressureMultiplier = bHiddenInLocker ? 0.65f : 1.0f;
	Dread = FMath::Clamp(Dread + PressureDreadPerSecond * FMath::Lerp(0.65f, 1.0f, AlertAlpha) * HiddenPressureMultiplier * DeltaSeconds, 0.0f, 100.0f);
	Fear = FMath::Clamp(Fear + PressureFearPerSecond * AlertAlpha * HiddenPressureMultiplier * DeltaSeconds, 0.0f, 100.0f);

	if (Now - LastAntiCampWarningTime > 12.0f)
	{
		LastAntiCampWarningTime = Now;
		SendStatusMessage(bHiddenInLocker ? TEXT("You have hidden too long. Move soon.") : TEXT("You have stayed in one place too long. Move soon."));
	}

	if (OverdueSeconds >= AlertDelaySeconds && Now - LastAntiCampNoiseTime >= AlertCooldownSeconds)
	{
		LastAntiCampNoiseTime = Now;
		SendStatusMessage(TEXT("Your restless breathing gave you away."));
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->NotifyLoudNoise(CurrentLocation, TEXT("restless breathing"));
			BHGM->RecordPlaytestTelemetryMarker(TEXT("anti_camp_alert"), CurrentLocation, FString::Printf(TEXT("idle=%.0fs"), IdleSeconds), BHPS);
		}
	}
}

void ABHCharacter::EmitFootstepStimulus(float Strength, const FString& Reason, EBHFootstepSurface Surface)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const EBHFootstepSurface ResolvedSurface = Surface == EBHFootstepSurface::Default ? ResolveFootstepSurface() : Surface;
	const FBHFootstepSurfaceProfile SurfaceProfile = GetFootstepSurfaceProfile(ResolvedSurface);
	const float ClampedStrength = FMath::Clamp(Strength * SurfaceProfile.NoiseMultiplier, 0.0f, 2.35f);
	const FString SurfaceReason = ResolvedSurface == EBHFootstepSurface::Default
		? Reason
		: FString::Printf(TEXT("%s on %s"), *Reason, BHFootstepSurfaceToken(ResolvedSurface));
	UAISense_Hearing::ReportNoiseEvent(
		GetWorld(),
		GetActorLocation(),
		ClampedStrength,
		this,
		SurfaceProfile.HearingRadiusBase + ClampedStrength * SurfaceProfile.HearingRadiusScale,
		FName(*SurfaceReason));
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->ReportBotStimulus(EBHBotStimulusType::Noise, GetActorLocation(), this, this, SurfaceReason, ClampedStrength);
		BHGM->ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Footstep, GetActorLocation(), this, this, ClampedStrength * SurfaceProfile.AtmosphereMultiplier, SurfaceReason);
	}

	const bool bFootstepSoundConfigured = SurfaceProfile.LocalVolume > 0.0f && !SurfaceProfile.FootstepSound.IsNull();
	const bool bFootstepEffectConfigured = !SurfaceProfile.FootstepEffect.IsNull() && SurfaceProfile.EffectScale > 0.0f;
	const bool bHasLocalSound = bFootstepSoundConfigured && BHCharacterSoftObjectPathExists(SurfaceProfile.FootstepSound.ToSoftObjectPath());
	const bool bHasLocalEffect = bFootstepEffectConfigured && BHCharacterSoftObjectPathExists(SurfaceProfile.FootstepEffect.ToSoftObjectPath());
	if (bFootstepSoundConfigured && !bHasLocalSound)
	{
		BHLogMissingOptionalCharacterAssetOnce(SurfaceProfile.FootstepSound.ToSoftObjectPath(), TEXT("footstep audio"));
	}
	if (bFootstepEffectConfigured && !bHasLocalEffect)
	{
		BHLogMissingOptionalCharacterAssetOnce(SurfaceProfile.FootstepEffect.ToSoftObjectPath(), TEXT("footstep effect"));
	}
	if (bHasLocalSound || bHasLocalEffect)
	{
		const float CueVolume = FMath::Clamp(FMath::Max(SurfaceProfile.LocalVolume, ClampedStrength * 0.28f), 0.0f, 1.5f);
		MulticastFootstepCue(ResolvedSurface, BHFootstepCueLocation(this), CueVolume);
	}
}

void ABHCharacter::MulticastFootstepCue_Implementation(EBHFootstepSurface Surface, FVector Location, float Volume)
{
	const FBHFootstepSurfaceProfile SurfaceProfile = GetFootstepSurfaceProfile(Surface);
	const float CueVolume = FMath::Clamp(Volume, 0.0f, 1.5f);
	if (SurfaceProfile.LocalVolume > 0.0f && !SurfaceProfile.FootstepSound.IsNull() && GetWorld())
	{
		FBHGameplayAudioCue Cue;
		Cue.AudioAsset = SurfaceProfile.FootstepSound;
		Cue.Location = Location;
		Cue.Volume = CueVolume;
		Cue.Pitch = FMath::RandRange(0.94f, 1.06f);
		Cue.MaxAudibleDistance = FMath::Clamp(SurfaceProfile.HearingRadiusBase + CueVolume * SurfaceProfile.HearingRadiusScale, 900.0f, 4200.0f);
		Cue.CaptionSeconds = 0.0f;
		Cue.bSpatial = true;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
			{
				PC->PlayGameplayAudioCueLocal(Cue);
			}
		}
	}

	if (SurfaceProfile.EffectScale > 0.0f && !SurfaceProfile.FootstepEffect.IsNull())
	{
		if (!BHCharacterSoftObjectPathExists(SurfaceProfile.FootstepEffect.ToSoftObjectPath()))
		{
			BHLogMissingOptionalCharacterAssetOnce(SurfaceProfile.FootstepEffect.ToSoftObjectPath(), TEXT("footstep effect"));
			return;
		}

		if (UNiagaraSystem* FootstepEffect = SurfaceProfile.FootstepEffect.LoadSynchronous())
		{
			const float CueScale = SurfaceProfile.EffectScale
				* FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 1.5f), FVector2D(0.55f, 1.35f), CueVolume);
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this,
				FootstepEffect,
				Location,
				FRotator::ZeroRotator,
				FVector(FMath::Max(0.01f, CueScale)),
				true,
				true,
				ENCPoolMethod::AutoRelease,
				true);
		}
	}
}

void ABHCharacter::MulticastTeacherMeleeSwing_Implementation(bool bConfirmedHit)
{
	PlayTeacherMeleeSwingLocal(bConfirmedHit);
}

void ABHCharacter::ServerSetFlashlight_Implementation(bool bNewOn)
{
	if (CanAct())
	{
		const bool bWasFlashlightOn = bFlashlightOn;
		const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		const bool bInfiniteFlashlight = (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester) || (BHGS && BHGS->bTestMode);
		if (bInfiniteFlashlight)
		{
			FlashlightBattery = 100.0f;
			bFlashlightEmptyTelemetryReported = false;
		}
		bFlashlightOn = bNewOn && (bInfiniteFlashlight || FlashlightBattery > 0.0f);
		ApplyFlashlightState();
		if (bWasFlashlightOn != bFlashlightOn)
		{
			BroadcastFlashlightAudioCue(bFlashlightOn);
		}
		if (bFlashlightOn)
		{
			if (ABHPlayerState* WarmupPS = GetPlayerState<ABHPlayerState>())
			{
				WarmupPS->MarkWarmupStep(EBHWarmupStep::Flashlight);
			}
		}
	}
}

void ABHCharacter::ServerSetSeated_Implementation(bool bNewSeated)
{
	SetSeatedAuthority(bNewSeated);
}

void ABHCharacter::SetSeatedAuthority(bool bNewSeated)
{
	if (!HasAuthority() || bNewSeated == bSeated)
	{
		return;
	}
	// You can only sit when otherwise free to act (not captured/frozen/in a locker); standing is always allowed.
	if (bNewSeated && !CanAct())
	{
		return;
	}

	bSeated = bNewSeated;

	// Freeze/restore movement server-side using the same idiom the rest of the class uses (DisableMovement sets
	// MovementMode = MOVE_None). The lowered eye height is cosmetic and handled per-frame in UpdateViewFeel.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (bSeated)
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		else if (Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	if (bSeated)
	{
		SendStatusMessage(TEXT("Seated. Move to stand up."));
	}
}

void ABHCharacter::CycleEmote()
{
	// Local input -> server. Cycle through the emote set; the server timestamps + replicates it.
	ServerEmote(LocalEmoteCycle);
	LocalEmoteCycle = (LocalEmoteCycle + 1) % 6;
}

void ABHCharacter::ServerEmote_Implementation(int32 InEmoteId)
{
	if (!CanAct())
	{
		return;
	}
	EmoteId = FMath::Clamp(InEmoteId, 0, 5);
	const AGameStateBase* BHGS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	EmoteStartServerTime = BHGS ? BHGS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

FString ABHCharacter::GetEmoteLabel(int32 Id)
{
	static const TCHAR* Labels[6] = { TEXT("o/ HI!"), TEXT("GG!"), TEXT("<3"), TEXT("LOL"), TEXT("CHEERS!"), TEXT("\\o/ WOO") };
	return Labels[FMath::Clamp(Id, 0, 5)];
}

bool ABHCharacter::GetActiveEmote(FString& OutLabel) const
{
	if (EmoteId < 0)
	{
		return false;
	}
	const AGameStateBase* BHGS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = BHGS ? BHGS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	if (Now - EmoteStartServerTime > 3.5f)
	{
		return false;
	}
	OutLabel = GetEmoteLabel(EmoteId);
	return true;
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

	if (IsSpecialMoveActive())
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Finish the move before interacting."));
		}
		return false;
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
	bool bResolvedFromView = false;
	if (bUseViewFallback && (!ResolvedTarget || !IsValidInteractionTarget(ResolvedTarget)))
	{
		FindInteractableFromView(ResolvedTarget, 175.0f);
		// A view-resolved target already passed FindInteractableFromView's ECC_Visibility trace, so it
		// is proven visible; only a directly client-supplied target still needs the LOS gate below.
		bResolvedFromView = (ResolvedTarget != nullptr);
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

	// Server-authoritative line-of-sight gate, applied ONLY to a directly client-supplied target (the
	// through-wall exploit path). A view-resolved target already proved visibility in
	// FindInteractableFromView, so re-gating it would false-reject legitimate interactions when a
	// teammate's body or thin prop sits between the player and the object's pivot in a crowded room.
	if (!bResolvedFromView && !HasInteractionLineOfSight(ResolvedTarget))
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Something is blocking your view of that."));
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

	CurrentServerInteractTarget = ResolvedTarget;
	IBHInteractableInterface::Execute_BeginInteract(ResolvedTarget, this);
	return true;
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

void ABHCharacter::EndStaleHeldInteractionAuthority()
{
	if (!HasAuthority() || bHiddenInLocker)
	{
		return;
	}

	// A hold interaction stays registered until the player releases E. If they keep E held and walk away,
	// IsValidInteractionTarget (the same distance gate BeginInteractAuthority used) now fails, so end the
	// interaction here — this drops the player from the station/breaker worker set and stops the task
	// completing from across the room. Distance-only on purpose: turning to read the question must not
	// cancel a legitimate hold, so we do not re-apply the line-of-sight gate every tick.
	AActor* HeldTarget = CurrentServerInteractTarget;
	if (HeldTarget && !IsValidInteractionTarget(HeldTarget))
	{
		EndInteractAuthority(HeldTarget);
	}
}

void ABHCharacter::ServerExitCurrentLocker_Implementation()
{
	ExitCurrentLockerAuthority();
}

void ABHCharacter::ExitCurrentLockerAuthority()
{
	ExitLocker(true); // voluntary exit -> roll-out-of-locker is allowed when Sprint is held
}

void ABHCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	// Every other movement RPC bails when the pawn cannot act; this one must too, or a modified
	// client can sprint while input-frozen/captured/in the final-escape cutscene (the stamina drain
	// is CanAct()-gated, so it would cost them nothing). Mirror ServerSetProne's authority guard.
	if (!CanAct())
	{
		return;
	}

	bSprintInputHeld = bNewSprinting;
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const float HunterPenaltyMultiplier = BHFinalEscapeHunterPenaltyActive(GetWorld(), this) ? 0.72f : 1.0f;
	const float TeacherCaptureMoveMultiplier = (BHPS && BHPS->IsAliveHunter() && IsTeacherCaptureAttackActive()) ? BHTeacherCaptureMoveMultiplier : 1.0f;
	const float CurrentWalkSpeed = BHRoleWalkSpeed(BHPS, WalkSpeed) * HunterPenaltyMultiplier * TeacherCaptureMoveMultiplier * BHHorrorWalkSpeedMultiplier(this, BHPS);
	const float CurrentSprintSpeed = BHRoleSprintSpeed(BHPS, SprintSpeed)
		* (PowerupComponent ? PowerupComponent->GetSprintSpeedMultiplier() : 1.0f)
		* HunterPenaltyMultiplier
		* TeacherCaptureMoveMultiplier
		* BHHorrorSprintSpeedMultiplier(this, BHPS);
	if (IsProne() || IsSpecialMoveActive())
	{
		GetCharacterMovement()->MaxWalkSpeed = IsProne() ? BHRoleProneSpeed(BHPS) * BHHorrorWalkSpeedMultiplier(this, BHPS) : CurrentWalkSpeed;
		return;
	}

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

void ABHCharacter::ServerStartSpecialMove_Implementation(EBHMovementSpecialState RequestedState, bool bEndProne, bool bEndProneRequiresInput)
{
	TryStartSpecialMoveAuthority(RequestedState, bEndProne, bEndProneRequiresInput);
}

void ABHCharacter::ServerSetProne_Implementation(bool bNewProne)
{
	SetProneAuthority(bNewProne, true);
}

void ABHCharacter::ServerSetProneInputHeld_Implementation(bool bHeld)
{
	// bProneInputHeld is otherwise only set by local input, so on the server it is permanently false
	// for every remote client — FinishSpecialMoveAuthority then never honours "hold prone to stay
	// down after a slide" for anyone but the listen-server host. Replicate the intent here and mirror
	// StopProne's local rule (releasing mid-slide cancels the settle-into-prone) so it matches.
	bProneInputHeld = bHeld;
	if (!bHeld && MovementSpecialState == EBHMovementSpecialState::Sliding && bSpecialMoveEndProneRequiresInput)
	{
		bSpecialMoveEndsProne = false;
		// Silent slide-stop: releasing Prone EARLY in the slide brakes the dash into a quiet crouch (third outcome,
		// alongside hold-to-prone and late-release-to-stand). It costs the full slide stamina but yields less
		// distance and skips the loud scrape -- a deliberate quiet-vs-distance choice. FinishSpecialMoveAuthority
		// reads bSpecialMoveEarlyBrake to settle into the crouch.
		const float Window = CVarBHSlideStopWindow.GetValueOnGameThread();
		if (Window > 0.0f && GetWorld())
		{
			const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
			const FBHMovementSpecialTuning Tuning = BHGetSpecialMoveTuning(BHPS, EBHMovementSpecialState::Sliding);
			const float Duration = FMath::Max(0.01f, Tuning.DurationSeconds);
			const float NormalizedTime = FMath::Clamp((GetWorld()->GetTimeSeconds() - SpecialMoveStartTime) / Duration, 0.0f, 1.0f);
			if (NormalizedTime <= Window)
			{
				bSpecialMoveEarlyBrake = true;
				MarkTutorialAction(TutorialActSlideStopBit); // movement-tutorial: a silent slide-stop specifically fired
				SpecialMoveEndTime = GetWorld()->GetTimeSeconds(); // cut the dash now; the Tick finisher settles to crouch
			}
		}
	}
}

void ABHCharacter::ServerSetCrouchInputHeld_Implementation(bool bHeld)
{
	// Same local-only-flag bug class as bProneInputHeld above: bCrouchInputHeld is otherwise only set by the owning
	// client's StartCrouch/StopCrouch, so on the server it is permanently false for every remote client. Landed()
	// runs ONLY on authority (HasAuthority gate) and decides the drop-roll via (bSprintInputHeld && bCrouchInputHeld);
	// bSprintInputHeld is mirrored by ServerSetSprinting but the crouch half was missing -- so the "HOLD Shift+Ctrl
	// through the landing" drop-roll never fired for anyone but the listen-server host. Mirror the intent here.
	bCrouchInputHeld = bHeld;
}

void ABHCharacter::ServerTryCapture_Implementation()
{
	TryCaptureAuthority(true);
}

void ABHCharacter::UpdateTeacherCaptureAttackAuthority(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (!HasAuthority() || !bTeacherCaptureAttackActive || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const ABHPlayerState* HunterPS = GetPlayerState<ABHPlayerState>();
	if (!HunterPS || !HunterPS->IsAliveHunter() || !CanAct())
	{
		bTeacherCaptureAttackActive = false;
		bTeacherCaptureAttackResolved = true;
		TeacherCaptureAttackEndServerTime = Now;
		RefreshMovementSpeedFromState();
		ForceNetUpdate();
		return;
	}

	if (!bTeacherCaptureAttackResolved && Now >= TeacherCaptureAttackResolveServerTime)
	{
		ResolveTeacherCaptureAttackAuthority();
	}

	if (Now >= TeacherCaptureAttackEndServerTime)
	{
		bTeacherCaptureAttackActive = false;
		RefreshMovementSpeedFromState();
		ForceNetUpdate();
	}
}

void ABHCharacter::StartTeacherCaptureRecoveryAuthority(float Now, float RecoverySeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const float ClampedRecovery = FMath::Max(0.0f, RecoverySeconds);
	TeacherCaptureNextAllowedServerTime = FMath::Max(TeacherCaptureNextAllowedServerTime, Now + ClampedRecovery);
	StaminaRecoveryLockedUntil = FMath::Max(StaminaRecoveryLockedUntil, Now + FMath::Min(0.85f, ClampedRecovery));
	RefreshMovementSpeedFromState();
	ForceNetUpdate();
}

bool ABHCharacter::InterruptTeacherCaptureAttack(const FString& Reason, float RecoverySeconds)
{
	if (!HasAuthority() || !bTeacherCaptureAttackActive || bTeacherCaptureAttackResolved || !GetWorld())
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	bTeacherCaptureAttackResolved = true;
	bTeacherCaptureAttackActive = false;
	TeacherCaptureAttackEndServerTime = Now;
	StartTeacherCaptureRecoveryAuthority(Now, RecoverySeconds > 0.0f ? RecoverySeconds : BHTeacherCaptureDoorSlamRecoverySeconds);
	SendStatusMessage(Reason.IsEmpty() ? TEXT("Swing interrupted. Recovering.") : Reason);
	return true;
}

bool ABHCharacter::HasDirectVisibilityToCharacter(const ABHCharacter* Target) const
{
	if (!GetWorld() || !Target)
	{
		return false;
	}

	FVector ViewLocation = GetActorLocation() + FVector(0.0f, 0.0f, 64.0f);
	FRotator ViewRotation = GetActorRotation();
	if (Controller)
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector TargetLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 62.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHTeacherCaptureLOS), false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TargetLocation, ECC_Visibility, Params);
}

float ABHCharacter::GetTeacherCaptureClockSeconds() const
{
	if (!GetWorld())
	{
		return 0.0f;
	}

	const ABHGameState* GameState = GetWorld()->GetGameState<ABHGameState>();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

bool ABHCharacter::IsTeacherCaptureCandidateAuthority(const ABHCharacter* Target, float& OutScore) const
{
	OutScore = TNumericLimits<float>::Max();
	if (!Target || Target == this)
	{
		return false;
	}

	const ABHPlayerState* TargetPS = Target->GetPlayerState<ABHPlayerState>();
	if (!TargetPS || !TargetPS->IsAliveSurvivor() || Target->IsHiddenInLocker())
	{
		return false;
	}

	// A Survivor properly sheltering in a crawl space is a sanctuary the Teacher cannot reach into, the same way
	// IsHiddenInLocker() above shields one inside a locker. The volume shoves the Teacher's body out to the gap's
	// mouth, but the capture reach (CaptureDistance + forgiveness) still overlaps the inside -- so without this a
	// Teacher at the mouth could tag a prone student just past it. Gated on a low-profile pose (the only states
	// CanCharacterUseCrawlSpace permits) so the volume scan is skipped for the common standing case, and a Survivor
	// merely standing at the lip earns no free pass.
	const EBHMovementSpecialState TargetSpecialState = Target->GetMovementSpecialState();
	if (TargetSpecialState == EBHMovementSpecialState::Prone
		|| TargetSpecialState == EBHMovementSpecialState::Sliding
		|| TargetSpecialState == EBHMovementSpecialState::Diving)
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<ABHCrawlSpaceVolume> It(World); It; ++It)
			{
				if (It->IsCharacterSheltering(Target))
				{
					return false;
				}
			}
		}
	}

	const FVector Delta = Target->GetActorLocation() - GetActorLocation();
	if (FMath::Abs(Delta.Z) > BHTeacherCaptureVerticalTolerance)
	{
		return false;
	}

	const FVector Delta2D(Delta.X, Delta.Y, 0.0f);
	const float DistanceSq = Delta2D.SizeSquared();
	const float EffectiveRange = CaptureDistance + BHTeacherCaptureRangeForgiveness;
	if (DistanceSq > FMath::Square(EffectiveRange))
	{
		return false;
	}

	FVector CaptureForward = GetActorForwardVector();
	if (Controller)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		CaptureForward = ViewRotation.Vector();
	}
	CaptureForward.Z = 0.0f;
	const FVector Forward2D = CaptureForward.GetSafeNormal();
	const FVector TargetDirection2D = Delta2D.GetSafeNormal();
	const float AimDot = TargetDirection2D.IsNearlyZero() ? 1.0f : FVector::DotProduct(Forward2D, TargetDirection2D);
	if (AimDot < BHTeacherCaptureArcMinDot)
	{
		return false;
	}

	const bool bVisible = (Controller && Controller->LineOfSightTo(const_cast<ABHCharacter*>(Target))) || HasDirectVisibilityToCharacter(Target);
	if (!bVisible)
	{
		return false;
	}

	const UWorld* TargetWorld = Target->GetWorld();
	const float NowT = TargetWorld ? TargetWorld->GetTimeSeconds() : 0.0f;
	if (Target->LockerExitTime > -900.0f && NowT - Target->LockerExitTime < BHLockerExitGraceSeconds)
	{
		return false;
	}

	OutScore = DistanceSq - AimDot * 25000.0f;
	return true;
}

bool ABHCharacter::IsTimedCaptureEvasionActive(float Now) const
{
	if (BHIsTransientSpecialMove(MovementSpecialState))
	{
		return true;
	}

	const bool bRecentlyCommittedEvasion = LastCaptureEvasionTime > -900.0f && Now - LastCaptureEvasionTime <= BHTeacherCaptureEvasionGraceSeconds;
	return bRecentlyCommittedEvasion && MovementSpecialState == EBHMovementSpecialState::Prone;
}

bool ABHCharacter::CanStaggerTeacherWithFlashlight(const ABHCharacter* Survivor) const
{
	if (!Survivor || !Survivor->bFlashlightOn || Survivor->FlashlightBattery < BHTeacherFlashlightStaggerBatteryCost * 0.35f || Survivor->IsHiddenInLocker() || Survivor->IsInTeacherBlackout())
	{
		// A flashlight smothered by the Teacher's own blackout is too weak to stagger them.
		return false;
	}

	const FVector ToTeacher = GetActorLocation() + FVector(0.0f, 0.0f, 56.0f) - (Survivor->GetActorLocation() + FVector(0.0f, 0.0f, 58.0f));
	if (ToTeacher.SizeSquared() > FMath::Square(BHTeacherFlashlightStaggerRange))
	{
		return false;
	}

	FVector FlashlightForward = Survivor->Flashlight ? Survivor->Flashlight->GetForwardVector() : Survivor->GetActorForwardVector();
	if (Survivor->Controller)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		Survivor->Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		FlashlightForward = ViewRotation.Vector();
	}

	const float AimDot = FVector::DotProduct(FlashlightForward.GetSafeNormal(), ToTeacher.GetSafeNormal());
	return AimDot >= BHTeacherFlashlightStaggerDot && Survivor->HasDirectVisibilityToCharacter(this);
}

void ABHCharacter::SpendFlashlightForTeacherStagger(float Now)
{
	if (!HasAuthority())
	{
		return;
	}

	const float PreviousBattery = FlashlightBattery;
	FlashlightBattery = FMath::Max(0.0f, FlashlightBattery - BHTeacherFlashlightStaggerBatteryCost);
	StaminaRecoveryLockedUntil = FMath::Max(StaminaRecoveryLockedUntil, Now + 0.35f);
	if (FlashlightBattery <= 0.0f)
	{
		if (PreviousBattery > 0.0f && !bFlashlightEmptyTelemetryReported)
		{
			if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
			{
				BHGM->RecordPlaytestTelemetryMarker(TEXT("battery_starved"), GetActorLocation(), TEXT("flashlight_stagger_empty"), GetBHPlayerState());
			}
			bFlashlightEmptyTelemetryReported = true;
		}
		bFlashlightOn = false;
	}
	ApplyFlashlightState();
	ForceNetUpdate();
}

void ABHCharacter::NotifyNearbySurvivorsOfTeacherCaptureWindup(float Now)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const float WarningRange = CaptureDistance + 360.0f;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Target = *It;
		ABHPlayerState* TargetPS = Target ? Target->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Target || Target == this || !TargetPS || !TargetPS->IsAliveSurvivor() || Target->IsHiddenInLocker())
		{
			continue;
		}

		if (FVector::DistSquared2D(Target->GetActorLocation(), GetActorLocation()) > FMath::Square(WarningRange))
		{
			continue;
		}

		// This fires once per swing (not per frame), so the old 5.5s re-warn suppression just silently denied the
		// dodge cue to a survivor who was warned by a recent earlier swing -- exactly the player being chased and
		// swung at repeatedly, who most needs it. Keep only the line-of-sight gate (the swing target is in front).
		if (!HasDirectVisibilityToCharacter(Target))
		{
			continue;
		}

		Target->LastTeacherCounterplayHintTime = Now;
		Target->SendStatusMessage(TEXT("Teacher swing: slide, roll, slam a door, or aim flashlight."));
	}
}

void ABHCharacter::ResolveTeacherCaptureAttackAuthority()
{
	if (!HasAuthority() || !bTeacherCaptureAttackActive || bTeacherCaptureAttackResolved || !GetWorld())
	{
		return;
	}

	bTeacherCaptureAttackResolved = true;
	const float Now = GetWorld()->GetTimeSeconds();
	ABHCharacter* BestTarget = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		float CandidateScore = TNumericLimits<float>::Max();
		ABHCharacter* Target = *It;
		if (IsTeacherCaptureCandidateAuthority(Target, CandidateScore) && CandidateScore < BestScore)
		{
			BestTarget = Target;
			BestScore = CandidateScore;
		}
	}

	if (!BestTarget)
	{
		StartTeacherCaptureRecoveryAuthority(Now, BHTeacherCaptureMissRecoverySeconds);
		SendStatusMessage(TEXT("Axe missed. Recovering."));
		return;
	}

	if (CanStaggerTeacherWithFlashlight(BestTarget))
	{
		BestTarget->SpendFlashlightForTeacherStagger(Now);
		BestTarget->SendStatusMessage(TEXT("Flashlight staggered the Teacher. Battery spent."));
		StartTeacherCaptureRecoveryAuthority(Now, BHTeacherCaptureFlashlightStaggerRecoverySeconds);
		SendStatusMessage(TEXT("Flashlight staggered you. Recovering."));
		return;
	}

	if (BestTarget->IsTimedCaptureEvasionActive(Now))
	{
		BestTarget->EmitFootstepStimulus(0.86f, TEXT("panic dodge"), BestTarget->ResolveFootstepSurface());
		BestTarget->SendStatusMessage(TEXT("Timed escape beat the swing. Keep moving."));
		StartTeacherCaptureRecoveryAuthority(Now, BHTeacherCaptureMissRecoverySeconds);
		SendStatusMessage(TEXT("Axe missed. Recovering."));
		return;
	}

	StartTeacherCaptureRecoveryAuthority(Now, BHTeacherCaptureSuccessRecoverySeconds);
	MulticastTeacherMeleeSwing(true);
	if (BHIsRoleWarmupPhase(GetWorld()->GetGameState<ABHGameState>()))
	{
		if (ABHPlayerState* CaptureTargetPS = BestTarget->GetPlayerState<ABHPlayerState>())
		{
			SendStatusMessage(FString::Printf(TEXT("Warmup tag: %s would be captured. Hunt start resets everyone."), *CaptureTargetPS->GetPlayerName()));
		}
		if (ABHPlayerController* TargetPC = Cast<ABHPlayerController>(BestTarget->GetController()))
		{
			TargetPC->ClientShowStatusMessage(TEXT("Warmup capture tag. You stay in play and reset for the Hunt."), 3.0f);
		}
		return;
	}
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifySurvivorCaptured(BestTarget, this);
	}
	else
	{
		BestTarget->MarkCaptured();
	}
	SendStatusMessage(TEXT("Axe capture connected."));
}

bool ABHCharacter::TryCaptureAuthority(bool bShowFailureMessages)
{
	const ABHPlayerState* HunterPS = GetPlayerState<ABHPlayerState>();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!HunterPS || !HunterPS->IsAliveHunter())
	{
		if (bShowFailureMessages && HunterPS && HunterPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			SendStatusMessage(TEXT("Hall monitors cannot capture. Use Q for a real hint, R to mark a false corridor, and G for traps."));
			return false;
		}
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Capture is Teacher-only. Press Enter on all players to assign roles."));
		}
		return false;
	}

	const bool bRoleWarmup = BHIsRoleWarmupPhase(BHGS);
	if (!BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && BHGS->RoundPhase != EBHRoundPhase::FinalEscape && !bRoleWarmup))
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Capture unlocks when the Hunt phase begins."));
		}
		return false;
	}

	if (!CanAct() || IsSpecialMoveActive() || IsProne())
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Finish movement before swinging."));
		}
		return false;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (bTeacherCaptureAttackActive)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(IsTeacherCaptureAttackInWindup() ? TEXT("Axe swing is winding up.") : TEXT("Axe swing is recovering."));
		}
		return false;
	}

	if (Now < TeacherCaptureNextAllowedServerTime)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(FString::Printf(TEXT("Axe recovering: %ds."), FMath::CeilToInt(TeacherCaptureNextAllowedServerTime - Now)));
		}
		return false;
	}

	const float CaptureMinStamina = BHTeacherCaptureMinStamina();
	if (Stamina < CaptureMinStamina)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Too exhausted to swing."));
		}
		return false;
	}

	if (!bRoleWarmup && (BHGS->bCaptureDisabled || BHGS->bHunterInputFrozen || BHFinalEscapeHunterPenaltyActive(GetWorld(), this)))
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Capture disrupted by the evacuation system."));
		}
		return false;
	}

	Stamina = FMath::Max(0.0f, Stamina - BHTeacherCaptureStaminaCost());
	StaminaRecoveryLockedUntil = Now + BHTeacherCaptureAttackSeconds;
	bTeacherCaptureAttackActive = true;
	bTeacherCaptureAttackResolved = false;
	TeacherCaptureAttackStartServerTime = Now;
	TeacherCaptureAttackResolveServerTime = Now + BHTeacherCaptureWindupSeconds;
	TeacherCaptureAttackEndServerTime = Now + BHTeacherCaptureAttackSeconds;
	TeacherCaptureNextAllowedServerTime = TeacherCaptureAttackEndServerTime;
	MulticastTeacherMeleeSwing(false);
	NotifyNearbySurvivorsOfTeacherCaptureWindup(Now);
	EmitFootstepStimulus(0.72f, TEXT("teacher axe windup"), ResolveFootstepSurface());
	if (bShowFailureMessages)
	{
		SendStatusMessage(bRoleWarmup ? TEXT("Warmup axe windup. Capture tags do not count.") : TEXT("Axe windup. Survivors can dodge or blind it."));
	}
	if (ABHPlayerState* WarmupPS = GetPlayerState<ABHPlayerState>())
	{
		WarmupPS->MarkWarmupStep(EBHWarmupStep::Tool);
	}
	RefreshMovementSpeedFromState();
	ForceNetUpdate();
	return true;
}

void ABHCharacter::ServerUseScan_Implementation()
{
	UseScanAuthority(true);
}

bool ABHCharacter::UseScanAuthority(bool bShowFailureMessages)
{
	const ABHPlayerState* HunterPS = GetPlayerState<ABHPlayerState>();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bRoleWarmup = BHIsRoleWarmupPhase(BHGS);
	// Tutorials hand-drive the AI cast: a bot must never autonomously scan. Each bot scan fires a loud-noise
	// presence spike (escalating the dread the tutorial is trying to keep calm) for no teaching value. The human's
	// own scan still works; the director stages any demonstration itself.
	if (BHGS && BHGS->bTutorialMode && HunterPS && HunterPS->IsABot())
	{
		return false;
	}
	if (HunterPS && HunterPS->PlayerRole == EBHPlayerRole::FakeHunter && HunterPS->LifeState == EBHPlayerLifeState::Alive)
	{
		if (!BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && !bRoleWarmup))
		{
			if (bShowFailureMessages)
			{
				SendStatusMessage(TEXT("Hall monitor hints unlock when the Hunt phase begins."));
			}
			return false;
		}

		FString MonitorBlockReason;
		if (!bRoleWarmup)
		{
			if (const ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
			{
				if (!BHGM->CanUseHallMonitorTools(HunterPS, MonitorBlockReason))
				{
					if (bShowFailureMessages)
					{
						SendStatusMessage(MonitorBlockReason);
					}
					return false;
				}
			}
		}

		const float Now = GetWorld()->GetTimeSeconds();
		const float HintCooldown = bRoleWarmup ? 2.0f : FMath::Max(12.0f, ScanCooldownSeconds * 0.75f);
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
		if (ABHPlayerState* WarmupPS = GetPlayerState<ABHPlayerState>())
		{
			WarmupPS->MarkWarmupStep(EBHWarmupStep::Scan);
		}
		if (bRoleWarmup && bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Warmup real hint sent. Contribution gates reset for the Hunt."));
		}
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

	if (!BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && !bRoleWarmup))
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Heartbeat scan unlocks when the Hunt phase begins."));
		}
		return false;
	}

	const int32 ScanFocusCharges = FMath::Clamp(HunterPS->GetPowerupCharges(EBHPowerupType::TeacherScanFocus), 0, 2);
	const float EffectiveScanCooldownSeconds = bRoleWarmup ? 2.0f : FMath::Max(8.0f, ScanCooldownSeconds * (1.0f - 0.18f * static_cast<float>(ScanFocusCharges)));
	const float ScanFearRadius = 3000.0f + 450.0f * static_cast<float>(ScanFocusCharges);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastScanTime < EffectiveScanCooldownSeconds)
	{
		if (bShowFailureMessages)
		{
			const int32 RemainingCooldown = FMath::CeilToInt(EffectiveScanCooldownSeconds - (Now - LastScanTime));
			SendStatusMessage(FString::Printf(TEXT("Heartbeat scan cooling down: %ds."), RemainingCooldown));
		}
		return false;
	}

	LastScanTime = Now;
	if (ABHPlayerState* WarmupPS = GetPlayerState<ABHPlayerState>())
	{
		WarmupPS->MarkWarmupStep(EBHWarmupStep::Scan);
	}
	ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>();
	if (BHGM && !bRoleWarmup)
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
		if (DistSq <= FMath::Square(ScanFearRadius))
		{
			if (!bRoleWarmup)
			{
				Target->AddFear(Target->IsDetentionMarked() ? 24.0f : 16.0f);
				if (ABHPlayerController* SurvivorPC = Cast<ABHPlayerController>(Target->GetController()))
				{
					SurvivorPC->ClientShowStatusMessage(Target->IsDetentionMarked()
						? TEXT("The detention mark burns. The Teacher scan found you.")
						: TEXT("Your heartbeat spikes. The Teacher scanned nearby."), 2.5f);
				}
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
		if (bRoleWarmup)
		{
			SendStatusMessage(TEXT("Warmup scan only. Cooldowns and positions reset when Hunt starts."));
		}
		const int32 PatrolIntelCharges = FMath::Clamp(HunterPS->GetPowerupCharges(EBHPowerupType::TeacherPatrolIntel), 0, 2);
		FVector LatestNoiseLocation = FVector::ZeroVector;
		if (PatrolIntelCharges > 0 && BHGM && BHGM->GetLatestBotNoiseLocation(14.0f + 8.0f * static_cast<float>(PatrolIntelCharges), LatestNoiseLocation))
		{
			SendStatusMessage(FString::Printf(TEXT("Patrol intel: recent noise roughly %s."), *BHCompassFromDelta(LatestNoiseLocation - GetActorLocation())));
		}
	}
	if (BHGM && !bRoleWarmup)
	{
		BHGM->RecordPlaytestTelemetryMarker(TEXT("teacher_scan"), GetActorLocation(), Nearest ? TEXT("found") : TEXT("empty"), HunterPS, Nearest ? Nearest->GetPlayerState<ABHPlayerState>() : nullptr);
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
	const bool bRoleWarmup = BHIsRoleWarmupPhase(BHGS);
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}

	// Tutorials hand-drive the AI cast: a bot must never autonomously blackout/false-marker. The bot blackout does
	// nothing visible on the circuit-less tutorial map anyway, but it spikes presence every few seconds. The human's
	// own R power still works; the director stages any demonstration itself.
	if (BHGS && BHGS->bTutorialMode && BHPS->IsABot())
	{
		return false;
	}

	if (!BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && !bRoleWarmup))
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
		const int32 BlackoutSurgeCharges = FMath::Clamp(BHPS->GetPowerupCharges(EBHPowerupType::TeacherBlackoutSurge), 0, 2);
		const float BlackoutCooldown = bRoleWarmup ? 4.0f : FMath::Max(20.0f, 34.0f - 5.0f * static_cast<float>(BlackoutSurgeCharges));
		if (Now - LastHunterPowerTime < BlackoutCooldown)
		{
			if (bShowFailureMessages)
			{
				SendStatusMessage(FString::Printf(TEXT("Blackout cooling down: %ds."), FMath::CeilToInt(BlackoutCooldown - (Now - LastHunterPowerTime))));
			}
			return false;
		}

		LastHunterPowerTime = Now;
		if (ABHPlayerState* WarmupPS = GetPlayerState<ABHPlayerState>())
		{
			WarmupPS->MarkWarmupStep(EBHWarmupStep::Tool);
		}
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->TriggerHunterBlackout(GetActorLocation(), BlackoutSurgeCharges);
			if (!bRoleWarmup)
			{
				BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("blackout surge"));
			}
		}
		if (bRoleWarmup && bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Warmup blackout fired. Lights and cooldown reset for the Hunt."));
		}
		return true;
	}

	if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		FString MonitorBlockReason;
		if (!bRoleWarmup)
		{
			if (const ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
			{
				if (!BHGM->CanUseHallMonitorTools(BHPS, MonitorBlockReason))
				{
					if (bShowFailureMessages)
					{
						SendStatusMessage(MonitorBlockReason);
					}
					return false;
				}
			}
		}

		const float FakeHintCooldown = bRoleWarmup ? 2.0f : 18.0f;
		if (Now - LastHunterPowerTime < FakeHintCooldown)
		{
			if (bShowFailureMessages)
			{
				SendStatusMessage(FString::Printf(TEXT("False marker cooling down: %ds."), FMath::CeilToInt(FakeHintCooldown - (Now - LastHunterPowerTime))));
			}
			return false;
		}

		LastHunterPowerTime = Now;
		SendFakeHunterHint(false);
		if (ABHPlayerState* WarmupPS = GetPlayerState<ABHPlayerState>())
		{
			WarmupPS->MarkWarmupStep(EBHWarmupStep::Tool);
		}
		if (bRoleWarmup && bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Warmup false hint sent. Hall Monitor cooldowns reset for the Hunt."));
		}
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
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bRoleWarmup = BHIsRoleWarmupPhase(BHGS);
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}

	// Tutorials hand-drive the AI cast: a bot student/teacher must not autonomously spam decoys or traps, because
	// every drop fires a noise alert that overwrites the on-screen lesson prompt. The tutorial director stages a
	// single demonstration decoy itself when teaching the concept.
	if (BHGS && BHGS->bTutorialMode && BHPS->IsABot())
	{
		return false;
	}

	if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		FString MonitorBlockReason;
		if (!bRoleWarmup)
		{
			if (const ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
			{
				if (!BHGM->CanUseHallMonitorTools(BHPS, MonitorBlockReason))
				{
					if (bShowFailureMessages)
					{
						SendStatusMessage(MonitorBlockReason);
					}
					return false;
				}
			}
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	// Human survivors no longer drop decoys on tap. During the live Hunt they can only throw a single-use
	// decoy the class handed them (see ABHGameMode::MaybeHandOutDecoys), so the held charge is the limiter and
	// they skip the shared cooldown. Tester debug, warmup practice, bot baiting, and the Hall Monitor trap all
	// stay on the cooldown so nothing else can be spammed.
	const bool bSurvivorChargeThrow = BHPS->IsAliveSurvivor()
		&& BHPS->PlayerRole != EBHPlayerRole::Tester
		&& !bRoleWarmup
		&& !(BHGS && BHGS->bTutorialMode)
		&& !BHPS->IsABot();
	const float EffectiveDecoyCooldownSeconds = bRoleWarmup ? 2.0f : DecoyCooldownSeconds;
	if (!bSurvivorChargeThrow && Now - LastDecoyTime < EffectiveDecoyCooldownSeconds)
	{
		return false;
	}

	const FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 115.0f + FVector(0.0f, 0.0f, 20.0f);
	if (BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		LastDecoyTime = Now;
		GetWorld()->SpawnActor<ABHNoiseDecoy>(SpawnLocation, GetActorRotation());
		GetWorld()->SpawnActor<ABHAlarmTrap>(SpawnLocation + GetActorRightVector() * 70.0f, GetActorRotation());
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Tester decoy and trap placed."));
		}
		return true;
	}

	if (BHPS->IsAliveSurvivor())
	{
		if (bSurvivorChargeThrow)
		{
			// Single use: consume the handed-out decoy charge. No charge means no throw -- this is what stops
			// ten players carpeting the map with decoys.
			ABHPlayerState* MutablePS = GetPlayerState<ABHPlayerState>();
			if (!MutablePS || !MutablePS->ConsumePowerupCharge(EBHPowerupType::DecoySound))
			{
				if (bShowFailureMessages)
				{
					SendStatusMessage(TEXT("No decoy in hand. The class hands a few out during the hunt."));
				}
				return false;
			}
		}
		LastDecoyTime = Now;
		GetWorld()->SpawnActor<ABHNoiseDecoy>(SpawnLocation, GetActorRotation());
		// The decoy now emits its own realistic footstep/breathing noise over its lifetime, so there is no
		// "decoy"-labelled ping here that would give it away to the Teacher.
		if (bShowFailureMessages)
		{
			SendStatusMessage(bRoleWarmup
				? TEXT("Warmup decoy placed. It will be cleared before the Hunt.")
				: TEXT("Decoy thrown. Fake footsteps will pull the Teacher off your trail."));
		}
		return true;
	}

	if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		LastDecoyTime = Now;
		GetWorld()->SpawnActor<ABHAlarmTrap>(SpawnLocation, GetActorRotation());
		if (ABHPlayerState* WarmupPS = GetPlayerState<ABHPlayerState>())
		{
			WarmupPS->MarkWarmupStep(EBHWarmupStep::Tool);
		}
		if (bShowFailureMessages)
		{
			SendStatusMessage(bRoleWarmup ? TEXT("Warmup trap placed. It will be cleared before the Hunt.") : TEXT("Hall monitor trap arming. Survivors can dodge it until it wakes."));
		}
		return true;
	}

	return false;
}

void ABHCharacter::ServerSubmitAnswer_Implementation(int32 AnswerIndex, bool bVisualAnswer)
{
	SubmitAnswerAuthority(nullptr, AnswerIndex, true, true, bVisualAnswer);
}

void ABHCharacter::ServerUsePowerup_Implementation(EBHPowerupType Type)
{
	if (!CanAct())
	{
		SendStatusMessage(TEXT("You cannot use a powerup right now."));
		return;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (BHIsRoleWarmupPhase(BHGS))
	{
		SendStatusMessage(TEXT("Powerups are saved for the real Hunt. Warmup resources reset at start."));
		return;
	}

	if (!PowerupComponent || !PowerupComponent->ServerUsePowerup(Type))
	{
		SendStatusMessage(TEXT("Powerup unavailable."));
	}
}

bool ABHCharacter::SubmitAnswerAuthority(ABHObjectiveStation* Station, int32 AnswerIndex, bool bUseViewFallback, bool bShowFailureMessages, bool bVisualAnswer)
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
		if (ABHTrainBonusQuestionTerminal* BonusTerminal = Cast<ABHTrainBonusQuestionTerminal>(Target))
		{
			return BonusTerminal->SubmitAnswer(this, AnswerIndex);
		}
	}
	if (!Station)
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Aim at a question station and press 1-4."));
		}
		return false;
	}

	if (PowerupComponent && PowerupComponent->HasActiveQuestionHint() && bShowFailureMessages && Station)
	{
		const FString HintText = Station->GetQuestionExplanation().IsEmpty()
			? Station->GetQuestionSubtopic()
			: Station->GetQuestionExplanation();
		if (!HintText.IsEmpty())
		{
			SendStatusMessage(FString::Printf(TEXT("Hint: %s"), *HintText.Left(160)));
		}
	}

	if (!IsValidInteractionTarget(Station))
	{
		if (bShowFailureMessages)
		{
			SendStatusMessage(TEXT("Question station is too far away."));
		}
		return false;
	}

	return Station->SubmitAnswer(this, AnswerIndex, bVisualAnswer);
}

bool ABHCharacter::SubmitNumericAnswerAuthority(float Value)
{
	if (!CanAct())
	{
		return false;
	}

	// Numeric entry only ever targets the objective station the player is aiming at; the
	// train bonus terminal stays multiple-choice (its focus is never cached for numeric entry).
	AActor* Target = nullptr;
	FindInteractableFromView(Target, 225.0f);
	ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target);
	if (!Station || !IsValidInteractionTarget(Station))
	{
		SendStatusMessage(TEXT("Aim at a calculation station, type your answer, then press Enter."));
		return false;
	}

	return Station->SubmitNumericAnswer(this, Value);
}

// Heading to a scanned target expressed relative to where the player is LOOKING (yaw), so the readout reads
// "to your left/right", "straight ahead", "behind you" - not a map compass bearing players can't orient to.
// UE yaw is clockwise from +X and +Y is to the player's right, so a positive relative angle is to the right.
static FString BHRelativeHeadingText(const FVector& Delta, float ViewYawDeg)
{
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	const float Rel = FMath::UnwindDegrees(TargetYaw - ViewYawDeg);
	const float A = FMath::Abs(Rel);
	if (A <= 22.5f) { return TEXT("straight ahead"); }
	if (A >= 157.5f) { return TEXT("behind you"); }
	const bool bRight = Rel > 0.0f;
	if (A <= 67.5f) { return bRight ? TEXT("ahead on your right") : TEXT("ahead on your left"); }
	if (A <= 112.5f) { return bRight ? TEXT("to your right") : TEXT("to your left"); }
	return bRight ? TEXT("behind you on the right") : TEXT("behind you on the left");
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

	const FVector Delta = TargetLocation - GetActorLocation();
	const float DistanceMeters = Delta.Size2D() / 100.0f;
	// Direction RELATIVE to where the Teacher is looking (their view yaw), not a map compass bearing.
	const FString Heading = BHRelativeHeadingText(Delta, GetControlRotation().Yaw);
	const FString HiddenText = bTargetHidden ? TEXT(" Hidden.") : TEXT("");
	PC->ShowLocalStatusMessage(
		FString::Printf(TEXT("Heartbeat: %s, %.0f m.%s"), *Heading, DistanceMeters, *HiddenText),
		3.0f);
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

void ABHCharacter::OnRep_MovementSpecialState()
{
	ApplyMovementSpecialState();
}

void ABHCharacter::OnRep_TeacherCaptureAttackActive()
{
	RefreshMovementSpeedFromState();
	if (bTeacherCaptureAttackActive)
	{
		PlayTeacherMeleeSwingLocal(false);
	}
}
