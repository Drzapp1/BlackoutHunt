#include "BHTrainBonusQuestionTerminal.h"
#include "BHCharacter.h"
#include "BHGameInstance.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPowerupComponent.h"
#include "BHPowerupLibrary.h"
#include "BHPropVisuals.h"
#include "BHRevisionQuestionBank.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

ABHTrainBonusQuestionTerminal::ABHTrainBonusQuestionTerminal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	InteractionLabel = FText::FromString(TEXT("Bonus Question"));
	SetActorScale3D(FVector(1.05f, 0.24f, 1.35f));
	LastAnswerServerTime = -100.0f;
	bFeedbackCorrect = false;

	PromptText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PromptText"));
	PromptText->SetupAttachment(SceneRoot);
	BHPropVisuals::ConfigureReadableText(PromptText, FVector(-58.0f, -60.0f, 92.0f), FRotator(0.0f, 90.0f, 0.0f), 12.0f, FColor(255, 210, 92));

	ChoicesText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ChoicesText"));
	ChoicesText->SetupAttachment(SceneRoot);
	BHPropVisuals::ConfigureReadableText(ChoicesText, FVector(-58.0f, -61.0f, 34.0f), FRotator(0.0f, 90.0f, 0.0f), 9.5f, FColor(220, 242, 232));
}

void ABHTrainBonusQuestionTerminal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (BHGS
		&& (BHGS->TrainPhase == EBHTrainPhase::Recap
			|| BHGS->TrainPhase == EBHTrainPhase::BonusQuestion
			|| BHGS->TrainPhase == EBHTrainPhase::Shop))
	{
		RefreshDisplay();
	}
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
	return BHPS
		&& BHPS->IsAliveSurvivor()
		&& BHGS
		&& BHGS->TrainPhase == EBHTrainPhase::BonusQuestion
		&& !Question.Prompt.IsEmpty()
		&& Question.Answer.Choices.Num() > 0
		&& Question.Answer.Choices.IsValidIndex(Question.Answer.CorrectChoiceIndex);
}

void ABHTrainBonusQuestionTerminal::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
	{
		const int32 Reward = FBHPowerupLibrary::QuestionPointValue(Question.Difficulty, true);
		PC->ClientShowStatusMessage(FString::Printf(TEXT("Bonus live: use 1-4 while looking at the terminal. Correct awards %d shop points, stamina, and nerve relief."), Reward), 3.25f);
	}
}

FText ABHTrainBonusQuestionTerminal::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || BHGS->TrainPhase != EBHTrainPhase::BonusQuestion)
	{
		return FText::FromString(TEXT("Bonus Locked - Wait for Phase"));
	}
	return FText::FromString(TEXT("Bonus Question 1-4"));
}

