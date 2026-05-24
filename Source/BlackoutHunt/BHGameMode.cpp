#include "BHGameMode.h"
#include "BHAmbientEmitter.h"
#include "BHBatteryPickup.h"
#include "BHBlockActor.h"
#include "BHBotController.h"
#include "BHBotPolicySubsystem.h"
#include "BHBreaker.h"
#include "BHCharacter.h"
#include "BHDoor.h"
#include "BHExitGate.h"
#include "BHFlickerLight.h"
#include "BHGameInstance.h"
#include "BHGameState.h"
#include "BHGameSettings.h"
#include "BHHUD.h"
#include "BHJumpscareMonster.h"
#include "BHLocker.h"
#include "BHObjectiveStation.h"
#include "BHPanicAlarm.h"
#include "BHPowerSwitch.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "BHSecurityShutter.h"
#include "BHSecurityTerminal.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/Navigation/NavigationDirtyArea.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
float CenterZForBlockBottom(float BottomZ, float ScaleZ)
{
	return BottomZ + ScaleZ * 50.0f;
}

float CenterZForBlockTop(float TopZ, float ScaleZ)
{
	return TopZ - ScaleZ * 50.0f;
}

int32 CountActiveRevisionStudents(const AGameStateBase* GameState, bool bHumansOnly)
{
	if (!GameState)
	{
		return 0;
	}

	int32 Count = 0;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS && BHPS->IsAliveSurvivor() && (!bHumansOnly || !BHPS->IsABot()))
		{
			++Count;
		}
	}
	return Count;
}

FLinearColor AvatarColorForIndex(int32 Index)
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

FString CompassFromDelta(const FVector& Delta)
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

FString NormalizeBHLevelName(FString LevelName)
{
	LevelName.TrimStartAndEndInline();
	if (LevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || LevelName.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
	{
		return TEXT("Foggrounds");
	}
	return LevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? TEXT("Substation") : TEXT("Facility");
}

bool IsTrueOption(const FString& Value)
{
	return Value.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("bot"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("bots"), ESearchCase::IgnoreCase);
}

EBHFogPreset ParseFogPreset(const FString& Value, EBHFogPreset DefaultPreset)
{
	if (Value.Equals(TEXT("Light"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
	{
		return EBHFogPreset::Light;
	}
	if (Value.Equals(TEXT("Extreme"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Max"), ESearchCase::IgnoreCase))
	{
		return EBHFogPreset::Extreme;
	}
	if (Value.Equals(TEXT("Heavy"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
	{
		return EBHFogPreset::Heavy;
	}
	return DefaultPreset;
}

FString FogPresetToString(EBHFogPreset Preset)
{
	switch (Preset)
	{
	case EBHFogPreset::Light:
		return TEXT("Light");
	case EBHFogPreset::Extreme:
		return TEXT("Extreme");
	case EBHFogPreset::Heavy:
	default:
		return TEXT("Heavy");
	}
}

EBHBotDifficulty ParseBotDifficulty(const FString& Value, EBHBotDifficulty DefaultDifficulty)
{
	if (Value.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Easy;
	}
	if (Value.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Hard;
	}
	if (Value.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Normal;
	}
	return DefaultDifficulty;
}

FString BotDifficultyToString(EBHBotDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EBHBotDifficulty::Easy:
		return TEXT("Easy");
	case EBHBotDifficulty::Hard:
		return TEXT("Hard");
	case EBHBotDifficulty::Normal:
	default:
		return TEXT("Normal");
	}
}

EBHRevisionDifficultyMix ParseRevisionDifficultyMix(const FString& Value, EBHRevisionDifficultyMix DefaultMix)
{
	if (Value.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Easy;
	}
	if (Value.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Hard;
	}
	if (Value.Equals(TEXT("Adaptive"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Adaptive;
	}
	if (Value.Equals(TEXT("Balanced"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Balanced;
	}
	return DefaultMix;
}

FString RevisionDifficultyMixToString(EBHRevisionDifficultyMix Mix)
{
	switch (Mix)
	{
	case EBHRevisionDifficultyMix::Easy:
		return TEXT("Easy");
	case EBHRevisionDifficultyMix::Hard:
		return TEXT("Hard");
	case EBHRevisionDifficultyMix::Balanced:
		return TEXT("Balanced");
	case EBHRevisionDifficultyMix::Adaptive:
	default:
		return TEXT("Adaptive");
	}
}

int32 ParsePhysicsTopicMask(const FString& Value, int32 DefaultMask)
{
	if (Value.IsEmpty() || Value.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		return 0x0F;
	}
	if (Value.IsNumeric())
	{
		return FMath::Clamp(FCString::Atoi(*Value), 1, 0x0F);
	}

	int32 Mask = 0;
	TArray<FString> Parts;
	Value.ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() == 0)
	{
		Parts.Add(Value);
	}
	for (FString Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (Part.Equals(TEXT("Forces"), ESearchCase::IgnoreCase) || Part.Equals(TEXT("Motion"), ESearchCase::IgnoreCase) || Part.Equals(TEXT("ForcesAndMotion"), ESearchCase::IgnoreCase))
		{
			Mask |= FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::ForcesAndMotion);
		}
		else if (Part.Equals(TEXT("Electricity"), ESearchCase::IgnoreCase))
		{
			Mask |= FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Electricity);
		}
		else if (Part.Equals(TEXT("Waves"), ESearchCase::IgnoreCase))
		{
			Mask |= FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Waves);
		}
		else if (Part.Equals(TEXT("Energy"), ESearchCase::IgnoreCase))
		{
			Mask |= FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Energy);
		}
	}
	return Mask == 0 ? DefaultMask : (Mask & 0x0F);
}

FString BotIntentToString(EBHBotIntent Intent)
{
	const UEnum* Enum = StaticEnum<EBHBotIntent>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Intent)) : TEXT("Unknown");
}

FString BotStimulusToString(EBHBotStimulusType Type)
{
	const UEnum* Enum = StaticEnum<EBHBotStimulusType>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Type)) : TEXT("Unknown");
}

const TCHAR* SelectPromptLine(const TCHAR* const* Lines, int32 LineCount, int32 Salt)
{
	if (!Lines || LineCount <= 0)
	{
		return TEXT("");
	}

	return Lines[FMath::Abs(Salt) % LineCount];
}
}

ABHGameMode::ABHGameMode()
{
	PlayerControllerClass = ABHPlayerController::StaticClass();
	PlayerStateClass = ABHPlayerState::StaticClass();
	GameStateClass = ABHGameState::StaticClass();
	DefaultPawnClass = ABHCharacter::StaticClass();
	HUDClass = ABHHUD::StaticClass();

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	MinPlayers = FMath::Max(1, Settings->MinPlayers);
	MaxPlayers = FMath::Max(MinPlayers, Settings->MaxPlayers);
	PrepSeconds = FMath::Max(0, Settings->PrepSeconds);
	HuntSeconds = FMath::Max(60, Settings->HuntSeconds);
	RequiredBreakers = FMath::Max(1, Settings->RequiredBreakers);
	bAllowHostForceStart = Settings->bAllowHostForceStart;
	HunterSpawn = FVector(-4050.0f, 0.0f, 120.0f);
	RuntimeLevelName = TEXT("Facility");
	NextRuntimeLevelName = TEXT("Facility");
	RuntimeFogPreset = EBHFogPreset::Heavy;
	NextFogPreset = EBHFogPreset::Heavy;
	bFogPresetOverride = false;
	bFacilityBuilt = false;
	RoundSeed = 0;
	TargetHunterCount = 1;
	ObjectiveIntensity = 2;
	ActiveBreakerCount = RequiredBreakers;
	ActiveSideObjectiveCount = 0;
	bInfectionMode = false;
	bPartyPace = false;
	bPracticeMode = false;
	bTestMode = false;
	RevisionMode = EBHRevisionMode::None;
	bRevisionMode = false;
	RevisionTopicMask = 0x0F;
	RevisionDifficultyMix = EBHRevisionDifficultyMix::Adaptive;
	RevisionClassThreshold = Settings->RevisionClassThreshold;
	RevisionIndividualThreshold = Settings->RevisionIndividualThreshold;
	RevisionRoundDuration = FMath::Max(60, Settings->RevisionRoundSeconds);
	RevisionScareIntensity = FMath::Clamp(Settings->RevisionScareIntensity, 0, 3);
	RevisionReviewTimeRemaining = 0;
	bBotMode = false;
	TargetBotCount = 0;
	BotDifficulty = Settings->DefaultBotDifficulty;
	PracticeRoundModifier = EBHRoundModifier::None;
	NoiseRadiusMultiplier = 1.0f;
	LastDirectorScareTime = -999.0f;
	LastMonsterChargeTime = -999.0f;
	LastColdCallTime = -999.0f;
	LastPresenceWhisperTime = -999.0f;
	LastPresenceSpikeTime = -999.0f;
	LastTeacherCounterScareTime = -999.0f;
	LastBotNoiseLocation = FVector::ZeroVector;
	LastBotNoiseTime = -9999.0f;
	bRuntimeNavigationReady = false;
	RuntimeNavBounds = nullptr;
}

void ABHGameMode::BeginPlay()
{
	Super::BeginPlay();

	BuildRuntimeFacility();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Lobby);
		BHGS->SetRemainingTime(0);
		BHGS->SetBreakerCounts(0, RequiredBreakers);
		BHGS->SetSideObjectiveCounts(0, 0);
		BHGS->SetExitUnlocked(false);
		BHGS->SetDirectorState(RoundSeed, TEXT("Reach the lobby and ready up."), NextRuntimeLevelName);
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, EBHRoundModifier::None);
		BHGS->SetFogOptions(NextFogPreset, bFogPresetOverride);
		BHGS->SetActiveFogPreset(RuntimeFogPreset);
		BHGS->SetActiveLevelName(RuntimeLevelName);
		BHGS->SetPresenceState(0.0f, TEXT("The building is listening."), 0);
		BHGS->SetPracticeMode(bPracticeMode);
		BHGS->SetTestMode(bTestMode);
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		BHGS->SetRevisionSummary(0.0f, EBHPhysicsTopic::ForcesAndMotion, 0, TEXT(""));
	}

	if (bRevisionMode)
	{
		FString BankSummary;
		const bool bBankValid = FBHRevisionQuestionBank::Validate(BankSummary);
		UE_LOG(LogTemp, Log, TEXT("%s"), *BankSummary);
		if (!bBankValid)
		{
			BroadcastStatus(TEXT("Physics question bank validation failed. Check logs."), 8.0f);
		}
	}

	if (bBotMode)
	{
		RefreshBotRoster(nullptr);
	}

	const FString ForceHuntOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHForceHunt="), TEXT("")) : FString();
	if (!bPracticeMode && !bTestMode && IsTrueOption(ForceHuntOption))
	{
		FTimerDelegate ForceHuntDelegate;
		ForceHuntDelegate.BindWeakLambda(this, [this]()
		{
			if (ABHGameState* BHGS = GetGameState<ABHGameState>())
			{
				if (BHGS->RoundPhase == EBHRoundPhase::Lobby || BHGS->RoundPhase == EBHRoundPhase::Prep)
				{
					StartHuntPhaseImmediately();
				}
			}
		});
		FTimerHandle ForceHuntHandle;
		GetWorldTimerManager().SetTimer(ForceHuntHandle, ForceHuntDelegate, 1.0f, false);
	}

	const FString NavCheckOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHRunBotNavCheck="), TEXT("")) : FString();
	if (IsTrueOption(NavCheckOption))
	{
		FTimerDelegate NavCheckDelegate;
		NavCheckDelegate.BindWeakLambda(this, [this]()
		{
			FString Summary;
			RunBotNavCheck(Summary);
		});
		FTimerHandle NavCheckHandle;
		GetWorldTimerManager().SetTimer(NavCheckHandle, NavCheckDelegate, 1.75f, false);
	}

	const FString ReportOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHBotReport="), TEXT("")) : FString();
	if (IsTrueOption(ReportOption))
	{
		FTimerDelegate ReportDelegate;
		ReportDelegate.BindWeakLambda(this, [this]()
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), *GetBotMemoryReport());
		});
		FTimerHandle ReportHandle;
		GetWorldTimerManager().SetTimer(ReportHandle, ReportDelegate, 10.0f, true, 5.0f);
	}
}

void ABHGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	const bool bCanJoinRound = bPracticeMode || bTestMode || !BHGS || BHGS->RoundPhase == EBHRoundPhase::Lobby;

	if (ABHPlayerState* BHPS = NewPlayer ? NewPlayer->GetPlayerState<ABHPlayerState>() : nullptr)
	{
		const int32 PlayerIndex = GameState ? FMath::Max(0, GameState->PlayerArray.IndexOfByKey(BHPS)) : 0;
		const FString HumanRoleOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHHumanRole="), TEXT("")) : FString();
		EBHPlayerRole RequestedRole = bTestMode ? EBHPlayerRole::Tester : (bBotMode ? EBHPlayerRole::Survivor : EBHPlayerRole::Unassigned);
		if (!bTestMode && (HumanRoleOption.Equals(TEXT("Hunter"), ESearchCase::IgnoreCase) || HumanRoleOption.Equals(TEXT("Teacher"), ESearchCase::IgnoreCase)))
		{
			RequestedRole = EBHPlayerRole::Hunter;
		}
		else if (!bTestMode && (HumanRoleOption.Equals(TEXT("FakeHunter"), ESearchCase::IgnoreCase) || HumanRoleOption.Equals(TEXT("Monitor"), ESearchCase::IgnoreCase)))
		{
			RequestedRole = EBHPlayerRole::FakeHunter;
		}
		BHPS->SetReady(bPracticeMode || bTestMode);
		BHPS->SetDesiredRole(bTestMode ? EBHPlayerRole::Tester : (bPracticeMode ? EBHPlayerRole::Survivor : RequestedRole));
		BHPS->SetRole(bTestMode ? EBHPlayerRole::Tester : (bPracticeMode ? EBHPlayerRole::Survivor : (bCanJoinRound ? EBHPlayerRole::Unassigned : EBHPlayerRole::Spectator)));
		BHPS->SetLifeState(bCanJoinRound ? EBHPlayerLifeState::Alive : EBHPlayerLifeState::Captured);
		BHPS->SetHiddenInLocker(false);
		BHPS->SetAvatarIndex(PlayerIndex);
		BHPS->SetAvatarColor(AvatarColorForIndex(PlayerIndex));
		BHPS->SetMapVote(TEXT(""));
		BHPS->ClearFogPresetVote();
		if (bRevisionMode)
		{
			BHPS->ResetRevisionStats();
		}
	}

	if (bBotMode)
	{
		TrimBotRosterToCapacity();
	}

	if (GameState && GameState->PlayerArray.Num() > MaxPlayers)
	{
		NewPlayer->ClientTravel(TEXT("/Engine/Maps/Entry"), TRAVEL_Absolute);
		return;
	}

	RestartPlayer(NewPlayer);

	if (bTestMode)
	{
		StartTestMode(Cast<ABHPlayerController>(NewPlayer));
	}
	else if (bPracticeMode)
	{
		StartPracticeMode(Cast<ABHPlayerController>(NewPlayer));
	}
	else if (bBotMode)
	{
		RefreshBotRoster(Cast<ABHPlayerController>(NewPlayer));
	}
}

void ABHGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		if (!bPracticeMode && !bTestMode && BHGS->RoundPhase == EBHRoundPhase::Hunt && CountAliveSurvivors() <= 0)
		{
			EndRound(EBHRoundPhase::HunterWin);
		}
	}
}

void ABHGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	if (APawn* ExistingPawn = NewPlayer->GetPawn())
	{
		ExistingPawn->Destroy();
	}

	RestartPlayerAtTransform(NewPlayer, GetSpawnTransformFor(NewPlayer));
}

void ABHGameMode::SetPlayerReady(ABHPlayerController* Controller, bool bReady)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	ABHPlayerState* BHPS = Controller ? Controller->GetPlayerState<ABHPlayerState>() : nullptr;
	if (bTestMode)
	{
		if (BHPS)
		{
			BHPS->SetReady(true);
			BHPS->SetRole(EBHPlayerRole::Tester);
			BHPS->SetDesiredRole(EBHPlayerRole::Tester);
			BHPS->SetLifeState(EBHPlayerLifeState::Alive);
			BHPS->SetHiddenInLocker(false);
		}
		if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
		{
			StartTestMode(Controller);
		}
		else if (Controller)
		{
			Controller->ClientShowStatusMessage(TEXT("Test Round is already open. No ready-up needed."), 2.5f);
		}
		return;
	}

	if (!BHGS || !BHPS || BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		return;
	}

	BHPS->SetReady(bReady);

	if (AreAllReady())
	{
		StartPrepPhase();
	}
	else if (Controller)
	{
		const int32 PlayerCount = GameState ? GameState->PlayerArray.Num() : 0;
		if (PlayerCount < MinPlayers)
		{
			Controller->ClientShowStatusMessage(FString::Printf(TEXT("Ready set. Waiting for at least %d players."), MinPlayers), 3.0f);
		}
		else
		{
			Controller->ClientShowStatusMessage(TEXT("Ready set. Waiting for every player to press Enter."), 3.0f);
		}
	}
}

void ABHGameMode::NotifyBreakerRepaired(const FVector& BreakerLocation)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || (BHGS->bExitUnlocked && !bTestMode))
	{
		return;
	}

	const int32 ActiveRequiredBreakers = FMath::Max(1, BHGS->BreakersRequired);
	const int32 NewCompleted = FMath::Min(BHGS->BreakersCompleted + 1, ActiveRequiredBreakers);
	BHGS->SetBreakerCounts(NewCompleted, ActiveRequiredBreakers);
	ReportBotStimulus(EBHBotStimulusType::Objective, BreakerLocation, nullptr, nullptr, TEXT("breaker repaired"), 1.2f);
	BroadcastStatus(FString::Printf(TEXT("Breaker repaired: %d/%d."), NewCompleted, ActiveRequiredBreakers), 3.0f);
	if (!FlickerLights.IsEmpty() && FMath::FRand() < 0.35f)
	{
		if (ABHFlickerLight* Light = FlickerLights[FMath::RandRange(0, FlickerLights.Num() - 1)])
		{
			Light->SetPowered(false);
		}
	}
	ApplyPresenceSpike(BreakerLocation, 54.0f + NewCompleted * 4.0f, TEXT("The Shape noticed the power coming back."));
	UpdateExitUnlockState();
}

void ABHGameMode::NotifyObjectiveStationCompleted(ABHObjectiveStation* Station)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || (BHGS->bExitUnlocked && !bTestMode) || !Station || !Station->IsDirectorActive())
	{
		return;
	}

	const int32 RequiredSideObjectives = FMath::Max(0, BHGS->SideObjectivesRequired);
	const int32 NewCompleted = FMath::Min(BHGS->SideObjectivesCompleted + 1, RequiredSideObjectives);
	BHGS->SetSideObjectiveCounts(NewCompleted, RequiredSideObjectives);
	ReportBotStimulus(EBHBotStimulusType::Objective, Station->GetActorLocation(), Station, Station, TEXT("completed station"), 1.3f);
	BroadcastStatus(FString::Printf(TEXT("Side objective complete: %d/%d."), NewCompleted, RequiredSideObjectives), 3.5f);
	NotifyLoudNoise(Station->GetActorLocation(), TEXT("completed station"));
	ApplyPresenceSpike(Station->GetActorLocation(), 58.0f + NewCompleted * 5.0f, TEXT("The next task made too much noise."));
	if (bRevisionMode)
	{
		RevisionReviewTimeRemaining = 60;
		UpdateRevisionSummary(FString::Printf(TEXT("Stage review %d/%d: discuss the explanation, formula, and weakest topic before moving."), NewCompleted, RequiredSideObjectives));
	}
	if (bPartyPace || BHGS->RoundModifier == EBHRoundModifier::PanicSurge)
	{
		TriggerScareEvent();
	}
	UpdateExitUnlockState();
}

void ABHGameMode::NotifySurvivorCaptured(ABHCharacter* Survivor)
{
	if (!Survivor)
	{
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	ABHPlayerState* SurvivorPS = Survivor->GetPlayerState<ABHPlayerState>();
	AController* SurvivorController = Survivor->GetController();
	if (bPracticeMode || bTestMode)
	{
		const FVector CaptureLocation = Survivor->GetActorLocation();
		Survivor->MarkCaptured();
		ApplyPresenceSpike(CaptureLocation, 70.0f, bTestMode ? TEXT("Test capture registered.") : TEXT("Practice capture registered."));
		if (SurvivorPS)
		{
			SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
			SurvivorPS->SetHiddenInLocker(false);
			if (bTestMode)
			{
				SurvivorPS->SetRole(EBHPlayerRole::Tester);
				SurvivorPS->SetDesiredRole(EBHPlayerRole::Tester);
			}
		}
		if (SurvivorController)
		{
			RestartPlayer(SurvivorController);
		}
		BroadcastStatus(bTestMode ? TEXT("Test capture registered. The round stays open.") : TEXT("Practice capture registered. The round stays open."), 3.5f);
		return;
	}

	if (bInfectionMode && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && SurvivorPS && SurvivorPS->IsAliveSurvivor())
	{
		SurvivorPS->SetRole(EBHPlayerRole::Hunter);
		SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
		SurvivorPS->SetHiddenInLocker(false);
		BroadcastStatus(FString::Printf(TEXT("%s was sent to detention and joined the Teacher."), *SurvivorPS->GetPlayerName()), 4.0f);
		RestartPlayer(Survivor->GetController());

		if (CountAliveSurvivors() <= 0)
		{
			EndRound(EBHRoundPhase::HunterWin);
		}
		return;
	}

	const bool bCanReturnAsFakeHunter = BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && SurvivorPS && SurvivorPS->IsAliveSurvivor() && SurvivorController;
	const FVector CaptureLocation = Survivor->GetActorLocation();
	Survivor->MarkCaptured();
	ReportBotStimulus(EBHBotStimulusType::Capture, CaptureLocation, Survivor, Survivor, TEXT("survivor captured"), 1.6f);
	ApplyPresenceSpike(CaptureLocation, 92.0f, TEXT("The Teacher found someone."));
	if (SurvivorPS && bCanReturnAsFakeHunter)
	{
		SurvivorPS->SetFakeHunterEligible(true);
	}

	if (bCanReturnAsFakeHunter && CountAliveSurvivors() > 0)
	{
		SurvivorPS->SetRole(EBHPlayerRole::FakeHunter);
		SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
		SurvivorPS->SetHiddenInLocker(false);
		SurvivorPS->SetFakeHunterEligible(false);
		BroadcastStatus(FString::Printf(TEXT("%s returned as a hall monitor. They can trap and mislead, but cannot capture."), *SurvivorPS->GetPlayerName()), 5.0f);
		RestartPlayer(SurvivorController);
		return;
	}

	if (CountAliveSurvivors() <= 0)
	{
		EndRound(EBHRoundPhase::HunterWin);
	}
}

void ABHGameMode::NotifySurvivorEscaped(ABHCharacter* Survivor)
{
	if (!Survivor)
	{
		return;
	}

	if (bPracticeMode || bTestMode)
	{
		BroadcastStatus(bTestMode ? TEXT("Test escape reached. The round stays open.") : TEXT("Practice escape reached. The lab stays open."), 3.5f);
		if (ABHGameState* BHGS = GetGameState<ABHGameState>())
		{
			BHGS->SetExitUnlocked(bTestMode);
			BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 64.0f), bTestMode ? TEXT("Test escape complete. Exit remains open.") : TEXT("Practice escape complete. Resetting the exit."), BHGS->PresencePulse + 1);
		}
		return;
	}

	ReportBotStimulus(EBHBotStimulusType::Escape, Survivor->GetActorLocation(), Survivor, Survivor, TEXT("survivor escaped"), 1.6f);
	Survivor->MarkEscaped();
	EndRound(EBHRoundPhase::SurvivorsWin);
}

void ABHGameMode::ToggleLightCircuit(int32 CircuitId)
{
	if (CircuitId <= 0)
	{
		return;
	}

	bool bFoundPoweredLight = false;
	for (TActorIterator<ABHFlickerLight> It(GetWorld()); It; ++It)
	{
		if (It->GetCircuitId() == CircuitId)
		{
			bFoundPoweredLight = It->IsPowered();
			break;
		}
	}

	const bool bNewPowered = !bFoundPoweredLight;
	for (TActorIterator<ABHFlickerLight> It(GetWorld()); It; ++It)
	{
		if (It->GetCircuitId() == CircuitId)
		{
			It->SetPowered(bNewPowered);
		}
	}
}

void ABHGameMode::OpenSecurityCircuit(int32 CircuitId)
{
	for (TActorIterator<ABHSecurityShutter> It(GetWorld()); It; ++It)
	{
		if (It->GetCircuitId() == CircuitId)
		{
			It->SetOpen(true);
		}
	}
}

void ABHGameMode::NotifyLoudNoise(const FVector& Location, const FString& Reason)
{
	LastBotNoiseLocation = Location;
	LastBotNoiseTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastBotNoiseTime;
	ReportBotStimulus(EBHBotStimulusType::Noise, Location, nullptr, nullptr, Reason, Reason.Contains(TEXT("decoy"), ESearchCase::IgnoreCase) ? 0.7f : 1.0f);
	SpawnAmbient(Location + FVector(0.0f, 0.0f, 80.0f), 150.0f, 0.18f, 0.08f, 3.2f, 3.5f);
	const float Spike = Reason.Contains(TEXT("decoy"), ESearchCase::IgnoreCase) ? 42.0f : 50.0f;
	ApplyPresenceSpike(Location, Spike, FString::Printf(TEXT("Something turned toward the %s."), *Reason));

	const bool bLearningCriticalAlert =
		Reason.Contains(TEXT("completed station"), ESearchCase::IgnoreCase) ||
		Reason.Contains(TEXT("wrong answer"), ESearchCase::IgnoreCase);
	const bool bShowHunterNoiseAlert = !bRevisionMode || bLearningCriticalAlert;
	if (!bShowHunterNoiseAlert || !GetWorld())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!PC || !BHPS || !Pawn || !BHPS->IsAliveHunter())
		{
			continue;
		}

		const FVector Delta = Location - Pawn->GetActorLocation();
		const float DistanceMeters = Delta.Size() / 100.0f;
		if (DistanceMeters <= 75.0f * FMath::Max(0.25f, NoiseRadiusMultiplier))
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Noise: %s %s, %.0fm away."), *Reason, *CompassFromDelta(Delta), DistanceMeters), 2.5f);
		}
	}
}

bool ABHGameMode::IsHostAdminController(const ABHPlayerController* RequestingController) const
{
	const UWorld* World = GetWorld();
	if (!RequestingController || !World)
	{
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	if (RequestingController->IsLocalController()
		&& (NetMode == NM_ListenServer || NetMode == NM_Standalone))
	{
		return true;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings && !Settings->bClassroomMode && Settings->bAllowStudentTeacherAdminControls)
	{
		const ABHPlayerState* BHPS = RequestingController->GetPlayerState<ABHPlayerState>();
		return BHPS && BHPS->IsAliveHunter();
	}

	return false;
}

bool ABHGameMode::RequireHostAdmin(ABHPlayerController* RequestingController, const TCHAR* ActionDescription) const
{
	if (IsHostAdminController(RequestingController))
	{
		return true;
	}

	const FString Action = ActionDescription && FCString::Strlen(ActionDescription) > 0
		? FString(ActionDescription)
		: FString(TEXT("use this classroom control"));
	UE_LOG(LogTemp, Warning, TEXT("Denied host-admin action '%s' from %s."),
		*Action,
		*GetNameSafe(RequestingController));

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Only the host machine can %s."), *Action), 3.0f);
	}

	return false;
}

void ABHGameMode::ForceStartRound(ABHPlayerController* RequestingController)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Force start is only available in the lobby."), 3.0f);
		}
		return;
	}

	if (!bAllowHostForceStart)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Host force-start is disabled in BHGameSettings."), 3.0f);
		}
		return;
	}

	if (!RequireHostAdmin(RequestingController, TEXT("force-start the round")))
	{
		return;
	}

	StartHuntPhaseImmediately();
}

void ABHGameMode::SetDesiredRole(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState, EBHPlayerRole DesiredRole)
{
	if (!RequireHostAdmin(RequestingController, TEXT("assign roles")))
	{
		return;
	}

	if (bPracticeMode && RequestingController && RequestingController->PlayerState == TargetPlayerState)
	{
		SetPracticeRole(RequestingController, DesiredRole);
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Roles can only be assigned in the lobby."), 3.0f);
		}
		return;
	}

	ABHPlayerState* TargetBHPS = Cast<ABHPlayerState>(TargetPlayerState);
	if (!TargetBHPS || !GameState || !GameState->PlayerArray.Contains(TargetBHPS))
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Could not find that player for role assignment."), 3.0f);
		}
		return;
	}

	if (DesiredRole != EBHPlayerRole::Hunter && DesiredRole != EBHPlayerRole::Survivor && DesiredRole != EBHPlayerRole::FakeHunter)
	{
		DesiredRole = EBHPlayerRole::Unassigned;
	}

	TargetBHPS->SetDesiredRole(DesiredRole);

	if (RequestingController)
	{
		const UEnum* RoleEnum = StaticEnum<EBHPlayerRole>();
		FString RoleName = RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(DesiredRole)) : FString(TEXT("Unassigned"));
		if (DesiredRole == EBHPlayerRole::Hunter)
		{
			RoleName = TEXT("Teacher");
		}
		else if (DesiredRole == EBHPlayerRole::FakeHunter)
		{
			RoleName = TEXT("Hall Monitor");
		}
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("%s queued as %s."),
			*TargetBHPS->GetPlayerName(),
			*RoleName), 2.5f);
	}
}

void ABHGameMode::KickPlayer(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState)
{
	if (!RequireHostAdmin(RequestingController, TEXT("kick players")))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_ListenServer)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Player kick is only available to the listen-server host."), 3.0f);
		}
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Players can only be kicked from the lobby."), 3.0f);
		}
		return;
	}

	ABHPlayerState* TargetBHPS = Cast<ABHPlayerState>(TargetPlayerState);
	if (!TargetBHPS || !GameState || !GameState->PlayerArray.Contains(TargetBHPS) || TargetBHPS->IsABot())
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Could not find a connected student to kick."), 3.0f);
		}
		return;
	}

	if (RequestingController && RequestingController->PlayerState == TargetBHPS)
	{
		RequestingController->ClientShowStatusMessage(TEXT("The host cannot kick themselves."), 3.0f);
		return;
	}

	ABHPlayerController* TargetController = nullptr;
	if (World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ABHPlayerController* CandidateController = Cast<ABHPlayerController>(It->Get());
			if (CandidateController && CandidateController->PlayerState == TargetBHPS)
			{
				TargetController = CandidateController;
				break;
			}
		}
	}

	if (!TargetController || TargetController->IsLocalController())
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("The host/local player cannot be kicked."), 3.0f);
		}
		return;
	}

	const FString TargetName = TargetBHPS->GetPlayerName().IsEmpty() ? FString(TEXT("Player")) : TargetBHPS->GetPlayerName();
	TargetBHPS->SetReady(false);
	TargetController->ClientShowStatusMessage(TEXT("You were removed from this classroom lobby by the host."), 6.0f);
	TargetController->ClientTravel(TEXT("/Engine/Maps/Entry?BHRemovedByHost=1"), TRAVEL_Absolute);

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Removed %s from the lobby."), *TargetName), 3.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("Classroom soft kick: host %s removed %s from lobby."),
		*GetNameSafe(RequestingController),
		*TargetName);
}

void ABHGameMode::SetNextLevel(ABHPlayerController* RequestingController, const FString& LevelName)
{
	if (!RequireHostAdmin(RequestingController, TEXT("set the next level")))
	{
		return;
	}

	NextRuntimeLevelName = NormalizeBHLevelName(LevelName);

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	UpdateDirectorGameState(BHGS ? BHGS->ObjectiveText : FString(TEXT("Next level updated.")));

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Next level set to %s."), *NextRuntimeLevelName), 3.0f);
	}
}

void ABHGameMode::SetMapVote(ABHPlayerController* RequestingController, const FString& LevelName)
{
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	const FString Vote = NormalizeBHLevelName(LevelName);
	BHPS->SetMapVote(Vote);
	RefreshNextLevelFromVotes();
	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Map vote set to %s."), *Vote), 2.75f);
}

void ABHGameMode::SetFogPresetOverride(ABHPlayerController* RequestingController, EBHFogPreset FogPreset)
{
	if (!RequireHostAdmin(RequestingController, TEXT("override the next fog preset")))
	{
		return;
	}

	NextFogPreset = FogPreset;
	bFogPresetOverride = true;
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	UpdateDirectorGameState(BHGS ? BHGS->ObjectiveText : FString(TEXT("Fog preset updated.")));

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Next fog preset overridden to %s."), *FogPresetToString(NextFogPreset)), 3.0f);
	}
}

void ABHGameMode::ClearFogPresetOverride(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("return fog preset control to votes")))
	{
		return;
	}

	bFogPresetOverride = false;
	NextFogPreset = RuntimeFogPreset;
	RefreshNextLevelFromVotes();

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Fog preset override cleared; lobby votes now choose next fog."), 3.0f);
	}
}

void ABHGameMode::SetFogPresetVote(ABHPlayerController* RequestingController, EBHFogPreset FogPreset)
{
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	BHPS->SetFogPresetVote(FogPreset);
	RefreshNextLevelFromVotes();
	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Fog vote set to %s."), *FogPresetToString(FogPreset)), 2.75f);
}

void ABHGameMode::SetPlayerAvatar(ABHPlayerController* RequestingController, int32 AvatarIndex)
{
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	const int32 NormalizedIndex = FMath::Clamp(AvatarIndex, 0, 7);
	BHPS->SetAvatarIndex(NormalizedIndex);

	if (ABHCharacter* Character = Cast<ABHCharacter>(RequestingController->GetPawn()))
	{
		Character->ApplyAvatarStyle();
	}

	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Avatar set to %d."), NormalizedIndex + 1), 2.5f);
}

void ABHGameMode::SetTargetHunterCount(ABHPlayerController* RequestingController, int32 NewHunterCount)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change Teacher count")))
	{
		return;
	}

	TargetHunterCount = FMath::Clamp(NewHunterCount, 1, 3);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, BHGS->RoundModifier);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Teacher count set to %d."), TargetHunterCount), 3.0f);
	}
}

