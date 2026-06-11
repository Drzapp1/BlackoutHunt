// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "UObject/Object.h"
#include "BHGameSettings.generated.h"

class UStateTree;
class UBHMovementTuningAsset;
class USoundMix;

UCLASS(Config = Game, DefaultConfig)
class BLACKOUTHUNT_API UBHGameSettings : public UObject
{
	GENERATED_BODY()

public:
	UBHGameSettings();

	// Returns the configured class size clamped to the engine-valid connection range [2, 64]. Used to set
	// the ?MaxPlayers= URL option so the engine AGameSession cap (default 16) matches the configured size.
	static int32 GetClampedClassMaxPlayers();

	// The real bot-roster cap: one short of the clamped class size (the host always occupies a slot). The menu
	// label and the replicated TargetBotCount clamp both read this, so they can never disagree with how many
	// bots the gamemode actually spawns (a hardcoded 11 made the label read 11/11 while a 32-player class
	// spawned 31).
	static int32 GetClampedClassMaxBots();

	// Appends ?MaxPlayers=<GetClampedClassMaxPlayers()> to a listen/travel URL if not already present. Every
	// listen-host and ServerTravel URL must carry this; otherwise the engine silently caps the session at 16
	// connections after the first level transition and rejects later students at login.
	static void AppendMaxPlayersOption(FString& Options);

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 MinPlayers;

	// Class size. Clamped to the engine-valid connection range [2,64]; the listen/EOS/travel caps all clamp
	// to 64 too, so a hand-edited config above 64 can't create a phantom higher cap the engine won't honor.
	UPROPERTY(Config, EditAnywhere, Category = "Rules", meta = (ClampMin = "2", ClampMax = "64"))
	int32 MaxPlayers;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 PrepSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 HuntSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 RequiredBreakers;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	bool bAllowHostForceStart;

	// Teaching: a longer first-play role warmup so a brand-new class has time to try everything.
	// The effective warmup uses ExtendedPrepSeconds when this is set OR the install has never hosted
	// a class before (auto-enable on the first session). Host ForceStart still ends Prep instantly.
	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	bool bExtendedFirstWarmup;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 ExtendedPrepSeconds;

	// Bookkeeping for the auto-enable-first-session behaviour above. Set true once the first Hunt of
	// the install starts, so experienced hosts keep the lean default warmup afterwards.
	UPROPERTY(Config)
	bool bHasHostedClassBefore;

	// When true, travel prefers an authored /Game/BlackoutHunt/Maps/<Level> .umap (discovered via a placed
	// ABHLevelMarker) over the procedural runtime generator, falling back to /Engine/Maps/Entry when the
	// authored asset is missing. Leave false until authored maps ship and host/launch travel is fully routed.
	UPROPERTY(Config, EditAnywhere, Category = "Levels")
	bool bUseAuthoredLevels;

	// Seconds a student who drops mid-round may rejoin straight back into their role/state before
	// falling back to a late-join spectator. 0 disables mid-round reconnect restore.
	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	float ReconnectGraceSeconds;

	// After the lobby->first-hunt ServerTravel, the ?BHAutoPrep=1 warmup start is held until every human
	// from the departure roster has re-logged-in (travel is non-seamless, so each student lands only after
	// their own map load). This is the give-up bound: Prep starts anyway once this many seconds pass, so a
	// student whose client never returns can't hold the whole class at a black screen. Clamped to >= 10 s
	// at the consumer so a hand-edited config can't make the gate meaningless on slow school machines.
	UPROPERTY(Config, EditAnywhere, Category = "Rules", meta = (ClampMin = "10.0"))
	float AutoPrepRosterTimeoutSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Classroom")
	bool bClassroomMode;

	UPROPERTY(Config, EditAnywhere, Category = "Classroom")
	bool bAllowStudentTeacherAdminControls;

	UPROPERTY(Config, EditAnywhere, Category = "Classroom")
	bool bAllowTunnelHelper;

	UPROPERTY(Config, EditAnywhere, Category = "Classroom")
	bool bAllowHotspotHelper;

	UPROPERTY(Config, EditAnywhere, Category = "Classroom")
	bool bClassroomLoopbackOnlyHost;

	UPROPERTY(Config, EditAnywhere, Category = "Classroom")
	TArray<FString> ClassroomJoinEndpoints;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float InteractDistance;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float CaptureDistance;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float FlashlightDrainPerSecond;

	// When the Teacher fires a blackout, a nearby student's flashlight is overwhelmed by the dark for a short
	// window: its battery drains FlashlightDrainPerSecond * this multiplier. 1.0 disables the battery surge.
	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float BlackoutFlashlightDrainMultiplier;

