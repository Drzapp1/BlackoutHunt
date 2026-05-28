#include "BHBotController.h"
#include "BHBotPolicySubsystem.h"
#include "BHBreaker.h"
#include "BHCharacter.h"
#include "BHExitGate.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHLocker.h"
#include "BHObjectiveStation.h"
#include "BHPlayerState.h"
#include "AITypes.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"

namespace
{
FString BHBotIntentName(EBHBotIntent Intent)
{
	const UEnum* Enum = StaticEnum<EBHBotIntent>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Intent)) : TEXT("UnknownIntent");
}

FString BHBotPersonalityName(EBHBotPersonality InPersonality)
{
	const UEnum* Enum = StaticEnum<EBHBotPersonality>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(InPersonality)) : TEXT("UnknownPersonality");
}

FString BHBotArchetypeName(EBHPlayerRole Role, EBHBotPersonality InPersonality)
{
	if (Role == EBHPlayerRole::Hunter)
	{
		switch (InPersonality)
		{
		case EBHBotPersonality::Aggressive:
			return TEXT("AggressiveTeacher");
		case EBHBotPersonality::Suspicious:
			return TEXT("LockerInspectorTeacher");
		case EBHBotPersonality::Ambusher:
			return TEXT("ObjectiveAmbusherTeacher");
		default:
			return TEXT("TeacherBot");
		}
	}

	if (Role == EBHPlayerRole::FakeHunter)
	{
		switch (InPersonality)
		{
		case EBHBotPersonality::Trickster:
			return TEXT("MisdirectionHallMonitor");
		case EBHBotPersonality::Suspicious:
			return TEXT("RoamingHallMonitor");
		case EBHBotPersonality::Ambusher:
			return TEXT("TrapHallMonitor");
		default:
			return TEXT("HallMonitorBot");
		}
	}

	switch (InPersonality)
	{
	case EBHBotPersonality::Objective:
		return TEXT("ObjectiveRunner");
	case EBHBotPersonality::Cautious:
	case EBHBotPersonality::Panicked:
		return TEXT("CautiousHider");
	case EBHBotPersonality::Trickster:
	case EBHBotPersonality::Bold:
		return TEXT("DecoyBaiter");
	default:
		return TEXT("SurvivorBot");
	}
}

FString BHBotDecisionLabel(EBHPlayerRole Role, EBHBotPersonality InPersonality, const TCHAR* Action)
{
	return FString::Printf(TEXT("%s: %s"), *BHBotArchetypeName(Role, InPersonality), Action);
}

float BHBotRoleMoveSpeed(const ABHPlayerState* BotPS)
{
	if (BotPS && BotPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		return 395.0f;
	}
	if (BotPS && BotPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		return 440.0f;
	}

	return 470.0f;
}

bool BHBotIntentRequiresMovement(EBHBotIntent Intent)
{
	switch (Intent)
	{
	case EBHBotIntent::Patrol:
	case EBHBotIntent::Chase:
	case EBHBotIntent::InvestigateNoise:
	case EBHBotIntent::InvestigateLastSeen:
	case EBHBotIntent::SearchLocker:
	case EBHBotIntent::AmbushObjective:
	case EBHBotIntent::UseScan:
	case EBHBotIntent::UsePower:
	case EBHBotIntent::DropTrap:
		return true;
	default:
		return false;
	}
}

void BHApplyBotMovementProfile(ACharacter* MoveCharacter, const ABHPlayerState* BotPS)
{
	if (UCharacterMovementComponent* Movement = MoveCharacter ? MoveCharacter->GetCharacterMovement() : nullptr)
	{
		Movement->MaxWalkSpeed = BHBotRoleMoveSpeed(BotPS);
	}
}
}

ABHBotController::ABHBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;

	StateTreeAIComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	if (StateTreeAIComponent)
	{
		StateTreeAIComponent->SetStartLogicAutomatically(false);
	}

	BotPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("BotPerception"));
	SightSenseConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightSenseConfig"));
	HearingSenseConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingSenseConfig"));
	if (SightSenseConfig)
	{
		SightSenseConfig->SightRadius = 2800.0f;
		SightSenseConfig->LoseSightRadius = 3400.0f;
		SightSenseConfig->PeripheralVisionAngleDegrees = 76.0f;
		SightSenseConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightSenseConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightSenseConfig->DetectionByAffiliation.bDetectNeutrals = true;
	}
	if (HearingSenseConfig)
	{
		HearingSenseConfig->HearingRange = 3600.0f;
		HearingSenseConfig->DetectionByAffiliation.bDetectEnemies = true;
		HearingSenseConfig->DetectionByAffiliation.bDetectFriendlies = true;
		HearingSenseConfig->DetectionByAffiliation.bDetectNeutrals = true;
	}
	if (BotPerceptionComponent)
	{
		if (SightSenseConfig)
		{
			BotPerceptionComponent->ConfigureSense(*SightSenseConfig);
		}
		if (HearingSenseConfig)
		{
			BotPerceptionComponent->ConfigureSense(*HearingSenseConfig);
		}
		BotPerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
		SetPerceptionComponent(*BotPerceptionComponent);
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	bUseStateTreeAI = Settings ? Settings->bUseStateTreeAI : false;
	bStateTreeBrainRunning = false;
	bStateTreePolicyFallbackActivated = false;
	ThinkInterval = Settings ? FMath::Max(0.05f, Settings->BotThinkInterval) : 0.25f;
	SightRange = Settings ? FMath::Max(800.0f, Settings->BotSightRange) : 2800.0f;
	HearingMemorySeconds = Settings ? FMath::Max(1.0f, Settings->BotHearingMemorySeconds) : 12.0f;
	StuckSeconds = Settings ? FMath::Max(1.0f, Settings->BotStuckSeconds) : 6.0f;
	if (SightSenseConfig)
	{
		SightSenseConfig->SightRadius = SightRange;
		SightSenseConfig->LoseSightRadius = SightRange + 650.0f;
	}
	if (HearingSenseConfig)
	{
		HearingSenseConfig->HearingRange = SightRange * 1.15f;
	}
	NextThinkTime = 0.0f;
	LastAnswerAttemptTime = -999.0f;
	LastTrapAttemptTime = -999.0f;
	LastPowerAttemptTime = -999.0f;
	LastProgressTime = 0.0f;
	LastDecisionTime = -999.0f;
	LastBaitAttemptTime = -999.0f;
	LastHideExitTime = -999.0f;
	LastSeenHunterStimulusTime = -999.0f;
	LastSeenSurvivorStimulusTime = -999.0f;
	DecisionsMade = 0;
	IntentSwitches = 0;
	LastProgressLocation = FVector::ZeroVector;
	LastKnownSurvivorLocation = FVector::ZeroVector;
	LastKnownSurvivorTime = -999.0f;
	LastDecisionScore = 0.0f;
	LastDecisionCandidateCount = 0;
	Personality = EBHBotPersonality::Objective;
	bPersonalityChosen = false;
	bLastDecisionUsedPolicyModel = false;
	CurrentIntent = EBHBotIntent::None;
	LastStateTreeIntent = EBHBotIntent::None;
	LastStateTreeIntentLocation = FVector::ZeroVector;
	LastStateTreeIntentTime = -999.0f;
	CurrentPatrolDestination = FVector::ZeroVector;
	CurrentPatrolDestinationExpireTime = -999.0f;
	bHasPatrolDestination = false;
}

void ABHBotController::BeginPlay()
{
	Super::BeginPlay();
	LastProgressTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	LastProgressLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
	if (BotPerceptionComponent)
	{
		BotPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ABHBotController::OnTargetPerceptionUpdated);
	}
}

void ABHBotController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BotPerceptionComponent)
	{
		BotPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(this, &ABHBotController::OnTargetPerceptionUpdated);
	}

	if (ABHGameMode* BHGM = GetBHGameMode())
	{
		BHGM->ReleaseBotObjective(this);
	}
	ClearInteraction();
	CurrentClaimTarget.Reset();
	CurrentInteractTarget.Reset();
	Super::EndPlay(EndPlayReason);
}

void ABHBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextThinkTime)
	{
		NextThinkTime = Now + ThinkInterval;
		if (ShouldUseStateTreeBrain())
		{
			FString FallbackReason;
			if (ShouldFallbackFromStateTree(FallbackReason))
			{
				ActivateStateTreePolicyFallback(FallbackReason);
				Think();
			}
		}
		else
		{
			Think();
		}
	}
}

FString ABHBotController::GetBotDebugLine() const
{
	const ABHPlayerState* BotPS = GetBHPlayerState();
	const APawn* ControlledPawn = GetPawn();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float LastSeenAge = LastKnownSurvivorTime <= -900.0f ? -1.0f : Now - LastKnownSurvivorTime;
	FString Brain = (bUseStateTreeAI && BotPS && BotPS->PlayerRole == EBHPlayerRole::Hunter)
		? (bStateTreePolicyFallbackActivated ? TEXT("Policy(StateTreeFallback)") : TEXT("StateTreeMissingAsset"))
		: TEXT("Policy");
	if (StateTreeAIComponent && StateTreeAIComponent->IsRunning())
	{
		Brain = TEXT("StateTree");
#if WITH_GAMEPLAY_DEBUGGER
		const TArray<FName> ActiveStates = StateTreeAIComponent->GetActiveStateNames();
		if (!ActiveStates.IsEmpty())
		{
			TArray<FString> ActiveStateStrings;
			for (const FName ActiveState : ActiveStates)
			{
				ActiveStateStrings.Add(ActiveState.ToString());
			}
			Brain += FString::Printf(TEXT("[%s]"), *FString::Join(ActiveStateStrings, TEXT(">")));
		}
#endif
	}
	return FString::Printf(TEXT("%s role=%s personality=%s archetype=%s brain=%s intent=%s score=%.2f candidates=%d policy=%s target=%s claim=%s lastSeen=%.1fs decisions=%d switches=%d last=%s pos=(%.0f,%.0f,%.0f)"),
		*GetNameSafe(BotPS),
		BotPS ? *StaticEnum<EBHPlayerRole>()->GetNameStringByValue(static_cast<int64>(BotPS->PlayerRole)) : TEXT("None"),
		*BHBotPersonalityName(Personality),
		*GetBotArchetypeLabel(),
		*Brain,
		*BHBotIntentName(CurrentIntent),
		LastDecisionScore,
		LastDecisionCandidateCount,
		bLastDecisionUsedPolicyModel ? TEXT("local") : TEXT("cpp"),
		*GetNameSafe(CurrentInteractTarget.Get()),
		*GetNameSafe(CurrentClaimTarget.Get()),
		LastSeenAge,
		DecisionsMade,
		IntentSwitches,
		*LastDecisionDebugLabel,
		ControlledPawn ? ControlledPawn->GetActorLocation().X : 0.0f,
		ControlledPawn ? ControlledPawn->GetActorLocation().Y : 0.0f,
		ControlledPawn ? ControlledPawn->GetActorLocation().Z : 0.0f);
}

FString ABHBotController::GetBotArchetypeLabel() const
{
	const ABHPlayerState* BotPS = GetBHPlayerState();
	return BHBotArchetypeName(BotPS ? BotPS->PlayerRole : EBHPlayerRole::Unassigned, Personality);
}

void ABHBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	LastProgressLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	LastProgressTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ABHPlayerState* BotPS = GetBHPlayerState();
	bStateTreePolicyFallbackActivated = false;

	if (ACharacter* MoveCharacter = Cast<ACharacter>(InPawn))
	{
		MoveCharacter->bUseControllerRotationYaw = false;
		if (UCharacterMovementComponent* Movement = MoveCharacter->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = BHBotRoleMoveSpeed(BotPS);
			Movement->bOrientRotationToMovement = true;
			Movement->bUseControllerDesiredRotation = false;
			Movement->RotationRate = FRotator(0.0f, 620.0f, 0.0f);
			Movement->SetAvoidanceEnabled(true);
			Movement->AvoidanceConsiderationRadius = 360.0f;
			Movement->AvoidanceWeight = 0.72f;
		}
	}

	if (BotPS)
	{
		Personality = ChoosePersonality(BotPS);
		bPersonalityChosen = true;
	}
	StartStateTreeIfAvailable();
}

bool ABHBotController::RunStateTreeIntent(EBHBotIntent Intent, AActor* Target, const FVector& Location, float AcceptanceRadius)
{
	ABHCharacter* BotCharacter = GetBHCharacter();
	ABHPlayerState* BotPS = GetBHPlayerState();
	ABHGameState* BHGS = GetBHGameState();
	if (!BotCharacter || !BotPS || !BHGS || !GetWorld() || BotPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}

	BHApplyBotMovementProfile(BotCharacter, BotPS);

	if (ABHGameMode* BHGM = GetBHGameMode())
	{
		BHGM->GetBotWorldMemorySnapshot(LocalMemory, HearingMemorySeconds + 8.0f);
	}

	AActor* ResolvedTarget = Target;
	FVector ResolvedLocation = Location;
	if (!ResolvedTarget && ResolvedLocation.IsNearlyZero())
	{
		switch (Intent)
		{
		case EBHBotIntent::Patrol:
			ResolvedLocation = GetStablePatrolPoint(BotCharacter);
			break;
		case EBHBotIntent::Chase:
			ResolvedTarget = LocalMemory.LastSeenSurvivor.Get();
			if (!ResolvedTarget)
			{
				ResolvedTarget = FindVisibleSurvivor(SightRange, false);
			}
			ResolvedLocation = ResolvedTarget ? ResolvedTarget->GetActorLocation() : LastKnownSurvivorLocation;
			break;
		case EBHBotIntent::InvestigateNoise:
			if (!GetBHGameMode() || !GetBHGameMode()->GetLatestBotNoiseLocation(HearingMemorySeconds, ResolvedLocation))
			{
				ResolvedLocation = LocalMemory.LastHeardLocation;
			}
			break;
		case EBHBotIntent::InvestigateLastSeen:
			ResolvedLocation = LastKnownSurvivorTime > -900.0f ? LastKnownSurvivorLocation : LocalMemory.LastSeenSurvivorLocation;
			break;
		case EBHBotIntent::SearchLocker:
			ResolvedTarget = FindSuspiciousLocker(
				LastKnownSurvivorTime > -900.0f ? LastKnownSurvivorLocation : BotCharacter->GetActorLocation(),
				1100.0f,
				true);
			ResolvedLocation = ResolvedTarget ? ResolvedTarget->GetActorLocation() : BotCharacter->GetActorLocation();
			break;
		case EBHBotIntent::AmbushObjective:
			ResolvedTarget = FindHunterPatrolObjective();
			ResolvedLocation = ResolvedTarget
				? BuildAmbushLocation(LastKnownSurvivorTime > -900.0f ? LastKnownSurvivorLocation : BotCharacter->GetActorLocation(), ResolvedTarget->GetActorLocation())
				: GetStablePatrolPoint(BotCharacter);
			break;
		case EBHBotIntent::UseScan:
		case EBHBotIntent::UsePower:
			ResolvedTarget = LocalMemory.LastSeenSurvivor.Get();
			ResolvedLocation = ResolvedTarget ? ResolvedTarget->GetActorLocation() : (LastKnownSurvivorTime > -900.0f ? LastKnownSurvivorLocation : BotCharacter->GetActorLocation());
			break;
		default:
			ResolvedLocation = BotCharacter->GetActorLocation();
			break;
		}
	}

	FBHBotDecisionCandidate Decision;
	Decision.Intent = Intent;
	Decision.Target = ResolvedTarget;
	Decision.Location = ResolvedLocation;
	Decision.Distance = GetPawn() ? FVector::Dist2D(GetPawn()->GetActorLocation(), ResolvedTarget ? ResolvedTarget->GetActorLocation() : ResolvedLocation) : 0.0f;
	Decision.BaseScore = 1.0f;
	Decision.Risk = 0.0f;
	Decision.Urgency = 1.0f;
	const FString StateTreeAction = FString::Printf(TEXT("state tree %s"), *BHBotIntentName(Intent));
	Decision.DebugLabel = BHBotDecisionLabel(BotPS->PlayerRole, Personality, *StateTreeAction);

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bSameTarget = LastStateTreeIntentTarget.Get() == ResolvedTarget;
	const bool bSameIntent = LastStateTreeIntent == Intent;
	const bool bSameLocation = ResolvedLocation.IsNearlyZero()
		? LastStateTreeIntentLocation.IsNearlyZero()
		: FVector::DistSquared2D(LastStateTreeIntentLocation, ResolvedLocation) <= FMath::Square(Intent == EBHBotIntent::Chase ? 110.0f : 220.0f);
	const float MinCommitInterval = Intent == EBHBotIntent::Chase ? 0.22f : 0.65f;
	if (bSameIntent && bSameTarget && bSameLocation && Now - LastStateTreeIntentTime < MinCommitInterval)
	{
		return true;
	}

	LastStateTreeIntent = Intent;
	LastStateTreeIntentTarget = ResolvedTarget;
	LastStateTreeIntentLocation = ResolvedLocation;
	LastStateTreeIntentTime = Now;
	LastDecisionCandidateCount = 1;
	bLastDecisionUsedPolicyModel = false;
	CommitDecision(BotCharacter, BotPS, BHGS, Decision);
	return true;
}

bool ABHBotController::HasRecentStateTreeStimulus(EBHBotStimulusType Type, float MaxAgeSeconds, FVector& OutLocation) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float AgeLimit = FMath::Max(0.0f, MaxAgeSeconds);
	FBHBotMemory Memory = LocalMemory;
	if (ABHGameMode* BHGM = GetBHGameMode())
	{
		BHGM->GetBotWorldMemorySnapshot(Memory, AgeLimit + 1.0f);
	}
	for (int32 Index = Memory.RecentStimuli.Num() - 1; Index >= 0; --Index)
	{
		const FBHBotStimulus& Stimulus = Memory.RecentStimuli[Index];
		if (Stimulus.Type == Type && Now - Stimulus.TimeSeconds <= AgeLimit)
		{
			OutLocation = Stimulus.Location;
			return true;
		}
	}
	if (Type == EBHBotStimulusType::Sight && Now - Memory.LastSeenSurvivorTime <= AgeLimit)
	{
		OutLocation = Memory.LastSeenSurvivorLocation;
		return true;
	}
	if (Type == EBHBotStimulusType::Noise && Now - Memory.LastHeardTime <= AgeLimit)
	{
		OutLocation = Memory.LastHeardLocation;
		return true;
	}
	return false;
}

void ABHBotController::StartStateTreeIfAvailable()
{
	bStateTreeBrainRunning = false;
	if (!HasAuthority() || !bUseStateTreeAI || !StateTreeAIComponent)
	{
		return;
	}

	const ABHPlayerState* BotPS = GetBHPlayerState();
	if (!BotPS || BotPS->PlayerRole != EBHPlayerRole::Hunter)
	{
		LastDecisionDebugLabel = TEXT("state tree disabled for non-hunter bot");
		return;
	}

	UStateTree* StateTreeAsset = HunterStateTreeAsset.Get();
	if (!StateTreeAsset)
	{
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		if (Settings)
		{
			TSoftObjectPtr<UStateTree> ConfiguredStateTree = Settings->HunterStateTreeAsset;
			StateTreeAsset = ConfiguredStateTree.LoadSynchronous();
		}
	}
	if (!StateTreeAsset)
	{
		LastDecisionDebugLabel = TEXT("state tree asset missing; policy fallback");
		return;
	}

	if (StateTreeAIComponent->IsRunning())
	{
		bStateTreeBrainRunning = true;
		if (!ActiveStateTreeAsset)
		{
			ActiveStateTreeAsset = StateTreeAsset;
		}
		LastDecisionDebugLabel = ActiveStateTreeAsset == StateTreeAsset
			? TEXT("state tree hunter brain")
			: TEXT("state tree already running; deferred asset change");
		return;
	}
	StateTreeAIComponent->SetStateTree(StateTreeAsset);
	StateTreeAIComponent->StartLogic();
	bStateTreeBrainRunning = StateTreeAIComponent->IsRunning();
	ActiveStateTreeAsset = bStateTreeBrainRunning ? StateTreeAsset : nullptr;
	LastDecisionDebugLabel = bStateTreeBrainRunning ? TEXT("state tree hunter brain") : TEXT("state tree failed to start; policy fallback");
}

bool ABHBotController::ShouldUseStateTreeBrain() const
{
	return bUseStateTreeAI && bStateTreeBrainRunning && StateTreeAIComponent && StateTreeAIComponent->IsRunning();
}

bool ABHBotController::ShouldFallbackFromStateTree(FString& OutReason) const
{
	OutReason.Reset();
	if (!ShouldUseStateTreeBrain() || !GetWorld())
	{
		return false;
	}

	const ABHPlayerState* BotPS = GetBHPlayerState();
	if (!BotPS || BotPS->PlayerRole != EBHPlayerRole::Hunter)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float StartupGraceSeconds = FMath::Max(1.0f, ThinkInterval * 4.0f);
	if ((LastDecisionTime <= -900.0f || CurrentIntent == EBHBotIntent::None)
		&& Now - LastProgressTime >= StartupGraceSeconds)
	{
		OutReason = TEXT("no hunter intent emitted");
		return true;
	}

	const ABHGameState* BHGS = GetBHGameState();
	if (BHGS
		&& BHGS->RoundPhase == EBHRoundPhase::Hunt
		&& BHBotIntentRequiresMovement(CurrentIntent)
		&& LastDecisionTime > -900.0f)
	{
		const UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();
		const APawn* ControlledPawn = GetPawn();
		const bool bPathMoving = PathFollowing && PathFollowing->GetStatus() == EPathFollowingStatus::Moving;
		const bool bPawnMoving = ControlledPawn && ControlledPawn->GetVelocity().SizeSquared2D() > FMath::Square(20.0f);
		const float IdleDecisionSeconds = Now - LastDecisionTime;
		const float IdleWatchdogSeconds = FMath::Max(1.25f, ThinkInterval * 5.0f);
		if (!bPathMoving && !bPawnMoving && IdleDecisionSeconds >= IdleWatchdogSeconds)
		{
			OutReason = FString::Printf(TEXT("hunter state tree stopped during %s"), *BHBotIntentName(CurrentIntent));
			return true;
		}
	}

	FBHBotMemory Memory = LocalMemory;
	if (ABHGameMode* BHGM = GetBHGameMode())
	{
		BHGM->GetBotWorldMemorySnapshot(Memory, 3.0f);
	}

	const float RecentSightAge = Now - Memory.LastSeenSurvivorTime;
	if (RecentSightAge >= 0.55f
		&& RecentSightAge <= 2.0f
		&& CurrentIntent != EBHBotIntent::Chase
		&& CurrentIntent != EBHBotIntent::InvestigateLastSeen)
	{
		OutReason = TEXT("fresh survivor sight ignored");
		return true;
	}

	const bool bNoiseResponsiveIntent = CurrentIntent == EBHBotIntent::InvestigateNoise
		|| CurrentIntent == EBHBotIntent::InvestigateLastSeen
		|| CurrentIntent == EBHBotIntent::Chase
		|| CurrentIntent == EBHBotIntent::UseScan;
	const float RecentNoiseAge = Now - Memory.LastHeardTime;
	if (RecentNoiseAge >= 0.75f
		&& RecentNoiseAge <= 2.0f
		&& !bNoiseResponsiveIntent)
	{
		OutReason = TEXT("fresh noise ignored");
		return true;
	}

	return false;
}

