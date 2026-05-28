#if WITH_DEV_AUTOMATION_TESTS

#include "BHGameMode.h"
#include "BHRevisionQuestionBank.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHGameModeRevisionTuningTest,
	"BlackoutHunt.GameMode.RevisionTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHGameModeRevisionTuningTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(TEXT("Ten active students target eighteen revision nodes when enough stations exist."),
		ABHGameMode::ResolveRevisionNodeTargetFor(10, 0, 30),
		18);
	TestEqual(TEXT("Revision node target clamps to available stations."),
		ABHGameMode::ResolveRevisionNodeTargetFor(10, 0, 12),
		12);
	TestEqual(TEXT("Ten-student stage one nodes use two normal questions."),
		ABHGameMode::ResolveRevisionQuestionTargetFor(10, 0),
		2);
	TestEqual(TEXT("Ten-student later stages use three normal questions."),
		ABHGameMode::ResolveRevisionQuestionTargetFor(10, 2),
		3);
	TestTrue(TEXT("Hall Monitors count as revision participants for mastery and contribution gates."),
		ABHGameMode::IsRevisionParticipantRole(EBHPlayerRole::FakeHunter));
	TestTrue(TEXT("Survivors count as revision participants for mastery and contribution gates."),
		ABHGameMode::IsRevisionParticipantRole(EBHPlayerRole::Survivor));
	TestFalse(TEXT("Teachers do not count toward student revision mastery gates."),
		ABHGameMode::IsRevisionParticipantRole(EBHPlayerRole::Hunter));
	TestFalse(TEXT("Late spectators do not count toward student revision mastery gates."),
		ABHGameMode::IsRevisionParticipantRole(EBHPlayerRole::Spectator));
	TestEqual(TEXT("Spectator Teacher preference is allowed but remains only a preference."),
		ABHGameMode::SanitizeSpectatorRolePreference(EBHPlayerRole::Hunter),
		EBHPlayerRole::Hunter);
	TestEqual(TEXT("Spectator cannot request tester/admin as a next-round role."),
		ABHGameMode::SanitizeSpectatorRolePreference(EBHPlayerRole::Tester),
		EBHPlayerRole::Unassigned);
	TestEqual(TEXT("Foggrounds doorframe origin is floor-aligned even when the door sits at gameplay height."),
		ABHGameMode::ResolveFoggroundsDoorFrameOrigin(FVector(200.0f, -300.0f, 120.0f)).Z,
		0.0);

	FString BankSummary;
	TestTrue(TEXT("Revision question bank remains valid for classroom reports and adaptive selection."),
		FBHRevisionQuestionBank::Validate(BankSummary));
	for (int32 TopicIndex = 0; TopicIndex < 4; ++TopicIndex)
	{
		for (int32 DifficultyIndex = 0; DifficultyIndex < 3; ++DifficultyIndex)
		{
			const EBHPhysicsTopic Topic = static_cast<EBHPhysicsTopic>(TopicIndex);
			const EBHQuestionDifficulty Difficulty = static_cast<EBHQuestionDifficulty>(DifficultyIndex);
			FBHRevisionQuestion Question;
			const bool bSelected = FBHRevisionQuestionBank::SelectQuestionByDifficulty(Topic, Difficulty, TopicIndex * 17 + DifficultyIndex, Question);
			TestTrue(TEXT("Adaptive selection finds a question for every topic/difficulty pair."), bSelected);
			if (bSelected)
			{
				TestEqual(TEXT("Adaptive selection preserves the requested topic."), Question.Topic, Topic);
				TestEqual(TEXT("Adaptive selection preserves the requested difficulty when available."), Question.Difficulty, Difficulty);
			}
		}
	}

	return true;
}

#endif
