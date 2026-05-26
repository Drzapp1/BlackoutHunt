#include "BHTrainBonusQuestionTerminal.h"
#include "BHCharacter.h"
#include "BHGameInstance.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPowerupComponent.h"
#include "BHPowerupLibrary.h"
#include "BHRevisionQuestionBank.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

ABHTrainBonusQuestionTerminal::ABHTrainBonusQuestionTerminal()
{
	InteractionLabel = FText::FromString(TEXT("Bonus Question"));
	SetActorScale3D(FVector(1.05f, 0.24f, 1.35f));
	LastAnswerServerTime = -100.0f;
	bFeedbackCorrect = false;

	PromptText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PromptText"));
	PromptText->SetupAttachment(SceneRoot);
	PromptText->SetRelativeLocation(FVector(-58.0f, -60.0f, 92.0f));
	PromptText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	PromptText->SetHorizontalAlignment(EHTA_Left);
	PromptText->SetVerticalAlignment(EVRTA_TextTop);
	PromptText->SetWorldSize(12.0f);
	PromptText->SetTextRenderColor(FColor(255, 210, 92));

	ChoicesText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ChoicesText"));
	ChoicesText->SetupAttachment(SceneRoot);
	ChoicesText->SetRelativeLocation(FVector(-58.0f, -61.0f, 34.0f));
	ChoicesText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	ChoicesText->SetHorizontalAlignment(EHTA_Left);
	ChoicesText->SetVerticalAlignment(EVRTA_TextTop);
	ChoicesText->SetWorldSize(9.5f);
	ChoicesText->SetTextRenderColor(FColor(220, 242, 232));
}

void ABHTrainBonusQuestionTerminal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainBonusQuestionTerminal, Question);
	DOREPLIFETIME(ABHTrainBonusQuestionTerminal, FeedbackText);
	DOREPLIFETIME(ABHTrainBonusQuestionTerminal, bFeedbackCorrect);
}

bool ABHTrainBonusQuestionTerminal::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHPS && BHPS->IsAliveSurvivor() && BHGS && BHGS->TrainPhase == EBHTrainPhase::BonusQuestion;
}

void ABHTrainBonusQuestionTerminal::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
	{
		PC->ClientShowStatusMessage(TEXT("Use 1-4 while looking at the bonus terminal."), 3.0f);
	}
}

FText ABHTrainBonusQuestionTerminal::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || BHGS->TrainPhase != EBHTrainPhase::BonusQuestion)
	{
		return FText::FromString(TEXT("Bonus Locked"));
	}
	return FText::FromString(TEXT("Bonus Question 1-4"));
}

bool ABHTrainBonusQuestionTerminal::SubmitAnswer(ABHCharacter* Character, int32 AnswerIndex)
{
	if (!HasAuthority() || !CanInteract_Implementation(Character))
	{
		return false;
	}

	ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr;
	if (!BHPS || !Question.Answer.Choices.IsValidIndex(AnswerIndex))
	{
		return false;
	}

	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	if (Now - LastAnswerServerTime < 0.35f)
	{
		return false;
	}
	LastAnswerServerTime = Now;

	const bool bCorrect = AnswerIndex == Question.Answer.CorrectChoiceIndex;
	const int32 Points = bCorrect ? FBHPowerupLibrary::QuestionPointValue(Question.Difficulty, true) : 0;
	if (bCorrect)
	{
		BHPS->AddQuestionPoints(Points);
		Character->RecoverStamina(10.0f);
		Character->AddFear(-5.0f);
		FeedbackText = FString::Printf(TEXT("Correct. +%d points. %s"), Points, *Question.Explanation);
	}
	else
	{
		Character->AddFear(4.0f);
		FeedbackText = FString::Printf(TEXT("Wrong. Answer: %s. %s"), *Question.Answer.Choices[Question.Answer.CorrectChoiceIndex], *Question.Explanation);
	}
	bFeedbackCorrect = bCorrect;

	FBHQuestionAttemptRecord Record;
	Record.PlayerName = BHPS->GetPlayerName();
	Record.QuestionId = Question.Id;
	Record.TopicName = Question.TopicName;
	Record.QuestionText = Question.Prompt;
	Record.QuestionSubtopic = Question.Subtopic;
	Record.SelectedAnswer = Question.Answer.Choices.IsValidIndex(AnswerIndex) ? Question.Answer.Choices[AnswerIndex] : FString();
	Record.CorrectAnswer = Question.Answer.Choices.IsValidIndex(Question.Answer.CorrectChoiceIndex) ? Question.Answer.Choices[Question.Answer.CorrectChoiceIndex] : FString();
	Record.Explanation = Question.Explanation;
	Record.Difficulty = Question.Difficulty;
	Record.QuestionType = Question.Type;
	Record.Topic = Question.Topic;
	Record.bCorrect = bCorrect;
	Record.PointsEarned = Points;
	Record.TimestampSeconds = Now;
	if (const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr)
	{
		Record.StageIndex = BHGS->TrainStageIndex;
	}
	if (const ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->GetAdaptiveRevisionPlan(BHPS, bCorrect, Record.AdaptiveRecommendedTopic, Record.AdaptiveRecommendedDifficulty, Record.AdaptiveReason);
	}
	if (UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr)
	{
		BHGI->RecordQuestionAttempt(Record);
	}

	if (PC)
	{
		PC->ClientShowStatusMessage(FeedbackText.Left(180), 4.0f);
	}

	LoadQuestion(Record.AdaptiveRecommendedTopic, FMath::Rand(), BHPS, bCorrect);
	return true;
}