	// ...and its beam strength collapses toward this fraction of normal while violently flickering (0.15 = a
	// near-dead 15% beam). 1.0 disables the dimming.
	UPROPERTY(Config, EditAnywhere, Category = "Horror", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlackoutFlashlightStrengthScale;

	// How long that blackout weakening (faster battery drain + flickering near-dead beam) lasts on a student.
	// Each Blackout Surge upgrade charge extends the window by two seconds.
	UPROPERTY(Config, EditAnywhere, Category = "Horror", meta = (ClampMin = "0.0"))
	float BlackoutFlashlightWeakenSeconds;

	// Radius (cm) around the Teacher's blackout within which students' flashlights are weakened. Used as a
	// fallback when no lights were killed; otherwise the killed-light spread plus a margin is used.
	UPROPERTY(Config, EditAnywhere, Category = "Horror", meta = (ClampMin = "0.0"))
	float BlackoutFlashlightEffectRadius;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float ScanCooldownSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float DecoyCooldownSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Hunter", meta = (ClampMin = "0.0"))
	float HunterSprintDrainMultiplierMax;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Hunter", meta = (ClampMin = "0.0"))
	float HunterStaminaRecoveryMultiplier;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Hunter", meta = (ClampMin = "0.0"))
	float TeacherAxeStaminaCost;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Hunter", meta = (ClampMin = "0.0"))
	float TeacherAxeMinStamina;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float BatteryRefillAmount;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampGraceSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampWarningSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampRequiredMoveSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampRequiredMoveDistance;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampMovementSpeedThreshold;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampPressureDreadPerSecond;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampPressureFearPerSecond;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampAlertDelaySeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|AntiCamp", meta = (ClampMin = "0.0"))
	float AntiCampAlertCooldownSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	TArray<FBHJumpscareVariant> JumpscareVariants;

	// Optional SoundMix pushed for JumpscareDuckSeconds when a jumpscare impact fires, to duck ambient audio
	// under the scare. Leave unset to disable ducking (no SoundMix asset shipped by default).
	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	TSoftObjectPtr<USoundMix> JumpscareDuckSoundMix;

	UPROPERTY(Config, EditAnywhere, Category = "Horror", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float JumpscareDuckSeconds = 1.4f;

	// Full-screen "face image" still frames slammed over the view for the FaceImage scare (the
	// "PNG in your face"). A random one is picked per scare. Defaults to the Free Customizable
	// Jumpscares monster textures; add your own imported scary textures here (or in DefaultGame.ini)
	// to extend the pool. Empty falls back to the procedural flash with no image.
	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	TArray<TSoftObjectPtr<UTexture2D>> JumpscareFaceImages;

	// "Behind you" directive scare: the longest the directive + movement lock can hold before the payoff
	// jumpscare auto-fires, so a player who refuses to turn is never stuck. The look input stays free.
	UPROPERTY(Config, EditAnywhere, Category = "Horror", meta = (ClampMin = "1.5", ClampMax = "8.0"))
	float BehindYouMaxHoldSeconds = 6.0f;

	// Corner-peek scare: seconds a peeking figure lingers at a wall edge before retreating if the player
	// never looks at it. Looking directly at it makes it duck back sooner.
	UPROPERTY(Config, EditAnywhere, Category = "Horror", meta = (ClampMin = "0.6", ClampMax = "6.0"))
	float PeekLingerSeconds = 2.6f;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|Footsteps")
	TArray<FBHFootstepSurfaceProfile> FootstepSurfaceProfiles;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Imported Assets")
	bool bUseImportedMovementAnimations;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Imported Assets")
	FBHMovementAnimationSet MovementAnimations;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Production")
	TSoftObjectPtr<UBHMovementTuningAsset> DefaultMovementTuningAsset;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Production")
	TArray<FBHMovementRoleTuning> MovementRoleTunings;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Production")
	FBHMovementAnimationProfile MovementAnimationProfile;

	UPROPERTY(Config, EditAnywhere, Category = "Movement|Production")
	FBHPOVAnimationTuning POVAnimationTuning;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|Variation")
	FBHHorrorVariationSettings HorrorVariation;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|Glass", meta = (ClampMin = "0.0"))
	float GlassCrackDamageThreshold;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|Glass", meta = (ClampMin = "0.0"))
	float GlassBreakDamageThreshold;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|Glass", meta = (ClampMin = "0.0"))
	float GlassBreakNoiseStrength;

	UPROPERTY(Config, EditAnywhere, Category = "Horror|Navigation", meta = (ClampMin = "0.0"))
	float ObjectiveBeatLifetimeSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultMasterVolume;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultMusicVolume;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultUiVolume;

	UPROPERTY(Config, EditAnywhere, Category = "Audio|Identity")
	TSoftObjectPtr<USoundBase> FlashlightOnSound;

	UPROPERTY(Config, EditAnywhere, Category = "Audio|Identity")
	TSoftObjectPtr<USoundBase> FlashlightOffSound;

	UPROPERTY(Config, EditAnywhere, Category = "Audio|Identity")
	TSoftObjectPtr<USoundBase> CCTVStaticSound;

	UPROPERTY(Config, EditAnywhere, Category = "Audio|Identity")
	TSoftObjectPtr<USoundBase> BreakerHumSound;

	UPROPERTY(Config, EditAnywhere, Category = "Audio|Identity")
	TSoftObjectPtr<USoundBase> BreakerCompleteSound;

	UPROPERTY(Config, EditAnywhere, Category = "Audio|Identity")
	TSoftObjectPtr<USoundBase> TeacherProximitySound;

	UPROPERTY(Config, EditAnywhere, Category = "Audio|Identity")
	TSoftObjectPtr<USoundBase> LockerKnockSound;

	UPROPERTY(Config, EditAnywhere, Category = "Audio|Identity")
	TSoftObjectPtr<USoundBase> PowerLossStingerSound;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultReducedJumpscares;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultReducedFlash;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultReducedCameraShake;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultCaptions;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultHighContrastHud;

	// When on, other players' bodies fade out locally when they crowd right against your camera, so a knot of
	// people around a node/breaker doesn't block your view. Local cosmetic only; never hides the Hunter.
	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultHideVeryClosePlayers;

	// Default for the looping "the train is moving" camera sway while riding (intermission/lobby). On by default;
	// turning it off removes the ride sway entirely for motion-sensitive players (separate from reduced shake).
	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultTrainSway;

	// HUD customization defaults (overridable per-player in the menu, persisted locally).
	// DefaultHudScale is the WIDGET size default (meter bars, minimap, equipment, panel chrome);
	// DefaultHudTextScale is the independent TEXT size default. Both default to 1.0 (no change).
	UPROPERTY(Config, EditAnywhere, Category = "Comfort", meta = (ClampMin = "0.75", ClampMax = "1.5"))
	float DefaultHudScale = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort", meta = (ClampMin = "0.80", ClampMax = "1.6"))
	float DefaultHudTextScale = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort", meta = (ClampMin = "0.35", ClampMax = "1.0"))
	float DefaultHudPanelOpacity = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultColorblindHud = false;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultShowMinimap = true;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultShowNameplates = true;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultShowVitals = true;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultShowObjectiveBeats = true;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultShowThreatArrow = true;

	UPROPERTY(Config, EditAnywhere, Category = "Comfort")
	bool bDefaultShowCrosshairDanger = true;

	UPROPERTY(Config, EditAnywhere, Category = "Bots")
	int32 DefaultBotCount;

	UPROPERTY(Config, EditAnywhere, Category = "Bots")
	EBHBotDifficulty DefaultBotDifficulty;

	UPROPERTY(Config, EditAnywhere, Category = "Bots")
	bool bUseStateTreeAI;

	UPROPERTY(Config, EditAnywhere, Category = "Bots")
	TSoftObjectPtr<UStateTree> HunterStateTreeAsset;

	UPROPERTY(Config, EditAnywhere, Category = "Bots")
	float BotThinkInterval;

	UPROPERTY(Config, EditAnywhere, Category = "Bots")
	float BotSightRange;

	UPROPERTY(Config, EditAnywhere, Category = "Bots")
	float BotHearingMemorySeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Bots")
	float BotStuckSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Revision")
	int32 RevisionRoundSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Revision")
	float RevisionClassThreshold;

	UPROPERTY(Config, EditAnywhere, Category = "Revision")
	float RevisionIndividualThreshold;

	UPROPERTY(Config, EditAnywhere, Category = "Revision")
	int32 RevisionScareIntensity;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	bool bUseTrainIntermissions;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	int32 TrainRecapSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	int32 TrainBonusQuestionSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	int32 TrainShopSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	int32 TrainStationStopSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	int32 TrainDepartureCountdownSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	int32 StageOneSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	int32 StageTwoSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Train")
	int32 StageThreeSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Final Escape")
	int32 FinalEscapeSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Final Escape")
	float FinalEscapeCutsceneSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Final Escape")
	float HunterReleaseDelaySeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Final Escape")
	float HunterAntiCampRadius;

	UPROPERTY(Config, EditAnywhere, Category = "Final Escape")
	float HunterAntiCampGraceSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Final Escape")
	float HunterAntiCampPenaltySeconds;
};
