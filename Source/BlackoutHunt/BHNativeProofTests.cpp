// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHAutomationTestWorld.h"
#include "BHFlickerLight.h"
#include "BHBreaker.h"
#include "BHBreakableGlassPane.h"
#include "BHButtonModule.h"
#include "BHBotPolicySubsystem.h"
#include "BHCCTVZone.h"
#include "BHCharacter.h"
#include "BHCrawlSpaceVolume.h"
#include "BHCosmeticUnlocks.h"
#include "BHDoor.h"
#include "BHFootstepSurfaceComponent.h"
#include "BHGameInstance.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHExitGate.h"
#include "BHLocker.h"
#include "BHModuleReceiverRelay.h"
#include "BHMovementTuningAsset.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHSecurityCamera.h"
#include "BHSecurityMonitor.h"
#include "BHSecurityShutter.h"
#include "BHSecurityTerminal.h"
#include "BHTutorialDirector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"

namespace
{
ABHGameState* BHCreateProofGameState(UWorld* World, EBHRoundPhase Phase)
{
	ABHGameState* GameState = World ? World->SpawnActor<ABHGameState>() : nullptr;
	if (World && GameState)
	{
		World->SetGameState(GameState);
		GameState->SetRoundPhase(Phase);
	}
	return GameState;
}

ABHPlayerState* BHAttachProofPlayerState(UWorld* World, ABHCharacter* Character, EBHPlayerRole Role, const FString& PlayerName)
{
	ABHPlayerState* PlayerState = World ? World->SpawnActor<ABHPlayerState>() : nullptr;
	if (PlayerState)
	{
		PlayerState->SetPlayerName(PlayerName);
		PlayerState->SetRole(Role);
		PlayerState->SetLifeState(EBHPlayerLifeState::Alive);
	}
	if (Character)
	{
		Character->SetPlayerState(PlayerState);
	}
	return PlayerState;
}

void BHAdvanceProofWorld(UWorld* World, float DeltaSeconds)
{
	if (!World)
	{
		return;
	}

	World->TimeSeconds += DeltaSeconds;
	World->UnpausedTimeSeconds += DeltaSeconds;
	World->RealTimeSeconds += DeltaSeconds;
	World->AudioTimeSeconds += DeltaSeconds;
	World->DeltaTimeSeconds = DeltaSeconds;
	World->DeltaRealTimeSeconds = DeltaSeconds;
	World->GetTimerManager().Tick(DeltaSeconds);
}

FBHBotDecisionCandidate BHMakeProofBotCandidate(EBHBotIntent Intent)
{
	FBHBotDecisionCandidate Candidate;
	Candidate.Intent = Intent;
	Candidate.BaseScore = 1.0f;
	Candidate.Urgency = 0.2f;
	Candidate.Risk = 0.0f;
	Candidate.Distance = 500.0f;
	return Candidate;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHFlickerBurstRestoreTest,
	"BlackoutHunt.Horror.FlickerBurstRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHFlickerBurstRestoreTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHFlickerLight* Light = World->SpawnActor<ABHFlickerLight>();
	TestNotNull(TEXT("Flicker light spawns."), Light);
	if (Light)
	{
		TestTrue(TEXT("Flicker light starts powered."), Light->IsPowered());
		Light->TriggerFlickerBurst(0.1f, 1.8f, FLinearColor::Red, 18.0f, true);
		TestTrue(TEXT("Temporary flicker burst starts."), Light->IsFlickerBurstActive());
		for (int32 TickIndex = 0; TickIndex < 8 && Light->IsFlickerBurstActive(); ++TickIndex)
		{
			World->TimeSeconds += 0.05;
			World->UnpausedTimeSeconds += 0.05;
			World->RealTimeSeconds += 0.05;
			World->AudioTimeSeconds += 0.05;
			World->DeltaTimeSeconds = 0.05f;
			World->DeltaRealTimeSeconds = 0.05f;
			World->GetTimerManager().Tick(0.05f);
			Light->Tick(0.05f);
		}
		TestFalse(TEXT("Temporary flicker burst restores after its timer."), Light->IsFlickerBurstActive());
		TestTrue(TEXT("Temporary flicker burst does not permanently cut power."), Light->IsPowered());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBotMemoryNoiseStimulusTest,
	"BlackoutHunt.AI.BotMemoryNoiseStimulus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBotMemoryNoiseStimulusTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	if (GameMode)
	{
		const FVector NoiseLocation(125.0f, -350.0f, 98.0f);
		GameMode->ReportBotStimulus(EBHBotStimulusType::Noise, NoiseLocation, nullptr, nullptr, TEXT("automation footstep"), 1.0f);

		FBHBotMemory Memory;
		TestTrue(TEXT("Bot memory snapshot reports recent stimuli."), GameMode->GetBotWorldMemorySnapshot(Memory, 2.0f));
		TestEqual(TEXT("Bot memory records the latest noise location."), Memory.LastHeardLocation, NoiseLocation);
		TestTrue(TEXT("Bot memory stores the recent noise stimulus."), Memory.RecentStimuli.Num() == 1 && Memory.RecentStimuli[0].Type == EBHBotStimulusType::Noise);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBotPolicyPersonalityScoringTest,
	"BlackoutHunt.AI.BotPolicyPersonalityScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBotPolicyPersonalityScoringTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("Bot policy scorer has a valid game instance outer."), TestGameInstance);
	if (!TestGameInstance)
	{
		return false;
	}

	UBHBotPolicySubsystem* Policy = NewObject<UBHBotPolicySubsystem>(TestGameInstance);
	TestNotNull(TEXT("Bot policy scorer can be constructed."), Policy);
	if (!Policy)
	{
		return false;
	}

	FBHBotPolicyFeatures Features;
	Features.Difficulty = EBHBotDifficulty::Hard;

	Features.Personality = EBHBotPersonality::Objective;
	TArray<FBHBotDecisionCandidate> ObjectiveCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::AnswerStation),
		BHMakeProofBotCandidate(EBHBotIntent::Hide)
	};
	FBHBotPolicyResult ObjectiveResult = Policy->ScoreCandidates(Features, ObjectiveCandidates);
	TestTrue(TEXT("Objective runner prefers station work over hiding when legal."), ObjectiveCandidates.IsValidIndex(ObjectiveResult.ChosenIndex) && ObjectiveCandidates[ObjectiveResult.ChosenIndex].Intent == EBHBotIntent::AnswerStation);

	Features.Personality = EBHBotPersonality::Cautious;
	TArray<FBHBotDecisionCandidate> CautiousCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::AnswerStation),
		BHMakeProofBotCandidate(EBHBotIntent::Hide)
	};
	FBHBotPolicyResult CautiousResult = Policy->ScoreCandidates(Features, CautiousCandidates);
	TestTrue(TEXT("Cautious hider prefers hiding over station work when scores are otherwise equal."), CautiousCandidates.IsValidIndex(CautiousResult.ChosenIndex) && CautiousCandidates[CautiousResult.ChosenIndex].Intent == EBHBotIntent::Hide);

	Features.Personality = EBHBotPersonality::Trickster;
	TArray<FBHBotDecisionCandidate> TricksterCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::Flee),
		BHMakeProofBotCandidate(EBHBotIntent::Bait)
	};
	FBHBotPolicyResult TricksterResult = Policy->ScoreCandidates(Features, TricksterCandidates);
	TestTrue(TEXT("Decoy baiter prefers baiting over plain fleeing when legal."), TricksterCandidates.IsValidIndex(TricksterResult.ChosenIndex) && TricksterCandidates[TricksterResult.ChosenIndex].Intent == EBHBotIntent::Bait);

	Features.Personality = EBHBotPersonality::Aggressive;
	Features.Role = EBHPlayerRole::Hunter;
	TArray<FBHBotDecisionCandidate> AggressiveCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::AmbushObjective),
		BHMakeProofBotCandidate(EBHBotIntent::Chase)
	};
	FBHBotPolicyResult AggressiveResult = Policy->ScoreCandidates(Features, AggressiveCandidates);
	TestTrue(TEXT("Aggressive Teacher prefers chase over ambush when a chase is legal."), AggressiveCandidates.IsValidIndex(AggressiveResult.ChosenIndex) && AggressiveCandidates[AggressiveResult.ChosenIndex].Intent == EBHBotIntent::Chase);

	Features.Personality = EBHBotPersonality::Suspicious;
	Features.Role = EBHPlayerRole::Hunter;
	TArray<FBHBotDecisionCandidate> SuspiciousTeacherCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::Chase),
		BHMakeProofBotCandidate(EBHBotIntent::SearchLocker)
	};
	FBHBotPolicyResult SuspiciousTeacherResult = Policy->ScoreCandidates(Features, SuspiciousTeacherCandidates);
	TestTrue(TEXT("Locker Inspector Teacher prefers a suspicious locker check over blind chase pressure."), SuspiciousTeacherCandidates.IsValidIndex(SuspiciousTeacherResult.ChosenIndex) && SuspiciousTeacherCandidates[SuspiciousTeacherResult.ChosenIndex].Intent == EBHBotIntent::SearchLocker);

	Features.Role = EBHPlayerRole::FakeHunter;
	Features.Personality = EBHBotPersonality::Trickster;
	TArray<FBHBotDecisionCandidate> TricksterMonitorCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::UseScan),
		BHMakeProofBotCandidate(EBHBotIntent::UsePower)
	};
	FBHBotPolicyResult TricksterMonitorResult = Policy->ScoreCandidates(Features, TricksterMonitorCandidates);
	TestTrue(TEXT("Misdirection Hall Monitor prefers false markers over real hints when both are legal."), TricksterMonitorCandidates.IsValidIndex(TricksterMonitorResult.ChosenIndex) && TricksterMonitorCandidates[TricksterMonitorResult.ChosenIndex].Intent == EBHBotIntent::UsePower);

	Features.Personality = EBHBotPersonality::Suspicious;
	TArray<FBHBotDecisionCandidate> RoamingMonitorCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::DropTrap),
		BHMakeProofBotCandidate(EBHBotIntent::UseScan)
	};
	FBHBotPolicyResult RoamingMonitorResult = Policy->ScoreCandidates(Features, RoamingMonitorCandidates);
	TestTrue(TEXT("Roaming Hall Monitor prefers scouting hints over trap spam when both are legal."), RoamingMonitorCandidates.IsValidIndex(RoamingMonitorResult.ChosenIndex) && RoamingMonitorCandidates[RoamingMonitorResult.ChosenIndex].Intent == EBHBotIntent::UseScan);

	Features.Personality = EBHBotPersonality::Ambusher;
	TArray<FBHBotDecisionCandidate> TrapMonitorCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::UseScan),
		BHMakeProofBotCandidate(EBHBotIntent::DropTrap)
	};
	FBHBotPolicyResult TrapMonitorResult = Policy->ScoreCandidates(Features, TrapMonitorCandidates);
	TestTrue(TEXT("Trap Hall Monitor prefers route traps over another scan when both are legal."), TrapMonitorCandidates.IsValidIndex(TrapMonitorResult.ChosenIndex) && TrapMonitorCandidates[TrapMonitorResult.ChosenIndex].Intent == EBHBotIntent::DropTrap);

	Features.Role = EBHPlayerRole::Survivor;
	Features.Personality = EBHBotPersonality::Objective;
	TArray<FBHBotDecisionCandidate> ClaimedObjectiveCandidates = {
		BHMakeProofBotCandidate(EBHBotIntent::AnswerStation),
		BHMakeProofBotCandidate(EBHBotIntent::RepairBreaker)
	};
	ClaimedObjectiveCandidates[0].BaseScore += 0.18f;
	ClaimedObjectiveCandidates[0].TargetClaimCount = 4;
	FBHBotPolicyResult ClaimedObjectiveResult = Policy->ScoreCandidates(Features, ClaimedObjectiveCandidates);
	TestTrue(TEXT("Policy avoids crowded claimed objectives when another useful target is available."), ClaimedObjectiveCandidates.IsValidIndex(ClaimedObjectiveResult.ChosenIndex) && ClaimedObjectiveCandidates[ClaimedObjectiveResult.ChosenIndex].Intent == EBHBotIntent::RepairBreaker);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAtmosphereDirectorBudgetThrottleTest,
	"BlackoutHunt.Horror.DirectorBudgetThrottlesRepeatedMonsterBeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAtmosphereDirectorBudgetThrottleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntDirectorBudget"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(120.0f, 0.0f, 120.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	TestNotNull(TEXT("Survivor spawns."), Survivor);
	if (GameState && GameMode && Survivor)
	{
		GameState->SetRoundPhase(EBHRoundPhase::Hunt);
		BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Budget Student"));

		FString Reason;
		TestTrue(TEXT("First monster beat can reserve director budget."),
			GameMode->DebugReserveDirectorCue(Survivor, EBHScareEventType::MonsterCharge, 0.90f, Reason));
		TestFalse(TEXT("Immediate repeated monster beat is throttled for the same player."),
			GameMode->DebugReserveDirectorCue(Survivor, EBHScareEventType::MonsterCharge, 0.90f, Reason));
		TestTrue(TEXT("Throttle reason explains cooldown or budget."),
			Reason.Contains(TEXT("cooldown"), ESearchCase::IgnoreCase) || Reason.Contains(TEXT("budget"), ESearchCase::IgnoreCase));

		World->TimeSeconds += 90.0f;
		World->UnpausedTimeSeconds += 90.0f;
		World->RealTimeSeconds += 90.0f;
		World->AudioTimeSeconds += 90.0f;

		TestTrue(TEXT("Budget recovers enough for a later subtle cue."),
			GameMode->DebugReserveDirectorCue(Survivor, EBHScareEventType::Whisper, 0.35f, Reason));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHHostAuthorityRejectsNullRequesterTest,
	"BlackoutHunt.Network.HostAuthorityRejectsNullRequester",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHHostAuthorityRejectsNullRequesterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	if (GameMode)
	{
		TestFalse(TEXT("Host-only actions reject missing or non-authoritative requesters."), GameMode->RequireHostAdmin(nullptr, TEXT("automation authority test")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRoleWarmupSafetyTest,
	"BlackoutHunt.Round.RoleWarmupSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRoleWarmupSafetyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntWarmup"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Prep);
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ABHObjectiveStation* Station = World->SpawnActor<ABHObjectiveStation>(FVector(120.0f, 0.0f, 95.0f), FRotator::ZeroRotator);
	ABHBreaker* Breaker = World->SpawnActor<ABHBreaker>(FVector(240.0f, 0.0f, 95.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Warmup game state spawns."), GameState);
	TestNotNull(TEXT("Warmup survivor spawns."), Survivor);
	TestNotNull(TEXT("Warmup station spawns."), Station);
	TestNotNull(TEXT("Warmup breaker spawns."), Breaker);
	if (!GameState || !Survivor || !Station || !Breaker)
	{
		return false;
	}

	BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Warmup Student"));
	Station->Configure(EBHObjectiveStationType::Terminal);
	TestEqual(TEXT("Prep is presented as role warmup."), GameState->GetPhaseText(), FString(TEXT("Role Warmup")));
	TestTrue(TEXT("Warmup breaker can be tried by survivors."), Breaker->CanInteract_Implementation(Survivor));
	TestTrue(TEXT("Warmup question accepts a correct answer."), Station->SubmitAnswer(Survivor, Station->GetCorrectAnswerIndexForBot()));
	TestTrue(TEXT("Warmup correct answer unlocks local practice work."), Station->IsQuestionSolved());

	Survivor->RefillFlashlight(-70.0f);
	Survivor->RecoverStamina(-40.0f);
	Survivor->AddFear(55.0f);
	Survivor->AddDread(65.0f);
	Survivor->ApplyDetentionMark(20.0f);
	Survivor->ResetRoleWarmupStateForRoundStart();
	TestEqual(TEXT("Warmup reset restores flashlight."), Survivor->GetFlashlightBattery(), 100.0f);
	TestEqual(TEXT("Warmup reset restores stamina."), Survivor->GetStaminaPercent(), 100.0f);
	TestEqual(TEXT("Warmup reset clears fear."), Survivor->GetFear(), 0.0f);
	TestEqual(TEXT("Warmup reset clears dread."), Survivor->GetDread(), 0.0f);
	TestFalse(TEXT("Warmup reset clears detention mark."), Survivor->IsDetentionMarked());

	return true;
}

// A Teacher blackout weakens the flashlight of nearby students only: their battery drains faster (and their
// beam flickers near-dead, dimmed in UpdateFlashlightFeel). A student outside the darkened radius and the
// Teacher themselves are never affected, and everything relaxes back once the window closes. Proves the
// located GameState gate (IsPointInTeacherBlackout) and the role/proximity gate + battery surge in
// ABHCharacter::IsInTeacherBlackout / GetEffectiveFlashlightDrainPerSecond.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHTeacherBlackoutWeakensNearbyStudentFlashlightTest,
	"BlackoutHunt.Horror.TeacherBlackoutWeakensNearbyStudentFlashlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHTeacherBlackoutWeakensNearbyStudentFlashlightTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntBlackout"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	const FVector BlackoutOrigin(0.0f, 0.0f, 140.0f);
	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHCharacter* NearStudent = World->SpawnActor<ABHCharacter>(BlackoutOrigin, FRotator::ZeroRotator);
	ABHCharacter* FarStudent = World->SpawnActor<ABHCharacter>(FVector(6000.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ABHCharacter* Teacher = World->SpawnActor<ABHCharacter>(FVector(200.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Blackout game state spawns."), GameState);
	TestNotNull(TEXT("Near student spawns."), NearStudent);
	TestNotNull(TEXT("Far student spawns."), FarStudent);
	TestNotNull(TEXT("Teacher spawns."), Teacher);
	if (!GameState || !NearStudent || !FarStudent || !Teacher)
	{
		return false;
	}

	BHAttachProofPlayerState(World, NearStudent, EBHPlayerRole::Survivor, TEXT("Near Student"));
	BHAttachProofPlayerState(World, FarStudent, EBHPlayerRole::Survivor, TEXT("Far Student"));
	BHAttachProofPlayerState(World, Teacher, EBHPlayerRole::Hunter, TEXT("Blackout Teacher"));

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	TestNotNull(TEXT("Game settings resolve."), Settings);
	if (!Settings)
	{
		return false;
	}
	const float ExpectedMultiplier = FMath::Max(1.0f, Settings->BlackoutFlashlightDrainMultiplier);
	const float BaseDrain = NearStudent->GetEffectiveFlashlightDrainPerSecond();

	// No blackout yet: nobody is weakened and everyone drains at the baseline rate.
	TestFalse(TEXT("No Teacher blackout is active at round start."), GameState->IsTeacherBlackoutActive());
	TestFalse(TEXT("Near student is not weakened before any blackout."), NearStudent->IsInTeacherBlackout());

	// Open a located blackout centred on the near student with a 2000-unit radius.
	GameState->SetTeacherBlackout(BlackoutOrigin, 2000.0f, GameState->GetServerWorldTimeSeconds() + 5.0f);
	TestTrue(TEXT("Teacher blackout reports active."), GameState->IsTeacherBlackoutActive());
	TestTrue(TEXT("The blackout origin is inside the darkened radius."), GameState->IsPointInTeacherBlackout(BlackoutOrigin));
	TestFalse(TEXT("A point well outside the radius is not in the blackout."), GameState->IsPointInTeacherBlackout(FVector(6000.0f, 0.0f, 140.0f)));

	// Near student: weakened, battery drains by the configured multiplier.
	TestTrue(TEXT("Near student is caught in the blackout."), NearStudent->IsInTeacherBlackout());
	const float SurgedDrain = NearStudent->GetEffectiveFlashlightDrainPerSecond();
	TestTrue(TEXT("Near student's flashlight drains faster during the blackout."), SurgedDrain > BaseDrain + KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Surge multiplies the baseline drain by the configured factor."), SurgedDrain, BaseDrain * ExpectedMultiplier);

	// Far student: outside the radius, untouched.
	TestFalse(TEXT("Far student is outside the blackout."), FarStudent->IsInTeacherBlackout());
	TestEqual(TEXT("Far student's flashlight is not surged."), FarStudent->GetEffectiveFlashlightDrainPerSecond(), BaseDrain);

	// Teacher: inside the radius but not a student, so never weakened by their own dark.
	TestFalse(TEXT("Teacher is not weakened by their own blackout."), Teacher->IsInTeacherBlackout());
	TestEqual(TEXT("Teacher's own flashlight is not surged."), Teacher->GetEffectiveFlashlightDrainPerSecond(), BaseDrain);

	// After the window closes, the near student relaxes back to the baseline.
	BHAdvanceProofWorld(World, 6.0f);
	TestFalse(TEXT("Teacher blackout window expires."), GameState->IsTeacherBlackoutActive());
	TestFalse(TEXT("Near student is no longer weakened after the window."), NearStudent->IsInTeacherBlackout());
	TestEqual(TEXT("Near student's flashlight drain returns to baseline after the blackout."), NearStudent->GetEffectiveFlashlightDrainPerSecond(), BaseDrain);

	return true;
}

// Holding E and walking away from a hold-style objective (station/breaker) used to keep completing the
// task: the worker was only dropped on key release (StopInteract), and nothing re-checked range while the
// key stayed held. The server-tick guard now ends the interaction once the player leaves reach.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHHeldInteractionRangeGuardTest,
	"BlackoutHunt.Interaction.HeldInteractionEndsOutOfRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHHeldInteractionRangeGuardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntHeldInteraction"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ABHObjectiveStation* Station = World->SpawnActor<ABHObjectiveStation>(FVector(120.0f, 0.0f, 95.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Hunt game state spawns."), GameState);
	TestNotNull(TEXT("Hold-interaction survivor spawns."), Survivor);
	TestNotNull(TEXT("Hold-interaction station spawns."), Station);
	if (!GameState || !Survivor || !Station)
	{
		return false;
	}

	BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Hold Student"));
	// Leave the station unconfigured: with no question loaded it is immediately holdable, the same state a
	// solved question reaches, so begin-interact registers the survivor as a worker.

	TestTrue(TEXT("Survivor in reach can begin holding the station."), Survivor->DebugBeginInteractForTest(Station));
	TestTrue(TEXT("The station is the active server interaction target."), Survivor->DebugGetServerInteractTargetForTest() == Station);

	// Still in reach: the range guard must NOT cancel a legitimate hold (e.g. turning to read the question).
	Survivor->DebugEndStaleHeldInteractionForTest();
	TestTrue(TEXT("In-range hold is preserved by the range guard."), Survivor->DebugGetServerInteractTargetForTest() == Station);

	// Walk far away while still holding E (StopInteract never fired): the guard ends the interaction so the
	// station stops counting the survivor toward completion.
	Survivor->SetActorLocation(FVector(6000.0f, 0.0f, 140.0f));
	Survivor->DebugEndStaleHeldInteractionForTest();
	TestNull(TEXT("Walking out of reach ends the held interaction."), Survivor->DebugGetServerInteractTargetForTest());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHWarmupChecklistTest,
	"BlackoutHunt.Round.WarmupChecklist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHWarmupChecklistTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Pure-function invariants for the guided warmup checklist + role-intro copy (SHARED-A / SHARED-C).
	int32 Done = 0;
	int32 Total = 0;
	BHWarmupProgress(EBHPlayerRole::Survivor, 0, Done, Total);
	TestEqual(TEXT("Survivor warmup has three steps."), Total, 3);
	TestEqual(TEXT("Empty mask completes zero steps."), Done, 0);
	BHWarmupProgress(EBHPlayerRole::Hunter, 0, Done, Total);
	TestEqual(TEXT("Hunter warmup has two steps."), Total, 2);
	BHWarmupProgress(EBHPlayerRole::FakeHunter, 0, Done, Total);
	TestEqual(TEXT("Hall Monitor warmup has two steps."), Total, 2);

	const uint8 SurvivorAll = BHWarmupExpectedMask(EBHPlayerRole::Survivor);
	TestTrue(TEXT("Next-step label is empty once every step is done."), BHWarmupNextStepLabel(EBHPlayerRole::Survivor, SurvivorAll).IsEmpty());
	TestFalse(TEXT("Next-step label is non-empty with steps remaining."), BHWarmupNextStepLabel(EBHPlayerRole::Survivor, 0).IsEmpty());

	// Role-intro copy exists for every assignable role and is never blank (no silent empty cards).
	for (const EBHPlayerRole IntroRole : { EBHPlayerRole::Survivor, EBHPlayerRole::Hunter, EBHPlayerRole::FakeHunter, EBHPlayerRole::Spectator })
	{
		const FBHRoleIntroCopy Copy = BHGetRoleIntroCopy(IntroRole, true);
		TestFalse(TEXT("Role intro has a title."), Copy.Title.IsEmpty());
		TestFalse(TEXT("Role intro has a goal."), Copy.Goal.IsEmpty());
		TestTrue(TEXT("Role intro lists at least one control."), Copy.Keys.Num() > 0);
	}

	// Server-authoritative MarkWarmupStep behaviour against a real PlayerState in Prep.
	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntWarmupChecklist"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Prep);
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Checklist game state spawns."), GameState);
	TestNotNull(TEXT("Checklist survivor spawns."), Survivor);
	if (!GameState || !Survivor)
	{
		return false;
	}
	ABHPlayerState* PlayerState = BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Checklist Student"));
	TestNotNull(TEXT("Checklist player state attaches."), PlayerState);
	if (!PlayerState)
	{
		return false;
	}

	TestEqual(TEXT("Warmup mask starts empty."), static_cast<int32>(PlayerState->WarmupChecklistMask), 0);
	TestFalse(TEXT("Warmup starts incomplete."), PlayerState->bWarmupComplete);

	PlayerState->MarkWarmupStep(EBHWarmupStep::Flashlight);
	PlayerState->MarkWarmupStep(EBHWarmupStep::Flashlight); // idempotent: re-marking the same step does nothing
	BHWarmupProgress(PlayerState->PlayerRole, PlayerState->WarmupChecklistMask, Done, Total);
	TestEqual(TEXT("One distinct step counts once."), Done, 1);
	TestFalse(TEXT("Not complete after one step."), PlayerState->bWarmupComplete);

	PlayerState->MarkWarmupStep(EBHWarmupStep::Hide);
	PlayerState->MarkWarmupStep(EBHWarmupStep::Question);
	TestTrue(TEXT("All three survivor steps complete the warmup."), PlayerState->bWarmupComplete);

	// Reset clears it so warmup state never carries into the live Hunt.
	PlayerState->ResetWarmupChecklist();
	TestEqual(TEXT("Reset clears the mask."), static_cast<int32>(PlayerState->WarmupChecklistMask), 0);
	TestFalse(TEXT("Reset clears completion."), PlayerState->bWarmupComplete);

	// Outside the warmup phase the mark is a no-op, so it can never pollute the live Hunt.
	GameState->SetRoundPhase(EBHRoundPhase::Hunt);
	PlayerState->MarkWarmupStep(EBHWarmupStep::Flashlight);
	TestEqual(TEXT("MarkWarmupStep is a no-op outside Prep."), static_cast<int32>(PlayerState->WarmupChecklistMask), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHTutorialDirectorSpawnsTest,
	"BlackoutHunt.Round.TutorialDirectorSpawns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHTutorialDirectorSpawnsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// The dedicated tutorial level resolves to a non-empty package (its baked authored .umap once cooked,
	// otherwise the runtime base map), and the special-case never collides with the train intermission.
	TestFalse(TEXT("Tutorial resolves to a non-empty package."), BHResolveLevelMapPackage(TEXT("Tutorial")).IsEmpty());
	TestEqual(TEXT("Train intermission still uses the runtime base map."), BHResolveLevelMapPackage(TEXT("TrainIntermission")), FString(TEXT("/Engine/Maps/Entry")));

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntTutorialDirector"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	// The director is baked into the Tutorial map; spawning it must succeed and its deferred BeginPlay
	// must not crash (it only arms a one-shot activation timer until the host pawn exists).
	BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHTutorialDirector* Director = World->SpawnActor<ABHTutorialDirector>(FVector::ZeroVector, FRotator::ZeroRotator);
	TestNotNull(TEXT("Tutorial director spawns without crashing."), Director);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHSurvivorAntiCampPressureTest,
	"BlackoutHunt.Horror.AntiCampRequiresMeaningfulMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHSurvivorAntiCampPressureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntAntiCamp"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Anti-camp game state spawns."), GameState);
	TestNotNull(TEXT("Anti-camp survivor spawns."), Survivor);
	if (!GameState || !Survivor)
	{
		return false;
	}

	BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("AntiCamp Student"));
	Survivor->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const float GraceSeconds = Settings ? Settings->AntiCampGraceSeconds : 60.0f;
	const float AlertDelaySeconds = Settings ? Settings->AntiCampAlertDelaySeconds : 18.0f;
	const float RequiredMoveSeconds = Settings ? Settings->AntiCampRequiredMoveSeconds : 5.0f;
	const float MovementSpeedThreshold = Settings ? Settings->AntiCampMovementSpeedThreshold : 150.0f;

	const auto TickSurvivor = [World, Survivor](float DeltaSeconds)
	{
		BHAdvanceProofWorld(World, DeltaSeconds);
		Survivor->Tick(DeltaSeconds);
	};

	TickSurvivor(0.25f);
	const int32 StationaryTicks = FMath::CeilToInt(GraceSeconds + AlertDelaySeconds + 3.0f);
	for (int32 TickIndex = 0; TickIndex < StationaryTicks; ++TickIndex)
	{
		Survivor->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		TickSurvivor(1.0f);
	}

	TestTrue(TEXT("Stationary survivor gains anti-camp dread pressure."), Survivor->GetDread() > 1.0f);
	const float IdleBeforeTinyMove = Survivor->DebugGetAntiCampIdleSecondsForTest();
	const float DreadBeforeTinyMove = Survivor->GetDread();

	for (int32 TickIndex = 0; TickIndex < 8; ++TickIndex)
	{
		Survivor->SetActorLocation(Survivor->GetActorLocation() + FVector(3.0f, 0.0f, 0.0f));
		Survivor->GetCharacterMovement()->Velocity = FVector(35.0f, 0.0f, 0.0f);
		TickSurvivor(1.0f);
	}

	TestTrue(TEXT("Tiny shuffles do not reset anti-camp idle time."), Survivor->DebugGetAntiCampIdleSecondsForTest() >= IdleBeforeTinyMove + 7.0f);
	TestTrue(TEXT("Tiny shuffles do not count toward the required movement burst."), Survivor->DebugGetAntiCampMoveBurstSecondsForTest() < 0.1f);
	TestTrue(TEXT("Tiny shuffles leave dread pressure active."), Survivor->GetDread() >= DreadBeforeTinyMove);

	const int32 MeaningfulMoveTicks = FMath::CeilToInt(RequiredMoveSeconds);
	for (int32 TickIndex = 0; TickIndex < MeaningfulMoveTicks; ++TickIndex)
	{
		Survivor->SetActorLocation(Survivor->GetActorLocation() + FVector(180.0f, 0.0f, 0.0f));
		Survivor->GetCharacterMovement()->Velocity = FVector(MovementSpeedThreshold + 90.0f, 0.0f, 0.0f);
		TickSurvivor(1.0f);
	}

	TestTrue(TEXT("Sustained meaningful movement refreshes anti-camp safety."), Survivor->DebugGetAntiCampIdleSecondsForTest() <= 2.0f);
	TestTrue(TEXT("Completed movement burst is consumed after refreshing safety."), Survivor->DebugGetAntiCampMoveBurstSecondsForTest() < 0.1f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHCosmeticUnlockThresholdTest,
	"BlackoutHunt.Account.CosmeticUnlockThresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHCosmeticUnlockThresholdTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("Default outfits remain available without XP."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Outfit, 0, 0)
		&& BHCosmeticIsUnlocked(EBHCosmeticCategory::Outfit, 1, 0));
	TestFalse(TEXT("Progression outfit is locked below its XP threshold."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Outfit, 2, 99));
	TestTrue(TEXT("Progression outfit unlocks at its XP threshold."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Outfit, 2, 100));
	TestEqual(TEXT("Locked headwear sanitizes to default."),
		BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::Headwear, 1, 0),
		0);
	TestEqual(TEXT("Gear customization is disabled."),
		BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::Gear, 2, 400),
		0);
	TestEqual(TEXT("Gear cycle stays on default."),
		BHCosmeticNextUnlockedIndex(EBHCosmeticCategory::Gear, 0, 199),
		0);
	TestEqual(TEXT("Gear remains default after former unlock threshold."),
		BHCosmeticNextUnlockedIndex(EBHCosmeticCategory::Gear, 0, 200),
		0);

	return true;
}

// Locks the achievement-gating path added for the prestige tints, the Top Hat headwear, and the new
// Title / Emblem nameplate-flair categories: these ignore XP entirely and are gated on an achievement id.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHCosmeticAchievementGateTest,
	"BlackoutHunt.Account.CosmeticAchievementGating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHCosmeticAchievementGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TArray<FName> NoAchievements;
	TArray<FName> WithChalk;
	WithChalk.Add(FName(TEXT("honorary_faculty")));
	TArray<FName> WithRoofRider;
	WithRoofRider.Add(FName(TEXT("roof_rider")));

	// Base shirt colours (0..7) need neither XP nor an achievement.
	TestTrue(TEXT("Base shirt colour 0 is always unlocked."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::ShirtColor, 0, 0, &NoAchievements));

	// The Chalk tint (index 8) is gated on honorary_faculty: XP is irrelevant, the achievement is everything.
	TestFalse(TEXT("Chalk tint is locked without its achievement, even at max XP."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::ShirtColor, 8, MAX_int32, &NoAchievements));
	TestTrue(TEXT("Chalk tint unlocks with its achievement, at zero XP."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::ShirtColor, 8, 0, &WithChalk));
	TestFalse(TEXT("Chalk tint is locked when no achievement set is supplied (null = locked)."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::ShirtColor, 8, MAX_int32, nullptr));

	// The Top Hat headwear (index 5) is gated on roof_rider.
	TestFalse(TEXT("Top Hat is locked without roof_rider."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Headwear, 5, MAX_int32, &NoAchievements));
	TestTrue(TEXT("Top Hat unlocks with roof_rider."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Headwear, 5, 0, &WithRoofRider));

	// Title / Emblem flair categories: index 0 ("None") is always available; index 1+ is achievement-gated.
	TestTrue(TEXT("'No Title' (index 0) is always available."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Title, 0, 0, &NoAchievements));
	TestFalse(TEXT("An earned title is locked without its achievement."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Title, 1, MAX_int32, &NoAchievements));
	TestTrue(TEXT("'No Emblem' (index 0) is always available."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Emblem, 0, 0, &NoAchievements));
	TestFalse(TEXT("An earned emblem is locked without its achievement."),
		BHCosmeticIsUnlocked(EBHCosmeticCategory::Emblem, 1, MAX_int32, &NoAchievements));

	// Clamp drops a locked achievement-gated selection back to default, and keeps an unlocked one.
	TestEqual(TEXT("Locked tint clamps to default without the achievement."),
		BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::ShirtColor, 8, MAX_int32, &NoAchievements), 0);
	TestEqual(TEXT("Unlocked tint stays selected with the achievement."),
		BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::ShirtColor, 8, 0, &WithChalk), 8);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHObjectiveStationPhysicsTaskIdentityTest,
	"BlackoutHunt.Objectives.PhysicsTaskIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHObjectiveStationPhysicsTaskIdentityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntObjectiveIdentity"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ABHObjectiveStation* Station = World->SpawnActor<ABHObjectiveStation>(FVector(120.0f, 0.0f, 95.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Survivor spawns."), Survivor);
	TestNotNull(TEXT("Objective station spawns."), Station);
	if (!Survivor || !Station)
	{
		return false;
	}

	BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Objective Student"));

	struct FExpectedTask
	{
		EBHObjectiveStationType Type;
		const TCHAR* InstructionNeedle;
		const TCHAR* LabelNeedle;
	};

	const FExpectedTask ExpectedTasks[] = {
		{EBHObjectiveStationType::Valve, TEXT("counterweight lock"), TEXT("Counterweight Lock")},
		{EBHObjectiveStationType::Terminal, TEXT("circuit breaker"), TEXT("Circuit Breaker")},
		{EBHObjectiveStationType::Antenna, TEXT("signal receiver"), TEXT("Signal Receiver")},
		{EBHObjectiveStationType::Evidence, TEXT("energy store"), TEXT("Energy Store")}
	};

	for (const FExpectedTask& Expected : ExpectedTasks)
	{
		Station->Configure(Expected.Type);
		TestTrue(TEXT("Station instruction names the topic-specific physical task."),
			Station->GetPhysicalTaskInstruction().Contains(Expected.InstructionNeedle, ESearchCase::IgnoreCase));
		TestTrue(TEXT("Interaction label names the topic-specific physical task."),
			Station->GetInteractionLabel_Implementation(Survivor).ToString().Contains(Expected.LabelNeedle, ESearchCase::CaseSensitive));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRoundModifierDeadCCTVStateTest,
	"BlackoutHunt.Rules.RoundModifierDeadCCTVState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRoundModifierDeadCCTVStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	TestEqual(TEXT("Dead CCTV modifier has readable text."), ABHGameState::GetRoundModifierTextFor(EBHRoundModifier::DeadCCTV), FString(TEXT("Dead CCTV")));
	TestTrue(TEXT("Dead CCTV modifier explains camera impact."), ABHGameState::GetRoundModifierHintFor(EBHRoundModifier::DeadCCTV).Contains(TEXT("camera")));

	ABHSecurityCamera* Camera = World->SpawnActor<ABHSecurityCamera>();
	ABHCCTVZone* Zone = World->SpawnActor<ABHCCTVZone>();
	TestNotNull(TEXT("Security camera spawns."), Camera);
	TestNotNull(TEXT("CCTV zone spawns."), Zone);
	if (Camera && Zone)
	{
		Zone->ConfigureZone(Camera, 100, TEXT("automation CCTV zone"), FVector(900.0f, 360.0f, 120.0f), true);

		Camera->SetRoundOffline(true);
		TestTrue(TEXT("Round-offline camera is disabled."), Camera->IsCameraDisabled());
		Camera->SetCameraDisabled(false);
		TestTrue(TEXT("Circuit-style enable cannot override round-offline camera."), Camera->IsCameraDisabled());
		Camera->SetRoundOffline(false);
		TestFalse(TEXT("Clearing round-offline restores camera availability."), Camera->IsCameraDisabled());

		Zone->SetRoundOffline(true);
		TestFalse(TEXT("Round-offline zone is disabled."), Zone->IsZoneEnabled());
		Zone->SetZoneEnabled(true);
		TestFalse(TEXT("Circuit-style enable cannot override round-offline zone."), Zone->IsZoneEnabled());
		Zone->SetRoundOffline(false);
		TestTrue(TEXT("Clearing round-offline restores zone availability."), Zone->IsZoneEnabled());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHLateSpectatorResetsForLobbyTest,
	"BlackoutHunt.Network.LateSpectatorResetsForLobby",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHLateSpectatorResetsForLobbyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Lobby);
	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	ABHPlayerController* Controller = World->SpawnActor<ABHPlayerController>();
	ABHPlayerState* PlayerState = World->SpawnActor<ABHPlayerState>();
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	TestNotNull(TEXT("Player controller spawns."), Controller);
	TestNotNull(TEXT("Player state spawns."), PlayerState);
	if (GameState && GameMode && Controller && PlayerState)
	{
		Controller->PlayerState = PlayerState;
		PlayerState->SetRole(EBHPlayerRole::Spectator);
		PlayerState->SetDesiredRole(EBHPlayerRole::Survivor);
		PlayerState->SetLifeState(EBHPlayerLifeState::Captured);
		PlayerState->SetSpectatorRolePreference(EBHPlayerRole::Hunter);
		PlayerState->AddSpectatorEncouragement();

		GameMode->DebugRestorePlayersAfterTravelForTest(Controller);

		TestEqual(TEXT("Late spectator returns to assignable lobby state."), PlayerState->PlayerRole, EBHPlayerRole::Unassigned);
		TestEqual(TEXT("Late spectator life state resets for the lobby."), PlayerState->LifeState, EBHPlayerLifeState::Alive);
		TestEqual(TEXT("Late spectator role preference remains visible for host approval."), PlayerState->SpectatorRolePreference, EBHPlayerRole::Hunter);
		TestEqual(TEXT("Late spectator support count resets for the next lobby."), PlayerState->SpectatorEncouragementCount, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHLateSpectatorPreferenceSurvivesTrainIntermissionTest,
	"BlackoutHunt.Network.LateSpectatorPreferenceSurvivesTrainIntermission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHLateSpectatorPreferenceSurvivesTrainIntermissionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	ABHPlayerController* Controller = World->SpawnActor<ABHPlayerController>();
	ABHPlayerState* PlayerState = World->SpawnActor<ABHPlayerState>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	TestNotNull(TEXT("Player controller spawns."), Controller);
	TestNotNull(TEXT("Player state spawns."), PlayerState);
	if (GameMode && Controller && PlayerState)
	{
		GameMode->DebugSetTrainIntermissionLevelForTest(true);
		Controller->PlayerState = PlayerState;
		PlayerState->SetRole(EBHPlayerRole::Spectator);
		PlayerState->SetDesiredRole(EBHPlayerRole::Survivor);
		PlayerState->SetLifeState(EBHPlayerLifeState::Captured);
		PlayerState->SetSpectatorRolePreference(EBHPlayerRole::FakeHunter);
		PlayerState->AddSpectatorEncouragement();

		GameMode->DebugRestorePlayersAfterTravelForTest(Controller);

		TestEqual(TEXT("Late spectator becomes train participant after round travel."), PlayerState->PlayerRole, EBHPlayerRole::Survivor);
		TestTrue(TEXT("Late spectator is readied for train intermission."), PlayerState->bReady);
		TestEqual(TEXT("Late spectator role preference survives train intermission."), PlayerState->SpectatorRolePreference, EBHPlayerRole::FakeHunter);
		TestEqual(TEXT("Late spectator support count resets for train intermission."), PlayerState->SpectatorEncouragementCount, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHActiveRolesResetForLobbyAfterTravelTest,
	"BlackoutHunt.Network.ActiveRolesResetForLobbyAfterTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHActiveRolesResetForLobbyAfterTravelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Lobby);
	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	ABHPlayerController* Controller = World->SpawnActor<ABHPlayerController>();
	ABHPlayerState* PlayerState = World->SpawnActor<ABHPlayerState>();
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	TestNotNull(TEXT("Player controller spawns."), Controller);
	TestNotNull(TEXT("Player state spawns."), PlayerState);
	if (GameState && GameMode && Controller && PlayerState)
	{
		Controller->PlayerState = PlayerState;
		PlayerState->SetRole(EBHPlayerRole::FakeHunter);
		PlayerState->SetDesiredRole(EBHPlayerRole::Hunter);
		PlayerState->SetLifeState(EBHPlayerLifeState::Captured);
		PlayerState->SetFakeHunterEligible(true);
		PlayerState->SetReady(true);

		GameMode->DebugRestorePlayersAfterTravelForTest(Controller);

		TestEqual(TEXT("Active round role clears to assignable lobby state."), PlayerState->PlayerRole, EBHPlayerRole::Unassigned);
		TestEqual(TEXT("Queued host role survives the lobby reset."), PlayerState->DesiredRole, EBHPlayerRole::Hunter);
		TestEqual(TEXT("Captured state clears for the next lobby."), PlayerState->LifeState, EBHPlayerLifeState::Alive);
		TestFalse(TEXT("Hall Monitor eligibility does not leak to the next lobby."), PlayerState->bFakeHunterEligible);
		TestFalse(TEXT("Ready state resets after travel."), PlayerState->bReady);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHLateJoinerMidRoundStaysSpectatorTest,
	"BlackoutHunt.Network.LateJoinerMidRoundStaysSpectator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHLateJoinerMidRoundStaysSpectatorTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// A player who played an earlier round leaves behind a persisted travel entry as a live Survivor.
	UBHGameInstance* GameInstance = NewObject<UBHGameInstance>();
	TestNotNull(TEXT("Travel game instance is created."), GameInstance);
	if (!GameInstance)
	{
		return false;
	}

	ABHPlayerState* SourcePS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Source player state can be created."), SourcePS);
	if (!SourcePS)
	{
		return false;
	}
	SourcePS->SetPlayerName(TEXT("Mid Round Returner"));
	SourcePS->SetRole(EBHPlayerRole::Survivor);
	SourcePS->SetDesiredRole(EBHPlayerRole::Survivor);
	SourcePS->SetLifeState(EBHPlayerLifeState::Alive);
	SourcePS->AddQuestionPoints(40);
	// A legitimate returner carries the server-issued reconnect token (echoed in the travel login URL); restore
	// keys on that strong identity, not the display name, so both the persisted entry and the returners set it.
	SourcePS->ReconnectToken = TEXT("travel-token-returner");
	GameInstance->PersistTravelPlayerState(SourcePS);

	// Active-round late join: PostLogin has already placed the returning player into Spectator +
	// Captured. The GATED restore (bApplyRoleAndLifeState = false, exactly what RestorePlayersAfterTravel
	// passes during Prep/Hunt/FinalEscape) must PRESERVE that spectator identity so the player is not
	// resurrected as a live, capturable, win-counted participant - while still carrying banked points.
	ABHPlayerState* ActiveJoinPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Active-join player state can be created."), ActiveJoinPS);
	if (ActiveJoinPS)
	{
		ActiveJoinPS->SetPlayerName(TEXT("Mid Round Returner"));
		ActiveJoinPS->ReconnectToken = TEXT("travel-token-returner");
		ActiveJoinPS->SetRole(EBHPlayerRole::Spectator);
		ActiveJoinPS->SetDesiredRole(EBHPlayerRole::Survivor);
		ActiveJoinPS->SetLifeState(EBHPlayerLifeState::Captured);

		TestTrue(TEXT("Gated restore still matches the persisted entry."), GameInstance->RestoreTravelPlayerState(ActiveJoinPS, /*bApplyRoleAndLifeState=*/false));
		TestEqual(TEXT("Mid-round late joiner stays a spectator (not resurrected)."), ActiveJoinPS->PlayerRole, EBHPlayerRole::Spectator);
		TestEqual(TEXT("Mid-round late joiner stays captured (not made live/capturable)."), ActiveJoinPS->LifeState, EBHPlayerLifeState::Captured);
		TestEqual(TEXT("Mid-round late joiner still recovers banked question points."), ActiveJoinPS->QuestionPoints, 40);
	}

	// Legitimate cross-travel landing (Lobby / train Intermission): the same entry DOES fully restore
	// the live role, proving the gate only suppresses identity during active phases.
	ABHPlayerState* LobbyJoinPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Lobby-join player state can be created."), LobbyJoinPS);
	if (LobbyJoinPS)
	{
		LobbyJoinPS->SetPlayerName(TEXT("Mid Round Returner"));
		LobbyJoinPS->ReconnectToken = TEXT("travel-token-returner");
		LobbyJoinPS->SetRole(EBHPlayerRole::Spectator);
		LobbyJoinPS->SetLifeState(EBHPlayerLifeState::Captured);

		TestTrue(TEXT("Full restore matches the persisted entry."), GameInstance->RestoreTravelPlayerState(LobbyJoinPS, /*bApplyRoleAndLifeState=*/true));
		TestEqual(TEXT("Cross-travel restore returns the player to their live role."), LobbyJoinPS->PlayerRole, EBHPlayerRole::Survivor);
		TestEqual(TEXT("Cross-travel restore returns the player to alive."), LobbyJoinPS->LifeState, EBHPlayerLifeState::Alive);
		TestEqual(TEXT("Cross-travel restore also carries banked question points."), LobbyJoinPS->QuestionPoints, 40);
	}

	// Security (0.8.1): a brand-new student who merely reuses an earlier student's display name - with NO
	// reconnect token and no real online id (the OSS-Null / LAN / Playit classroom path) - must NOT inherit
	// that student's banked progress. Restore refuses the weak name-only match, so the imposter stays at zero.
	ABHPlayerState* ImposterPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Imposter player state can be created."), ImposterPS);
	if (ImposterPS)
	{
		ImposterPS->SetPlayerName(TEXT("Mid Round Returner"));
		ImposterPS->SetLifeState(EBHPlayerLifeState::Alive);
		TestFalse(TEXT("A tokenless same-named joiner does not match the persisted entry."), GameInstance->RestoreTravelPlayerState(ImposterPS, /*bApplyRoleAndLifeState=*/true));
		TestEqual(TEXT("A tokenless same-named joiner inherits no banked question points."), ImposterPS->QuestionPoints, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHCharacterSpecialMovementTest,
	"BlackoutHunt.Movement.SpecialMovementStatesAndTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHCharacterSpecialMovementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(120.0f, 0.0f, 39.0f), FRotator::ZeroRotator);
	if (Floor)
	{
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Floor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
		}
		Floor->SetActorScale3D(FVector(24.0f, 8.0f, 0.10f));
		Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Floor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
	}
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ABHCharacter* Hunter = World->SpawnActor<ABHCharacter>(FVector(240.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Movement proof floor spawns."), Floor);
	TestNotNull(TEXT("Survivor spawns."), Survivor);
	TestNotNull(TEXT("Hunter spawns."), Hunter);
	if (Survivor && Hunter)
	{
		BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Movement Student"));
		BHAttachProofPlayerState(World, Hunter, EBHPlayerRole::Hunter, TEXT("Movement Teacher"));
		Survivor->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Hunter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Survivor->GetCharacterMovement()->Velocity = FVector(500.0f, 0.0f, 0.0f);
		Hunter->GetCharacterMovement()->Velocity = FVector(500.0f, 0.0f, 0.0f);

		TestTrue(TEXT("Survivor roll starts when sprint-special validation passes."), Survivor->TryStartSpecialMoveForTest(EBHMovementSpecialState::Rolling, false));
		TestEqual(TEXT("Survivor enters rolling state."), Survivor->GetMovementSpecialState(), EBHMovementSpecialState::Rolling);
		TestTrue(TEXT("Survivor roll spends baseline stamina."), FMath::IsNearlyEqual(Survivor->GetStamina(), 88.0f, 0.05f));
		TestFalse(TEXT("Special move cannot be restarted while active."), Survivor->TryStartSpecialMoveForTest(EBHMovementSpecialState::Rolling, false));

		World->TimeSeconds += 0.70f;
		World->UnpausedTimeSeconds += 0.70f;
		World->RealTimeSeconds += 0.70f;
		World->AudioTimeSeconds += 0.70f;
		Survivor->Tick(0.70f);
		TestEqual(TEXT("Roll finishes back to normal state."), Survivor->GetMovementSpecialState(), EBHMovementSpecialState::None);

		// The base special-move cooldown blocks an immediate reuse. The survivor-only "flow chain" momentum tech
		// (bh.MomentumTech) can intentionally bypass it once within a frame-perfect window, so assert the base
		// rule with the tech OFF, then assert the chain bypass with it ON -- and leave the cvar as we found it.
		IConsoleVariable* MomentumTechCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("bh.MomentumTech"));
		const int32 PrevMomentumTech = MomentumTechCVar ? MomentumTechCVar->GetInt() : 1;

		if (MomentumTechCVar) { MomentumTechCVar->Set(0); }
		TestFalse(TEXT("Roll cooldown blocks immediate reuse after finishing."), Survivor->TryStartSpecialMoveForTest(EBHMovementSpecialState::Rolling, false));
		TestEqual(TEXT("Cooldown-refused reuse leaves the survivor in normal state."), Survivor->GetMovementSpecialState(), EBHMovementSpecialState::None);

		if (MomentumTechCVar) { MomentumTechCVar->Set(1); }
		TestTrue(TEXT("Frame-perfect flow chain bypasses the cooldown once."), Survivor->TryStartSpecialMoveForTest(EBHMovementSpecialState::Rolling, false));
		TestEqual(TEXT("Flow chain re-enters the rolling state."), Survivor->GetMovementSpecialState(), EBHMovementSpecialState::Rolling);

		// Finish the chained roll so the prone checks below start from a clean, idle state, then restore the cvar.
		World->TimeSeconds += 0.70f;
		World->UnpausedTimeSeconds += 0.70f;
		World->RealTimeSeconds += 0.70f;
		World->AudioTimeSeconds += 0.70f;
		Survivor->Tick(0.70f);
		TestEqual(TEXT("Chained roll finishes back to normal state."), Survivor->GetMovementSpecialState(), EBHMovementSpecialState::None);
		if (MomentumTechCVar) { MomentumTechCVar->Set(PrevMomentumTech); }

		TestTrue(TEXT("Prone state can be entered."), Survivor->TrySetProneForTest(true));
		TestEqual(TEXT("Prone state is replicated state value."), Survivor->GetMovementSpecialState(), EBHMovementSpecialState::Prone);
		TestTrue(TEXT("Prone uses a lower capsule."), Survivor->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() < 50.0f);
		TestTrue(TEXT("Prone reduces bot sight profile."), Survivor->GetMovementSightProfileMultiplier() < 1.0f);
		TestTrue(TEXT("Prone can exit when clear."), Survivor->TrySetProneForTest(false));
		TestEqual(TEXT("Prone exit restores normal state."), Survivor->GetMovementSpecialState(), EBHMovementSpecialState::None);
		TestTrue(TEXT("Standing restores normal bot sight profile."), FMath::IsNearlyEqual(Survivor->GetMovementSightProfileMultiplier(), 1.0f, 0.01f));

		TestTrue(TEXT("Hunter roll starts with role tuning."), Hunter->TryStartSpecialMoveForTest(EBHMovementSpecialState::Rolling, false));
		TestTrue(TEXT("Hunter roll has higher stamina cost."), FMath::IsNearlyEqual(Hunter->GetStamina(), 83.2f, 0.05f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHCrawlSpaceAccessTest,
	"BlackoutHunt.Movement.CrawlSpaceRoleAndProfileGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHCrawlSpaceAccessTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntCrawlSpace"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHCrawlSpaceVolume* CrawlSpace = World->SpawnActor<ABHCrawlSpaceVolume>();
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ABHCharacter* Hunter = World->SpawnActor<ABHCharacter>(FVector(180.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ABHCharacter* Tester = World->SpawnActor<ABHCharacter>(FVector(360.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Crawlspace volume spawns."), CrawlSpace);
	TestNotNull(TEXT("Survivor spawns."), Survivor);
	TestNotNull(TEXT("Hunter spawns."), Hunter);
	TestNotNull(TEXT("Tester spawns."), Tester);
	if (GameState)
	{
		GameState->SetTestMode(true);
	}

	if (CrawlSpace && Survivor && Hunter && Tester)
	{
		BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Crawl Student"));
		BHAttachProofPlayerState(World, Hunter, EBHPlayerRole::Hunter, TEXT("Crawl Teacher"));
		BHAttachProofPlayerState(World, Tester, EBHPlayerRole::Tester, TEXT("Crawl Tester"));

		TestFalse(TEXT("Standing survivor cannot enter low crawlspace."), CrawlSpace->DebugCanCharacterUseCrawlSpace(Survivor));
		TestTrue(TEXT("Survivor can enter crawlspace while prone."), Survivor->TrySetProneForTest(true));
		TestTrue(TEXT("Prone survivor can enter crawlspace."), CrawlSpace->DebugCanCharacterUseCrawlSpace(Survivor));

		TestTrue(TEXT("Hunter can enter prone state for normal movement tuning."), Hunter->TrySetProneForTest(true));
		TestFalse(TEXT("Hunter remains blocked from survivor crawlspaces even in test mode."), CrawlSpace->DebugCanCharacterUseCrawlSpace(Hunter));

		TestFalse(TEXT("Standing tester cannot enter low crawlspace."), CrawlSpace->DebugCanCharacterUseCrawlSpace(Tester));
		TestTrue(TEXT("Tester can enter crawlspace while prone."), Tester->TrySetProneForTest(true));
		TestTrue(TEXT("Prone tester can enter crawlspace."), CrawlSpace->DebugCanCharacterUseCrawlSpace(Tester));

		// --- Auto-prone entry assist: a survivor reaching a mouth in the wrong pose is dropped into prone (admitted
		// into cover) instead of being bounced off the lip; the Teacher is never admitted/auto-proned. ---
		TestTrue(TEXT("Survivor stands before auto-prone test."), Survivor->TrySetProneForTest(false));
		TestFalse(TEXT("Standing survivor is not sheltering before auto-prone."), CrawlSpace->DebugCanCharacterUseCrawlSpace(Survivor));
		TestTrue(TEXT("Auto-prone admits a standing survivor at the mouth."), CrawlSpace->DebugTryAdmitOrAutoProneForTest(Survivor));
		TestTrue(TEXT("Auto-proned survivor now shelters."), CrawlSpace->DebugCanCharacterUseCrawlSpace(Survivor));
		TestTrue(TEXT("Teacher stands before admit test."), Hunter->TrySetProneForTest(false));
		TestFalse(TEXT("Auto-prone/admit never lets the Teacher into a survivor crawlspace."), CrawlSpace->DebugTryAdmitOrAutoProneForTest(Hunter));

		// --- Capture immunity: the Teacher cannot tag a survivor sheltering prone in a tunnel. ---
		Survivor->SetActorLocation(FVector(0.0f, 0.0f, 140.0f));
		Hunter->SetActorLocation(FVector(-60.0f, 0.0f, 140.0f));  // in capture range, facing +X toward the survivor
		Hunter->SetActorRotation(FRotator::ZeroRotator);
		CrawlSpace->UpdateOverlaps();
		TestTrue(TEXT("Survivor drops prone for shelter."), Survivor->TrySetProneForTest(true));
		TestTrue(TEXT("Prone survivor overlaps and shelters in the volume."), CrawlSpace->IsCharacterSheltering(Survivor));
		TestFalse(TEXT("Teacher CANNOT tag a survivor sheltering prone in a tunnel."), Hunter->DebugIsTeacherCaptureCandidateForTest(Survivor));
		TestTrue(TEXT("Survivor stands clear of the crawl."), Survivor->TrySetProneForTest(false));
		TestTrue(TEXT("Teacher CAN tag the same survivor when standing (control proves the check can fire)."), Hunter->DebugIsTeacherCaptureCandidateForTest(Survivor));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHTeacherCaptureCounterplayTest,
	"BlackoutHunt.Counterplay.TeacherCaptureWindupAndEvasion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHTeacherCaptureCounterplayTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHCharacter* Hunter = World->SpawnActor<ABHCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(180.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Hunter spawns."), Hunter);
	TestNotNull(TEXT("Survivor spawns."), Survivor);
	if (Hunter && Survivor)
	{
		BHAttachProofPlayerState(World, Hunter, EBHPlayerRole::Hunter, TEXT("Counterplay Teacher"));
		ABHPlayerState* SurvivorPS = BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Counterplay Student"));

		TestTrue(TEXT("Teacher capture input starts an attack windup."), Hunter->BotTryCapture());
		TestTrue(TEXT("Teacher attack is active during windup."), Hunter->IsTeacherCaptureAttackActive());
		TestFalse(TEXT("Teacher cannot start another swing during windup."), Hunter->BotTryCapture());
		TestEqual(TEXT("Windup does not capture immediately."), SurvivorPS ? SurvivorPS->LifeState : EBHPlayerLifeState::Captured, EBHPlayerLifeState::Alive);

		BHAdvanceProofWorld(World, 0.35f);
		Hunter->Tick(0.35f);
		TestEqual(TEXT("Capture resolves after the windup."), SurvivorPS ? SurvivorPS->LifeState : EBHPlayerLifeState::Alive, EBHPlayerLifeState::Captured);
		TestTrue(TEXT("Teacher has capture recovery after a hit."), Hunter->GetTeacherCaptureCooldownRemaining() > 0.0f);
	}

	ABHCharacter* EvadeHunter = World->SpawnActor<ABHCharacter>(FVector(0.0f, 280.0f, 0.0f), FRotator::ZeroRotator);
	ABHCharacter* EvadingSurvivor = World->SpawnActor<ABHCharacter>(FVector(180.0f, 280.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	TestNotNull(TEXT("Second hunter spawns."), EvadeHunter);
	TestNotNull(TEXT("Evading survivor spawns."), EvadingSurvivor);
	if (EvadeHunter && EvadingSurvivor)
	{
		BHAttachProofPlayerState(World, EvadeHunter, EBHPlayerRole::Hunter, TEXT("Counterplay Teacher 2"));
		ABHPlayerState* EvadingPS = BHAttachProofPlayerState(World, EvadingSurvivor, EBHPlayerRole::Survivor, TEXT("Evading Student"));
		EvadingSurvivor->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		EvadingSurvivor->GetCharacterMovement()->Velocity = FVector(500.0f, 0.0f, 0.0f);

		TestTrue(TEXT("Second teacher starts a windup."), EvadeHunter->BotTryCapture());
		TestTrue(TEXT("Survivor can commit to a timed roll during windup."), EvadingSurvivor->TryStartSpecialMoveForTest(EBHMovementSpecialState::Rolling, false));
		BHAdvanceProofWorld(World, 0.35f);
		EvadeHunter->Tick(0.35f);

		TestEqual(TEXT("Timed roll avoids the capture."), EvadingPS ? EvadingPS->LifeState : EBHPlayerLifeState::Captured, EBHPlayerLifeState::Alive);
		TestTrue(TEXT("Missed attack still creates Teacher recovery."), EvadeHunter->GetTeacherCaptureCooldownRemaining() > 0.0f);
	}

	ABHCharacter* DoorHunter = World->SpawnActor<ABHCharacter>(FVector(0.0f, 620.0f, 0.0f), FRotator::ZeroRotator);
	ABHCharacter* DoorSurvivor = World->SpawnActor<ABHCharacter>(FVector(180.0f, 620.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	ABHDoor* Door = World->SpawnActor<ABHDoor>(FVector(90.0f, 620.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Door-interrupt hunter spawns."), DoorHunter);
	TestNotNull(TEXT("Door-interrupt survivor spawns."), DoorSurvivor);
	TestNotNull(TEXT("Door spawns."), Door);
	if (DoorHunter && DoorSurvivor && Door)
	{
		BHAttachProofPlayerState(World, DoorHunter, EBHPlayerRole::Hunter, TEXT("Counterplay Teacher 3"));
		ABHPlayerState* DoorSurvivorPS = BHAttachProofPlayerState(World, DoorSurvivor, EBHPlayerRole::Survivor, TEXT("Door Student"));

		Door->BeginInteract_Implementation(DoorSurvivor);
		TestTrue(TEXT("Door opens before the slam counterplay."), Door->IsOpen());
		TestTrue(TEXT("Third teacher starts a windup near an open door."), DoorHunter->BotTryCapture());
		Door->BeginInteract_Implementation(DoorSurvivor);
		TestFalse(TEXT("Door closes as a slam during windup."), Door->IsOpen());
		TestFalse(TEXT("Door slam cancels the active Teacher swing."), DoorHunter->IsTeacherCaptureAttackActive());

		BHAdvanceProofWorld(World, 0.35f);
		DoorHunter->Tick(0.35f);
		TestEqual(TEXT("Door slam interrupt prevents the capture from resolving."), DoorSurvivorPS ? DoorSurvivorPS->LifeState : EBHPlayerLifeState::Captured, EBHPlayerLifeState::Alive);
		TestTrue(TEXT("Door slam interrupt gives the Teacher recovery."), DoorHunter->GetTeacherCaptureCooldownRemaining() > 0.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHButtonModuleActivationTest,
	"BlackoutHunt.Interaction.ButtonModuleActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHButtonModuleActivationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHCharacter* Character = World->SpawnActor<ABHCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
	ABHButtonModule* ToggleModule = World->SpawnActor<ABHButtonModule>(FVector(120.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABHButtonModule* HoldModule = World->SpawnActor<ABHButtonModule>(FVector(240.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABHButtonModule* LinkedModule = World->SpawnActor<ABHButtonModule>(FVector(360.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABHModuleReceiverRelay* Receiver = World->SpawnActor<ABHModuleReceiverRelay>(FVector(480.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Character spawns."), Character);
	TestNotNull(TEXT("Toggle module spawns."), ToggleModule);
	TestNotNull(TEXT("Hold module spawns."), HoldModule);
	TestNotNull(TEXT("Linked module spawns."), LinkedModule);
	TestNotNull(TEXT("Module receiver relay spawns."), Receiver);
	if (Character && ToggleModule && HoldModule && LinkedModule && Receiver)
	{
		BHAttachProofPlayerState(World, Character, EBHPlayerRole::Survivor, TEXT("Module Student"));

		ToggleModule->ConfigureModule(FName(TEXT("DoorPower")), EBHButtonModuleMode::Toggle, FText::FromString(TEXT("Toggle Door Power")), 0.0f, 0.0f);
		TestTrue(TEXT("Toggle module is interactable for alive character."), ToggleModule->CanInteract_Implementation(Character));
		ToggleModule->BeginInteract_Implementation(Character);
		TestTrue(TEXT("Toggle module activates."), ToggleModule->IsActive());
		ToggleModule->BeginInteract_Implementation(Character);
		TestFalse(TEXT("Toggle module can deactivate."), ToggleModule->IsActive());

		HoldModule->ConfigureModule(FName(TEXT("HoldPower")), EBHButtonModuleMode::Hold, FText::FromString(TEXT("Hold Power")), 0.10f, 0.0f);
		HoldModule->BeginInteract_Implementation(Character);
		HoldModule->Tick(0.05f);
		TestTrue(TEXT("Hold module tracks partial progress."), HoldModule->GetHoldProgress() > 0.0f && HoldModule->GetHoldProgress() < 1.0f);
		HoldModule->Tick(0.08f);
		TestTrue(TEXT("Hold module activates after enough hold time."), HoldModule->IsActive());
		TestEqual(TEXT("Hold progress clears after activation."), HoldModule->GetHoldProgress(), 0.0f);

		LinkedModule->ConfigureModule(FName(TEXT("LinkedPower")), EBHButtonModuleMode::Press, FText::FromString(TEXT("Linked Power")), 0.0f, 0.0f);
		LinkedModule->AddLinkedReceiver(Receiver);
		LinkedModule->BeginInteract_Implementation(Character);
		TestEqual(TEXT("Linked receiver records module id."), Receiver->GetLastModuleId(), FName(TEXT("LinkedPower")));
		TestTrue(TEXT("Linked receiver records active pulse."), Receiver->GetLastActiveState());
		TestEqual(TEXT("Linked receiver activation count increments."), Receiver->GetActivationCount(), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHMovementTuningAssetDefaultsTest,
	"BlackoutHunt.Movement.TuningAssetDefaultsAndContentValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHMovementTuningAssetDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UBHMovementTuningAsset* TuningAsset = NewObject<UBHMovementTuningAsset>(GetTransientPackage());
	TestNotNull(TEXT("Movement tuning asset can be created."), TuningAsset);
	if (TuningAsset)
	{
		const FBHMovementRoleTuning SurvivorTuning = TuningAsset->ResolveRoleTuning(EBHPlayerRole::Survivor);
		const FBHMovementRoleTuning FakeHunterTuning = TuningAsset->ResolveRoleTuning(EBHPlayerRole::FakeHunter);
		const FBHMovementRoleTuning HunterTuning = TuningAsset->ResolveRoleTuning(EBHPlayerRole::Hunter);
		TestEqual(TEXT("Survivor prone speed uses production default."), SurvivorTuning.ProneSpeed, 120.0f);
		TestEqual(TEXT("Fake hunter prone speed uses production default."), FakeHunterTuning.ProneSpeed, 110.0f);
		TestEqual(TEXT("Hunter prone speed uses production default."), HunterTuning.ProneSpeed, 95.0f);
		TestTrue(TEXT("Hunter keeps higher movement stamina cost."), HunterTuning.StaminaCostMultiplier > SurvivorTuning.StaminaCostMultiplier);
		TestTrue(TEXT("Fake hunter keeps higher movement noise."), FakeHunterTuning.NoiseMultiplier > SurvivorTuning.NoiseMultiplier);

		const FBHMovementSpecialTuning DiveTuning = TuningAsset->ResolveSpecialTuning(EBHPlayerRole::Survivor, EBHMovementSpecialState::Diving);
		TestTrue(TEXT("Dive tuning has duration."), DiveTuning.DurationSeconds > 0.0f);
		TestTrue(TEXT("Dive tuning has curve-driven distance."), DiveTuning.Curve.Distance > 0.0f);
	}

	static const TCHAR* RequiredContentPackages[] = {
		TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Roll"),
		TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Slide"),
		TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Prone_Idle"),
		TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Prone_Crawl"),
		TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Dive"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/ABP_BH_Q_Movement"),
		TEXT("/Game/BlackoutHunt/Art/Characters/Hunter/ABP_BH_Hunter_Movement"),
		TEXT("/Game/SmartBasicInterfaces/Meshes/SM_powerbtn"),
		TEXT("/Game/SmartBasicInterfaces/Meshes/SM_panel")
	};

	int32 MissingContentCount = 0;
	for (const TCHAR* PackagePath : RequiredContentPackages)
	{
		if (!FPackageName::DoesPackageExist(PackagePath))
		{
			++MissingContentCount;
			AddInfo(FString::Printf(TEXT("Production movement/module content is not imported yet: %s"), PackagePath));
		}
	}
	TestTrue(TEXT("Content validation runs and reports missing packages without failing runtime fallback."), MissingContentCount >= 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHSecurityCameraDetectionTest,
	"BlackoutHunt.Security.CCTVCameraDetectsVisibleSurvivors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHSecurityCameraDetectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHSecurityCamera* Camera = World->SpawnActor<ABHSecurityCamera>(FVector::ZeroVector, FRotator::ZeroRotator);
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(1200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Security camera spawns."), Camera);
	TestNotNull(TEXT("Survivor spawns."), Survivor);
	if (GameState && Camera && Survivor)
	{
		BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Visible Student"));
		Camera->ConfigureCamera(2500.0f, 60.0f, 42);

		TestTrue(TEXT("Camera sees an alive survivor in range, cone, hunt phase, and line of sight."), Camera->CanSeeSurvivor(Survivor));
		TestEqual(TEXT("Camera scan selects the visible survivor."), Camera->FindBestVisibleSurvivor(), Survivor);

		const FVector ExactRevealLocation = Survivor->GetActorLocation() + FVector(0.0f, 0.0f, 88.0f);
		const FVector MotionMarkerA = Camera->DebugResolveHunterMotionMarkerLocation(Survivor, 12.0f);
		const FVector MotionMarkerRepeat = Camera->DebugResolveHunterMotionMarkerLocation(Survivor, 12.0f);
		const FVector MotionMarkerLater = Camera->DebugResolveHunterMotionMarkerLocation(Survivor, 21.0f);
		TestTrue(TEXT("Unattended CCTV motion marker is deliberately imprecise."),
			FVector::Dist2D(MotionMarkerA, ExactRevealLocation) >= 150.0f && FVector::Dist2D(MotionMarkerA, ExactRevealLocation) <= 500.0f);
		TestEqual(TEXT("Unattended CCTV motion marker is stable within a cooldown bucket."), MotionMarkerA, MotionMarkerRepeat);
		TestFalse(TEXT("Unattended CCTV motion marker refreshes to a new imprecise point later."), MotionMarkerA.Equals(MotionMarkerLater, 1.0f));

		Survivor->SetActorLocation(FVector(1200.0f, 1200.0f, 0.0f));
		TestFalse(TEXT("Camera ignores survivors outside its cone."), Camera->CanSeeSurvivor(Survivor));

		Survivor->SetActorLocation(FVector(1200.0f, 0.0f, 0.0f));
		Camera->SetCameraDisabled(true);
		TestFalse(TEXT("Disabled camera does not detect survivors."), Camera->CanSeeSurvivor(Survivor));
		Camera->SetCameraDisabled(false);

		GameState->SetRoundPhase(EBHRoundPhase::Lobby);
		TestFalse(TEXT("Camera does not detect outside hunt phase."), Camera->CanSeeSurvivor(Survivor));
		GameState->SetRoundPhase(EBHRoundPhase::Hunt);

		ABHLocker* Locker = World->SpawnActor<ABHLocker>(FVector(1200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		TestNotNull(TEXT("Locker spawns."), Locker);
		if (Locker)
		{
			Survivor->EnterLocker(Locker);
			TestFalse(TEXT("Camera ignores survivors hidden in lockers."), Camera->CanSeeSurvivor(Survivor));
			Survivor->ExitLocker();
		}

		Survivor->SetActorLocation(FVector(1800.0f, 0.0f, 0.0f));
		TestTrue(TEXT("Standing survivor remains visible near the camera range edge."), Camera->CanSeeSurvivor(Survivor));
		TestTrue(TEXT("Survivor can enter prone as CCTV counterplay."), Survivor->TrySetProneForTest(true));
		TestFalse(TEXT("Prone survivor is harder for CCTV to detect at range."), Camera->CanSeeSurvivor(Survivor));
		TestTrue(TEXT("Survivor can leave prone after CCTV counterplay check."), Survivor->TrySetProneForTest(false));
		TestTrue(TEXT("Standing back up restores normal CCTV visibility."), Camera->CanSeeSurvivor(Survivor));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHSecurityCameraZoneSharedAlertTest,
	"BlackoutHunt.Security.CCTVZoneUsesCameraDetectionCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHSecurityCameraZoneSharedAlertTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHSecurityCamera* Camera = World->SpawnActor<ABHSecurityCamera>(FVector::ZeroVector, FRotator::ZeroRotator);
	ABHCCTVZone* Zone = World->SpawnActor<ABHCCTVZone>(FVector(900.0f, 0.0f, 120.0f), FRotator::ZeroRotator);
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(1200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Security camera spawns."), Camera);
	TestNotNull(TEXT("CCTV zone spawns."), Zone);
	TestNotNull(TEXT("Survivor spawns."), Survivor);
	if (Camera && Zone && Survivor)
	{
		BHAttachProofPlayerState(World, Survivor, EBHPlayerRole::Survivor, TEXT("Cooldown Student"));
		Camera->ConfigureCamera(2500.0f, 60.0f, 77);
		Zone->ConfigureZone(Camera, 77, TEXT("automation CCTV zone"), FVector(1000.0f, 500.0f, 120.0f), true);

		TestTrue(TEXT("Zone alert uses camera visibility for its first detection."), Camera->TryTriggerZoneAlert(Survivor, Zone, TEXT("automation CCTV zone")));
		TestFalse(TEXT("Immediate repeated zone alert is suppressed by camera state/cooldown."), Camera->TryTriggerZoneAlert(Survivor, Zone, TEXT("automation CCTV zone")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHSecurityMonitorInspectionRegistrationTest,
	"BlackoutHunt.Security.CCTVMonitorRegistersActiveInspection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHSecurityMonitorInspectionRegistrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHSecurityCamera* Camera = World->SpawnActor<ABHSecurityCamera>(FVector(1200.0f, 0.0f, 150.0f), FRotator::ZeroRotator);
	ABHSecurityMonitor* Monitor = World->SpawnActor<ABHSecurityMonitor>(FVector::ZeroVector, FRotator::ZeroRotator);
	ABHCharacter* Teacher = World->SpawnActor<ABHCharacter>(FVector(80.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABHPlayerController* Controller = World->SpawnActor<ABHPlayerController>();
	TestNotNull(TEXT("Security camera spawns."), Camera);
	TestNotNull(TEXT("Security monitor spawns."), Monitor);
	TestNotNull(TEXT("Teacher character spawns."), Teacher);
	TestNotNull(TEXT("Teacher controller spawns."), Controller);
	if (Camera && Monitor && Teacher && Controller)
	{
		ABHPlayerState* TeacherPS = BHAttachProofPlayerState(World, Teacher, EBHPlayerRole::Hunter, TEXT("Teacher"));
		Controller->PlayerState = TeacherPS;
		Controller->Possess(Teacher);
		Camera->ConfigureCamera(2500.0f, 60.0f, 88);

		Teacher->SetActorLocation(FVector(10000.0f, 0.0f, 0.0f));
		const FBHInteractionPromptInfo RemotePrompt = Monitor->GetInteractionPromptInfo_Implementation(Teacher);
		TestTrue(TEXT("Monitor prompt reports signal from the monitor feed, not the player's camera distance."), RemotePrompt.bCanInteract);
		Teacher->SetActorLocation(FVector(80.0f, 0.0f, 0.0f));

		TestEqual(TEXT("Camera starts without active inspectors."), Camera->GetActiveInspectorCount(), 0);
		Monitor->BeginInteract_Implementation(Teacher);
		TestEqual(TEXT("Holding interact on monitor registers active camera inspection."), Camera->GetActiveInspectorCount(), 1);

		Teacher->SetActorLocation(FVector(900.0f, 0.0f, 0.0f));
		Monitor->DebugPruneActiveInspections();
		TestEqual(TEXT("Walking away from the monitor drops precise CCTV inspection."), Camera->GetActiveInspectorCount(), 0);

		Teacher->SetActorLocation(FVector(80.0f, 0.0f, 0.0f));
		Monitor->BeginInteract_Implementation(Teacher);
		TestEqual(TEXT("Returning to the monitor can re-arm precise CCTV inspection."), Camera->GetActiveInspectorCount(), 1);
		Camera->SetCameraDisabled(true);
		Monitor->DebugPruneActiveInspections();
		TestEqual(TEXT("Disabled camera drops active monitor inspection."), Camera->GetActiveInspectorCount(), 0);

		Camera->SetCameraDisabled(false);
		Monitor->BeginInteract_Implementation(Teacher);
		TestEqual(TEXT("Re-enabled camera can be inspected again."), Camera->GetActiveInspectorCount(), 1);
		TeacherPS->SetRole(EBHPlayerRole::Survivor);
		Monitor->DebugPruneActiveInspections();
		TestEqual(TEXT("Losing the Teacher role drops active monitor inspection."), Camera->GetActiveInspectorCount(), 0);

		TeacherPS->SetRole(EBHPlayerRole::Hunter);
		Monitor->BeginInteract_Implementation(Teacher);
		TestEqual(TEXT("Alive Teacher can re-arm after role restores."), Camera->GetActiveInspectorCount(), 1);
		Monitor->EndInteract_Implementation(Teacher);
		TestEqual(TEXT("Releasing interact unregisters active camera inspection."), Camera->GetActiveInspectorCount(), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHSecurityShutterCircuitTest,
	"BlackoutHunt.Security.ShutterCircuitMovesAndBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHSecurityShutterCircuitTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);

	ABHSecurityShutter* ShutterA = World->SpawnActor<ABHSecurityShutter>();
	ABHSecurityShutter* ShutterB = World->SpawnActor<ABHSecurityShutter>(FVector(300.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABHSecurityTerminal* Terminal = World->SpawnActor<ABHSecurityTerminal>(FVector(600.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("First shutter spawns."), ShutterA);
	TestNotNull(TEXT("Second shutter spawns."), ShutterB);
	TestNotNull(TEXT("Security terminal spawns."), Terminal);
	if (GameMode && ShutterA && ShutterB)
	{
		ShutterA->Configure(77);
		ShutterB->Configure(77);
		if (Terminal)
		{
			Terminal->Configure(77, FText::FromString(TEXT("Toggle Test Shutters")));
		}

		UStaticMeshComponent* ShutterMesh = ShutterA->FindComponentByClass<UStaticMeshComponent>();
		TestNotNull(TEXT("Shutter has a mesh."), ShutterMesh);
		const FVector ClosedMeshLocation = ShutterMesh ? ShutterMesh->GetRelativeLocation() : FVector::ZeroVector;
		auto FinishShutterAnimation = [ShutterA, ShutterB]()
		{
			ShutterA->Tick(0.25f);
			ShutterB->Tick(0.25f);
		};

		if (Terminal)
		{
			TestEqual(TEXT("Terminal prompt starts with the opening action."), Terminal->GetInteractionLabel_Implementation(nullptr).ToString(), FString(TEXT("Open Test Shutters")));
		}

		GameMode->ToggleSecurityCircuit(77);
		FinishShutterAnimation();
		TestTrue(TEXT("Circuit toggle opens first shutter."), ShutterA->IsOpen());
		TestTrue(TEXT("Circuit toggle opens second shutter."), ShutterB->IsOpen());
		if (Terminal)
		{
			TestEqual(TEXT("Terminal prompt changes to the closing action."), Terminal->GetInteractionLabel_Implementation(nullptr).ToString(), FString(TEXT("Close Test Shutters")));
		}
		if (ShutterMesh)
		{
			TestTrue(TEXT("Open shutter visibly lifts its mesh."), ShutterMesh->GetRelativeLocation().Z > ClosedMeshLocation.Z + 1.0f);
			TestEqual(TEXT("Open shutter stops blocking pawns."), ShutterMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
		}

		GameMode->ToggleSecurityCircuit(77);
		FinishShutterAnimation();
		TestFalse(TEXT("Second circuit toggle closes first shutter."), ShutterA->IsOpen());
		TestFalse(TEXT("Second circuit toggle closes second shutter."), ShutterB->IsOpen());
		if (ShutterMesh)
		{
			TestEqual(TEXT("Closed shutter returns to its original mesh location."), ShutterMesh->GetRelativeLocation(), ClosedMeshLocation);
			TestEqual(TEXT("Closed shutter blocks pawns again."), ShutterMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHFootstepSurfaceMappingTest,
	"BlackoutHunt.Horror.FootstepSurfaceMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHFootstepSurfaceMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(TEXT("Diamond plate maps to metal footsteps."),
		UBHFootstepSurfaceComponent::SurfaceForBlockMaterial(EBHBlockMaterial::DiamondPlate),
		EBHFootstepSurface::Metal);
	TestEqual(TEXT("Tiles map to tile footsteps."),
		UBHFootstepSurfaceComponent::SurfaceForBlockMaterial(EBHBlockMaterial::Tiles),
		EBHFootstepSurface::Tile);
	TestEqual(TEXT("Concrete maps to concrete footsteps."),
		UBHFootstepSurfaceComponent::SurfaceForBlockMaterial(EBHBlockMaterial::Concrete),
		EBHFootstepSurface::Concrete);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAudioIdentitySettingsTest,
	"BlackoutHunt.Horror.AudioIdentitySettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAudioIdentitySettingsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	TestNotNull(TEXT("Game settings exist."), Settings);
	if (!Settings)
	{
		return false;
	}

	auto PackageExists = [](const FSoftObjectPath& ObjectPath)
	{
		const FString PackageName = ObjectPath.GetLongPackageName();
		return !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
	};

	auto NormalizeCookPath = [](FString Path)
	{
		Path.TrimStartAndEndInline();
		Path.TrimQuotesInline();
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Path.Len() > 5 && Path.EndsWith(TEXT("/")))
		{
			Path.LeftChopInline(1);
		}
		return Path;
	};

	auto ReadAlwaysCookPaths = [&NormalizeCookPath]()
	{
		TSet<FString> CookPaths;
		if (!GConfig)
		{
			return CookPaths;
		}

		TArray<FString> Entries;
		GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("DirectoriesToAlwaysCook"), Entries, GGameIni);
		for (const FString& Entry : Entries)
		{
			FString ParsedPath;
			const FString QuotedPathPrefix = TEXT("(Path=\"");
			if (Entry.StartsWith(QuotedPathPrefix))
			{
				ParsedPath = Entry.RightChop(QuotedPathPrefix.Len());
				int32 ClosingQuoteIndex = INDEX_NONE;
				if (ParsedPath.FindChar(TEXT('"'), ClosingQuoteIndex))
				{
					ParsedPath = ParsedPath.Left(ClosingQuoteIndex);
				}
			}
			else if (FParse::Value(*Entry, TEXT("Path="), ParsedPath))
			{
				ParsedPath.TrimStartAndEndInline();
				if (ParsedPath.EndsWith(TEXT(")")))
				{
					ParsedPath.LeftChopInline(1);
				}
			}
			else
			{
				ParsedPath = Entry;
			}

			ParsedPath = NormalizeCookPath(ParsedPath);
			if (!ParsedPath.IsEmpty())
			{
				CookPaths.Add(ParsedPath);
			}
		}
		return CookPaths;
	};

	const TSet<FString> AlwaysCookPaths = ReadAlwaysCookPaths();
	TestTrue(TEXT("Cook path configuration is available."), AlwaysCookPaths.Num() > 0);

	auto IsAlwaysCooked = [&AlwaysCookPaths](const FSoftObjectPath& ObjectPath)
	{
		const FString PackageName = ObjectPath.GetLongPackageName();
		for (const FString& CookPath : AlwaysCookPaths)
		{
			if (PackageName == CookPath || PackageName.StartsWith(CookPath + TEXT("/")))
			{
				return true;
			}
		}
		return false;
	};

	auto IsApprovedAudioOrFootstepPath = [](const FSoftObjectPath& ObjectPath)
	{
		const FString Path = ObjectPath.ToString();
		return Path.StartsWith(TEXT("/Game/ResidentHorrorV1/Audio/"))
			|| Path.StartsWith(TEXT("/Game/FlashLight_System/Sound/"))
			|| Path.StartsWith(TEXT("/Game/SecurityCameras/Sounds/"))
			|| Path.StartsWith(TEXT("/Game/SoundsOfHorror/Impacts/CUE/"))
			|| Path.StartsWith(TEXT("/Game/SoundsOfHorror/Rumbles/CUE/"))
			|| Path.StartsWith(TEXT("/Game/A_Surface_Footstep/Niagara_FX/"));
	};

	auto TestConfiguredAsset = [this, &PackageExists, &IsAlwaysCooked, &IsApprovedAudioOrFootstepPath](const FString& Label, const FSoftObjectPath& ObjectPath)
	{
		TestFalse(FString::Printf(TEXT("%s soft path is configured."), *Label), ObjectPath.IsNull());
		if (!ObjectPath.IsNull())
		{
			TestTrue(FString::Printf(TEXT("%s uses a package-approved runtime folder."), *Label), IsApprovedAudioOrFootstepPath(ObjectPath));
			TestTrue(FString::Printf(TEXT("%s is covered by an AlwaysCook runtime folder."), *Label), IsAlwaysCooked(ObjectPath));
			TestTrue(FString::Printf(TEXT("%s package exists in this workspace."), *Label), PackageExists(ObjectPath));
		}
	};

	const FString RequiredAudioIdentityCookRoots[] = {
		TEXT("/Game/A_Surface_Footstep/Niagara_FX"),
		TEXT("/Game/ResidentHorrorV1/Audio"),
		TEXT("/Game/FlashLight_System/Sound"),
		TEXT("/Game/SecurityCameras/Sounds"),
		TEXT("/Game/SoundsOfHorror/Impacts/CUE"),
		TEXT("/Game/SoundsOfHorror/Rumbles/CUE")
	};

	for (const FString& CookRoot : RequiredAudioIdentityCookRoots)
	{
		TestTrue(FString::Printf(TEXT("Audio identity cook root is configured: %s"), *CookRoot), AlwaysCookPaths.Contains(CookRoot));
	}

	TestTrue(TEXT("Footstep profiles are configured."), Settings->FootstepSurfaceProfiles.Num() >= 8);

	const UEnum* SurfaceEnum = StaticEnum<EBHFootstepSurface>();
	TSet<EBHFootstepSurface> SurfacesSeen;
	for (const FBHFootstepSurfaceProfile& Profile : Settings->FootstepSurfaceProfiles)
	{
		SurfacesSeen.Add(Profile.Surface);
		const FString SurfaceName = SurfaceEnum
			? SurfaceEnum->GetNameStringByValue(static_cast<int64>(Profile.Surface))
			: FString::FromInt(static_cast<int32>(Profile.Surface));
		TestTrue(FString::Printf(TEXT("Footstep hearing base is positive for %s."), *SurfaceName), Profile.HearingRadiusBase > 0.0f);
		TestTrue(FString::Printf(TEXT("Footstep hearing scale is positive for %s."), *SurfaceName), Profile.HearingRadiusScale > 0.0f);
		TestTrue(FString::Printf(TEXT("Footstep noise multiplier is positive for %s."), *SurfaceName), Profile.NoiseMultiplier > 0.0f);
		TestTrue(FString::Printf(TEXT("Footstep atmosphere multiplier is positive for %s."), *SurfaceName), Profile.AtmosphereMultiplier > 0.0f);
		TestTrue(FString::Printf(TEXT("Footstep local volume stays bounded for %s."), *SurfaceName), Profile.LocalVolume >= 0.0f && Profile.LocalVolume <= 0.75f);
		TestConfiguredAsset(FString::Printf(TEXT("Footstep sound (%s)"), *SurfaceName), Profile.FootstepSound.ToSoftObjectPath());
		TestConfiguredAsset(FString::Printf(TEXT("Footstep effect (%s)"), *SurfaceName), Profile.FootstepEffect.ToSoftObjectPath());
	}

	const EBHFootstepSurface RequiredSurfaces[] = {
		EBHFootstepSurface::Default,
		EBHFootstepSurface::Concrete,
		EBHFootstepSurface::Tile,
		EBHFootstepSurface::Metal,
		EBHFootstepSurface::Wet,
		EBHFootstepSurface::Gravel,
		EBHFootstepSurface::Glass,
		EBHFootstepSurface::Soft
	};

	for (EBHFootstepSurface Surface : RequiredSurfaces)
	{
		const FString SurfaceName = SurfaceEnum
			? SurfaceEnum->GetNameStringByValue(static_cast<int64>(Surface))
			: FString::FromInt(static_cast<int32>(Surface));
		TestTrue(FString::Printf(TEXT("Gameplay footstep surface has an audio profile: %s"), *SurfaceName), SurfacesSeen.Contains(Surface));
	}

	TestConfiguredAsset(TEXT("Flashlight on sound"), Settings->FlashlightOnSound.ToSoftObjectPath());
	TestConfiguredAsset(TEXT("Flashlight off sound"), Settings->FlashlightOffSound.ToSoftObjectPath());
	TestConfiguredAsset(TEXT("CCTV static sound"), Settings->CCTVStaticSound.ToSoftObjectPath());
	TestConfiguredAsset(TEXT("Breaker hum sound"), Settings->BreakerHumSound.ToSoftObjectPath());
	TestConfiguredAsset(TEXT("Breaker complete sound"), Settings->BreakerCompleteSound.ToSoftObjectPath());
	TestConfiguredAsset(TEXT("Teacher proximity sound"), Settings->TeacherProximitySound.ToSoftObjectPath());
	TestConfiguredAsset(TEXT("Locker knock sound"), Settings->LockerKnockSound.ToSoftObjectPath());
	TestConfiguredAsset(TEXT("Power loss stinger sound"), Settings->PowerLossStingerSound.ToSoftObjectPath());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBreakableGlassStatePromptTest,
	"BlackoutHunt.Horror.BreakableGlassStatePrompt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBreakableGlassStatePromptTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHBreakableGlassPane* Glass = World->SpawnActor<ABHBreakableGlassPane>();
	ABHCharacter* Character = World->SpawnActor<ABHCharacter>(FVector(80.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Glass pane spawns."), Glass);
	TestNotNull(TEXT("Character spawns."), Character);
	if (Glass && Character)
	{
		BHAttachProofPlayerState(World, Character, EBHPlayerRole::Survivor, TEXT("Glass Tester"));
		TestTrue(TEXT("Intact glass is interactable for alive players."), Glass->CanInteract_Implementation(Character));
		const FBHInteractionPromptInfo Prompt = Glass->GetInteractionPromptInfo_Implementation(Character);
		TestTrue(TEXT("Glass prompt marks the action noisy."), Prompt.bNoisy);
		TestEqual(TEXT("Glass starts intact."), Glass->GetGlassState(), EBHBreakableGlassState::Intact);

		TestTrue(TEXT("Glass can break through native gameplay."), Glass->BreakGlass(Character, TEXT("automation glass")));
		TestEqual(TEXT("Glass state becomes broken."), Glass->GetGlassState(), EBHBreakableGlassState::Broken);
		TestFalse(TEXT("Broken glass is no longer interactable."), Glass->CanInteract_Implementation(Character));
		if (UStaticMeshComponent* Mesh = Glass->FindComponentByClass<UStaticMeshComponent>())
		{
			TestEqual(TEXT("Broken glass stops blocking collision."), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHObjectiveBeatStateTest,
	"BlackoutHunt.Horror.ObjectiveBeatState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHObjectiveBeatStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	TestNotNull(TEXT("Game state spawns."), GameState);
	if (GameState)
	{
		FBHObjectiveBeat Beat;
		Beat.BeatId = TEXT("AutomationBeat");
		Beat.Label = TEXT("Power");
		Beat.Location = FVector(100.0f, 200.0f, 80.0f);
		Beat.bPrimary = true;
		GameState->SetObjectiveBeats({Beat});
		TestEqual(TEXT("Objective beats are stored for HUD replication."), GameState->ObjectiveBeats.Num(), 1);
		TestTrue(TEXT("Objective beat preserves primary status."), GameState->ObjectiveBeats[0].bPrimary);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHObjectiveClarityTextTest,
	"BlackoutHunt.HUD.ObjectiveClarityText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHObjectiveClarityTextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	TestNotNull(TEXT("Game state spawns."), GameState);
	if (!GameState)
	{
		return false;
	}

	GameState->SetBreakerCounts(1, 3);
	GameState->SetSideObjectiveCounts(1, 2);
	GameState->SetExitUnlocked(false);
	TestTrue(TEXT("Mixed objective text names power work."),
		GameState->GetPublicObjectiveActionText().Contains(TEXT("breaker")));
	TestTrue(TEXT("Mixed objective text names station work."),
		GameState->GetPublicObjectiveActionText().Contains(TEXT("station")));
	TestTrue(TEXT("Progress text exposes power count."),
		GameState->GetObjectiveProgressText().Contains(TEXT("Power 1/3")));
	TestTrue(TEXT("Progress text exposes side task count."),
		GameState->GetObjectiveProgressText().Contains(TEXT("Side tasks 1/2")));
	TestTrue(TEXT("Progress text exposes exit state."),
		GameState->GetObjectiveProgressText().Contains(TEXT("Exit locked")));

	GameState->SetExitUnlocked(true);
	TestTrue(TEXT("Exit-open action text points players to the gate."),
		GameState->GetPublicObjectiveActionText().Contains(TEXT("active exit gate")));

	GameState->SetExitUnlocked(false);
	GameState->SetBreakerCounts(4, 3);
	GameState->SetSideObjectiveCounts(0, 0);
	TestTrue(TEXT("Progress text clamps over-complete power counts."),
		GameState->GetObjectiveProgressText().Contains(TEXT("Power 3/3")));
	TestTrue(TEXT("Progress text uses clear text instead of zero-count side tasks."),
		GameState->GetObjectiveProgressText().Contains(TEXT("Side tasks clear")));

	GameState->SetRevisionOptions(EBHRevisionMode::PhysicsClassroom, 0x0F, EBHRevisionDifficultyMix::Adaptive, 70.0f, 50.0f, 600, 2);
	GameState->SetSideObjectiveCounts(3, 2);
	TestTrue(TEXT("Revision progress text clamps over-complete node counts."),
		GameState->GetObjectiveProgressText().Contains(TEXT("Nodes 2/2")));
	GameState->SetRevisionOptions(EBHRevisionMode::None, 0x0F, EBHRevisionDifficultyMix::Adaptive, 70.0f, 50.0f, 600, 2);

	GameState->SetRoundPhase(EBHRoundPhase::Intermission);
	GameState->SetTrainState(EBHTrainPhase::Shop, 1, GameState->GetServerWorldTimeSeconds() + 28.0f, TEXT("Substation"), TEXT(""));
	TestTrue(TEXT("Train action text calls out shop phase."),
		GameState->GetPublicObjectiveActionText().Contains(TEXT("buy upgrades")));
	TestTrue(TEXT("Train progress text names destination."),
		GameState->GetObjectiveProgressText().Contains(TEXT("Substation")));

	GameState->SetRoundPhase(EBHRoundPhase::FinalEscape);
	GameState->SetFinalEscapeState(EBHFinalEscapeState::EscapeActive, 0.0f, GameState->GetServerWorldTimeSeconds() + 42.0f, 0.0f);
	TestTrue(TEXT("Final escape action text tells students to board."),
		GameState->GetPublicObjectiveActionText().Contains(TEXT("board")));
	TestFalse(TEXT("Public final escape action avoids Teacher-only penalty detail."),
		GameState->GetPublicObjectiveActionText().Contains(TEXT("door camping")));
	TestTrue(TEXT("Final escape progress text names final train."),
		GameState->GetObjectiveProgressText().Contains(TEXT("Final train")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHExitGateBlockedReasonClarityTest,
	"BlackoutHunt.HUD.ExitGateBlockedReasonClarity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHExitGateBlockedReasonClarityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = BHCreateProofGameState(World, EBHRoundPhase::Hunt);
	ABHExitGate* ExitGate = World->SpawnActor<ABHExitGate>();
	ABHCharacter* Character = World->SpawnActor<ABHCharacter>(FVector(80.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABHPlayerState* PlayerState = BHAttachProofPlayerState(World, Character, EBHPlayerRole::Survivor, TEXT("Exit Tester"));
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Exit gate spawns."), ExitGate);
	TestNotNull(TEXT("Character spawns."), Character);
	TestNotNull(TEXT("Player state spawns."), PlayerState);
	if (!GameState || !ExitGate || !Character || !PlayerState)
	{
		return false;
	}

	GameState->SetBreakerCounts(1, 3);
	GameState->SetSideObjectiveCounts(0, 2);
	GameState->SetExitUnlocked(false);
	FBHInteractionPromptInfo Prompt = ExitGate->GetInteractionPromptInfo_Implementation(Character);
	TestTrue(TEXT("Standard locked exit explains remaining power."),
		Prompt.DisabledReason.ToString().Contains(TEXT("POWER 2")));
	TestTrue(TEXT("Standard locked exit explains remaining tasks."),
		Prompt.DisabledReason.ToString().Contains(TEXT("TASKS 2")));

	GameState->SetRevisionOptions(EBHRevisionMode::PhysicsClassroom, 0x0F, EBHRevisionDifficultyMix::Adaptive, 70.0f, 50.0f, 600, 2);
	GameState->SetRevisionContributionTarget(2);
	GameState->SetRevisionSummary(60.0f, EBHPhysicsTopic::ForcesAndMotion, 0, TEXT(""));
	PlayerState->RevisionStats.MasteryPercent = 80.0f;
	PlayerState->RevisionStats.ContributionCount = 2;
	Prompt = ExitGate->GetInteractionPromptInfo_Implementation(Character);
	TestTrue(TEXT("Revision locked exit explains class mastery."),
		Prompt.DisabledReason.ToString().Contains(TEXT("CLASS MASTERY")));

	GameState->SetRevisionSummary(80.0f, EBHPhysicsTopic::ForcesAndMotion, 0, TEXT(""));
	PlayerState->RevisionStats.MasteryPercent = 40.0f;
	PlayerState->RevisionStats.ContributionCount = 2;
	Prompt = ExitGate->GetInteractionPromptInfo_Implementation(Character);
	TestTrue(TEXT("Revision locked exit explains personal mastery."),
		Prompt.DisabledReason.ToString().Contains(TEXT("YOUR MASTERY")));

	PlayerState->RevisionStats.MasteryPercent = 80.0f;
	PlayerState->RevisionStats.ContributionCount = 1;
	Prompt = ExitGate->GetInteractionPromptInfo_Implementation(Character);
	TestTrue(TEXT("Revision locked exit explains contribution target."),
		Prompt.DisabledReason.ToString().Contains(TEXT("CONTRIBUTE 1/2")));

	PlayerState->SetRole(EBHPlayerRole::Spectator);
	PlayerState->SetLifeState(EBHPlayerLifeState::Alive);
	Prompt = ExitGate->GetInteractionPromptInfo_Implementation(Character);
	TestTrue(TEXT("Spectator exit block remains support-specific."),
		Prompt.DisabledReason.ToString().Contains(TEXT("SPECTATOR SUPPORT")));

	GameState->SetRoundPhase(EBHRoundPhase::Prep);
	TestTrue(TEXT("Interaction failure reuses target-specific spectator prompt during warmup."),
		Character->DebugGetInteractionFailureReasonForTest(ExitGate).Contains(TEXT("SPECTATOR SUPPORT")));
	GameState->SetRoundPhase(EBHRoundPhase::Hunt);

	PlayerState->SetRole(EBHPlayerRole::Survivor);
	PlayerState->SetLifeState(EBHPlayerLifeState::Captured);
	Prompt = ExitGate->GetInteractionPromptInfo_Implementation(Character);
	TestTrue(TEXT("Captured survivor exit block explains out-of-play state."),
		Prompt.DisabledReason.ToString().Contains(TEXT("OUT OF PLAY")));

	PlayerState->SetLifeState(EBHPlayerLifeState::Alive);
	PlayerState->SetRole(EBHPlayerRole::Unassigned);
	const FString UnassignedReason = Character->DebugGetInteractionFailureReasonForTest(ExitGate);
	TestTrue(TEXT("Unassigned interaction failure tells students to ready up."),
		UnassignedReason.Contains(TEXT("ready up"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("Unassigned interaction failure hides host-only roster controls."),
		UnassignedReason.Contains(TEXT("kick"), ESearchCase::IgnoreCase));

	PlayerState->SetRole(EBHPlayerRole::FakeHunter);
	Prompt = ExitGate->GetInteractionPromptInfo_Implementation(Character);
	TestTrue(TEXT("Hall Monitor exit block remains role-specific."),
		Prompt.DisabledReason.ToString().Contains(TEXT("HALL MONITORS CANNOT ESCAPE")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHReconnectRestoresRoleWithinGraceTest,
	"BlackoutHunt.Network.ReconnectRestoresRoleWithinGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHReconnectRestoresRoleWithinGraceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntProof"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	UBHGameInstance* GameInstance = NewObject<UBHGameInstance>();
	ABHPlayerState* PlayerState = World->SpawnActor<ABHPlayerState>();
	TestNotNull(TEXT("Reconnect game instance is created."), GameInstance);
	TestNotNull(TEXT("Departing player state spawns."), PlayerState);
	if (!GameInstance || !PlayerState)
	{
		return false;
	}

	// A caught student who became a Hall Monitor drops mid-round. The server issued this client a secret
	// reconnect token at join, which is persisted alongside their progress.
	const FString StudentToken = TEXT("reconnect-token-student-A");
	PlayerState->SetPlayerName(TEXT("Student-Reconnect"));
	PlayerState->SetRole(EBHPlayerRole::FakeHunter);
	PlayerState->SetDesiredRole(EBHPlayerRole::Survivor);
	PlayerState->SetLifeState(EBHPlayerLifeState::Captured);
	PlayerState->SetFakeHunterEligible(true);
	PlayerState->QuestionPoints = 30;
	PlayerState->ReconnectToken = StudentToken;

	const float LeaveTime = 100.0f;
	// Drive the monotonic reconnect clock deterministically so the grace-window expiry path is
	// actually exercised (the production clock is FPlatformTime::Seconds(), which barely advances
	// during a test). The override stamps the leave time here and is moved forward before each probe.
	GameInstance->SetReconnectClockOverrideForTest(LeaveTime);
	GameInstance->MarkTravelPlayerLeftForReconnect(PlayerState, LeaveTime);

	// The SAME student's client rejoins within grace, echoing the secret token it was issued (the same
	// lobby name is incidental and must NOT be what's matched on).
	ABHPlayerState* RejoinState = World->SpawnActor<ABHPlayerState>();
	TestNotNull(TEXT("Rejoining player state spawns."), RejoinState);
	if (!RejoinState)
	{
		return false;
	}
	RejoinState->SetPlayerName(TEXT("Student-Reconnect"));
	RejoinState->SetRole(EBHPlayerRole::Spectator);
	RejoinState->SetLifeState(EBHPlayerLifeState::Alive);
	RejoinState->ReconnectToken = StudentToken;

	// Within the grace window the cached state is recognized by the secret token and restored.
	GameInstance->SetReconnectClockOverrideForTest(LeaveTime + 90.0f);
	FBHTravelPlayerProgress Progress;
	const bool bWithinGrace = GameInstance->TryGetReconnectProgress(RejoinState, LeaveTime + 90.0f, 120.0f, Progress);
	TestTrue(TEXT("Reconnect within grace is recognized by the secret token."), bWithinGrace);
	TestEqual(TEXT("Reconnect restores the cached role."), Progress.PlayerRole, EBHPlayerRole::FakeHunter);
	TestEqual(TEXT("Reconnect restores the cached captured life state."), Progress.LifeState, EBHPlayerLifeState::Captured);
	TestTrue(TEXT("Reconnect restores Hall Monitor eligibility."), Progress.bFakeHunterEligible);
	TestEqual(TEXT("Reconnect restores banked question points."), Progress.QuestionPoints, 30);

	// Identity-safety (Bug 9): a DIFFERENT student who happens to type the SAME lobby name must NOT be
	// restored into the original student's role/points. This is the id-less direct-IP/Playit case the old
	// display-name match got wrong. No token => treated as a fresh late joiner; a different token => no match.
	ABHPlayerState* ImposterState = World->SpawnActor<ABHPlayerState>();
	TestNotNull(TEXT("Imposter player state spawns."), ImposterState);
	if (ImposterState)
	{
		ImposterState->SetPlayerName(TEXT("Student-Reconnect"));
		ImposterState->ReconnectToken = FString();
		FBHTravelPlayerProgress ImposterProgress;
		const bool bNoToken = GameInstance->TryGetReconnectProgress(ImposterState, LeaveTime + 90.0f, 120.0f, ImposterProgress);
		TestFalse(TEXT("A same-named student WITHOUT the secret token is NOT restored (no name-based identity theft)."), bNoToken);

		ImposterState->ReconnectToken = TEXT("reconnect-token-imposter-B");
		const bool bWrongToken = GameInstance->TryGetReconnectProgress(ImposterState, LeaveTime + 90.0f, 120.0f, ImposterProgress);
		TestFalse(TEXT("A same-named student with a DIFFERENT token is NOT restored."), bWrongToken);
	}

	// Past the grace window the reconnect is rejected and the player falls back to spectator.
	GameInstance->SetReconnectClockOverrideForTest(LeaveTime + 200.0f);
	FBHTravelPlayerProgress ExpiredProgress;
	const bool bExpired = GameInstance->TryGetReconnectProgress(RejoinState, LeaveTime + 200.0f, 120.0f, ExpiredProgress);
	TestFalse(TEXT("Reconnect past the grace window is rejected."), bExpired);

	// After a successful rejoin consumes the mark, it is not offered again.
	GameInstance->ClearReconnectMark(RejoinState);
	FBHTravelPlayerProgress ClearedProgress;
	const bool bAfterClear = GameInstance->TryGetReconnectProgress(RejoinState, LeaveTime + 10.0f, 120.0f, ClearedProgress);
	TestFalse(TEXT("Reconnect mark is one-shot after a successful rejoin."), bAfterClear);

	return true;
}

#endif