FBHInteractionPromptInfo ABHTrainBonusQuestionTerminal::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (Info.bCanInteract)
	{
		const int32 Reward = FBHPowerupLibrary::QuestionPointValue(Question.Difficulty, true);
		const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		const float Now = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
		const int32 RemainingSeconds = BHGS ? FMath::Max(0, FMath::CeilToInt(BHGS->TrainPhaseEndServerTime - Now)) : 0;
		Info.RiskText = FText::FromString(FString::Printf(TEXT("ANSWER 1-4 / +%d PTS / %02d:%02d"),
			Reward,
			RemainingSeconds / 60,
			RemainingSeconds % 60));
		return Info;
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHGS || BHGS->TrainPhase != EBHTrainPhase::BonusQuestion)
	{
		Info.DisabledReason = FText::FromString(TEXT("BONUS PHASE ONLY"));
	}
	else if (!BHPS || !BHPS->IsAliveSurvivor())
	{
		Info.DisabledReason = FText::FromString(TEXT("SURVIVOR BONUS ONLY"));
	}
	else if (Question.Prompt.IsEmpty() || Question.Answer.Choices.Num() <= 0 || !Question.Answer.Choices.IsValidIndex(Question.Answer.CorrectChoiceIndex))
	{
		Info.DisabledReason = FText::FromString(TEXT("QUESTION SYNCING"));
	}
	else
	{
		Info.DisabledReason = FText::FromString(TEXT("BONUS LOCKED"));
	}
	return Info;
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

	// Anti-gaming correction hold: after a wrong answer, block resubmission for a few seconds so
	// the student reads the correction instead of cycling 1-4. Input-only; never pins the player.
	if (Now < CorrectionHoldUntil)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Read the correction first. Retry in %.0fs."), FMath::Max(0.0f, CorrectionHoldUntil - Now) + 0.5f), 2.0f);
		}
		return false;
	}

	if (!Question.Answer.Choices.IsValidIndex(Question.Answer.CorrectChoiceIndex))
	{
		FeedbackText = TEXT("Bonus question data was incomplete. Reloading terminal.");
		bFeedbackCorrect = false;
		RefreshDisplay();
		if (PC)
		{
			PC->ClientShowStatusMessage(FeedbackText, 2.75f);
		}
		LoadQuestion(GetWorld() && GetWorld()->GetGameState<ABHGameState>() ? GetWorld()->GetGameState<ABHGameState>()->RevisionWeakTopic : EBHPhysicsTopic::ForcesAndMotion, FMath::Rand(), BHPS, false);
		return false;
	}

	const FString CorrectChoice = Question.Answer.Choices[Question.Answer.CorrectChoiceIndex];
	const bool bCorrect = AnswerIndex == Question.Answer.CorrectChoiceIndex;
	const FString SelectedAnswer = Question.Answer.Choices.IsValidIndex(AnswerIndex) ? Question.Answer.Choices[AnswerIndex] : FString();

	// Route the answer through the unified classroom authority so bonus answers BUILD topic
	// mastery (and therefore count toward the class escape) just like station answers — but pass
	// bCountsAsContribution=false so they do not satisfy the team-station hall-monitor gate.
	// RecordRevisionAnswer also owns the spaced-repetition queue + attempt telemetry in revision
	// mode and returns the bonus shop points it awarded.
	ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr;
	int32 Awarded = 0;
	EBHPhysicsTopic NextTopic = Question.Topic;
	if (BHGM && BHGM->IsRevisionMode())
	{
		Awarded = BHGM->RecordRevisionAnswer(Character, Question, bCorrect, bCurrentQuestionIsReview, SelectedAnswer, FString(), /*bCountsAsContribution=*/false, /*bBonusPoints=*/true);
		EBHQuestionDifficulty NextDifficulty = EBHQuestionDifficulty::Easy;
		FString NextReason;
		BHGM->GetAdaptiveRevisionPlan(BHPS, bCorrect, NextTopic, NextDifficulty, NextReason);
	}
	else
	{
		// Non-revision fallback: award locally and keep the spaced-repetition queue.
		if (bCorrect)
		{
			Awarded = FBHPowerupLibrary::QuestionPointValue(Question.Difficulty, true);
			BHPS->AddQuestionPoints(Awarded);
			BHPS->DequeueRevisionReview(Question.Id);
		}
		else
		{
			BHPS->EnqueueRevisionReview(Question.Id);
		}
	}

	if (bCorrect)
	{
		Character->RecoverStamina(10.0f);
		Character->AddFear(-5.0f);
		FeedbackText = FString::Printf(TEXT("Correct. +%d shop points, stamina recovered, mastery banked. %s"), Awarded, *Question.Explanation);
	}
	else
	{
		Character->AddFear(4.0f);
		// Hold resubmission so the student reads the correction before retrying.
		CorrectionHoldUntil = Now + 4.0f;
		FeedbackText = FString::Printf(TEXT("Wrong. No bonus points; pressure rises and mastery dipped. Answer: %s. %s Read the correction before retrying."), *CorrectChoice, *Question.Explanation);
	}
	bFeedbackCorrect = bCorrect;

	if (PC)
	{
		PC->ClientShowStatusMessage(FeedbackText.Left(180), 4.0f);
	}

	LoadQuestion(NextTopic, FMath::Rand(), BHPS, bCorrect);
	return true;
}

void ABHTrainBonusQuestionTerminal::LoadQuestion(EBHPhysicsTopic PreferredTopic, int32 Seed, const ABHPlayerState* AdaptivePlayerState, bool bLastAnswerCorrect)
{
	FBHRevisionQuestion NewQuestion;
	bool bSelected = false;
	bool bReviewSelected = false;

	// Spaced repetition: re-ask a previously missed question before normal selection.
	if (AdaptivePlayerState)
	{
		const FString ReviewId = AdaptivePlayerState->PeekRevisionReview();
		if (!ReviewId.IsEmpty() && FBHRevisionQuestionBank::FindQuestion(ReviewId, NewQuestion))
		{
			bSelected = true;
			bReviewSelected = true;
		}
	}

	if (!bSelected && AdaptivePlayerState)
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
		bCurrentQuestionIsReview = bReviewSelected;
	}
	RefreshDisplay();
}