void ABHBotController::ActivateStateTreePolicyFallback(const FString& Reason)
{
	if (bStateTreePolicyFallbackActivated)
	{
		bStateTreeBrainRunning = false;
		return;
	}

	bStateTreePolicyFallbackActivated = true;
	bStateTreeBrainRunning = false;
	if (StateTreeAIComponent && StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->StopLogic(Reason);
	}
	LastDecisionDebugLabel = FString::Printf(TEXT("state tree policy fallback: %s"), *Reason);
	UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt hunter StateTree fallback for %s: %s"),
		*GetNameSafe(this),
		Reason.IsEmpty() ? TEXT("unspecified") : *Reason);
}

void ABHBotController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!HasAuthority() || !Actor || !Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const FVector StimulusLocation = Stimulus.StimulusLocation.IsNearlyZero() ? Actor->GetActorLocation() : Stimulus.StimulusLocation;
	const bool bSightStimulus = Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>();
	const bool bHearingStimulus = Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>();

	if (const ABHCharacter* SeenCharacter = Cast<ABHCharacter>(Actor))
	{
		const ABHPlayerState* SeenPS = SeenCharacter->GetPlayerState<ABHPlayerState>();
		if (SeenPS && SeenPS->IsAliveSurvivor())
		{
			LastKnownSurvivorLocation = StimulusLocation;
			LastKnownSurvivorTime = Now;
			LocalMemory.LastSeenSurvivorLocation = StimulusLocation;
			LocalMemory.LastSeenSurvivorTime = Now;
			LocalMemory.LastSeenSurvivor = const_cast<ABHCharacter*>(SeenCharacter);
			if (bSightStimulus)
			{
				RecordStimulus(EBHBotStimulusType::Sight, Actor, StimulusLocation, TEXT("bot perception survivor sight"), 1.0f);
			}
		}
		else if (SeenPS && SeenPS->IsAliveHunter())
		{
			LocalMemory.LastSeenHunterLocation = StimulusLocation;
			LocalMemory.LastSeenHunterTime = Now;
			LocalMemory.LastSeenHunter = const_cast<ABHCharacter*>(SeenCharacter);
			if (bSightStimulus)
			{
				RecordStimulus(EBHBotStimulusType::Sight, Actor, StimulusLocation, TEXT("bot perception hunter sight"), 1.0f);
			}
		}
	}

	if (bHearingStimulus)
	{
		LocalMemory.LastHeardLocation = StimulusLocation;
		LocalMemory.LastHeardTime = Now;
		RecordStimulus(EBHBotStimulusType::Noise, Actor, StimulusLocation, TEXT("bot perception hearing"), 0.8f);
	}
}

void ABHBotController::ReportStateTreeDebugState(const FString& InStateName)
{
	LastDecisionDebugLabel = FString::Printf(TEXT("state tree %s"), *InStateName);
}

ABHCharacter* ABHBotController::GetBHCharacter() const
{
	return Cast<ABHCharacter>(GetPawn());
}

ABHPlayerState* ABHBotController::GetBHPlayerState() const
{
	return GetPlayerState<ABHPlayerState>();
}

ABHGameState* ABHBotController::GetBHGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
}

ABHGameMode* ABHBotController::GetBHGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr;
}

void ABHBotController::Think()
{
	ABHCharacter* BotCharacter = GetBHCharacter();
	ABHPlayerState* BotPS = GetBHPlayerState();
	ABHGameState* BHGS = GetBHGameState();
	if (!BotCharacter || !BotPS || !BHGS || BotPS->LifeState != EBHPlayerLifeState::Alive)
	{
		ClearInteraction();
		StopMovement();
		return;
	}

	if (ABHGameMode* BHGM = GetBHGameMode(); BHGM && !BHGM->IsRuntimeNavigationReady())
	{
		ClearInteraction();
		StopMovement();
		return;
	}

	if (!bPersonalityChosen)
	{
		Personality = ChoosePersonality(BotPS);
		bPersonalityChosen = true;
	}
	BHApplyBotMovementProfile(BotCharacter, BotPS);
	if (ABHGameMode* BHGM = GetBHGameMode())
	{
		BHGM->GetBotWorldMemorySnapshot(LocalMemory, HearingMemorySeconds + 8.0f);
	}

	if (BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		ClearInteraction();
		if (BHGS->RoundPhase == EBHRoundPhase::Prep)
		{
			MoveTowardLocation(GetStablePatrolPoint(BotCharacter, 5.5f, 800.0f), 180.0f);
		}
		return;
	}

	if (BotPS->PlayerRole == EBHPlayerRole::Survivor)
	{
		ThinkWithCandidates(BotCharacter, BotPS, BHGS, TEXT("no free survivor objective"));
	}
	else if (BotPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		ThinkWithCandidates(BotCharacter, BotPS, BHGS, TEXT("no teacher lead"));
	}
	else if (BotPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		ThinkWithCandidates(BotCharacter, BotPS, BHGS, TEXT("monitor patrol"));
	}
}

void ABHBotController::ThinkWithCandidates(ABHCharacter* BotCharacter, ABHPlayerState* BotPS, ABHGameState* BHGS, const TCHAR* EmptyPatrolLabel)
{
	TArray<FBHBotDecisionCandidate> Candidates;
	BuildDecisionCandidates(BotCharacter, BotPS, BHGS, Candidates);
	if (Candidates.IsEmpty())
	{
		FBHBotDecisionCandidate PatrolDecision;
		PatrolDecision.Intent = EBHBotIntent::Patrol;
		PatrolDecision.Location = GetStablePatrolPoint(BotCharacter);
		PatrolDecision.DebugLabel = EmptyPatrolLabel;
		LastDecisionCandidateCount = 1;
		bLastDecisionUsedPolicyModel = false;
		CommitDecision(BotCharacter, BotPS, BHGS, PatrolDecision);
		return;
	}

	FBHBotPolicyResult Result = ScoreDecisionCandidate(BuildPolicyFeatures(BotCharacter, BotPS, BHGS, Candidates), Candidates);
	if (Candidates.IsValidIndex(Result.ChosenIndex))
	{
		LastDecisionCandidateCount = Candidates.Num();
		bLastDecisionUsedPolicyModel = Result.bUsedModel;
		CommitDecision(BotCharacter, BotPS, BHGS, Candidates[Result.ChosenIndex]);
	}
}

void ABHBotController::BuildDecisionCandidates(ABHCharacter* BotCharacter, ABHPlayerState* BotPS, ABHGameState* BHGS, TArray<FBHBotDecisionCandidate>& OutCandidates)
{
	OutCandidates.Reset();
	if (!BotCharacter || !BotPS || !BHGS)
	{
		return;
	}

	if (BotPS->PlayerRole == EBHPlayerRole::Survivor)
	{
		BuildSurvivorDecisionCandidates(BotCharacter, BHGS, OutCandidates);
	}
	else if (BotPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		BuildTeacherDecisionCandidates(BotCharacter, true, OutCandidates);
	}
	else if (BotPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		BuildTeacherDecisionCandidates(BotCharacter, false, OutCandidates);
	}
}

