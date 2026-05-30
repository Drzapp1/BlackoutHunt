#pragma once

#include "CoreMinimal.h"
#include "BHTypes.generated.h"

class AActor;
class AController;
class ABHCharacter;
class UAnimInstance;
class UAnimSequence;
class UCurveFloat;
class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class USkeletalMesh;
class UStaticMesh;

UENUM(BlueprintType)
enum class EBHRoundPhase : uint8
{
	Lobby UMETA(DisplayName = "Lobby"),
	Prep UMETA(DisplayName = "Prep"),
	Hunt UMETA(DisplayName = "Hunt"),
	Intermission UMETA(DisplayName = "Train Intermission"),
	FinalEscape UMETA(DisplayName = "Final Escape"),
	SurvivorsWin UMETA(DisplayName = "Survivors Win"),
	HunterWin UMETA(DisplayName = "Hunter Win")
};

UENUM(BlueprintType)
enum class EBHPlayerRole : uint8
{
	Unassigned UMETA(DisplayName = "Unassigned"),
	Hunter UMETA(DisplayName = "Hunter"),
	FakeHunter UMETA(DisplayName = "Fake Hunter"),
	Survivor UMETA(DisplayName = "Survivor"),
	Tester UMETA(DisplayName = "Tester"),
	Spectator UMETA(DisplayName = "Spectator")
};

UENUM(BlueprintType)
enum class EBHPlayerLifeState : uint8
{
	Alive UMETA(DisplayName = "Alive"),
	Captured UMETA(DisplayName = "Captured"),
	Escaped UMETA(DisplayName = "Escaped")
};

UENUM(BlueprintType)
enum class EBHMovementSpecialState : uint8
{
	None UMETA(DisplayName = "None"),
	Rolling UMETA(DisplayName = "Rolling"),
	Sliding UMETA(DisplayName = "Sliding"),
	Prone UMETA(DisplayName = "Prone"),
	Diving UMETA(DisplayName = "Diving")
};

UENUM(BlueprintType)
enum class EBHRoundModifier : uint8
{
	None UMETA(DisplayName = "None"),
	LightsOut UMETA(DisplayName = "Lights Out"),
	LoudFooting UMETA(DisplayName = "Loud Footing"),
	JammedDoors UMETA(DisplayName = "Jammed Doors"),
	DeadCCTV UMETA(DisplayName = "Dead CCTV"),
	PanicSurge UMETA(DisplayName = "Panic Surge")
};

UENUM(BlueprintType)
enum class EBHFogPreset : uint8
{
	Light UMETA(DisplayName = "Light"),
	Heavy UMETA(DisplayName = "Heavy"),
	Extreme UMETA(DisplayName = "Extreme")
};

UENUM(BlueprintType)
enum class EBHBotDifficulty : uint8
{
	Easy UMETA(DisplayName = "Easy"),
	Normal UMETA(DisplayName = "Normal"),
	Hard UMETA(DisplayName = "Hard")
};

UENUM(BlueprintType)
enum class EBHBotPersonality : uint8
{
	Cautious UMETA(DisplayName = "Cautious"),
	Objective UMETA(DisplayName = "Objective"),
	Bold UMETA(DisplayName = "Bold"),
	Trickster UMETA(DisplayName = "Trickster"),
	Panicked UMETA(DisplayName = "Panicked"),
	Aggressive UMETA(DisplayName = "Aggressive"),
	Suspicious UMETA(DisplayName = "Suspicious"),
	Ambusher UMETA(DisplayName = "Ambusher")
};

UENUM(BlueprintType)
enum class EBHBotIntent : uint8
{
	None UMETA(DisplayName = "None"),
	Patrol UMETA(DisplayName = "Patrol"),
	AnswerStation UMETA(DisplayName = "Answer Station"),
	WorkStation UMETA(DisplayName = "Work Station"),
	RepairBreaker UMETA(DisplayName = "Repair Breaker"),
	Escape UMETA(DisplayName = "Escape"),
	Hide UMETA(DisplayName = "Hide"),
	Flee UMETA(DisplayName = "Flee"),
	Bait UMETA(DisplayName = "Bait"),
	Chase UMETA(DisplayName = "Chase"),
	InvestigateNoise UMETA(DisplayName = "Investigate Noise"),
	InvestigateLastSeen UMETA(DisplayName = "Investigate Last Seen"),
	SearchLocker UMETA(DisplayName = "Search Locker"),
	AmbushObjective UMETA(DisplayName = "Ambush Objective"),
	UseScan UMETA(DisplayName = "Use Scan"),
	UsePower UMETA(DisplayName = "Use Power"),
	DropTrap UMETA(DisplayName = "Drop Trap")
};

