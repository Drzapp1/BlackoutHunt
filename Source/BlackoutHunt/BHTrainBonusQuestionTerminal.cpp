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
#include "BHDiagramRenderer.h"
#include "BHRevisionQuestionBank.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

// Opt-in: draw the live diagram render-target plane on the terminal. Default off so shipped play
// uses the verified text "Given:" fold; enable to use/tune the 3D diagram surface in-editor without
// a rebuild. (The plane's exact transform is a quick in-editor tweak.)
static TAutoConsoleVariable<int32> CVarBHTerminalDiagramRT(
	TEXT("bh.Diagrams.TerminalRT"), 0,
	TEXT("1 = draw the diagram render-target plane on the bonus terminal (needs M_BH_DiagramRT); 0 = text-only."),
	ECVF_Default);

ABHTrainBonusQuestionTerminal::ABHTrainBonusQuestionTerminal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	InteractionLabel = FText::FromString(TEXT("Bonus Question"));
	SetActorScale3D(FVector(1.05f, 0.24f, 1.35f));
	bFeedbackCorrect = false;

	// Prompt/choices on the +Y FRONT face (reader's side) so the body never occludes them; same +Y facing as
	// before so they read correctly (not mirrored). Prompt above, choices spaced below.
	PromptText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PromptText"));
	PromptText->SetupAttachment(SceneRoot);
	BHPropVisuals::ConfigureReadableText(PromptText, FVector(-58.0f, 75.0f, 52.0f), FRotator(0.0f, 90.0f, 0.0f), 12.0f, FColor(255, 210, 92));

	ChoicesText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ChoicesText"));
	ChoicesText->SetupAttachment(SceneRoot);
	BHPropVisuals::ConfigureReadableText(ChoicesText, FVector(-58.0f, 75.0f, 26.0f), FRotator(0.0f, 90.0f, 0.0f), 9.5f, FColor(220, 242, 232));

	DiagramPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiagramPlane"));
	DiagramPlane->SetupAttachment(SceneRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		DiagramPlane->SetStaticMesh(PlaneMesh.Object);
	}
	DiagramPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DiagramPlane->SetCastShadow(false);
	// Sits above the prompt, facing the player like the text. Transform is a quick in-editor tweak;
	// the plane is hidden unless bh.Diagrams.TerminalRT is enabled and M_BH_DiagramRT loads.
	DiagramPlane->SetRelativeLocation(FVector(-57.0f, -30.0f, 150.0f));
	DiagramPlane->SetRelativeRotation(FRotator(90.0f, 90.0f, 0.0f));
	DiagramPlane->SetRelativeScale3D(FVector(1.0f, 0.5f, 1.0f));
	DiagramPlane->SetVisibility(false);
}

void ABHTrainBonusQuestionTerminal::BeginPlay()
{
	Super::BeginPlay();
	// Display only; a dedicated server never needs the render target.
	if (IsNetMode(NM_DedicatedServer) || !DiagramPlane)
	{
		return;
	}
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BlackoutHunt/Art/Diagrams/M_BH_DiagramRT.M_BH_DiagramRT"));
	if (!BaseMaterial)
	{
		// Graceful: no display material -> no plane; the text "Given:" fold is the experience.
		return;
	}
	DiagramRT = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), 512, 256);
	if (!DiagramRT)
	{
		return;
	}
	DiagramRT->ClearColor = FLinearColor(0.02f, 0.025f, 0.028f, 1.0f);
	DiagramMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (DiagramMID)
	{
		DiagramMID->SetTextureParameterValue(TEXT("RT"), DiagramRT);
		DiagramPlane->SetMaterial(0, DiagramMID);
	}
}