void ABHBotController::BuildSurvivorDecisionCandidates(ABHCharacter* BotCharacter, ABHGameState* BHGS, TArray<FBHBotDecisionCandidate>& OutCandidates)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const FVector BotLocation = BotCharacter->GetActorLocation();
	const float ObjectivePressure = GetObjectivePressure(BHGS);
	const bool bObjectiveRunner = Personality == EBHBotPersonality::Objective;
	const bool bCautiousHider = Personality == EBHBotPersonality::Cautious || Personality == EBHBotPersonality::Panicked;
	const bool bDecoyBaiter = Personality == EBHBotPersonality::Trickster || Personality == EBHBotPersonality::Bold;
	ABHCharacter* Threat = FindVisibleTeacherThreat(SightRange);
	if (Threat)
	{
		LocalMemory.LastSeenHunter = Threat;
		LocalMemory.LastSeenHunterLocation = Threat->GetActorLocation();
		LocalMemory.LastSeenHunterTime = Now;
		if (Now - LastSeenHunterStimulusTime > 2.0f)
		{
			LastSeenHunterStimulusTime = Now;
			RecordStimulus(EBHBotStimulusType::Sight, Threat, Threat->GetActorLocation(), TEXT("survivor saw teacher"), 1.4f);
		}
	}

	if (BotCharacter->IsHiddenInLocker())
	{
		if (ShouldStayHidden(BotCharacter, Threat, BHGS))
		{
			AddCandidate(OutCandidates, EBHBotIntent::Hide, nullptr, BotLocation, bCautiousHider ? 2.55f : 2.0f, 0.05f, 0.7f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("stay hidden")));
			return;
		}
	}

	if (Threat && !BotCharacter->IsHiddenInLocker())
	{
		const float LockerSearchRange = bCautiousHider ? 1450.0f : (bObjectiveRunner ? 850.0f : 950.0f);
		if (ABHLocker* Locker = FindNearestLocker(true, false, LockerSearchRange))
		{
			AddCandidate(OutCandidates, EBHBotIntent::Hide, Locker, Locker->GetActorLocation(), bCautiousHider ? 3.08f : 2.42f, bCautiousHider ? 0.10f : 0.18f, 1.0f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("hide from teacher")));
		}

		const FVector Away = (BotLocation - Threat->GetActorLocation()).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Away, FVector::UpVector).GetSafeNormal();
		const FVector FleeLocation = BotLocation + (Away * FMath::FRandRange(850.0f, 1250.0f)) + (Side * FMath::FRandRange(-420.0f, 420.0f));
		AddCandidate(OutCandidates, EBHBotIntent::Flee, Threat, FleeLocation, bCautiousHider ? 2.45f : (bObjectiveRunner ? 2.12f : 2.25f), 0.24f, 0.92f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("break line of sight")));
		if (bDecoyBaiter && Now - LastBaitAttemptTime > (Personality == EBHBotPersonality::Trickster ? 7.0f : 9.0f))
		{
			AddCandidate(OutCandidates, EBHBotIntent::Bait, Threat, FleeLocation, Personality == EBHBotPersonality::Trickster ? 2.36f : 2.08f, 0.42f, 0.88f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("drop decoy bait")));
		}
	}
	else if (!Threat && LocalMemory.LastSeenHunterTime > -1000.0f)
	{
		const float HunterMemoryAge = Now - LocalMemory.LastSeenHunterTime;
		const bool bFreshHunterMemory = HunterMemoryAge <= (bCautiousHider ? 7.5f : 5.0f);
		if (bFreshHunterMemory && bCautiousHider && ObjectivePressure < (Personality == EBHBotPersonality::Panicked ? 0.92f : 0.74f))
		{
			const float LockerSearchRange = Personality == EBHBotPersonality::Panicked ? 1550.0f : 1250.0f;
			if (ABHLocker* Locker = FindNearestLocker(true, false, LockerSearchRange))
			{
				AddCandidate(OutCandidates, EBHBotIntent::Hide, Locker, Locker->GetActorLocation(), Personality == EBHBotPersonality::Panicked ? 2.36f : 2.08f, 0.08f, 0.72f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("hide after losing teacher")));
			}
		}
		if (bFreshHunterMemory && bDecoyBaiter && Now - LastBaitAttemptTime > (Personality == EBHBotPersonality::Trickster ? 8.0f : 11.0f))
		{
			FVector Away = (BotLocation - LocalMemory.LastSeenHunterLocation).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal();
			}
			const FVector Side = FVector::CrossProduct(Away, FVector::UpVector).GetSafeNormal();
			const FVector BaitLocation = BotLocation + Away * FMath::FRandRange(650.0f, 980.0f) + Side * FMath::FRandRange(-360.0f, 360.0f);
			AddCandidate(OutCandidates, EBHBotIntent::Bait, LocalMemory.LastSeenHunter.Get(), BaitLocation, Personality == EBHBotPersonality::Trickster ? 1.78f : 1.52f, 0.34f, 0.58f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("draw teacher off route")));
		}
	}

	if (BHGS && BHGS->bExitUnlocked)
	{
		for (TActorIterator<ABHExitGate> It(GetWorld()); It; ++It)
		{
			ABHExitGate* Exit = *It;
			if (Exit && Exit->IsDirectorActive())
			{
				AddCandidate(OutCandidates, EBHBotIntent::Escape, Exit, Exit->GetActorLocation(), 2.8f + ObjectivePressure + (bObjectiveRunner ? 0.35f : 0.0f), GetThreatPressureForLocation(Exit->GetActorLocation()), 1.0f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("escape")));
			}
		}
	}

	for (TActorIterator<ABHObjectiveStation> It(GetWorld()); It; ++It)
	{
		ABHObjectiveStation* Station = *It;
		if (!Station || !Station->IsDirectorActive() || Station->IsCompleted())
		{
			continue;
		}

		const EBHBotIntent Intent = Station->IsQuestionSolved() ? EBHBotIntent::WorkStation : EBHBotIntent::AnswerStation;
		float Base = Station->IsQuestionSolved() ? 1.35f : 1.65f;
		float Risk = GetThreatPressureForLocation(Station->GetActorLocation());
		float Urgency = 0.75f + ObjectivePressure * 0.2f;
		if (bObjectiveRunner)
		{
			Base += 0.42f;
			Risk *= 0.72f;
			Urgency += 0.12f;
		}
		else if (bCautiousHider)
		{
			Base -= ObjectivePressure >= 0.78f ? 0.06f : 0.24f;
			Risk += 0.14f;
		}
		AddCandidate(OutCandidates, Intent, Station, Station->GetActorLocation(), Base + ObjectivePressure * 0.55f, Risk, Urgency, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, Station->IsQuestionSolved() ? TEXT("work station") : TEXT("answer station")));
	}

	for (TActorIterator<ABHBreaker> It(GetWorld()); It; ++It)
	{
		ABHBreaker* Breaker = *It;
		if (!Breaker || !Breaker->IsDirectorActive() || Breaker->IsRepaired())
		{
			continue;
		}

		const float SideWorkDone = BHGS && BHGS->SideObjectivesRequired > 0
			? static_cast<float>(BHGS->SideObjectivesCompleted) / static_cast<float>(BHGS->SideObjectivesRequired)
			: 1.0f;
		AddCandidate(OutCandidates, EBHBotIntent::RepairBreaker, Breaker, Breaker->GetActorLocation(), 1.15f + SideWorkDone * 0.35f + ObjectivePressure * 0.35f + (bObjectiveRunner ? 0.30f : 0.0f), GetThreatPressureForLocation(Breaker->GetActorLocation()) + (bCautiousHider ? 0.10f : 0.0f), 0.62f + ObjectivePressure * 0.25f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("repair breaker")));
	}

	if (OutCandidates.IsEmpty() || FMath::FRand() < 0.12f)
	{
		AddCandidate(OutCandidates, EBHBotIntent::Patrol, nullptr, GetStablePatrolPoint(BotCharacter), bDecoyBaiter ? 0.46f : 0.28f, 0.1f, 0.1f, BHBotDecisionLabel(EBHPlayerRole::Survivor, Personality, TEXT("route variation")));
	}
}

void ABHBotController::BuildTeacherDecisionCandidates(ABHCharacter* BotCharacter, bool bCanCapture, TArray<FBHBotDecisionCandidate>& OutCandidates)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const EBHPlayerRole BotRole = bCanCapture ? EBHPlayerRole::Hunter : EBHPlayerRole::FakeHunter;
	const bool bAggressiveTeacher = bCanCapture && Personality == EBHBotPersonality::Aggressive;
	const bool bSuspiciousHunter = Personality == EBHBotPersonality::Suspicious;
	const bool bAmbusher = Personality == EBHBotPersonality::Ambusher;
	const bool bHallMonitor = !bCanCapture;
	const bool bMisdirectionMonitor = bHallMonitor && Personality == EBHBotPersonality::Trickster;
	const bool bRoamingMonitor = bHallMonitor && Personality == EBHBotPersonality::Suspicious;
	const bool bTrapMonitor = bHallMonitor && Personality == EBHBotPersonality::Ambusher;
	if (ABHCharacter* Target = FindVisibleSurvivor(SightRange, false))
	{
		LastKnownSurvivorLocation = Target->GetActorLocation();
		LastKnownSurvivorTime = Now;
		LocalMemory.LastSeenSurvivor = Target;
		LocalMemory.LastSeenSurvivorLocation = LastKnownSurvivorLocation;
		LocalMemory.LastSeenSurvivorTime = Now;
		if (Now - LastSeenSurvivorStimulusTime > 1.4f)
		{
			LastSeenSurvivorStimulusTime = Now;
			RecordStimulus(EBHBotStimulusType::Sight, Target, LastKnownSurvivorLocation, bCanCapture ? TEXT("teacher saw survivor") : TEXT("monitor saw survivor"), 1.5f);
		}
		const float ContactBase = bCanCapture
			? (bAggressiveTeacher ? 3.52f : (bAmbusher ? 2.94f : 3.1f))
			: (bMisdirectionMonitor ? 2.34f : (bTrapMonitor ? 2.18f : 2.02f));
		AddCandidate(OutCandidates, bCanCapture ? EBHBotIntent::Chase : EBHBotIntent::DropTrap, Target, Target->GetActorLocation(), ContactBase, 0.05f, 1.0f, BHBotDecisionLabel(BotRole, Personality, bCanCapture ? TEXT("chase visible survivor") : TEXT("pressure visible survivor")));
		if (Now - LastPowerAttemptTime > (bAggressiveTeacher ? 3.6f : 5.0f))
		{
			AddCandidate(OutCandidates, EBHBotIntent::UsePower, Target, Target->GetActorLocation(), bCanCapture ? (bAggressiveTeacher ? 1.95f : 1.65f) : 1.32f, 0.0f, bAggressiveTeacher ? 0.78f : 0.65f, BHBotDecisionLabel(BotRole, Personality, bCanCapture ? TEXT("blackout on contact") : TEXT("false pressure on contact")));
		}
	}

	const EBHBotDifficulty Difficulty = GetBHGameMode() ? GetBHGameMode()->GetBotDifficulty() : EBHBotDifficulty::Normal;
	const bool bAllowEmptySuspicion = Difficulty == EBHBotDifficulty::Hard || bSuspiciousHunter;
	const float LastSeenAge = Now - LastKnownSurvivorTime;
	if (LastSeenAge <= HearingMemorySeconds + 8.0f)
	{
		AddCandidate(OutCandidates, EBHBotIntent::InvestigateLastSeen, nullptr, LastKnownSurvivorLocation, 1.55f - LastSeenAge * 0.035f + (bSuspiciousHunter ? 0.22f : 0.0f) + (bAggressiveTeacher ? 0.12f : 0.0f), 0.04f, 0.68f, BHBotDecisionLabel(BotRole, Personality, TEXT("last seen route")));
		if (bMisdirectionMonitor && Now - LastPowerAttemptTime > 13.0f)
		{
			const FVector FalseMarkerLocation = BuildAmbushLocation(LastKnownSurvivorLocation, BotCharacter->GetActorLocation());
			AddCandidate(OutCandidates, EBHBotIntent::UsePower, nullptr, FalseMarkerLocation, 1.22f - LastSeenAge * 0.025f, 0.0f, 0.62f, BHBotDecisionLabel(BotRole, Personality, TEXT("false marker off last seen")));
		}
		const float LockerSearchRange = Difficulty == EBHBotDifficulty::Hard ? 1250.0f : (bSuspiciousHunter ? 1180.0f : 900.0f);
		if (ABHLocker* Locker = FindSuspiciousLocker(LastKnownSurvivorLocation, LockerSearchRange, bAllowEmptySuspicion))
		{
			AddCandidate(OutCandidates, EBHBotIntent::SearchLocker, Locker, Locker->GetActorLocation(), 1.38f + (bAllowEmptySuspicion ? 0.28f : 0.0f) + (bSuspiciousHunter ? 0.24f : 0.0f), 0.08f, 0.7f, BHBotDecisionLabel(BotRole, Personality, TEXT("check suspicious locker")));
		}
	}

	FVector NoiseLocation = FVector::ZeroVector;
	if (GetBHGameMode() && GetBHGameMode()->GetLatestBotNoiseLocation(HearingMemorySeconds, NoiseLocation))
	{
		AddCandidate(OutCandidates, EBHBotIntent::InvestigateNoise, nullptr, NoiseLocation, 1.22f + (bSuspiciousHunter ? 0.24f : 0.0f) + (bRoamingMonitor ? 0.18f : 0.0f), 0.08f, 0.58f, BHBotDecisionLabel(BotRole, Personality, TEXT("investigate noise")));
		if (Now - LastPowerAttemptTime > (bAggressiveTeacher ? 5.5f : 7.0f))
		{
			AddCandidate(OutCandidates, EBHBotIntent::UseScan, nullptr, NoiseLocation, 0.9f + (bSuspiciousHunter ? 0.18f : 0.0f) + (bRoamingMonitor ? 0.18f : 0.0f), 0.0f, 0.44f, BHBotDecisionLabel(BotRole, Personality, TEXT("scan stale noise")));
			if (bMisdirectionMonitor)
			{
				AddCandidate(OutCandidates, EBHBotIntent::UsePower, nullptr, NoiseLocation, 1.08f, 0.0f, 0.48f, BHBotDecisionLabel(BotRole, Personality, TEXT("false marker near noise")));
			}
		}
	}

	for (TActorIterator<ABHObjectiveStation> It(GetWorld()); It; ++It)
	{
		ABHObjectiveStation* Station = *It;
		if (Station && Station->IsDirectorActive() && !Station->IsCompleted())
		{
			const float CrowdBonus = CountNearbyAllies(Station->GetActorLocation(), EBHPlayerRole::Survivor, 1400.0f) * 0.05f;
			const float RoutePrediction = LastSeenAge <= HearingMemorySeconds + 8.0f
				? FMath::Clamp(1.0f - FVector::Dist2D(Station->GetActorLocation(), LastKnownSurvivorLocation) / 5200.0f, 0.0f, 0.55f)
				: 0.0f;
			const FVector AmbushLocation = LastSeenAge <= HearingMemorySeconds + 8.0f ? BuildAmbushLocation(LastKnownSurvivorLocation, Station->GetActorLocation()) : Station->GetActorLocation();
			AddCandidate(OutCandidates, EBHBotIntent::AmbushObjective, Station, AmbushLocation, 1.0f + FMath::Max(0.0f, CrowdBonus) + RoutePrediction + (bAmbusher ? 0.34f : 0.0f) + (bRoamingMonitor ? 0.16f : 0.0f) + (bTrapMonitor ? 0.20f : 0.0f), 0.0f, 0.46f + RoutePrediction, BHBotDecisionLabel(BotRole, Personality, bCanCapture ? TEXT("ambush station") : TEXT("monitor station route")));
		}
	}
	for (TActorIterator<ABHBreaker> It(GetWorld()); It; ++It)
	{
		ABHBreaker* Breaker = *It;
		if (Breaker && Breaker->IsDirectorActive() && !Breaker->IsRepaired())
		{
			const float RoutePrediction = LastSeenAge <= HearingMemorySeconds + 8.0f
				? FMath::Clamp(1.0f - FVector::Dist2D(Breaker->GetActorLocation(), LastKnownSurvivorLocation) / 5200.0f, 0.0f, 0.45f)
				: 0.0f;
			const FVector AmbushLocation = LastSeenAge <= HearingMemorySeconds + 8.0f ? BuildAmbushLocation(LastKnownSurvivorLocation, Breaker->GetActorLocation()) : Breaker->GetActorLocation();
			AddCandidate(OutCandidates, EBHBotIntent::AmbushObjective, Breaker, AmbushLocation, 0.86f + RoutePrediction + (bAmbusher ? 0.28f : 0.0f) + (bRoamingMonitor ? 0.16f : 0.0f) + (bTrapMonitor ? 0.18f : 0.0f), 0.0f, 0.36f + RoutePrediction, BHBotDecisionLabel(BotRole, Personality, bCanCapture ? TEXT("ambush breaker") : TEXT("monitor breaker route")));
		}
	}
	if (ABHExitGate* Exit = FindActiveExit())
	{
		const FVector AmbushLocation = LastSeenAge <= HearingMemorySeconds + 8.0f ? BuildAmbushLocation(LastKnownSurvivorLocation, Exit->GetActorLocation()) : Exit->GetActorLocation();
		AddCandidate(OutCandidates, EBHBotIntent::AmbushObjective, Exit, AmbushLocation, 1.7f + (bAmbusher ? 0.26f : 0.0f), 0.0f, 0.86f, BHBotDecisionLabel(BotRole, Personality, bCanCapture ? TEXT("guard exit") : TEXT("monitor exit route")));
	}

	if (!bCanCapture && Now - LastTrapAttemptTime > 8.0f)
	{
		if (AActor* TrapTarget = FindHunterPatrolObjective())
		{
			AddCandidate(OutCandidates, EBHBotIntent::DropTrap, TrapTarget, TrapTarget->GetActorLocation(), 1.15f + (bMisdirectionMonitor ? 0.30f : 0.16f) + (bTrapMonitor ? 0.24f : 0.0f), 0.0f, 0.52f, BHBotDecisionLabel(BotRole, Personality, TEXT("trap route")));
		}
	}

	if (!bCanCapture && (OutCandidates.IsEmpty() || Personality == EBHBotPersonality::Suspicious || FMath::FRand() < 0.18f))
	{
		AddCandidate(OutCandidates, EBHBotIntent::Patrol, nullptr, GetStablePatrolPoint(BotCharacter, 4.0f, 1000.0f), 0.82f, 0.0f, 0.22f, BHBotDecisionLabel(BotRole, Personality, TEXT("roam corridor")));
	}
	else if (OutCandidates.IsEmpty())
	{
		AddCandidate(OutCandidates, EBHBotIntent::Patrol, nullptr, GetStablePatrolPoint(BotCharacter), bAggressiveTeacher ? 0.48f : 0.35f, 0.0f, 0.18f, BHBotDecisionLabel(BotRole, Personality, TEXT("hunter patrol")));
	}
}