void ABHTrainBonusQuestionTerminal::LoadQuestion(EBHPhysicsTopic PreferredTopic, int32 Seed, const ABHPlayerState* AdaptivePlayerState, bool bLastAnswerCorrect)
{
	FBHRevisionQuestion NewQuestion;
	bool bSelected = false;
	if (AdaptivePlayerState)
	{
		const ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr;
		EBHPhysicsTopic AdaptiveTopic = PreferredTopic;
		EBHQuestionDifficulty AdaptiveDifficulty = EBHQuestionDifficulty::Easy;
		FString AdaptiveReason;
		if (BHGM)
		{
			BHGM->GetAdaptiveRevisionPlan(AdaptivePlayerState, bLastAnswerCorrect, AdaptiveTopic, AdaptiveDifficulty, AdaptiveReason);
		}
		bSelected = FBHRevisionQuestionBank::SelectQuestionByDifficulty(AdaptiveTopic, AdaptiveDifficulty, Seed, NewQuestion);
	}

	if (!bSelected)
	{
		TArray<EBHPhysicsTopic> WeakTopics;
		WeakTopics.Add(PreferredTopic);
		bSelected = FBHRevisionQuestionBank::SelectQuestion(PreferredTopic, EBHRevisionDifficultyMix::Adaptive, Seed, WeakTopics, NewQuestion);
	}

	if (bSelected)
	{
		Question = NewQuestion;
	}
	RefreshDisplay();
}

void ABHTrainBonusQuestionTerminal::RefreshDisplay()
{
	if (PromptText)
	{
		const FString Header = FString::Printf(TEXT("BONUS %s\n%s"), *Question.TopicName.ToUpper(), *Question.Prompt.Left(180));
		PromptText->SetText(FText::FromString(Header));
	}
	if (ChoicesText)
	{
		TArray<FString> Lines;
		for (int32 Index = 0; Index < Question.Answer.Choices.Num() && Index < 4; ++Index)
		{
			Lines.Add(FString::Printf(TEXT("%d. %s"), Index + 1, *Question.Answer.Choices[Index].Left(94)));
		}
		if (!FeedbackText.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("\n%s"), *FeedbackText.Left(140)));
		}
		ChoicesText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
		ChoicesText->SetTextRenderColor(bFeedbackCorrect ? FColor(134, 255, 172) : FColor(220, 242, 232));
	}
}

FString ABHTrainBonusQuestionTerminal::GetQuestionPrompt() const
{
	return Question.Prompt;
}

FString ABHTrainBonusQuestionTerminal::GetQuestionChoice(int32 ChoiceIndex) const
{
	return Question.Answer.Choices.IsValidIndex(ChoiceIndex) ? Question.Answer.Choices[ChoiceIndex] : FString();
}

int32 ABHTrainBonusQuestionTerminal::GetQuestionChoiceCount() const
{
	return Question.Answer.Choices.Num();
}

FString ABHTrainBonusQuestionTerminal::GetQuestionFeedback() const
{
	return FeedbackText;
}

void ABHTrainBonusQuestionTerminal::OnRep_Question()
{
	RefreshDisplay();
}