void ABHTrainBonusQuestionTerminal::RefreshDisplay()
{
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bBonusLive = BHGS && BHGS->TrainPhase == EBHTrainPhase::BonusQuestion;
	const bool bQuestionReady = !Question.Prompt.IsEmpty()
		&& Question.Answer.Choices.Num() > 0
		&& Question.Answer.Choices.IsValidIndex(Question.Answer.CorrectChoiceIndex);
	const int32 Reward = FBHPowerupLibrary::QuestionPointValue(Question.Difficulty, true);
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	const int32 RemainingSeconds = BHGS ? FMath::Max(0, FMath::CeilToInt(BHGS->TrainPhaseEndServerTime - Now)) : 0;
	if (PromptText)
	{
		const FString TopicName = Question.TopicName.IsEmpty()
			? (BHGS ? FBHRevisionQuestionBank::TopicToString(BHGS->RevisionWeakTopic) : FString(TEXT("WEAK TOPIC")))
			: Question.TopicName.ToUpper();
		// This terminal is text-only (no diagram), so fold any on-screen diagram givens
		// into the prompt text to keep visual questions answerable here.
		FString Givens;
		{
			const FString DiagramLabels[] = {Question.Diagram.LabelA, Question.Diagram.LabelB, Question.Diagram.LabelC, Question.Diagram.LabelD};
			TArray<FString> Parts;
			for (const FString& DiagramLabel : DiagramLabels)
			{
				if (!DiagramLabel.IsEmpty())
				{
					Parts.Add(DiagramLabel);
				}
			}
			if (Parts.Num() > 0)
			{
				Givens = FString::Printf(TEXT("\nGiven: %s"), *FString::Join(Parts, TEXT(", ")));
			}
		}
		const FString Header = bBonusLive
			? (bQuestionReady
				? FString::Printf(TEXT("BONUS LIVE %02d:%02d  +%d PTS\n%s\n%s%s"), RemainingSeconds / 60, RemainingSeconds % 60, Reward, *TopicName, *Question.Prompt.Left(160), *Givens)
				: FString::Printf(TEXT("BONUS SYNCING %02d:%02d\n%s\nQuestion loading. Watch this terminal."), RemainingSeconds / 60, RemainingSeconds % 60, *TopicName))
			: FString::Printf(TEXT("BONUS LOCKED\n%s\nOpens after recap. Watch train displays."), *TopicName);
		PromptText->SetText(FText::FromString(Header));
		PromptText->SetTextRenderColor(bBonusLive && bQuestionReady ? FColor(255, 210, 92) : FColor(186, 170, 126));
	}
	if (ChoicesText)
	{
		TArray<FString> Lines;
		if (bBonusLive && bQuestionReady)
		{
			Lines.Add(TEXT("Answer with 1-4. Correct lowers fear; wrong adds pressure."));
			Lines.Add(TEXT("This uses the same classroom revision authority as stations."));
			for (int32 Index = 0; Index < Question.Answer.Choices.Num() && Index < 4; ++Index)
			{
				Lines.Add(FString::Printf(TEXT("%d. %s"), Index + 1, *Question.Answer.Choices[Index].Left(86)));
			}
		}
		else if (bBonusLive)
		{
			Lines.Add(TEXT("Question syncing."));
			Lines.Add(TEXT("Wait for choices before answering."));
		}
		else if (BHGS && BHGS->TrainPhase == EBHTrainPhase::Shop)
		{
			Lines.Add(TEXT("Bonus phase closed."));
			Lines.Add(TEXT("Use shop terminals before the station stop."));
		}
		else
		{
			Lines.Add(TEXT("Stand by for weak-topic bonus questions."));
			Lines.Add(TEXT("The next phase uses the same classroom authority."));
		}
		if (!FeedbackText.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("\nLAST: %s"), *FeedbackText.Left(132)));
		}
		ChoicesText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
		ChoicesText->SetTextRenderColor(bFeedbackCorrect ? FColor(134, 255, 172) : (bBonusLive ? FColor(220, 242, 232) : FColor(170, 184, 178)));
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