FBHBotPolicyFeatures ABHBotController::BuildPolicyFeatures(ABHCharacter* BotCharacter, ABHPlayerState* BotPS, ABHGameState* BHGS, const TArray<FBHBotDecisionCandidate>& Candidates) const
{
	FBHBotPolicyFeatures Features;
	Features.Personality = Personality;
	Features.Difficulty = GetBHGameMode() ? GetBHGameMode()->GetBotDifficulty() : EBHBotDifficulty::Normal;
	Features.Role = BotPS ? BotPS->PlayerRole : EBHPlayerRole::Unassigned;
	Features.ThreatPressure = BotCharacter ? GetThreatPressureForLocation(BotCharacter->GetActorLocation()) : 0.0f;
	Features.ObjectivePressure = GetObjectivePressure(BHGS);
	Features.TimeRemainingRatio = BHGS ? FMath::Clamp(static_cast<float>(BHGS->RemainingTime) / 900.0f, 0.0f, 1.0f) : 1.0f;
	Features.NearbyAllyCount = BotCharacter && BotPS ? CountNearbyAllies(BotCharacter->GetActorLocation(), BotPS->PlayerRole, 1300.0f) : 0;
	if (!Candidates.IsEmpty())
	{
		const FBHBotDecisionCandidate& First = Candidates[0];
		Features.DistanceToTarget = First.Distance;
	}
	return Features;
}

FBHBotPolicyResult ABHBotController::ScoreDecisionCandidate(const FBHBotPolicyFeatures& Features, TArray<FBHBotDecisionCandidate>& Candidates) const
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UBHBotPolicySubsystem* Policy = GameInstance ? GameInstance->GetSubsystem<UBHBotPolicySubsystem>() : nullptr)
	{
		return Policy->ScoreCandidates(Features, Candidates);
	}

	FBHBotPolicyResult Result;
	float BestScore = -TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		Candidates[Index].BaseScore = ApplyDifficultyNoise(Candidates[Index].BaseScore
			+ Candidates[Index].Urgency * 0.35f
			- Candidates[Index].Risk * 0.45f
			- static_cast<float>(Candidates[Index].TargetClaimCount) * 0.45f);
		if (Candidates[Index].BaseScore > BestScore)
		{
			BestScore = Candidates[Index].BaseScore;
			Result.ChosenIndex = Index;
			Result.Score = BestScore;
			Result.DebugLabel = Candidates[Index].DebugLabel;
		}
	}
	return Result;
}

void ABHBotController::CommitDecision(ABHCharacter* BotCharacter, ABHPlayerState* BotPS, ABHGameState* BHGS, const FBHBotDecisionCandidate& Decision)
{
	if (!BotCharacter || !BotPS || !GetWorld())
	{
		return;
	}

	ABHGameMode* BHGM = GetBHGameMode();
	AActor* Target = Decision.Target.Get();
	const float Now = GetWorld()->GetTimeSeconds();

	if (BotCharacter->IsHiddenInLocker() && Decision.Intent != EBHBotIntent::Hide)
	{
		if (Now - LastHideExitTime > 1.0f)
		{
			LastHideExitTime = Now;
			BotCharacter->BotExitCurrentLocker();
		}
		return;
	}

	if (CurrentClaimTarget.IsValid() && CurrentClaimTarget.Get() != Target && BHGM)
	{
		BHGM->ReleaseBotObjective(this, CurrentClaimTarget.Get());
		CurrentClaimTarget = nullptr;
	}

	const bool bObjectiveIntent = Decision.Intent == EBHBotIntent::AnswerStation
		|| Decision.Intent == EBHBotIntent::WorkStation
		|| Decision.Intent == EBHBotIntent::RepairBreaker
		|| Decision.Intent == EBHBotIntent::Escape
		|| Decision.Intent == EBHBotIntent::Hide
		|| Decision.Intent == EBHBotIntent::SearchLocker
		|| Decision.Intent == EBHBotIntent::DropTrap;
	if (bObjectiveIntent && Target && BHGM)
	{
		if (!BHGM->ClaimBotObjective(this, Target, Decision.Intent, 9.0f + FMath::FRandRange(0.0f, 4.0f)))
		{
			ClearInteraction();
			MoveTowardLocation(GetStablePatrolPoint(BotCharacter, 4.0f, 850.0f), 220.0f);
			return;
		}
		CurrentClaimTarget = Target;
	}

	const EBHBotIntent PreviousIntent = CurrentIntent;
	CurrentIntent = Decision.Intent;
	LastDecisionTime = Now;
	++DecisionsMade;
	if (PreviousIntent != Decision.Intent)
	{
		++IntentSwitches;
	}
	LastDecisionScore = Decision.BaseScore;
	LastDecisionDebugLabel = Decision.DebugLabel;

	switch (Decision.Intent)
	{
	case EBHBotIntent::AnswerStation:
	case EBHBotIntent::WorkStation:
	case EBHBotIntent::RepairBreaker:
	case EBHBotIntent::Escape:
		HandleSurvivorObjective(BotCharacter, Target, BHGS);
		break;
	case EBHBotIntent::Hide:
		if (!Target)
		{
			StopMovement();
			ClearInteraction();
			break;
		}
		if (IsCloseTo(Target, 260.0f))
		{
			HoldInteraction(Target);
		}
		else
		{
			const float HideDecoyCooldown = Personality == EBHBotPersonality::Trickster ? 6.0f : (Personality == EBHBotPersonality::Cautious ? 12.0f : 9.0f);
			if (Now - LastTrapAttemptTime > HideDecoyCooldown)
			{
				LastTrapAttemptTime = Now;
				BotCharacter->BotDropDecoyOrTrap();
			}
			MoveTowardActor(Target, 170.0f);
		}
		break;
	case EBHBotIntent::Flee:
	case EBHBotIntent::Bait:
	{
		ClearInteraction();
		if (Decision.Intent == EBHBotIntent::Bait)
		{
			LastBaitAttemptTime = Now;
		}
		const float DecoyCooldown = Decision.Intent == EBHBotIntent::Bait
			? (Personality == EBHBotPersonality::Trickster ? 3.0f : 4.0f)
			: (Personality == EBHBotPersonality::Panicked ? 6.0f : 8.0f);
		if (Now - LastTrapAttemptTime > DecoyCooldown)
		{
			LastTrapAttemptTime = Now;
			BotCharacter->BotDropDecoyOrTrap();
		}
		MoveTowardLocation(Decision.Location, 170.0f);
		break;
	}
	case EBHBotIntent::Chase:
		ClearInteraction();
		if (Target && IsCloseTo(Target, 245.0f) && BotPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			BotCharacter->BotTryCapture();
		}
		if (Target)
		{
			MoveTowardActor(Target, 145.0f);
		}
		if (Now - LastPowerAttemptTime > (Personality == EBHBotPersonality::Aggressive ? 3.1f : 4.0f))
		{
			LastPowerAttemptTime = Now;
			BotCharacter->BotUseHunterPower();
		}
		break;
	case EBHBotIntent::InvestigateNoise:
	case EBHBotIntent::InvestigateLastSeen:
		ClearInteraction();
		MoveTowardLocation(Decision.Location, 190.0f);
		if (Now - LastPowerAttemptTime > 7.0f)
		{
			LastPowerAttemptTime = Now;
			BotCharacter->BotUseScan();
		}
		break;
	case EBHBotIntent::SearchLocker:
		ClearInteraction();
		if (Target)
		{
			MoveTowardActor(Target, 230.0f);
		}
		break;
	case EBHBotIntent::AmbushObjective:
		ClearInteraction();
		if (!Decision.Location.IsNearlyZero() && (!Target || FVector::DistSquared2D(Decision.Location, Target->GetActorLocation()) > FMath::Square(240.0f)))
		{
			MoveTowardLocation(Decision.Location, 260.0f);
		}
		else if (Target)
		{
			MoveTowardActor(Target, 300.0f);
		}
		else
		{
			MoveTowardLocation(Decision.Location, 260.0f);
		}
		if (BotPS->PlayerRole == EBHPlayerRole::FakeHunter && Now - LastTrapAttemptTime > (Personality == EBHBotPersonality::Ambusher ? 5.5f : (Personality == EBHBotPersonality::Trickster ? 6.0f : 9.0f)))
		{
			LastTrapAttemptTime = Now;
			BotCharacter->BotDropDecoyOrTrap();
		}
		else if (BotPS->PlayerRole == EBHPlayerRole::Hunter && Now - LastPowerAttemptTime > 9.0f)
		{
			LastPowerAttemptTime = Now;
			BotCharacter->BotUseScan();
			BotCharacter->BotUseHunterPower();
		}
		break;
	case EBHBotIntent::UseScan:
		ClearInteraction();
		if (Now - LastPowerAttemptTime > (BotPS->PlayerRole == EBHPlayerRole::FakeHunter ? 5.5f : 4.0f))
		{
			LastPowerAttemptTime = Now;
			BotCharacter->BotUseScan();
		}
		MoveTowardLocation(Decision.Location, 220.0f);
		break;
	case EBHBotIntent::UsePower:
	{
		ClearInteraction();
		const float PowerCooldown = BotPS->PlayerRole == EBHPlayerRole::Hunter
			? (Personality == EBHBotPersonality::Aggressive ? 3.0f : 4.0f)
			: (Personality == EBHBotPersonality::Trickster ? 5.0f : 6.5f);
		const bool bPowerReady = Now - LastPowerAttemptTime > PowerCooldown;
		if (Target)
		{
			if (bPowerReady)
			{
				LastPowerAttemptTime = Now;
				BotCharacter->BotUseHunterPower();
			}
			MoveTowardActor(Target, BotPS->PlayerRole == EBHPlayerRole::Hunter ? 145.0f : 240.0f);
		}
		else if (!Decision.Location.IsNearlyZero())
		{
			if (bPowerReady && FVector::DistSquared2D(BotCharacter->GetActorLocation(), Decision.Location) <= FMath::Square(560.0f))
			{
				LastPowerAttemptTime = Now;
				BotCharacter->BotUseHunterPower();
			}
			MoveTowardLocation(Decision.Location, 240.0f);
		}
		else
		{
			if (bPowerReady)
			{
				LastPowerAttemptTime = Now;
				BotCharacter->BotUseHunterPower();
			}
		}
		break;
	}
	case EBHBotIntent::DropTrap:
		ClearInteraction();
		if (Now - LastTrapAttemptTime > (BotPS->PlayerRole == EBHPlayerRole::FakeHunter ? (Personality == EBHBotPersonality::Ambusher ? 5.5f : 7.5f) : 4.0f))
		{
			LastTrapAttemptTime = Now;
			BotCharacter->BotDropDecoyOrTrap();
		}
		if (Target)
		{
			MoveTowardActor(Target, 240.0f);
		}
		else
		{
			MoveTowardLocation(Decision.Location, 240.0f);
		}
		break;
	case EBHBotIntent::Patrol:
	default:
		ClearInteraction();
		MoveTowardLocation(GetStablePatrolPoint(BotCharacter), 220.0f);
		break;
	}
}

