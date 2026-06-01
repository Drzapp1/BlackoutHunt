#if WITH_DEV_AUTOMATION_TESTS

#include "BHDiagramRenderer.h"
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
	FString BuiltInSummary;
	TestTrue(TEXT("Built-in compiled bank keeps its exact shipped distribution (376 total, 94/topic, 28/38/28 difficulty)."),
		FBHRevisionQuestionBank::ValidateBuiltInDistribution(BuiltInSummary));
	// Answer-safety: every built-in question's four options must be mutually distinct (no duplicate or
	// whitespace-only-different option), so no question can present two identical / both-correct buttons.
	// ValidateBuiltInDistribution also enforces this now; this loop reports the exact offender for diagnosis.
	{
		int32 DuplicateOptionQuestions = 0;
		for (const FBHRevisionQuestion& Q : FBHRevisionQuestionBank::GetBuiltInQuestions())
		{
			TSet<FString> SeenChoices;
			for (const FString& Choice : Q.Answer.Choices)
			{
				bool bAlreadyPresent = false;
				SeenChoices.Add(Choice.TrimStartAndEnd(), &bAlreadyPresent);
				if (bAlreadyPresent)
				{
					AddError(FString::Printf(TEXT("Question '%s' has a duplicate answer option: '%s'."), *Q.Id, *Choice));
					++DuplicateOptionQuestions;
					break;
				}
			}
		}
		TestEqual(TEXT("No built-in question has duplicate/ambiguous answer options."), DuplicateOptionQuestions, 0);
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAdaptiveDifficultyPerTopicTest,
	"BlackoutHunt.GameMode.AdaptiveDifficultyPerTopic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAdaptiveDifficultyPerTopicTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	ABHGameMode* GameMode = NewObject<ABHGameMode>();
	ABHPlayerState* PlayerState = NewObject<ABHPlayerState>();
	if (!TestNotNull(TEXT("Game mode constructs for adaptive-plan testing."), GameMode)
		|| !TestNotNull(TEXT("Player state constructs for adaptive-plan testing."), PlayerState))
	{
		return false;
	}

	EBHPhysicsTopic OutTopic = EBHPhysicsTopic::ForcesAndMotion;
	EBHQuestionDifficulty OutDifficulty = EBHQuestionDifficulty::Easy;
	FString OutReason;

	// The core fix: difficulty is calibrated to the TARGETED (weakest) topic's mastery, not the
	// student's cross-topic average. This student is strong overall (~81%) yet weak in Forces
	// (40%). The next question must target Forces AND be eased to that topic — NOT served Hard on
	// the strength of an average the student does not have in the topic being asked.
	PlayerState->RevisionStats.Attempts = 10;
	PlayerState->RevisionStats.ForcesMastery = 40.0f;
	PlayerState->RevisionStats.ElectricityMastery = 95.0f;
	PlayerState->RevisionStats.WavesMastery = 95.0f;
	PlayerState->RevisionStats.EnergyMastery = 95.0f;
	PlayerState->RevisionStats.MasteryPercent = 81.25f; // mean of the four topics
	GameMode->GetAdaptiveRevisionPlan(PlayerState, /*bLastAnswerCorrect=*/true, OutTopic, OutDifficulty, OutReason);
	TestEqual(TEXT("Adaptive plan targets the student's weakest topic."), OutTopic, EBHPhysicsTopic::ForcesAndMotion);
	TestEqual(TEXT("A topic the student is weak in is eased, not served Hard on a strong overall average."),
		OutDifficulty, EBHQuestionDifficulty::Easy);
	TestNotEqual(TEXT("A weak topic is never escalated to Hard off the overall average."),
		OutDifficulty, EBHQuestionDifficulty::Hard);

	// Control: a student genuinely strong in every topic, last answer correct, earns a Hard question.
	PlayerState->RevisionStats.ForcesMastery = 90.0f;
	PlayerState->RevisionStats.ElectricityMastery = 90.0f;
	PlayerState->RevisionStats.WavesMastery = 90.0f;
	PlayerState->RevisionStats.EnergyMastery = 90.0f;
	PlayerState->RevisionStats.MasteryPercent = 90.0f;
	GameMode->GetAdaptiveRevisionPlan(PlayerState, /*bLastAnswerCorrect=*/true, OutTopic, OutDifficulty, OutReason);
	TestEqual(TEXT("A uniformly strong student who just answered correctly is challenged with Hard."),
		OutDifficulty, EBHQuestionDifficulty::Hard);

	// Cold start: a student with no attempts always begins on Easy regardless of stored mastery.
	PlayerState->RevisionStats.Attempts = 0;
	GameMode->GetAdaptiveRevisionPlan(PlayerState, /*bLastAnswerCorrect=*/false, OutTopic, OutDifficulty, OutReason);
	TestEqual(TEXT("A student with no attempts always starts on Easy."),
		OutDifficulty, EBHQuestionDifficulty::Easy);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionQuestionBankJsonTest,
	"BlackoutHunt.GameMode.RevisionQuestionBankJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionQuestionBankJsonTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// "Migrate all" fidelity: serializing the built-in bank to JSON and parsing it back must
	// reproduce every question field exactly, so a teacher can export, edit, and reload safely.
	const TArray<FBHRevisionQuestion>& BuiltIn = FBHRevisionQuestionBank::GetBuiltInQuestions();
	const FString Json = FBHRevisionQuestionBank::SerializeQuestionsToJson(BuiltIn);
	TestTrue(TEXT("Serialized question bank JSON is non-empty."), Json.Len() > 0);

	TArray<FBHRevisionQuestion> RoundTripped;
	FString ParseError;
	const bool bParsed = FBHRevisionQuestionBank::ParseQuestionsFromJson(Json, RoundTripped, ParseError);
	TestTrue(FString::Printf(TEXT("Built-in bank JSON parses back (%s)."), *ParseError), bParsed);
	TestEqual(TEXT("Round-trip preserves the question count."), RoundTripped.Num(), BuiltIn.Num());

	if (bParsed && RoundTripped.Num() == BuiltIn.Num())
	{
		bool bAllFieldsMatch = true;
		for (int32 Index = 0; Index < BuiltIn.Num(); ++Index)
		{
			const FBHRevisionQuestion& A = BuiltIn[Index];
			const FBHRevisionQuestion& B = RoundTripped[Index];
			bAllFieldsMatch = bAllFieldsMatch
				&& A.Id == B.Id
				&& A.Topic == B.Topic
				&& A.TopicName == B.TopicName
				&& A.Subtopic == B.Subtopic
				&& A.Difficulty == B.Difficulty
				&& A.Type == B.Type
				&& A.DiagramType == B.DiagramType
				&& A.Prompt == B.Prompt
				&& A.Answer.Choices == B.Answer.Choices
				&& A.Answer.CorrectChoiceIndex == B.Answer.CorrectChoiceIndex
				&& A.Answer.Formula == B.Answer.Formula
				&& FMath::IsNearlyEqual(A.Answer.NumericAnswer, B.Answer.NumericAnswer, 0.01f)
				&& FMath::IsNearlyEqual(A.Answer.NumericTolerance, B.Answer.NumericTolerance, 0.01f)
				&& A.Hint == B.Hint
				&& A.CorrectionPrompt == B.CorrectionPrompt
				&& A.Explanation == B.Explanation
				&& FMath::IsNearlyEqual(A.MasteryWeight, B.MasteryWeight, 0.01f)
				&& A.Diagram.LabelA == B.Diagram.LabelA
				&& A.Diagram.LabelB == B.Diagram.LabelB
				&& A.Diagram.LabelC == B.Diagram.LabelC
				&& A.Diagram.LabelD == B.Diagram.LabelD
				&& A.Diagram.XAxis == B.Diagram.XAxis
				&& A.Diagram.YAxis == B.Diagram.YAxis
				&& A.Diagram.ImageSoftPath == B.Diagram.ImageSoftPath
				&& FMath::IsNearlyEqual(A.Diagram.ValueA, B.Diagram.ValueA, 0.01f)
				&& FMath::IsNearlyEqual(A.Diagram.ValueB, B.Diagram.ValueB, 0.01f)
				&& FMath::IsNearlyEqual(A.Diagram.ValueC, B.Diagram.ValueC, 0.01f)
				&& FMath::IsNearlyEqual(A.Diagram.ValueD, B.Diagram.ValueD, 0.01f)
				&& FMath::IsNearlyEqual(A.Diagram.AngleOrShape, B.Diagram.AngleOrShape, 0.01f)
				&& A.Diagram.ShapeVariant == B.Diagram.ShapeVariant;
			if (!bAllFieldsMatch)
			{
				AddError(FString::Printf(TEXT("Round-trip mismatch at question index %d (id '%s')."), Index, *A.Id));
				break;
			}
		}
		TestTrue(TEXT("Every built-in question round-trips through JSON with all fields intact."), bAllFieldsMatch);
	}

	// The relaxed validator accepts well-formed teacher content of any size, provided every
	// physics topic is represented and each question is structurally sound.
	TArray<FBHRevisionQuestion> SmallBank;
	for (int32 TopicIndex = 0; TopicIndex < 4; ++TopicIndex)
	{
		FBHRevisionQuestion Q;
		Q.Id = FString::Printf(TEXT("custom_%d"), TopicIndex);
		Q.Topic = static_cast<EBHPhysicsTopic>(TopicIndex);
		Q.Prompt = TEXT("Custom prompt?");
		Q.Hint = TEXT("Custom hint.");
		Q.Explanation = TEXT("Custom explanation.");
		Q.Answer.Choices = {TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D")};
		Q.Answer.CorrectChoiceIndex = 1;
		SmallBank.Add(Q);
	}
	FString CustomJson = FBHRevisionQuestionBank::SerializeQuestionsToJson(SmallBank);
	TArray<FBHRevisionQuestion> CustomParsed;
	FString CustomError;
	TestTrue(TEXT("A small hand-built bank serializes and parses."),
		FBHRevisionQuestionBank::ParseQuestionsFromJson(CustomJson, CustomParsed, CustomError));
	TestEqual(TEXT("Small custom bank preserves its four questions."), CustomParsed.Num(), 4);

	// Malformed content is rejected rather than silently accepted.
	TArray<FBHRevisionQuestion> Garbage;
	FString GarbageError;
	TestFalse(TEXT("Non-JSON content is rejected."),
		FBHRevisionQuestionBank::ParseQuestionsFromJson(TEXT("not json at all"), Garbage, GarbageError));
	TestFalse(TEXT("JSON without a questions array is rejected."),
		FBHRevisionQuestionBank::ParseQuestionsFromJson(TEXT("{\"version\":1}"), Garbage, GarbageError));

	return true;
}

// Answer-safety invariant: a diagram shows the question's GIVENS, never the answer. If any diagram
// label (or axis caption) contains the full correct-choice string, the picture reveals the answer.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionDiagramAnswerSafetyTest,
	"BlackoutHunt.GameMode.RevisionDiagramAnswerSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionDiagramAnswerSafetyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TArray<FBHRevisionQuestion>& Bank = FBHRevisionQuestionBank::GetBuiltInQuestions();
	int32 Leaks = 0;
	for (const FBHRevisionQuestion& Q : Bank)
	{
		if (Q.DiagramType == EBHDiagramType::None) { continue; }
		if (!Q.Answer.Choices.IsValidIndex(Q.Answer.CorrectChoiceIndex)) { continue; }
		const FString Correct = Q.Answer.Choices[Q.Answer.CorrectChoiceIndex].TrimStartAndEnd().ToLower();
		// Skip trivial/boolean choices that could coincidentally appear; physics givens won't
		// contain them anyway and they carry no numeric answer to leak.
		if (Correct.Len() < 3 || Correct == TEXT("true") || Correct == TEXT("false")) { continue; }
		const FString Labels[] = {Q.Diagram.LabelA, Q.Diagram.LabelB, Q.Diagram.LabelC, Q.Diagram.LabelD, Q.Diagram.XAxis, Q.Diagram.YAxis};
		for (const FString& Label : Labels)
		{
			if (Label.IsEmpty()) { continue; }
			if (Label.TrimStartAndEnd().ToLower().Contains(Correct))
			{
				AddError(FString::Printf(TEXT("Diagram for '%s' leaks the answer: label '%s' contains the correct choice '%s'."),
					*Q.Id, *Label, *Q.Answer.Choices[Q.Answer.CorrectChoiceIndex]));
				++Leaks;
			}
		}
	}
	TestEqual(TEXT("No built-in diagram label reveals the correct answer."), Leaks, 0);
	return true;
}

