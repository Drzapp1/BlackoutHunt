// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTypes.h"
#include "BHObjectiveStation.generated.h"

class ABHPlayerController;

UCLASS()
class BLACKOUTHUNT_API ABHObjectiveStation : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHObjectiveStation();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void Configure(EBHObjectiveStationType NewStationType);
	void ConfigureRevisionCounterNode(EBHRevisionCounterNodeType NewCounterType);
	void ConfigureTeacherMirrorTrapNode();
	// Tutorial: present a fixed, self-contained visual (diagram) multiple-choice question so the guided
	// lesson can teach reading a diagram and answering it by clicking the choices - with no revision-mode or
	// question-bank dependency. Safe to call after Configure() to override whatever question it picked.
	void ConfigureTutorialVisualQuestion();
	// Tutorial: present a fixed, self-contained INTERACTIVE drag-drop matching question (drag units onto
	// quantities, then Submit), so the lesson can teach the hands-on pointer answer flow. Self-contained too.
	void ConfigureTutorialInteractiveQuestion();
	void SetDirectorActive(bool bNewActive);
	// bVisualAnswer = the answer was given via the interactive visual method (drag arrangement, or
	// clicking a diagram element) rather than picking a multiple-choice option; visual answers earn
	// extra mastery (see FinalizeRevisionAnswer). Keyboard/choice-row/bot answers pass false.
	bool SubmitAnswer(ABHCharacter* Character, int32 AnswerIndex, bool bVisualAnswer = false);
	// Typed-numeric answer path for Calculation questions: validates the value against
	// the question's NumericAnswer +/- tolerance, then shares the choice path's result handling.
	bool SubmitNumericAnswer(ABHCharacter* Character, float Value);
	// Drag/drop answer path for DragDropMatching + Ordering questions. SlotToPiece[slot] is the
	// index (into GetInteractivePieces) of the piece the player dropped on that slot. Graded
	// server-side against the correct choice, then mapped to a synthetic choice index so it shares
	// the exact same evaluation pipeline (team voting, anti-gaming hold, feedback) as SubmitAnswer.
	bool SubmitArrangement(ABHCharacter* Character, const TArray<int32>& SlotToPiece);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsDirectorActive() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsCompleted() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	EBHObjectiveStationType GetStationType() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	FString GetPhysicalTaskInstruction() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	bool IsQuestionSolved() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionTopic() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionPrompt() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	int32 GetQuestionChoiceCount() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	int32 GetCorrectAnswerIndexForBot() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionChoice(int32 ChoiceIndex) const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionFeedback() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	bool IsQuestionFeedbackCorrect() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	EBHQuestionType GetQuestionType() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	EBHQuestionDifficulty GetQuestionDifficulty() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	EBHDiagramType GetQuestionDiagramType() const;

	// Optional data-driven diagram parameters (resistor/voltage values, lever distances,
	// wave amplitude, ray angle, optional image path) for the active question. Empty for
	// questions without visual givens, in which case the HUD draws the generic schematic.
	const FBHDiagramParams& GetQuestionDiagram() const { return QuestionDiagram; }

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionSubtopic() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionFormula() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	float GetQuestionNumericAnswer() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	float GetQuestionNumericTolerance() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionExplanation() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetRevisionTeamSummary() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	int32 GetRevisionQuestionsSolved() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	int32 GetRevisionQuestionsRequired() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	EBHRevisionCounterNodeType GetRevisionCounterType() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	bool IsTeacherMirrorTrapNode() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	bool IsReviewQuestion() const;

	// Interactive drag/drop data for DragDropMatching + Ordering questions. Pieces are the draggable
	// chips (matching values / ordering steps) SHUFFLED so the on-screen order never reveals the
	// answer; Slots are the fixed drop targets (matching keys / "1.","2." positions). Both empty for
	// every other question type. Carries no answer (grading re-derives the correct pairing server-side).
	const TArray<FString>& GetInteractivePieces() const { return InteractivePieces; }
	const TArray<FString>& GetInteractiveSlots() const { return InteractiveSlots; }
	bool HasInteractiveArrangement() const { return InteractiveSlots.Num() > 0 && InteractivePieces.Num() == InteractiveSlots.Num(); }