void ABHBotController::AddCandidate(TArray<FBHBotDecisionCandidate>& Candidates, EBHBotIntent Intent, AActor* Target, const FVector& Location, float BaseScore, float Risk, float Urgency, const FString& DebugLabel) const
{
	const APawn* ControlledPawn = GetPawn();
	ABHGameMode* BHGM = GetBHGameMode();
	if (BHGM && Target && BHGM->IsBotTargetOnCooldown(this, Target))
	{
		return;
	}

	FBHBotDecisionCandidate Candidate;
	Candidate.Intent = Intent;
	Candidate.Target = Target;
	Candidate.Location = Location;
	Candidate.Risk = FMath::Clamp(Risk, 0.0f, 2.0f);
	Candidate.Urgency = FMath::Clamp(Urgency, 0.0f, 2.0f);
	Candidate.Distance = ControlledPawn ? FVector::Dist2D(ControlledPawn->GetActorLocation(), Target ? Target->GetActorLocation() : Location) : 0.0f;
	Candidate.BaseScore = BaseScore - FMath::Clamp(Candidate.Distance / 10000.0f, 0.0f, 0.7f);
	if (BHGM && Target)
	{
		int32 ClaimCount = BHGM->CountBotClaimsForTarget(Target);
		if (CurrentClaimTarget.Get() == Target)
		{
			ClaimCount = FMath::Max(0, ClaimCount - 1);
		}
		Candidate.TargetClaimCount = ClaimCount;
	}
	if (Target)
	{
		const ABHPlayerState* BotPS = GetBHPlayerState();
		if (BotPS && BotPS->PlayerRole == EBHPlayerRole::Survivor && (Intent == EBHBotIntent::AnswerStation || Intent == EBHBotIntent::WorkStation || Intent == EBHBotIntent::RepairBreaker))
		{
			Candidate.BaseScore -= FMath::Min(0.65f, static_cast<float>(CountNearbyAllies(Target->GetActorLocation(), EBHPlayerRole::Survivor, 950.0f)) * 0.22f);
		}
	}
	if (Intent == EBHBotIntent::Hide && Personality == EBHBotPersonality::Panicked)
	{
		Candidate.BaseScore += 0.25f;
	}
	switch (Personality)
	{
	case EBHBotPersonality::Cautious:
		Candidate.BaseScore += (Intent == EBHBotIntent::Hide || Intent == EBHBotIntent::Flee) ? 0.22f : 0.0f;
		Candidate.BaseScore -= Candidate.Risk * 0.14f;
		break;
	case EBHBotPersonality::Objective:
		Candidate.BaseScore += (Intent == EBHBotIntent::AnswerStation || Intent == EBHBotIntent::WorkStation || Intent == EBHBotIntent::RepairBreaker || Intent == EBHBotIntent::Escape) ? 0.28f : 0.0f;
		break;
	case EBHBotPersonality::Bold:
		Candidate.BaseScore += (Intent == EBHBotIntent::Bait || Intent == EBHBotIntent::RepairBreaker || Intent == EBHBotIntent::Flee) ? 0.18f : 0.0f;
		break;
	case EBHBotPersonality::Trickster:
		Candidate.BaseScore += (Intent == EBHBotIntent::Bait || Intent == EBHBotIntent::DropTrap || Intent == EBHBotIntent::Patrol) ? 0.24f : 0.0f;
		break;
	case EBHBotPersonality::Aggressive:
		Candidate.BaseScore += (Intent == EBHBotIntent::Chase || Intent == EBHBotIntent::UsePower || Intent == EBHBotIntent::InvestigateLastSeen) ? 0.24f : 0.0f;
		break;
	case EBHBotPersonality::Suspicious:
		Candidate.BaseScore += (Intent == EBHBotIntent::InvestigateNoise || Intent == EBHBotIntent::SearchLocker || Intent == EBHBotIntent::Patrol) ? 0.20f : 0.0f;
		break;
	case EBHBotPersonality::Ambusher:
		Candidate.BaseScore += (Intent == EBHBotIntent::AmbushObjective || Intent == EBHBotIntent::DropTrap) ? 0.24f : 0.0f;
		break;
	case EBHBotPersonality::Panicked:
	default:
		break;
	}
	Candidate.BaseScore = ApplyDifficultyNoise(Candidate.BaseScore);
	Candidate.DebugLabel = DebugLabel;
	Candidates.Add(Candidate);
}

void ABHBotController::RecordStimulus(EBHBotStimulusType Type, AActor* TargetActor, const FVector& Location, const FString& Reason, float Strength)
{
	if (ABHGameMode* BHGM = GetBHGameMode())
	{
		BHGM->ReportBotStimulus(Type, Location, GetPawn(), TargetActor, Reason, Strength);
	}
}

EBHBotPersonality ABHBotController::ChoosePersonality(ABHPlayerState* BotPS) const
{
	const uint32 Seed = GetTypeHash(GetName()) ^ static_cast<uint32>(BotPS ? FMath::Max(0, BotPS->GetPlayerId()) : 0);
	const EBHPlayerRole BotRole = BotPS ? BotPS->PlayerRole : EBHPlayerRole::Unassigned;
	if (BotRole == EBHPlayerRole::Hunter)
	{
		static const EBHBotPersonality HunterPersonalities[] = {
			EBHBotPersonality::Aggressive,
			EBHBotPersonality::Suspicious,
			EBHBotPersonality::Ambusher,
			EBHBotPersonality::Aggressive
		};
		return HunterPersonalities[Seed % UE_ARRAY_COUNT(HunterPersonalities)];
	}
	if (BotRole == EBHPlayerRole::FakeHunter)
	{
		static const EBHBotPersonality MonitorPersonalities[] = {
			EBHBotPersonality::Trickster,
			EBHBotPersonality::Suspicious,
			EBHBotPersonality::Ambusher
		};
		return MonitorPersonalities[Seed % UE_ARRAY_COUNT(MonitorPersonalities)];
	}

	static const EBHBotPersonality SurvivorPersonalities[] = {
		EBHBotPersonality::Cautious,
		EBHBotPersonality::Objective,
		EBHBotPersonality::Bold,
		EBHBotPersonality::Trickster,
		EBHBotPersonality::Panicked,
		EBHBotPersonality::Objective
	};
	return SurvivorPersonalities[Seed % UE_ARRAY_COUNT(SurvivorPersonalities)];
}

float ABHBotController::ApplyDifficultyNoise(float Score) const
{
	const ABHGameMode* BHGM = GetBHGameMode();
	const EBHBotDifficulty Difficulty = BHGM ? BHGM->GetBotDifficulty() : EBHBotDifficulty::Normal;
	switch (Difficulty)
	{
	case EBHBotDifficulty::Easy:
		return Score + FMath::FRandRange(-0.16f, 0.12f);
	case EBHBotDifficulty::Hard:
		return Score + FMath::FRandRange(-0.035f, 0.055f);
	case EBHBotDifficulty::Normal:
	default:
		return Score + FMath::FRandRange(-0.075f, 0.075f);
	}
}

FVector ABHBotController::GetStablePatrolPoint(const ABHCharacter* BotCharacter, float HoldSeconds, float MinDistance)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const FVector BotLocation = BotCharacter ? BotCharacter->GetActorLocation() : (GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector);
	const float MinDistanceSq = FMath::Square(FMath::Max(120.0f, MinDistance));
	if (bHasPatrolDestination
		&& Now < CurrentPatrolDestinationExpireTime
		&& FVector::DistSquared2D(BotLocation, CurrentPatrolDestination) > MinDistanceSq)
	{
		return CurrentPatrolDestination;
	}

	ABHGameMode* BHGM = GetBHGameMode();
	FVector BestLocation = BotLocation;
	float BestDistanceSq = -1.0f;
	for (int32 Attempt = 0; Attempt < 10; ++Attempt)
	{
		const FVector Candidate = BHGM ? BHGM->GetRandomBotPatrolPoint() : (BotLocation + FMath::VRand() * FMath::FRandRange(850.0f, 1500.0f));
		FVector Projected = FVector::ZeroVector;
		if (!ProjectUsableNavLocation(Candidate, Projected, nullptr))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(BotLocation, Projected);
		if (DistanceSq > BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestLocation = Projected;
			if (DistanceSq >= MinDistanceSq)
			{
				break;
			}
		}
	}

	CurrentPatrolDestination = BestLocation;
	CurrentPatrolDestinationExpireTime = Now + FMath::Max(1.5f, HoldSeconds) + FMath::FRandRange(0.0f, 2.0f);
	bHasPatrolDestination = true;
	return CurrentPatrolDestination;
}

