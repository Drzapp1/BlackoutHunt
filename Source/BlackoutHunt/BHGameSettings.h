#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "UObject/Object.h"
#include "BHGameSettings.generated.h"

class UStateTree;

UCLASS(Config = Game, DefaultConfig)
class BLACKOUTHUNT_API UBHGameSettings : public UObject
{
	GENERATED_BODY()

public:
	UBHGameSettings();

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 MinPlayers;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 MaxPlayers;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 PrepSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 HuntSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	int32 RequiredBreakers;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	bool bAllowHostForceStart;

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

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float ScanCooldownSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float DecoyCooldownSeconds;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float BatteryRefillAmount;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	TArray<FBHJumpscareVariant> JumpscareVariants;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultMasterVolume;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultMusicVolume;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultUiVolume;

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
