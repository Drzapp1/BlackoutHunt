// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHGameSettings.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "StateTree.h"

namespace
{
FBHJumpscareVariant BHMakeJumpscareVariant(
	const FName VariantId,
	const FString& DisplayName,
	float Weight,
	int32 MinimumScareIntensity,
	const TCHAR* SkeletalMeshPath,
	const TCHAR* RunAnimationPath,
	const TCHAR* StaticMeshPath,
	const TCHAR* MaterialPath,
	const TCHAR* VisualActorClassPath,
	const TCHAR* LaunchSoundPath,
	const FVector& VisualOffset,
	const FRotator& VisualRotation,
	const FVector& VisualScale,
	const FVector& CloseVisualOffset,
	const FRotator& CloseVisualRotation,
	const FVector& CloseVisualScale,
	const FLinearColor& LightColor,
	float FocusHeight,
	float CameraShakeIntensity,
	float FlashIntensity,
	float CameraJitterDuration)
{
	FBHJumpscareVariant Variant;
	Variant.VariantId = VariantId;
	Variant.DisplayName = DisplayName;
	Variant.Weight = Weight;
	Variant.MinimumScareIntensity = FMath::Clamp(MinimumScareIntensity, 0, 3);
	if (SkeletalMeshPath && FCString::Strlen(SkeletalMeshPath) > 0)
	{
		Variant.SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshPath));
	}
	if (RunAnimationPath && FCString::Strlen(RunAnimationPath) > 0)
	{
		Variant.RunAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(RunAnimationPath));
	}
	if (StaticMeshPath && FCString::Strlen(StaticMeshPath) > 0)
	{
		Variant.StaticMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(StaticMeshPath));
	}
	if (MaterialPath && FCString::Strlen(MaterialPath) > 0)
	{
		Variant.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
	}
	if (VisualActorClassPath && FCString::Strlen(VisualActorClassPath) > 0)
	{
		Variant.VisualActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(VisualActorClassPath));
	}
	if (LaunchSoundPath && FCString::Strlen(LaunchSoundPath) > 0)
	{
		Variant.LaunchSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(LaunchSoundPath));
	}
	Variant.VisualOffset = VisualOffset;
	Variant.VisualRotation = VisualRotation;
	Variant.VisualScale = VisualScale;
	Variant.CloseVisualOffset = CloseVisualOffset;
	Variant.CloseVisualRotation = CloseVisualRotation;
	Variant.CloseVisualScale = CloseVisualScale;
	Variant.LightColor = LightColor;
	Variant.FocusHeight = FocusHeight;
	Variant.CameraShakeIntensity = CameraShakeIntensity;
	Variant.FlashIntensity = FlashIntensity;
	Variant.CameraJitterDuration = CameraJitterDuration;
	return Variant;
}

FBHFootstepSurfaceProfile BHMakeFootstepSurfaceProfile(
	EBHFootstepSurface Surface,
	float NoiseMultiplier,
	float HearingRadiusBase,
	float HearingRadiusScale,
	float AtmosphereMultiplier,
	float LocalVolume,
	const TCHAR* FootstepSoundPath = nullptr,
	const TCHAR* FootstepEffectPath = nullptr,
	float EffectScale = 1.0f)
{
	FBHFootstepSurfaceProfile Profile;
	Profile.Surface = Surface;
	Profile.NoiseMultiplier = NoiseMultiplier;
	Profile.HearingRadiusBase = HearingRadiusBase;
	Profile.HearingRadiusScale = HearingRadiusScale;
	Profile.AtmosphereMultiplier = AtmosphereMultiplier;
	Profile.LocalVolume = LocalVolume;
	if (FootstepSoundPath && FCString::Strlen(FootstepSoundPath) > 0)
	{
		Profile.FootstepSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(FootstepSoundPath));
	}
	if (FootstepEffectPath && FCString::Strlen(FootstepEffectPath) > 0)
	{
		Profile.FootstepEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(FootstepEffectPath));
	}
	Profile.EffectScale = FMath::Max(0.0f, EffectScale);
	return Profile;
}