UENUM(BlueprintType)
enum class EBHBotStimulusType : uint8
{
	Sight UMETA(DisplayName = "Sight"),
	Noise UMETA(DisplayName = "Noise"),
	Locker UMETA(DisplayName = "Locker"),
	Objective UMETA(DisplayName = "Objective"),
	Trap UMETA(DisplayName = "Trap"),
	Capture UMETA(DisplayName = "Capture"),
	Escape UMETA(DisplayName = "Escape"),
	Unreachable UMETA(DisplayName = "Unreachable")
};

UENUM(BlueprintType)
enum class EBHAtmosphereStimulusType : uint8
{
	Noise UMETA(DisplayName = "Noise"),
	Objective UMETA(DisplayName = "Objective"),
	Locker UMETA(DisplayName = "Locker"),
	Footstep UMETA(DisplayName = "Footstep"),
	CCTV UMETA(DisplayName = "CCTV"),
	Power UMETA(DisplayName = "Power"),
	Monster UMETA(DisplayName = "Monster"),
	Manual UMETA(DisplayName = "Manual")
};

UENUM(BlueprintType)
enum class EBHFootstepSurface : uint8
{
	Default UMETA(DisplayName = "Default"),
	Concrete UMETA(DisplayName = "Concrete"),
	Tile UMETA(DisplayName = "Tile"),
	Metal UMETA(DisplayName = "Metal"),
	Wet UMETA(DisplayName = "Wet"),
	Gravel UMETA(DisplayName = "Gravel"),
	Glass UMETA(DisplayName = "Glass"),
	Soft UMETA(DisplayName = "Soft")
};

UENUM(BlueprintType)
enum class EBHBreakableGlassState : uint8
{
	Intact UMETA(DisplayName = "Intact"),
	Cracked UMETA(DisplayName = "Cracked"),
	Broken UMETA(DisplayName = "Broken")
};

USTRUCT(BlueprintType)
struct FBHFootstepSurfaceProfile
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps")
	EBHFootstepSurface Surface = EBHFootstepSurface::Default;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps", meta = (ClampMin = "0.0"))
	float NoiseMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps", meta = (ClampMin = "0.0"))
	float HearingRadiusBase = 1600.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps", meta = (ClampMin = "0.0"))
	float HearingRadiusScale = 1700.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps", meta = (ClampMin = "0.0"))
	float AtmosphereMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps", meta = (ClampMin = "0.0"))
	float LocalVolume = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps")
	TSoftObjectPtr<USoundBase> FootstepSound;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps")
	TSoftObjectPtr<UNiagaraSystem> FootstepEffect;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Footsteps", meta = (ClampMin = "0.0"))
	float EffectScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FBHMovementAnimationSet
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UAnimSequence> Idle;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UAnimSequence> Walk;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UAnimSequence> Run;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UAnimSequence> Roll;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UAnimSequence> Slide;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UAnimSequence> Dive;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UAnimSequence> ProneIdle;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UAnimSequence> ProneCrawl;
};

USTRUCT(BlueprintType)
struct FBHMovementCurveTuning
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float Distance = 420.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float VerticalImpulse = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float MinForwardClearance = 220.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float FloorProbeDistance = 140.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxSlopeDegrees = 48.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float MaxDropHeight = 74.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftObjectPtr<UCurveFloat> DistanceCurve;
};

USTRUCT(BlueprintType)
struct FBHMovementSpecialTuning
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	EBHMovementSpecialState State = EBHMovementSpecialState::None;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float StaminaCost = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float NoiseStrength = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	FBHMovementCurveTuning Curve;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	FText LowStaminaText;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	FText CooldownText;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	FText BlockedText;
};

UENUM(BlueprintType)
enum class EBHPOVAnimIntensity : uint8
{
	Subtle,
	Moderate,
	Punchy
};