protected:
	void CompleteObjective();
	void ConfigureQuestion();
	// Apply a bank question (revision selection, or a drag question mixed into standard Hunt) onto the
	// station's replicated fields: rotates choices, sets type/diagram/hint/etc., and builds the drag
	// tray for matching/ordering. Seed drives the choice rotation + piece shuffle.
	void ApplyRevisionQuestion(const FBHRevisionQuestion& Selected, int32 Seed);
	// Rebuild InteractivePieces/InteractiveSlots from the current question's correct choice for
	// DragDropMatching/Ordering (no-op otherwise). Pieces are shuffled deterministically from Seed.
	void BuildInteractiveArrangement(int32 Seed);
	// Server-side grade of a drag arrangement: true iff every slot holds its canonically-correct piece.
	bool GradeArrangement(const TArray<int32>& SlotToPiece) const;
	// RevisionParticipants pairs each answer-team voter with THEIR OWN correctness (vote == correct choice),
	// so mastery/contribution is credited per student and a correct team never carries a wrong voter. The
	// node still progresses on the team-majority result passed as bCorrect.
	bool FinalizeRevisionAnswer(ABHCharacter* Character, ABHPlayerController* PC, bool bCorrect, const FString& EvaluatedSelectedAnswer, TArray<TPair<ABHCharacter*, bool>>& RevisionParticipants, bool bActiveRevisionMode, bool bVisualAnswer = false);
	void QueueAdaptiveQuestionForParticipants(const TArray<ABHCharacter*>& Participants, bool bLastAnswerCorrect);
	int32 ResolveRevisionQuestionTarget() const;
	FString GetActionVerb() const;
	FString GetStationName() const;
	void ApplyStationVisuals();

	UFUNCTION()
	void OnRep_StationVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FixtureA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FixtureB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FixtureC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FixtureD;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FixtureE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StatusLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ProgressBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ProgressFill;

	UPROPERTY(ReplicatedUsing = OnRep_StationVisuals, BlueprintReadOnly, Category = "Objective")
	EBHObjectiveStationType StationType;

	UPROPERTY(ReplicatedUsing = OnRep_StationVisuals, BlueprintReadOnly, Category = "Objective")
	float WorkProgress;

	UPROPERTY(ReplicatedUsing = OnRep_StationVisuals, BlueprintReadOnly, Category = "Objective")
	bool bCompleted;

	UPROPERTY(ReplicatedUsing = OnRep_StationVisuals, BlueprintReadOnly, Category = "Objective")
	bool bDirectorActive;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionTopic;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionPrompt;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	TArray<FString> QuestionChoices;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	int32 CorrectAnswerIndex;

	UPROPERTY(ReplicatedUsing = OnRep_StationVisuals, BlueprintReadOnly, Category = "Question")
	bool bQuestionSolved;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionHint;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionFeedback;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	bool bQuestionFeedbackCorrect;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString RevisionQuestionId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	EBHQuestionType QuestionType;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	EBHQuestionDifficulty QuestionDifficulty;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	EBHDiagramType QuestionDiagramType;

	// Optional per-question diagram parameters so the HUD can render the question's actual
	// values (answer-safe: givens only). Empty => generic schematic.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FBHDiagramParams QuestionDiagram;

	// Shuffled draggable pieces + fixed drop-target slots for interactive matching/ordering. Display
	// data only (no answer): the shuffle hides the order, and grading re-parses the server-only
	// correct choice. Empty for non-arrangement questions.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	TArray<FString> InteractivePieces;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	TArray<FString> InteractiveSlots;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionSubtopic;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionFormula;

	// Target numeric answer for Calculation questions (0 for non-numeric questions).
	// Lets the HUD diagram annotate the actual value the player must compute and
	// backs the typed numeric-entry validation path.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	float QuestionNumericAnswer;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	float QuestionNumericTolerance;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionExplanation;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString RevisionTeamSummary;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	int32 RevisionQuestionsSolved;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	int32 RevisionQuestionsRequired;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	int32 RevisionQuestionStep;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	EBHRevisionCounterNodeType RevisionCounterType;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	bool bTeacherMirrorTrapNode;

	// True when the current question was re-surfaced from a participant's review
	// queue (a previously missed question). Drives the HUD "review" framing.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	bool bRevisionReviewQuestion;

	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	float WorkSeconds;

	TSet<TWeakObjectPtr<ABHCharacter>> Workers;
	TMap<int32, int32> RevisionTeamVotes;
	TSet<int32> RevisionTeamPlayerIds;
	TSet<TWeakObjectPtr<ABHCharacter>> PendingCorrectionCharacters;
	bool bUseAdaptiveQuestionOverride;
	EBHPhysicsTopic AdaptiveQuestionTopic;
	EBHQuestionDifficulty AdaptiveQuestionDifficulty;
	// Server-only: question ID pulled from the lowest-mastery participant's review
	// queue during adaptive planning, consumed by the next ConfigureQuestion.
	FString PendingReviewQuestionId;
	float LastNoiseTime;
	// Per-player answer throttle keyed by PlayerId. A single shared timestamp would let one
	// student's press eat every other student's press inside the 0.45s window, silently dropping
	// votes when a whole class answers together. Mirrors BHTrainBonusQuestionTerminal.
	TMap<int32, float> LastAnswerTimeByPlayerId;
	// Server-only anti-gaming state. After a wrong answer the station holds answer submission
	// until this server time so the student reads the correction instead of brute-forcing the
	// four choices; the hold escalates with consecutive wrong answers. Neither pins the player
	// in place — they can always walk away from the station.
	float CorrectionHoldUntil = 0.0f;
	int32 ConsecutiveWrongAtStation = 0;
};
