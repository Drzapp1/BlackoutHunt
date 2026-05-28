#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTypes.h"
#include "BHTrainBonusQuestionTerminal.generated.h"

class UTextRenderComponent;
class ABHPlayerState;

UCLASS()
class BLACKOUTHUNT_API ABHTrainBonusQuestionTerminal : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainBonusQuestionTerminal();

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

	UPROPERTY(ReplicatedUsing = OnRep_Question, BlueprintReadOnly, Category = "Question")
	FBHRevisionQuestion Question;

	UPROPERTY(ReplicatedUsing = OnRep_Question, BlueprintReadOnly, Category = "Question")
	FString FeedbackText;

	UPROPERTY(ReplicatedUsing = OnRep_Question, BlueprintReadOnly, Category = "Question")
	bool bFeedbackCorrect;

	float LastAnswerServerTime;
};