// Procedural, asset-free first-person camera animation layer (additive camera tilt/roll/punch
// synced to rolls, slides, dives, and the capture swing) plus the Hunter third-person body lunge.
// All magnitudes are authored at the "Moderate" level and scaled by Intensity, then by
// ReducedMotionScale when the player has the reduced-camera-shake comfort option enabled.
USTRUCT(BlueprintType)
struct FBHPOVAnimationTuning
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	bool bEnablePOVAnimation = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	EBHPOVAnimIntensity Intensity = EBHPOVAnimIntensity::Moderate;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReducedMotionScale = 0.30f;

	// Roll (degrees / cm)
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float RollSpinDegrees = 320.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float RollPitchDipDeg = 14.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float RollPosPunchCm = 6.0f;

	// Slide
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float SlidePitchDipDeg = 11.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float SlideRollLeanDeg = 7.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float SlideSettleRiseDeg = 4.0f;

	// Dive
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float DivePitchDipDeg = 18.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float DivePosPunchCm = 9.0f;

	// Swing / capture (local first-person)
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float SwingWindupPitchUpDeg = 9.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float SwingWindupYawDeg = 4.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float SwingPunchPitchDownDeg = 12.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float SwingPunchYawDeg = 7.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV")
	float SwingPosPunchCm = 5.0f;

	// Global interp speeds
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV", meta = (ClampMin = "0.0"))
	float RotInterpSpeed = 14.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|POV", meta = (ClampMin = "0.0"))
	float PosInterpSpeed = 16.0f;
};

USTRUCT(BlueprintType)
struct FBHMovementRoleTuning
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	EBHPlayerRole Role = EBHPlayerRole::Survivor;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 360.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 900.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float SprintDrainMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float ProneSpeed = 120.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float StaminaCostMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float CooldownMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float NoiseMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float ProneNoiseMultiplier = 0.38f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement", meta = (ClampMin = "0.0"))
	float ProneVisibilityMultiplier = 0.55f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TArray<FBHMovementSpecialTuning> SpecialMoves;
};

USTRUCT(BlueprintType)
struct FBHMovementAnimationProfile
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	bool bPreferAnimBlueprint = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	FBHMovementAnimationSet Animations;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftClassPtr<UAnimInstance> QuaterniusAnimInstanceClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Movement")
	TSoftClassPtr<UAnimInstance> HunterAnimInstanceClass;
};

USTRUCT(BlueprintType)
struct FBHInteractionPromptInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction")
	bool bUsePromptInfo = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction")
	bool bCanInteract = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction", meta = (ClampMin = "0.0"))
	float HoldSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Progress = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction")
	FText RiskText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction")
	FText DisabledReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction")
	bool bNoisy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Interaction")
	bool bDangerous = false;
};

USTRUCT(BlueprintType)
struct FBHHorrorVariationSettings
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SurfacePatchChance = 0.65f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0"))
	int32 GlassPaneCount = 5;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0"))
	int32 DecoyCueCount = 4;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float IntensityScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FBHObjectiveBeat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Navigation")
	FName BeatId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Navigation")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Navigation")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Navigation", meta = (ClampMin = "0.0"))
	float Radius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Navigation", meta = (ClampMin = "0.0"))
	float ExpireServerTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Navigation")
	bool bPrimary = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Navigation")
	bool bDanger = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Navigation")
	uint8 AudienceRoleMask = 0;
};

UENUM(BlueprintType)
enum class EBHScareEventType : uint8
{
	Ambient UMETA(DisplayName = "Ambient"),
	MonsterCharge UMETA(DisplayName = "Monster Charge"),
	FaceFlash UMETA(DisplayName = "Face Flash"),
	AudioStinger UMETA(DisplayName = "Audio Stinger"),
	LightCut UMETA(DisplayName = "Light Cut"),
	CCTVGlitch UMETA(DisplayName = "CCTV Glitch"),
	LockerKnock UMETA(DisplayName = "Locker Knock"),
	Whisper UMETA(DisplayName = "Whisper"),
	FootstepEcho UMETA(DisplayName = "Footstep Echo")
};

