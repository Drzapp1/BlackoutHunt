// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTypes.h"
#include "BHTrainBonusQuestionTerminal.generated.h"

class UTextRenderComponent;
class UStaticMeshComponent;
class UCanvasRenderTarget2D;
class UMaterialInstanceDynamic;
class ABHPlayerState;

UCLASS()
class BLACKOUTHUNT_API ABHTrainBonusQuestionTerminal : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainBonusQuestionTerminal();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	bool SubmitAnswer(ABHCharacter* Character, int32 AnswerIndex);
	void LoadQuestion(EBHPhysicsTopic PreferredTopic, int32 Seed, const ABHPlayerState* AdaptivePlayerState = nullptr, bool bLastAnswerCorrect = true);
	void RefreshDisplay();

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Train")
	FString GetQuestionPrompt() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Train")
	FString GetQuestionChoice(int32 ChoiceIndex) const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Train")
	int32 GetQuestionChoiceCount() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Train")
	FString GetQuestionFeedback() const;

protected:
	UFUNCTION()
	void OnRep_Question();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> PromptText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> ChoicesText;

	// Render-to-texture diagram surface. The plane shows DiagramRT via a dynamic instance of
	// M_BH_DiagramRT; FBHDiagramRenderer draws the same picture the HUD does. All optional: if the
	// material/asset is missing the plane stays hidden and the text "Given:" fold is the experience.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DiagramPlane;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasRenderTarget2D> DiagramRT;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiagramMID;

	// Avoids re-rendering the target every refresh; only redraws when the shown question changes.
	FString LastDrawnDiagramKey;

	void RefreshDiagramTexture();

	// Replicated to clients for DISPLAY ONLY. Its answer fields (CorrectChoiceIndex / NumericAnswer)
	// are scrubbed before replication so a modified client cannot read the answer off the wire.
	UPROPERTY(ReplicatedUsing = OnRep_Question, BlueprintReadOnly, Category = "Question")
	FBHRevisionQuestion Question;

	// Server-only authoritative question (full answer). Used for grading; never replicated.
	UPROPERTY()
	FBHRevisionQuestion ServerQuestion;

	UPROPERTY(ReplicatedUsing = OnRep_Question, BlueprintReadOnly, Category = "Question")
	FString FeedbackText;

	UPROPERTY(ReplicatedUsing = OnRep_Question, BlueprintReadOnly, Category = "Question")
	bool bFeedbackCorrect;

	// Per-player server-side throttle (keyed by PlayerId) so one student's answer timing cannot drop
	// another student's answer — the single shared terminal is used by the whole class at once.
	TMap<int32, float> LastAnswerTimeByPlayerId;
	// Players who already answered the CURRENT shared question. Prevents point-farming and lets the
	// shown question stay stable for everyone instead of being re-rolled out from under other readers.
	// Cleared whenever a new question is loaded.
	TSet<int32> PlayersAnsweredCurrentQuestion;
	// True when the loaded question was re-surfaced from the player's spaced-repetition review
	// queue (a previously missed question), so a correct answer counts as a correction.
	bool bCurrentQuestionIsReview = false;
};