FBHMovementSpecialTuning BHMakeMovementSpecialTuning(
	EBHMovementSpecialState State,
	float DurationSeconds,
	float StaminaCost,
	float CooldownSeconds,
	float NoiseStrength,
	float Distance,
	float VerticalImpulse,
	float MinForwardClearance,
	float MaxDropHeight)
{
	FBHMovementSpecialTuning Tuning;
	Tuning.State = State;
	Tuning.DurationSeconds = DurationSeconds;
	Tuning.StaminaCost = StaminaCost;
	Tuning.CooldownSeconds = CooldownSeconds;
	Tuning.NoiseStrength = NoiseStrength;
	Tuning.Curve.Distance = Distance;
	Tuning.Curve.VerticalImpulse = VerticalImpulse;
	Tuning.Curve.MinForwardClearance = MinForwardClearance;
	Tuning.Curve.MaxDropHeight = MaxDropHeight;
	Tuning.Curve.FloorProbeDistance = 140.0f;
	Tuning.Curve.MaxSlopeDegrees = 48.0f;
	Tuning.LowStaminaText = FText::FromString(TEXT("Too exhausted for that move."));
	Tuning.CooldownText = FText::FromString(TEXT("Movement cooling down."));
	Tuning.BlockedText = FText::FromString(TEXT("Not enough room for that move."));
	return Tuning;
}

FBHMovementRoleTuning BHMakeMovementRoleTuning(
	EBHPlayerRole Role,
	float WalkSpeed,
	float SprintSpeed,
	float SprintDrainMultiplier,
	float ProneSpeed,
	float StaminaCostMultiplier,
	float CooldownMultiplier,
	float NoiseMultiplier,
	float ProneNoiseMultiplier,
	float ProneVisibilityMultiplier)
{
	FBHMovementRoleTuning Tuning;
	Tuning.Role = Role;
	Tuning.WalkSpeed = WalkSpeed;
	Tuning.SprintSpeed = SprintSpeed;
	Tuning.SprintDrainMultiplier = SprintDrainMultiplier;
	Tuning.ProneSpeed = ProneSpeed;
	Tuning.StaminaCostMultiplier = StaminaCostMultiplier;
	Tuning.CooldownMultiplier = CooldownMultiplier;
	Tuning.NoiseMultiplier = NoiseMultiplier;
	Tuning.ProneNoiseMultiplier = ProneNoiseMultiplier;
	Tuning.ProneVisibilityMultiplier = ProneVisibilityMultiplier;
	Tuning.SpecialMoves = {
		BHMakeMovementSpecialTuning(EBHMovementSpecialState::Rolling, 0.55f, 12.0f, 1.25f, 0.72f, 420.0f, 0.0f, 210.0f, 64.0f),
		BHMakeMovementSpecialTuning(EBHMovementSpecialState::Sliding, 0.75f, 16.0f, 1.60f, 1.06f, 620.0f, 0.0f, 310.0f, 58.0f),
		BHMakeMovementSpecialTuning(EBHMovementSpecialState::Diving, 0.65f, 22.0f, 2.40f, 1.24f, 510.0f, 115.0f, 285.0f, 86.0f)
	};
	return Tuning;
}
}

int32 UBHGameSettings::GetClampedClassMaxPlayers()
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	return Settings ? FMath::Clamp(Settings->MaxPlayers, 2, 64) : 32;
}

int32 UBHGameSettings::GetClampedClassMaxBots()
{
	return FMath::Max(1, GetClampedClassMaxPlayers() - 1);
}

void UBHGameSettings::AppendMaxPlayersOption(FString& Options)
{
	// Boundary-aware check: a bare Contains("MaxPlayers=") would also match a colliding option key
	// such as "?GameMaxPlayers=" and silently skip the append, re-introducing the engine's default
	// 16-player cap this function exists to lift. Require the option to begin at a URL delimiter.
	const bool bHasMaxPlayers =
		Options.StartsWith(TEXT("MaxPlayers="), ESearchCase::IgnoreCase) ||
		Options.Contains(TEXT("?MaxPlayers="), ESearchCase::IgnoreCase) ||
		Options.Contains(TEXT("&MaxPlayers="), ESearchCase::IgnoreCase);
	if (!bHasMaxPlayers)
	{
		Options += FString::Printf(TEXT("?MaxPlayers=%d"), GetClampedClassMaxPlayers());
	}
}