// How a monster-charge jumpscare approaches the target. Used to vary the spawn framing so
// repeated scares stay unpredictable.
UENUM(BlueprintType)
enum class EBHJumpscareApproach : uint8
{
	// Spawns in view ahead of the player and charges head-on (the classic approach).
	HeadOn UMETA(DisplayName = "Head-On"),
	// Spawns behind the player, out of view, so the reveal lands on contact ("it was behind you").
	Behind UMETA(DisplayName = "Behind"),
	// Spawns close and already in view, then lunges almost immediately ("it's already there").
	AlreadyThere UMETA(DisplayName = "Already There"),
	// Spawns above the player and descends onto them ("ceiling drop").
	CeilingDrop UMETA(DisplayName = "Ceiling Drop")
};

USTRUCT(BlueprintType)
struct FBHJumpscareVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FName VariantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0", ClampMax = "3"))
	int32 MinimumScareIntensity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<UAnimSequence> RunAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftClassPtr<AActor> VisualActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<USoundBase> LaunchSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FVector VisualOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FRotator VisualRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FVector VisualScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FVector CloseVisualOffset = FVector(92.0f, 0.0f, -118.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FRotator CloseVisualRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FVector CloseVisualScale = FVector(1.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FLinearColor LightColor = FLinearColor(1.0f, 0.02f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "60.0", ClampMax = "320.0"))
	float FocusHeight = 145.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CameraShakeIntensity = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlashIntensity = 0.70f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0"))
	float CameraJitterDuration = 1.10f;

	// Degrees the field of view snaps inward at the moment of impact, then eases back. 0 disables the punch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float ImpactFOVPunch = 14.0f;

	// Brief client-local hitstop (slow-motion) applied on impact, in real seconds. 0 disables.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float ImpactHitStopSeconds = 0.07f;

	// Gamepad rumble strength on impact (0..1). 0 disables.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ImpactRumbleIntensity = 0.85f;

	// Optional extra audio layer (e.g. a low sub-boom / transient) played alongside the main impact scream.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<USoundBase> ImpactStinger;
};

USTRUCT(BlueprintType)
struct FBHScareEventSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	EBHScareEventType EventType = EBHScareEventType::Ambient;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TObjectPtr<ABHCharacter> Target = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0"))
	float LockSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0"))
	float LightRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<USoundBase> AudioAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSubclassOf<AActor> VisualActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FName VariantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlashIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FLinearColor FlashColor = FLinearColor(1.0f, 0.04f, 0.02f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	bool bGameplayCritical = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	bool bBypassDirectorBudget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	bool bAllowCueTypeSubstitution = true;
};