// Coverage dashboard: reports how many diagram questions carry per-question data. The soft floor
// guards against regressing below the authored baseline; it is tightened toward full coverage as
// the authoring phase lands.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionDiagramCoverageTest,
	"BlackoutHunt.GameMode.RevisionDiagramCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionDiagramCoverageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TArray<FBHRevisionQuestion>& Bank = FBHRevisionQuestionBank::GetBuiltInQuestions();
	int32 WithDiagram = 0;
	int32 Authored = 0;
	int32 DataExpected = 0;
	int32 DataExpectedAuthored = 0;
	TArray<FString> UnauthoredExpected;
	for (const FBHRevisionQuestion& Q : Bank)
	{
		if (Q.DiagramType == EBHDiagramType::None) { continue; }
		++WithDiagram;
		const bool bAuthored = Q.Diagram.HasValues() || Q.Diagram.ShapeVariant != 0 || Q.Diagram.AngleOrShape != 0.0f || Q.Diagram.HasImage() || !Q.Diagram.XAxis.IsEmpty() || !Q.Diagram.YAxis.IsEmpty();
		if (bAuthored) { ++Authored; }
		// "Data-expected" = a calculation/graph question whose diagram type actually consumes
		// per-question values. Purely schematic types (static charge, magnetic field lines, the
		// particle model) convey their meaning structurally, so they need no per-question data.
		const bool bSchematicType = (Q.DiagramType == EBHDiagramType::StaticCharge
			|| Q.DiagramType == EBHDiagramType::MagneticField
			|| Q.DiagramType == EBHDiagramType::ParticleModel);
		const bool bExpected = (Q.Type == EBHQuestionType::Calculation || Q.Type == EBHQuestionType::GraphReading) && !bSchematicType;
		if (bExpected)
		{
			++DataExpected;
			if (bAuthored) { ++DataExpectedAuthored; }
			else { UnauthoredExpected.Add(Q.Id); }
		}
	}
	AddInfo(FString::Printf(TEXT("Diagram coverage: %d with diagram, %d authored (%.0f%%); data-expected %d, authored %d (%.0f%%)."),
		WithDiagram, Authored, WithDiagram ? 100.0f * Authored / WithDiagram : 0.0f,
		DataExpected, DataExpectedAuthored, DataExpected ? 100.0f * DataExpectedAuthored / DataExpected : 0.0f));
	if (UnauthoredExpected.Num() > 0)
	{
		AddInfo(FString::Printf(TEXT("Data-expected questions still missing diagram data: %s"), *FString::Join(UnauthoredExpected, TEXT(", "))));
	}
	// Every Calculation / GraphReading diagram question must carry per-question data so the picture
	// shows the question's own values, not a generic schematic.
	TestEqual(TEXT("Every data-expected diagram question carries per-question data."), DataExpectedAuthored, DataExpected);
	return true;
}