void ABHTrainBonusQuestionTerminal::RefreshDiagramTexture()
{
	if (!DiagramRT || !GetWorld() || Question.DiagramType == EBHDiagramType::None)
	{
		return;
	}
	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize(0.0f, 0.0f);
	FDrawToRenderTargetContext DrawContext;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, DiagramRT, Canvas, CanvasSize, DrawContext);
	if (Canvas)
	{
		FBHDiagramDrawContext Ctx;
		Ctx.Scale = CanvasSize.Y / 130.0f;
		Ctx.Font = GEngine ? GEngine->GetSmallFont() : nullptr;
		Ctx.TimeSeconds = 0.0f;
		Ctx.bEnhanced = FBHDiagramRenderer::IsEnhanced();
		// Match the HUD: hide editorial concept captions on recall/identify questions.
		Ctx.bShowConceptCaptions = (Question.Type == EBHQuestionType::Calculation || Question.Type == EBHQuestionType::DragDropMatching || Question.Type == EBHQuestionType::Ordering);
		// Draws the same answer-safe picture as the HUD; the answer is never on the diagram.
		FBHDiagramRenderer::Draw(Canvas, Question.DiagramType, Question.Diagram, Question.Subtopic, Question.Answer.Formula, nullptr, 0.0f, 0.0f, CanvasSize.X, CanvasSize.Y, Ctx);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, DrawContext);
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
	// Per-player throttle: the whole class shares one terminal, so a single shared cooldown would
	// silently drop a second student's answer that lands within 0.35s of the first.
	const int32 PlayerId = BHPS->GetPlayerId();
	if (Now - LastAnswerTimeByPlayerId.FindRef(PlayerId) < 0.35f)
	{
		return false;
	}
	LastAnswerTimeByPlayerId.Add(PlayerId, Now);

	// One answer per student per shared question: prevents point-farming and keeps the prompt stable
	// for everyone else still reading it (we no longer re-roll the shared question on each answer).
	if (PlayersAnsweredCurrentQuestion.Contains(PlayerId))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("You already answered this bonus question - wait for the next round's terminal."), 2.5f);
		}
		return false;
	}

	// Per-player anti-gaming correction hold: after a wrong answer, block THIS student's resubmission
	// for a few seconds so they read the correction instead of cycling 1-4. Never pins the player, and
	// per-player so one student's wrong answer cannot hold up the rest of the class.
	const float CorrectionHoldUntil = CorrectionHoldUntilByPlayerId.FindRef(PlayerId);
	if (Now < CorrectionHoldUntil)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Read the correction first. Retry in %.0fs."), FMath::Max(0.0f, CorrectionHoldUntil - Now) + 0.5f), 2.0f);
		}
		return false;
	}

	if (!ServerQuestion.Answer.Choices.IsValidIndex(ServerQuestion.Answer.CorrectChoiceIndex))
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

	const FString CorrectChoice = ServerQuestion.Answer.Choices[ServerQuestion.Answer.CorrectChoiceIndex];
	const bool bCorrect = AnswerIndex == ServerQuestion.Answer.CorrectChoiceIndex;
	const FString SelectedAnswer = ServerQuestion.Answer.Choices.IsValidIndex(AnswerIndex) ? ServerQuestion.Answer.Choices[AnswerIndex] : FString();

	// Route the answer through the unified classroom authority so bonus answers BUILD topic
	// mastery (and therefore count toward the class escape) just like station answers — but pass
	// bCountsAsContribution=false so they do not satisfy the team-station hall-monitor gate.
	// RecordRevisionAnswer also owns the spaced-repetition queue + attempt telemetry in revision
	// mode and returns the bonus shop points it awarded.
	ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr;
	int32 Awarded = 0;
	if (BHGM && BHGM->IsRevisionMode())
	{
		Awarded = BHGM->RecordRevisionAnswer(Character, ServerQuestion, bCorrect, bCurrentQuestionIsReview, SelectedAnswer, FString(), /*bCountsAsContribution=*/false, /*bBonusPoints=*/true);
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
		// Hold this student's resubmission so they read the correction before retrying.
		CorrectionHoldUntilByPlayerId.Add(PlayerId, Now + 4.0f);
		FeedbackText = FString::Printf(TEXT("Wrong. No bonus points; pressure rises and mastery dipped. Answer: %s. %s Read the correction before retrying."), *CorrectChoice, *Question.Explanation);
	}
	bFeedbackCorrect = bCorrect;

	if (PC)
	{
		PC->ClientShowStatusMessage(FeedbackText.Left(180), 4.0f);
	}

	// Mark this student done with the current shared question. We intentionally do NOT re-roll the
	// shared question here: doing so swapped the prompt out from under every other reader and pushed
	// this player's result onto the whole class. The missed question still returns via spaced repetition.
	PlayersAnsweredCurrentQuestion.Add(PlayerId);
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
		ServerQuestion = NewQuestion;
		Question = NewQuestion;
		// The built-in bank stores every question's correct choice at index 0 with no shuffle, so merely
		// zeroing the replicated CorrectChoiceIndex (below) would still leave Choices[0] == the correct
		// answer on the wire -- a free answer any client could read (even unmodified: the HUD labels
		// Choices[0] as option 1). Rotate the choices by a per-question amount (mirrors ABHObjectiveStation's
		// ChoiceRotation): the server-only ServerQuestion used for grading carries the true rotated index,
		// while the replicated Question carries the same rotated choices with its index scrubbed, so the
		// correct answer no longer sits at a fixed/known position on the wire.
		if (NewQuestion.Answer.Choices.Num() > 1 && NewQuestion.Answer.Choices.IsValidIndex(NewQuestion.Answer.CorrectChoiceIndex))
		{
			const int32 ChoiceCount = NewQuestion.Answer.Choices.Num();
			// Per-question, non-zero rotation in [1, ChoiceCount-1]; unsigned to avoid negative-modulo UB.
			// Derived from the question id (varies per question) XOR the selection seed, so it isn't a
			// constant "always pick slot N" offset a student could memorize across questions.
			const uint32 RotationSeed = GetTypeHash(NewQuestion.Id) ^ static_cast<uint32>(Seed);
			const int32 Rotation = 1 + static_cast<int32>(RotationSeed % static_cast<uint32>(ChoiceCount - 1));
			TArray<FString> RotatedChoices;
			RotatedChoices.Reserve(ChoiceCount);
			for (int32 ChoiceIndex = 0; ChoiceIndex < ChoiceCount; ++ChoiceIndex)
			{
				RotatedChoices.Add(NewQuestion.Answer.Choices[(ChoiceIndex + Rotation) % ChoiceCount]);
			}
			ServerQuestion.Answer.Choices = RotatedChoices;
			ServerQuestion.Answer.CorrectChoiceIndex = ((NewQuestion.Answer.CorrectChoiceIndex - Rotation) % ChoiceCount + ChoiceCount) % ChoiceCount;
			Question.Answer.Choices = RotatedChoices;
		}
		// Scrub the answer from the replicated copy so it never travels to clients. A fixed 0 keeps
		// the existing IsValidIndex() readiness checks true while revealing nothing (grading uses
		// ServerQuestion). Defeats reading the correct answer off the wire to farm the economy.
		Question.Answer.CorrectChoiceIndex = 0;
		Question.Answer.NumericAnswer = 0.0f;
		bCurrentQuestionIsReview = bReviewSelected;
		// A new shared question is shown: everyone may answer it once. Also clear the per-player throttle
		// maps so they cannot accumulate stale entries for departed/reconnected PlayerIds over a long host
		// session — the cooldown / correction-hold only need to be valid for the currently shown question.
		PlayersAnsweredCurrentQuestion.Reset();
		LastAnswerTimeByPlayerId.Reset();
		CorrectionHoldUntilByPlayerId.Reset();
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
			// Axis captions are answer-safe givens (they name the quantities, never the answer), so
			// fold them in too -- they are what a graph-reading question relies on when the picture
			// is not drawn. (The shape variant / ray angle are deliberately NOT folded: for "identify
			// the shape/angle" questions they would be the answer.)
			if (!Question.Diagram.XAxis.IsEmpty() && !Question.Diagram.YAxis.IsEmpty())
			{
				Parts.Add(FString::Printf(TEXT("graph %s vs %s"), *Question.Diagram.YAxis, *Question.Diagram.XAxis));
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

	// Render-to-texture diagram surface (opt-in via bh.Diagrams.TerminalRT). Redraws only when the
	// shown question changes; hidden when off, not live, or the question has no diagram.
	if (DiagramPlane && DiagramMID)
	{
		const bool bShowDiagram = CVarBHTerminalDiagramRT.GetValueOnGameThread() != 0
			&& bBonusLive && bQuestionReady && Question.DiagramType != EBHDiagramType::None;
		DiagramPlane->SetVisibility(bShowDiagram);
		if (bShowDiagram)
		{
			const FString DiagramKey = FString::Printf(TEXT("%d|%s"), static_cast<int32>(Question.DiagramType), *Question.Subtopic);
			if (DiagramKey != LastDrawnDiagramKey)
			{
				LastDrawnDiagramKey = DiagramKey;
				RefreshDiagramTexture();
			}
		}
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