void ABHGameMode::SetObjectiveIntensity(ABHPlayerController* RequestingController, int32 NewObjectiveIntensity)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change objective intensity")))
	{
		return;
	}

	ObjectiveIntensity = FMath::Clamp(NewObjectiveIntensity, 0, 3);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, BHGS->RoundModifier);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Objective intensity set to %d."), ObjectiveIntensity), 3.0f);
	}

	if (bTestMode)
	{
		RefreshTestDirector(TEXT("Test objectives refreshed."));
	}
	else if (bPracticeMode)
	{
		RefreshPracticeDirector(TEXT("Practice objectives refreshed."));
	}
}

void ABHGameMode::ToggleInfectionMode(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("toggle infection mode")))
	{
		return;
	}

	bInfectionMode = !bInfectionMode;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, BHGS->RoundModifier);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(bInfectionMode ? TEXT("Infection mode enabled.") : TEXT("Infection mode disabled."), 3.0f);
	}
}

void ABHGameMode::TogglePaceMode(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change round pacing")))
	{
		return;
	}

	bPartyPace = !bPartyPace;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, BHGS->RoundModifier);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(bPartyPace ? TEXT("Pace set to party chaos.") : TEXT("Pace set to slow horror."), 3.0f);
	}

	if (bTestMode)
	{
		RefreshTestDirector(TEXT("Test pacing refreshed."));
	}
	else if (bPracticeMode)
	{
		RefreshPracticeDirector(TEXT("Practice pacing refreshed."));
	}
}

void ABHGameMode::SetBotCount(ABHPlayerController* RequestingController, int32 NewBotCount)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change bot count")))
	{
		return;
	}

	TargetBotCount = FMath::Clamp(NewBotCount, 0, FMath::Max(0, MaxPlayers - 1));
	bBotMode = TargetBotCount > 0 || bBotMode;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
		if (BHGS->RoundPhase == EBHRoundPhase::Lobby || bPracticeMode)
		{
			RefreshBotRoster(RequestingController);
		}
		else if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Bot count queued for next round: %d."), TargetBotCount), 3.0f);
		}
	}
}

void ABHGameMode::SetBotDifficulty(ABHPlayerController* RequestingController, EBHBotDifficulty NewDifficulty)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change bot difficulty")))
	{
		return;
	}

	BotDifficulty = NewDifficulty;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Bot difficulty set to %s."), *BotDifficultyToString(BotDifficulty)), 3.0f);
	}
}

bool ABHGameMode::IsRevisionMode() const
{
	return bRevisionMode;
}

EBHRevisionDifficultyMix ABHGameMode::GetRevisionDifficultyMix() const
{
	return RevisionDifficultyMix;
}

TArray<EBHPhysicsTopic> ABHGameMode::GetRevisionWeakTopics() const
{
	return {GetWeakestRevisionTopic()};
}

int32 ABHGameMode::GetRevisionQuestionTargetPerNode() const
{
	int32 StudentCount = CountActiveRevisionStudents(GameState, true);
	if (StudentCount <= 0)
	{
		StudentCount = CountActiveRevisionStudents(GameState, false);
	}

	if (StudentCount >= 10)
	{
		return 4;
	}
	if (StudentCount >= 6)
	{
		return 3;
	}
	return 2;
}

int32 ABHGameMode::GetRevisionAnswerTeamTargetSize() const
{
	int32 StudentCount = CountActiveRevisionStudents(GameState, true);
	if (StudentCount <= 0)
	{
		StudentCount = CountActiveRevisionStudents(GameState, false);
	}

	if (StudentCount <= 0)
	{
		return 1;
	}
	if (StudentCount <= 3)
	{
		return StudentCount;
	}
	if (StudentCount >= 10)
	{
		return 5;
	}
	return 4;
}

int32 ABHGameMode::GetRevisionMinimumContributionTarget() const
{
	return FMath::Clamp(GetRevisionQuestionTargetPerNode() - 1, 1, 3);
}

bool ABHGameMode::IsRevisionAdmin(const ABHPlayerController* RequestingController) const
{
	return IsHostAdminController(RequestingController);
}

void ABHGameMode::RecordRevisionAnswer(ABHCharacter* Character, const FBHRevisionQuestion& Question, bool bCorrect, bool bCorrection)
{
	ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!bRevisionMode || !BHPS || BHPS->PlayerRole != EBHPlayerRole::Survivor)
	{
		return;
	}

	FBHPlayerRevisionStats Stats = BHPS->RevisionStats;
	Stats.Attempts = FMath::Max(0, Stats.Attempts + 1);
	if (bCorrect)
	{
		Stats.CorrectAnswers = FMath::Max(0, Stats.CorrectAnswers + 1);
		Stats.ContributionCount = FMath::Max(0, Stats.ContributionCount + 1);
		if (bCorrection)
		{
			Stats.CorrectionsCompleted = FMath::Max(0, Stats.CorrectionsCompleted + 1);
		}

		const float TopicGain = 24.0f * FMath::Max(0.75f, Question.MasteryWeight);
		switch (Question.Topic)
		{
		case EBHPhysicsTopic::ForcesAndMotion:
			Stats.ForcesMastery = FMath::Clamp(Stats.ForcesMastery + TopicGain, 0.0f, 100.0f);
			break;
		case EBHPhysicsTopic::Electricity:
			Stats.ElectricityMastery = FMath::Clamp(Stats.ElectricityMastery + TopicGain, 0.0f, 100.0f);
			break;
		case EBHPhysicsTopic::Waves:
			Stats.WavesMastery = FMath::Clamp(Stats.WavesMastery + TopicGain, 0.0f, 100.0f);
			break;
		case EBHPhysicsTopic::Energy:
			Stats.EnergyMastery = FMath::Clamp(Stats.EnergyMastery + TopicGain, 0.0f, 100.0f);
			break;
		}
	}
	else
	{
		Stats.HintCount = FMath::Max(0, Stats.HintCount + 1);
	}

	const int32 MasteredAnswers = Stats.CorrectAnswers + Stats.CorrectionsCompleted;
	Stats.MasteryPercent = Stats.Attempts > 0 ? FMath::Clamp(100.0f * static_cast<float>(MasteredAnswers) / static_cast<float>(Stats.Attempts), 0.0f, 100.0f) : 0.0f;
	BHPS->RevisionStats = Stats;

	UpdateRevisionSummary(bCorrect
		? FString::Printf(TEXT("%s banked %s mastery."), *BHPS->GetPlayerName(), *Question.TopicName)
		: FString::Printf(TEXT("%s needs a correction in %s."), *BHPS->GetPlayerName(), *Question.TopicName));
	UpdateDirectorGameState(GetRevisionObjectiveText());
}

bool ABHGameMode::BuildRevisionAnswerTeam(ABHObjectiveStation* Station, ABHCharacter* RequestingCharacter, TSet<int32>& OutPlayerIds, FString& OutSummary) const
{
	OutPlayerIds.Reset();
	OutSummary = TEXT("");
	if (!bRevisionMode || !GameState)
	{
		return false;
	}

	const EBHPhysicsTopic Topic = Station ? FBHRevisionQuestionBank::TopicForStationType(Station->GetStationType()) : EBHPhysicsTopic::ForcesAndMotion;
	const auto TopicMastery = [Topic](const ABHPlayerState* PS)
	{
		if (!PS)
		{
			return 0.0f;
		}
		switch (Topic)
		{
		case EBHPhysicsTopic::ForcesAndMotion:
			return PS->RevisionStats.ForcesMastery;
		case EBHPhysicsTopic::Electricity:
			return PS->RevisionStats.ElectricityMastery;
		case EBHPhysicsTopic::Waves:
			return PS->RevisionStats.WavesMastery;
		case EBHPhysicsTopic::Energy:
			return PS->RevisionStats.EnergyMastery;
		default:
			return 0.0f;
		}
	};

	TArray<ABHPlayerState*> Students;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS && BHPS->IsAliveSurvivor() && !BHPS->IsABot())
		{
			Students.Add(BHPS);
		}
	}
	if (Students.IsEmpty())
	{
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
			if (BHPS && BHPS->IsAliveSurvivor())
			{
				Students.Add(BHPS);
			}
		}
	}
	if (Students.IsEmpty())
	{
		return false;
	}

	Students.Sort([&TopicMastery](const ABHPlayerState& A, const ABHPlayerState& B)
	{
		if (A.RevisionStats.ContributionCount != B.RevisionStats.ContributionCount)
		{
			return A.RevisionStats.ContributionCount < B.RevisionStats.ContributionCount;
		}
		if (!FMath::IsNearlyEqual(TopicMastery(&A), TopicMastery(&B)))
		{
			return TopicMastery(&A) < TopicMastery(&B);
		}
		return A.GetPlayerId() < B.GetPlayerId();
	});

	const ABHPlayerState* RequestingPS = RequestingCharacter ? RequestingCharacter->GetPlayerState<ABHPlayerState>() : nullptr;
	const int32 TargetTeamSize = FMath::Clamp(GetRevisionAnswerTeamTargetSize(), 1, Students.Num());
	if (RequestingPS)
	{
		OutPlayerIds.Add(RequestingPS->GetPlayerId());
	}
	for (ABHPlayerState* Student : Students)
	{
		if (Student && OutPlayerIds.Num() < TargetTeamSize)
		{
			OutPlayerIds.Add(Student->GetPlayerId());
		}
	}

	TArray<FString> Names;
	for (ABHPlayerState* Student : Students)
	{
		if (Student && OutPlayerIds.Contains(Student->GetPlayerId()))
		{
			Names.Add(Student->GetPlayerName());
		}
	}
	OutSummary = FString::Join(Names, TEXT(", "));
	return OutPlayerIds.Num() > 0;
}

void ABHGameMode::SetPhysicsTopics(ABHPlayerController* RequestingController, const FString& TopicList)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change classroom topics")))
	{
		return;
	}

	RevisionTopicMask = ParsePhysicsTopicMask(TopicList, RevisionTopicMask);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
	}
	BroadcastStatus(FString::Printf(TEXT("Physics topics updated. Mask=%d."), RevisionTopicMask), 3.0f);
}

void ABHGameMode::SetRevisionDifficultyMix(ABHPlayerController* RequestingController, const FString& DifficultyMix)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change difficulty mix")))
	{
		return;
	}

	RevisionDifficultyMix = ParseRevisionDifficultyMix(DifficultyMix, RevisionDifficultyMix);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
	}
	BroadcastStatus(FString::Printf(TEXT("Revision difficulty mix: %s."), *RevisionDifficultyMixToString(RevisionDifficultyMix)), 3.0f);
}

void ABHGameMode::SetRevisionThresholds(ABHPlayerController* RequestingController, float NewClassThreshold, float NewIndividualThreshold)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change revision thresholds")))
	{
		return;
	}

	RevisionClassThreshold = FMath::Clamp(NewClassThreshold, 0.0f, 100.0f);
	RevisionIndividualThreshold = FMath::Clamp(NewIndividualThreshold, 0.0f, 100.0f);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
	}
	UpdateRevisionSummary(TEXT("Thresholds changed by classroom admin."));
	BroadcastStatus(FString::Printf(TEXT("Revision thresholds set: class %.0f%%, individual %.0f%%."), RevisionClassThreshold, RevisionIndividualThreshold), 3.5f);
}

void ABHGameMode::SetScareIntensity(ABHPlayerController* RequestingController, int32 NewScareIntensity)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change scare intensity")))
	{
		return;
	}

	RevisionScareIntensity = FMath::Clamp(NewScareIntensity, 0, 3);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
	}
	BroadcastStatus(FString::Printf(TEXT("Scare intensity set to %d."), RevisionScareIntensity), 3.0f);
}

void ABHGameMode::ForceReview(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("force review")))
	{
		return;
	}

	RevisionReviewTimeRemaining = 60;
	UpdateRevisionSummary(TEXT("Review forced: discuss weak topics, formulas, and corrected mistakes."));
	BroadcastStatus(TEXT("Physics review screen forced for 60 seconds."), 3.5f);
}

void ABHGameMode::ResetRevisionStats()
{
	if (!GameState || !bRevisionMode)
	{
		return;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		if (ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS))
		{
			BHPS->ResetRevisionStats();
		}
	}
	RevisionReviewTimeRemaining = 0;
	UpdateRevisionSummary();
}

FBHClassRevisionSummary ABHGameMode::ComputeRevisionSummary() const
{
	FBHClassRevisionSummary Summary;
	Summary.LowestStudentMastery = 100.0f;
	if (!GameState)
	{
		Summary.LowestStudentMastery = 0.0f;
		return Summary;
	}

	float MasterySum = 0.0f;
	float TopicSums[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	const int32 MinimumContributionTarget = GetRevisionMinimumContributionTarget();
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS || !BHPS->IsAliveSurvivor() || BHPS->IsABot())
		{
			continue;
		}
		++Summary.ActiveStudentCount;
		MasterySum += BHPS->RevisionStats.MasteryPercent;
		Summary.LowestStudentMastery = FMath::Min(Summary.LowestStudentMastery, BHPS->RevisionStats.MasteryPercent);
		if (BHPS->RevisionStats.MasteryPercent < RevisionIndividualThreshold)
		{
			++Summary.StudentsBelowIndividualThreshold;
		}
		if (BHPS->RevisionStats.ContributionCount < MinimumContributionTarget)
		{
			++Summary.StudentsWithoutContribution;
		}
		TopicSums[0] += BHPS->RevisionStats.ForcesMastery;
		TopicSums[1] += BHPS->RevisionStats.ElectricityMastery;
		TopicSums[2] += BHPS->RevisionStats.WavesMastery;
		TopicSums[3] += BHPS->RevisionStats.EnergyMastery;
	}

	if (Summary.ActiveStudentCount <= 0)
	{
		Summary.LowestStudentMastery = 0.0f;
		return Summary;
	}

	Summary.ClassMasteryAverage = MasterySum / Summary.ActiveStudentCount;
	int32 WeakIndex = 0;
	float WeakScore = TNumericLimits<float>::Max();
	for (int32 TopicIndex = 0; TopicIndex < 4; ++TopicIndex)
	{
		const EBHPhysicsTopic Topic = static_cast<EBHPhysicsTopic>(TopicIndex);
		if ((RevisionTopicMask & FBHRevisionQuestionBank::TopicMaskBit(Topic)) == 0)
		{
			continue;
		}
		const float TopicAverage = TopicSums[TopicIndex] / Summary.ActiveStudentCount;
		if (TopicAverage < WeakScore)
		{
			WeakScore = TopicAverage;
			WeakIndex = TopicIndex;
		}
	}
	Summary.WeakTopic = static_cast<EBHPhysicsTopic>(WeakIndex);
	return Summary;
}

void ABHGameMode::UpdateRevisionSummary(const FString& ReviewText)
{
	if (!bRevisionMode)
	{
		return;
	}

	const FBHClassRevisionSummary Summary = ComputeRevisionSummary();
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		const FString Text = ReviewText.IsEmpty()
			? FString::Printf(TEXT("Weak topic: %s. Class %.0f%%, lowest %.0f%%, below contribution target %d."),
				*FBHRevisionQuestionBank::TopicToString(Summary.WeakTopic),
				Summary.ClassMasteryAverage,
				Summary.LowestStudentMastery,
				Summary.StudentsWithoutContribution)
			: ReviewText;
		BHGS->SetRevisionSummary(Summary.ClassMasteryAverage, Summary.WeakTopic, RevisionReviewTimeRemaining, Text);
	}
}

bool ABHGameMode::CanUnlockRevisionExit(FString& OutReason) const
{
	if (!bRevisionMode)
	{
		return true;
	}

	const FBHClassRevisionSummary Summary = ComputeRevisionSummary();
	if (Summary.ActiveStudentCount <= 0)
	{
		OutReason = TEXT("Classroom exit locked: no active students.");
		return false;
	}
	if (Summary.ClassMasteryAverage < RevisionClassThreshold)
	{
		OutReason = FString::Printf(TEXT("Exit locked: class mastery %.0f%% needs %.0f%%."), Summary.ClassMasteryAverage, RevisionClassThreshold);
		return false;
	}
	if (Summary.StudentsBelowIndividualThreshold > 0)
	{
		OutReason = FString::Printf(TEXT("Exit locked: %d student(s) below %.0f%% mastery."), Summary.StudentsBelowIndividualThreshold, RevisionIndividualThreshold);
		return false;
	}
	if (Summary.StudentsWithoutContribution > 0)
	{
		OutReason = FString::Printf(TEXT("Exit locked: %d student(s) still need %d answer-team or correction contribution(s)."), Summary.StudentsWithoutContribution, GetRevisionMinimumContributionTarget());
		return false;
	}
	return true;
}

FString ABHGameMode::GetRevisionObjectiveText() const
{
	const FBHClassRevisionSummary Summary = ComputeRevisionSummary();
	return FString::Printf(TEXT("Physics Classroom: complete %d team nodes (%d questions each, %d-student teams). Escape needs class %.0f%%/%.0f%%, every student %.0f%%/%.0f%%, %d contribution(s) each. Weak topic: %s."),
		ActiveSideObjectiveCount,
		GetRevisionQuestionTargetPerNode(),
		GetRevisionAnswerTeamTargetSize(),
		Summary.ClassMasteryAverage,
		RevisionClassThreshold,
		Summary.LowestStudentMastery,
		RevisionIndividualThreshold,
		GetRevisionMinimumContributionTarget(),
		*FBHRevisionQuestionBank::TopicToString(Summary.WeakTopic));
}

EBHPhysicsTopic ABHGameMode::GetWeakestRevisionTopic() const
{
	return ComputeRevisionSummary().WeakTopic;
}

FString ABHGameMode::GetRevisionStatusReport() const
{
	const FBHClassRevisionSummary Summary = ComputeRevisionSummary();
	return FString::Printf(TEXT("RevisionStatus mode=%s class=%.0f threshold=%.0f lowest=%.0f individual=%.0f students=%d below=%d belowContrib=%d minContrib=%d questionsPerNode=%d teamSize=%d weak=%s mix=%s topicsMask=%d review=%ds scare=%d"),
		bRevisionMode ? TEXT("PhysicsClassroom") : TEXT("off"),
		Summary.ClassMasteryAverage,
		RevisionClassThreshold,
		Summary.LowestStudentMastery,
		RevisionIndividualThreshold,
		Summary.ActiveStudentCount,
		Summary.StudentsBelowIndividualThreshold,
		Summary.StudentsWithoutContribution,
		GetRevisionMinimumContributionTarget(),
		GetRevisionQuestionTargetPerNode(),
		GetRevisionAnswerTeamTargetSize(),
		*FBHRevisionQuestionBank::TopicToString(Summary.WeakTopic),
		*RevisionDifficultyMixToString(RevisionDifficultyMix),
		RevisionTopicMask,
		RevisionReviewTimeRemaining,
		RevisionScareIntensity);
}

bool ABHGameMode::IsBotMode() const
{
	return bBotMode;
}

EBHBotDifficulty ABHGameMode::GetBotDifficulty() const
{
	return BotDifficulty;
}

bool ABHGameMode::IsRuntimeNavigationReady() const
{
	return bRuntimeNavigationReady;
}

bool ABHGameMode::GetLatestBotNoiseLocation(float MaxAgeSeconds, FVector& OutLocation) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (LastBotNoiseTime < -1000.0f || Now - LastBotNoiseTime > MaxAgeSeconds)
	{
		return false;
	}

	OutLocation = LastBotNoiseLocation;
	return true;
}

FVector ABHGameMode::GetRandomBotPatrolPoint() const
{
	if (!ScarePoints.IsEmpty())
	{
		return ScarePoints[FMath::RandRange(0, ScarePoints.Num() - 1)];
	}
	if (!SurvivorSpawns.IsEmpty())
	{
		return SurvivorSpawns[FMath::RandRange(0, SurvivorSpawns.Num() - 1)];
	}
	return FVector::ZeroVector;
}

void ABHGameMode::ReportBotStimulus(EBHBotStimulusType Type, const FVector& Location, AActor* SourceActor, AActor* TargetActor, const FString& Reason, float Strength)
{
	if (!GetWorld())
	{
		return;
	}

	SweepExpiredBotTacticalState();

	FBHBotStimulus Stimulus;
	Stimulus.Type = Type;
	Stimulus.SourceActor = SourceActor;
	Stimulus.TargetActor = TargetActor;
	Stimulus.Location = Location;
	Stimulus.TimeSeconds = GetWorld()->GetTimeSeconds();
	Stimulus.Strength = FMath::Max(0.0f, Strength);
	Stimulus.Reason = Reason;
	BotWorldStimuli.Add(Stimulus);
	if (BotWorldStimuli.Num() > 80)
	{
		BotWorldStimuli.RemoveAt(0, BotWorldStimuli.Num() - 80, EAllowShrinking::No);
	}
}

bool ABHGameMode::GetBotWorldMemorySnapshot(FBHBotMemory& OutMemory, float MaxAgeSeconds) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	OutMemory = FBHBotMemory();
	for (int32 Index = BotWorldStimuli.Num() - 1; Index >= 0; --Index)
	{
		const FBHBotStimulus& Stimulus = BotWorldStimuli[Index];
		if (Now - Stimulus.TimeSeconds > MaxAgeSeconds)
		{
			continue;
		}

		OutMemory.RecentStimuli.Add(Stimulus);
		if (Stimulus.Type == EBHBotStimulusType::Noise && OutMemory.LastHeardTime < Stimulus.TimeSeconds)
		{
			OutMemory.LastHeardLocation = Stimulus.Location;
			OutMemory.LastHeardTime = Stimulus.TimeSeconds;
		}
		if (Stimulus.Type == EBHBotStimulusType::Sight)
		{
			ABHCharacter* TargetCharacter = Cast<ABHCharacter>(Stimulus.TargetActor.Get());
			const ABHPlayerState* TargetPS = TargetCharacter ? TargetCharacter->GetPlayerState<ABHPlayerState>() : nullptr;
			if (TargetPS && TargetPS->IsAliveSurvivor() && OutMemory.LastSeenSurvivorTime < Stimulus.TimeSeconds)
			{
				OutMemory.LastSeenSurvivor = TargetCharacter;
				OutMemory.LastSeenSurvivorLocation = Stimulus.Location;
				OutMemory.LastSeenSurvivorTime = Stimulus.TimeSeconds;
			}
			else if (TargetPS && TargetPS->IsAliveHunter() && OutMemory.LastSeenHunterTime < Stimulus.TimeSeconds)
			{
				OutMemory.LastSeenHunter = TargetCharacter;
				OutMemory.LastSeenHunterLocation = Stimulus.Location;
				OutMemory.LastSeenHunterTime = Stimulus.TimeSeconds;
			}
		}
	}

	OutMemory.ThreatPressure = FMath::Clamp(static_cast<float>(OutMemory.RecentStimuli.Num()) / 10.0f, 0.0f, 1.0f);
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (BHGS)
	{
		const int32 RemainingWork = FMath::Max(0, BHGS->BreakersRequired - BHGS->BreakersCompleted)
			+ FMath::Max(0, BHGS->SideObjectivesRequired - BHGS->SideObjectivesCompleted);
		OutMemory.ObjectivePressure = BHGS->bExitUnlocked ? 1.0f : FMath::Clamp(1.0f - static_cast<float>(RemainingWork) / 8.0f, 0.0f, 1.0f);
	}
	return !OutMemory.RecentStimuli.IsEmpty();
}

bool ABHGameMode::ClaimBotObjective(AController* Claimant, AActor* Target, EBHBotIntent Intent, float ClaimSeconds)
{
	if (!Claimant || !Target || !GetWorld() || !IsBotTargetStillUseful(Target))
	{
		return false;
	}

	SweepExpiredBotTacticalState();

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bExclusive = IsExclusiveBotClaimIntent(Intent);
	for (FBHBotObjectiveClaim& Claim : BotObjectiveClaims)
	{
		if (Claim.Target.Get() != Target || Claim.ExpireTimeSeconds <= Now)
		{
			continue;
		}
		if (Claim.Claimant.Get() == Claimant)
		{
			Claim.Intent = Intent;
			Claim.ExpireTimeSeconds = Now + FMath::Max(1.0f, ClaimSeconds);
			return true;
		}
		if (bExclusive)
		{
			return false;
		}
	}

	ReleaseBotObjective(Claimant);

	FBHBotObjectiveClaim NewClaim;
	NewClaim.Target = Target;
	NewClaim.Claimant = Claimant;
	NewClaim.Intent = Intent;
	NewClaim.ExpireTimeSeconds = Now + FMath::Max(1.0f, ClaimSeconds);
	BotObjectiveClaims.Add(NewClaim);
	return true;
}

void ABHGameMode::ReleaseBotObjective(AController* Claimant, AActor* Target)
{
	if (!Claimant)
	{
		return;
	}

	BotObjectiveClaims.RemoveAll([Claimant, Target](const FBHBotObjectiveClaim& Claim)
	{
		return Claim.Claimant.Get() == Claimant && (!Target || Claim.Target.Get() == Target);
	});
}

int32 ABHGameMode::CountBotClaimsForTarget(const AActor* Target) const
{
	if (!Target || !GetWorld())
	{
		return 0;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	int32 Count = 0;
	for (const FBHBotObjectiveClaim& Claim : BotObjectiveClaims)
	{
		if (Claim.Target.Get() == Target && Claim.ExpireTimeSeconds > Now)
		{
			++Count;
		}
	}
	return Count;
}

bool ABHGameMode::IsBotTargetOnCooldown(const AController* Claimant, const AActor* Target) const
{
	if (!Claimant || !Target || !GetWorld())
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	for (const FBHBotTargetCooldown& Cooldown : BotTargetCooldowns)
	{
		if (Cooldown.Claimant.Get() == Claimant && Cooldown.Target.Get() == Target && Cooldown.ExpireTimeSeconds > Now)
		{
			return true;
		}
	}
	return false;
}

void ABHGameMode::AddBotTargetCooldown(AController* Claimant, AActor* Target, float CooldownSeconds, const FString& Reason)
{
	if (!Claimant || !Target || !GetWorld())
	{
		return;
	}

	SweepExpiredBotTacticalState();
	ReleaseBotObjective(Claimant, Target);

	BotTargetCooldowns.RemoveAll([Claimant, Target](const FBHBotTargetCooldown& Cooldown)
	{
		return Cooldown.Claimant.Get() == Claimant && Cooldown.Target.Get() == Target;
	});

	FBHBotTargetCooldown Cooldown;
	Cooldown.Claimant = Claimant;
	Cooldown.Target = Target;
	Cooldown.ExpireTimeSeconds = GetWorld()->GetTimeSeconds() + FMath::Max(1.0f, CooldownSeconds);
	BotTargetCooldowns.Add(Cooldown);
	ReportBotStimulus(EBHBotStimulusType::Unreachable, Target->GetActorLocation(), Claimant->GetPawn(), Target, Reason, 0.5f);

	const FString LogKey = FString::Printf(TEXT("%s|%s"), *GetNameSafe(Target), *Reason);
	if (!LoggedBotTacticalWarnings.Contains(LogKey))
	{
		LoggedBotTacticalWarnings.Add(LogKey);
		UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt bot target cooldown: %s avoided for %.1fs after %s first reported %s"),
			*GetNameSafe(Target),
			CooldownSeconds,
			*GetNameSafe(Claimant),
			*Reason);
	}
}

bool ABHGameMode::GetBotApproachPoint(AActor* Target, const FVector& FromLocation, float DesiredDistance, FVector& OutLocation)
{
	if (!Target)
	{
		return false;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		OutLocation = Target->GetActorLocation();
		return true;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const float RadiusA = FMath::Clamp(DesiredDistance, 180.0f, 620.0f);
	const float Radii[] = { RadiusA, 320.0f, 480.0f, 650.0f, 820.0f };
	float BestScore = TNumericLimits<float>::Max();
	FVector BestLocation = FVector::ZeroVector;
	bool bFound = false;

	for (float Radius : Radii)
	{
		for (int32 Step = 0; Step < 16; ++Step)
		{
			const float Angle = static_cast<float>(Step) * (2.0f * PI / 16.0f);
			const FVector Candidate = TargetLocation + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
			FNavLocation Projected;
			if (!NavSystem->ProjectPointToNavigation(Candidate, Projected, FVector(260.0f, 260.0f, 280.0f)))
			{
				continue;
			}

			UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), FromLocation, Projected.Location);
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				continue;
			}

			float CrowdPenalty = 0.0f;
			for (TActorIterator<ABHCharacter> CharacterIt(GetWorld()); CharacterIt; ++CharacterIt)
			{
				const ABHCharacter* Character = *CharacterIt;
				const ABHPlayerState* CharacterPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
				if (!Character || !CharacterPS || CharacterPS->LifeState != EBHPlayerLifeState::Alive)
				{
					continue;
				}

				const FVector CharacterLocation = Character->GetActorLocation();
				if (FVector::DistSquared2D(CharacterLocation, FromLocation) <= FMath::Square(120.0f))
				{
					continue;
				}

				const float Separation = FVector::Dist2D(CharacterLocation, Projected.Location);
				if (Separation < 380.0f)
				{
					CrowdPenalty += FMath::Square(380.0f - Separation) * 80.0f;
				}
			}

			const float Score = FVector::DistSquared2D(FromLocation, Projected.Location)
				+ FMath::Abs(FVector::Dist2D(TargetLocation, Projected.Location) - RadiusA) * 100.0f
				+ CrowdPenalty;
			if (Score < BestScore)
			{
				BestScore = Score;
				BestLocation = Projected.Location;
				bFound = true;
			}
		}
	}

	if (bFound)
	{
		OutLocation = BestLocation;
		return true;
	}

	FNavLocation DirectProjected;
	if (NavSystem->ProjectPointToNavigation(TargetLocation, DirectProjected, FVector(320.0f, 320.0f, 300.0f)))
	{
		OutLocation = DirectProjected.Location;
		return true;
	}
	return false;
}

int32 ABHGameMode::RunBotNavCheck(FString& OutSummary)
{
	SweepExpiredBotTacticalState();

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		OutSummary = TEXT("Bot nav check failed: no navigation system.");
		return 0;
	}

	int32 Total = 0;
	int32 Failed = 0;
	TArray<FString> Failures;
	auto CheckPoint = [&](const FString& Label, const FVector& Location)
	{
		++Total;
		FNavLocation Projected;
		if (!NavSystem->ProjectPointToNavigation(Location, Projected, FVector(320.0f, 320.0f, 300.0f)))
		{
			++Failed;
			if (Failures.Num() < 10)
			{
				Failures.Add(Label);
			}
		}
	};
	auto CheckActor = [&](const FString& Label, AActor* Actor)
	{
		if (!Actor)
		{
			return;
		}
		++Total;
		FVector Approach = FVector::ZeroVector;
		if (!GetBotApproachPoint(Actor, Actor->GetActorLocation() + FVector(800.0f, 0.0f, 0.0f), 420.0f, Approach))
		{
			++Failed;
			if (Failures.Num() < 10)
			{
				Failures.Add(FString::Printf(TEXT("%s:%s"), *Label, *GetNameSafe(Actor)));
			}
		}
	};

	CheckPoint(TEXT("TeacherSpawn"), HunterSpawn);
	for (int32 Index = 0; Index < SurvivorSpawns.Num(); ++Index)
	{
		CheckPoint(FString::Printf(TEXT("SurvivorSpawn%d"), Index), SurvivorSpawns[Index]);
	}
	for (ABHObjectiveStation* Station : ObjectiveStations)
	{
		if (Station && Station->IsDirectorActive() && !Station->IsCompleted())
		{
			CheckActor(TEXT("Station"), Station);
		}
	}
	for (ABHBreaker* Breaker : BreakerActors)
	{
		if (Breaker && Breaker->IsDirectorActive() && !Breaker->IsRepaired())
		{
			CheckActor(TEXT("Breaker"), Breaker);
		}
	}
	for (ABHExitGate* ExitGate : ExitGates)
	{
		if (ExitGate && ExitGate->IsDirectorActive())
		{
			CheckActor(TEXT("Exit"), ExitGate);
		}
	}
	for (TActorIterator<ABHLocker> It(GetWorld()); It; ++It)
	{
		CheckActor(TEXT("Locker"), *It);
	}

	OutSummary = FString::Printf(TEXT("Bot nav check: %d/%d passed, failed=%d%s%s"),
		Total - Failed,
		Total,
		Failed,
		Failures.IsEmpty() ? TEXT("") : TEXT(" failures="),
		Failures.IsEmpty() ? TEXT("") : *FString::Join(Failures, TEXT(",")));
	UE_LOG(LogTemp, Log, TEXT("%s"), *OutSummary);
	return Failed;
}

FString ABHGameMode::GetBotStatusReport() const
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UBHBotPolicySubsystem* Policy = GameInstance ? GameInstance->GetSubsystem<UBHBotPolicySubsystem>() : nullptr;
	const FString PolicyStatus = Policy ? Policy->GetPolicyStatus() : TEXT("policy=unavailable");
	int32 BotHunterCount = 0;
	int32 BotSurvivorCount = 0;
	int32 BotFakeHunterCount = 0;
	for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
	{
		const ABHPlayerState* BotPS = Bot ? Bot->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!BotPS)
		{
			continue;
		}
		if (BotPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			++BotHunterCount;
		}
		else if (BotPS->PlayerRole == EBHPlayerRole::Survivor)
		{
			++BotSurvivorCount;
		}
		else if (BotPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			++BotFakeHunterCount;
		}
	}
	return FString::Printf(TEXT("BotStatus mode=%s target=%d active=%d botHunters=%d botSurvivors=%d botMonitors=%d difficulty=%s nav=%s claims=%d cooldowns=%d stimuli=%d map=%s %s"),
		bBotMode ? TEXT("on") : TEXT("off"),
		TargetBotCount,
		BotControllers.Num(),
		BotHunterCount,
		BotSurvivorCount,
		BotFakeHunterCount,
		*BotDifficultyToString(BotDifficulty),
		bRuntimeNavigationReady ? TEXT("ready") : TEXT("building"),
		BotObjectiveClaims.Num(),
		BotTargetCooldowns.Num(),
		BotWorldStimuli.Num(),
		*RuntimeLevelName,
		*PolicyStatus);
}