USTRUCT(BlueprintType)
struct FBHClientHorrorCue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	EBHScareEventType EventType = EBHScareEventType::Ambient;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FVector FocusLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0"))
	float DurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0"))
	float LockSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShakeIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	bool bSnapToFocus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	bool bLockInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<USoundBase> AudioAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float AudioVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftClassPtr<AActor> VisualActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FVector CloseVisualOffset = FVector(92.0f, 0.0f, -118.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FRotator CloseVisualRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FVector CloseVisualScale = FVector(1.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FName VariantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlashIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	FLinearColor FlashColor = FLinearColor(1.0f, 0.04f, 0.02f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0"))
	float CameraJitterDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0"))
	float CameraJitterFrequency = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	bool bCloseRangeFocus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	bool bUpperBodyCloseVisual = false;

	// Degrees the field of view snaps inward on impact, then eases back. 0 disables.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float FOVPunch = 0.0f;

	// Brief client-local hitstop (slow-motion) applied on impact, in real seconds. 0 disables.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float HitStopSeconds = 0.0f;

	// Gamepad rumble strength on impact (0..1). 0 disables.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RumbleIntensity = 0.0f;

	// Optional extra audio layer (e.g. a low sub-boom / transient) played alongside the main impact scream.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	TSoftObjectPtr<USoundBase> ImpactStinger;

	// When true the impact scream is doubled with a pitched-down "roar" layer for extra low-end weight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Horror")
	bool bLayeredImpactAudio = false;
};

USTRUCT(BlueprintType)
struct FBHGameplayAudioCue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio")
	TSoftObjectPtr<USoundBase> AudioAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio")
	FString Caption;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Volume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float Pitch = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio", meta = (ClampMin = "0.0"))
	float MaxAudibleDistance = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio", meta = (ClampMin = "0.0"))
	float CaptionSeconds = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio")
	bool bSpatial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackout Hunt|Audio")
	bool bApplyHorrorComfort = false;
};

UENUM(BlueprintType)
enum class EBHObjectiveStationType : uint8
{
	Valve UMETA(DisplayName = "Valve"),
	Terminal UMETA(DisplayName = "Terminal"),
	Antenna UMETA(DisplayName = "Antenna"),
	Evidence UMETA(DisplayName = "Evidence")
};

UENUM(BlueprintType)
enum class EBHRevisionCounterNodeType : uint8
{
	None UMETA(DisplayName = "None"),
	PeerReview UMETA(DisplayName = "Peer Review"),
	DemonstrationTrap UMETA(DisplayName = "Demonstration Trap")
};

UENUM(BlueprintType)
enum class EBHRevisionMode : uint8
{
	None UMETA(DisplayName = "None"),
	PhysicsClassroom UMETA(DisplayName = "Physics Classroom")
};

UENUM(BlueprintType)
enum class EBHPhysicsTopic : uint8
{
	ForcesAndMotion UMETA(DisplayName = "Forces and Motion"),
	Electricity UMETA(DisplayName = "Electricity"),
	Waves UMETA(DisplayName = "Waves"),
	Energy UMETA(DisplayName = "Energy")
};

UENUM(BlueprintType)
enum class EBHQuestionDifficulty : uint8
{
	Easy UMETA(DisplayName = "Easy"),
	Medium UMETA(DisplayName = "Medium"),
	Hard UMETA(DisplayName = "Hard")
};

UENUM(BlueprintType)
enum class EBHQuestionType : uint8
{
	MultipleChoice UMETA(DisplayName = "Multiple Choice"),
	TrueFalse UMETA(DisplayName = "True/False"),
	Calculation UMETA(DisplayName = "Calculation"),
	FormulaFill UMETA(DisplayName = "IGCSE Skill"),
	GraphReading UMETA(DisplayName = "Graph Reading"),
	DragDropMatching UMETA(DisplayName = "Drag/Drop Matching"),
	Ordering UMETA(DisplayName = "Ordering")
};

UENUM(BlueprintType)
enum class EBHDiagramType : uint8
{
	None UMETA(DisplayName = "None"),
	MotionGraph UMETA(DisplayName = "Motion Graph"),
	VelocityGraph UMETA(DisplayName = "Velocity Graph"),
	ForceArrows UMETA(DisplayName = "Force Arrows"),
	SpringGraph UMETA(DisplayName = "Spring Graph"),
	MomentBeam UMETA(DisplayName = "Moment Beam"),
	Circuit UMETA(DisplayName = "Circuit"),
	IVGraph UMETA(DisplayName = "IV Graph"),
	StaticCharge UMETA(DisplayName = "Static Charge"),
	Wave UMETA(DisplayName = "Wave"),
	EMSpectrum UMETA(DisplayName = "EM Spectrum"),
	RayDiagram UMETA(DisplayName = "Ray Diagram"),
	Sankey UMETA(DisplayName = "Sankey"),
	EnergyChain UMETA(DisplayName = "Energy Chain"),
	// Added types (serialize by name via reflection, so order only needs to be append-only).
	Lens UMETA(DisplayName = "Lens"),                 // converging lens ray diagram (Waves)
	Transformer UMETA(DisplayName = "Transformer"),   // primary/secondary coils, Np:Ns (Electricity)
	MagneticField UMETA(DisplayName = "Magnetic Field"), // bar magnet field lines (Electricity)
	InclinedPlane UMETA(DisplayName = "Inclined Plane"), // ramp with weight/normal/friction (Forces)
	PressureColumn UMETA(DisplayName = "Pressure Column"), // liquid column P = rho g h (Forces)
	EnergyBars UMETA(DisplayName = "Energy Bars"),    // before/after energy bar chart (Energy)
	ParticleModel UMETA(DisplayName = "Particle Model") // solid/liquid/gas particles (Energy/thermal)
};

UENUM(BlueprintType)
enum class EBHRevisionDifficultyMix : uint8
{
	Balanced UMETA(DisplayName = "Balanced"),
	Easy UMETA(DisplayName = "Easy"),
	Hard UMETA(DisplayName = "Hard"),
	Adaptive UMETA(DisplayName = "Adaptive")
};

UENUM(BlueprintType)
enum class EBHTrainPhase : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Arrival UMETA(DisplayName = "Arrival"),
	Recap UMETA(DisplayName = "Class Recap"),
	BonusQuestion UMETA(DisplayName = "Bonus Question"),
	Shop UMETA(DisplayName = "Shop"),
	StationStop UMETA(DisplayName = "Station Stop"),
	Departing UMETA(DisplayName = "Departing")
};

