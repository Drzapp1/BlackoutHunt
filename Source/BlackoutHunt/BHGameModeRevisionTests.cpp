#if WITH_DEV_AUTOMATION_TESTS

#include "BHGameMode.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "BHTypes.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionReviewQueueTest,
	"BlackoutHunt.GameMode.RevisionReviewQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionReviewQueueTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// The spaced-repetition loop re-asks the exact missed question, so a by-ID
	// lookup must round-trip every authored question.
	const TArray<FBHRevisionQuestion>& Questions = FBHRevisionQuestionBank::GetQuestions();
	TestTrue(TEXT("Question bank is non-empty for review lookups."), Questions.Num() > 0);
	if (Questions.Num() > 0)
	{
		const FString KnownId = Questions[0].Id;
		FBHRevisionQuestion Found;
		TestTrue(TEXT("FindQuestion locates a known question ID."), FBHRevisionQuestionBank::FindQuestion(KnownId, Found));
		TestEqual(TEXT("FindQuestion returns the requested question."), Found.Id, KnownId);
		FBHRevisionQuestion Missing;
		TestFalse(TEXT("FindQuestion fails for an unknown ID."), FBHRevisionQuestionBank::FindQuestion(TEXT("does_not_exist_999"), Missing));
	}

	ABHPlayerState* PlayerState = NewObject<ABHPlayerState>();
	if (!TestNotNull(TEXT("Player state constructs for review-queue testing."), PlayerState))
	{
		return false;
	}

	// A fresh player has nothing to review.
	TestEqual(TEXT("Review queue starts empty."), PlayerState->RevisionReviewQueue.Num(), 0);
	TestTrue(TEXT("Peek on an empty queue returns an empty ID."), PlayerState->PeekRevisionReview().IsEmpty());

	// A miss enqueues the question; oldest is surfaced first.
	PlayerState->EnqueueRevisionReview(TEXT("forces_q1"));
	PlayerState->EnqueueRevisionReview(TEXT("waves_q2"));
	TestEqual(TEXT("Two distinct misses queue two questions."), PlayerState->RevisionReviewQueue.Num(), 2);
	TestEqual(TEXT("Oldest missed question is surfaced first."), PlayerState->PeekRevisionReview(), FString(TEXT("forces_q1")));

	// Re-missing an already-queued question dedups (moves it to the back).
	PlayerState->EnqueueRevisionReview(TEXT("forces_q1"));
	TestEqual(TEXT("Re-missing a queued question does not duplicate it."), PlayerState->RevisionReviewQueue.Num(), 2);
	TestEqual(TEXT("Re-missed question moves behind the other pending review."), PlayerState->PeekRevisionReview(), FString(TEXT("waves_q2")));

	// Answering a queued question correctly clears it.
	TestTrue(TEXT("Dequeue removes a queued question and reports success."), PlayerState->DequeueRevisionReview(TEXT("waves_q2")));
	TestEqual(TEXT("Queue shrinks after a correct review answer."), PlayerState->RevisionReviewQueue.Num(), 1);
	TestFalse(TEXT("Dequeue of an absent question reports no removal."), PlayerState->DequeueRevisionReview(TEXT("waves_q2")));

	// The queue is capped so a long session cannot grow it without bound.
	PlayerState->ResetRevisionStats();
	TestEqual(TEXT("Resetting revision stats also clears the review queue."), PlayerState->RevisionReviewQueue.Num(), 0);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		PlayerState->EnqueueRevisionReview(FString::Printf(TEXT("q_%d"), Index));
	}
	TestTrue(TEXT("Review queue stays bounded across many misses."), PlayerState->RevisionReviewQueue.Num() <= 8);
	TestEqual(TEXT("Capped queue keeps the most recent miss reachable."), PlayerState->RevisionReviewQueue.Last(), FString(TEXT("q_19")));

	return true;
}

#endif