// Any illustrated-diagram texture path (teacher JSON or future built-in art) must be a well-formed
// object path so ResolveDiagramTexture can load it; a malformed path silently falls back forever.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionDiagramImagePathsTest,
	"BlackoutHunt.GameMode.RevisionDiagramImagePaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionDiagramImagePathsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TArray<FBHRevisionQuestion>& Bank = FBHRevisionQuestionBank::GetBuiltInQuestions();
	for (const FBHRevisionQuestion& Q : Bank)
	{
		if (Q.Diagram.ImageSoftPath.IsEmpty()) { continue; }
		TestTrue(FString::Printf(TEXT("ImageSoftPath for '%s' is a well-formed object path (starts with '/')."), *Q.Id),
			Q.Diagram.ImageSoftPath.StartsWith(TEXT("/")) && !Q.Diagram.ImageSoftPath.Contains(TEXT(" ")));
	}
	return true;
}

// Every diagram type (including the newly added ones) must report a sane band height so the HUD
// reserves enough room without overflowing the question panel; unlisted types fall back to a safe
// default, which this also confirms stays in range.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionDiagramBandHeightsTest,
	"BlackoutHunt.GameMode.RevisionDiagramBandHeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionDiagramBandHeightsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UEnum* Enum = StaticEnum<EBHDiagramType>();
	if (!Enum)
	{
		AddError(TEXT("EBHDiagramType enum reflection not found."));
		return false;
	}
	for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
	{
		const FString Name = Enum->GetNameStringByIndex(Index);
		if (Name.Contains(TEXT("MAX")))
		{
			continue;
		}
		const EBHDiagramType Type = static_cast<EBHDiagramType>(Enum->GetValueByIndex(Index));
		if (Type == EBHDiagramType::None)
		{
			continue;
		}
		const float Height = FBHDiagramRenderer::BandHeightFor(Type);
		TestTrue(FString::Printf(TEXT("BandHeightFor(%s) is sane (90..200)."), *Name), Height >= 90.0f && Height <= 200.0f);
	}
	return true;
}

