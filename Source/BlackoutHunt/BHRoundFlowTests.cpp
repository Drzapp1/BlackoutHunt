// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHAutomationTestWorld.h"
#include "BHCharacter.h"
#include "BHGameInstance.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "GameFramework/DefaultPawn.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include <initializer_list>

// Round-flow / reconnect-lifecycle proofs: the hunter reconnect grace counter, the round-scoped
// invalidation of reconnect marks, the AutoPrep expected-roster count, the combat-log pawn snapshot,
// and the hunt-start hunter guarantee. Pure-helper tests — no networking, no full login flow.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHHunterReconnectGraceCountingTest,
	"BlackoutHunt.Network.HunterReconnectGraceCounting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHHunterReconnectGraceCountingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UBHGameInstance* GameInstance = NewObject<UBHGameInstance>();
	TestNotNull(TEXT("Game instance is created."), GameInstance);
	if (!GameInstance)
	{
		return false;
	}

	ABHPlayerState* HunterPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Hunter player state can be created."), HunterPS);
	if (!HunterPS)
	{
		return false;
	}
	HunterPS->SetPlayerName(TEXT("Grace Teacher"));
	HunterPS->SetRole(EBHPlayerRole::Hunter);
	HunterPS->SetLifeState(EBHPlayerLifeState::Alive);
	HunterPS->ReconnectToken = TEXT("grace-teacher-token");

	GameInstance->SetReconnectClockOverrideForTest(1000.0);
	GameInstance->MarkTravelPlayerLeftForReconnect(HunterPS, 0.0f);

	TestEqual(TEXT("A dropped alive hunter counts inside the grace window."),
		GameInstance->CountReconnectableAliveHunters(120.0f), 1);
	TestEqual(TEXT("A dropped hunter never counts as a reconnectable survivor."),
		GameInstance->CountReconnectableAliveSurvivors(120.0f), 0);
	TestEqual(TEXT("Grace 0 disables the hunter hold entirely."),
		GameInstance->CountReconnectableAliveHunters(0.0f), 0);

	GameInstance->SetReconnectClockOverrideForTest(1130.0);
	TestEqual(TEXT("The hunter hold expires with the grace window."),
		GameInstance->CountReconnectableAliveHunters(120.0f), 0);

	// Re-mark inside the window, then consume it the way PostLogin does on EVERY successful login.
	GameInstance->SetReconnectClockOverrideForTest(2000.0);
	GameInstance->MarkTravelPlayerLeftForReconnect(HunterPS, 0.0f);
	TestEqual(TEXT("A fresh mark counts again."), GameInstance->CountReconnectableAliveHunters(120.0f), 1);
	GameInstance->ClearReconnectMark(HunterPS);
	TestEqual(TEXT("A successful login consumes the mark so a present player cannot hold the round."),
		GameInstance->CountReconnectableAliveHunters(120.0f), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHReconnectMarksInvalidatedAtRoundEndTest,
	"BlackoutHunt.Network.ReconnectMarksInvalidatedAtRoundEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHReconnectMarksInvalidatedAtRoundEndTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UBHGameInstance* GameInstance = NewObject<UBHGameInstance>();
	TestNotNull(TEXT("Game instance is created."), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->SetReconnectClockOverrideForTest(1000.0);

	ABHPlayerState* HunterPS = NewObject<ABHPlayerState>();
	ABHPlayerState* SurvivorPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Hunter player state can be created."), HunterPS);
	TestNotNull(TEXT("Survivor player state can be created."), SurvivorPS);
	if (!HunterPS || !SurvivorPS)
	{
		return false;
	}
	HunterPS->SetPlayerName(TEXT("Round Scoped Teacher"));
	HunterPS->SetRole(EBHPlayerRole::Hunter);
	HunterPS->SetLifeState(EBHPlayerLifeState::Alive);
	HunterPS->ReconnectToken = TEXT("round-scoped-teacher-token");
	SurvivorPS->SetPlayerName(TEXT("Round Scoped Student"));
	SurvivorPS->SetRole(EBHPlayerRole::Survivor);
	SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
	SurvivorPS->AddQuestionPoints(30);
	SurvivorPS->ReconnectToken = TEXT("round-scoped-student-token");

	GameInstance->MarkTravelPlayerLeftForReconnect(HunterPS, 0.0f);
	GameInstance->MarkTravelPlayerLeftForReconnect(SurvivorPS, 0.0f);
	TestEqual(TEXT("Both marks are pending before the round resolves."),
		GameInstance->CountReconnectableAliveHunters(120.0f) + GameInstance->CountReconnectableAliveSurvivors(120.0f), 2);

	// EndRound invalidates every pending mark: round-N snapshots must not restore into round N+1.
	GameInstance->InvalidateAllReconnectMarks();
	TestEqual(TEXT("No hunter mark survives the round end."), GameInstance->CountReconnectableAliveHunters(120.0f), 0);
	TestEqual(TEXT("No survivor mark survives the round end."), GameInstance->CountReconnectableAliveSurvivors(120.0f), 0);

	FBHTravelPlayerProgress Progress;
	TestFalse(TEXT("The grace restore no longer matches after the round end."),
		GameInstance->TryGetReconnectProgress(HunterPS, 0.0f, 120.0f, Progress));

	// Only the round-scoped MARK is gone — the banked-progress snapshot still restores on the next
	// legitimate travel login (this is how points carry to round N+1).
	ABHPlayerState* NextRoundPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Next-round player state can be created."), NextRoundPS);
	if (NextRoundPS)
	{
		NextRoundPS->SetPlayerName(TEXT("Round Scoped Student"));
		NextRoundPS->ReconnectToken = TEXT("round-scoped-student-token");
		TestTrue(TEXT("The travel snapshot still restores banked progress after marks are invalidated."),
			GameInstance->RestoreTravelPlayerState(NextRoundPS, /*bApplyRoleAndLifeState=*/true));
		TestEqual(TEXT("Banked question points carry to the next round."), NextRoundPS->QuestionPoints, 30);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAutoPrepExpectedRosterCountsHumansOnlyTest,
	"BlackoutHunt.Round.AutoPrepExpectedRosterCountsHumansOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAutoPrepExpectedRosterCountsHumansOnlyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UBHGameInstance* GameInstance = NewObject<UBHGameInstance>();
	TestNotNull(TEXT("Game instance is created."), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->SetReconnectClockOverrideForTest(1000.0);

	ABHPlayerState* HumanPS = NewObject<ABHPlayerState>();
	ABHPlayerState* BotPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Human player state can be created."), HumanPS);
	TestNotNull(TEXT("Bot player state can be created."), BotPS);
	if (!HumanPS || !BotPS)
	{
		return false;
	}
	HumanPS->SetPlayerName(TEXT("Roster Human"));
	HumanPS->ReconnectToken = TEXT("roster-human-token");
	BotPS->SetPlayerName(TEXT("Roster Bot"));
	BotPS->SetIsABot(true);

	GameInstance->PersistTravelPlayerState(HumanPS);
	GameInstance->PersistTravelPlayerState(BotPS);

	TestEqual(TEXT("The AutoPrep roster counts only humans (bots never re-login)."),
		GameInstance->CountRecentlyPersistedHumanPlayers(90.0), 1);
	TestEqual(TEXT("A non-positive window counts nobody."),
		GameInstance->CountRecentlyPersistedHumanPlayers(0.0), 0);

	// Entries persisted before the window belong to an earlier travel (departed players) and must not
	// inflate the expected roster on a later map load.
	GameInstance->SetReconnectClockOverrideForTest(1100.0);
	TestEqual(TEXT("A snapshot older than the window no longer counts as expected."),
		GameInstance->CountRecentlyPersistedHumanPlayers(90.0), 0);

	// A re-persist (the departure sweep before the next travel) refreshes the stamp and counts again.
	GameInstance->PersistTravelPlayerState(HumanPS);
	TestEqual(TEXT("A refreshed snapshot counts toward the next departure roster."),
		GameInstance->CountRecentlyPersistedHumanPlayers(90.0), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHCombatLogReconnectSnapshotTest,
	"BlackoutHunt.Network.CombatLogReconnectSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHCombatLogReconnectSnapshotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntRoundFlow"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	UBHGameInstance* GameInstance = NewObject<UBHGameInstance>();
	ABHPlayerState* LeaverPS = World->SpawnActor<ABHPlayerState>();
	ADefaultPawn* LeaverPawn = World->SpawnActor<ADefaultPawn>(FVector(1234.0f, -567.0f, 89.0f), FRotator(0.0f, 135.0f, 0.0f));
	TestNotNull(TEXT("Game instance is created."), GameInstance);
	TestNotNull(TEXT("Leaver player state spawns."), LeaverPS);
	TestNotNull(TEXT("Leaver pawn spawns."), LeaverPawn);
	if (!GameInstance || !LeaverPS || !LeaverPawn)
	{
		return false;
	}
	LeaverPS->SetPlayerName(TEXT("Combat Logger"));
	LeaverPS->SetRole(EBHPlayerRole::Survivor);
	LeaverPS->SetLifeState(EBHPlayerLifeState::Alive);
	LeaverPS->SetPlayerId(777);
	LeaverPS->ReconnectToken = TEXT("combat-logger-token");

	// A LIVE-round (Hunt/FinalEscape) leave passes the pawn: the snapshot pins the drop spot + PlayerId.
	GameInstance->SetReconnectClockOverrideForTest(3000.0);
	GameInstance->MarkTravelPlayerLeftForReconnect(LeaverPS, 0.0f, LeaverPawn);

	FBHTravelPlayerProgress Progress;
	TestTrue(TEXT("The grace restore matches inside the window."),
		GameInstance->TryGetReconnectProgress(LeaverPS, 0.0f, 120.0f, Progress));
	TestTrue(TEXT("A live-round leave snapshots the pawn transform."), Progress.bHasPawnTransform);
	TestTrue(TEXT("The snapshot pins the drop location."),
		Progress.PawnLocation.Equals(FVector(1234.0f, -567.0f, 89.0f), 1.0f));
	TestTrue(TEXT("The snapshot pins the drop facing."),
		FMath::IsNearlyEqual(Progress.PawnRotation.Yaw, 135.0f, 0.5f));
	TestEqual(TEXT("The snapshot records the old PlayerId for the once-per-node remap."),
		Progress.OldPlayerId, 777);

	// A non-live leave (Prep/Intermission) passes no pawn: the restore must use a normal fresh spawn.
	GameInstance->MarkTravelPlayerLeftForReconnect(LeaverPS, 0.0f);
	TestTrue(TEXT("The grace restore still matches after the re-mark."),
		GameInstance->TryGetReconnectProgress(LeaverPS, 0.0f, 120.0f, Progress));
	TestFalse(TEXT("A pawn-less leave clears the pawn-transform flag (fresh spawn restore)."),
		Progress.bHasPawnTransform);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHHuntStartHunterGuaranteeTest,
	"BlackoutHunt.Round.HuntStartHunterGuarantee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHHuntStartHunterGuaranteeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntRoundFlow"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = World->SpawnActor<ABHGameState>();
	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	ABHPlayerState* MonitorPS = World->SpawnActor<ABHPlayerState>();
	ABHPlayerState* SurvivorAPS = World->SpawnActor<ABHPlayerState>();
	ABHPlayerState* SurvivorBPS = World->SpawnActor<ABHPlayerState>();
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	TestNotNull(TEXT("Monitor player state spawns."), MonitorPS);
	TestNotNull(TEXT("Survivor A player state spawns."), SurvivorAPS);
	TestNotNull(TEXT("Survivor B player state spawns."), SurvivorBPS);
	if (!GameState || !GameMode || !MonitorPS || !SurvivorAPS || !SurvivorBPS)
	{
		return false;
	}

	World->SetGameState(GameState);
	GameState->SetRoundPhase(EBHRoundPhase::Prep);
	GameMode->GameState = GameState;

	const auto ResetPlayer = [](ABHPlayerState* PS, const TCHAR* Name, EBHPlayerRole Role)
	{
		PS->SetPlayerName(Name);
		PS->SetRole(Role);
		PS->SetLifeState(EBHPlayerLifeState::Alive);
		PS->SetFakeHunterEligible(false);
	};

	// Hunter left during Prep, a Monitor is available: the Monitor is promoted first so the
	// survivor count stays intact (mirrors AssignRoles' preference order).
	ResetPlayer(MonitorPS, TEXT("Guarantee Monitor"), EBHPlayerRole::FakeHunter);
	ResetPlayer(SurvivorAPS, TEXT("Guarantee Survivor A"), EBHPlayerRole::Survivor);
	ResetPlayer(SurvivorBPS, TEXT("Guarantee Survivor B"), EBHPlayerRole::Survivor);
	GameState->AddPlayerState(MonitorPS);
	GameState->AddPlayerState(SurvivorAPS);
	GameState->AddPlayerState(SurvivorBPS);

	GameMode->DebugPromoteReplacementHunterForHuntStartForTest();
	TestEqual(TEXT("The Monitor is promoted to hunter first."), MonitorPS->PlayerRole, EBHPlayerRole::Hunter);
	TestEqual(TEXT("Survivor A keeps the survivor role."), SurvivorAPS->PlayerRole, EBHPlayerRole::Survivor);
	TestEqual(TEXT("Survivor B keeps the survivor role."), SurvivorBPS->PlayerRole, EBHPlayerRole::Survivor);

	// A hunter already present: the guarantee is a no-op (idempotent at hunt start).
	GameMode->DebugPromoteReplacementHunterForHuntStartForTest();
	TestEqual(TEXT("No second hunter is promoted while one is alive."),
		(SurvivorAPS->PlayerRole == EBHPlayerRole::Hunter ? 1 : 0) + (SurvivorBPS->PlayerRole == EBHPlayerRole::Hunter ? 1 : 0), 0);

	// No Monitor available: a Survivor is promoted, but only while another survivor remains.
	ResetPlayer(MonitorPS, TEXT("Guarantee Monitor"), EBHPlayerRole::Spectator);
	ResetPlayer(SurvivorAPS, TEXT("Guarantee Survivor A"), EBHPlayerRole::Survivor);
	ResetPlayer(SurvivorBPS, TEXT("Guarantee Survivor B"), EBHPlayerRole::Survivor);
	GameMode->DebugPromoteReplacementHunterForHuntStartForTest();
	TestEqual(TEXT("Exactly one survivor is promoted when no Monitor exists."),
		(SurvivorAPS->PlayerRole == EBHPlayerRole::Hunter ? 1 : 0) + (SurvivorBPS->PlayerRole == EBHPlayerRole::Hunter ? 1 : 0), 1);

	// A lone participant is never promoted: they stay the survivor (mirrors the survivor guarantee).
	ResetPlayer(MonitorPS, TEXT("Guarantee Monitor"), EBHPlayerRole::Spectator);
	ResetPlayer(SurvivorAPS, TEXT("Guarantee Survivor A"), EBHPlayerRole::Spectator);
	ResetPlayer(SurvivorBPS, TEXT("Guarantee Survivor B"), EBHPlayerRole::Survivor);
	GameMode->DebugPromoteReplacementHunterForHuntStartForTest();
	TestEqual(TEXT("A lone participant stays the survivor."), SurvivorBPS->PlayerRole, EBHPlayerRole::Survivor);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHDecidedRoundCaptureIsInertTest,
	"BlackoutHunt.Round.DecidedRoundCaptureIsInert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHDecidedRoundCaptureIsInertTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntRoundFlow"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameState* GameState = World->SpawnActor<ABHGameState>();
	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	ABHCharacter* Survivor = World->SpawnActor<ABHCharacter>(FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ABHPlayerState* SurvivorPS = World->SpawnActor<ABHPlayerState>();
	TestNotNull(TEXT("Game state spawns."), GameState);
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	TestNotNull(TEXT("Survivor spawns."), Survivor);
	TestNotNull(TEXT("Survivor player state spawns."), SurvivorPS);
	if (!GameState || !GameMode || !Survivor || !SurvivorPS)
	{
		return false;
	}

	World->SetGameState(GameState);
	GameMode->GameState = GameState;
	SurvivorPS->SetPlayerName(TEXT("Post Round Student"));
	SurvivorPS->SetRole(EBHPlayerRole::Survivor);
	SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
	SurvivorPS->AddQuestionPoints(40);
	Survivor->SetPlayerState(SurvivorPS);
	GameState->AddPlayerState(SurvivorPS);

	// A Teacher swing landing AFTER the round is decided must be inert: no 25% point dock, no capture.
	for (const EBHRoundPhase DecidedPhase : { EBHRoundPhase::SurvivorsWin, EBHRoundPhase::HunterWin })
	{
		GameState->SetRoundPhase(DecidedPhase);
		GameMode->NotifySurvivorCaptured(Survivor, nullptr);
		TestEqual(TEXT("No question points are docked in a decided round."), SurvivorPS->QuestionPoints, 40);
		TestEqual(TEXT("The victim is not captured in a decided round."), SurvivorPS->LifeState, EBHPlayerLifeState::Alive);
		TestFalse(TEXT("The victim's pawn is not hidden in a decided round."), Survivor->IsHidden());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