FString ABHGameMode::GetBotMemoryReport() const
{
	TArray<FString> Lines;
	Lines.Add(GetBotStatusReport());
	for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
	{
		if (Bot && Lines.Num() < 8)
		{
			Lines.Add(Bot->GetBotDebugLine());
		}
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	for (int32 Index = BotWorldStimuli.Num() - 1; Index >= 0 && Lines.Num() < 13; --Index)
	{
		const FBHBotStimulus& Stimulus = BotWorldStimuli[Index];
		Lines.Add(FString::Printf(TEXT("%s age=%.1f loc=(%.0f,%.0f,%.0f) src=%s target=%s reason=%s"),
			*BotStimulusToString(Stimulus.Type),
			Now - Stimulus.TimeSeconds,
			Stimulus.Location.X,
			Stimulus.Location.Y,
			Stimulus.Location.Z,
			*GetNameSafe(Stimulus.SourceActor.Get()),
			*GetNameSafe(Stimulus.TargetActor.Get()),
			*Stimulus.Reason));
	}
	return FString::Join(Lines, TEXT(" | "));
}

void ABHGameMode::ForceBotHunt(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("force bot hunt")))
	{
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS)
	{
		return;
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Hunt)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Bot hunt is already running."), 2.5f);
		}
		return;
	}

	if (!bBotMode)
	{
		bBotMode = true;
		TargetBotCount = FMath::Max(TargetBotCount, GetDefault<UBHGameSettings>()->DefaultBotCount);
		RefreshBotRoster(RequestingController);
	}

	StartHuntPhaseImmediately();
}

void ABHGameMode::StartBotSoak(ABHPlayerController* RequestingController, const FString& LevelName, int32 DurationSeconds, int32 NewBotCount)
{
	if (!RequireHostAdmin(RequestingController, TEXT("start bot soak")))
	{
		return;
	}

	const FString NormalizedLevel = NormalizeBHLevelName(LevelName);
	if (!RuntimeLevelName.Equals(NormalizedLevel, ESearchCase::IgnoreCase) && GetWorld())
	{
		const FString TravelURL = FString::Printf(TEXT("/Engine/Maps/Entry?listen?BHLevel=%s?BHFogPreset=%s?BHBotMode=1?BHBotCount=%d?BHBotDifficulty=%s?BHHuntSeconds=%d?BHForceHunt=1"),
			*NormalizedLevel,
			*FogPresetToString(NextFogPreset),
			FMath::Clamp(NewBotCount, 0, FMath::Max(0, MaxPlayers - 1)),
			*BotDifficultyToString(BotDifficulty),
			FMath::Clamp(DurationSeconds, 30, 3600));
		GetWorld()->ServerTravel(TravelURL);
		return;
	}

	HuntSeconds = FMath::Clamp(DurationSeconds, 30, 3600);
	bBotMode = true;
	TargetBotCount = FMath::Clamp(NewBotCount, 0, FMath::Max(0, MaxPlayers - 1));
	RefreshBotRoster(RequestingController);
	ForceBotHunt(RequestingController);
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Bot soak started: %s, %d seconds, %d bots."), *RuntimeLevelName, HuntSeconds, TargetBotCount), 4.0f);
	}
}

void ABHGameMode::SetPracticeRole(ABHPlayerController* RequestingController, EBHPlayerRole NewRole)
{
	if (!RequireHostAdmin(RequestingController, TEXT("switch Practice Lab roles")))
	{
		return;
	}

	if (!bPracticeMode)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Practice roles are only available in Practice Lab."), 3.0f);
		}
		return;
	}

	if (NewRole != EBHPlayerRole::Survivor && NewRole != EBHPlayerRole::Hunter && NewRole != EBHPlayerRole::FakeHunter)
	{
		NewRole = EBHPlayerRole::Survivor;
	}

	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	BHPS->SetRole(NewRole);
	BHPS->SetDesiredRole(NewRole);
	BHPS->SetLifeState(EBHPlayerLifeState::Alive);
	BHPS->SetReady(true);
	BHPS->SetHiddenInLocker(false);
	BHPS->SetFakeHunterEligible(false);
	RestartPlayer(RequestingController);

	FString RoleName = TEXT("Survivor");
	if (NewRole == EBHPlayerRole::Hunter)
	{
		RoleName = TEXT("Teacher");
	}
	else if (NewRole == EBHPlayerRole::FakeHunter)
	{
		RoleName = TEXT("Hall Monitor");
	}

	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Practice role: %s. No ready-up or match timer."), *RoleName), 3.5f);
}

void ABHGameMode::SetPracticeModifier(ABHPlayerController* RequestingController, EBHRoundModifier NewModifier)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change Practice Lab modifiers")))
	{
		return;
	}

	if (!bPracticeMode)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Practice modifiers are only available in Practice Lab."), 3.0f);
		}
		return;
	}

	PracticeRoundModifier = NewModifier;
	RefreshPracticeDirector(FString::Printf(TEXT("Practice modifier: %s."), *GetRoundModifierText(PracticeRoundModifier)));
}

void ABHGameMode::RefreshPracticeRound(ABHPlayerController* RequestingController)
{
	if (bTestMode)
	{
		if (!RequireHostAdmin(RequestingController, TEXT("refresh Test Round")))
		{
			return;
		}

		RefreshTestDirector(TEXT("Test round refreshed."));
		return;
	}

	if (!bPracticeMode)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Practice refresh is only available in Practice Lab."), 3.0f);
		}
		return;
	}

	if (!RequireHostAdmin(RequestingController, TEXT("refresh Practice Lab")))
	{
		return;
	}

	RefreshPracticeDirector(TEXT("Practice round refreshed."));
}

void ABHGameMode::TriggerPracticeJumpscare(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("trigger Practice Lab scares")))
	{
		return;
	}

	if (!bPracticeMode && !bTestMode)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Manual jumpscare testing is only available in Practice Lab or Test Round."), 3.0f);
		}
		return;
	}

	ABHCharacter* Target = RequestingController ? Cast<ABHCharacter>(RequestingController->GetPawn()) : nullptr;
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!Target || !BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Could not find an alive practice pawn for the jumpscare."), 3.0f);
		}
		return;
	}
	if (!GetWorld())
	{
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	FVector Forward = RequestingController ? RequestingController->GetControlRotation().Vector() : Target->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = Target->GetActorForwardVector().GetSafeNormal2D();
	}
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector Directions[] = { Forward, Right, -Right, -Forward };
	const float Distances[] = { 4300.0f, 3700.0f, 3100.0f, 2500.0f, 1900.0f };
	const float TargetFloorZ = TargetLocation.Z - Target->GetSimpleCollisionHalfHeight();
	const FVector EyeTarget = TargetLocation + FVector(0.0f, 0.0f, 95.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHPracticeMonsterLine), false, Target);
	FVector SpawnLocation = TargetLocation + Forward * 2400.0f;
	SpawnLocation.Z = TargetFloorZ + 4.0f;
	float BestScore = -1.0f;

	for (int32 DirectionIndex = 0; DirectionIndex < UE_ARRAY_COUNT(Directions); ++DirectionIndex)
	{
		const FVector Direction = Directions[DirectionIndex];
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		for (float Distance : Distances)
		{
			FVector Candidate = TargetLocation + Direction * Distance;
			Candidate.Z = TargetFloorZ + 4.0f;
			const FVector CandidateEye = Candidate + FVector(0.0f, 0.0f, 160.0f);

			FHitResult Hit;
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, EyeTarget, CandidateEye, ECC_Visibility, Params);
			float VisibleDistance = Distance;
			if (bBlocked)
			{
				VisibleDistance = FMath::Sqrt(FVector::DistSquared2D(TargetLocation, Hit.Location)) - 300.0f;
				if (VisibleDistance < 700.0f)
				{
					continue;
				}
				Candidate = TargetLocation + Direction * VisibleDistance;
				Candidate.Z = TargetFloorZ + 4.0f;
			}

			const float DirectionBonus = DirectionIndex == 0 ? 650.0f : (DirectionIndex == 3 ? -500.0f : 0.0f);
			const float ClearLineBonus = bBlocked ? 0.0f : 275.0f;
			const float Score = VisibleDistance + DirectionBonus + ClearLineBonus;
			if (Score > BestScore)
			{
				BestScore = Score;
				SpawnLocation = Candidate;
			}
		}
	}

	const FVector DirectionToTarget = (TargetLocation - SpawnLocation).GetSafeNormal2D();
	const FRotator SpawnRotation = DirectionToTarget.IsNearlyZero() ? Forward.Rotation() : DirectionToTarget.Rotation();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = RequestingController;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABHJumpscareMonster* Monster = GetWorld()->SpawnActor<ABHJumpscareMonster>(SpawnLocation, SpawnRotation, SpawnParams))
	{
		Monster->Configure(Target, 9300.0f, 8.6f, 3.05f);
		if (ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController()))
		{
			TargetPC->ClientSnapViewToFlatFocus(SpawnLocation + FVector(0.0f, 0.0f, 145.0f));
		}
		FreezeTargetForJumpscare(Target, 3.0f);
		CutLightsForJumpscare(TargetLocation, SpawnLocation, 0.0f, 10.25f);
		LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
		Target->AddFear(48.0f);
		Target->AddDread(46.0f);
	}
	else if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Could not spawn the visible monster model."), 3.0f);
		return;
	}
}

void ABHGameMode::TriggerTargetedJumpscare(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState)
{
	if (!RequestingController || !GetWorld())
	{
		return;
	}

	if (!RequireHostAdmin(RequestingController, TEXT("trigger targeted scares")))
	{
		return;
	}

	ABHCharacter* Target = FindCharacterForPlayerState(TargetPlayerState);
	if (!Target)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Could not find that player for a scare."), 2.75f);
		return;
	}

	TriggerMonsterChargeJumpscare(Target);
	if (ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController()))
	{
		TargetPC->ClientShowStatusMessage(TEXT("The Teacher chose you."), 2.75f);
	}
}

void ABHGameMode::TriggerHunterBlackout(const FVector& SourceLocation)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	TArray<ABHFlickerLight*> PoweredLights;
	for (ABHFlickerLight* Light : FlickerLights)
	{
		if (Light && Light->GetCircuitId() > 0 && Light->IsPowered())
		{
			PoweredLights.Add(Light);
		}
	}

	if (!PoweredLights.IsEmpty())
	{
		PoweredLights.Sort([&SourceLocation](const ABHFlickerLight& A, const ABHFlickerLight& B)
		{
			return FVector::DistSquared2D(A.GetActorLocation(), SourceLocation) < FVector::DistSquared2D(B.GetActorLocation(), SourceLocation);
		});

		const int32 Limit = FMath::Min(5, PoweredLights.Num());
		for (int32 Index = 0; Index < Limit; ++Index)
		{
			if (PoweredLights[Index])
			{
				PoweredLights[Index]->SetPowered(false);
			}
		}
	}

	SpawnAmbient(SourceLocation + FVector(0.0f, 0.0f, 80.0f), 90.0f, 0.32f, 0.22f, 5.0f, 5.5f);
	ApplyPresenceSpike(SourceLocation, 72.0f, TEXT("The dark moved with the Teacher."));
	BroadcastStatus(TEXT("The Teacher killed a bank of lights."), 3.5f);
}