// Interactive drag answers (matching/ordering): the correct choice of every such question must parse
// into a well-formed slot/piece set so the on-screen drag tray can be built and graded.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionArrangementParseTest,
	"BlackoutHunt.GameMode.RevisionArrangementParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionArrangementParseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Grammar shape checks for the two arrangement forms.
	{
		TArray<FString> Slots;
		TArray<FString> Pieces;
		TestTrue(TEXT("Ordering string parses."), FBHRevisionQuestionBank::ParseArrangementChoice(TEXT("first -> second -> third"), EBHQuestionType::Ordering, Slots, Pieces));
		TestEqual(TEXT("Ordering yields three slots."), Slots.Num(), 3);
		TestEqual(TEXT("Ordering yields three pieces."), Pieces.Num(), 3);
		TestEqual(TEXT("Ordering piece 0 is 'first'."), Pieces[0], FString(TEXT("first")));
		TestEqual(TEXT("Ordering piece 2 is 'third'."), Pieces[2], FString(TEXT("third")));
	}
	{
		TArray<FString> Slots;
		TArray<FString> Pieces;
		// Mixed separators (": " and " -> ") inside one matching string must both parse.
		TestTrue(TEXT("Matching string parses."), FBHRevisionQuestionBank::ParseArrangementChoice(TEXT("Vector: force; Scalar -> time"), EBHQuestionType::DragDropMatching, Slots, Pieces));
		TestEqual(TEXT("Matching yields two slots."), Slots.Num(), 2);
		TestEqual(TEXT("Matching slot 0 key is 'Vector'."), Slots[0], FString(TEXT("Vector")));
		TestEqual(TEXT("Matching piece 0 value is 'force'."), Pieces[0], FString(TEXT("force")));
		TestEqual(TEXT("Matching slot 1 key is 'Scalar'."), Slots[1], FString(TEXT("Scalar")));
		TestEqual(TEXT("Matching piece 1 value is 'time'."), Pieces[1], FString(TEXT("time")));
	}
	{
		TArray<FString> Slots;
		TArray<FString> Pieces;
		TestFalse(TEXT("A plain answer string is not a valid arrangement."), FBHRevisionQuestionBank::ParseArrangementChoice(TEXT("Balanced forces"), EBHQuestionType::Ordering, Slots, Pieces));
	}

	int32 Checked = 0;
	int32 Failures = 0;
	for (const FBHRevisionQuestion& Q : FBHRevisionQuestionBank::GetBuiltInQuestions())
	{
		if (Q.Type != EBHQuestionType::DragDropMatching && Q.Type != EBHQuestionType::Ordering)
		{
			continue;
		}
		++Checked;
		if (!Q.Answer.Choices.IsValidIndex(Q.Answer.CorrectChoiceIndex))
		{
			++Failures;
			AddError(FString::Printf(TEXT("[%s] correct choice index out of range."), *Q.Id));
			continue;
		}
		TArray<FString> Slots;
		TArray<FString> Pieces;
		if (!FBHRevisionQuestionBank::ParseArrangementChoice(Q.Answer.Choices[Q.Answer.CorrectChoiceIndex], Q.Type, Slots, Pieces)
			|| Slots.Num() < 2 || Slots.Num() != Pieces.Num())
		{
			++Failures;
			AddError(FString::Printf(TEXT("[%s] correct choice '%s' did not parse into matching slots/pieces."), *Q.Id, *Q.Answer.Choices[Q.Answer.CorrectChoiceIndex]));
			continue;
		}
		for (const FString& Piece : Pieces)
		{
			if (Piece.IsEmpty())
			{
				++Failures;
				AddError(FString::Printf(TEXT("[%s] produced an empty arrangement piece."), *Q.Id));
				break;
			}
		}
	}
	TestTrue(TEXT("At least one matching/ordering question exists to validate."), Checked > 0);
	TestEqual(TEXT("Every matching/ordering correct choice parses into a drag arrangement."), Failures, 0);
	return true;
}

