// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHGameInstance.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "BHPowerupLibrary.h"
#include "BHPowerupShopTerminal.h"
#include "BHTrainActivityStation.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHTrainEconomyTest,
	"BlackoutHunt.Train.Economy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHTrainEconomyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const auto TestContainsText = [this](const TCHAR* Description, const FString& ActualText, const TCHAR* ExpectedText)
	{
		const bool bContains = ActualText.Contains(ExpectedText, ESearchCase::IgnoreCase);
		return TestTrue(FString::Printf(TEXT("%s Actual='%s' ExpectedSubstring='%s'"),
			Description,
			*ActualText,
			ExpectedText),
			bContains);
	};

	ABHPlayerState* SurvivorPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Survivor player state can be created."), SurvivorPS);
	SurvivorPS->SetRole(EBHPlayerRole::Survivor);
	SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
	SurvivorPS->AddQuestionPoints(100);
	TestEqual(TEXT("Caught penalty removes 25 percent of unspent question points."), SurvivorPS->ApplyCaughtQuestionPointPenalty(), 25);
	TestEqual(TEXT("Caught penalty preserves remaining question points."), SurvivorPS->QuestionPoints, 75);
	SurvivorPS->SpendQuestionPoints(75);
	TestEqual(TEXT("Caught penalty at zero stays zero."), SurvivorPS->ApplyCaughtQuestionPointPenalty(), 0);
	TestEqual(TEXT("Question points never go negative after caught penalty."), SurvivorPS->QuestionPoints, 0);

	ABHPlayerState* TeacherPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Teacher player state can be created."), TeacherPS);
	TeacherPS->SetRole(EBHPlayerRole::Hunter);
	TeacherPS->SetLifeState(EBHPlayerLifeState::Alive);
	TeacherPS->AddHunterPoints(40);
	TestEqual(TEXT("Teacher captures award capture points."), TeacherPS->HunterPoints, 40);
	TestTrue(TEXT("Teacher can spend earned capture points."), TeacherPS->SpendHunterPoints(35));
	TestEqual(TEXT("Capture point spend reduces current capture points."), TeacherPS->HunterPoints, 5);
	TestFalse(TEXT("Teacher cannot overspend capture points."), TeacherPS->SpendHunterPoints(40));

	FBHPowerupDefinition ScanFocus;
	TestTrue(TEXT("Teacher Scan Focus is defined."), FBHPowerupLibrary::GetDefinition(EBHPowerupType::TeacherScanFocus, ScanFocus));
	TestEqual(TEXT("Teacher Scan Focus costs 40 capture points."), ScanFocus.Cost, 40);
	TestTrue(TEXT("Teacher Scan Focus is classified as a Teacher upgrade."), FBHPowerupLibrary::IsTeacherPowerup(EBHPowerupType::TeacherScanFocus));
	TestFalse(TEXT("Survivor stamina boost is not a Teacher upgrade."), FBHPowerupLibrary::IsTeacherPowerup(EBHPowerupType::StaminaBoost));
	TestTrue(TEXT("Teacher role can buy Teacher upgrade."), ABHPowerupShopTerminal::IsRoleAllowedForPowerup(TeacherPS, EBHPowerupType::TeacherScanFocus));
	TestFalse(TEXT("Survivor role cannot buy Teacher upgrade."), ABHPowerupShopTerminal::IsRoleAllowedForPowerup(SurvivorPS, EBHPowerupType::TeacherScanFocus));
	TestTrue(TEXT("Survivor role can buy Survivor powerup."), ABHPowerupShopTerminal::IsRoleAllowedForPowerup(SurvivorPS, EBHPowerupType::StaminaBoost));
	TestFalse(TEXT("Teacher role cannot buy Survivor powerup."), ABHPowerupShopTerminal::IsRoleAllowedForPowerup(TeacherPS, EBHPowerupType::StaminaBoost));
	TestFalse(TEXT("Null player state cannot buy train items."), ABHPowerupShopTerminal::IsRoleAllowedForPowerup(nullptr, EBHPowerupType::StaminaBoost));
	const FString TeacherShopSummary = ABHPowerupShopTerminal::BuildShopItemSummary(EBHPowerupType::TeacherScanFocus);
	TestTrue(TEXT("Teacher shop summary includes capture currency."), TeacherShopSummary.Contains(TEXT("capture pts")));
	TestTrue(TEXT("Teacher shop summary includes charge cap."), TeacherShopSummary.Contains(TEXT("Max charges")));
	TestTrue(TEXT("Teacher shop summary describes passive upgrades."), TeacherShopSummary.Contains(TEXT("Passive upgrade")));
	TestTrue(TEXT("Survivor buying Teacher upgrade gets a role-specific failure."), ABHPowerupShopTerminal::BuildPurchaseStatusText(SurvivorPS, EBHPowerupType::TeacherScanFocus).Contains(TEXT("Teacher-only")));

	ABHPlayerState* ShopSurvivorPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Shop survivor player state can be created."), ShopSurvivorPS);
	ShopSurvivorPS->SetRole(EBHPlayerRole::Survivor);
	ShopSurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
	TestTrue(TEXT("Shop status explains insufficient question points."), ABHPowerupShopTerminal::BuildPurchaseStatusText(ShopSurvivorPS, EBHPowerupType::StaminaBoost).Contains(TEXT("Need")));
	ShopSurvivorPS->AddQuestionPoints(40);
	TestTrue(TEXT("Shop status confirms ready purchase when affordable."), ABHPowerupShopTerminal::BuildPurchaseStatusText(ShopSurvivorPS, EBHPowerupType::StaminaBoost).Contains(TEXT("ready")));
	ShopSurvivorPS->AddPowerupCharge(EBHPowerupType::StaminaBoost, 2);
	ShopSurvivorPS->AddPowerupCharge(EBHPowerupType::StaminaBoost, 2);
	TestTrue(TEXT("Shop status explains full charges."), ABHPowerupShopTerminal::BuildPurchaseStatusText(ShopSurvivorPS, EBHPowerupType::StaminaBoost).Contains(TEXT("full")));

	TestTrue(TEXT("Train snack activity summary names stamina recovery."),
		ABHTrainActivityStation::BuildActivitySummary(EBHTrainActivityType::SnackCart).Contains(TEXT("stamina"), ESearchCase::IgnoreCase));
	TestTrue(TEXT("Train drink activity summary names flashlight refill."),
		ABHTrainActivityStation::BuildActivitySummary(EBHTrainActivityType::DrinkCooler).Contains(TEXT("flashlight"), ESearchCase::IgnoreCase));
	TestTrue(TEXT("Train reflex activity summary describes timing."),
		ABHTrainActivityStation::BuildActivitySummary(EBHTrainActivityType::ReflexArcade).Contains(TEXT("GREEN"), ESearchCase::IgnoreCase));
	TestTrue(TEXT("Train memory activity summary describes matching."),
		ABHTrainActivityStation::BuildActivitySummary(EBHTrainActivityType::MemoryTable).Contains(TEXT("match"), ESearchCase::IgnoreCase));
	TestTrue(TEXT("Alive survivor can use train activities."), ABHTrainActivityStation::IsPassengerEligibleForActivity(SurvivorPS));
	TestTrue(TEXT("Alive Teacher can use train activities."), ABHTrainActivityStation::IsPassengerEligibleForActivity(TeacherPS));
	ABHPlayerState* UnassignedPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Unassigned player state can be created."), UnassignedPS);
	UnassignedPS->SetLifeState(EBHPlayerLifeState::Alive);
	TestFalse(TEXT("Unassigned player cannot use train activities."), ABHTrainActivityStation::IsPassengerEligibleForActivity(UnassignedPS));

	ABHGameState* TrainState = NewObject<ABHGameState>();
	TestNotNull(TEXT("Game state can be created for public train text checks."), TrainState);
	TrainState->SetRoundPhase(EBHRoundPhase::Intermission);
	TrainState->SetTrainState(EBHTrainPhase::Arrival, 0, 10.0f, TEXT("Next Stop: Substation"), TEXT(""));
	TestContainsText(TEXT("Arrival public action tells players to board now."), TrainState->GetPublicObjectiveActionText(), TEXT("board now"));
	TestContainsText(TEXT("Arrival progress text marks doors open."), TrainState->GetObjectiveProgressText(), TEXT("doors open"));
	TrainState->SetTrainState(EBHTrainPhase::Recap, 0, 10.0f, TEXT("Next Stop: Substation"), TEXT(""));
	TestContainsText(TEXT("Recap public action points players to recap boards."), TrainState->GetPublicObjectiveActionText(), TEXT("recap boards"));
	TestContainsText(TEXT("Recap progress text marks moving door lock."), TrainState->GetObjectiveProgressText(), TEXT("doors closed while moving"));
	TrainState->SetTrainState(EBHTrainPhase::BonusQuestion, 0, 10.0f, TEXT("Next Stop: Substation"), TEXT(""));
	TestContainsText(TEXT("Bonus public action names the 1-4 terminal."), TrainState->GetPublicObjectiveActionText(), TEXT("1-4"));
	TrainState->SetTrainState(EBHTrainPhase::Shop, 0, 10.0f, TEXT("Next Stop: Substation"), TEXT(""));
	TestContainsText(TEXT("Shop public action preserves stable buy-upgrades wording."), TrainState->GetPublicObjectiveActionText(), TEXT("buy upgrades"));
	TestContainsText(TEXT("Shop public action names role-eligible upgrades."), TrainState->GetPublicObjectiveActionText(), TEXT("role-eligible"));
	TrainState->SetTrainState(EBHTrainPhase::StationStop, 0, 10.0f, TEXT("Next Stop: Substation"), TEXT(""));
	TestContainsText(TEXT("Station stop public action explains safe auto-board."), TrainState->GetPublicObjectiveActionText(), TEXT("pulled aboard"));
	TestContainsText(TEXT("Station stop progress text marks doors open."), TrainState->GetObjectiveProgressText(), TEXT("doors open"));
	TrainState->SetTrainState(EBHTrainPhase::Departing, 0, 10.0f, TEXT("Next Stop: Substation"), TEXT(""));
	TestContainsText(TEXT("Departing public action tells players to hold position."), TrainState->GetPublicObjectiveActionText(), TEXT("hold position"));
	TestContainsText(TEXT("Departing progress text marks departure lock."), TrainState->GetObjectiveProgressText(), TEXT("doors closed for departure"));
	TrainState->SetRoundPhase(EBHRoundPhase::FinalEscape);
	TrainState->SetFinalEscapeState(EBHFinalEscapeState::Cutscene, 10.0f, 90.0f, 16.0f);
	TestContainsText(TEXT("Final escape cutscene public action explains Teacher delay."), TrainState->GetPublicObjectiveActionText(), TEXT("Teacher release is delayed"));
	TestContainsText(TEXT("Final escape cutscene progress exposes control return."), TrainState->GetObjectiveProgressText(), TEXT("Control"));
	TestContainsText(TEXT("Final escape cutscene progress exposes Teacher release."), TrainState->GetObjectiveProgressText(), TEXT("Teacher release"));
	TrainState->SetFinalEscapeState(EBHFinalEscapeState::EscapeActive, 0.0f, 90.0f, 0.0f);
	TestContainsText(TEXT("Final escape active public action tells players to board before departure."), TrainState->GetPublicObjectiveActionText(), TEXT("before departure"));
	TestContainsText(TEXT("Final escape active progress marks Teacher released."), TrainState->GetObjectiveProgressText(), TEXT("Teacher released"));

	for (const FBHPowerupDefinition& Definition : FBHPowerupLibrary::GetDefaultPowerups())
	{
		TestFalse(FString::Printf(TEXT("%s has a display name."), *Definition.Name), Definition.Name.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s has a positive cost."), *Definition.Name), Definition.Cost > 0);
		TestTrue(FString::Printf(TEXT("%s has at least one charge."), *Definition.Name), Definition.MaxCharges > 0);
	}

	TestTrue(TEXT("Adding first Stamina Boost charge succeeds."), SurvivorPS->AddPowerupCharge(EBHPowerupType::StaminaBoost, 2));
	TestTrue(TEXT("Adding second Stamina Boost charge succeeds."), SurvivorPS->AddPowerupCharge(EBHPowerupType::StaminaBoost, 2));
	TestFalse(TEXT("Adding beyond max Stamina Boost charges is rejected."), SurvivorPS->AddPowerupCharge(EBHPowerupType::StaminaBoost, 2));
	TestEqual(TEXT("Stamina Boost charges remain capped."), SurvivorPS->GetPowerupCharges(EBHPowerupType::StaminaBoost), 2);

	UBHGameInstance* GameInstance = NewObject<UBHGameInstance>();
	TestNotNull(TEXT("Game instance can be created for travel persistence test."), GameInstance);
	TeacherPS->SetPlayerName(TEXT("Teacher Persist"));
	TeacherPS->AddHunterPoints(75);
	GameInstance->PersistTravelPlayerState(TeacherPS);
	ABHPlayerState* RestoredPS = NewObject<ABHPlayerState>();
	RestoredPS->SetPlayerName(TEXT("Teacher Persist"));
	TestTrue(TEXT("Travel restore finds persisted Teacher progress."), GameInstance->RestoreTravelPlayerState(RestoredPS));
	TestEqual(TEXT("Travel restore carries current capture points."), RestoredPS->HunterPoints, 80);
	TestEqual(TEXT("Travel restore carries lifetime capture points."), RestoredPS->LifetimeHunterPoints, 115);
	GameInstance->ResetPersistentHunterPoints();
	ABHPlayerState* ResetPS = NewObject<ABHPlayerState>();
	ResetPS->SetPlayerName(TEXT("Teacher Persist"));
	TestTrue(TEXT("Travel restore still finds player after capture point reset."), GameInstance->RestoreTravelPlayerState(ResetPS));
	TestEqual(TEXT("Capture point reset clears current capture points."), ResetPS->HunterPoints, 0);
	TestEqual(TEXT("Capture point reset clears lifetime capture points."), ResetPS->LifetimeHunterPoints, 0);

	ABHPlayerState* CapturedTravelPS = NewObject<ABHPlayerState>();
	TestNotNull(TEXT("Captured travel player state can be created."), CapturedTravelPS);
	CapturedTravelPS->SetPlayerName(TEXT("Captured Persist"));
	CapturedTravelPS->SetRole(EBHPlayerRole::Survivor);
	CapturedTravelPS->SetDesiredRole(EBHPlayerRole::FakeHunter);
	CapturedTravelPS->SetLifeState(EBHPlayerLifeState::Captured);
	CapturedTravelPS->SetFakeHunterEligible(true);
	CapturedTravelPS->AddQuestionPoints(64);
	TestTrue(TEXT("Captured travel player can hold a powerup charge."), CapturedTravelPS->AddPowerupCharge(EBHPowerupType::StaminaBoost, 2));
	CapturedTravelPS->SetPowerupCooldown(EBHPowerupType::StaminaBoost, 1234.0f);
	GameInstance->PersistTravelPlayerState(CapturedTravelPS);

	ABHPlayerState* RestoredCapturedPS = NewObject<ABHPlayerState>();
	RestoredCapturedPS->SetPlayerName(TEXT("Captured Persist"));
	TestTrue(TEXT("Travel restore finds captured survivor progress."), GameInstance->RestoreTravelPlayerState(RestoredCapturedPS));
	TestEqual(TEXT("Travel restore makes captured survivor playable again."), RestoredCapturedPS->LifeState, EBHPlayerLifeState::Alive);
	TestFalse(TEXT("Travel restore clears one-round Hall Monitor eligibility."), RestoredCapturedPS->bFakeHunterEligible);
	TestEqual(TEXT("Travel restore keeps earned question points for the train run."), RestoredCapturedPS->QuestionPoints, 64);
	TestEqual(TEXT("Travel restore keeps bought/earned powerup charges."), RestoredCapturedPS->GetPowerupCharges(EBHPowerupType::StaminaBoost), 1);
	TestEqual(TEXT("Travel restore drops stale server-time powerup cooldowns."), RestoredCapturedPS->GetPowerupCooldownEnd(EBHPowerupType::StaminaBoost), 0.0f);

	GameInstance->ResetPersistentTrainRunProgress();
	ABHPlayerState* FreshLobbyPS = NewObject<ABHPlayerState>();
	FreshLobbyPS->SetPlayerName(TEXT("Captured Persist"));
	TestTrue(TEXT("Final train-run reset still finds the player."), GameInstance->RestoreTravelPlayerState(FreshLobbyPS));
	TestEqual(TEXT("Final train-run reset clears question points."), FreshLobbyPS->QuestionPoints, 0);
	TestEqual(TEXT("Final train-run reset clears lifetime question points."), FreshLobbyPS->LifetimeQuestionPoints, 0);
	TestEqual(TEXT("Final train-run reset clears powerup inventory."), FreshLobbyPS->GetPowerupCharges(EBHPowerupType::StaminaBoost), 0);
	TestEqual(TEXT("Final train-run reset leaves player alive for the next lobby."), FreshLobbyPS->LifeState, EBHPlayerLifeState::Alive);
	TestFalse(TEXT("Final train-run reset clears stale Hall Monitor eligibility."), FreshLobbyPS->bFakeHunterEligible);

	return true;
}

#endif