void ABHGameMode::BuildRuntimeFacility()
{
	if (bFacilityBuilt || !GetWorld())
	{
		return;
	}

	bFacilityBuilt = true;
	BreakerActors.Reset();
	DoorActors.Reset();
	ExitGates.Reset();
	FlickerLights.Reset();
	ObjectiveStations.Reset();
	ScarePoints.Reset();
	const FString TestOption = GetWorld()->URL.GetOption(TEXT("BHTestMode="), TEXT(""));
	bTestMode = IsTrueOption(TestOption);
	const FString PracticeOption = GetWorld()->URL.GetOption(TEXT("BHPractice="), TEXT(""));
	bPracticeMode = !bTestMode && (PracticeOption.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| PracticeOption.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| PracticeOption.Equals(TEXT("Practice"), ESearchCase::IgnoreCase));
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const FString HuntSecondsOption = GetWorld()->URL.GetOption(TEXT("BHHuntSeconds="), TEXT(""));
	if (!HuntSecondsOption.IsEmpty())
	{
		HuntSeconds = FMath::Clamp(FCString::Atoi(*HuntSecondsOption), 30, 3600);
	}
	const FString RevisionModeOption = GetWorld()->URL.GetOption(TEXT("BHRevisionMode="), TEXT(""));
	bRevisionMode = !bPracticeMode && !bTestMode && IsTrueOption(RevisionModeOption);
	RevisionMode = bRevisionMode ? EBHRevisionMode::PhysicsClassroom : EBHRevisionMode::None;
	if (bRevisionMode)
	{
		const FString RevisionTopicsOption = GetWorld()->URL.GetOption(TEXT("BHRevisionTopics="), TEXT("All"));
		const FString RevisionMixOption = GetWorld()->URL.GetOption(TEXT("BHRevisionDifficultyMix="), TEXT("Adaptive"));
		const FString ClassThresholdOption = GetWorld()->URL.GetOption(TEXT("BHRevisionClassThreshold="), *FString::SanitizeFloat(Settings->RevisionClassThreshold));
		const FString IndividualThresholdOption = GetWorld()->URL.GetOption(TEXT("BHRevisionIndividualThreshold="), *FString::SanitizeFloat(Settings->RevisionIndividualThreshold));
		const FString ScareIntensityOption = GetWorld()->URL.GetOption(TEXT("BHScareIntensity="), *FString::FromInt(Settings->RevisionScareIntensity));
		RevisionTopicMask = ParsePhysicsTopicMask(RevisionTopicsOption, 0x0F);
		RevisionDifficultyMix = ParseRevisionDifficultyMix(RevisionMixOption, EBHRevisionDifficultyMix::Adaptive);
		RevisionClassThreshold = FMath::Clamp(FCString::Atof(*ClassThresholdOption), 0.0f, 100.0f);
		RevisionIndividualThreshold = FMath::Clamp(FCString::Atof(*IndividualThresholdOption), 0.0f, 100.0f);
		RevisionRoundDuration = FMath::Clamp(Settings->RevisionRoundSeconds, 60, 3600);
		HuntSeconds = FMath::Clamp(HuntSecondsOption.IsEmpty() ? Settings->RevisionRoundSeconds : HuntSeconds, 60, 3600);
		RevisionRoundDuration = HuntSeconds;
		RevisionScareIntensity = FMath::Clamp(FCString::Atoi(*ScareIntensityOption), 0, 3);
		RevisionReviewTimeRemaining = 0;
		ObjectiveIntensity = 4;
	}
	const FString BotModeOption = GetWorld()->URL.GetOption(TEXT("BHBotMode="), TEXT(""));
	bBotMode = !bPracticeMode && !bTestMode && !bRevisionMode && IsTrueOption(BotModeOption);
	if (bBotMode)
	{
		const FString BotCountOption = GetWorld()->URL.GetOption(TEXT("BHBotCount="), *FString::FromInt(Settings->DefaultBotCount));
		const FString BotDifficultyOption = GetWorld()->URL.GetOption(TEXT("BHBotDifficulty="), *BotDifficultyToString(Settings->DefaultBotDifficulty));
		TargetBotCount = FMath::Clamp(FCString::Atoi(*BotCountOption), 0, FMath::Max(0, MaxPlayers - 1));
		BotDifficulty = ParseBotDifficulty(BotDifficultyOption, Settings->DefaultBotDifficulty);
	}
	else
	{
		TargetBotCount = 0;
		BotDifficulty = Settings->DefaultBotDifficulty;
	}
	RuntimeLevelName = GetWorld()->URL.GetOption(TEXT("BHLevel="), TEXT(""));
	if (RuntimeLevelName.IsEmpty())
	{
		RuntimeLevelName = TEXT("Facility");
	}
	RuntimeLevelName = NormalizeBHLevelName(RuntimeLevelName);
	NextRuntimeLevelName = RuntimeLevelName;
	const FString FogPresetOption = GetWorld()->URL.GetOption(TEXT("BHFogPreset="), TEXT("Heavy"));
	RuntimeFogPreset = ParseFogPreset(FogPresetOption, EBHFogPreset::Heavy);
	NextFogPreset = RuntimeFogPreset;
	const FString FogOverrideOption = GetWorld()->URL.GetOption(TEXT("BHFogOverride="), TEXT(""));
	bFogPresetOverride = IsTrueOption(FogOverrideOption);

	if (RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
	{
		RuntimeLevelName = TEXT("Substation");
		NextRuntimeLevelName = RuntimeLevelName;
		BuildSubstationLevel();
		return;
	}
	if (RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		RuntimeLevelName = TEXT("Foggrounds");
		NextRuntimeLevelName = RuntimeLevelName;
		BuildFoggroundsLevel();
		return;
	}

	RuntimeLevelName = TEXT("Facility");
	NextRuntimeLevelName = RuntimeLevelName;
	SurvivorSpawns = {
		FVector(3450.0f, -2800.0f, 120.0f),
		FVector(3800.0f, -2350.0f, 120.0f),
		FVector(3450.0f, 2700.0f, 120.0f),
		FVector(3650.0f, 2200.0f, 120.0f),
		FVector(900.0f, 3000.0f, 120.0f),
		FVector(900.0f, -3050.0f, 120.0f),
		FVector(-450.0f, 2850.0f, 120.0f),
		FVector(-450.0f, -2850.0f, 120.0f),
		FVector(2400.0f, 3150.0f, 120.0f),
		FVector(2400.0f, -3150.0f, 120.0f),
		FVector(-2350.0f, 3150.0f, 120.0f),
		FVector(-2350.0f, -3150.0f, 120.0f)
	};

	const FLinearColor FloorTint(0.12f, 0.14f, 0.15f, 1.0f);
	const FLinearColor CeilingTint(0.06f, 0.07f, 0.08f, 1.0f);
	const FLinearColor WallTint(0.28f, 0.31f, 0.33f, 1.0f);
	const FLinearColor UtilityTint(0.19f, 0.23f, 0.25f, 1.0f);
	const FLinearColor StorageTint(0.30f, 0.27f, 0.19f, 1.0f);
	const FLinearColor LabTint(0.16f, 0.26f, 0.30f, 1.0f);
	const FLinearColor WardTint(0.22f, 0.18f, 0.24f, 1.0f);
	const FLinearColor HazardTint(0.40f, 0.08f, 0.06f, 1.0f);

	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.25f)), FVector(110.0f, 88.0f, 0.25f), FloorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(335.0f, 0.12f)), FVector(110.0f, 88.0f, 0.12f), CeilingTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	SpawnBlock(FVector(0.0f, -4400.0f, CenterZForBlockBottom(0.0f, 3.35f)), FVector(110.0f, 0.35f, 3.35f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(0.0f, 4400.0f, CenterZForBlockBottom(0.0f, 3.35f)), FVector(110.0f, 0.35f, 3.35f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(-5500.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.35f)), FVector(0.35f, 88.0f, 3.35f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(5500.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.35f)), FVector(0.35f, 88.0f, 3.35f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	const TArray<TPair<FVector, FVector>> InternalWalls = {
		{FVector(-3000.0f, -2400.0f, 165.0f), FVector(26.0f, 0.28f, 3.1f)},
		{FVector(650.0f, -2400.0f, 165.0f), FVector(15.0f, 0.28f, 3.1f)},
		{FVector(3300.0f, -2400.0f, 165.0f), FVector(18.0f, 0.28f, 3.1f)},
		{FVector(-3300.0f, -1200.0f, 165.0f), FVector(22.0f, 0.28f, 3.1f)},
		{FVector(0.0f, -1200.0f, 165.0f), FVector(18.0f, 0.28f, 3.1f)},
		{FVector(3200.0f, -1200.0f, 165.0f), FVector(20.0f, 0.28f, 3.1f)},
		{FVector(-3300.0f, 1200.0f, 165.0f), FVector(20.0f, 0.28f, 3.1f)},
		{FVector(0.0f, 1200.0f, 165.0f), FVector(18.0f, 0.28f, 3.1f)},
		{FVector(3300.0f, 1200.0f, 165.0f), FVector(20.0f, 0.28f, 3.1f)},
		{FVector(-2800.0f, 2400.0f, 165.0f), FVector(26.0f, 0.28f, 3.1f)},
		{FVector(1200.0f, 2400.0f, 165.0f), FVector(24.0f, 0.28f, 3.1f)},
		{FVector(-3000.0f, -2850.0f, 165.0f), FVector(0.28f, 15.0f, 3.1f)},
		{FVector(-3000.0f, -450.0f, 165.0f), FVector(0.28f, 9.0f, 3.1f)},
		{FVector(-3000.0f, 1850.0f, 165.0f), FVector(0.28f, 18.0f, 3.1f)},
		{FVector(-1500.0f, -2550.0f, 165.0f), FVector(0.28f, 17.0f, 3.1f)},
		{FVector(-1500.0f, 300.0f, 165.0f), FVector(0.28f, 12.0f, 3.1f)},
		{FVector(-1500.0f, 2850.0f, 165.0f), FVector(0.28f, 11.0f, 3.1f)},
		{FVector(0.0f, -2900.0f, 165.0f), FVector(0.28f, 12.0f, 3.1f)},
		{FVector(0.0f, 1750.0f, 165.0f), FVector(0.28f, 15.0f, 3.1f)},
		{FVector(1500.0f, -2200.0f, 165.0f), FVector(0.28f, 20.0f, 3.1f)},
		{FVector(1500.0f, 200.0f, 165.0f), FVector(0.28f, 12.0f, 3.1f)},
		{FVector(1500.0f, 2600.0f, 165.0f), FVector(0.28f, 14.0f, 3.1f)},
		{FVector(3000.0f, -2700.0f, 165.0f), FVector(0.28f, 14.0f, 3.1f)},
		{FVector(3000.0f, -200.0f, 165.0f), FVector(0.28f, 14.0f, 3.1f)},
		{FVector(3000.0f, 2300.0f, 165.0f), FVector(0.28f, 16.0f, 3.1f)},
		{FVector(-5000.0f, -3200.0f, 165.0f), FVector(0.28f, 18.0f, 3.1f)},
		{FVector(-4650.0f, -3550.0f, 165.0f), FVector(12.0f, 0.28f, 3.1f)},
		{FVector(-4650.0f, -1850.0f, 165.0f), FVector(13.0f, 0.28f, 3.1f)},
		{FVector(-5000.0f, 2350.0f, 165.0f), FVector(0.28f, 22.0f, 3.1f)},
		{FVector(-4550.0f, 3500.0f, 165.0f), FVector(15.0f, 0.28f, 3.1f)},
		{FVector(-4550.0f, 1750.0f, 165.0f), FVector(12.0f, 0.28f, 3.1f)},
		{FVector(5000.0f, -2700.0f, 165.0f), FVector(0.28f, 20.0f, 3.1f)},
		{FVector(4550.0f, -3550.0f, 165.0f), FVector(15.0f, 0.28f, 3.1f)},
		{FVector(4700.0f, -1650.0f, 165.0f), FVector(12.0f, 0.28f, 3.1f)},
		{FVector(5000.0f, 2450.0f, 165.0f), FVector(0.28f, 18.0f, 3.1f)},
		{FVector(4550.0f, 3550.0f, 165.0f), FVector(15.0f, 0.28f, 3.1f)},
		{FVector(4700.0f, 1700.0f, 165.0f), FVector(12.0f, 0.28f, 3.1f)}
	};

	for (const TPair<FVector, FVector>& Wall : InternalWalls)
	{
		SpawnBlock(FVector(Wall.Key.X, Wall.Key.Y, CenterZForBlockBottom(0.0f, Wall.Value.Z)), Wall.Value, WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	}

	auto SpawnDoorAt = [this](const FVector& Location, const FRotator& Rotation)
	{
		if (ABHDoor* Door = GetWorld()->SpawnActor<ABHDoor>(Location, Rotation))
		{
			DoorActors.Add(Door);
		}
	};

	SpawnDoorAt(FVector(-3000.0f, -1300.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-1500.0f, -1200.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(1350.0f, -1200.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(3000.0f, -1350.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-1500.0f, 1200.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(1500.0f, 1200.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(3000.0f, 800.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-3000.0f, 800.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(0.0f, -2400.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(0.0f, 2400.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(-5000.0f, -2450.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-5000.0f, 2950.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(5000.0f, -3000.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(5000.0f, 2950.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));

	const TArray<TPair<FVector, FRotator>> Breakers = {
		{FVector(-3950.0f, -2850.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(3850.0f, -2750.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)},
		{FVector(-3750.0f, 2800.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(550.0f, -3180.0f, 80.0f), FRotator(0.0f, 180.0f, 0.0f)},
		{FVector(3650.0f, 2450.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)},
		{FVector(-5200.0f, -3850.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(5150.0f, 3750.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}
	};

	for (const TPair<FVector, FRotator>& Breaker : Breakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	const TArray<TPair<FVector, EBHObjectiveStationType>> ObjectiveStationSpecs = {
		{FVector(-4200.0f, -500.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(4200.0f, 1000.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-2400.0f, 3050.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(2450.0f, 3050.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-300.0f, -3050.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-5200.0f, 950.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(5200.0f, -950.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-2150.0f, -720.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-2150.0f, 720.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-650.0f, -720.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-650.0f, 720.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(850.0f, -720.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(850.0f, 720.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(2300.0f, -720.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(2300.0f, 720.0f, 95.0f), EBHObjectiveStationType::Evidence}
	};
	for (const TPair<FVector, EBHObjectiveStationType>& Spec : ObjectiveStationSpecs)
	{
		if (ABHObjectiveStation* Station = GetWorld()->SpawnActor<ABHObjectiveStation>(Spec.Key, FRotator::ZeroRotator))
		{
			Station->Configure(Spec.Value);
			ObjectiveStations.Add(Station);
		}
	}

	const FLinearColor RevisionBlue(0.16f, 0.58f, 0.88f, 1.0f);
	const FLinearColor RevisionYellow(0.95f, 0.78f, 0.18f, 1.0f);
	const FLinearColor RevisionGreen(0.10f, 0.72f, 0.48f, 1.0f);
	const FVector RevisionHubCenters[] = {
		FVector(-2150.0f, 0.0f, CenterZForBlockBottom(0.65f, 0.035f)),
		FVector(-650.0f, 0.0f, CenterZForBlockBottom(0.70f, 0.035f)),
		FVector(850.0f, 0.0f, CenterZForBlockBottom(0.75f, 0.035f)),
		FVector(2300.0f, 0.0f, CenterZForBlockBottom(0.80f, 0.035f))
	};
	for (int32 HubIndex = 0; HubIndex < UE_ARRAY_COUNT(RevisionHubCenters); ++HubIndex)
	{
		const FVector Hub = RevisionHubCenters[HubIndex];
		const FLinearColor HubTint = (HubIndex % 3 == 0) ? RevisionBlue : ((HubIndex % 3 == 1) ? RevisionYellow : RevisionGreen);
		SpawnBlock(Hub, FVector(4.6f, 2.2f, 0.035f), HubTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
		SpawnBlock(Hub + FVector(0.0f, -150.0f, 1.0f), FVector(4.2f, 0.08f, 0.045f), RevisionYellow, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(Hub + FVector(0.0f, 150.0f, 1.0f), FVector(4.2f, 0.08f, 0.045f), RevisionYellow, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	}
	if (ABHObjectiveStation* HiddenSwitch = GetWorld()->SpawnActor<ABHObjectiveStation>(FVector(-5125.0f, -3825.0f, 88.0f), FRotator(0.0f, 35.0f, 0.0f)))
	{
		HiddenSwitch->ConfigureTeacherMirrorTrapNode();
	}

	for (int32 ShelfIndex = 0; ShelfIndex < 5; ++ShelfIndex)
	{
		SpawnBlock(FVector(-3900.0f + ShelfIndex * 330.0f, -3050.0f, 75.0f), FVector(0.35f, 2.9f, 1.55f), StorageTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(FVector(-3920.0f + ShelfIndex * 350.0f, -2680.0f, 75.0f), FVector(2.5f, 0.35f, 1.35f), StorageTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
	}

	for (int32 BenchIndex = 0; BenchIndex < 5; ++BenchIndex)
	{
		SpawnBlock(FVector(3250.0f + (BenchIndex % 2) * 520.0f, -3200.0f + BenchIndex * 260.0f, 20.0f), FVector(2.8f, 0.9f, 0.55f), LabTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	}

	for (int32 BedIndex = 0; BedIndex < 7; ++BedIndex)
	{
		const float X = -4000.0f + (BedIndex % 3) * 520.0f;
		const float Y = 2200.0f + (BedIndex / 3) * 420.0f;
		SpawnBlock(FVector(X, Y, 20.0f), FVector(2.6f, 1.1f, 0.45f), WardTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(X + 170.0f, Y - 70.0f, 105.0f), FVector(0.12f, 1.25f, 1.65f), WardTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	}

	const FVector Crates[] = {
		FVector(-600.0f, -3250.0f, 40.0f), FVector(-250.0f, -3050.0f, 40.0f), FVector(150.0f, -3350.0f, 40.0f),
		FVector(1800.0f, -3250.0f, 40.0f), FVector(2200.0f, -2850.0f, 40.0f), FVector(2600.0f, -3200.0f, 40.0f),
		FVector(-750.0f, 3200.0f, 40.0f), FVector(-250.0f, 2920.0f, 40.0f), FVector(250.0f, 3250.0f, 40.0f),
		FVector(2100.0f, 1800.0f, 40.0f), FVector(2450.0f, 2150.0f, 40.0f), FVector(2150.0f, 2600.0f, 40.0f)
	};
	for (const FVector& Crate : Crates)
	{
		SpawnBlock(Crate, FVector(0.85f, 0.85f, 0.85f), UtilityTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	}

	const FVector Pillars[] = {
		FVector(-900.0f, -900.0f, 120.0f), FVector(900.0f, -900.0f, 120.0f), FVector(-900.0f, 900.0f, 120.0f), FVector(900.0f, 900.0f, 120.0f),
		FVector(-4200.0f, 0.0f, 120.0f), FVector(4200.0f, 0.0f, 120.0f), FVector(0.0f, -3400.0f, 120.0f), FVector(0.0f, 3400.0f, 120.0f)
	};
	for (const FVector& Pillar : Pillars)
	{
		SpawnBlock(Pillar, FVector(0.75f, 0.75f, 2.4f), UtilityTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	}

	const FVector LockerLocations[] = {
		FVector(-4200.0f, -3150.0f, 100.0f), FVector(-3450.0f, -3150.0f, 100.0f), FVector(-2350.0f, -3150.0f, 100.0f),
		FVector(3200.0f, -3320.0f, 100.0f), FVector(3850.0f, -3320.0f, 100.0f), FVector(4100.0f, -1850.0f, 100.0f),
		FVector(-4200.0f, 3150.0f, 100.0f), FVector(-3600.0f, 3150.0f, 100.0f), FVector(-2650.0f, 3150.0f, 100.0f),
		FVector(3600.0f, 3150.0f, 100.0f), FVector(4200.0f, 2700.0f, 100.0f), FVector(4200.0f, 1850.0f, 100.0f),
		FVector(-900.0f, -3300.0f, 100.0f), FVector(300.0f, -3300.0f, 100.0f), FVector(900.0f, -2100.0f, 100.0f),
		FVector(-900.0f, 3300.0f, 100.0f), FVector(300.0f, 3300.0f, 100.0f), FVector(900.0f, 2100.0f, 100.0f),
		FVector(-4300.0f, 500.0f, 100.0f), FVector(-4300.0f, -500.0f, 100.0f), FVector(4300.0f, 500.0f, 100.0f), FVector(4300.0f, -500.0f, 100.0f),
		FVector(-5200.0f, -3900.0f, 100.0f), FVector(-5200.0f, -1600.0f, 100.0f), FVector(-5200.0f, 1500.0f, 100.0f), FVector(-5200.0f, 3900.0f, 100.0f),
		FVector(5200.0f, -3900.0f, 100.0f), FVector(5200.0f, -1500.0f, 100.0f), FVector(5200.0f, 1500.0f, 100.0f), FVector(5200.0f, 3900.0f, 100.0f)
	};

	for (const FVector& Location : LockerLocations)
	{
		GetWorld()->SpawnActor<ABHLocker>(Location, FRotator::ZeroRotator);
	}

	const FVector BatteryLocations[] = {
		FVector(-3600.0f, -2000.0f, 70.0f), FVector(3900.0f, -1900.0f, 70.0f), FVector(-3900.0f, 1700.0f, 70.0f),
		FVector(3650.0f, 1500.0f, 70.0f), FVector(-650.0f, -2750.0f, 70.0f), FVector(550.0f, 2750.0f, 70.0f),
		FVector(1900.0f, 0.0f, 70.0f), FVector(-1950.0f, 0.0f, 70.0f),
		FVector(-5050.0f, -3550.0f, 70.0f), FVector(-5050.0f, 3450.0f, 70.0f),
		FVector(5050.0f, -3450.0f, 70.0f), FVector(5050.0f, 3550.0f, 70.0f)
	};
	for (const FVector& Location : BatteryLocations)
	{
		GetWorld()->SpawnActor<ABHBatteryPickup>(Location, FRotator(0.0f, 90.0f, 0.0f));
	}

	const FVector AlarmLocations[] = {
		FVector(-4100.0f, -1150.0f, 105.0f), FVector(-4050.0f, 1150.0f, 105.0f),
		FVector(-950.0f, -2350.0f, 105.0f), FVector(950.0f, 2350.0f, 105.0f),
		FVector(4100.0f, -1150.0f, 105.0f), FVector(4050.0f, 1150.0f, 105.0f)
	};
	for (const FVector& Location : AlarmLocations)
	{
		GetWorld()->SpawnActor<ABHPanicAlarm>(Location, FRotator::ZeroRotator);
	}

	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FVector(4450.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FVector(-5450.0f, 0.0f, 120.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	SpawnBlock(FVector(4428.0f, -520.0f, 165.0f), FVector(0.04f, 1.25f, 0.65f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(4428.0f, 520.0f, 165.0f), FVector(0.04f, 1.25f, 0.65f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(-5428.0f, -520.0f, 165.0f), FVector(0.04f, 1.25f, 0.65f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(-5428.0f, 520.0f, 165.0f), FVector(0.04f, 1.25f, 0.65f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(-3600.0f, -3558.0f, 165.0f), FVector(1.25f, 0.04f, 0.65f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(3600.0f, -3558.0f, 165.0f), FVector(1.25f, 0.04f, 0.65f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	const FLinearColor ExitGreen(0.05f, 0.84f, 0.42f, 1.0f);
	const FLinearColor ExitAmber(0.98f, 0.76f, 0.12f, 1.0f);
	const FLinearColor ExitBlue(0.10f, 0.52f, 0.92f, 1.0f);
	const float ExitFloorZ = CenterZForBlockBottom(0.72f, 0.045f);
	const float FacilityExitCenters[] = {4975.0f, -4975.0f};
	const float FacilityGateXs[] = {4450.0f, -5450.0f};
	for (int32 ExitIndex = 0; ExitIndex < UE_ARRAY_COUNT(FacilityExitCenters); ++ExitIndex)
	{
		const float CenterX = FacilityExitCenters[ExitIndex];
		const float GateX = FacilityGateXs[ExitIndex];
		const float Direction = CenterX > 0.0f ? 1.0f : -1.0f;
		SpawnBlock(FVector(CenterX, 0.0f, ExitFloorZ), FVector(7.4f, 2.25f, 0.045f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
		SpawnBlock(FVector(CenterX, -275.0f, ExitFloorZ + 1.5f), FVector(7.0f, 0.08f, 0.055f), ExitAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(FVector(CenterX, 275.0f, ExitFloorZ + 1.5f), FVector(7.0f, 0.08f, 0.055f), ExitAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(FVector(GateX, 0.0f, 252.0f), FVector(0.08f, 2.45f, 0.16f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GateX, 0.0f, 68.0f), FVector(0.08f, 2.45f, 0.12f), ExitAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(FVector(GateX - Direction * 420.0f, -350.0f, 92.0f), FVector(0.16f, 0.16f, 1.25f), ExitBlue, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GateX - Direction * 420.0f, 350.0f, 92.0f), FVector(0.16f, 0.16f, 1.25f), ExitBlue, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	}

	const struct FLightSpec { FVector Location; int32 Circuit; FLinearColor Color; float Intensity; } Lights[] = {
		{FVector(-3650.0f, -2850.0f, 275.0f), 1, FLinearColor(0.95f, 0.75f, 0.45f, 1.0f), 1050.0f},
		{FVector(-2200.0f, -3000.0f, 275.0f), 1, FLinearColor(0.95f, 0.75f, 0.45f, 1.0f), 900.0f},
		{FVector(0.0f, -3000.0f, 275.0f), 2, FLinearColor(0.45f, 0.72f, 1.0f, 1.0f), 850.0f},
		{FVector(1800.0f, -3000.0f, 275.0f), 2, FLinearColor(0.45f, 0.72f, 1.0f, 1.0f), 850.0f},
		{FVector(3650.0f, -2850.0f, 275.0f), 3, FLinearColor(0.62f, 0.98f, 1.0f, 1.0f), 1000.0f},
		{FVector(3750.0f, -1500.0f, 275.0f), 3, FLinearColor(0.62f, 0.98f, 1.0f, 1.0f), 900.0f},
		{FVector(-3600.0f, 2850.0f, 275.0f), 4, FLinearColor(0.85f, 0.58f, 0.95f, 1.0f), 900.0f},
		{FVector(-2200.0f, 2850.0f, 275.0f), 4, FLinearColor(0.85f, 0.58f, 0.95f, 1.0f), 850.0f},
		{FVector(0.0f, 3000.0f, 275.0f), 5, FLinearColor(0.55f, 0.9f, 0.7f, 1.0f), 900.0f},
		{FVector(1800.0f, 2800.0f, 275.0f), 5, FLinearColor(0.55f, 0.9f, 0.7f, 1.0f), 850.0f},
		{FVector(3650.0f, 2600.0f, 275.0f), 6, FLinearColor(1.0f, 0.28f, 0.18f, 1.0f), 750.0f},
		{FVector(-900.0f, 0.0f, 275.0f), 0, FLinearColor(0.60f, 0.80f, 1.0f, 1.0f), 650.0f},
		{FVector(900.0f, 0.0f, 275.0f), 0, FLinearColor(0.60f, 0.80f, 1.0f, 1.0f), 650.0f},
		{FVector(3150.0f, 0.0f, 275.0f), 6, FLinearColor(1.0f, 0.22f, 0.16f, 1.0f), 650.0f}
	};

	for (const FLightSpec& Spec : Lights)
	{
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(Spec.Location, FRotator::ZeroRotator))
		{
			Light->Configure(Spec.Circuit, Spec.Color, Spec.Intensity, 1050.0f);
			FlickerLights.Add(Light);
		}
	}

	const TArray<TPair<FVector, int32>> Switches = {
		{FVector(-4300.0f, -2350.0f, 120.0f), 1},
		{FVector(-200.0f, -3450.0f, 120.0f), 2},
		{FVector(4300.0f, -2350.0f, 120.0f), 3},
		{FVector(-4300.0f, 2350.0f, 120.0f), 4},
		{FVector(0.0f, 3450.0f, 120.0f), 5},
		{FVector(4300.0f, 1800.0f, 120.0f), 6}
	};

	for (const TPair<FVector, int32>& Switch : Switches)
	{
		if (ABHPowerSwitch* PowerSwitch = GetWorld()->SpawnActor<ABHPowerSwitch>(Switch.Key, FRotator::ZeroRotator))
		{
			PowerSwitch->Configure(Switch.Value, FText::FromString(FString::Printf(TEXT("Toggle Circuit %d"), Switch.Value)));
		}
	}

	const TArray<TPair<FVector, FRotator>> Shutters = {
		{FVector(-1500.0f, 0.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(1500.0f, 0.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(0.0f, -1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(0.0f, 1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}
	};

	for (const TPair<FVector, FRotator>& Shutter : Shutters)
	{
		if (ABHSecurityShutter* SecurityShutter = GetWorld()->SpawnActor<ABHSecurityShutter>(Shutter.Key, Shutter.Value))
		{
			SecurityShutter->Configure(100);
		}
	}

	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(-4200.0f, 0.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		Terminal->Configure(100, FText::FromString(TEXT("Open Central Shutters")));
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(4200.0f, 1000.0f, 110.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		Terminal->Configure(100, FText::FromString(TEXT("Open Central Shutters")));
	}

	SpawnAmbient(FVector(-3800.0f, -3000.0f, 160.0f), 42.0f, 0.18f, 0.04f, 0.16f);
	SpawnAmbient(FVector(3850.0f, -2850.0f, 160.0f), 68.0f, 0.14f, 0.025f, 0.30f);
	SpawnAmbient(FVector(-3600.0f, 2850.0f, 160.0f), 36.0f, 0.11f, 0.03f, 0.20f);
	SpawnAmbient(FVector(3600.0f, 2450.0f, 160.0f), 88.0f, 0.10f, 0.02f, 0.45f);
	SpawnAmbient(FVector(0.0f, -3150.0f, 160.0f), 52.0f, 0.12f, 0.04f, 0.18f);
	SpawnAmbient(FVector(4300.0f, 0.0f, 160.0f), 30.0f, 0.16f, 0.05f, 0.10f);

	ScarePoints.Append({
		FVector(-3950.0f, -2850.0f, 110.0f),
		FVector(3850.0f, -2750.0f, 110.0f),
		FVector(-3750.0f, 2800.0f, 110.0f),
		FVector(3650.0f, 2450.0f, 110.0f),
		FVector(-900.0f, -3300.0f, 110.0f),
		FVector(900.0f, 3300.0f, 110.0f),
		FVector(-4300.0f, 500.0f, 110.0f),
		FVector(4300.0f, -500.0f, 110.0f),
		FVector(0.0f, -1200.0f, 110.0f),
		FVector(0.0f, 1200.0f, 110.0f),
		FVector(-5150.0f, -3600.0f, 110.0f),
		FVector(-5150.0f, 3600.0f, 110.0f),
		FVector(5150.0f, -3600.0f, 110.0f),
		FVector(5150.0f, 3600.0f, 110.0f),
		FVector(-5150.0f, 0.0f, 110.0f),
		FVector(5150.0f, 0.0f, 110.0f)
	});

	AddMoodPass(FLinearColor(0.020f, 0.032f, 0.040f, 1.0f), 0.014f, 0.55f, 0.14f);
	AddFacilityDetailPass();
	AddClassroomHorrorPass();
	BuildRuntimeNavigation();
}

void ABHGameMode::BuildFoggroundsLevel()
{
	SurvivorSpawns = {
		FVector(7100.0f, -5350.0f, 120.0f), FVector(6550.0f, -5350.0f, 120.0f), FVector(6000.0f, -5350.0f, 120.0f),
		FVector(7100.0f, -4850.0f, 120.0f), FVector(6550.0f, -4850.0f, 120.0f), FVector(6000.0f, -4850.0f, 120.0f),
		FVector(7100.0f, -4300.0f, 120.0f), FVector(6550.0f, -4300.0f, 120.0f), FVector(6000.0f, -4300.0f, 120.0f),
		FVector(7000.0f, -3650.0f, 120.0f), FVector(6350.0f, -3650.0f, 120.0f), FVector(5700.0f, -3650.0f, 120.0f)
	};
	HunterSpawn = FVector(-7350.0f, 4550.0f, 120.0f);

	const FLinearColor Ground(0.055f, 0.070f, 0.073f, 1.0f);
	const FLinearColor Wall(0.14f, 0.17f, 0.17f, 1.0f);
	const FLinearColor WetMetal(0.10f, 0.13f, 0.14f, 1.0f);
	const FLinearColor Warning(0.72f, 0.52f, 0.13f, 1.0f);
	const FLinearColor ExitGreen(0.05f, 0.78f, 0.42f, 1.0f);
	const FLinearColor FogBlue(0.14f, 0.35f, 0.42f, 1.0f);
	const FLinearColor Trunk(0.070f, 0.050f, 0.032f, 1.0f);
	const FLinearColor Needles(0.025f, 0.115f, 0.070f, 1.0f);
	const FLinearColor DeadNeedles(0.16f, 0.13f, 0.075f, 1.0f);
	const FLinearColor Road(0.028f, 0.034f, 0.035f, 1.0f);
	const FLinearColor Shed(0.12f, 0.16f, 0.16f, 1.0f);
	const FLinearColor Roof(0.09f, 0.10f, 0.105f, 1.0f);
	const FLinearColor Mast(0.42f, 0.46f, 0.46f, 1.0f);
	const FLinearColor DimRed(0.55f, 0.055f, 0.04f, 1.0f);

	const auto LocalPoint = [](const FVector& Origin, const FRotator& Rotation, const FVector& Offset)
	{
		return Origin + Rotation.RotateVector(Offset);
	};
	const auto SpawnShed = [&](const FVector& Origin, const FRotator& Rotation)
	{
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 78.0f)), FVector(4.4f, 2.7f, 1.55f), Shed, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 168.0f)), FVector(4.9f, 3.1f, 0.24f), Roof, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, -142.0f, 102.0f)), FVector(1.05f, 0.045f, 1.05f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
	};
	const auto SpawnWatchPost = [&](const FVector& Origin)
	{
		const FVector Legs[] = {
			FVector(-130.0f, -130.0f, 150.0f), FVector(130.0f, -130.0f, 150.0f),
			FVector(-130.0f, 130.0f, 150.0f), FVector(130.0f, 130.0f, 150.0f)
		};
		for (const FVector& Leg : Legs)
		{
			SpawnBlock(Origin + Leg, FVector(0.16f, 0.16f, 3.0f), WetMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		}
		SpawnBlock(Origin + FVector(0.0f, 0.0f, 312.0f), FVector(3.2f, 3.2f, 0.22f), WetMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(Origin + FVector(0.0f, 0.0f, 405.0f), FVector(2.5f, 2.1f, 1.25f), Shed, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(Origin + FVector(0.0f, 0.0f, 492.0f), FVector(2.9f, 2.5f, 0.22f), Roof, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(Origin + FVector(-190.0f, 0.0f, 235.0f), FVector(0.10f, 2.7f, 0.14f), Warning, FRotator(0.0f, 0.0f, 28.0f), false, EBHBlockMaterial::WarningSign);
	};
	const auto SpawnRoadBarrier = [&](const FVector& Origin, const FRotator& Rotation)
	{
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 44.0f)), FVector(3.0f, 0.24f, 0.65f), WetMetal, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-115.0f, 0.0f, 94.0f)), FVector(0.18f, 0.30f, 0.86f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(115.0f, 0.0f, 94.0f)), FVector(0.18f, 0.30f, 0.86f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
	};
	const auto SpawnCrawlRun = [&](const FVector& Origin, const FRotator& Rotation)
	{
		const FLinearColor MouthShadow(0.018f, 0.023f, 0.023f, 1.0f);
		const float RoofBottomZ = 148.0f;
		const float RoofScaleZ = 0.20f;
		const float RunScaleX = 5.6f;
		const float SideOffsetY = 96.0f;

		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(0.54f, 0.035f))), FVector(5.2f, 1.85f, 0.035f), Road, Rotation, false, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, -SideOffsetY, CenterZForBlockBottom(0.0f, 1.14f))), FVector(RunScaleX, 0.12f, 1.14f), WetMetal, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, SideOffsetY, CenterZForBlockBottom(0.0f, 1.14f))), FVector(RunScaleX, 0.12f, 1.14f), WetMetal, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(RoofBottomZ, RoofScaleZ))), FVector(RunScaleX, 2.12f, RoofScaleZ), Shed, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-292.0f, -74.0f, 72.0f)), FVector(0.045f, 0.22f, 1.08f), MouthShadow, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-292.0f, 74.0f, 72.0f)), FVector(0.045f, 0.22f, 1.08f), MouthShadow, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(292.0f, -74.0f, 72.0f)), FVector(0.045f, 0.22f, 1.08f), MouthShadow, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(292.0f, 74.0f, 72.0f)), FVector(0.045f, 0.22f, 1.08f), MouthShadow, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-240.0f, 0.0f, 136.0f)), FVector(0.08f, 1.72f, 0.08f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(240.0f, 0.0f, 136.0f)), FVector(0.08f, 1.72f, 0.08f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
	};
	const auto SpawnDoorGateFrame = [&](const FVector& Origin, const FRotator& Rotation)
	{
		const FVector PostScale(0.18f, 0.12f, 2.68f);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, -98.0f, CenterZForBlockBottom(0.0f, PostScale.Z))), PostScale, WetMetal, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 98.0f, CenterZForBlockBottom(0.0f, PostScale.Z))), PostScale, WetMetal, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 272.0f)), FVector(0.20f, 2.35f, 0.14f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(0.50f, 0.035f))), FVector(0.32f, 2.15f, 0.035f), Road, Rotation, false, EBHBlockMaterial::DiamondPlate);
	};

	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.28f)), FVector(158.0f, 124.0f, 0.28f), Ground, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);

	SpawnBlock(FVector(0.0f, -6200.0f, CenterZForBlockBottom(0.0f, 2.4f)), FVector(158.0f, 0.18f, 2.4f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 6200.0f, CenterZForBlockBottom(0.0f, 2.4f)), FVector(158.0f, 0.18f, 2.4f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-7900.0f, 0.0f, CenterZForBlockBottom(0.0f, 2.4f)), FVector(0.18f, 124.0f, 2.4f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(7900.0f, 0.0f, CenterZForBlockBottom(0.0f, 2.4f)), FVector(0.18f, 124.0f, 2.4f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);

	SpawnBlock(FVector(5600.0f, -4850.0f, CenterZForBlockBottom(0.62f, 0.035f)), FVector(40.0f, 10.5f, 0.035f), Road, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(0.0f, -4050.0f, CenterZForBlockBottom(0.58f, 0.035f)), FVector(118.0f, 7.0f, 0.035f), Road, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(-5750.0f, 4050.0f, CenterZForBlockBottom(0.52f, 0.035f)), FVector(32.0f, 8.0f, 0.035f), Road, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		SpawnRoadBarrier(FVector(5000.0f + Index * 340.0f, -5650.0f, 0.0f), FRotator::ZeroRotator);
	}
	SpawnShed(FVector(-5200.0f, -5050.0f, 0.0f), FRotator(0.0f, 8.0f, 0.0f));
	SpawnShed(FVector(4100.0f, 3050.0f, 0.0f), FRotator(0.0f, 92.0f, 0.0f));
	SpawnShed(FVector(-6150.0f, 2250.0f, 0.0f), FRotator(0.0f, -8.0f, 0.0f));
	SpawnWatchPost(FVector(6900.0f, -1200.0f, 0.0f));
	SpawnWatchPost(FVector(-6950.0f, 3550.0f, 0.0f));
	SpawnBlock(FVector(-980.0f, 5050.0f, 340.0f), FVector(0.20f, 0.20f, 6.8f), Mast, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-980.0f, 5050.0f, 830.0f), FVector(0.12f, 0.12f, 3.0f), Mast, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-980.0f, 5050.0f, 610.0f), FVector(2.3f, 0.08f, 0.08f), Mast, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-980.0f, 5050.0f, 760.0f), FVector(0.08f, 2.6f, 0.08f), Mast, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-980.0f, 5050.0f, 1010.0f), FVector(0.34f, 0.34f, 0.34f), DimRed, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(-5200.0f, -4580.0f, 78.0f), FVector(3.5f, 1.25f, 1.25f), WetMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(-4820.0f, -4580.0f, 78.0f), FVector(2.5f, 1.05f, 1.05f), WetMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(-4430.0f, -4580.0f, 120.0f), FVector(0.26f, 2.8f, 1.65f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(7250.0f, -3150.0f, 125.0f), FVector(0.05f, 2.4f, 0.78f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(-7300.0f, 4700.0f, 125.0f), FVector(0.05f, 2.4f, 0.78f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);

	const TArray<TPair<FVector, FVector>> MazeWalls = {
		{FVector(-5400.0f, -3600.0f, 190.0f), FVector(30.0f, 0.28f, 3.55f)}, {FVector(-2100.0f, -3600.0f, 190.0f), FVector(18.0f, 0.28f, 3.55f)}, {FVector(2500.0f, -3600.0f, 190.0f), FVector(44.0f, 0.28f, 3.55f)},
		{FVector(-4700.0f, -1200.0f, 190.0f), FVector(34.0f, 0.28f, 3.55f)}, {FVector(-700.0f, -1200.0f, 190.0f), FVector(26.0f, 0.28f, 3.55f)}, {FVector(4300.0f, -1200.0f, 190.0f), FVector(32.0f, 0.28f, 3.55f)},
		{FVector(-3900.0f, 1450.0f, 190.0f), FVector(48.0f, 0.28f, 3.55f)}, {FVector(900.0f, 1450.0f, 190.0f), FVector(26.0f, 0.28f, 3.55f)}, {FVector(5200.0f, 1450.0f, 190.0f), FVector(24.0f, 0.28f, 3.55f)},
		{FVector(-5100.0f, 3950.0f, 190.0f), FVector(36.0f, 0.28f, 3.55f)}, {FVector(-900.0f, 3950.0f, 190.0f), FVector(32.0f, 0.28f, 3.55f)}, {FVector(3950.0f, 3950.0f, 190.0f), FVector(36.0f, 0.28f, 3.55f)},
		{FVector(-6100.0f, -2300.0f, 190.0f), FVector(0.28f, 34.0f, 3.55f)}, {FVector(-6100.0f, 2600.0f, 190.0f), FVector(0.28f, 36.0f, 3.55f)},
		{FVector(-3100.0f, -5200.0f, 190.0f), FVector(0.28f, 20.0f, 3.55f)}, {FVector(-3100.0f, 900.0f, 190.0f), FVector(0.28f, 42.0f, 3.55f)},
		{FVector(300.0f, -2400.0f, 190.0f), FVector(0.28f, 32.0f, 3.55f)}, {FVector(300.0f, 3300.0f, 190.0f), FVector(0.28f, 28.0f, 3.55f)},
		{FVector(3500.0f, -5200.0f, 190.0f), FVector(0.28f, 22.0f, 3.55f)}, {FVector(3500.0f, -200.0f, 190.0f), FVector(0.28f, 38.0f, 3.55f)},
		{FVector(6200.0f, -2500.0f, 190.0f), FVector(0.28f, 34.0f, 3.55f)}, {FVector(6200.0f, 2850.0f, 190.0f), FVector(0.28f, 32.0f, 3.55f)}
	};
	for (const TPair<FVector, FVector>& WallSpec : MazeWalls)
	{
		const FVector FenceScale(WallSpec.Value.X, WallSpec.Value.Y, 2.65f);

		const auto SpawnFenceSegment = [&](const FVector& SegmentLocation, const FVector& SegmentScale)
		{
			SpawnBlock(FVector(SegmentLocation.X, SegmentLocation.Y, CenterZForBlockBottom(0.0f, SegmentScale.Z)), SegmentScale, Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(SegmentLocation.X, SegmentLocation.Y, 164.0f), FVector(SegmentScale.X, SegmentScale.Y, 0.045f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		};

		struct FWallCutSpec
		{
			FVector WallLocation;
			float OffsetAlongWall;
			float HalfWidth;
			bool bCrawl;
		};
		const FWallCutSpec WallCuts[] = {
			{FVector(2500.0f, -3600.0f, 190.0f), -1650.0f, 132.0f, true},
			{FVector(-4700.0f, -1200.0f, 190.0f), 1050.0f, 132.0f, true},
			{FVector(-3900.0f, 1450.0f, 190.0f), -1250.0f, 132.0f, true},
			{FVector(3950.0f, 3950.0f, 190.0f), -1050.0f, 132.0f, true},
			{FVector(-6100.0f, 2600.0f, 190.0f), -1200.0f, 132.0f, true},
			{FVector(-3100.0f, 900.0f, 190.0f), -1350.0f, 132.0f, true},
			{FVector(3500.0f, -200.0f, 190.0f), 1200.0f, 132.0f, true},
			{FVector(6200.0f, 2850.0f, 190.0f), -850.0f, 132.0f, true},
			{FVector(-6100.0f, -2300.0f, 190.0f), 1200.0f, 118.0f, false},
			{FVector(-700.0f, -1200.0f, 190.0f), 1000.0f, 118.0f, false},
			{FVector(2500.0f, -3600.0f, 190.0f), 1000.0f, 118.0f, false},
			{FVector(-3100.0f, 900.0f, 190.0f), 1000.0f, 118.0f, false},
			{FVector(900.0f, 1450.0f, 190.0f), -600.0f, 118.0f, false},
			{FVector(-6100.0f, 2600.0f, 190.0f), 1625.0f, 118.0f, false},
			{FVector(6200.0f, 2850.0f, 190.0f), 1430.0f, 118.0f, false}
		};

		TArray<FWallCutSpec> MatchingCuts;
		for (const FWallCutSpec& Cut : WallCuts)
		{
			if (WallSpec.Key.Equals(Cut.WallLocation, 0.1f))
			{
				MatchingCuts.Add(Cut);
			}
		}

		if (MatchingCuts.IsEmpty())
		{
			SpawnFenceSegment(WallSpec.Key, FenceScale);
			continue;
		}

		const bool bWallRunsAlongX = FenceScale.X >= FenceScale.Y;
		const float LongScale = bWallRunsAlongX ? FenceScale.X : FenceScale.Y;
		const float HalfSpan = LongScale * 50.0f;

		const auto SpawnFenceSpan = [&](float SpanStart, float SpanEnd)
		{
			const float SpanLength = SpanEnd - SpanStart;
			if (SpanLength < 70.0f)
			{
				return;
			}

			FVector SegmentLocation = WallSpec.Key;
			FVector SegmentScale = FenceScale;
			const float SegmentOffset = (SpanStart + SpanEnd) * 0.5f;
			if (bWallRunsAlongX)
			{
				SegmentLocation.X += SegmentOffset;
				SegmentScale.X = SpanLength / 100.0f;
			}
			else
			{
				SegmentLocation.Y += SegmentOffset;
				SegmentScale.Y = SpanLength / 100.0f;
			}
			SpawnFenceSegment(SegmentLocation, SegmentScale);
		};

		MatchingCuts.Sort([](const FWallCutSpec& Left, const FWallCutSpec& Right)
		{
			return Left.OffsetAlongWall < Right.OffsetAlongWall;
		});

		float SpanCursor = -HalfSpan;
		for (const FWallCutSpec& Cut : MatchingCuts)
		{
			const float CutHalfWidth = FMath::Max(90.0f, Cut.HalfWidth);
			const float CutOffset = FMath::Clamp(Cut.OffsetAlongWall, -HalfSpan + CutHalfWidth + 45.0f, HalfSpan - CutHalfWidth - 45.0f);
			const float CutStart = FMath::Max(-HalfSpan, CutOffset - CutHalfWidth);
			const float CutEnd = FMath::Min(HalfSpan, CutOffset + CutHalfWidth);
			if (CutStart > SpanCursor)
			{
				SpawnFenceSpan(SpanCursor, CutStart);
			}

			FVector CutLocation(WallSpec.Key.X, WallSpec.Key.Y, 0.0f);
			if (bWallRunsAlongX)
			{
				CutLocation.X += CutOffset;
			}
			else
			{
				CutLocation.Y += CutOffset;
			}

			if (Cut.bCrawl)
			{
				const float LintelBottomZ = 148.0f;
				const float LintelScaleZ = FMath::Max(0.1f, (FenceScale.Z * 100.0f - LintelBottomZ) / 100.0f);
				const FVector LintelLocation(CutLocation.X, CutLocation.Y, CenterZForBlockBottom(LintelBottomZ, LintelScaleZ));
				const FVector LintelScale = bWallRunsAlongX
					? FVector((CutHalfWidth * 2.0f) / 100.0f, FenceScale.Y, LintelScaleZ)
					: FVector(FenceScale.X, (CutHalfWidth * 2.0f) / 100.0f, LintelScaleZ);
				SpawnBlock(LintelLocation, LintelScale, Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
				SpawnBlock(FVector(CutLocation.X, CutLocation.Y, 164.0f), FVector(LintelScale.X, LintelScale.Y, 0.045f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
				SpawnCrawlRun(CutLocation, bWallRunsAlongX ? FRotator(0.0f, 90.0f, 0.0f) : FRotator::ZeroRotator);
			}

			SpanCursor = FMath::Max(SpanCursor, CutEnd);
		}
		SpawnFenceSpan(SpanCursor, HalfSpan);
	}

	struct FFoggroundsScreenSpec
	{
		FVector Location;
		FVector Scale;
		FRotator Rotation;
		FLinearColor Tint;
		EBHBlockMaterial Material;
	};

	const FLinearColor DarkScreen(0.075f, 0.105f, 0.105f, 1.0f);
	const FLinearColor BrushScreen(0.045f, 0.105f, 0.070f, 1.0f);
	const FFoggroundsScreenSpec SightScreens[] = {
		{FVector(-6900.0f, -2700.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(7.8f, 0.38f, 2.85f), FRotator(0.0f, -12.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-4700.0f, -4725.0f, CenterZForBlockBottom(0.0f, 2.95f)), FVector(0.42f, 11.0f, 2.95f), FRotator(0.0f, 8.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(-2550.0f, -2920.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(12.0f, 0.38f, 2.80f), FRotator(0.0f, 14.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-900.0f, -4875.0f, CenterZForBlockBottom(0.0f, 2.90f)), FVector(0.42f, 10.2f, 2.90f), FRotator(0.0f, -10.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(1400.0f, -3250.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(12.6f, 0.40f, 2.80f), FRotator(0.0f, -18.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(4800.0f, -3025.0f, CenterZForBlockBottom(0.0f, 2.95f)), FVector(0.42f, 12.0f, 2.95f), FRotator(0.0f, 6.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(6650.0f, -1850.0f, CenterZForBlockBottom(0.0f, 2.75f)), FVector(8.0f, 0.36f, 2.75f), FRotator(0.0f, 18.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-5550.0f, 525.0f, CenterZForBlockBottom(0.0f, 2.90f)), FVector(0.42f, 12.4f, 2.90f), FRotator(0.0f, 4.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-3600.0f, 2675.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(11.5f, 0.38f, 2.80f), FRotator(0.0f, -12.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-1350.0f, 4000.0f, CenterZForBlockBottom(0.0f, 2.90f)), FVector(9.5f, 0.40f, 2.90f), FRotator(0.0f, 16.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(1750.0f, 1180.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(0.42f, 11.6f, 2.85f), FRotator(0.0f, -7.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(4550.0f, 2675.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(10.8f, 0.38f, 2.80f), FRotator(0.0f, 10.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(6350.0f, 4300.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(0.42f, 8.6f, 2.85f), FRotator(0.0f, -15.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(3200.0f, 4725.0f, CenterZForBlockBottom(0.0f, 2.75f)), FVector(8.6f, 0.36f, 2.75f), FRotator(0.0f, -8.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-7050.0f, 1925.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(7.0f, 0.38f, 2.85f), FRotator(0.0f, 22.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(7200.0f, 2550.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(0.42f, 8.4f, 2.85f), FRotator(0.0f, 8.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-800.0f, -100.0f, CenterZForBlockBottom(0.0f, 2.60f)), FVector(9.0f, 0.36f, 2.60f), FRotator(0.0f, 24.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(3000.0f, -50.0f, CenterZForBlockBottom(0.0f, 2.60f)), FVector(0.38f, 8.8f, 2.60f), FRotator(0.0f, -18.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal}
	};
	for (const FFoggroundsScreenSpec& Screen : SightScreens)
	{
		SpawnBlock(Screen.Location, Screen.Scale, Screen.Tint, Screen.Rotation, true, Screen.Material);
	}

	const TArray<TPair<FVector, FRotator>> DoorSpecs = {
		{FVector(-6100.0f, -1100.0f, 120.0f), FRotator::ZeroRotator}, {FVector(-3100.0f, -3600.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(300.0f, -1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(3500.0f, -3600.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(6200.0f, -700.0f, 120.0f), FRotator::ZeroRotator}, {FVector(-3100.0f, 1900.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(300.0f, 1450.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(3500.0f, 1450.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-6100.0f, 4230.0f, 120.0f), FRotator::ZeroRotator}, {FVector(6200.0f, 4280.0f, 120.0f), FRotator::ZeroRotator}
	};
	for (const TPair<FVector, FRotator>& DoorSpec : DoorSpecs)
	{
		SpawnDoorGateFrame(DoorSpec.Key, DoorSpec.Value);
		if (ABHDoor* Door = GetWorld()->SpawnActor<ABHDoor>(DoorSpec.Key, DoorSpec.Value))
		{
			DoorActors.Add(Door);
		}
	}

	const TArray<TPair<FVector, FRotator>> Breakers = {
		{FVector(-7000.0f, -5200.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-4500.0f, -5000.0f, 80.0f), FRotator::ZeroRotator},
		{FVector(-800.0f, -5050.0f, 80.0f), FRotator::ZeroRotator}, {FVector(2750.0f, -5100.0f, 80.0f), FRotator::ZeroRotator},
		{FVector(7100.0f, -4200.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}, {FVector(-7100.0f, 4200.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-500.0f, 5200.0f, 80.0f), FRotator::ZeroRotator}, {FVector(6900.0f, 5000.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}
	};
	for (const TPair<FVector, FRotator>& Breaker : Breakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	const TArray<TPair<FVector, EBHObjectiveStationType>> ObjectiveStationSpecs = {
		{FVector(-6900.0f, -800.0f, 95.0f), EBHObjectiveStationType::Valve}, {FVector(-5200.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-2250.0f, -2500.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(-1100.0f, 2500.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(1100.0f, -2500.0f, 95.0f), EBHObjectiveStationType::Evidence}, {FVector(2600.0f, 2700.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(5200.0f, -2500.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(7050.0f, 800.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(4550.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Valve}, {FVector(0.0f, 0.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-7050.0f, -4100.0f, 95.0f), EBHObjectiveStationType::Antenna}, {FVector(6900.0f, -3500.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-3300.0f, 4550.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(6550.0f, 4250.0f, 95.0f), EBHObjectiveStationType::Valve}
	};
	for (const TPair<FVector, EBHObjectiveStationType>& Spec : ObjectiveStationSpecs)
	{
		if (ABHObjectiveStation* Station = GetWorld()->SpawnActor<ABHObjectiveStation>(Spec.Key, FRotator::ZeroRotator))
		{
			Station->Configure(Spec.Value);
			ObjectiveStations.Add(Station);
		}
	}

	for (int32 Index = 0; Index < 24; ++Index)
	{
		const float X = -6500.0f + (Index % 6) * 2600.0f;
		const float Y = (Index < 9 ? -1.0f : 1.0f) * (2100.0f + (Index % 3) * 1250.0f);
		GetWorld()->SpawnActor<ABHLocker>(FVector(X, Y, 100.0f), FRotator::ZeroRotator);
	}

	for (const FVector& Location : {FVector(-7200.0f, -3100.0f, 70.0f), FVector(-3500.0f, -5200.0f, 70.0f), FVector(1200.0f, -5200.0f, 70.0f), FVector(5200.0f, -4800.0f, 70.0f), FVector(-6300.0f, 3200.0f, 70.0f), FVector(-1500.0f, 5200.0f, 70.0f), FVector(3100.0f, 5200.0f, 70.0f), FVector(7200.0f, 3200.0f, 70.0f)})
	{
		GetWorld()->SpawnActor<ABHBatteryPickup>(Location, FRotator::ZeroRotator);
	}

	const FVector FoggroundAlarms[] = {
		FVector(7100.0f, -3100.0f, 105.0f), FVector(5200.0f, -5050.0f, 105.0f),
		FVector(2650.0f, -3650.0f, 105.0f), FVector(-700.0f, -1200.0f, 105.0f),
		FVector(-6200.0f, -800.0f, 105.0f), FVector(-6200.0f, 4050.0f, 105.0f),
		FVector(3600.0f, 3950.0f, 105.0f), FVector(7050.0f, 800.0f, 105.0f)
	};
	for (const FVector& Location : FoggroundAlarms)
	{
		GetWorld()->SpawnActor<ABHPanicAlarm>(Location, FRotator::ZeroRotator);
	}

	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FVector(7440.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FVector(-7440.0f, 0.0f, 120.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	const float ExitFloorZ = CenterZForBlockBottom(0.72f, 0.045f);
	SpawnBlock(FVector(7100.0f, 0.0f, ExitFloorZ), FVector(8.4f, 2.8f, 0.045f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
	SpawnBlock(FVector(-7100.0f, 0.0f, ExitFloorZ), FVector(8.4f, 2.8f, 0.045f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);

	struct FFogLightSpec
	{
		FVector Location;
		int32 Circuit;
		FLinearColor Color;
		float Intensity;
	};
	const FFogLightSpec Lights[] = {
		{FVector(-6200.0f, -4600.0f, 290.0f), 1, FogBlue, 780.0f}, {FVector(-2600.0f, -4700.0f, 290.0f), 1, FogBlue, 730.0f},
		{FVector(1200.0f, -4700.0f, 290.0f), 2, FLinearColor(0.48f, 0.72f, 0.76f, 1.0f), 720.0f}, {FVector(5200.0f, -4300.0f, 290.0f), 2, FLinearColor(0.48f, 0.72f, 0.76f, 1.0f), 720.0f},
		{FVector(-6500.0f, 0.0f, 290.0f), 3, FLinearColor(0.65f, 0.82f, 0.70f, 1.0f), 760.0f}, {FVector(0.0f, 0.0f, 290.0f), 0, FLinearColor(0.56f, 0.65f, 0.70f, 1.0f), 600.0f},
		{FVector(6500.0f, 0.0f, 290.0f), 4, FLinearColor(0.65f, 0.82f, 0.70f, 1.0f), 760.0f}, {FVector(-5200.0f, 4300.0f, 290.0f), 5, FLinearColor(0.84f, 0.64f, 0.45f, 1.0f), 690.0f},
		{FVector(-1200.0f, 4700.0f, 290.0f), 5, FLinearColor(0.84f, 0.64f, 0.45f, 1.0f), 690.0f}, {FVector(4200.0f, 4550.0f, 290.0f), 6, FLinearColor(0.90f, 0.48f, 0.36f, 1.0f), 660.0f}
	};
	for (const FFogLightSpec& Spec : Lights)
	{
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(Spec.Location, FRotator::ZeroRotator))
		{
			Light->Configure(Spec.Circuit, Spec.Color, Spec.Intensity, 1100.0f);
			FlickerLights.Add(Light);
		}
	}

	const TArray<TPair<FVector, int32>> FoggroundSwitches = {
		{FVector(-7000.0f, -4100.0f, 120.0f), 1},
		{FVector(2500.0f, -5300.0f, 120.0f), 2},
		{FVector(-7050.0f, 900.0f, 120.0f), 3},
		{FVector(7050.0f, -900.0f, 120.0f), 4},
		{FVector(-1800.0f, 5250.0f, 120.0f), 5},
		{FVector(5200.0f, 5000.0f, 120.0f), 6}
	};
	for (const TPair<FVector, int32>& Switch : FoggroundSwitches)
	{
		if (ABHPowerSwitch* PowerSwitch = GetWorld()->SpawnActor<ABHPowerSwitch>(Switch.Key, FRotator::ZeroRotator))
		{
			PowerSwitch->Configure(Switch.Value, FText::FromString(FString::Printf(TEXT("Toggle Fog Circuit %d"), Switch.Value)));
		}
	}

	if (ABHSecurityShutter* Shutter = GetWorld()->SpawnActor<ABHSecurityShutter>(FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator))
	{
		Shutter->Configure(300);
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(-7000.0f, 0.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		Terminal->Configure(300, FText::FromString(TEXT("Open Fogground Shutter")));
	}

	AddIndustrialClutter({FVector(-5100.0f, -5050.0f, 55.0f), FVector(-700.0f, -4700.0f, 55.0f), FVector(4300.0f, -4900.0f, 55.0f), FVector(-6400.0f, 2100.0f, 55.0f), FVector(-1800.0f, 3200.0f, 55.0f), FVector(3600.0f, 3000.0f, 55.0f), FVector(6600.0f, 2200.0f, 55.0f)}, WetMetal);
	AddSurfaceDetailGrid(7600.0f, 5900.0f, FLinearColor(0.08f, 0.12f, 0.13f, 1.0f));

	FRandomStream NatureStream(73421);
	for (int32 Index = 0; Index < 62; ++Index)
	{
		const float X = NatureStream.FRandRange(-7350.0f, 7350.0f);
		const float Y = NatureStream.FRandRange(-5650.0f, 5650.0f);
		if ((X > 5100.0f && FMath::Abs(Y) < 1600.0f) || FMath::Abs(Y + 3600.0f) < 460.0f || FMath::Abs(Y - 3950.0f) < 360.0f)
		{
			continue;
		}

		const float Scale = NatureStream.FRandRange(0.82f, 1.22f);
		SpawnBlock(FVector(X, Y, 88.0f * Scale), FVector(0.16f, 0.16f, 1.75f * Scale), Trunk, FRotator(0.0f, Index * 17.0f, 0.0f), true);
		SpawnBlock(FVector(X, Y, 220.0f * Scale), FVector(0.90f * Scale, 0.90f * Scale, 0.70f * Scale), (Index % 6 == 0) ? DeadNeedles : Needles, FRotator(0.0f, Index * 29.0f, 0.0f), false);
		SpawnBlock(FVector(X, Y, 294.0f * Scale), FVector(0.58f * Scale, 0.58f * Scale, 0.48f * Scale), Needles, FRotator(0.0f, Index * 41.0f, 0.0f), false);
	}

	for (int32 Index = 0; Index < 26; ++Index)
	{
		const float X = NatureStream.FRandRange(-7200.0f, 7200.0f);
		const float Y = NatureStream.FRandRange(-5550.0f, 5550.0f);
		SpawnBlock(FVector(X, Y, 36.0f), FVector(NatureStream.FRandRange(0.65f, 1.45f), NatureStream.FRandRange(0.45f, 1.20f), NatureStream.FRandRange(0.32f, 0.58f)), FLinearColor(0.13f, 0.14f, 0.13f, 1.0f), FRotator(0.0f, Index * 23.0f, 0.0f), true, EBHBlockMaterial::Concrete);
	}

	ScarePoints.Append({
		FVector(-7050.0f, -4900.0f, 110.0f), FVector(-4200.0f, -4550.0f, 110.0f), FVector(-400.0f, -4550.0f, 110.0f), FVector(3500.0f, -4550.0f, 110.0f), FVector(7050.0f, -3600.0f, 110.0f),
		FVector(-7000.0f, -400.0f, 110.0f), FVector(-3200.0f, -300.0f, 110.0f), FVector(0.0f, 300.0f, 110.0f), FVector(3200.0f, -300.0f, 110.0f), FVector(7000.0f, 400.0f, 110.0f),
		FVector(-7050.0f, 4200.0f, 110.0f), FVector(-3300.0f, 4550.0f, 110.0f), FVector(500.0f, 4550.0f, 110.0f), FVector(4200.0f, 4550.0f, 110.0f), FVector(7050.0f, 4200.0f, 110.0f)
	});

	SpawnAmbient(FVector(-6200.0f, -4700.0f, 150.0f), 36.0f, 0.16f, 0.05f, 0.18f);
	SpawnAmbient(FVector(6200.0f, 4700.0f, 150.0f), 52.0f, 0.14f, 0.04f, 0.24f);
	SpawnAmbient(FVector(0.0f, 0.0f, 150.0f), 24.0f, 0.12f, 0.06f, 0.12f);

	AddFoggroundsLightingPass();
	AddFoggroundsMoodPass();
	BuildRuntimeNavigation();
}

void ABHGameMode::AddFoggroundsMoodPass()
{
	switch (RuntimeFogPreset)
	{
	case EBHFogPreset::Light:
		AddMoodPass(FLinearColor(0.060f, 0.090f, 0.095f, 1.0f), 0.075f, 0.56f, 0.14f);
		break;
	case EBHFogPreset::Extreme:
		AddMoodPass(FLinearColor(0.055f, 0.078f, 0.082f, 1.0f), 0.820f, 0.68f, 0.16f);
		break;
	case EBHFogPreset::Heavy:
	default:
		AddMoodPass(FLinearColor(0.012f, 0.022f, 0.024f, 1.0f), 1.150f, 0.94f, 0.36f);
		break;
	}

	AddFoggroundsModeledFog();
}

void ABHGameMode::AddFoggroundsLightingPass()
{
	const bool bExtremeFog = RuntimeFogPreset == EBHFogPreset::Extreme;
	const bool bHeavyFog = RuntimeFogPreset == EBHFogPreset::Heavy;
	const float SkyBrightness = bExtremeFog ? 0.43f : (bHeavyFog ? 0.34f : 0.29f);
	const FLinearColor MoonTint(0.36f, 0.48f, 0.72f, 1.0f);
	const FLinearColor SkyTint(0.10f, 0.16f, 0.27f, 1.0f);

	if (ASkyAtmosphere* SkyAtmosphere = GetWorld()->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		SkyAtmosphere->SetReplicates(true);
		SkyAtmosphere->SetReplicateMovement(false);
		SkyAtmosphere->bAlwaysRelevant = true;
		if (USkyAtmosphereComponent* SkyComponent = SkyAtmosphere->GetComponent())
		{
			SkyComponent->SetSkyLuminanceFactor(FLinearColor(SkyBrightness * 0.78f, SkyBrightness * 0.92f, SkyBrightness * 1.12f, 1.0f));
			SkyComponent->SetSkyAndAerialPerspectiveLuminanceFactor(FLinearColor(SkyBrightness * 0.72f, SkyBrightness * 0.86f, SkyBrightness, 1.0f));
			SkyComponent->SetGroundAlbedo(FColor(22, 28, 34));
			SkyComponent->SetMultiScatteringFactor(0.35f);
			SkyComponent->SetHeightFogContribution(0.10f);
			SkyComponent->SetAerialPespectiveViewDistanceScale(0.25f);
		}
	}

	if (ADirectionalLight* MoonLight = GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-38.0f, 132.0f, 0.0f)))
	{
		MoonLight->SetReplicates(true);
		MoonLight->SetReplicateMovement(false);
		MoonLight->bAlwaysRelevant = true;
		MoonLight->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* LightComponent = Cast<UDirectionalLightComponent>(MoonLight->GetLightComponent()))
		{
			LightComponent->SetIntensity(bExtremeFog ? 0.68f : (bHeavyFog ? 0.56f : 0.43f));
			LightComponent->SetLightColor(MoonTint);
			LightComponent->SetIndirectLightingIntensity(bExtremeFog ? 0.50f : (bHeavyFog ? 0.40f : 0.32f));
			LightComponent->SetVolumetricScatteringIntensity(0.10f);
			LightComponent->SetCastShadows(false);
			LightComponent->SetAtmosphereSunLight(true);
			LightComponent->SetAtmosphereSunLightIndex(0);
			LightComponent->SetAtmosphereSunDiskColorScale(FLinearColor(0.12f, 0.18f, 0.30f, 1.0f));
		}
	}

	if (ASkyLight* SkyLight = GetWorld()->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		SkyLight->SetReplicates(true);
		SkyLight->SetReplicateMovement(false);
		SkyLight->bAlwaysRelevant = true;
		if (USkyLightComponent* SkyLightComponent = SkyLight->GetLightComponent())
		{
			SkyLightComponent->SetMobility(EComponentMobility::Movable);
			SkyLightComponent->SourceType = SLS_CapturedScene;
			SkyLightComponent->SetIntensity(bExtremeFog ? 0.30f : (bHeavyFog ? 0.24f : 0.18f));
			SkyLightComponent->SetLightColor(SkyTint);
			SkyLightComponent->SetLowerHemisphereColor(FLinearColor(0.018f, 0.026f, 0.032f, 1.0f));
			SkyLightComponent->SetVolumetricScatteringIntensity(0.12f);
			SkyLightComponent->SetRealTimeCaptureEnabled(true);
			SkyLightComponent->SetCaptureIsDirty();
		}
	}
}

void ABHGameMode::AddFoggroundsModeledFog()
{
	int32 HorizontalBankCount = 40;
	int32 VerticalWispCount = 18;
	float BaseAlpha = 0.38f;
	float LocalVolumeDensity = 1.25f;
	FLinearColor FogTint(0.55f, 0.68f, 0.70f, BaseAlpha);

	switch (RuntimeFogPreset)
	{
	case EBHFogPreset::Light:
		HorizontalBankCount = 24;
		VerticalWispCount = 9;
		BaseAlpha = 0.26f;
		LocalVolumeDensity = 0.70f;
		FogTint = FLinearColor(0.60f, 0.72f, 0.73f, BaseAlpha);
		break;
	case EBHFogPreset::Extreme:
		HorizontalBankCount = 82;
		VerticalWispCount = 34;
		BaseAlpha = 0.42f;
		LocalVolumeDensity = 2.20f;
		FogTint = FLinearColor(0.42f, 0.56f, 0.58f, BaseAlpha);
		break;
	case EBHFogPreset::Heavy:
		HorizontalBankCount = 170;
		VerticalWispCount = 96;
		BaseAlpha = 0.78f;
		LocalVolumeDensity = 5.25f;
		FogTint = FLinearColor(0.24f, 0.32f, 0.34f, BaseAlpha);
		break;
	default:
		break;
	}

	const bool bExtremeFog = RuntimeFogPreset == EBHFogPreset::Extreme;
	const bool bDenseFog = RuntimeFogPreset == EBHFogPreset::Heavy || RuntimeFogPreset == EBHFogPreset::Extreme;
	const auto SpawnFogSheet = [](const FVector& Location, const FVector& Scale, const FRotator& Rotation, float AlphaScale)
	{
		// Fog cards read as visible rectangular planes in cooked builds. Local fog volumes below keep the modeled fog without geometry artifacts.
		(void)Location;
		(void)Scale;
		(void)Rotation;
		(void)AlphaScale;
	};

	struct FFogSheetSpec
	{
		FVector Location;
		FVector Scale;
		FRotator Rotation;
		float AlphaScale;
	};

	const FFogSheetSpec FixedSheets[] = {
		{FVector(5600.0f, -4920.0f, 42.0f), FVector(30.0f, 5.6f, 0.030f), FRotator(0.0f, 2.0f, 0.0f), 1.20f},
		{FVector(6500.0f, -3900.0f, 58.0f), FVector(18.0f, 4.0f, 0.030f), FRotator(0.0f, -15.0f, 0.0f), 1.15f},
		{FVector(0.0f, -4050.0f, 44.0f), FVector(48.0f, 4.4f, 0.030f), FRotator(0.0f, 0.0f, 0.0f), 1.05f},
		{FVector(-5800.0f, 4050.0f, 52.0f), FVector(24.0f, 5.2f, 0.030f), FRotator(0.0f, 8.0f, 0.0f), 1.25f},
		{FVector(-7100.0f, 4600.0f, 125.0f), FVector(12.0f, 0.050f, 2.4f), FRotator(0.0f, 90.0f, 0.0f), 1.30f},
		{FVector(-6900.0f, 1200.0f, 112.0f), FVector(10.0f, 0.050f, 2.1f), FRotator(0.0f, 104.0f, 0.0f), 1.10f},
		{FVector(-5200.0f, -5100.0f, 92.0f), FVector(11.0f, 0.045f, 1.7f), FRotator(0.0f, -8.0f, 0.0f), 0.92f},
		{FVector(7000.0f, -1150.0f, 132.0f), FVector(9.0f, 0.045f, 2.2f), FRotator(0.0f, 82.0f, 0.0f), 1.05f},
		{FVector(-980.0f, 5050.0f, 130.0f), FVector(13.5f, 0.055f, 2.6f), FRotator(0.0f, 38.0f, 0.0f), 1.30f},
		{FVector(-1200.0f, 4800.0f, 65.0f), FVector(14.0f, 4.2f, 0.035f), FRotator(0.0f, -20.0f, 0.0f), 1.18f}
	};
	for (const FFogSheetSpec& Sheet : FixedSheets)
	{
		SpawnFogSheet(Sheet.Location, Sheet.Scale, Sheet.Rotation, Sheet.AlphaScale);
	}

	if (bDenseFog)
	{
		const FFogSheetSpec ExtremeCurtains[] = {
			{FVector(5900.0f, -4725.0f, 155.0f), FVector(34.0f, 0.060f, 3.8f), FRotator(0.0f, 6.0f, 0.0f), 1.35f},
			{FVector(4200.0f, -3525.0f, 145.0f), FVector(26.0f, 0.060f, 3.4f), FRotator(0.0f, -12.0f, 0.0f), 1.20f},
			{FVector(0.0f, -4000.0f, 140.0f), FVector(42.0f, 0.060f, 3.3f), FRotator(0.0f, 2.0f, 0.0f), 1.25f},
			{FVector(-4300.0f, -4200.0f, 150.0f), FVector(24.0f, 0.060f, 3.5f), FRotator(0.0f, 15.0f, 0.0f), 1.28f},
			{FVector(-6500.0f, 700.0f, 155.0f), FVector(20.0f, 0.060f, 3.6f), FRotator(0.0f, 96.0f, 0.0f), 1.32f},
			{FVector(-5850.0f, 4050.0f, 145.0f), FVector(28.0f, 0.060f, 3.4f), FRotator(0.0f, -8.0f, 0.0f), 1.35f},
			{FVector(-800.0f, 4450.0f, 150.0f), FVector(30.0f, 0.060f, 3.5f), FRotator(0.0f, 12.0f, 0.0f), 1.25f},
			{FVector(4200.0f, 3300.0f, 145.0f), FVector(24.0f, 0.060f, 3.3f), FRotator(0.0f, -18.0f, 0.0f), 1.24f},
			{FVector(7000.0f, 900.0f, 155.0f), FVector(18.0f, 0.060f, 3.5f), FRotator(0.0f, 88.0f, 0.0f), 1.28f},
			{FVector(-980.0f, 5050.0f, 210.0f), FVector(22.0f, 0.065f, 4.2f), FRotator(0.0f, 40.0f, 0.0f), 1.42f}
		};
		for (const FFogSheetSpec& Curtain : ExtremeCurtains)
		{
			SpawnFogSheet(Curtain.Location, bExtremeFog ? Curtain.Scale * 0.82f : Curtain.Scale, Curtain.Rotation, bExtremeFog ? Curtain.AlphaScale * 0.50f : Curtain.AlphaScale);
		}
	}

	FRandomStream FogStream(91273 + static_cast<int32>(RuntimeFogPreset) * 811);
	for (int32 Index = 0; Index < HorizontalBankCount; ++Index)
	{
		const float X = FogStream.FRandRange(-7350.0f, 7350.0f);
		const float Y = FogStream.FRandRange(-5650.0f, 5650.0f);
		if (X > 5400.0f && Y < -3300.0f && RuntimeFogPreset == EBHFogPreset::Light)
		{
			continue;
		}

		const FVector Location(X, Y, FogStream.FRandRange(28.0f, 72.0f));
		const float SheetScaleBoost = bExtremeFog ? 0.88f : 1.0f;
		const FVector Scale(FogStream.FRandRange(7.5f, 19.0f) * SheetScaleBoost, FogStream.FRandRange(2.0f, 6.2f) * SheetScaleBoost, FogStream.FRandRange(0.020f, 0.045f) * (bExtremeFog ? 0.75f : 1.0f));
		const FRotator Rotation(0.0f, FogStream.FRandRange(-35.0f, 35.0f), 0.0f);
		SpawnFogSheet(Location, Scale, Rotation, bExtremeFog ? FogStream.FRandRange(0.34f, 0.74f) : FogStream.FRandRange(0.65f, 1.20f));
	}

	for (int32 Index = 0; Index < VerticalWispCount; ++Index)
	{
		const float X = FogStream.FRandRange(-7600.0f, 7350.0f);
		const float Y = FogStream.FRandRange(-5700.0f, 5700.0f);
		const FVector Location(X, Y, FogStream.FRandRange(92.0f, 170.0f));
		const float WispScaleBoost = bExtremeFog ? 0.82f : 1.0f;
		const FVector Scale(FogStream.FRandRange(5.0f, 13.5f) * WispScaleBoost, FogStream.FRandRange(0.030f, 0.070f) * (bExtremeFog ? 0.72f : 1.0f), FogStream.FRandRange(1.2f, 3.0f) * WispScaleBoost);
		const FRotator Rotation(0.0f, FogStream.FRandRange(-180.0f, 180.0f), 0.0f);
		SpawnFogSheet(Location, Scale, Rotation, bExtremeFog ? FogStream.FRandRange(0.38f, 0.82f) : FogStream.FRandRange(0.75f, 1.35f));
	}

	struct FFogVolumeSpec
	{
		FVector Location;
		FVector Scale;
		float DensityScale;
	};

	const FFogVolumeSpec VolumeSpecs[] = {
		{FVector(6100.0f, -4850.0f, 130.0f), FVector(7.8f, 3.0f, 1.15f), 1.20f},
		{FVector(0.0f, -4050.0f, 125.0f), FVector(10.5f, 2.2f, 1.05f), 0.95f},
		{FVector(-5850.0f, 4050.0f, 135.0f), FVector(6.5f, 3.1f, 1.20f), 1.25f},
		{FVector(-6950.0f, 4000.0f, 160.0f), FVector(4.5f, 4.0f, 1.55f), 1.35f},
		{FVector(-6600.0f, -900.0f, 150.0f), FVector(4.8f, 3.5f, 1.30f), 1.05f},
		{FVector(6900.0f, -900.0f, 145.0f), FVector(4.2f, 3.0f, 1.20f), 0.95f},
		{FVector(-980.0f, 5050.0f, 190.0f), FVector(5.5f, 4.4f, 1.80f), 1.40f},
		{FVector(2500.0f, 2400.0f, 135.0f), FVector(6.0f, 3.6f, 1.25f), 0.90f}
	};

	for (const FFogVolumeSpec& Spec : VolumeSpecs)
	{
		if (ALocalFogVolume* Volume = GetWorld()->SpawnActor<ALocalFogVolume>(Spec.Location, FRotator::ZeroRotator))
		{
			Volume->SetReplicates(true);
			Volume->SetReplicateMovement(false);
			Volume->bAlwaysRelevant = true;
			Volume->SetActorScale3D(Spec.Scale);
			if (ULocalFogVolumeComponent* FogComponent = Volume->GetComponent())
			{
				FogComponent->SetRadialFogExtinction(LocalVolumeDensity * Spec.DensityScale);
				FogComponent->SetHeightFogExtinction(LocalVolumeDensity * (bExtremeFog ? 0.38f : 0.55f) * Spec.DensityScale);
				FogComponent->SetHeightFogFalloff(bExtremeFog ? 2200.0f : (RuntimeFogPreset == EBHFogPreset::Heavy ? 3600.0f : 1800.0f));
				FogComponent->SetHeightFogOffset(bExtremeFog ? -0.38f : (RuntimeFogPreset == EBHFogPreset::Heavy ? -0.65f : -0.35f));
				FogComponent->SetFogAlbedo(FLinearColor(FogTint.R, FogTint.G, FogTint.B, 1.0f));
				FogComponent->SetFogEmissive(bExtremeFog ? FLinearColor(0.016f, 0.024f, 0.025f, 1.0f) : FLinearColor(0.008f, 0.014f, 0.016f, 1.0f));
				FogComponent->SetFogPhaseG(bExtremeFog ? 0.32f : 0.35f);
			}
		}
	}
}

void ABHGameMode::BuildSubstationLevel()
{
	SurvivorSpawns = {
		FVector(5200.0f, -3600.0f, 120.0f), FVector(5550.0f, -3100.0f, 120.0f), FVector(5100.0f, -2500.0f, 120.0f),
		FVector(5400.0f, 3400.0f, 120.0f), FVector(4850.0f, 3000.0f, 120.0f), FVector(4100.0f, 3800.0f, 120.0f),
		FVector(1400.0f, 4000.0f, 120.0f), FVector(1400.0f, -4000.0f, 120.0f), FVector(-900.0f, 4100.0f, 120.0f),
		FVector(-900.0f, -4100.0f, 120.0f), FVector(-3600.0f, 3500.0f, 120.0f), FVector(-3600.0f, -3500.0f, 120.0f)
	};
	HunterSpawn = FVector(-5600.0f, 0.0f, 120.0f);

	const FLinearColor Concrete(0.11f, 0.12f, 0.125f, 1.0f);
	const FLinearColor Ceiling(0.045f, 0.050f, 0.055f, 1.0f);
	const FLinearColor Wall(0.21f, 0.24f, 0.25f, 1.0f);
	const FLinearColor Steel(0.20f, 0.22f, 0.23f, 1.0f);
	const FLinearColor Rust(0.34f, 0.16f, 0.08f, 1.0f);
	const FLinearColor Cable(0.02f, 0.025f, 0.028f, 1.0f);
	const FLinearColor Warning(0.85f, 0.58f, 0.08f, 1.0f);
	const FLinearColor ControlBlue(0.08f, 0.22f, 0.30f, 1.0f);
	const FLinearColor SickGreen(0.08f, 0.26f, 0.14f, 1.0f);

	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.25f)), FVector(122.0f, 92.0f, 0.25f), Concrete, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(350.0f, 0.12f)), FVector(122.0f, 92.0f, 0.12f), Ceiling, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	SpawnBlock(FVector(0.0f, -4600.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(122.0f, 0.35f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(0.0f, 4600.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(122.0f, 0.35f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(-6100.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(0.35f, 92.0f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(6100.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(0.35f, 92.0f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	const TArray<TPair<FVector, FVector>> Walls = {
		{FVector(-4400.0f, -2500.0f, 175.0f), FVector(34.0f, 0.30f, 3.25f)}, {FVector(-900.0f, -2500.0f, 175.0f), FVector(24.0f, 0.30f, 3.25f)}, {FVector(3200.0f, -2500.0f, 175.0f), FVector(40.0f, 0.30f, 3.25f)},
		{FVector(-4200.0f, 0.0f, 175.0f), FVector(36.0f, 0.30f, 3.25f)}, {FVector(100.0f, 0.0f, 175.0f), FVector(28.0f, 0.30f, 3.25f)}, {FVector(4300.0f, 0.0f, 175.0f), FVector(28.0f, 0.30f, 3.25f)},
		{FVector(-4100.0f, 2500.0f, 175.0f), FVector(38.0f, 0.30f, 3.25f)}, {FVector(250.0f, 2500.0f, 175.0f), FVector(34.0f, 0.30f, 3.25f)}, {FVector(4450.0f, 2500.0f, 175.0f), FVector(30.0f, 0.30f, 3.25f)},
		{FVector(-4500.0f, -3400.0f, 175.0f), FVector(0.30f, 24.0f, 3.25f)}, {FVector(-4500.0f, 1200.0f, 175.0f), FVector(0.30f, 34.0f, 3.25f)},
		{FVector(-2500.0f, -1200.0f, 175.0f), FVector(0.30f, 26.0f, 3.25f)}, {FVector(-2500.0f, 3400.0f, 175.0f), FVector(0.30f, 24.0f, 3.25f)},
		{FVector(-500.0f, -3400.0f, 175.0f), FVector(0.30f, 24.0f, 3.25f)}, {FVector(-500.0f, 1250.0f, 175.0f), FVector(0.30f, 33.0f, 3.25f)},
		{FVector(1800.0f, -1400.0f, 175.0f), FVector(0.30f, 32.0f, 3.25f)}, {FVector(1800.0f, 3600.0f, 175.0f), FVector(0.30f, 20.0f, 3.25f)},
		{FVector(3900.0f, -3400.0f, 175.0f), FVector(0.30f, 24.0f, 3.25f)}, {FVector(3900.0f, 1300.0f, 175.0f), FVector(0.30f, 32.0f, 3.25f)}
	};
	for (const TPair<FVector, FVector>& WallSpec : Walls)
	{
		SpawnBlock(FVector(WallSpec.Key.X, WallSpec.Key.Y, CenterZForBlockBottom(0.0f, WallSpec.Value.Z)), WallSpec.Value, Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	}

	const TArray<TPair<FVector, FRotator>> Doors = {
		{FVector(-4500.0f, -2450.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-2500.0f, -2500.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(-500.0f, -2500.0f, 120.0f), FRotator::ZeroRotator}, {FVector(1800.0f, -2500.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(3900.0f, -2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-2500.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-500.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(1800.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(3900.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-2500.0f, 2500.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(-500.0f, 2500.0f, 120.0f), FRotator::ZeroRotator}, {FVector(1800.0f, 2500.0f, 120.0f), FRotator::ZeroRotator}
	};
	for (const TPair<FVector, FRotator>& Door : Doors)
	{
		if (ABHDoor* DoorActor = GetWorld()->SpawnActor<ABHDoor>(Door.Key, Door.Value))
		{
			DoorActors.Add(DoorActor);
		}
	}

	const TArray<TPair<FVector, FRotator>> Breakers = {
		{FVector(-5550.0f, -3650.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-5400.0f, 3650.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-850.0f, -3800.0f, 80.0f), FRotator(0.0f, 180.0f, 0.0f)}, {FVector(850.0f, 3800.0f, 80.0f), FRotator::ZeroRotator},
		{FVector(3300.0f, -3750.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}, {FVector(5450.0f, 3300.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)},
		{FVector(5200.0f, 350.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}
	};
	for (const TPair<FVector, FRotator>& Breaker : Breakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	const TArray<TPair<FVector, EBHObjectiveStationType>> ObjectiveStationSpecs = {
		{FVector(-5850.0f, -1650.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-5850.0f, 1650.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-1200.0f, -3650.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(1650.0f, 3650.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(5350.0f, -1700.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(5300.0f, 2100.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-3600.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-3600.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-1200.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-1200.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(1450.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(1450.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(3700.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(3700.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Terminal}
	};
	for (const TPair<FVector, EBHObjectiveStationType>& Spec : ObjectiveStationSpecs)
	{
		if (ABHObjectiveStation* Station = GetWorld()->SpawnActor<ABHObjectiveStation>(Spec.Key, FRotator::ZeroRotator))
		{
			Station->Configure(Spec.Value);
			ObjectiveStations.Add(Station);
		}
	}

	const FLinearColor RevisionCyan(0.12f, 0.68f, 0.86f, 1.0f);
	const FLinearColor RevisionAmber(0.95f, 0.68f, 0.12f, 1.0f);
	const FLinearColor RevisionViolet(0.58f, 0.32f, 0.86f, 1.0f);
	const FVector RevisionLaneCenters[] = {
		FVector(-3600.0f, 0.0f, CenterZForBlockBottom(0.65f, 0.035f)),
		FVector(-1200.0f, 0.0f, CenterZForBlockBottom(0.70f, 0.035f)),
		FVector(1450.0f, 0.0f, CenterZForBlockBottom(0.75f, 0.035f)),
		FVector(3700.0f, 0.0f, CenterZForBlockBottom(0.80f, 0.035f))
	};
	for (int32 LaneIndex = 0; LaneIndex < UE_ARRAY_COUNT(RevisionLaneCenters); ++LaneIndex)
	{
		const FVector Lane = RevisionLaneCenters[LaneIndex];
		const FLinearColor LaneTint = (LaneIndex % 3 == 0) ? RevisionCyan : ((LaneIndex % 3 == 1) ? RevisionAmber : RevisionViolet);
		SpawnBlock(Lane, FVector(4.8f, 2.7f, 0.035f), LaneTint, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(Lane + FVector(0.0f, -185.0f, 1.0f), FVector(4.4f, 0.08f, 0.045f), RevisionAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(Lane + FVector(0.0f, 185.0f, 1.0f), FVector(4.4f, 0.08f, 0.045f), RevisionAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	}
	if (ABHObjectiveStation* HiddenSwitch = GetWorld()->SpawnActor<ABHObjectiveStation>(FVector(-5750.0f, 3825.0f, 88.0f), FRotator(0.0f, -35.0f, 0.0f)))
	{
		HiddenSwitch->ConfigureTeacherMirrorTrapNode();
	}

	for (int32 Row = -3; Row <= 3; ++Row)
	{
		for (int32 Col = -2; Col <= 2; ++Col)
		{
			if ((Row + Col) % 2 == 0)
			{
				SpawnBlock(FVector(Row * 950.0f, Col * 780.0f, 80.0f), FVector(1.1f, 3.5f, 1.7f), Steel, FRotator(0.0f, 12.0f * Row, 0.0f), true, EBHBlockMaterial::DiamondPlate);
				SpawnBlock(FVector(Row * 950.0f + 165.0f, Col * 780.0f, 245.0f), FVector(0.28f, 3.7f, 0.16f), Rust, FRotator(0.0f, 12.0f * Row, 0.0f), false, EBHBlockMaterial::RustedMetal);
			}
		}
	}

	for (int32 I = 0; I < 13; ++I)
	{
		const float X = -5700.0f + I * 950.0f;
		SpawnBlock(FVector(X, -4300.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(4.8f, 0.08f, 0.08f), Warning, FRotator::ZeroRotator, false);
		SpawnBlock(FVector(X + 260.0f, -4300.0f, CenterZForBlockBottom(0.5f, 0.09f)), FVector(1.8f, 0.09f, 0.09f), Cable, FRotator(0.0f, 35.0f, 0.0f), false);
		SpawnBlock(FVector(X, 4300.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(4.8f, 0.08f, 0.08f), Warning, FRotator::ZeroRotator, false);
		SpawnBlock(FVector(X + 260.0f, 4300.0f, CenterZForBlockBottom(0.5f, 0.09f)), FVector(1.8f, 0.09f, 0.09f), Cable, FRotator(0.0f, -35.0f, 0.0f), false);
	}

	const TArray<FVector> ClutterCenters = {
		FVector(-5200.0f, -3300.0f, 40.0f), FVector(-3600.0f, -3300.0f, 40.0f), FVector(-1200.0f, -3500.0f, 40.0f), FVector(1200.0f, -3500.0f, 40.0f),
		FVector(3450.0f, -3300.0f, 40.0f), FVector(5200.0f, -2700.0f, 40.0f), FVector(-5200.0f, 3200.0f, 40.0f), FVector(-3300.0f, 3400.0f, 40.0f),
		FVector(-900.0f, 3500.0f, 40.0f), FVector(1550.0f, 3500.0f, 40.0f), FVector(3900.0f, 3300.0f, 40.0f), FVector(5300.0f, 2300.0f, 40.0f)
	};
	AddIndustrialClutter(ClutterCenters, Rust);

	const TArray<FVector> Lockers = {
		FVector(-5750.0f, -4050.0f, 100.0f), FVector(-5000.0f, -4050.0f, 100.0f), FVector(-3100.0f, -4050.0f, 100.0f), FVector(-1800.0f, -4050.0f, 100.0f),
		FVector(-250.0f, -4050.0f, 100.0f), FVector(1250.0f, -4050.0f, 100.0f), FVector(3000.0f, -4050.0f, 100.0f), FVector(5000.0f, -4050.0f, 100.0f),
		FVector(-5750.0f, 4050.0f, 100.0f), FVector(-5000.0f, 4050.0f, 100.0f), FVector(-3100.0f, 4050.0f, 100.0f), FVector(-1800.0f, 4050.0f, 100.0f),
		FVector(-250.0f, 4050.0f, 100.0f), FVector(1250.0f, 4050.0f, 100.0f), FVector(3000.0f, 4050.0f, 100.0f), FVector(5000.0f, 4050.0f, 100.0f),
		FVector(-5850.0f, -900.0f, 100.0f), FVector(-5850.0f, 900.0f, 100.0f), FVector(5850.0f, -900.0f, 100.0f), FVector(5850.0f, 900.0f, 100.0f),
		FVector(2200.0f, -400.0f, 100.0f), FVector(2200.0f, 720.0f, 100.0f), FVector(-3100.0f, -640.0f, 100.0f), FVector(-3100.0f, 760.0f, 100.0f)
	};
	for (const FVector& Location : Lockers)
	{
		GetWorld()->SpawnActor<ABHLocker>(Location, FRotator::ZeroRotator);
	}

	const FVector Batteries[] = {
		FVector(-5200.0f, -1650.0f, 70.0f), FVector(-5200.0f, 1650.0f, 70.0f), FVector(-1500.0f, -3650.0f, 70.0f), FVector(-1500.0f, 3650.0f, 70.0f),
		FVector(900.0f, -3650.0f, 70.0f), FVector(900.0f, 3650.0f, 70.0f), FVector(3300.0f, -1450.0f, 70.0f), FVector(3300.0f, 1450.0f, 70.0f),
		FVector(5400.0f, -600.0f, 70.0f), FVector(5400.0f, 1900.0f, 70.0f)
	};
	for (const FVector& Location : Batteries)
	{
		GetWorld()->SpawnActor<ABHBatteryPickup>(Location, FRotator(0.0f, 90.0f, 0.0f));
	}

	const FVector Alarms[] = {
		FVector(-5600.0f, -2400.0f, 105.0f), FVector(-5600.0f, 2400.0f, 105.0f),
		FVector(-850.0f, -2500.0f, 105.0f), FVector(850.0f, 2500.0f, 105.0f),
		FVector(3650.0f, -2500.0f, 105.0f), FVector(3650.0f, 2500.0f, 105.0f),
		FVector(5600.0f, -900.0f, 105.0f), FVector(5600.0f, 900.0f, 105.0f)
	};
	for (const FVector& Location : Alarms)
	{
		GetWorld()->SpawnActor<ABHPanicAlarm>(Location, FRotator::ZeroRotator);
	}

	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FVector(6000.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	SpawnBlock(FVector(5968.0f, -720.0f, 170.0f), FVector(0.04f, 1.35f, 0.70f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(5968.0f, 720.0f, 170.0f), FVector(0.04f, 1.35f, 0.70f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	const FLinearColor ExitGreen(0.05f, 0.84f, 0.42f, 1.0f);
	const FLinearColor ExitAmber(0.98f, 0.76f, 0.12f, 1.0f);
	const FLinearColor ExitBlue(0.10f, 0.52f, 0.92f, 1.0f);
	const float SubstationExitFloorZ = CenterZForBlockBottom(0.72f, 0.045f);
	SpawnBlock(FVector(5550.0f, 0.0f, SubstationExitFloorZ), FVector(10.2f, 3.0f, 0.045f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(5550.0f, -360.0f, SubstationExitFloorZ + 1.5f), FVector(9.4f, 0.08f, 0.055f), ExitAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(5550.0f, 360.0f, SubstationExitFloorZ + 1.5f), FVector(9.4f, 0.08f, 0.055f), ExitAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(5968.0f, 0.0f, 260.0f), FVector(0.09f, 2.85f, 0.18f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(5968.0f, 0.0f, 70.0f), FVector(0.09f, 2.85f, 0.12f), ExitAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(5460.0f, -455.0f, 96.0f), FVector(0.16f, 0.16f, 1.28f), ExitBlue, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(5460.0f, 455.0f, 96.0f), FVector(0.16f, 0.16f, 1.28f), ExitBlue, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-5850.0f, -2500.0f, CenterZForBlockBottom(0.5f, 0.04f)), FVector(6.5f, 1.4f, 0.04f), Steel, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(-5850.0f, 2500.0f, CenterZForBlockBottom(0.5f, 0.04f)), FVector(6.5f, 1.4f, 0.04f), Steel, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(4200.0f, -2500.0f, CenterZForBlockBottom(0.5f, 0.04f)), FVector(7.5f, 1.4f, 0.04f), Steel, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(4200.0f, 2500.0f, CenterZForBlockBottom(0.5f, 0.04f)), FVector(7.5f, 1.4f, 0.04f), Steel, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);

	for (int32 Circuit = 1; Circuit <= 8; ++Circuit)
	{
		const float X = -5200.0f + Circuit * 1250.0f;
		if (ABHFlickerLight* LightA = GetWorld()->SpawnActor<ABHFlickerLight>(FVector(X, -3150.0f, 285.0f), FRotator::ZeroRotator))
		{
			LightA->Configure(Circuit, Circuit % 2 == 0 ? FLinearColor(0.55f, 0.90f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.72f, 0.38f, 1.0f), 900.0f, 1150.0f);
			FlickerLights.Add(LightA);
		}
		if (ABHFlickerLight* LightB = GetWorld()->SpawnActor<ABHFlickerLight>(FVector(X, 3150.0f, 285.0f), FRotator::ZeroRotator))
		{
			LightB->Configure(Circuit, Circuit % 3 == 0 ? FLinearColor(0.80f, 1.0f, 0.62f, 1.0f) : FLinearColor(0.70f, 0.82f, 1.0f, 1.0f), 820.0f, 1150.0f);
			FlickerLights.Add(LightB);
		}
		if (ABHPowerSwitch* Switch = GetWorld()->SpawnActor<ABHPowerSwitch>(FVector(-5900.0f + Circuit * 1300.0f, -4450.0f, 120.0f), FRotator::ZeroRotator))
		{
			Switch->Configure(Circuit, FText::FromString(FString::Printf(TEXT("Toggle Substation Circuit %d"), Circuit)));
		}
	}

	for (const FVector& Location : {FVector(-900.0f, 0.0f, 120.0f), FVector(1800.0f, 0.0f, 120.0f), FVector(3900.0f, 0.0f, 120.0f), FVector(-2500.0f, 2500.0f, 120.0f)})
	{
		if (ABHSecurityShutter* Shutter = GetWorld()->SpawnActor<ABHSecurityShutter>(Location, FRotator(0.0f, 90.0f, 0.0f)))
		{
			Shutter->Configure(200);
		}
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(-5850.0f, 0.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		Terminal->Configure(200, FText::FromString(TEXT("Open Substation Shutters")));
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(5800.0f, 2100.0f, 110.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		Terminal->Configure(200, FText::FromString(TEXT("Open Substation Shutters")));
	}

	AddSurfaceDetailGrid(6100.0f, 4600.0f, FLinearColor(0.020f, 0.022f, 0.024f, 1.0f));

	SpawnAmbient(FVector(-5200.0f, 0.0f, 160.0f), 34.0f, 0.20f, 0.05f, 0.12f);
	SpawnAmbient(FVector(-1200.0f, -3200.0f, 160.0f), 56.0f, 0.15f, 0.035f, 0.20f);
	SpawnAmbient(FVector(1200.0f, 3200.0f, 160.0f), 72.0f, 0.14f, 0.025f, 0.33f);
	SpawnAmbient(FVector(4300.0f, -2800.0f, 160.0f), 42.0f, 0.16f, 0.04f, 0.16f);
	SpawnAmbient(FVector(5300.0f, 1200.0f, 160.0f), 92.0f, 0.11f, 0.025f, 0.45f);

	SpawnBlock(FVector(-5600.0f, 0.0f, CenterZForBlockBottom(0.5f, 0.06f)), FVector(3.5f, 4.0f, 0.06f), ControlBlue, FRotator::ZeroRotator, false);
	SpawnBlock(FVector(5650.0f, 0.0f, CenterZForBlockBottom(0.5f, 0.06f)), FVector(4.0f, 6.0f, 0.06f), SickGreen, FRotator::ZeroRotator, false);

	ScarePoints.Append({
		FVector(-5550.0f, -3650.0f, 110.0f),
		FVector(-5400.0f, 3650.0f, 110.0f),
		FVector(-850.0f, -3800.0f, 110.0f),
		FVector(850.0f, 3800.0f, 110.0f),
		FVector(3300.0f, -3750.0f, 110.0f),
		FVector(5450.0f, 3300.0f, 110.0f),
		FVector(5200.0f, 350.0f, 110.0f),
		FVector(-5850.0f, -900.0f, 110.0f),
		FVector(5850.0f, 900.0f, 110.0f),
		FVector(1800.0f, 0.0f, 110.0f),
		FVector(-2500.0f, 2500.0f, 110.0f)
	});

	AddMoodPass(FLinearColor(0.018f, 0.045f, 0.052f, 1.0f), 0.010f, 0.50f, 0.10f);
	AddClassroomHorrorPass();
	BuildRuntimeNavigation();
}

void ABHGameMode::BuildRuntimeNavigation()
{
	if (!GetWorld())
	{
		return;
	}

	bRuntimeNavigationReady = false;

	const bool bSubstation = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase);
	const bool bFoggrounds = RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	const FVector BoundsSize = bFoggrounds ? FVector(17000.0f, 13600.0f, 1000.0f) : (bSubstation ? FVector(13200.0f, 10000.0f, 800.0f) : FVector(12000.0f, 9600.0f, 800.0f));
	const FVector BoundsCenter(0.0f, 0.0f, BoundsSize.Z * 0.5f);

	if (!RuntimeNavBounds)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		RuntimeNavBounds = GetWorld()->SpawnActor<ANavMeshBoundsVolume>(BoundsCenter, FRotator::ZeroRotator, SpawnParams);
	}

	if (RuntimeNavBounds)
	{
		if (UBrushComponent* BrushComponent = RuntimeNavBounds->GetBrushComponent())
		{
			BrushComponent->SetMobility(EComponentMobility::Movable);
			BrushComponent->SetCanEverAffectNavigation(true);
		}
		RuntimeNavBounds->SetActorLocation(BoundsCenter);
		RuntimeNavBounds->SetActorScale3D(FVector::OneVector);

		UBoxComponent* BoundsBox = RuntimeNavBounds->FindComponentByClass<UBoxComponent>();
		if (!BoundsBox)
		{
			BoundsBox = NewObject<UBoxComponent>(RuntimeNavBounds, TEXT("RuntimeNavigationBounds"));
			BoundsBox->SetupAttachment(RuntimeNavBounds->GetRootComponent());
			RuntimeNavBounds->AddInstanceComponent(BoundsBox);
			RuntimeNavBounds->AddOwnedComponent(BoundsBox);
			BoundsBox->RegisterComponentWithWorld(GetWorld());
		}
		if (BoundsBox)
		{
			BoundsBox->SetMobility(EComponentMobility::Movable);
			BoundsBox->SetRelativeLocation(FVector::ZeroVector);
			BoundsBox->SetBoxExtent(BoundsSize * 0.5f, true);
			BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BoundsBox->SetHiddenInGame(true);
			BoundsBox->SetCanEverAffectNavigation(false);
			BoundsBox->UpdateBounds();
		}

		const FBox RuntimeBounds = RuntimeNavBounds->GetComponentsBoundingBox(true);
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt runtime nav bounds: %s valid=%s"), *RuntimeBounds.ToString(), RuntimeBounds.IsValid ? TEXT("true") : TEXT("false"));

		if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			NavSystem->OnNavigationBoundsUpdated(RuntimeNavBounds);
			FTimerHandle NavBuildHandle;
			GetWorldTimerManager().SetTimer(NavBuildHandle, this, &ABHGameMode::CompleteRuntimeNavigationBuild, 0.25f, false);
		}
	}
}

void ABHGameMode::CompleteRuntimeNavigationBuild()
{
	if (!GetWorld() || !RuntimeNavBounds)
	{
		return;
	}

	const FBox RuntimeBounds = RuntimeNavBounds->GetComponentsBoundingBox(true);
	int32 UpdatedActors = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == RuntimeNavBounds)
		{
			continue;
		}

		const FBox ActorBounds = Actor->GetComponentsBoundingBox(true);
		if (ActorBounds.IsValid && RuntimeBounds.Intersect(ActorBounds))
		{
			UNavigationSystemV1::UpdateActorAndComponentsInNavOctree(*Actor, false);
			++UpdatedActors;
		}
	}

	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSystem->AddDirtyArea(RuntimeBounds, ENavigationDirtyFlag::All | ENavigationDirtyFlag::NavigationBounds, TEXT("BlackoutHunt runtime nav build"));
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt runtime nav build: updated %d actors in %s"), UpdatedActors, *RuntimeBounds.ToString());
		NavSystem->Build();
		bRuntimeNavigationReady = true;
	}
}

void ABHGameMode::AddFacilityDetailPass()
{
	AddSurfaceDetailGrid(5500.0f, 4400.0f, FLinearColor(0.020f, 0.022f, 0.024f, 1.0f));

	const FLinearColor Trim(0.04f, 0.05f, 0.055f, 1.0f);
	const FLinearColor Rust(0.32f, 0.13f, 0.07f, 1.0f);
	const FLinearColor Hazard(0.78f, 0.55f, 0.08f, 1.0f);
	const FLinearColor Pipe(0.12f, 0.15f, 0.16f, 1.0f);
	const FLinearColor Grime(0.015f, 0.018f, 0.016f, 1.0f);

	SpawnBlock(FVector(3600.0f, -2940.0f, CenterZForBlockBottom(0.5f, 0.035f)), FVector(13.0f, 11.0f, 0.035f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
	SpawnBlock(FVector(-3600.0f, 2780.0f, CenterZForBlockBottom(0.5f, 0.035f)), FVector(13.0f, 10.0f, 0.035f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
	SpawnBlock(FVector(-3500.0f, -2950.0f, CenterZForBlockBottom(0.5f, 0.035f)), FVector(10.0f, 8.0f, 0.035f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(650.0f, -2950.0f, CenterZForBlockBottom(0.5f, 0.035f)), FVector(10.0f, 8.0f, 0.035f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);

	for (float Y = -3100.0f; Y <= 3100.0f; Y += 620.0f)
	{
		SpawnBlock(FVector(-4455.0f, Y, 210.0f), FVector(0.08f, 2.2f, 0.16f), Pipe, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(4455.0f, Y, 210.0f), FVector(0.08f, 2.2f, 0.16f), Pipe, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	}
	for (float X = -4000.0f; X <= 4000.0f; X += 800.0f)
	{
		SpawnBlock(FVector(X, -3540.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(2.2f, 0.08f, 0.08f), Hazard, FRotator(0.0f, 28.0f, 0.0f), false);
		SpawnBlock(FVector(X + 260.0f, -3540.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(2.2f, 0.08f, 0.08f), Trim, FRotator(0.0f, -28.0f, 0.0f), false);
		SpawnBlock(FVector(X, 3540.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(2.2f, 0.08f, 0.08f), Hazard, FRotator(0.0f, -28.0f, 0.0f), false);
		SpawnBlock(FVector(X + 260.0f, 3540.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(2.2f, 0.08f, 0.08f), Trim, FRotator(0.0f, 28.0f, 0.0f), false);
	}

	const TArray<FVector> ExtraClutter = {
		FVector(-3800.0f, -2100.0f, 40.0f), FVector(-2450.0f, -1800.0f, 40.0f), FVector(-950.0f, -2850.0f, 40.0f),
		FVector(950.0f, -2850.0f, 40.0f), FVector(2350.0f, -1950.0f, 40.0f), FVector(3750.0f, -2100.0f, 40.0f),
		FVector(-3900.0f, 2100.0f, 40.0f), FVector(-2500.0f, 1850.0f, 40.0f), FVector(-950.0f, 2850.0f, 40.0f),
		FVector(950.0f, 2850.0f, 40.0f), FVector(2300.0f, 1950.0f, 40.0f), FVector(3750.0f, 2100.0f, 40.0f)
	};
	AddIndustrialClutter(ExtraClutter, Rust);

	for (const FVector& Location : {FVector(-3300.0f, 300.0f, 100.0f), FVector(-2500.0f, 300.0f, 100.0f), FVector(-900.0f, -900.0f, 100.0f), FVector(900.0f, -900.0f, 100.0f), FVector(2600.0f, 3150.0f, 100.0f), FVector(3200.0f, 3150.0f, 100.0f), FVector(4300.0f, 1350.0f, 100.0f), FVector(-4300.0f, -1350.0f, 100.0f)})
	{
		GetWorld()->SpawnActor<ABHLocker>(Location, FRotator::ZeroRotator);
	}

	const TArray<TPair<FVector, FRotator>> ExtraBreakers = {
		{FVector(-350.0f, 3180.0f, 80.0f), FRotator::ZeroRotator},
		{FVector(2450.0f, -3180.0f, 80.0f), FRotator(0.0f, 180.0f, 0.0f)}
	};
	for (const TPair<FVector, FRotator>& Breaker : ExtraBreakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	for (const FVector& Smear : {FVector(-1250.0f, -400.0f, 4.0f), FVector(1450.0f, 450.0f, 4.0f), FVector(3200.0f, -650.0f, 4.0f), FVector(-3350.0f, 650.0f, 4.0f), FVector(200.0f, 2100.0f, 4.0f), FVector(200.0f, -2100.0f, 4.0f)})
	{
		SpawnBlock(Smear, FVector(3.5f, 1.7f, 0.035f), Grime, FRotator(0.0f, FMath::FRandRange(-24.0f, 24.0f), 0.0f), false, EBHBlockMaterial::RustedMetal);
	}
}

void ABHGameMode::AddClassroomHorrorPass()
{
	const FLinearColor Blackboard(0.015f, 0.075f, 0.055f, 1.0f);
	const FLinearColor Chalk(0.82f, 0.86f, 0.78f, 1.0f);
	const FLinearColor Desk(0.22f, 0.17f, 0.11f, 1.0f);
	const FLinearColor Chair(0.09f, 0.10f, 0.11f, 1.0f);
	const FLinearColor RedMark(0.72f, 0.035f, 0.025f, 1.0f);

	auto SpawnClassroom = [this, Blackboard, Chalk, Desk, Chair, RedMark](const FVector& Origin, float YawDegrees)
	{
		const FRotator Rotation(0.0f, YawDegrees, 0.0f);
		const auto Local = [&Origin, &Rotation](float X, float Y, float Z)
		{
			return Origin + Rotation.RotateVector(FVector(X, Y, Z));
		};

		SpawnBlock(Local(0.0f, -300.0f, 145.0f), FVector(3.4f, 0.05f, 0.78f), Blackboard, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(Local(-145.0f, -303.0f, 178.0f), FVector(0.85f, 0.035f, 0.035f), Chalk, FRotator(0.0f, YawDegrees + 8.0f, 0.0f), false);
		SpawnBlock(Local(80.0f, -304.0f, 120.0f), FVector(0.55f, 0.035f, 0.035f), RedMark, FRotator(0.0f, YawDegrees - 12.0f, 0.0f), false);
		SpawnBlock(Local(0.0f, -95.0f, 42.0f), FVector(2.3f, 0.78f, 0.46f), Desk, Rotation, false, EBHBlockMaterial::PaintedMetal);

		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				const float X = -260.0f + Col * 260.0f;
				const float Y = 150.0f + Row * 210.0f;
				const float Skew = (Row + Col) % 2 == 0 ? 6.0f : -5.0f;
				SpawnBlock(Local(X, Y, 32.0f), FVector(0.95f, 0.62f, 0.32f), Desk, FRotator(0.0f, YawDegrees + Skew, 0.0f), false, EBHBlockMaterial::PaintedMetal);
				SpawnBlock(Local(X, Y + 72.0f, 58.0f), FVector(0.72f, 0.10f, 0.72f), Chair, FRotator(0.0f, YawDegrees + Skew, 0.0f), false, EBHBlockMaterial::RustedMetal);
			}
		}
	};

	SpawnClassroom(FVector(-3600.0f, -450.0f, 0.0f), 90.0f);
	SpawnClassroom(FVector(3620.0f, 1600.0f, 0.0f), -90.0f);
	SpawnClassroom(FVector(-900.0f, 2850.0f, 0.0f), 180.0f);
	SpawnClassroom(FVector(1200.0f, -3000.0f, 0.0f), 0.0f);
}

void ABHGameMode::AddSurfaceDetailGrid(float HalfX, float HalfY, const FLinearColor& LineTint)
{
	for (float X = -HalfX + 600.0f; X < HalfX; X += 600.0f)
	{
		SpawnBlock(FVector(X, 0.0f, 2.0f), FVector(0.025f, HalfY / 100.0f, 0.025f), LineTint, FRotator::ZeroRotator, false);
	}
	for (float Y = -HalfY + 600.0f; Y < HalfY; Y += 600.0f)
	{
		SpawnBlock(FVector(0.0f, Y, 2.5f), FVector(HalfX / 100.0f, 0.025f, 0.025f), LineTint, FRotator::ZeroRotator, false);
	}
}

void ABHGameMode::AddIndustrialClutter(const TArray<FVector>& Centers, const FLinearColor& Tint)
{
	const FLinearColor DarkMetal(0.08f, 0.09f, 0.095f, 1.0f);
	for (int32 Index = 0; Index < Centers.Num(); ++Index)
	{
		const FVector& Center = Centers[Index];
		const float Angle = (Index * 37) % 180;
		SpawnBlock(Center, FVector(0.85f, 0.85f, 0.85f), Tint, FRotator(0.0f, Angle, 0.0f), true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(Center + FVector(115.0f, -85.0f, 25.0f), FVector(0.55f, 0.42f, 0.46f), DarkMetal, FRotator(0.0f, Angle + 18.0f, 0.0f), true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(Center + FVector(-95.0f, 115.0f, 75.0f), FVector(0.35f, 0.35f, 1.25f), DarkMetal, FRotator(0.0f, Angle - 25.0f, 0.0f), true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(Center + FVector(20.0f, 0.0f, 165.0f), FVector(0.10f, 1.4f, 0.10f), DarkMetal, FRotator(0.0f, Angle, 0.0f), false, EBHBlockMaterial::PaintedMetal);
	}
}

void ABHGameMode::AddMoodPass(const FLinearColor& FogColor, float FogDensity, float VignetteIntensity, float FilmGrainIntensity)
{
	const bool bFoggrounds = RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	const bool bExtremeFog = bFoggrounds && RuntimeFogPreset == EBHFogPreset::Extreme;
	const bool bHeavyFog = bFoggrounds && RuntimeFogPreset == EBHFogPreset::Heavy;
	if (APostProcessVolume* PostProcess = GetWorld()->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		PostProcess->bUnbound = true;
		PostProcess->Settings.bOverride_VignetteIntensity = true;
		PostProcess->Settings.VignetteIntensity = VignetteIntensity;
		PostProcess->Settings.bOverride_FilmGrainIntensity = true;
		PostProcess->Settings.FilmGrainIntensity = FilmGrainIntensity;
		PostProcess->Settings.bOverride_ColorSaturation = true;
		PostProcess->Settings.ColorSaturation = bExtremeFog ? FVector4(0.88f, 0.94f, 0.96f, 1.0f) : FVector4(0.78f, 0.84f, 0.90f, 1.0f);
		PostProcess->Settings.bOverride_ColorContrast = true;
		PostProcess->Settings.ColorContrast = bExtremeFog ? FVector4(1.02f, 1.02f, 1.00f, 1.0f) : FVector4(1.12f, 1.10f, 1.06f, 1.0f);
		PostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
		PostProcess->Settings.AutoExposureMinBrightness = bExtremeFog ? 0.025f : (bHeavyFog ? 0.024f : 0.030f);
		PostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
		PostProcess->Settings.AutoExposureMaxBrightness = bExtremeFog ? 0.85f : (bHeavyFog ? 0.78f : 0.95f);
		if (bFoggrounds)
		{
			PostProcess->Settings.bOverride_AutoExposureBias = true;
			PostProcess->Settings.AutoExposureBias = bExtremeFog ? -0.32f : (bHeavyFog ? -0.24f : -0.14f);
			PostProcess->Settings.bOverride_SceneColorTint = true;
			PostProcess->Settings.SceneColorTint = bExtremeFog ? FLinearColor(0.94f, 0.99f, 1.04f, 1.0f) : FLinearColor(0.92f, 0.96f, 1.02f, 1.0f);
			PostProcess->Settings.bOverride_IndirectLightingColor = true;
			PostProcess->Settings.IndirectLightingColor = FLinearColor(0.12f, 0.18f, 0.28f, 1.0f);
			PostProcess->Settings.bOverride_IndirectLightingIntensity = true;
			PostProcess->Settings.IndirectLightingIntensity = bExtremeFog ? 0.48f : (bHeavyFog ? 0.38f : 0.30f);
		}
	}

	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetFogDensity(FogDensity);
			FogComponent->SetFogHeightFalloff(bExtremeFog ? 0.085f : 0.075f);
			FogComponent->SetFogInscatteringColor(FogColor);
			FogComponent->SetFogMaxOpacity(bExtremeFog ? 0.78f : 1.0f);
			FogComponent->SetStartDistance(0.0f);
			FogComponent->SetEndDistance(0.0f);
			FogComponent->SetFogCutoffDistance(0.0f);
			FogComponent->SetDirectionalInscatteringExponent(8.0f);
			FogComponent->SetDirectionalInscatteringStartDistance(0.0f);
			const float DirectionalInscatterScale = bExtremeFog ? 1.75f : 1.45f;
			FogComponent->SetDirectionalInscatteringColor(FLinearColor(FogColor.R * DirectionalInscatterScale, FogColor.G * DirectionalInscatterScale, FogColor.B * DirectionalInscatterScale, 1.0f));
			FogComponent->SetVolumetricFog(true);
			FogComponent->SetVolumetricFogScatteringDistribution(bExtremeFog ? 0.38f : 0.45f);
			FogComponent->SetVolumetricFogAlbedo(FColor(
				FMath::Clamp(FMath::RoundToInt(FogColor.R * 255.0f), 0, 255),
				FMath::Clamp(FMath::RoundToInt(FogColor.G * 255.0f), 0, 255),
				FMath::Clamp(FMath::RoundToInt(FogColor.B * 255.0f), 0, 255)));
			FogComponent->SetVolumetricFogEmissive(FLinearColor(FogColor.R * (bExtremeFog ? 0.030f : 0.018f), FogColor.G * (bExtremeFog ? 0.030f : 0.018f), FogColor.B * (bExtremeFog ? 0.030f : 0.018f), 1.0f));
			FogComponent->SetVolumetricFogExtinctionScale(FMath::Clamp(FogDensity * 18.0f, 0.55f, bExtremeFog ? 2.65f : 3.25f));
			FogComponent->SetVolumetricFogDistance(bExtremeFog ? 4200.0f : 5200.0f);
			FogComponent->SetVolumetricFogStartDistance(0.0f);
			FogComponent->SetVolumetricFogNearFadeInDistance(bExtremeFog ? 75.0f : 55.0f);
		}
	}
}

void ABHGameMode::SpawnBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rotation, bool bCollides, EBHBlockMaterial Material)
{
	if (ABHBlockActor* Block = GetWorld()->SpawnActor<ABHBlockActor>(Location, Rotation))
	{
		Block->SetActorScale3D(Scale);
		Block->SetVisualTint(Tint);
		Block->SetBlockMaterial(Material);
		Block->SetBlockCollisionEnabled(bCollides);
	}
}

void ABHGameMode::SpawnAmbient(const FVector& Location, float Frequency, float Volume, float Noise, float Pulse, float LifeSpan)
{
	if (ABHAmbientEmitter* Emitter = GetWorld()->SpawnActor<ABHAmbientEmitter>(Location, FRotator::ZeroRotator))
	{
		Emitter->Configure(Frequency, Volume, Noise, Pulse);
		if (LifeSpan > 0.0f)
		{
			Emitter->SetLifeSpan(LifeSpan);
		}
	}
}

void ABHGameMode::PrepareRoundDirector()
{
	RoundSeed = FMath::Rand();
	FRandomStream Stream(RoundSeed);

	BreakerActors.RemoveAll([](const TObjectPtr<ABHBreaker>& Breaker) { return !IsValid(Breaker); });
	DoorActors.RemoveAll([](const TObjectPtr<ABHDoor>& Door) { return !IsValid(Door); });
	ExitGates.RemoveAll([](const TObjectPtr<ABHExitGate>& ExitGate) { return !IsValid(ExitGate); });
	FlickerLights.RemoveAll([](const TObjectPtr<ABHFlickerLight>& Light) { return !IsValid(Light); });
	ObjectiveStations.RemoveAll([](const TObjectPtr<ABHObjectiveStation>& Station) { return !IsValid(Station); });

	const EBHRoundModifier ChosenModifier = bPracticeMode ? PracticeRoundModifier : ChooseRoundModifier(Stream);
	NoiseRadiusMultiplier = ChosenModifier == EBHRoundModifier::LoudFooting ? 1.55f : 1.0f;
	if (bPartyPace)
	{
		NoiseRadiusMultiplier *= 1.25f;
	}

	TArray<int32> BreakerOrder;
	for (int32 Index = 0; Index < BreakerActors.Num(); ++Index)
	{
		BreakerOrder.Add(Index);
	}
	for (int32 Index = BreakerOrder.Num() - 1; Index > 0; --Index)
	{
		BreakerOrder.Swap(Index, Stream.RandRange(0, Index));
	}

	ActiveSideObjectiveCount = FMath::Clamp(ObjectiveIntensity, 0, ObjectiveStations.Num());
	if (bTestMode)
	{
		ActiveSideObjectiveCount = ObjectiveStations.Num();
	}
	else if (bRevisionMode)
	{
		int32 StudentCount = CountActiveRevisionStudents(GameState, true);
		if (StudentCount <= 0)
		{
			StudentCount = CountActiveRevisionStudents(GameState, false);
		}
		const int32 RevisionNodeTarget = StudentCount >= 10 ? 10 : (StudentCount >= 6 ? 8 : 6);
		ActiveSideObjectiveCount = FMath::Min(RevisionNodeTarget, ObjectiveStations.Num());
	}
	else if (bPartyPace && ObjectiveIntensity > 0)
	{
		ActiveSideObjectiveCount = FMath::Max(1, ObjectiveIntensity - 1);
	}
	ActiveBreakerCount = bTestMode ? BreakerActors.Num() : (bRevisionMode ? 0 : FMath::Clamp(RequiredBreakers - FMath::Min(2, ActiveSideObjectiveCount), 2, FMath::Max(1, BreakerActors.Num())));
	TSet<int32> ActiveBreakerIndexes;
	for (int32 Index = 0; Index < ActiveBreakerCount && BreakerOrder.IsValidIndex(Index); ++Index)
	{
		ActiveBreakerIndexes.Add(BreakerOrder[Index]);
	}

	for (int32 Index = 0; Index < BreakerActors.Num(); ++Index)
	{
		if (ABHBreaker* Breaker = BreakerActors[Index])
		{
			Breaker->SetDirectorActive(ActiveBreakerIndexes.Contains(Index));
		}
	}

	TSet<int32> ActiveStationIndexes;
	if (bRevisionMode)
	{
		const EBHObjectiveStationType TopicStationTypes[4] = {
			EBHObjectiveStationType::Valve,
			EBHObjectiveStationType::Terminal,
			EBHObjectiveStationType::Antenna,
			EBHObjectiveStationType::Evidence
		};
		for (int32 TopicSlot = 0; TopicSlot < 4; ++TopicSlot)
		{
			const EBHPhysicsTopic Topic = FBHRevisionQuestionBank::TopicForStationType(TopicStationTypes[TopicSlot]);
			if ((RevisionTopicMask & FBHRevisionQuestionBank::TopicMaskBit(Topic)) == 0)
			{
				continue;
			}

			TArray<int32> Candidates;
			for (int32 StationIndex = 0; StationIndex < ObjectiveStations.Num(); ++StationIndex)
			{
				ABHObjectiveStation* Station = ObjectiveStations[StationIndex];
				if (Station && Station->GetStationType() == TopicStationTypes[TopicSlot])
				{
					Candidates.Add(StationIndex);
				}
			}

			if (Candidates.IsValidIndex(0))
			{
				ActiveStationIndexes.Add(Candidates[Stream.RandRange(0, Candidates.Num() - 1)]);
			}
		}

		if (ActiveStationIndexes.Num() < ActiveSideObjectiveCount)
		{
			for (int32 StationIndex = ObjectiveStations.Num() - 1; StationIndex >= 0 && ActiveStationIndexes.Num() < ActiveSideObjectiveCount; --StationIndex)
			{
				ActiveStationIndexes.Add(StationIndex);
			}
		}
		ActiveSideObjectiveCount = ActiveStationIndexes.Num();
	}
	else
	{
		TArray<int32> StationOrder;
		for (int32 Index = 0; Index < ObjectiveStations.Num(); ++Index)
		{
			StationOrder.Add(Index);
		}
		for (int32 Index = StationOrder.Num() - 1; Index > 0; --Index)
		{
			StationOrder.Swap(Index, Stream.RandRange(0, Index));
		}
		for (int32 Index = 0; Index < ActiveSideObjectiveCount && StationOrder.IsValidIndex(Index); ++Index)
		{
			ActiveStationIndexes.Add(StationOrder[Index]);
		}
	}
	for (int32 Index = 0; Index < ObjectiveStations.Num(); ++Index)
	{
		if (ABHObjectiveStation* Station = ObjectiveStations[Index])
		{
			Station->ConfigureRevisionCounterNode(EBHRevisionCounterNodeType::None);
			Station->SetDirectorActive(ActiveStationIndexes.Contains(Index));
		}
	}
	if (bRevisionMode)
	{
		TArray<int32> CounterCandidates = ActiveStationIndexes.Array();
		CounterCandidates.Sort(TGreater<int32>());
		if (CounterCandidates.IsValidIndex(0))
		{
			if (ABHObjectiveStation* PeerReview = ObjectiveStations[CounterCandidates[0]])
			{
				PeerReview->ConfigureRevisionCounterNode(EBHRevisionCounterNodeType::PeerReview);
			}
		}
		if (CounterCandidates.IsValidIndex(1))
		{
			if (ABHObjectiveStation* DemonstrationTrap = ObjectiveStations[CounterCandidates[1]])
			{
				DemonstrationTrap->ConfigureRevisionCounterNode(EBHRevisionCounterNodeType::DemonstrationTrap);
			}
		}
	}

	const int32 ActiveExitIndex = ExitGates.Num() > 0 ? Stream.RandRange(0, ExitGates.Num() - 1) : INDEX_NONE;
	for (int32 Index = 0; Index < ExitGates.Num(); ++Index)
	{
		if (ABHExitGate* ExitGate = ExitGates[Index])
		{
			ExitGate->SetDirectorActive(bTestMode || Index == ActiveExitIndex);
		}
	}

	for (int32 Index = 0; Index < DoorActors.Num(); ++Index)
	{
		if (ABHDoor* Door = DoorActors[Index])
		{
			const float OpenChance = ChosenModifier == EBHRoundModifier::JammedDoors ? 0.08f : 0.28f;
			Door->SetOpen(Stream.FRand() < OpenChance);
		}
	}

	for (ABHFlickerLight* Light : FlickerLights)
	{
		if (Light && Light->GetCircuitId() > 0)
		{
			const float OutageChance = ChosenModifier == EBHRoundModifier::LightsOut ? 0.52f : 0.18f;
			Light->SetPowered(Stream.FRand() >= OutageChance);
		}
	}

	LastDirectorScareTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -999.0f;
	LastMonsterChargeTime = LastDirectorScareTime - 999.0f;
	LastColdCallTime = LastDirectorScareTime - 999.0f;
	LastPresenceWhisperTime = LastDirectorScareTime - 999.0f;
	LastPresenceSpikeTime = LastDirectorScareTime - 999.0f;
	LastTeacherCounterScareTime = LastDirectorScareTime - 999.0f;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, ChosenModifier);
		BHGS->SetPresenceState(12.0f, TEXT("The building is listening."), 0);
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
	}
	if (bRevisionMode)
	{
		ResetRevisionStats();
		UpdateRevisionSummary(TEXT("Physics Classroom started: solve each zone, correct mistakes, and keep every student above threshold."));
		UpdateDirectorGameState(GetRevisionObjectiveText());
	}
	else
	{
		UpdateDirectorGameState(FString::Printf(TEXT("Repair %d breakers, answer %d class questions, then finish each task before the Teacher finds you. Modifier: %s."), ActiveBreakerCount, ActiveSideObjectiveCount, *GetRoundModifierText(ChosenModifier)));
	}
}

void ABHGameMode::StartDirectorTimer()
{
	GetWorldTimerManager().ClearTimer(DirectorTimerHandle);
	GetWorldTimerManager().SetTimer(DirectorTimerHandle, this, &ABHGameMode::TickDirector, 7.0f, true, 5.0f);
}

void ABHGameMode::TickDirector()
{
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	UpdatePresenceDirector();

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bPanicRound = BHGS->RoundModifier == EBHRoundModifier::PanicSurge;
	const float RevisionScareScale = bRevisionMode ? (RevisionScareIntensity <= 0 ? 0.0f : (0.45f + 0.28f * RevisionScareIntensity)) : 1.0f;
	const float PresenceAlpha = FMath::Clamp(BHGS->PresenceLevel / 100.0f, 0.0f, 1.0f);
	const float TimePressureAlpha = BHGS->RemainingTime > 0 && HuntSeconds > 0
		? 1.0f - FMath::Clamp(static_cast<float>(BHGS->RemainingTime) / static_cast<float>(HuntSeconds), 0.0f, 1.0f)
		: 0.0f;
	const float ObjectiveTotal = FMath::Max(1.0f, static_cast<float>(BHGS->BreakersRequired + BHGS->SideObjectivesRequired));
	const float ObjectiveAlpha = FMath::Clamp(static_cast<float>(BHGS->BreakersCompleted + BHGS->SideObjectivesCompleted) / ObjectiveTotal, 0.0f, 1.0f);
	const float DirectorPressure = FMath::Clamp(PresenceAlpha * 0.62f + TimePressureAlpha * 0.24f + ObjectiveAlpha * 0.28f, 0.0f, 1.0f);
	const float ScareCooldown = (bPartyPace || bPanicRound ? 11.0f : 24.0f) / FMath::Max(0.65f, RevisionScareScale);
	const float ScareChance = (bPartyPace || bPanicRound ? 0.58f : 0.24f) * FMath::Lerp(0.52f, 1.24f, DirectorPressure) * RevisionScareScale;
	const bool bScareWindow = Now - LastPresenceSpikeTime >= 3.5f || DirectorPressure >= 0.72f;
	if (bScareWindow && Now - LastDirectorScareTime >= ScareCooldown && FMath::FRand() < ScareChance)
	{
		TriggerScareEvent();
	}

	const bool bColdCallWindow = DirectorPressure >= 0.38f || ObjectiveAlpha >= 0.34f || TimePressureAlpha >= 0.52f;
	const float ColdCallCooldown = (bPartyPace || bPanicRound ? 16.0f : 30.0f) / FMath::Max(0.75f, RevisionScareScale);
	const float ColdCallChance = bColdCallWindow
		? ((bPartyPace || bPanicRound ? 0.18f : 0.055f) + PresenceAlpha * 0.24f + ObjectiveAlpha * 0.10f) * RevisionScareScale
		: 0.0f;
	if (Now - LastColdCallTime >= ColdCallCooldown && FMath::FRand() < ColdCallChance)
	{
		TriggerColdCallEvent();
	}
}

void ABHGameMode::UpdatePresenceDirector()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || !GetWorld() || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	int32 SurvivorCount = 0;
	int32 HiddenCount = 0;
	int32 HunterCount = 0;
	float BestThreatAlpha = 0.0f;
	ABHCharacter* BestTarget = nullptr;
	float BestTargetDistance = TNumericLimits<float>::Max();
	bool bBestTargetSeen = false;

	TArray<ABHCharacter*> Hunters;
	TArray<ABHCharacter*> Survivors;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Character = *It;
		const ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Character || !BHPS)
		{
			continue;
		}

		if (BHPS->IsAliveHunter())
		{
			Hunters.Add(Character);
			++HunterCount;
		}
		else if (BHPS->IsAliveSurvivor())
		{
			Survivors.Add(Character);
			++SurvivorCount;
			if (Character->IsHiddenInLocker())
			{
				++HiddenCount;
			}
		}
	}

	for (ABHCharacter* Survivor : Survivors)
	{
		if (!Survivor)
		{
			continue;
		}

		float NearestHunterDistance = TNumericLimits<float>::Max();
		bool bHunterCanSee = false;
		for (ABHCharacter* Hunter : Hunters)
		{
			if (!Hunter)
			{
				continue;
			}

			const float Distance = FVector::Dist2D(Hunter->GetActorLocation(), Survivor->GetActorLocation());
			if (Distance < NearestHunterDistance)
			{
				NearestHunterDistance = Distance;
				bHunterCanSee = Hunter->GetController() ? Hunter->GetController()->LineOfSightTo(Survivor) : false;
			}
		}

		const float DistanceAlpha = NearestHunterDistance < TNumericLimits<float>::Max()
			? 1.0f - FMath::Clamp(NearestHunterDistance / 4200.0f, 0.0f, 1.0f)
			: 0.0f;
		const float HiddenBonus = Survivor->IsHiddenInLocker() ? 0.16f : 0.0f;
		const float SeenBonus = bHunterCanSee && !Survivor->IsHiddenInLocker() ? 0.25f : 0.0f;
		const float PanicBonus = FMath::Clamp(FMath::Max(Survivor->GetFear(), Survivor->GetDread()) / 100.0f, 0.0f, 1.0f) * 0.24f;
		const float ThreatAlpha = FMath::Clamp(DistanceAlpha + HiddenBonus + SeenBonus + PanicBonus, 0.0f, 1.0f);
		if (ThreatAlpha > BestThreatAlpha)
		{
			BestThreatAlpha = ThreatAlpha;
			BestTarget = Survivor;
			BestTargetDistance = NearestHunterDistance;
			bBestTargetSeen = bHunterCanSee;
		}
	}

	const float ObjectiveTotal = FMath::Max(1.0f, static_cast<float>(BHGS->BreakersRequired + BHGS->SideObjectivesRequired));
	const float ObjectiveDone = static_cast<float>(BHGS->BreakersCompleted + BHGS->SideObjectivesCompleted);
	const float ObjectiveAlpha = FMath::Clamp(ObjectiveDone / ObjectiveTotal, 0.0f, 1.0f);
	const float HiddenAlpha = SurvivorCount > 0 ? static_cast<float>(HiddenCount) / static_cast<float>(SurvivorCount) : 0.0f;
	const float TimeAlpha = BHGS->RemainingTime > 0 ? 1.0f - FMath::Clamp(static_cast<float>(BHGS->RemainingTime) / static_cast<float>(HuntSeconds), 0.0f, 1.0f) : 0.0f;
	float DesiredPresence = FMath::Max(BestThreatAlpha * 86.0f, ObjectiveAlpha * 38.0f + HiddenAlpha * 18.0f + TimeAlpha * 28.0f);

	if (BHGS->RoundModifier == EBHRoundModifier::PanicSurge)
	{
		DesiredPresence += 12.0f;
	}
	else if (BHGS->RoundModifier == EBHRoundModifier::LightsOut)
	{
		DesiredPresence += 8.0f;
	}
	if (HunterCount == 0)
	{
		DesiredPresence *= 0.45f;
	}

	const float CurrentPresence = BHGS->PresenceLevel;
	const float DecayedPresence = FMath::Max(0.0f, CurrentPresence - 10.0f);
	const float NewPresence = FMath::Clamp(FMath::Max(DesiredPresence, DecayedPresence), 0.0f, 100.0f);

	FString PresenceText = TEXT("The building is listening.");
	const int32 PresenceTextSalt = RoundSeed + BHGS->PresencePulse * 11 + FMath::FloorToInt(NewPresence * 3.0f) + BHGS->RemainingTime;
	if (NewPresence >= 82.0f && BestTarget && BestTarget->IsHiddenInLocker())
	{
		static const TCHAR* HiddenCriticalLines[] = {
			TEXT("It is waiting outside a hiding place."),
			TEXT("The locker trace is warmer than it should be."),
			TEXT("The hiding place is no longer quiet."),
			TEXT("A handle moved on the heat trace.")
		};
		PresenceText = SelectPromptLine(HiddenCriticalLines, UE_ARRAY_COUNT(HiddenCriticalLines), PresenceTextSalt);
	}
	else if (NewPresence >= 78.0f && (bBestTargetSeen || BestTargetDistance <= 2600.0f))
	{
		static const TCHAR* CloseThreatLines[] = {
			TEXT("The Teacher is close enough to hear breathing."),
			TEXT("A warm path is closing behind you."),
			TEXT("The route has a second pulse on it."),
			TEXT("The corridor trace bends toward you.")
		};
		PresenceText = SelectPromptLine(CloseThreatLines, UE_ARRAY_COUNT(CloseThreatLines), PresenceTextSalt);
	}
	else if (NewPresence >= 62.0f && ObjectiveAlpha > 0.35f)
	{
		static const TCHAR* ObjectivePressureLines[] = {
			TEXT("Every completed task makes the route louder."),
			TEXT("The objective trail is glowing too clearly."),
			TEXT("The heat trace remembers each repaired station."),
			TEXT("The route is compromised.")
		};
		PresenceText = SelectPromptLine(ObjectivePressureLines, UE_ARRAY_COUNT(ObjectivePressureLines), PresenceTextSalt);
	}
	else if (NewPresence >= 48.0f && HiddenCount > 0)
	{
		static const TCHAR* HiddenPressureLines[] = {
			TEXT("The lockers do not feel empty."),
			TEXT("The map shows warmth where nobody is moving."),
			TEXT("A hiding place keeps returning a signal."),
			TEXT("The quiet spots are being counted.")
		};
		PresenceText = SelectPromptLine(HiddenPressureLines, UE_ARRAY_COUNT(HiddenPressureLines), PresenceTextSalt);
	}
	else if (NewPresence >= 30.0f)
	{
		static const TCHAR* SuspicionLines[] = {
			TEXT("Something is mapping your route."),
			TEXT("The heat trace corrects itself after you move."),
			TEXT("A path appears before anyone walks it."),
			TEXT("The sensor sees a line through the dark.")
		};
		PresenceText = SelectPromptLine(SuspicionLines, UE_ARRAY_COUNT(SuspicionLines), PresenceTextSalt);
	}

	const bool bPulse = NewPresence >= 72.0f || FMath::Abs(NewPresence - CurrentPresence) >= 18.0f;
	BHGS->SetPresenceState(NewPresence, PresenceText, bPulse ? BHGS->PresencePulse + 1 : BHGS->PresencePulse);

	const float Now = GetWorld()->GetTimeSeconds();
	const float WhisperCooldown = bPartyPace || BHGS->RoundModifier == EBHRoundModifier::PanicSurge ? 8.5f : 14.0f;
	const bool bWhisperMoment = BestTarget
		&& (BestTargetDistance <= 4200.0f || BestTarget->IsHiddenInLocker() || BestTarget->GetDread() >= 52.0f || BestTarget->IsDetentionMarked());
	if (bWhisperMoment && NewPresence >= 54.0f && Now - LastPresenceWhisperTime >= WhisperCooldown)
	{
		LastPresenceWhisperTime = Now;
		const FVector BehindTarget = BestTarget->GetActorLocation() - BestTarget->GetActorForwardVector() * FMath::FRandRange(180.0f, 420.0f) + FVector(0.0f, 0.0f, 88.0f);
		SpawnAmbient(BehindTarget, FMath::FRandRange(190.0f, 340.0f), BestTarget->IsHiddenInLocker() ? 0.22f : 0.18f, 0.18f, 5.6f, 3.75f);
		BestTarget->AddFear(BestTarget->IsHiddenInLocker() ? 12.0f : 8.0f);
		BestTarget->AddDread(BestTarget->IsHiddenInLocker() ? 16.0f : 9.0f);

		if (ABHPlayerController* PC = Cast<ABHPlayerController>(BestTarget->GetController()))
		{
			const TCHAR* Message = nullptr;
			if (BestTarget->IsHiddenInLocker())
			{
				static const TCHAR* HiddenMessages[] = {
					TEXT("The handle moves once, then stops."),
					TEXT("Something listens inches from your face."),
					TEXT("The air inside the locker goes cold."),
					TEXT("A fingertip taps the other side."),
					TEXT("The locker breathes after you do."),
					TEXT("Something writes on the outside panel."),
					TEXT("A shoe stops directly outside."),
					TEXT("The silence leans against the door.")
				};
				Message = SelectPromptLine(HiddenMessages, UE_ARRAY_COUNT(HiddenMessages), FMath::Rand());
			}
			else if (bBestTargetSeen || BestTargetDistance <= 2200.0f)
			{
				static const TCHAR* CloseMessages[] = {
					TEXT("You hear a second set of footsteps match yours."),
					TEXT("A breath cuts off when you turn."),
					TEXT("Something scraped the wall beside you."),
					TEXT("Footsteps stop exactly when you stop."),
					TEXT("The wall next to you clicks twice."),
					TEXT("Your next turn is already occupied."),
					TEXT("A shoulder brushes past where no one is standing."),
					TEXT("The dark beside you inhales.")
				};
				Message = SelectPromptLine(CloseMessages, UE_ARRAY_COUNT(CloseMessages), FMath::Rand());
			}
			else
			{
				static const TCHAR* OpenMessages[] = {
					TEXT("You hear a second set of footsteps match yours."),
					TEXT("The dark ahead feels occupied."),
					TEXT("A breath cuts off when you turn."),
					TEXT("Your shadow moves half a second late."),
					TEXT("A route draws itself through a room you have not entered."),
					TEXT("The floor settles under an extra step."),
					TEXT("A light blinks in the shape of your path."),
					TEXT("Something waits at the end of the heat trace.")
				};
				Message = SelectPromptLine(OpenMessages, UE_ARRAY_COUNT(OpenMessages), FMath::Rand());
			}
			PC->ClientShowStatusMessage(Message, bBestTargetSeen || BestTargetDistance <= 2200.0f ? 2.85f : 3.0f);
		}

		if (!FlickerLights.IsEmpty() && FMath::FRand() < 0.48f)
		{
			ABHFlickerLight* NearestLight = nullptr;
			float NearestLightDistSq = TNumericLimits<float>::Max();
			for (ABHFlickerLight* Light : FlickerLights)
			{
				if (!Light || Light->GetCircuitId() <= 0 || !Light->IsPowered())
				{
					continue;
				}

				const float DistSq = FVector::DistSquared2D(Light->GetActorLocation(), BestTarget->GetActorLocation());
				if (DistSq < NearestLightDistSq)
				{
					NearestLightDistSq = DistSq;
					NearestLight = Light;
				}
			}

			if (NearestLight && NearestLightDistSq <= FMath::Square(2600.0f))
			{
				NearestLight->SetPowered(false);
			}
		}
	}
}

void ABHGameMode::ApplyPresenceSpike(const FVector& SourceLocation, float SpikeLevel, const FString& PresenceText)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || !GetWorld() || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float NewPresence = FMath::Clamp(FMath::Max(BHGS->PresenceLevel, SpikeLevel), 0.0f, 100.0f);
	BHGS->SetPresenceState(NewPresence, PresenceText, BHGS->PresencePulse + 1);
	LastPresenceSpikeTime = Now;

	if (Now - LastPresenceWhisperTime > 5.0f)
	{
		SpawnAmbient(SourceLocation + FVector(0.0f, 0.0f, 86.0f), FMath::FRandRange(125.0f, 260.0f), 0.20f, 0.16f, 4.7f, 3.25f);
	}
}

void ABHGameMode::TriggerScareEvent()
{
	TArray<ABHCharacter*> Candidates;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ABHCharacter* Character = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
		const ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (Character && BHPS && BHPS->IsAliveSurvivor() && !Character->IsHiddenInLocker())
		{
			Candidates.Add(Character);
		}
	}

	if (Candidates.IsEmpty())
	{
		return;
	}

	ABHCharacter* Target = Candidates[0];
	float BestCandidateScore = -1.0f;
	for (ABHCharacter* Candidate : Candidates)
	{
		if (!Candidate)
		{
			continue;
		}

		float NearestHunterDistance = TNumericLimits<float>::Max();
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			const ABHCharacter* Hunter = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
			const ABHPlayerState* HunterPS = Hunter ? Hunter->GetPlayerState<ABHPlayerState>() : nullptr;
			if (Hunter && HunterPS && HunterPS->IsAliveHunter())
			{
				NearestHunterDistance = FMath::Min(NearestHunterDistance, FVector::Dist2D(Hunter->GetActorLocation(), Candidate->GetActorLocation()));
			}
		}

		const float DreadAlpha = FMath::Clamp(Candidate->GetDread() / 100.0f, 0.0f, 1.0f);
		const float FearAlpha = FMath::Clamp(Candidate->GetFear() / 100.0f, 0.0f, 1.0f);
		const float NearMissAlpha = NearestHunterDistance < TNumericLimits<float>::Max()
			? FMath::Clamp((NearestHunterDistance - 1200.0f) / 3600.0f, 0.0f, 1.0f)
			: 0.25f;
		const float CandidateScore = FMath::FRandRange(0.0f, 0.32f)
			+ DreadAlpha * 0.34f
			+ FearAlpha * 0.20f
			+ NearMissAlpha * 0.18f
			+ (Candidate->IsDetentionMarked() ? 0.22f : 0.0f);
		if (CandidateScore > BestCandidateScore)
		{
			BestCandidateScore = CandidateScore;
			Target = Candidate;
		}
	}

	FVector ScareLocation = Target->GetActorLocation() - Target->GetActorForwardVector() * 320.0f + FVector(0.0f, 0.0f, 80.0f);
	if (!ScarePoints.IsEmpty())
	{
		TArray<FVector> NearbyScares;
		for (const FVector& Point : ScarePoints)
		{
			if (FVector::DistSquared2D(Point, Target->GetActorLocation()) <= FMath::Square(1900.0f))
			{
				NearbyScares.Add(Point);
			}
		}
		if (!NearbyScares.IsEmpty())
		{
			ScareLocation = NearbyScares[FMath::RandRange(0, NearbyScares.Num() - 1)];
		}
	}

	const float MonsterCooldown = bPartyPace ? 24.0f : 38.0f;
	if (GetWorld() && GetWorld()->GetTimeSeconds() - LastMonsterChargeTime >= MonsterCooldown && FMath::FRand() < (bPartyPace ? 0.45f : 0.30f))
	{
		TriggerMonsterChargeJumpscare(Target);
		LastDirectorScareTime = GetWorld()->GetTimeSeconds();
		return;
	}

	if (bRevisionMode)
	{
		TriggerRevisionThemedAmbientScare(Target);
		LastDirectorScareTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastDirectorScareTime;
		return;
	}

	SpawnAmbient(ScareLocation, FMath::FRandRange(180.0f, 340.0f), 0.30f, 0.18f, 6.0f, 4.5f);
	Target->AddFear(22.0f);
	Target->AddDread(8.0f);
	LastDirectorScareTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastDirectorScareTime;

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController()))
	{
		static const TCHAR* Messages[] = {
			TEXT("Something scraped the wall beside you."),
			TEXT("A light snaps behind you."),
			TEXT("You hear a breath where nobody should be."),
			TEXT("Metal shifts in the dark."),
			TEXT("A door settles shut in a room you never opened."),
			TEXT("The floor clicks once under an extra footstep."),
			TEXT("Your flashlight catches movement, then forgets it."),
			TEXT("Something drags along the wall, keeping pace."),
			TEXT("The ceiling tiles creak one by one above you."),
			TEXT("A shape crosses the heat trace and disappears."),
			TEXT("The air behind your neck goes warm."),
			TEXT("A locker knocks from the inside as you pass.")
		};
		PC->ClientShowStatusMessage(Messages[FMath::RandRange(0, UE_ARRAY_COUNT(Messages) - 1)], 2.75f);
	}

	if (!FlickerLights.IsEmpty() && FMath::FRand() < 0.45f)
	{
		ABHFlickerLight* Light = FlickerLights[FMath::RandRange(0, FlickerLights.Num() - 1)];
		if (Light && Light->GetCircuitId() > 0)
		{
			Light->SetPowered(false);
		}
	}
}

void ABHGameMode::TriggerRevisionThemedAmbientScare(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const FVector BehindTarget = TargetLocation - Target->GetActorForwardVector() * 260.0f + FVector(0.0f, 0.0f, 92.0f);
	const int32 Variant = FMath::RandRange(0, 11);
	FString Message;
	float Frequency = 260.0f;
	float Pulse = 5.6f;
	switch (Variant)
	{
	case 0:
		Message = TEXT("Blackboard slam: the correct formula appears too close.");
		Frequency = 185.0f;
		Pulse = 6.4f;
		break;
	case 1:
		Message = TEXT("Exam bell spike: TIME flashes red in the corner of your eye.");
		Frequency = 520.0f;
		Pulse = 7.0f;
		break;
	case 2:
		Message = TEXT("Circuit breaker pop: every light nearby clicks once.");
		Frequency = 360.0f;
		Pulse = 5.8f;
		break;
	case 3:
		Message = TEXT("Graph line lunge: a plotted line races across your vision.");
		Frequency = 310.0f;
		Pulse = 6.1f;
		break;
	case 4:
		Message = TEXT("Roll call: the register whispers your name.");
		Frequency = 140.0f;
		Pulse = 4.9f;
		break;
	case 5:
		Message = TEXT("Formula whisper: rearrange it before it screams.");
		Frequency = 230.0f;
		Pulse = 5.2f;
		break;
	case 6:
		Message = TEXT("Chalk dust falls upward and spells your last mistake.");
		Frequency = 205.0f;
		Pulse = 5.9f;
		break;
	case 7:
		Message = TEXT("A desk leg drags beside the wall, keeping pace.");
		Frequency = 155.0f;
		Pulse = 6.0f;
		break;
	case 8:
		Message = TEXT("Calculator keys click your heartbeat back at you.");
		Frequency = 490.0f;
		Pulse = 6.8f;
		break;
	case 9:
		Message = TEXT("A ruler snaps in the dark and points at your feet.");
		Frequency = 575.0f;
		Pulse = 7.1f;
		break;
	case 10:
		Message = TEXT("A chair turns toward you without touching the floor.");
		Frequency = 245.0f;
		Pulse = 5.5f;
		break;
	default:
		Message = TEXT("Reflection scare: something appears at the equal angle.");
		Frequency = 430.0f;
		Pulse = 6.7f;
		break;
	}

	SpawnAmbient(BehindTarget, Frequency, 0.28f, 0.20f, Pulse, 4.4f);
	Target->AddFear(20.0f);
	Target->AddDread(10.0f);
	if (Variant == 1 || Variant == 2 || Variant == 8 || Variant == 9)
	{
		CutLightsForJumpscare(TargetLocation, BehindTarget, 1700.0f, 4.8f);
	}
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController()))
	{
		PC->ClientShowStatusMessage(Message, 2.9f);
	}
}

void ABHGameMode::TriggerTeacherFlatScare(ABHCharacter* Target, const FVector& FocusLocation, const FString& Message, float LockSeconds)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	SpawnAmbient(FocusLocation + FVector(0.0f, 0.0f, 85.0f), 620.0f, 0.32f, 0.24f, 7.4f, 4.2f);
	CutLightsForJumpscare(Target->GetActorLocation(), FocusLocation, 2200.0f, 5.5f);
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController()))
	{
		PC->ClientSnapViewToFlatFocus(FocusLocation + FVector(0.0f, 0.0f, 120.0f));
		PC->ClientShowStatusMessage(Message, 3.2f);
	}
	FreezeTargetForJumpscare(Target, LockSeconds);
	Target->AddFear(18.0f);
	Target->AddDread(24.0f);
}

void ABHGameMode::TriggerTeacherCounterJumpscare(ABHObjectiveStation* Station, EBHRevisionCounterNodeType CounterType)
{
	if (!bRevisionMode || !Station || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastTeacherCounterScareTime < 2.0f)
	{
		return;
	}
	LastTeacherCounterScareTime = Now;

	FString Message = CounterType == EBHRevisionCounterNodeType::PeerReview
		? TEXT("Peer review counter: the class corrected your marking.")
		: TEXT("Demonstration trap: the practical fired back.");
	if (CounterType == EBHRevisionCounterNodeType::DemonstrationTrap)
	{
		switch (Station->GetStationType())
		{
		case EBHObjectiveStationType::Terminal:
			Message = TEXT("Circuit breaker pop: the students overloaded your scare.");
			break;
		case EBHObjectiveStationType::Valve:
			Message = TEXT("Force demo: a desk slams across your path.");
			break;
		case EBHObjectiveStationType::Antenna:
			Message = TEXT("Wave demo: the Doppler scream comes back at you.");
			break;
		case EBHObjectiveStationType::Evidence:
			Message = TEXT("Energy demo: the Sankey arrow snaps shut.");
			break;
		}
	}

	int32 HitTeachers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHCharacter* Teacher = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
		const ABHPlayerState* BHPS = Teacher ? Teacher->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Teacher || !BHPS || !BHPS->IsAliveHunter())
		{
			continue;
		}

		++HitTeachers;
		if (CounterType == EBHRevisionCounterNodeType::DemonstrationTrap && FMath::FRand() < 0.35f)
		{
			TriggerMonsterChargeJumpscare(Teacher);
			PC->ClientShowStatusMessage(Message, 3.2f);
		}
		else
		{
			TriggerTeacherFlatScare(Teacher, Station->GetActorLocation(), Message, CounterType == EBHRevisionCounterNodeType::PeerReview ? 1.65f : 2.0f);
		}
	}

	if (HitTeachers > 0)
	{
		BroadcastStatus(CounterType == EBHRevisionCounterNodeType::PeerReview
			? TEXT("Peer Review complete: the Teacher was stunned by a correction chain.")
			: TEXT("Demonstration Trap complete: the practical fired back at the Teacher."), 4.0f);
		ApplyPresenceSpike(Station->GetActorLocation(), 72.0f, TEXT("The class fought back with physics."));
	}
}

void ABHGameMode::ActivateStudentScareRelay(ABHCharacter* Activator, ABHObjectiveStation* SourceNode)
{
	if (!Activator || !GetWorld())
	{
		return;
	}

	int32 ScaredTeachers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHCharacter* TeacherTarget = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
		const ABHPlayerState* BHPS = TeacherTarget ? TeacherTarget->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!TeacherTarget || TeacherTarget == Activator || !BHPS || !BHPS->IsAliveHunter())
		{
			continue;
		}

		++ScaredTeachers;
		TriggerMonsterChargeJumpscare(TeacherTarget);
		PC->ClientShowStatusMessage(TEXT("Something sprints toward you."), 2.75f);
	}

	if (ABHPlayerController* ActivatorPC = Cast<ABHPlayerController>(Activator->GetController()))
	{
		ActivatorPC->ClientShowStatusMessage(ScaredTeachers > 0 ? TEXT("Scare relay armed.") : TEXT("Scare relay armed, but no Teacher signal was found."), 2.75f);
	}
}

void ABHGameMode::TriggerMonsterChargeJumpscare(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const float TargetFloorZ = TargetLocation.Z - Target->GetSimpleCollisionHalfHeight();
	const FVector EyeTarget = TargetLocation + FVector(0.0f, 0.0f, 95.0f);
	FVector SpawnLocation = FVector::ZeroVector;
	float BestScore = -1.0f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHMonsterChargeLine), false, Target);
	for (const FVector& Point : ScarePoints)
	{
		const float DistSq = FVector::DistSquared2D(Point, TargetLocation);
		if (DistSq < FMath::Square(3000.0f) || DistSq > FMath::Square(6400.0f))
		{
			continue;
		}

		FHitResult Hit;
		const FVector CandidateEye = Point + FVector(0.0f, 0.0f, 125.0f);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, EyeTarget, CandidateEye, ECC_Visibility, Params);
		if (bBlocked)
		{
			continue;
		}

		const float Score = FMath::Sqrt(DistSq);
		if (Score > BestScore)
		{
			BestScore = Score;
			SpawnLocation = Point;
		}
	}

	if (BestScore < 0.0f)
	{
		const float MaxX = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? 5850.0f : 5350.0f;
		const float MaxY = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? 4250.0f : 4250.0f;
		FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			Forward = FVector::ForwardVector;
		}
		const FVector Right = Target->GetActorRightVector().GetSafeNormal2D();
		const FVector Directions[] = {Forward, Right, -Right, -Forward};
		const float Distances[] = {4600.0f, 3800.0f, 3000.0f, 2200.0f, 1500.0f};
		for (const FVector& DirectionCandidate : Directions)
		{
			if (DirectionCandidate.IsNearlyZero())
			{
				continue;
			}

			for (float Distance : Distances)
			{
				FVector Candidate = TargetLocation + DirectionCandidate * Distance;
				Candidate.X = FMath::Clamp(Candidate.X, -MaxX, MaxX);
				Candidate.Y = FMath::Clamp(Candidate.Y, -MaxY, MaxY);
				Candidate.Z = TargetFloorZ + 4.0f;
				const FVector CandidateEye = Candidate + FVector(0.0f, 0.0f, 125.0f);

				FHitResult Hit;
				const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, EyeTarget, CandidateEye, ECC_Visibility, Params);
				if (!bBlocked)
				{
					SpawnLocation = Candidate;
					BestScore = Distance;
					break;
				}

				const float VisibleDistance = FMath::Sqrt(FVector::DistSquared2D(TargetLocation, Hit.Location)) - 320.0f;
				if (VisibleDistance >= 900.0f)
				{
					SpawnLocation = TargetLocation + DirectionCandidate * VisibleDistance;
					SpawnLocation.X = FMath::Clamp(SpawnLocation.X, -MaxX, MaxX);
					SpawnLocation.Y = FMath::Clamp(SpawnLocation.Y, -MaxY, MaxY);
					SpawnLocation.Z = TargetFloorZ + 4.0f;
					BestScore = VisibleDistance;
					break;
				}
			}

			if (BestScore >= 0.0f)
			{
				break;
			}
		}

		if (BestScore < 0.0f)
		{
			LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
			SpawnAmbient(TargetLocation + Target->GetActorForwardVector() * 220.0f + FVector(0.0f, 0.0f, 105.0f), 260.0f, 0.26f, 0.16f, 5.4f, 3.4f);
			Target->AddFear(16.0f);
			Target->AddDread(18.0f);
			return;
		}
	}
	SpawnLocation.Z = TargetFloorZ + 4.0f;

	const FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal2D();
	const FRotator SpawnRotation = Direction.IsNearlyZero() ? Target->GetActorRotation() : Direction.Rotation();
	if (ABHJumpscareMonster* Monster = GetWorld()->SpawnActor<ABHJumpscareMonster>(SpawnLocation, SpawnRotation))
	{
		const float Speed = bPartyPace ? 9800.0f : 8800.0f;
		const float HoldDuration = bPartyPace ? 2.25f : 2.95f;
		Monster->Configure(Target, Speed, bPartyPace ? 7.4f : 8.6f, HoldDuration);
		if (ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController()))
		{
			TargetPC->ClientSnapViewToFlatFocus(SpawnLocation + FVector(0.0f, 0.0f, 145.0f));
		}
		FreezeTargetForJumpscare(Target, HoldDuration);
		CutLightsForJumpscare(TargetLocation, SpawnLocation, 0.0f, bPartyPace ? 9.0f : 10.25f);
	}

	LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
	Target->AddFear(38.0f);
	Target->AddDread(42.0f);

	for (ABHFlickerLight* Light : FlickerLights)
	{
		if (Light && Light->GetCircuitId() > 0 && FVector::DistSquared2D(Light->GetActorLocation(), TargetLocation) <= FMath::Square(2400.0f) && FMath::FRand() < 0.34f)
		{
			Light->SetPowered(false);
		}
	}
}

void ABHGameMode::FreezeTargetForJumpscare(ABHCharacter* Target, float DurationSeconds)
{
	if (!Target || !GetWorld() || DurationSeconds <= 0.0f)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController());
	if (PC)
	{
		PC->ClientSetJumpscareInputLocked(true);
	}

	TWeakObjectPtr<ABHCharacter> WeakTarget = Target;
	TWeakObjectPtr<ABHPlayerController> WeakPC = PC;
	FTimerDelegate RestoreDelegate;
	RestoreDelegate.BindLambda([WeakTarget, WeakPC]()
	{
		if (WeakPC.IsValid())
		{
			WeakPC->ClientSetJumpscareInputLocked(false);
		}

		if (!WeakTarget.IsValid())
		{
			return;
		}

		ABHCharacter* FrozenTarget = WeakTarget.Get();
		const ABHPlayerState* BHPS = FrozenTarget->GetPlayerState<ABHPlayerState>();
		const bool bAlive = !BHPS || BHPS->LifeState == EBHPlayerLifeState::Alive;
		if (!FrozenTarget->IsHiddenInLocker() && bAlive)
		{
			if (UCharacterMovementComponent* Movement = FrozenTarget->GetCharacterMovement())
			{
				Movement->SetMovementMode(MOVE_Walking);
			}
		}
	});

	FTimerHandle RestoreHandle;
	GetWorldTimerManager().SetTimer(RestoreHandle, RestoreDelegate, DurationSeconds, false);
}

void ABHGameMode::CutLightsForJumpscare(const FVector& TargetLocation, const FVector& MonsterLocation, float Radius, float RestoreDelaySeconds)
{
	TArray<TWeakObjectPtr<ABHFlickerLight>> AffectedLights;
	TArray<bool> OriginalPoweredStates;

	if (Radius <= 0.0f)
	{
		for (ABHFlickerLight* Light : FlickerLights)
		{
			if (Light)
			{
				AffectedLights.Add(Light);
				OriginalPoweredStates.Add(Light->IsPowered());
				Light->SetPowered(false);
			}
		}
	}
	else
	{
		const float RadiusSq = FMath::Square(Radius);
		for (ABHFlickerLight* Light : FlickerLights)
		{
			if (!Light)
			{
				continue;
			}

			const FVector LightLocation = Light->GetActorLocation();
			const bool bNearTarget = FVector::DistSquared2D(LightLocation, TargetLocation) <= RadiusSq;
			const bool bNearMonster = FVector::DistSquared2D(LightLocation, MonsterLocation) <= RadiusSq;
			if (bNearTarget || bNearMonster)
			{
				AffectedLights.Add(Light);
				OriginalPoweredStates.Add(Light->IsPowered());
				Light->SetPowered(false);
			}
		}
	}

	if (AffectedLights.IsEmpty() || RestoreDelaySeconds <= 0.0f || !GetWorld())
	{
		return;
	}

	FTimerDelegate RestoreDelegate;
	RestoreDelegate.BindLambda([AffectedLights = MoveTemp(AffectedLights), OriginalPoweredStates = MoveTemp(OriginalPoweredStates)]() mutable
	{
		const int32 RestoreCount = FMath::Min(AffectedLights.Num(), OriginalPoweredStates.Num());
		for (int32 Index = 0; Index < RestoreCount; ++Index)
		{
			if (AffectedLights[Index].IsValid())
			{
				AffectedLights[Index]->SetPowered(OriginalPoweredStates[Index]);
			}
		}
	});

	FTimerHandle RestoreHandle;
	GetWorldTimerManager().SetTimer(RestoreHandle, RestoreDelegate, RestoreDelaySeconds, false);
}

void ABHGameMode::TriggerColdCallEvent()
{
	TArray<ABHCharacter*> Candidates;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Character = *It;
		const ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (Character && BHPS && BHPS->IsAliveSurvivor())
		{
			Candidates.Add(Character);
		}
	}

	if (Candidates.IsEmpty())
	{
		return;
	}

	ABHCharacter* Target = Candidates[0];
	float BestCandidateScore = -1.0f;
	for (ABHCharacter* Candidate : Candidates)
	{
		if (!Candidate)
		{
			continue;
		}

		const float DreadAlpha = FMath::Clamp(Candidate->GetDread() / 100.0f, 0.0f, 1.0f);
		const float FearAlpha = FMath::Clamp(Candidate->GetFear() / 100.0f, 0.0f, 1.0f);
		const float CandidateScore = FMath::FRandRange(0.0f, 0.34f)
			+ DreadAlpha * 0.36f
			+ FearAlpha * 0.22f
			+ (Candidate->IsHiddenInLocker() ? 0.24f : 0.0f)
			+ (Candidate->IsDetentionMarked() ? 0.18f : 0.0f);
		if (CandidateScore > BestCandidateScore)
		{
			BestCandidateScore = CandidateScore;
			Target = Candidate;
		}
	}

	const bool bTargetHidden = Target->IsHiddenInLocker();
	const FVector SourceLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 84.0f);
	SpawnAmbient(SourceLocation - Target->GetActorForwardVector() * 180.0f, FMath::FRandRange(210.0f, 380.0f), bTargetHidden ? 0.24f : 0.18f, 0.18f, 6.2f, 4.0f);
	Target->AddFear(bTargetHidden ? 20.0f : 12.0f);
	Target->AddDread(bTargetHidden ? 26.0f : 16.0f);
	ApplyPresenceSpike(SourceLocation, bTargetHidden ? 78.0f : 66.0f, TEXT("The Teacher called on someone."));
	LastColdCallTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastColdCallTime;

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController()))
	{
		static const TCHAR* OpenMessages[] = {
			TEXT("The intercom says your name."),
			TEXT("A desk scrapes behind you, but there is no classroom."),
			TEXT("You hear chalk write the answer you got wrong."),
			TEXT("The Teacher asks you to explain your working."),
			TEXT("A chair is pulled out for you in the dark."),
			TEXT("The register marks you present twice."),
			TEXT("Someone turns a page directly behind your head."),
			TEXT("The whiteboard squeals your route number."),
			TEXT("A detention slip slides under a door ahead."),
			TEXT("The bell rings once, but only in your corridor."),
			TEXT("A desk lid opens and shuts in time with your steps."),
			TEXT("Your name is whispered from the wrong side of the wall.")
		};
		static const TCHAR* HiddenMessages[] = {
			TEXT("The Teacher calls your name from outside the locker."),
			TEXT("A register is taken inches from your face."),
			TEXT("You hear your chair scrape across the floor."),
			TEXT("The locker walls tighten like a detention desk."),
			TEXT("A clipboard taps against the locker door."),
			TEXT("The roll call stops one name before yours."),
			TEXT("Someone writes detention on the other side."),
			TEXT("A chair leg scrapes under your feet."),
			TEXT("The locker vents breathe chalk dust."),
			TEXT("The Teacher waits for you to answer in a whisper."),
			TEXT("A desk is dragged into place outside the door."),
			TEXT("The register knows where you are hiding.")
		};
		const TCHAR** MessageSet = bTargetHidden ? HiddenMessages : OpenMessages;
		const int32 MessageCount = bTargetHidden ? UE_ARRAY_COUNT(HiddenMessages) : UE_ARRAY_COUNT(OpenMessages);
		PC->ClientShowStatusMessage(MessageSet[FMath::RandRange(0, MessageCount - 1)], 3.25f);
	}

	if (bTargetHidden && FMath::FRand() < 0.34f)
	{
		NotifyLoudNoise(Target->GetActorLocation(), TEXT("stifled answer"));
	}
}

void ABHGameMode::BroadcastStatus(const FString& Message, float DurationSeconds) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(Message, DurationSeconds);
		}
	}
}

ABHCharacter* ABHGameMode::FindCharacterForPlayerState(APlayerState* TargetPlayerState) const
{
	if (!TargetPlayerState || !GetWorld())
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->PlayerState == TargetPlayerState)
		{
			return Cast<ABHCharacter>(PC->GetPawn());
		}
	}

	return nullptr;
}

void ABHGameMode::UpdateDirectorGameState(const FString& ObjectiveText)
{
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetDirectorState(RoundSeed, ObjectiveText, NextRuntimeLevelName);
		BHGS->SetFogOptions(NextFogPreset, bFogPresetOverride);
		BHGS->SetActiveFogPreset(RuntimeFogPreset);
		BHGS->SetActiveLevelName(RuntimeLevelName);
	}
}

void ABHGameMode::UpdateExitUnlockState()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->bExitUnlocked)
	{
		return;
	}

	if (BHGS->BreakersCompleted >= BHGS->BreakersRequired && BHGS->SideObjectivesCompleted >= BHGS->SideObjectivesRequired)
	{
		if (bRevisionMode)
		{
			UpdateRevisionSummary();
			FString RevisionBlockReason;
			if (!CanUnlockRevisionExit(RevisionBlockReason))
			{
				BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 54.0f), TEXT("The Teacher is holding a correction conference."), BHGS->PresencePulse + 1);
				UpdateDirectorGameState(GetRevisionObjectiveText());
				BroadcastStatus(RevisionBlockReason, 5.0f);
				return;
			}
		}
		BHGS->SetExitUnlocked(true);
		BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 76.0f), bRevisionMode ? TEXT("The class passed. The Teacher is furious.") : TEXT("The exit woke up. So did everything else."), BHGS->PresencePulse + 1);
		BroadcastStatus(bRevisionMode ? TEXT("Physics mastery met. Reach the active exit gate.") : TEXT("Exit power restored. Reach the active exit gate."), 4.0f);
	}
}

void ABHGameMode::RefreshNextLevelFromVotes()
{
	if (!GameState)
	{
		return;
	}

	int32 FacilityVotes = 0;
	int32 SubstationVotes = 0;
	int32 FoggroundsVotes = 0;
	int32 LightFogVotes = 0;
	int32 HeavyFogVotes = 0;
	int32 ExtremeFogVotes = 0;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS)
		{
			continue;
		}

		if (!BHPS->MapVote.IsEmpty())
		{
			const FString Vote = NormalizeBHLevelName(BHPS->MapVote);
			if (Vote.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
			{
				++SubstationVotes;
			}
			else if (Vote.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
			{
				++FoggroundsVotes;
			}
			else
			{
				++FacilityVotes;
			}
		}

		if (BHPS->bHasFogPresetVote)
		{
			switch (BHPS->FogPresetVote)
			{
			case EBHFogPreset::Light:
				++LightFogVotes;
				break;
			case EBHFogPreset::Extreme:
				++ExtremeFogVotes;
				break;
			case EBHFogPreset::Heavy:
			default:
				++HeavyFogVotes;
				break;
			}
		}
	}

	if (FacilityVotes > SubstationVotes && FacilityVotes > FoggroundsVotes)
	{
		NextRuntimeLevelName = TEXT("Facility");
	}
	else if (SubstationVotes > FacilityVotes && SubstationVotes > FoggroundsVotes)
	{
		NextRuntimeLevelName = TEXT("Substation");
	}
	else if (FoggroundsVotes > FacilityVotes && FoggroundsVotes > SubstationVotes)
	{
		NextRuntimeLevelName = TEXT("Foggrounds");
	}

	if (!bFogPresetOverride)
	{
		if (LightFogVotes > HeavyFogVotes && LightFogVotes > ExtremeFogVotes)
		{
			NextFogPreset = EBHFogPreset::Light;
		}
		else if (ExtremeFogVotes > LightFogVotes && ExtremeFogVotes > HeavyFogVotes)
		{
			NextFogPreset = EBHFogPreset::Extreme;
		}
		else if (HeavyFogVotes > LightFogVotes && HeavyFogVotes > ExtremeFogVotes)
		{
			NextFogPreset = EBHFogPreset::Heavy;
		}
	}

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	UpdateDirectorGameState(BHGS ? BHGS->ObjectiveText : FString(TEXT("Map votes updated.")));
	const FString FogMode = bFogPresetOverride ? TEXT("host override") : TEXT("votes");
	BroadcastStatus(FString::Printf(TEXT("Map votes: Facility %d, Substation %d, Foggrounds %d. Next: %s. Fog: %s (%s)."),
		FacilityVotes,
		SubstationVotes,
		FoggroundsVotes,
		*NextRuntimeLevelName,
		*FogPresetToString(NextFogPreset),
		*FogMode), 3.5f);
}

void ABHGameMode::StartPracticeMode(ABHPlayerController* RequestingController)
{
	bPracticeMode = true;
	bTestMode = false;
	GetWorldTimerManager().ClearTimer(RoundTimerHandle);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!BHPS)
		{
			continue;
		}

		if (BHPS->PlayerRole == EBHPlayerRole::Unassigned || BHPS->PlayerRole == EBHPlayerRole::Spectator)
		{
			BHPS->SetRole(EBHPlayerRole::Survivor);
			BHPS->SetDesiredRole(EBHPlayerRole::Survivor);
		}
		BHPS->SetReady(true);
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
		BHPS->SetFakeHunterEligible(false);
	}

	RefreshPracticeDirector(TEXT("Practice Lab started. Use Escape to switch roles, modifiers, and objective pressure."));
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Practice Lab: no ready-up, no match timer, no forced round end."), 5.0f);
	}
}

void ABHGameMode::RefreshPracticeDirector(const FString& Reason)
{
	if (!bPracticeMode)
	{
		return;
	}

	PrepareRoundDirector();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetPracticeMode(true);
		BHGS->SetTestMode(false);
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(0);
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(false);
		BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 18.0f), TEXT("Practice Lab is holding the round open."), BHGS->PresencePulse + 1);
	}

	UpdateDirectorGameState(FString::Printf(TEXT("Practice Lab: no ready-up and no match timer. Repair %d breakers, answer %d questions, test roles and modifier: %s."),
		ActiveBreakerCount,
		ActiveSideObjectiveCount,
		*GetRoundModifierText(PracticeRoundModifier)));

	StartDirectorTimer();
	BroadcastStatus(Reason, 4.0f);
}

void ABHGameMode::StartTestMode(ABHPlayerController* RequestingController)
{
	bTestMode = true;
	bPracticeMode = false;
	bBotMode = false;
	bRevisionMode = false;
	RevisionMode = EBHRevisionMode::None;
	TargetBotCount = 0;
	GetWorldTimerManager().ClearTimer(RoundTimerHandle);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!BHPS)
		{
			continue;
		}

		BHPS->SetRole(EBHPlayerRole::Tester);
		BHPS->SetDesiredRole(EBHPlayerRole::Tester);
		BHPS->SetReady(true);
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
		BHPS->SetFakeHunterEligible(false);
	}

	RefreshTestDirector(TEXT("Test Round started. Tester role can use survivor objectives and Teacher tools."));
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Test Round: no minimum players, no match timer, infinite flashlight, round end blocked."), 5.0f);
	}
}

void ABHGameMode::RefreshTestDirector(const FString& Reason)
{
	if (!bTestMode)
	{
		return;
	}

	PrepareRoundDirector();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetPracticeMode(false);
		BHGS->SetTestMode(true);
		BHGS->SetBotOptions(false, 0, BotDifficulty);
		BHGS->SetRevisionOptions(EBHRevisionMode::None, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(0);
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(true);
		BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 12.0f), TEXT("Test Round is holding the simulation open."), BHGS->PresencePulse + 1);
	}

	UpdateDirectorGameState(FString::Printf(TEXT("Test Round: Tester role has survivor objectives, Teacher capture/scan/blackout, lockers, doors, lights, scares, and exit access. No ready-up, no match timer, no forced win/loss. Active: %d breakers, %d stations."),
		ActiveBreakerCount,
		ActiveSideObjectiveCount));

	StartDirectorTimer();
	BroadcastStatus(Reason, 4.0f);
}

EBHRoundModifier ABHGameMode::ChooseRoundModifier(FRandomStream& Stream) const
{
	if (ObjectiveIntensity <= 0 && !bPartyPace)
	{
		return EBHRoundModifier::None;
	}

	const int32 Roll = Stream.RandRange(0, 99);
	if (bPartyPace)
	{
		if (Roll < 10)
		{
			return EBHRoundModifier::None;
		}
		if (Roll < 34)
		{
			return EBHRoundModifier::LightsOut;
		}
		if (Roll < 56)
		{
			return EBHRoundModifier::LoudFooting;
		}
		if (Roll < 76)
		{
			return EBHRoundModifier::JammedDoors;
		}
		return EBHRoundModifier::PanicSurge;
	}

	if (Roll < 24)
	{
		return EBHRoundModifier::None;
	}
	if (Roll < 45)
	{
		return EBHRoundModifier::LightsOut;
	}
	if (Roll < 64)
	{
		return EBHRoundModifier::LoudFooting;
	}
	if (Roll < 82)
	{
		return EBHRoundModifier::JammedDoors;
	}
	return EBHRoundModifier::PanicSurge;
}

FString ABHGameMode::GetRoundModifierText(EBHRoundModifier Modifier) const
{
	switch (Modifier)
	{
	case EBHRoundModifier::LightsOut:
		return TEXT("Lights Out");
	case EBHRoundModifier::LoudFooting:
		return TEXT("Loud Footing");
	case EBHRoundModifier::JammedDoors:
		return TEXT("Jammed Doors");
	case EBHRoundModifier::PanicSurge:
		return TEXT("Panic Surge");
	case EBHRoundModifier::None:
	default:
		return TEXT("None");
	}
}

void ABHGameMode::StartPrepPhase()
{
	AssignRoles();
	PrepareRoundDirector();
	BotWorldStimuli.Reset();
	BotObjectiveClaims.Reset();
	BotTargetCooldowns.Reset();
	BotApproachPointCache.Reset();
	LoggedBotTacticalWarnings.Reset();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Prep);
		BHGS->SetRemainingTime(PrepSeconds);
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(false);
	}

	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ABHGameMode::TickRoundTimer, 1.0f, true);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(TEXT("Prep started. Breaker routes, doors, and lights were randomized."), 4.0f);
		}
	}
}

void ABHGameMode::StartHuntPhaseImmediately()
{
	AssignRoles();
	PrepareRoundDirector();
	BotWorldStimuli.Reset();
	BotObjectiveClaims.Reset();
	BotTargetCooldowns.Reset();
	BotApproachPointCache.Reset();
	LoggedBotTacticalWarnings.Reset();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(false);
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(HuntSeconds);
	}

	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ABHGameMode::TickRoundTimer, 1.0f, true);
	StartDirectorTimer();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(TEXT("Test hunt started. Answer questions, finish tasks, use lockers sparingly, and keep moving."), 4.0f);
		}
	}
}

void ABHGameMode::StartHuntPhase()
{
	SweepExpiredBotTacticalState();
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(HuntSeconds);
	}
	StartDirectorTimer();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(TEXT("Hunt started. Answer questions, finish tasks, and hide before the Teacher learns your route."), 4.0f);
		}
	}
}

void ABHGameMode::AssignRoles()
{
	if (!GameState)
	{
		return;
	}

	TArray<ABHPlayerState*> Players;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS)
		{
			Players.Add(BHPS);
		}
	}
	for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
	{
		ABHPlayerState* BotPS = Bot ? Bot->GetPlayerState<ABHPlayerState>() : nullptr;
		if (BotPS && !Players.Contains(BotPS))
		{
			Players.Add(BotPS);
			GameState->AddPlayerState(BotPS);
		}
	}

	if (Players.IsEmpty())
	{
		return;
	}

	const int32 DesiredHunterCount = FMath::Clamp(TargetHunterCount, 1, FMath::Max(1, Players.Num() - 1));
	TArray<ABHPlayerState*> ChosenHunters;
	for (ABHPlayerState* BHPS : Players)
	{
		if (BHPS && BHPS->DesiredRole == EBHPlayerRole::Hunter && ChosenHunters.Num() < DesiredHunterCount)
		{
			ChosenHunters.Add(BHPS);
		}
	}

	for (ABHPlayerState* BHPS : Players)
	{
		if (BHPS && BHPS->DesiredRole != EBHPlayerRole::Survivor && BHPS->DesiredRole != EBHPlayerRole::FakeHunter && !ChosenHunters.Contains(BHPS) && ChosenHunters.Num() < DesiredHunterCount)
		{
			ChosenHunters.Add(BHPS);
		}
	}

	for (ABHPlayerState* BHPS : Players)
	{
		if (BHPS && !ChosenHunters.Contains(BHPS) && ChosenHunters.Num() < DesiredHunterCount)
		{
			ChosenHunters.Add(BHPS);
		}
	}

	if (bBotMode)
	{
		const auto IsBotPlayerState = [this](const ABHPlayerState* Candidate)
		{
			if (!Candidate)
			{
				return false;
			}
			if (Candidate->IsABot())
			{
				return true;
			}
			for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
			{
				if (Bot && Bot->GetPlayerState<ABHPlayerState>() == Candidate)
				{
					return true;
				}
			}
			return false;
		};

		bool bChosenHumanExplicitlyRequestedHunter = false;
		for (const ABHPlayerState* Chosen : ChosenHunters)
		{
			if (Chosen && !IsBotPlayerState(Chosen) && Chosen->DesiredRole == EBHPlayerRole::Hunter)
			{
				bChosenHumanExplicitlyRequestedHunter = true;
				break;
			}
		}

		for (int32 HunterIndex = 0; HunterIndex < ChosenHunters.Num(); ++HunterIndex)
		{
			ABHPlayerState* Chosen = ChosenHunters[HunterIndex];
			if (!Chosen || IsBotPlayerState(Chosen) || bChosenHumanExplicitlyRequestedHunter)
			{
				continue;
			}

			ABHPlayerState* ReplacementBot = nullptr;
			for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
			{
				ABHPlayerState* Candidate = Bot ? Bot->GetPlayerState<ABHPlayerState>() : nullptr;
				if (Candidate && !ChosenHunters.Contains(Candidate) && Candidate->DesiredRole != EBHPlayerRole::FakeHunter)
				{
					ReplacementBot = Candidate;
					break;
				}
			}

			if (ReplacementBot)
			{
				UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot mode role override: %s takes Teacher slot instead of %s"),
					*GetNameSafe(ReplacementBot),
					*GetNameSafe(Chosen));
				ChosenHunters[HunterIndex] = ReplacementBot;
			}
		}
	}

	int32 AssignedSurvivors = 0;
	for (ABHPlayerState* BHPS : Players)
	{
		if (!BHPS)
		{
			continue;
		}

		BHPS->SetReady(false);
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
		if (ChosenHunters.Contains(BHPS))
		{
			BHPS->SetRole(EBHPlayerRole::Hunter);
			BHPS->SetFakeHunterEligible(false);
		}
		else if (BHPS->DesiredRole == EBHPlayerRole::FakeHunter || BHPS->bFakeHunterEligible)
		{
			BHPS->SetRole(EBHPlayerRole::FakeHunter);
			BHPS->SetFakeHunterEligible(false);
		}
		else
		{
			BHPS->SetRole(EBHPlayerRole::Survivor);
			++AssignedSurvivors;
		}
	}

	if (AssignedSurvivors == 0 && Players.Num() > 1)
	{
		for (int32 Index = ChosenHunters.Num() - 1; Index >= 0; --Index)
		{
			ABHPlayerState* BHPS = ChosenHunters[Index];
			if (BHPS)
			{
				BHPS->SetRole(EBHPlayerRole::Survivor);
				BHPS->SetFakeHunterEligible(false);
				++AssignedSurvivors;
				break;
			}
		}
	}

	int32 AssignedHunters = 0;
	int32 AssignedFakeHunters = 0;
	int32 AssignedBotHunters = 0;
	int32 AssignedBotSurvivors = 0;
	for (const ABHPlayerState* BHPS : Players)
	{
		if (!BHPS)
		{
			continue;
		}
		if (BHPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			++AssignedHunters;
			if (BHPS->IsABot())
			{
				++AssignedBotHunters;
			}
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Survivor)
		{
			if (BHPS->IsABot())
			{
				++AssignedBotSurvivors;
			}
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			++AssignedFakeHunters;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt roles assigned: players=%d hunters=%d survivors=%d monitors=%d botHunters=%d botSurvivors=%d"),
		Players.Num(),
		AssignedHunters,
		AssignedSurvivors,
		AssignedFakeHunters,
		AssignedBotHunters,
		AssignedBotSurvivors);

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		ABHPlayerState* BHPS = Controller ? Controller->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Controller || !Players.Contains(BHPS))
		{
			continue;
		}
		RestartPlayer(Controller);
	}
}

void ABHGameMode::TickRoundTimer()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS)
	{
		return;
	}

	if (bPracticeMode || bTestMode)
	{
		BHGS->SetRemainingTime(0);
		return;
	}

	BHGS->SetRemainingTime(BHGS->RemainingTime - 1);
	if (bRevisionMode && RevisionReviewTimeRemaining > 0)
	{
		RevisionReviewTimeRemaining = FMath::Max(0, RevisionReviewTimeRemaining - 1);
		UpdateRevisionSummary();
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Prep && BHGS->RemainingTime <= 0)
	{
		StartHuntPhase();
		return;
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Hunt)
	{
		if (CountEscapedSurvivors() > 0)
		{
			EndRound(EBHRoundPhase::SurvivorsWin);
		}
		else if (CountAliveSurvivors() <= 0 || BHGS->RemainingTime <= 0)
		{
			EndRound(EBHRoundPhase::HunterWin);
		}
	}
}

void ABHGameMode::EndRound(EBHRoundPhase ResultPhase)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase == EBHRoundPhase::HunterWin || BHGS->RoundPhase == EBHRoundPhase::SurvivorsWin)
	{
		return;
	}

	if (bPracticeMode || bTestMode)
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(0);
		BroadcastStatus(bTestMode ? TEXT("Test Round blocked the round end.") : TEXT("Practice Lab blocked the round end."), 2.5f);
		return;
	}

	BHGS->SetRoundPhase(ResultPhase);
	BHGS->SetRemainingTime(0);
	if (bBotMode)
	{
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot round summary: result=%s %s"),
			*StaticEnum<EBHRoundPhase>()->GetNameStringByValue(static_cast<int64>(ResultPhase)),
			*GetBotStatusReport());
	}
	if (bRevisionMode)
	{
		RevisionReviewTimeRemaining = 60;
		UpdateRevisionSummary(TEXT("Final review: check weak topics, corrected mistakes, and formula gaps before the next round."));
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt revision round summary: result=%s %s"),
			*StaticEnum<EBHRoundPhase>()->GetNameStringByValue(static_cast<int64>(ResultPhase)),
			*GetRevisionStatusReport());
	}

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ABHPlayerController* BHPC = Cast<ABHPlayerController>(It->Get());
			ABHPlayerState* BHPS = BHPC ? BHPC->GetPlayerState<ABHPlayerState>() : nullptr;
			if (BHPC && BHPS)
			{
				BHPC->ClientRecordRoundResult(BHPS->PlayerRole, BHPS->LifeState, ResultPhase);
			}
		}
	}

	GetWorldTimerManager().ClearTimer(RoundTimerHandle);
	GetWorldTimerManager().ClearTimer(DirectorTimerHandle);
	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ABHGameMode::ResetRoundByTravel, 8.0f, false);
}

void ABHGameMode::ResetRoundByTravel()
{
	if (GetWorld())
	{
		const FString TravelLevelName = NextRuntimeLevelName.IsEmpty() ? RuntimeLevelName : NextRuntimeLevelName;
		FString TravelURL = FString::Printf(TEXT("/Engine/Maps/Entry?listen?BHLevel=%s?BHFogPreset=%s"),
			*TravelLevelName,
			*FogPresetToString(NextFogPreset));
		if (bFogPresetOverride)
		{
			TravelURL += TEXT("?BHFogOverride=1");
		}
		if (bBotMode)
		{
			TravelURL += FString::Printf(TEXT("?BHBotMode=1?BHBotCount=%d?BHBotDifficulty=%s?BHHumanRole=Survivor"),
				FMath::Clamp(TargetBotCount, 0, FMath::Max(0, MaxPlayers - 1)),
				*BotDifficultyToString(BotDifficulty));
		}
		if (bTestMode)
		{
			TravelURL += TEXT("?BHTestMode=1");
		}
		if (bRevisionMode)
		{
			TravelURL += FString::Printf(TEXT("?BHRevisionMode=1?BHRevisionTopics=%d?BHRevisionDifficultyMix=%s?BHRevisionClassThreshold=%.0f?BHRevisionIndividualThreshold=%.0f?BHHuntSeconds=%d?BHScareIntensity=%d"),
				RevisionTopicMask,
				*RevisionDifficultyMixToString(RevisionDifficultyMix),
				RevisionClassThreshold,
				RevisionIndividualThreshold,
				RevisionRoundDuration,
				RevisionScareIntensity);
		}
		GetWorld()->ServerTravel(TravelURL);
	}
}

bool ABHGameMode::AreAllReady() const
{
	if (!GameState || GameState->PlayerArray.Num() < MinPlayers)
	{
		return false;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS || !BHPS->bReady)
		{
			return false;
		}
	}

	return true;
}

int32 ABHGameMode::CountAliveSurvivors() const
{
	int32 Count = 0;
	if (!GameState)
	{
		return Count;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS && BHPS->IsAliveSurvivor())
		{
			++Count;
		}
	}

	return Count;
}

int32 ABHGameMode::CountEscapedSurvivors() const
{
	int32 Count = 0;
	if (!GameState)
	{
		return Count;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Survivor && BHPS->LifeState == EBHPlayerLifeState::Escaped)
		{
			++Count;
		}
	}

	return Count;
}

void ABHGameMode::RefreshBotRoster(ABHPlayerController* RequestingController)
{
	if (!HasAuthority() || !bBotMode || !GetWorld())
	{
		return;
	}

	BotControllers.RemoveAll([](const TObjectPtr<ABHBotController>& Bot)
	{
		return !IsValid(Bot);
	});

	const int32 HumanCount = CountHumanPlayers();
	const int32 DesiredLiveBots = FMath::Clamp(TargetBotCount, 0, FMath::Max(0, MaxPlayers - HumanCount));
	while (BotControllers.Num() > DesiredLiveBots)
	{
		if (!RemoveOneBot())
		{
			break;
		}
	}

	while (BotControllers.Num() < DesiredLiveBots)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABHBotController* Bot = GetWorld()->SpawnActor<ABHBotController>(ABHBotController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (!Bot)
		{
			break;
		}

		if (!Bot->PlayerState)
		{
			Bot->InitPlayerState();
		}

		ABHPlayerState* BotPS = Bot->GetPlayerState<ABHPlayerState>();
		if (BotPS)
		{
			if (GameState && !GameState->PlayerArray.Contains(BotPS))
			{
				GameState->AddPlayerState(BotPS);
			}
			const int32 BotIndex = BotControllers.Num() + 1;
			BotPS->SetIsABot(true);
			BotPS->SetPlayerName(FString::Printf(TEXT("Bot %02d"), BotIndex));
			BotPS->SetReady(true);
			BotPS->SetDesiredRole(EBHPlayerRole::Unassigned);
			BotPS->SetRole(EBHPlayerRole::Unassigned);
			BotPS->SetLifeState(EBHPlayerLifeState::Alive);
			BotPS->SetHiddenInLocker(false);
			BotPS->SetAvatarIndex(BotIndex % 8);
			BotPS->SetAvatarColor(AvatarColorForIndex(BotIndex));
			BotPS->SetMapVote(TEXT(""));
			BotPS->ClearFogPresetVote();
			BotPS->SetFakeHunterEligible(false);
		}

		BotControllers.Add(Bot);
		RestartPlayer(Bot);
	}

	TrimBotRosterToCapacity();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
	}

	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot roster: %d active, target %d, humans %d"),
		BotControllers.Num(),
		TargetBotCount,
		HumanCount);

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Bot roster: %d active, target %d."), BotControllers.Num(), TargetBotCount), 3.0f);
	}
}

bool ABHGameMode::RemoveOneBot()
{
	for (int32 Index = BotControllers.Num() - 1; Index >= 0; --Index)
	{
		ABHBotController* Bot = BotControllers[Index];
		BotControllers.RemoveAt(Index);
		if (!Bot)
		{
			continue;
		}

		if (APawn* Pawn = Bot->GetPawn())
		{
			Pawn->Destroy();
		}
		if (GameState && Bot->PlayerState)
		{
			GameState->RemovePlayerState(Bot->PlayerState);
		}
		Bot->Destroy();
		return true;
	}

	return false;
}

void ABHGameMode::TrimBotRosterToCapacity()
{
	if (!GameState)
	{
		return;
	}

	while (GameState->PlayerArray.Num() > MaxPlayers)
	{
		if (!RemoveOneBot())
		{
			break;
		}
	}
}

int32 ABHGameMode::CountHumanPlayers() const
{
	int32 Count = 0;
	if (!GameState)
	{
		return Count;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		if (RawPS && !RawPS->IsABot())
		{
			++Count;
		}
	}

	return Count;
}

void ABHGameMode::SweepExpiredBotTacticalState()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	BotWorldStimuli.RemoveAll([Now](const FBHBotStimulus& Stimulus)
	{
		return Now - Stimulus.TimeSeconds > 45.0f;
	});
	BotObjectiveClaims.RemoveAll([Now, this](const FBHBotObjectiveClaim& Claim)
	{
		return !Claim.Claimant.IsValid() || !Claim.Target.IsValid() || Claim.ExpireTimeSeconds <= Now || !IsBotTargetStillUseful(Claim.Target.Get());
	});
	BotTargetCooldowns.RemoveAll([Now](const FBHBotTargetCooldown& Cooldown)
	{
		return !Cooldown.Claimant.IsValid() || !Cooldown.Target.IsValid() || Cooldown.ExpireTimeSeconds <= Now;
	});
	for (auto It = BotApproachPointCache.CreateIterator(); It; ++It)
	{
		if (!It.Key().ResolveObjectPtr())
		{
			It.RemoveCurrent();
		}
	}
}

bool ABHGameMode::IsExclusiveBotClaimIntent(EBHBotIntent Intent) const
{
	return Intent == EBHBotIntent::AnswerStation
		|| Intent == EBHBotIntent::WorkStation
		|| Intent == EBHBotIntent::RepairBreaker
		|| Intent == EBHBotIntent::Hide
		|| Intent == EBHBotIntent::SearchLocker
		|| Intent == EBHBotIntent::DropTrap;
}

bool ABHGameMode::IsBotTargetStillUseful(const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	if (const ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target))
	{
		return Station->IsDirectorActive() && !Station->IsCompleted();
	}
	if (const ABHBreaker* Breaker = Cast<ABHBreaker>(Target))
	{
		return Breaker->IsDirectorActive() && !Breaker->IsRepaired();
	}
	if (const ABHExitGate* Exit = Cast<ABHExitGate>(Target))
	{
		const ABHGameState* BHGS = GetGameState<ABHGameState>();
		return Exit->IsDirectorActive() && BHGS && BHGS->bExitUnlocked;
	}
	if (Cast<ABHLocker>(Target))
	{
		return true;
	}
	if (Cast<ABHCharacter>(Target))
	{
		return true;
	}
	return !Target->IsActorBeingDestroyed();
}

FTransform ABHGameMode::GetSpawnTransformFor(AController* Controller) const
{
	const ABHPlayerState* BHPS = Controller ? Controller->GetPlayerState<ABHPlayerState>() : nullptr;
	int32 PlayerIndex = 0;
	if (GameState && BHPS)
	{
		PlayerIndex = GameState->PlayerArray.IndexOfByKey(BHPS);
	}

	if (BHPS && (BHPS->PlayerRole == EBHPlayerRole::Hunter || BHPS->PlayerRole == EBHPlayerRole::FakeHunter))
	{
		const float OffsetSign = PlayerIndex % 2 == 0 ? 1.0f : -1.0f;
		const FVector SpawnOffset = BHPS->PlayerRole == EBHPlayerRole::FakeHunter
			? FVector(-180.0f, OffsetSign * (160.0f + 70.0f * PlayerIndex), 0.0f)
			: FVector(0.0f, OffsetSign * 80.0f * PlayerIndex, 0.0f);
		return FTransform(FRotator(0.0f, 0.0f, 0.0f), HunterSpawn + SpawnOffset);
	}

	const FVector SpawnLocation = SurvivorSpawns.IsValidIndex(PlayerIndex % FMath::Max(1, SurvivorSpawns.Num()))
		? SurvivorSpawns[PlayerIndex % SurvivorSpawns.Num()]
		: FVector(1000.0f, 0.0f, 120.0f);

	return FTransform(FRotator(0.0f, 180.0f, 0.0f), SpawnLocation);
}