// Mirrors ABHObjectiveStation::GradeArrangement (shuffled-piece-per-slot vs canonical) without a
// spawned actor: the correct assignment must grade true and a rotated one must grade false.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionArrangementGradeTest,
	"BlackoutHunt.GameMode.RevisionArrangementGrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionArrangementGradeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	int32 Checked = 0;
	int32 Failures = 0;
	for (const FBHRevisionQuestion& Q : FBHRevisionQuestionBank::GetBuiltInQuestions())
	{
		if (Q.Type != EBHQuestionType::DragDropMatching && Q.Type != EBHQuestionType::Ordering)
		{
			continue;
		}
		if (!Q.Answer.Choices.IsValidIndex(Q.Answer.CorrectChoiceIndex))
		{
			continue;
		}
		TArray<FString> Slots;
		TArray<FString> Canonical;
		if (!FBHRevisionQuestionBank::ParseArrangementChoice(Q.Answer.Choices[Q.Answer.CorrectChoiceIndex], Q.Type, Slots, Canonical))
		{
			continue;
		}
		const int32 N = Canonical.Num();
		if (N < 2)
		{
			continue;
		}
		++Checked;

		// Deterministic non-identity "shuffle": reverse the canonical order.
		TArray<FString> Shuffled;
		Shuffled.Reserve(N);
		for (int32 i = N - 1; i >= 0; --i)
		{
			Shuffled.Add(Canonical[i]);
		}

		auto GradeWith = [&](const TArray<int32>& SlotToPiece) -> bool
		{
			if (SlotToPiece.Num() != N)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < N; ++Slot)
			{
				const int32 PieceIdx = SlotToPiece[Slot];
				if (!Shuffled.IsValidIndex(PieceIdx) || !Shuffled[PieceIdx].Equals(Canonical[Slot], ESearchCase::CaseSensitive))
				{
					return false;
				}
			}
			return true;
		};

		TArray<int32> Correct;
		Correct.Reserve(N);
		for (int32 Slot = 0; Slot < N; ++Slot)
		{
			int32 Found = INDEX_NONE;
			for (int32 j = 0; j < N; ++j)
			{
				if (Shuffled[j].Equals(Canonical[Slot], ESearchCase::CaseSensitive))
				{
					Found = j;
					break;
				}
			}
			Correct.Add(Found);
		}
		if (Correct.Contains(INDEX_NONE) || !GradeWith(Correct))
		{
			++Failures;
			AddError(FString::Printf(TEXT("[%s] correct arrangement did not grade as correct."), *Q.Id));
			continue;
		}

		// When the pieces are all distinct, a one-step rotation must be graded wrong.
		bool bDistinct = true;
		for (int32 a = 0; a < N && bDistinct; ++a)
		{
			for (int32 b = a + 1; b < N; ++b)
			{
				if (Canonical[a].Equals(Canonical[b], ESearchCase::CaseSensitive))
				{
					bDistinct = false;
					break;
				}
			}
		}
		if (bDistinct)
		{
			TArray<int32> Rotated;
			Rotated.Reserve(N);
			for (int32 Slot = 0; Slot < N; ++Slot)
			{
				Rotated.Add(Correct[(Slot + 1) % N]);
			}
			if (GradeWith(Rotated))
			{
				++Failures;
				AddError(FString::Printf(TEXT("[%s] a rotated (wrong) arrangement graded as correct."), *Q.Id));
			}
		}
	}
	TestTrue(TEXT("At least one matching/ordering question exercised the grader."), Checked > 0);
	TestEqual(TEXT("Arrangement grading accepts the correct order and rejects a rotated one."), Failures, 0);
	return true;
}

// Every station topic must have at least one drag (matching/ordering) question, so the
// "mix proper questions into all modes" bias always finds one for any station.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHRevisionSelectDragQuestionTest,
	"BlackoutHunt.GameMode.RevisionSelectDragQuestion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHRevisionSelectDragQuestionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const EBHPhysicsTopic Topics[] = {
		EBHPhysicsTopic::ForcesAndMotion, EBHPhysicsTopic::Electricity,
		EBHPhysicsTopic::Waves, EBHPhysicsTopic::Energy
	};
	for (const EBHPhysicsTopic Topic : Topics)
	{
		FBHRevisionQuestion Out;
		const bool bFound = FBHRevisionQuestionBank::SelectDragQuestion(Topic, EBHQuestionDifficulty::Easy, 12345, Out);
		TestTrue(FString::Printf(TEXT("Topic %d has a drag question."), static_cast<int32>(Topic)), bFound);
		if (bFound)
		{
			TestTrue(TEXT("Selected drag question is matching or ordering."),
				Out.Type == EBHQuestionType::DragDropMatching || Out.Type == EBHQuestionType::Ordering);
			TestEqual(TEXT("Selected drag question is in the requested topic."), Out.Topic, Topic);
		}
	}
	return true;
}

#endif