UENUM(BlueprintType)
enum class EBHFinalEscapeState : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Locked UMETA(DisplayName = "Locked"),
	Cutscene UMETA(DisplayName = "Unlock Cutscene"),
	EscapeActive UMETA(DisplayName = "Escape Active"),
	Departed UMETA(DisplayName = "Departed"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EBHPowerupType : uint8
{
	StaminaBoost UMETA(DisplayName = "Stamina Boost"),
	SprintBurst UMETA(DisplayName = "Sprint Burst"),
	FlashlightBoost UMETA(DisplayName = "Light Boost"),
	QuestionHint UMETA(DisplayName = "Question Hint"),
	DecoySound UMETA(DisplayName = "Decoy Sound"),
	DoorRush UMETA(DisplayName = "Door Rush"),
	TeamBeacon UMETA(DisplayName = "Team Beacon"),
	AntiScareCharm UMETA(DisplayName = "Anti-Scare Charm"),
	TeacherScanFocus UMETA(DisplayName = "Teacher Scan Focus"),
	TeacherBlackoutSurge UMETA(DisplayName = "Teacher Blackout Surge"),
	TeacherPatrolIntel UMETA(DisplayName = "Teacher Patrol Intel")
};

USTRUCT(BlueprintType)
struct FBHPowerupDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	EBHPowerupType Type = EBHPowerupType::StaminaBoost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	int32 MaxCharges = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	float DurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	float CooldownSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct FBHPowerupInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	EBHPowerupType Type = EBHPowerupType::StaminaBoost;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	int32 Charges = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	float CooldownEndServerTime = 0.0f;
};

USTRUCT(BlueprintType)
struct FBHQuestionAttemptRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString QuestionId;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString TopicName;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString QuestionText;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString QuestionSubtopic;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString SelectedAnswer;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString CorrectAnswer;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHQuestionDifficulty Difficulty = EBHQuestionDifficulty::Easy;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHQuestionType QuestionType = EBHQuestionType::MultipleChoice;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHPhysicsTopic Topic = EBHPhysicsTopic::ForcesAndMotion;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	bool bCorrect = false;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	bool bCorrection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 StageIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float TimestampSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 PointsEarned = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString TeamSummary;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHPhysicsTopic AdaptiveRecommendedTopic = EBHPhysicsTopic::ForcesAndMotion;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHQuestionDifficulty AdaptiveRecommendedDifficulty = EBHQuestionDifficulty::Easy;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString AdaptiveReason;
};

USTRUCT(BlueprintType)
struct FBHRevisionAnswerPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	TArray<FString> Choices;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 CorrectChoiceIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Formula;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float NumericAnswer = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float NumericTolerance = 0.0f;
};

// Optional per-question parameters that let a diagram render the question's actual
// values (e.g. a real resistor value, lever distance, wave amplitude, ray angle) so the
// player must read the visual to answer. Carries only the givens shown on screen -- never
// the answer. Empty by default, in which case the HUD draws the generic schematic.
USTRUCT(BlueprintType)
struct FBHDiagramParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ValueA = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ValueB = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ValueC = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ValueD = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString LabelA;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString LabelB;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString LabelC;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString LabelD;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString XAxis;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString YAxis;

	// Reused per diagram type: incident ray angle (RayDiagram), amplitude fraction (Wave),
	// or lever ratio. Zero leaves the generic shape untouched.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float AngleOrShape = 0.0f;

	// Per-type shape selector so one diagram type renders the specific case the question asks
	// about. 0 = the historical default shape. Conventions:
	//   IVGraph:        0 ohmic (straight), 1 filament lamp (S-curve), 2 diode (threshold)
	//   Circuit:        0 single, 1 series-2, 2 parallel-2, 3 series-3
	//   MotionGraph:    0 generic, 1 constant velocity, 2 accelerating, 3 decelerating, 4 accel-then-constant
	//   VelocityGraph:  0 generic, 1 constant, 2 accelerating, 3 decelerating, 4 accel-then-constant
	//   ForceArrows:    0 horizontal pair, 1 adds a vertical weight/normal pair (free body)
	//   EMSpectrum:     0 none, 1..7 highlights band radio..gamma
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 ShapeVariant = 0;

	// Optional illustrated diagram texture (object path). Empty => procedural rendering.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString ImageSoftPath;

	bool HasValues() const { return !LabelA.IsEmpty() || !LabelB.IsEmpty() || !LabelC.IsEmpty() || !LabelD.IsEmpty(); }
	bool HasImage() const { return !ImageSoftPath.IsEmpty(); }
};