UBHGameSettings::UBHGameSettings()
{
	MinPlayers = 2;
	MaxPlayers = 12;
	PrepSeconds = 45;
	bExtendedFirstWarmup = false;
	ExtendedPrepSeconds = 90;
	bHasHostedClassBefore = false;
	HuntSeconds = 900;
	RequiredBreakers = 6;
	bAllowHostForceStart = false;
	bUseAuthoredLevels = false;
	ReconnectGraceSeconds = 120.0f;
	bClassroomMode = true;
	bAllowStudentTeacherAdminControls = false;
	bAllowTunnelHelper = true;
	bAllowHotspotHelper = false;
	// False: the classroom host binds all interfaces so LAN students can join by IP and the Playit tunnel
	// still works for off-LAN students. True would bind 127.0.0.1 only (tunnel-mandatory, no LAN fallback).
	// DefaultGame.ini is the operative value for packaged builds; this is the code-side default/safety net.
	bClassroomLoopbackOnlyHost = false;

	InteractDistance = 550.0f;
	CaptureDistance = 220.0f;

	FlashlightDrainPerSecond = 0.17f;
	BlackoutFlashlightDrainMultiplier = 3.0f;
	BlackoutFlashlightStrengthScale = 0.15f;
	BlackoutFlashlightWeakenSeconds = 6.0f;
	BlackoutFlashlightEffectRadius = 2600.0f;
	ScanCooldownSeconds = 25.0f;
	DecoyCooldownSeconds = 10.0f;
	HunterSprintDrainMultiplierMax = 0.85f;
	HunterStaminaRecoveryMultiplier = 0.8f;
	TeacherAxeStaminaCost = 5.0f;
	TeacherAxeMinStamina = 2.0f;
	BatteryRefillAmount = 45.0f;
	AntiCampGraceSeconds = 60.0f;
	AntiCampWarningSeconds = 50.0f;
	AntiCampRequiredMoveSeconds = 5.0f;
	AntiCampRequiredMoveDistance = 650.0f;
	AntiCampMovementSpeedThreshold = 150.0f;
	AntiCampPressureDreadPerSecond = 2.6f;
	AntiCampPressureFearPerSecond = 0.55f;
	AntiCampAlertDelaySeconds = 18.0f;
	AntiCampAlertCooldownSeconds = 18.0f;
	bUseImportedMovementAnimations = true;
	MovementAnimations.Idle = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Idle.A_BH_Q_Idle")));
	MovementAnimations.Walk = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Walk.A_BH_Q_Walk")));
	MovementAnimations.Run = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Run.A_BH_Q_Run")));
	MovementAnimations.Roll = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/BH_Q_AnimationsCharacterArmature_Roll.BH_Q_AnimationsCharacterArmature_Roll")));
	DefaultMovementTuningAsset = TSoftObjectPtr<UBHMovementTuningAsset>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Data/DA_BH_MovementTuning.DA_BH_MovementTuning")));
	MovementRoleTunings = {
		BHMakeMovementRoleTuning(EBHPlayerRole::Survivor, 360.0f, 900.0f, 1.0f, 120.0f, 1.0f, 1.0f, 1.0f, 0.38f, 0.55f),
		BHMakeMovementRoleTuning(EBHPlayerRole::Tester, 360.0f, 900.0f, 1.0f, 120.0f, 1.0f, 1.0f, 1.0f, 0.38f, 0.55f),
		BHMakeMovementRoleTuning(EBHPlayerRole::FakeHunter, 360.0f, 900.0f, 1.0f, 110.0f, 1.20f, 1.0f, 1.25f, 0.48f, 0.68f),
		BHMakeMovementRoleTuning(EBHPlayerRole::Hunter, 315.0f, 1150.0f, 0.85f, 95.0f, 1.40f, 1.30f, 1.35f, 0.54f, 0.82f)
	};
	MovementAnimationProfile.bPreferAnimBlueprint = true;
	MovementAnimationProfile.Animations = MovementAnimations;
	MovementAnimationProfile.Animations.Roll = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Roll.A_BH_FAL_Roll")));
	MovementAnimationProfile.Animations.Slide = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Slide.A_BH_FAL_Slide")));
	MovementAnimationProfile.Animations.Dive = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Dive.A_BH_FAL_Dive")));
	MovementAnimationProfile.Animations.ProneIdle = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Prone_Idle.A_BH_FAL_Prone_Idle")));
	MovementAnimationProfile.Animations.ProneCrawl = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Prone_Crawl.A_BH_FAL_Prone_Crawl")));
	MovementAnimationProfile.QuaterniusAnimInstanceClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/ABP_BH_Q_Movement.ABP_BH_Q_Movement_C")));
	MovementAnimationProfile.HunterAnimInstanceClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Hunter/ABP_BH_Hunter_Movement.ABP_BH_Hunter_Movement_C")));
	JumpscareVariants = {
		BHMakeJumpscareVariant(
			TEXT("ProxyRed"),
			TEXT("Procedural Red Proxy"),
			0.45f,
			0,
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.SW_TerrifiedScreamFaint"),
			FVector(-20.0f, 0.0f, -88.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.0f),
			FVector(90.0f, 0.0f, -52.0f),
			FRotator::ZeroRotator,
			FVector(1.34f),
			FLinearColor(1.0f, 0.02f, 0.0f, 1.0f),
			145.0f,
			0.96f,
			0.70f,
			1.10f),
		BHMakeJumpscareVariant(
			TEXT("ProxyCyan"),
			TEXT("Procedural Cyan Proxy"),
			0.30f,
			1,
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.SW_TerrifiedScreamFaint"),
			FVector(-20.0f, 0.0f, -88.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.0f),
			FVector(86.0f, 0.0f, -52.0f),
			FRotator::ZeroRotator,
			FVector(1.40f),
			FLinearColor(0.0f, 0.78f, 1.0f, 1.0f),
			154.0f,
			0.86f,
			0.58f,
			1.00f),
		BHMakeJumpscareVariant(
			TEXT("ProxyViolet"),
			TEXT("Procedural Violet Proxy"),
			0.26f,
			2,
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.SW_TerrifiedScreamFaint"),
			FVector(-20.0f, 0.0f, -88.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.0f),
			FVector(82.0f, 0.0f, -52.0f),
			FRotator::ZeroRotator,
			FVector(1.46f),
			FLinearColor(0.78f, 0.03f, 1.0f, 1.0f),
			162.0f,
			0.92f,
			0.64f,
			1.10f),
		BHMakeJumpscareVariant(
			TEXT("FabMonster01"),
			TEXT("Free Customizable Jumpscares Monster 01"),
			1.60f,
			0,
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Meshes/Skeletal_Meshes/SKM_Monster_01.SKM_Monster_01"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Animations/AS_Monster_01_Idle.AS_Monster_01_Idle"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Materials/M_Monster_01.M_Monster_01"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Sounds/SW_Jumpscare_01.SW_Jumpscare_01"),
			FVector(0.0f, 0.0f, -84.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.0f),
			FVector(86.0f, 0.0f, -52.0f),
			FRotator::ZeroRotator,
			FVector(1.42f),
			FLinearColor(1.0f, 0.0f, 0.02f, 1.0f),
			152.0f,
			1.0f,
			0.86f,
			1.35f),
		BHMakeJumpscareVariant(
			TEXT("FabMonster02"),
			TEXT("Free Customizable Jumpscares Monster 02"),
			1.50f,
			0,
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Meshes/Skeletal_Meshes/SKM_Monster_02.SKM_Monster_02"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Animations/AS_Monster_02_Walk.AS_Monster_02_Walk"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Materials/M_Monster_02.M_Monster_02"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Sounds/SW_Jumpscare_02.SW_Jumpscare_02"),
			FVector(0.0f, 0.0f, -92.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.08f),
			FVector(82.0f, 0.0f, -52.0f),
			FRotator::ZeroRotator,
			FVector(1.46f),
			FLinearColor(0.78f, 0.03f, 1.0f, 1.0f),
			158.0f,
			1.0f,
			0.82f,
			1.25f),
		BHMakeJumpscareVariant(
			TEXT("FabMonster03"),
			TEXT("Free Customizable Jumpscares Monster 03"),
			1.40f,
			1,
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Meshes/Skeletal_Meshes/SKM_Monster_03.SKM_Monster_03"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Animations/AS_Monster_03_Jumpscare.AS_Monster_03_Jumpscare"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Materials/M_Monster_03.M_Monster_03"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Sounds/SW_Jumpscare_03.SW_Jumpscare_03"),
			FVector(0.0f, 0.0f, -96.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.14f),
			FVector(78.0f, 0.0f, -52.0f),
			FRotator::ZeroRotator,
			FVector(1.50f),
			FLinearColor(0.0f, 0.78f, 1.0f, 1.0f),
			166.0f,
			1.0f,
			0.90f,
			1.45f)
	};

	// Every pack monster shares the SK_Monster skeleton, so the one shipped lunge-at-camera clip
	// (AS_Monster_03_Jumpscare) drives all of them. Use it as the close-up pose for any variant that owns a
	// skeletal mesh but lacks its own aggressive clip (Monster 01 ships only an idle, Monster 02 only a walk),
	// so the "in your face" slam, the behind-you payoff and corner peeks always present a camera-facing
	// creature instead of a neutral pose that reads as turned 90 degrees.
	const TSoftObjectPtr<UAnimSequence> SharedLungeAnimation(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Animations/AS_Monster_03_Jumpscare.AS_Monster_03_Jumpscare")));
	for (FBHJumpscareVariant& Variant : JumpscareVariants)
	{
		if (Variant.CloseUpAnimation.IsNull() && !Variant.SkeletalMesh.IsNull())
		{
			Variant.CloseUpAnimation = SharedLungeAnimation;
		}
	}

	// Default "PNG in your face" pool: the Free Customizable Jumpscares monster textures. They are
	// cooked as dependencies of the M_Monster_* materials and via the Textures cook directory, so a
	// random one can be slammed full-screen at runtime. Hosts can add imported scary stills here.
	JumpscareFaceImages = {
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Textures/T_Monster_01.T_Monster_01"))),
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Textures/T_Monster_02.T_Monster_02"))),
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Textures/T_Monster_03.T_Monster_03")))
	};
	FootstepSurfaceProfiles = {
		BHMakeFootstepSurfaceProfile(EBHFootstepSurface::Default, 1.00f, 1600.0f, 1700.0f, 1.00f, 0.24f, TEXT("/Game/ResidentHorrorV1/Audio/CharactersAudio/WaveFiles/Foley/Ground/WAV_ground_4.WAV_ground_4"), TEXT("/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_General1_Surface.PSN_General1_Surface"), 0.42f),
		BHMakeFootstepSurfaceProfile(EBHFootstepSurface::Concrete, 1.05f, 1700.0f, 1850.0f, 1.05f, 0.30f, TEXT("/Game/ResidentHorrorV1/Audio/CharactersAudio/WaveFiles/Foley/Concrete/Walk/CUE_Foley_fs_1p_sneaker_concrete_walk_04_Cue.CUE_Foley_fs_1p_sneaker_concrete_walk_04_Cue"), TEXT("/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_General1_Surface.PSN_General1_Surface"), 0.46f),
		BHMakeFootstepSurfaceProfile(EBHFootstepSurface::Tile, 1.24f, 1850.0f, 2050.0f, 1.14f, 0.34f, TEXT("/Game/ResidentHorrorV1/Audio/CharactersAudio/WaveFiles/Foley/Concrete/Walk/WAV_Foley_fs_1p_sneaker_concrete_walk_13.WAV_Foley_fs_1p_sneaker_concrete_walk_13"), TEXT("/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_General2_Surface.PSN_General2_Surface"), 0.42f),
		BHMakeFootstepSurfaceProfile(EBHFootstepSurface::Metal, 1.42f, 2050.0f, 2300.0f, 1.22f, 0.42f, TEXT("/Game/ResidentHorrorV1/Audio/Metal/CUE_Footstep_Metal_Boots_Jog_1_Cue.CUE_Footstep_Metal_Boots_Jog_1_Cue"), TEXT("/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_Sparks_Surface.PSN_Sparks_Surface"), 0.34f),
		BHMakeFootstepSurfaceProfile(EBHFootstepSurface::Wet, 1.18f, 1900.0f, 2150.0f, 1.24f, 0.38f, TEXT("/Game/ResidentHorrorV1/Audio/CharactersAudio/WaveFiles/Foley/Water/CUE_water_6_Cue.CUE_water_6_Cue"), TEXT("/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_WaterLight_Surface.PSN_WaterLight_Surface"), 0.48f),
		BHMakeFootstepSurfaceProfile(EBHFootstepSurface::Gravel, 1.34f, 1950.0f, 2200.0f, 1.18f, 0.38f, TEXT("/Game/ResidentHorrorV1/Audio/Cue/CUE_ground_6_Cue.CUE_ground_6_Cue"), TEXT("/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_Gravel_Surface.PSN_Gravel_Surface"), 0.45f),
		BHMakeFootstepSurfaceProfile(EBHFootstepSurface::Glass, 1.58f, 2150.0f, 2500.0f, 1.32f, 0.46f, TEXT("/Game/ResidentHorrorV1/Audio/Metal/WAV_Footstep_Metal_Boots_Jog_Stop_4.WAV_Footstep_Metal_Boots_Jog_Stop_4"), TEXT("/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_Sparks_Surface.PSN_Sparks_Surface"), 0.30f),
		BHMakeFootstepSurfaceProfile(EBHFootstepSurface::Soft, 0.62f, 1100.0f, 1250.0f, 0.74f, 0.20f, TEXT("/Game/ResidentHorrorV1/Audio/CharactersAudio/WaveFiles/Foley/Grass/CUE_grass_1_Cue.CUE_grass_1_Cue"), TEXT("/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_Grass_Surface.PSN_Grass_Surface"), 0.40f)
	};
	HorrorVariation.SurfacePatchChance = 0.65f;
	HorrorVariation.GlassPaneCount = 5;
	HorrorVariation.DecoyCueCount = 4;
	HorrorVariation.IntensityScale = 1.0f;
	GlassCrackDamageThreshold = 12.0f;
	GlassBreakDamageThreshold = 34.0f;
	GlassBreakNoiseStrength = 1.15f;
	ObjectiveBeatLifetimeSeconds = 18.0f;

	DefaultMasterVolume = 1.0f;
	DefaultMusicVolume = 0.85f;
	DefaultUiVolume = 0.9f;
	FlashlightOnSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/FlashLight_System/Sound/Flashlight_On.Flashlight_On")));
	FlashlightOffSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/FlashLight_System/Sound/Flashlight_OFF.Flashlight_OFF")));
	CCTVStaticSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/SecurityCameras/Sounds/Wav_Static_CCTV.Wav_Static_CCTV")));
	BreakerHumSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/SecurityCameras/Sounds/Wav_Electricity.Wav_Electricity")));
	BreakerCompleteSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/SecurityCameras/Sounds/Wav_Fix.Wav_Fix")));
	TeacherProximitySound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/ResidentHorrorV1/Audio/WavFiles/WAV_tbsfx-heartbeat-415837.WAV_tbsfx-heartbeat-415837")));
	LockerKnockSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/SoundsOfHorror/Impacts/CUE/CUE_SOH_IP_03.CUE_SOH_IP_03")));
	PowerLossStingerSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/SoundsOfHorror/Rumbles/CUE/CUE_SOH_RU_04.CUE_SOH_RU_04")));
	bDefaultReducedJumpscares = false;
	bDefaultReducedFlash = false;
	bDefaultReducedCameraShake = false;
	bDefaultCaptions = true;
	bDefaultHighContrastHud = false;
	bDefaultHideVeryClosePlayers = false;
	bDefaultTrainSway = true;

	DefaultBotCount = 5;
	DefaultBotDifficulty = EBHBotDifficulty::Normal;
	bUseStateTreeAI = false;
	HunterStateTreeAsset = TSoftObjectPtr<UStateTree>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/AI/ST_BH_HunterAtmosphere.ST_BH_HunterAtmosphere")));
	BotThinkInterval = 0.25f;
	BotSightRange = 2800.0f;
	BotHearingMemorySeconds = 12.0f;
	BotStuckSeconds = 6.0f;
	RevisionRoundSeconds = 600;
	RevisionClassThreshold = 70.0f;
	RevisionIndividualThreshold = 50.0f;
	RevisionScareIntensity = 3;

	bUseTrainIntermissions = true;
	TrainRecapSeconds = 35;
	TrainBonusQuestionSeconds = 60;
	TrainShopSeconds = 45;
	TrainStationStopSeconds = 30;
	TrainDepartureCountdownSeconds = 12;
	StageOneSeconds = 300;
	StageTwoSeconds = 420;
	StageThreeSeconds = 600;

	FinalEscapeSeconds = 120;
	FinalEscapeCutsceneSeconds = 6.5f;
	HunterReleaseDelaySeconds = 3.5f;
	HunterAntiCampRadius = 520.0f;
	HunterAntiCampGraceSeconds = 4.0f;
	HunterAntiCampPenaltySeconds = 2.5f;
}