bool ABHBotController::MoveTowardActor(AActor* Target, float AcceptanceRadius)
{
	FVector MoveLocation = FVector::ZeroVector;
	if (!Target)
	{
		return false;
	}
	if (const ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target))
	{
		if (!Station->IsDirectorActive() || Station->IsCompleted())
		{
			return false;
		}
	}
	else if (const ABHBreaker* Breaker = Cast<ABHBreaker>(Target))
	{
		if (!Breaker->IsDirectorActive() || Breaker->IsRepaired())
		{
			return false;
		}
	}
	else if (const ABHExitGate* Exit = Cast<ABHExitGate>(Target))
	{
		if (!Exit->IsDirectorActive())
		{
			return false;
		}
	}

	const bool bUseApproachPoint = !Cast<ABHCharacter>(Target);
	if (bUseApproachPoint && GetBHGameMode() && GetBHGameMode()->GetBotApproachPoint(Target, GetPawn() ? GetPawn()->GetActorLocation() : Target->GetActorLocation(), AcceptanceRadius + 220.0f, MoveLocation))
	{
	}
	else if (!ProjectUsableNavLocation(Target->GetActorLocation(), MoveLocation, Target))
	{
		if (ABHGameMode* BHGM = GetBHGameMode())
		{
			BHGM->AddBotTargetCooldown(this, Target, 18.0f, TEXT("target off navmesh"));
		}
		return false;
	}

	ClearInteraction();
	const EPathFollowingRequestResult::Type Result = MoveToLocation(MoveLocation, AcceptanceRadius, true, true, true, false, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		LogUnreachableOnce(Target, TEXT("MoveToActor failed"));
		if (ABHGameMode* BHGM = GetBHGameMode())
		{
			BHGM->AddBotTargetCooldown(this, Target, 20.0f, TEXT("move failed"));
		}
		return false;
	}

	NoteMovementProgress();
	if (Result == EPathFollowingRequestResult::AlreadyAtGoal || (GetPawn() && FVector::DistSquared2D(GetPawn()->GetActorLocation(), MoveLocation) <= FMath::Square(AcceptanceRadius + 140.0f)))
	{
		LastProgressLocation = GetPawn() ? GetPawn()->GetActorLocation() : LastProgressLocation;
		LastProgressTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastProgressTime;
		return true;
	}
	if (IsCloseTo(Target, 620.0f))
	{
		LastProgressLocation = GetPawn() ? GetPawn()->GetActorLocation() : LastProgressLocation;
		LastProgressTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastProgressTime;
		return true;
	}
	if (HandleStuck(Target))
	{
		if (ABHGameMode* BHGM = GetBHGameMode())
		{
			BHGM->AddBotTargetCooldown(this, Target, 20.0f, TEXT("stuck"));
		}
		return false;
	}
	return true;
}

bool ABHBotController::MoveTowardLocation(const FVector& Location, float AcceptanceRadius)
{
	FVector MoveLocation = FVector::ZeroVector;
	if (!ProjectUsableNavLocation(Location, MoveLocation, nullptr))
	{
		return false;
	}

	ClearInteraction();
	const EPathFollowingRequestResult::Type Result = MoveToLocation(MoveLocation, AcceptanceRadius, true, true, true, false, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		return false;
	}

	NoteMovementProgress();
	if (Result == EPathFollowingRequestResult::AlreadyAtGoal || (GetPawn() && FVector::DistSquared2D(GetPawn()->GetActorLocation(), MoveLocation) <= FMath::Square(AcceptanceRadius + 140.0f)))
	{
		LastProgressLocation = GetPawn() ? GetPawn()->GetActorLocation() : LastProgressLocation;
		LastProgressTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastProgressTime;
		return true;
	}
	HandleStuck(nullptr);
	return true;
}

void ABHBotController::ClearInteraction()
{
	if (ABHCharacter* BotCharacter = GetBHCharacter())
	{
		if (AActor* Target = CurrentInteractTarget.Get())
		{
			BotCharacter->BotEndInteract(Target);
		}
	}
	CurrentInteractTarget = nullptr;
}

bool ABHBotController::HoldInteraction(AActor* Target)
{
	ABHCharacter* BotCharacter = GetBHCharacter();
	if (!BotCharacter || !Target)
	{
		return false;
	}

	StopMovement();
	if (CurrentInteractTarget.Get() != Target)
	{
		ClearInteraction();
		if (!BotCharacter->BotBeginInteract(Target))
		{
			return false;
		}
		CurrentInteractTarget = Target;
	}
	return true;
}

bool ABHBotController::IsCloseTo(const AActor* Target, float Distance) const
{
	const APawn* ControlledPawn = GetPawn();
	return ControlledPawn && Target && FVector::DistSquared(ControlledPawn->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(Distance);
}

bool ABHBotController::HasUsableNavLocation(const FVector& Location, AActor* LogTarget)
{
	FVector ProjectedLocation = FVector::ZeroVector;
	return ProjectUsableNavLocation(Location, ProjectedLocation, LogTarget);
}

bool ABHBotController::ProjectUsableNavLocation(const FVector& Location, FVector& OutLocation, AActor* LogTarget)
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		OutLocation = Location;
		return true;
	}

	FNavLocation Projected;
	const bool bProjected = NavSystem->ProjectPointToNavigation(Location, Projected, FVector(220.0f, 220.0f, 260.0f));
	if (!bProjected && LogTarget)
	{
		LogUnreachableOnce(LogTarget, TEXT("target is off navmesh"));
	}
	if (bProjected)
	{
		OutLocation = Projected.Location;
	}
	return bProjected;
}

void ABHBotController::NoteMovementProgress()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !GetWorld())
	{
		return;
	}

	const FVector Location = ControlledPawn->GetActorLocation();
	if (FVector::DistSquared2D(Location, LastProgressLocation) > FMath::Square(80.0f))
	{
		LastProgressLocation = Location;
		LastProgressTime = GetWorld()->GetTimeSeconds();
	}
}

bool ABHBotController::HandleStuck(AActor* GoalActor)
{
	if (!GetWorld() || !GetPawn())
	{
		return false;
	}

	if (GetWorld()->GetTimeSeconds() - LastProgressTime < StuckSeconds)
	{
		return false;
	}

	if (!GoalActor)
	{
		StopMovement();
		LastProgressLocation = GetPawn()->GetActorLocation();
		LastProgressTime = GetWorld()->GetTimeSeconds();
		bHasPatrolDestination = false;
		return false;
	}

	LogUnreachableOnce(GoalActor, TEXT("bot abandoned a stuck move"));
	StopMovement();
	LastProgressLocation = GetPawn()->GetActorLocation();
	LastProgressTime = GetWorld()->GetTimeSeconds();
	LastUnreachableTarget = GoalActor;
	return true;
}

void ABHBotController::LogUnreachableOnce(AActor* Target, const FString& Reason)
{
	if (Target && LastUnreachableTarget.Get() == Target)
	{
		return;
	}

	LastUnreachableTarget = Target;
	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot path note: %s %s%s%s"),
		*GetNameSafe(this),
		*Reason,
		Target ? TEXT(": ") : TEXT(""),
		Target ? *GetNameSafe(Target) : TEXT(""));
}

ABHCharacter* ABHBotController::FindVisibleSurvivor(float Range, bool bIncludeHidden) const
{
	ABHCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Candidate = *It;
		const ABHPlayerState* PS = Candidate ? Candidate->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Candidate || !PS || !PS->IsAliveSurvivor() || !CanSeeCharacter(Candidate, Range, bIncludeHidden))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), GetPawn()->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	return Best;
}

ABHCharacter* ABHBotController::FindVisibleTeacherThreat(float Range) const
{
	ABHCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Candidate = *It;
		const ABHPlayerState* PS = Candidate ? Candidate->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Candidate || !PS || !PS->IsAliveHunter() || !CanSeeCharacter(Candidate, Range, false))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), GetPawn()->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	return Best;
}

bool ABHBotController::CanSeeCharacter(const ABHCharacter* Other, float Range, bool bIncludeHidden) const
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !Other || (!bIncludeHidden && Other->IsHiddenInLocker()))
	{
		return false;
	}

	const FVector ToTarget = Other->GetActorLocation() - ControlledPawn->GetActorLocation();
	const float SightProfileMultiplier = FMath::Clamp(Other->GetMovementSightProfileMultiplier(), 0.25f, 1.0f);
	if (ToTarget.SizeSquared() > FMath::Square(Range * SightProfileMultiplier))
	{
		return false;
	}

	const float Dot = FVector::DotProduct(ControlledPawn->GetActorForwardVector(), ToTarget.GetSafeNormal());
	const float RequiredFacingDot = FMath::Lerp(0.34f, 0.18f, SightProfileMultiplier);
	if (Dot < RequiredFacingDot)
	{
		return false;
	}

	return LineOfSightTo(Other);
}

ABHObjectiveStation* ABHBotController::FindActiveStation(bool bNeedUnsolvedQuestion) const
{
	ABHObjectiveStation* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}

	for (TActorIterator<ABHObjectiveStation> It(GetWorld()); It; ++It)
	{
		ABHObjectiveStation* Station = *It;
		if (!Station || !Station->IsDirectorActive() || Station->IsCompleted())
		{
			continue;
		}
		if (bNeedUnsolvedQuestion && Station->IsQuestionSolved())
		{
			continue;
		}
		if (!bNeedUnsolvedQuestion && !Station->IsQuestionSolved())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), Station->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Station;
		}
	}
	return Best;
}

ABHBreaker* ABHBotController::FindActiveBreaker() const
{
	ABHBreaker* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}

	for (TActorIterator<ABHBreaker> It(GetWorld()); It; ++It)
	{
		ABHBreaker* Breaker = *It;
		if (!Breaker || !Breaker->IsDirectorActive() || Breaker->IsRepaired())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), Breaker->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Breaker;
		}
	}
	return Best;
}

ABHExitGate* ABHBotController::FindActiveExit() const
{
	const ABHGameState* BHGS = GetBHGameState();
	if (!BHGS || !BHGS->bExitUnlocked)
	{
		return nullptr;
	}

	ABHExitGate* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const APawn* ControlledPawn = GetPawn();
	for (TActorIterator<ABHExitGate> It(GetWorld()); It; ++It)
	{
		ABHExitGate* Exit = *It;
		if (!Exit || !Exit->IsDirectorActive())
		{
			continue;
		}

		const float DistSq = ControlledPawn ? FVector::DistSquared(ControlledPawn->GetActorLocation(), Exit->GetActorLocation()) : 0.0f;
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Exit;
		}
	}
	return Best;
}

