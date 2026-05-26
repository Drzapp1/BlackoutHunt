#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTypes.h"
#include "BHObjectiveStation.generated.h"

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

	void Configure(EBHObjectiveStationType NewStationType);
	void ConfigureRevisionCounterNode(EBHRevisionCounterNodeType NewCounterType);
	void ConfigureTeacherMirrorTrapNode();
	void SetDirectorActive(bool bNewActive);
	bool SubmitAnswer(ABHCharacter* Character, int32 AnswerIndex);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsDirectorActive() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsCompleted() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	EBHObjectiveStationType GetStationType() const;

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

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionSubtopic() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Question")
	FString GetQuestionFormula() const;

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

protected:
	void CompleteObjective();
	void ConfigureQuestion();
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

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionSubtopic;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Question")
	FString QuestionFormula;

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

	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	float WorkSeconds;

	TSet<TWeakObjectPtr<ABHCharacter>> Workers;
	TMap<int32, int32> RevisionTeamVotes;
	TSet<int32> RevisionTeamPlayerIds;
	TSet<TWeakObjectPtr<ABHCharacter>> PendingCorrectionCharacters;
	bool bUseAdaptiveQuestionOverride;
	EBHPhysicsTopic AdaptiveQuestionTopic;
	EBHQuestionDifficulty AdaptiveQuestionDifficulty;
	float LastNoiseTime;
	float LastAnswerTime;
};