USTRUCT(BlueprintType)
struct FBHRevisionQuestion
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHPhysicsTopic Topic = EBHPhysicsTopic::ForcesAndMotion;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString TopicName;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Subtopic;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHQuestionDifficulty Difficulty = EBHQuestionDifficulty::Easy;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHQuestionType Type = EBHQuestionType::MultipleChoice;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHDiagramType DiagramType = EBHDiagramType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Prompt;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FBHRevisionAnswerPayload Answer;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FBHDiagramParams Diagram;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Hint;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString CorrectionPrompt;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float MasteryWeight = 1.0f;
};

USTRUCT(BlueprintType)
struct FBHPlayerRevisionStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 Attempts = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 CorrectAnswers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 CorrectionsCompleted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 ContributionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 HintCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float MasteryPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ForcesMastery = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ElectricityMastery = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float WavesMastery = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float EnergyMastery = 0.0f;
};

USTRUCT(BlueprintType)
struct FBHClassRevisionSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ClassMasteryAverage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float LowestStudentMastery = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 ActiveStudentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 StudentsBelowIndividualThreshold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 StudentsWithoutContribution = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHPhysicsTopic WeakTopic = EBHPhysicsTopic::ForcesAndMotion;
};

struct FBHBotStimulus
{
	EBHBotStimulusType Type = EBHBotStimulusType::Noise;
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<AActor> TargetActor;
	FVector Location = FVector::ZeroVector;
	float TimeSeconds = 0.0f;
	float Strength = 1.0f;
	FString Reason;
};

struct FBHBotMemory
{
	TWeakObjectPtr<ABHCharacter> LastSeenSurvivor;
	TWeakObjectPtr<ABHCharacter> LastSeenHunter;
	FVector LastSeenSurvivorLocation = FVector::ZeroVector;
	FVector LastSeenHunterLocation = FVector::ZeroVector;
	FVector LastHeardLocation = FVector::ZeroVector;
	float LastSeenSurvivorTime = -9999.0f;
	float LastSeenHunterTime = -9999.0f;
	float LastHeardTime = -9999.0f;
	float ThreatPressure = 0.0f;
	float ObjectivePressure = 0.0f;
	TArray<FBHBotStimulus> RecentStimuli;
};

struct FBHBotObjectiveClaim
{
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AController> Claimant;
	EBHBotIntent Intent = EBHBotIntent::None;
	float ExpireTimeSeconds = 0.0f;
};

struct FBHBotTargetCooldown
{
	TWeakObjectPtr<AController> Claimant;
	TWeakObjectPtr<AActor> Target;
	float ExpireTimeSeconds = 0.0f;
};

struct FBHBotDecisionCandidate
{
	EBHBotIntent Intent = EBHBotIntent::None;
	TWeakObjectPtr<AActor> Target;
	FVector Location = FVector::ZeroVector;
	float BaseScore = 0.0f;
	float Risk = 0.0f;
	float Urgency = 0.0f;
	float Distance = 0.0f;
	int32 TargetClaimCount = 0;
	FString DebugLabel;
};

struct FBHBotPolicyFeatures
{
	EBHBotPersonality Personality = EBHBotPersonality::Objective;
	EBHBotDifficulty Difficulty = EBHBotDifficulty::Normal;
	EBHPlayerRole Role = EBHPlayerRole::Unassigned;
	float ThreatPressure = 0.0f;
	float ObjectivePressure = 0.0f;
	float TeamSpread = 0.0f;
	float TimeRemainingRatio = 1.0f;
	float DistanceToTarget = 0.0f;
	int32 VisibleEnemyCount = 0;
	int32 NearbyAllyCount = 0;
};

struct FBHBotPolicyResult
{
	int32 ChosenIndex = INDEX_NONE;
	float Score = 0.0f;
	bool bUsedModel = false;
	FString DebugLabel;
};