ABHLocker* ABHBotController::FindNearestLocker(bool bRequireEmpty, bool bRequireOccupied, float Range) const
{
	ABHLocker* Best = nullptr;
	float BestDistSq = FMath::Square(Range);
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}

	for (TActorIterator<ABHLocker> It(GetWorld()); It; ++It)
	{
		ABHLocker* Locker = *It;
		if (!Locker)
		{
			continue;
		}
		const bool bOccupied = Locker->GetOccupant() != nullptr;
		if ((bRequireEmpty && bOccupied) || (bRequireOccupied && !bOccupied))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), Locker->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Locker;
		}
	}
	return Best;
}

ABHLocker* ABHBotController::FindSuspiciousLocker(const FVector& NearLocation, float Range, bool bAllowEmptySuspicion) const
{
	ABHLocker* Best = nullptr;
	float BestDistSq = FMath::Square(Range);
	const bool bHasRecentKnownLocation = GetWorld() && GetWorld()->GetTimeSeconds() - LastKnownSurvivorTime <= HearingMemorySeconds;
	for (TActorIterator<ABHLocker> It(GetWorld()); It; ++It)
	{
		ABHLocker* Locker = *It;
		if (!Locker)
		{
			continue;
		}

		const bool bOccupied = Locker->GetOccupant() != nullptr;
		if (!bOccupied && !bAllowEmptySuspicion)
		{
			continue;
		}
		if (bHasRecentKnownLocation && FVector::DistSquared(Locker->GetActorLocation(), NearLocation) > FMath::Square(Range))
		{
			continue;
		}

		const float DistSq = GetPawn() ? FVector::DistSquared(GetPawn()->GetActorLocation(), Locker->GetActorLocation()) : 0.0f;
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Locker;
		}
	}
	return Best;
}

AActor* ABHBotController::FindSurvivorObjective() const
{
	if (ABHObjectiveStation* Station = FindActiveStation(true))
	{
		return Station;
	}
	if (ABHObjectiveStation* Station = FindActiveStation(false))
	{
		return Station;
	}
	if (ABHBreaker* Breaker = FindActiveBreaker())
	{
		return Breaker;
	}
	return FindActiveExit();
}

AActor* ABHBotController::FindHunterPatrolObjective() const
{
	if (ABHObjectiveStation* Station = FindActiveStation(true))
	{
		return Station;
	}
	if (ABHBreaker* Breaker = FindActiveBreaker())
	{
		return Breaker;
	}
	if (ABHExitGate* Exit = FindActiveExit())
	{
		return Exit;
	}
	return nullptr;
}

float ABHBotController::GetObjectivePressure(const ABHGameState* BHGS) const
{
	if (!BHGS)
	{
		return 0.0f;
	}

	if (BHGS->bExitUnlocked)
	{
		return 1.0f;
	}

	const int32 RemainingBreakers = FMath::Max(0, BHGS->BreakersRequired - BHGS->BreakersCompleted);
	const int32 RemainingSideObjectives = FMath::Max(0, BHGS->SideObjectivesRequired - BHGS->SideObjectivesCompleted);
	const int32 TotalRemaining = RemainingBreakers + RemainingSideObjectives;
	const float TimePressure = 1.0f - FMath::Clamp(static_cast<float>(BHGS->RemainingTime) / 900.0f, 0.0f, 1.0f);
	return FMath::Clamp(0.25f + TimePressure * 0.45f + (TotalRemaining <= 2 ? 0.30f : 0.0f), 0.0f, 1.0f);
}

float ABHBotController::GetThreatPressureForLocation(const FVector& Location) const
{
	float Pressure = 0.0f;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		const ABHCharacter* Candidate = *It;
		const ABHPlayerState* PS = Candidate ? Candidate->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Candidate || !PS || !PS->IsAliveHunter())
		{
			continue;
		}

		const float Dist = FVector::Dist2D(Location, Candidate->GetActorLocation());
		if (Dist <= SightRange * 1.15f)
		{
			Pressure = FMath::Max(Pressure, 1.0f - Dist / (SightRange * 1.15f));
		}
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (LocalMemory.LastSeenHunterTime > -1000.0f && Now - LocalMemory.LastSeenHunterTime < HearingMemorySeconds)
	{
		const float Dist = FVector::Dist2D(Location, LocalMemory.LastSeenHunterLocation);
		Pressure = FMath::Max(Pressure, FMath::Clamp(1.0f - Dist / 2400.0f, 0.0f, 1.0f) * 0.75f);
	}
	return Pressure;
}

int32 ABHBotController::CountNearbyAllies(const FVector& Location, EBHPlayerRole AllyRole, float Range) const
{
	int32 Count = 0;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		const ABHCharacter* Candidate = *It;
		const ABHPlayerState* PS = Candidate ? Candidate->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Candidate || Candidate == GetPawn() || !PS || PS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}
		if (PS->PlayerRole != AllyRole)
		{
			continue;
		}
		if (FVector::DistSquared2D(Location, Candidate->GetActorLocation()) <= FMath::Square(Range))
		{
			++Count;
		}
	}
	return Count;
}

bool ABHBotController::ShouldStayHidden(const ABHCharacter* BotCharacter, const ABHCharacter* Threat, const ABHGameState* BHGS) const
{
	if (!BotCharacter)
	{
		return false;
	}

	const float ObjectivePressure = GetObjectivePressure(BHGS);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Threat && FVector::DistSquared2D(Threat->GetActorLocation(), BotCharacter->GetActorLocation()) < FMath::Square(1800.0f))
	{
		return true;
	}
	if (Now - LocalMemory.LastSeenHunterTime < 4.5f && ObjectivePressure < 0.85f)
	{
		return true;
	}
	if (Personality == EBHBotPersonality::Panicked && ObjectivePressure < 0.92f && Now - LastHideExitTime < 8.0f)
	{
		return true;
	}
	if (Personality == EBHBotPersonality::Cautious && ObjectivePressure < 0.70f && Now - LastHideExitTime < 5.0f)
	{
		return true;
	}
	if (Personality == EBHBotPersonality::Objective && ObjectivePressure > 0.58f && Now - LocalMemory.LastSeenHunterTime > 3.0f)
	{
		return false;
	}
	return false;
}

FVector ABHBotController::BuildAmbushLocation(const FVector& SeenLocation, const FVector& ObjectiveLocation) const
{
	const FVector ToObjective = ObjectiveLocation - SeenLocation;
	if (ToObjective.SizeSquared2D() < FMath::Square(600.0f))
	{
		return ObjectiveLocation;
	}

	const FVector Direction = ToObjective.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal2D();
	const float InterceptRatio = Personality == EBHBotPersonality::Ambusher ? 0.58f : (Personality == EBHBotPersonality::Suspicious ? 0.44f : 0.50f);
	const float SideOffset = Personality == EBHBotPersonality::Ambusher ? 520.0f : 320.0f;
	const float Sign = (FMath::RandBool() ? 1.0f : -1.0f);
	return SeenLocation + ToObjective * InterceptRatio + Side * SideOffset * Sign;
}

void ABHBotController::HandleSurvivorThreat(ABHCharacter* BotCharacter, ABHCharacter* Threat)
{
	if (ABHLocker* Locker = FindNearestLocker(true, false, 950.0f))
	{
		if (IsCloseTo(Locker, 260.0f))
		{
			HoldInteraction(Locker);
			return;
		}

		BotCharacter->BotDropDecoyOrTrap();
		MoveTowardActor(Locker, 180.0f);
		return;
	}

	ClearInteraction();
	const FVector Away = (BotCharacter->GetActorLocation() - Threat->GetActorLocation()).GetSafeNormal();
	BotCharacter->BotDropDecoyOrTrap();
	MoveTowardLocation(BotCharacter->GetActorLocation() + Away * 900.0f, 160.0f);
}

void ABHBotController::HandleSurvivorObjective(ABHCharacter* BotCharacter, AActor* Target, ABHGameState* BHGS)
{
	if (!Target)
	{
		return;
	}

	if (!IsCloseTo(Target, 520.0f))
	{
		MoveTowardActor(Target, 180.0f);
		return;
	}

	if (ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target))
	{
		if (!Station->IsQuestionSolved())
		{
			HandleStationAnswer(BotCharacter, Station);
			return;
		}
		if (Station->IsCompleted())
		{
			ClearInteraction();
			return;
		}
		HoldInteraction(Station);
		return;
	}

	if (ABHBreaker* Breaker = Cast<ABHBreaker>(Target))
	{
		if (Breaker->IsRepaired())
		{
			ClearInteraction();
			return;
		}
		HoldInteraction(Breaker);
		return;
	}

	if (ABHExitGate* Exit = Cast<ABHExitGate>(Target))
	{
		HoldInteraction(Exit);
		return;
	}
}

void ABHBotController::HandleStationAnswer(ABHCharacter* BotCharacter, ABHObjectiveStation* Station)
{
	if (!BotCharacter || !Station || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	float AnswerDelay = 1.25f;
	const ABHGameMode* BHGM = GetBHGameMode();
	const EBHBotDifficulty Difficulty = BHGM ? BHGM->GetBotDifficulty() : EBHBotDifficulty::Normal;
	if (Difficulty == EBHBotDifficulty::Easy)
	{
		AnswerDelay = 1.85f;
	}
	else if (Difficulty == EBHBotDifficulty::Hard)
	{
		AnswerDelay = 0.72f;
	}
	if (Personality == EBHBotPersonality::Panicked)
	{
		AnswerDelay += 0.45f;
	}
	else if (Personality == EBHBotPersonality::Cautious)
	{
		AnswerDelay += 0.18f;
	}
	else if (Personality == EBHBotPersonality::Objective)
	{
		AnswerDelay -= 0.30f;
	}

	if (Now - LastAnswerAttemptTime < AnswerDelay)
	{
		return;
	}

	LastAnswerAttemptTime = Now;
	BotCharacter->BotSubmitAnswer(Station, ChooseAnswerIndex(Station));
}

float ABHBotController::GetCorrectAnswerChance() const
{
	const ABHGameMode* BHGM = GetBHGameMode();
	const EBHBotDifficulty Difficulty = BHGM ? BHGM->GetBotDifficulty() : EBHBotDifficulty::Normal;
	switch (Difficulty)
	{
	case EBHBotDifficulty::Easy:
		return Personality == EBHBotPersonality::Panicked ? 0.56f : (Personality == EBHBotPersonality::Objective ? 0.72f : 0.65f);
	case EBHBotDifficulty::Hard:
		return Personality == EBHBotPersonality::Objective ? 0.99f : (Personality == EBHBotPersonality::Panicked ? 0.92f : 0.97f);
	case EBHBotDifficulty::Normal:
	default:
		return Personality == EBHBotPersonality::Panicked ? 0.78f : (Personality == EBHBotPersonality::Objective ? 0.93f : (Personality == EBHBotPersonality::Cautious ? 0.82f : 0.85f));
	}
}

int32 ABHBotController::ChooseAnswerIndex(ABHObjectiveStation* Station) const
{
	if (!Station)
	{
		return 0;
	}

	const int32 ChoiceCount = FMath::Max(1, Station->GetQuestionChoiceCount());
	const int32 CorrectIndex = FMath::Clamp(Station->GetCorrectAnswerIndexForBot(), 0, ChoiceCount - 1);
	if (FMath::FRand() <= GetCorrectAnswerChance() || ChoiceCount <= 1)
	{
		return CorrectIndex;
	}

	int32 WrongIndex = FMath::RandRange(0, ChoiceCount - 1);
	if (WrongIndex == CorrectIndex)
	{
		WrongIndex = (WrongIndex + 1) % ChoiceCount;
	}
	return WrongIndex;
}
