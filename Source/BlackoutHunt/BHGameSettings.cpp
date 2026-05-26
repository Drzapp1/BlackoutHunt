#include "BHGameSettings.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
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
}

UBHGameSettings::UBHGameSettings()
{
	MinPlayers = 2;
	MaxPlayers = 12;
	PrepSeconds = 45;
	HuntSeconds = 900;
	RequiredBreakers = 6;
	bAllowHostForceStart = false;
	bClassroomMode = true;
	bAllowStudentTeacherAdminControls = false;
	bAllowTunnelHelper = true;
	bAllowHotspotHelper = false;
	bClassroomLoopbackOnlyHost = true;

	InteractDistance = 550.0f;
	CaptureDistance = 220.0f;

	FlashlightDrainPerSecond = 0.17f;
	ScanCooldownSeconds = 25.0f;
	DecoyCooldownSeconds = 10.0f;
	BatteryRefillAmount = 45.0f;
	JumpscareVariants = {
		BHMakeJumpscareVariant(
			TEXT("SCP096"),
			TEXT("SCP-096 Prototype"),
			0.55f,
			0,
			TEXT("/Game/BlackoutHunt/Art/SCP096/Skeletal/SK_SCP096.SK_SCP096"),
			TEXT("/Game/BlackoutHunt/Art/SCP096/Skeletal/A_SCP096_Run.A_SCP096_Run"),
			TEXT("/Game/BlackoutHunt/Art/SCP096/SM_SCP096.SM_SCP096"),
			TEXT("/Game/BlackoutHunt/Art/SCP096/M_SCP096.M_SCP096"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.SW_TerrifiedScreamFaint"),
			FVector(-20.0f, 0.0f, -88.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.32f),
			FVector(90.0f, 0.0f, -138.0f),
			FRotator::ZeroRotator,
			FVector(1.55f),
			FLinearColor(1.0f, 0.02f, 0.0f, 1.0f),
			145.0f,
			0.96f,
			0.70f,
			1.10f),
		BHMakeJumpscareVariant(
			TEXT("FabMonster01"),
			TEXT("Free Customizable Jumpscares Monster 01"),
			1.20f,
			1,
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster01/SK_FCJ_Monster01.SK_FCJ_Monster01"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster01/A_FCJ_Monster01_Run.A_FCJ_Monster01_Run"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster01/M_FCJ_Monster01.M_FCJ_Monster01"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Blueprints/BP_FCJ_Jumpscare01.BP_FCJ_Jumpscare01_C"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Audio/SW_FCJ_Jumpscare01.SW_FCJ_Jumpscare01"),
			FVector(0.0f, 0.0f, -84.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.0f),
			FVector(86.0f, 0.0f, -124.0f),
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
			1.15f,
			1,
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster02/SK_FCJ_Monster02.SK_FCJ_Monster02"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster02/A_FCJ_Monster02_Run.A_FCJ_Monster02_Run"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster02/M_FCJ_Monster02.M_FCJ_Monster02"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Blueprints/BP_FCJ_Jumpscare02.BP_FCJ_Jumpscare02_C"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Audio/SW_FCJ_Jumpscare02.SW_FCJ_Jumpscare02"),
			FVector(0.0f, 0.0f, -92.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.08f),
			FVector(82.0f, 0.0f, -130.0f),
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
			1.10f,
			2,
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster03/SK_FCJ_Monster03.SK_FCJ_Monster03"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster03/A_FCJ_Monster03_Run.A_FCJ_Monster03_Run"),
			TEXT(""),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Monster03/M_FCJ_Monster03.M_FCJ_Monster03"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Blueprints/BP_FCJ_Jumpscare03.BP_FCJ_Jumpscare03_C"),
			TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Audio/SW_FCJ_Jumpscare03.SW_FCJ_Jumpscare03"),
			FVector(0.0f, 0.0f, -96.0f),
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(1.14f),
			FVector(78.0f, 0.0f, -134.0f),
			FRotator::ZeroRotator,
			FVector(1.50f),
			FLinearColor(0.0f, 0.78f, 1.0f, 1.0f),
			166.0f,
			1.0f,
			0.90f,
			1.45f)
	};

	DefaultMasterVolume = 1.0f;
	DefaultMusicVolume = 0.85f;
	DefaultUiVolume = 0.9f;

	DefaultBotCount = 5;
	DefaultBotDifficulty = EBHBotDifficulty::Normal;
	bUseStateTreeAI = true;
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
