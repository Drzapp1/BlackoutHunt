// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "SBHClassroomBoard.h"

#include "BHGameInstance.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FSlateBrush* BoardWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	}

	FSlateFontInfo BoardFont(const int32 Size, const FName Typeface = FName(TEXT("Regular")))
	{
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}

	FString BoardClock(const int32 TotalSeconds)
	{
		const int32 ClampedSeconds = FMath::Max(0, TotalSeconds);
		return FString::Printf(TEXT("%02d:%02d"), ClampedSeconds / 60, ClampedSeconds % 60);
	}

	FString BoardTrainPhaseName(const EBHTrainPhase TrainPhase)
	{
		switch (TrainPhase)
		{
		case EBHTrainPhase::Arrival:
			return TEXT("Train: Arrival");
		case EBHTrainPhase::Recap:
			return TEXT("Train: Class Recap");
		case EBHTrainPhase::BonusQuestion:
			return TEXT("Train: Bonus Question");
		case EBHTrainPhase::Shop:
			return TEXT("Train: Shop");
		case EBHTrainPhase::StationStop:
			return TEXT("Train: Station Stop");
		case EBHTrainPhase::Departing:
			return TEXT("Train: Departing");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("Train");
		}
	}

}

void SBHClassroomBoard::Construct(const FArguments& InArgs)
{
	PlayerController = InArgs._PlayerController;
	bStandaloneWindow = InArgs._bStandaloneWindow;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(BoardWhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.010f, 0.014f, 0.018f, 0.98f))
		.Padding(bStandaloneWindow ? FMargin(22.0f, 18.0f) : FMargin(12.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(BoardFont(bStandaloneWindow ? 30 : 20, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.86f, 1.0f, 0.94f, 1.0f))
						.Text(FText::FromString(TEXT("CLASSROOM BOARD")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(BoardFont(bStandaloneWindow ? 13 : 10))
						.ColorAndOpacity(FLinearColor(0.62f, 0.72f, 0.72f, 1.0f))
						.Text(FText::FromString(TEXT("Host projector view. Tactical locations and correct answers stay hidden.")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(BoardFont(bStandaloneWindow ? 16 : 11, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.95f, 0.80f, 0.40f, 1.0f))
					.Text(this, &SBHClassroomBoard::GetJoinText)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					BuildMetricCard(FText::FromString(TEXT("Session")), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHClassroomBoard::GetSessionText)), FLinearColor(0.28f, 0.74f, 0.68f, 1.0f))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					BuildMetricCard(FText::FromString(TEXT("Roster")), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHClassroomBoard::GetRosterText)), FLinearColor(0.92f, 0.70f, 0.30f, 1.0f))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					BuildMetricCard(FText::FromString(TEXT("Roles")), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHClassroomBoard::GetRoleMixText)), FLinearColor(0.78f, 0.44f, 0.88f, 1.0f))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					BuildMetricCard(FText::FromString(TEXT("Revision")), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHClassroomBoard::GetRevisionText)), FLinearColor(0.48f, 0.82f, 0.42f, 1.0f))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(this, &SBHClassroomBoard::GetHuntStatusVisibility)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMetricCard(FText::FromString(TEXT("In Play")), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHClassroomBoard::GetInPlayText)), FLinearColor(0.28f, 0.74f, 0.68f, 1.0f))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMetricCard(FText::FromString(TEXT("Caught")), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHClassroomBoard::GetCaughtText)), FLinearColor(0.94f, 0.32f, 0.28f, 1.0f))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						BuildMetricCard(FText::FromString(TEXT("Escaped")), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHClassroomBoard::GetEscapedText)), FLinearColor(0.56f, 0.94f, 0.48f, 1.0f))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(this, &SBHClassroomBoard::GetPresenceVisibility)
				[
					SNew(SBorder)
					.BorderImage(BoardWhiteBrush())
					.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.98f))
					.Padding(FMargin(12.0f, 9.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(bStandaloneWindow ? 200.0f : 150.0f)
							[
								SNew(STextBlock)
								.Font(BoardFont(bStandaloneWindow ? 14 : 10, FName(TEXT("Bold"))))
								.ColorAndOpacity(FLinearColor(0.95f, 0.42f, 0.28f, 1.0f))
								.Text(this, &SBHClassroomBoard::GetPresenceLabelText)
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						.Padding(8.0f, 0.0f)
						[
							SNew(SBox).HeightOverride(bStandaloneWindow ? 16.0f : 11.0f)
							[
								SNew(SProgressBar)
								.Percent(TAttribute<TOptional<float>>::Create(
									TAttribute<TOptional<float>>::FGetter::CreateSP(this, &SBHClassroomBoard::GetPresencePercent)))
								.FillColorAndOpacity(FLinearColor(0.88f, 0.22f, 0.18f, 1.0f))
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TopicBarsBox, SVerticalBox)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(BoardWhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.030f, 0.038f, 0.044f, 0.95f))
				.Padding(FMargin(12.0f, 10.0f))
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(BoardFont(bStandaloneWindow ? 16 : 12, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.88f, 0.93f, 0.88f, 1.0f))
					.Text(this, &SBHClassroomBoard::GetObjectiveText)
				]
			]
		]
	];

	RebuildPlayerRows();
	LastRosterSignature = BuildRosterSignature();
}

void SBHClassroomBoard::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Building the per-player roster signature is non-trivial (a Printf and string append per student),
	// so only recompute it at ~4Hz rather than every frame. Rows still rebuild immediately on any change.
	constexpr float RosterSignatureCheckInterval = 0.25f;
	RosterSignatureCheckAccumulator += InDeltaTime;
	if (RosterSignatureCheckAccumulator < RosterSignatureCheckInterval)
	{
		return;
	}
	RosterSignatureCheckAccumulator = 0.0f;

	const FString RosterSignature = BuildRosterSignature();
	if (RosterSignature != LastRosterSignature)
	{
		LastRosterSignature = RosterSignature;
		RebuildPlayerRows();
	}
}

const ABHGameState* SBHClassroomBoard::GetBHGameState() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	return World ? World->GetGameState<ABHGameState>() : nullptr;
}

void SBHClassroomBoard::RebuildPlayerRows()
{
	RebuildTopicBars();
}

EVisibility SBHClassroomBoard::GetHuntStatusVisibility() const
{
	const ABHGameState* GS = GetBHGameState();
	if (!GS || GS->RoundPhase == EBHRoundPhase::Lobby)
	{
		return EVisibility::Collapsed;
	}
	return EVisibility::Visible;
}

namespace
{
	// Count students (non-bot, non-Hunter, non-Spectator) by life state.
	void BoardCountStudentLifeStates(const ABHGameState* GS, int32& OutAlive, int32& OutCaught, int32& OutEscaped)
	{
		OutAlive = OutCaught = OutEscaped = 0;
		if (!GS) return;
		for (APlayerState* Raw : GS->PlayerArray)
		{
			const ABHPlayerState* PS = Cast<ABHPlayerState>(Raw);
			if (!PS || PS->IsABot()
				|| PS->PlayerRole == EBHPlayerRole::Hunter
				|| PS->PlayerRole == EBHPlayerRole::Spectator)
			{
				continue;
			}
			if (PS->LifeState == EBHPlayerLifeState::Captured) ++OutCaught;
			else if (PS->LifeState == EBHPlayerLifeState::Escaped) ++OutEscaped;
			else ++OutAlive;
		}
	}
}

FText SBHClassroomBoard::GetInPlayText() const
{
	int32 Alive, Caught, Escaped;
	BoardCountStudentLifeStates(GetBHGameState(), Alive, Caught, Escaped);
	return FText::FromString(FString::FromInt(Alive));
}

FText SBHClassroomBoard::GetCaughtText() const
{
	int32 Alive, Caught, Escaped;
	BoardCountStudentLifeStates(GetBHGameState(), Alive, Caught, Escaped);
	return FText::FromString(FString::FromInt(Caught));
}

FText SBHClassroomBoard::GetEscapedText() const
{
	int32 Alive, Caught, Escaped;
	BoardCountStudentLifeStates(GetBHGameState(), Alive, Caught, Escaped);
	return FText::FromString(FString::FromInt(Escaped));
}

EVisibility SBHClassroomBoard::GetPresenceVisibility() const
{
	const ABHGameState* GS = GetBHGameState();
	return (GS && GS->RoundPhase == EBHRoundPhase::Hunt) ? EVisibility::Visible : EVisibility::Collapsed;
}

TOptional<float> SBHClassroomBoard::GetPresencePercent() const
{
	const ABHGameState* GS = GetBHGameState();
	return GS ? TOptional<float>(FMath::Clamp(GS->PresenceLevel / 100.0f, 0.0f, 1.0f)) : TOptional<float>();
}

FText SBHClassroomBoard::GetPresenceLabelText() const
{
	const ABHGameState* GS = GetBHGameState();
	const float Level = GS ? GS->PresenceLevel : 0.0f;
	return FText::FromString(FString::Printf(TEXT("PRESENCE  %.0f%%"), Level));
}

void SBHClassroomBoard::RebuildTopicBars()
{
	if (!TopicBarsBox.IsValid())
	{
		return;
	}

	TopicBarsBox->ClearChildren();

	const ABHGameState* GameState = GetBHGameState();
	if (!GameState || !GameState->bRevisionMode)
	{
		return;
	}

	// Compute per-topic class averages from replicated player states (skip bots + spectators).
	float TopicSum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	int32 StudentCount = 0;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* PS = Cast<ABHPlayerState>(RawPS);
		if (!PS || PS->IsABot() || PS->PlayerRole == EBHPlayerRole::Spectator)
		{
			continue;
		}
		const FBHPlayerRevisionStats& S = PS->RevisionStats;
		TopicSum[0] += S.ForcesMastery;
		TopicSum[1] += S.ElectricityMastery;
		TopicSum[2] += S.WavesMastery;
		TopicSum[3] += S.EnergyMastery;
		++StudentCount;
	}
	float TopicAvg[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	if (StudentCount > 0)
	{
		for (int32 i = 0; i < 4; i++)
		{
			TopicAvg[i] = TopicSum[i] / StudentCount;
		}
	}

	const int32 WeakIndex = static_cast<int32>(GameState->RevisionWeakTopic);

	const TCHAR* TopicLabels[4] = {TEXT("Forces & Motion"), TEXT("Electricity"), TEXT("Waves"), TEXT("Energy")};
	const FLinearColor NormalBarColor(0.28f, 0.74f, 0.68f, 1.0f);
	const FLinearColor WeakBarColor(0.95f, 0.58f, 0.20f, 1.0f);
	const FLinearColor NormalLabelColor(0.74f, 0.84f, 0.84f, 1.0f);
	const FLinearColor WeakLabelColor(0.95f, 0.75f, 0.30f, 1.0f);

	const int32 LabelSize = bStandaloneWindow ? 14 : 10;
	const int32 ValueSize = bStandaloneWindow ? 14 : 10;
	const float LabelWidth = bStandaloneWindow ? 160.0f : 120.0f;
	const float ValueWidth = bStandaloneWindow ? 52.0f : 40.0f;
	const float BarHeight = bStandaloneWindow ? 18.0f : 13.0f;

	TSharedRef<SVerticalBox> BarsContainer = SNew(SVerticalBox);

	// Header row
	BarsContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
			.Font(BoardFont(bStandaloneWindow ? 12 : 9, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.48f, 0.82f, 0.42f, 1.0f))
			.Text(FText::FromString(TEXT("TOPIC BREAKDOWN")))
		];

	for (int32 i = 0; i < 4; i++)
	{
		const bool bIsWeak = (i == WeakIndex);
		const float Pct = TopicAvg[i];
		const FLinearColor BarColor = bIsWeak ? WeakBarColor : NormalBarColor;
		const FLinearColor LabelColor = bIsWeak ? WeakLabelColor : NormalLabelColor;
		const FString PctStr = FString::Printf(TEXT("%.0f%%"), Pct);
		const FString LabelStr = bIsWeak
			? FString::Printf(TEXT("%s [weak]"), TopicLabels[i])
			: FString(TopicLabels[i]);

		BarsContainer->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, bStandaloneWindow ? 7.0f : 5.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(LabelWidth)
					[
						SNew(STextBlock)
						.Font(BoardFont(LabelSize, FName(TEXT("Bold"))))
						.ColorAndOpacity(LabelColor)
						.Text(FText::FromString(LabelStr))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(6.0f, 0.0f)
				[
					SNew(SBox).HeightOverride(BarHeight)
					[
						SNew(SProgressBar)
						.Percent(TOptional<float>(Pct / 100.0f))
						.FillColorAndOpacity(BarColor)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(ValueWidth)
					[
						SNew(STextBlock)
						.Font(BoardFont(ValueSize, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.90f, 0.96f, 0.94f, 1.0f))
						.Text(FText::FromString(PctStr))
					]
				]
			];
	}

	TopicBarsBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(BoardWhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.98f))
			.Padding(FMargin(12.0f, 10.0f))
			[
				BarsContainer
			]
		];
}

FString SBHClassroomBoard::BuildRosterSignature() const
{
	const ABHGameState* GameState = GetBHGameState();
	if (!GameState)
	{
		return TEXT("no-session");
	}

	FString Signature = FString::Printf(
		TEXT("phase=%d;time=%d;rev=%d;class=%.1f;weak=%d;players=%d"),
		static_cast<int32>(GameState->RoundPhase),
		GameState->RemainingTime,
		GameState->bRevisionMode ? 1 : 0,
		GameState->RevisionClassMasteryAverage,
		static_cast<int32>(GameState->RevisionWeakTopic),
		GameState->PlayerArray.Num());

	for (APlayerState* RawPlayerState : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPlayerState = Cast<ABHPlayerState>(RawPlayerState);
		if (!BHPlayerState)
		{
			continue;
		}

		const FBHPlayerRevisionStats& Stats = BHPlayerState->RevisionStats;
		Signature += FString::Printf(
			TEXT("|%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%.1f:%.1f:%.1f:%.1f:%.1f"),
			*BHPlayerState->GetPlayerName(),
			BHPlayerState->bReady ? 1 : 0,
			static_cast<int32>(BHPlayerState->PlayerRole),
			static_cast<int32>(BHPlayerState->DesiredRole),
			static_cast<int32>(BHPlayerState->SpectatorRolePreference),
			BHPlayerState->SpectatorEncouragementCount,
			static_cast<int32>(BHPlayerState->LifeState),
			Stats.Attempts,
			Stats.CorrectAnswers,
			Stats.CorrectionsCompleted,
			Stats.ContributionCount,
			Stats.MasteryPercent,
			Stats.ForcesMastery,
			Stats.ElectricityMastery,
			Stats.WavesMastery,
			Stats.EnergyMastery);
	}

	return Signature;
}

TSharedRef<SWidget> SBHClassroomBoard::BuildMetricCard(const FText& Label, const TAttribute<FText>& Value, const FLinearColor& AccentColor) const
{
	return SNew(SBorder)
		.BorderImage(BoardWhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.98f))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(BoardFont(bStandaloneWindow ? 12 : 9, FName(TEXT("Bold"))))
				.ColorAndOpacity(AccentColor)
				.Text(Label)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(BoardFont(bStandaloneWindow ? 17 : 12, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.90f, 0.96f, 0.94f, 1.0f))
				.Text(Value)
			]
		];
}

FText SBHClassroomBoard::GetSessionText() const
{
	const ABHGameState* GameState = GetBHGameState();
	if (!GameState)
	{
		return FText::FromString(TEXT("No session"));
	}

	// During the train intermission the round phase is static, so show the active train phase
	// and its own countdown instead of the (paused) round timer. Keeps the projector informative
	// between rounds.
	if (GameState->RoundPhase == EBHRoundPhase::Intermission && GameState->TrainPhase != EBHTrainPhase::Inactive)
	{
		const FString TrainPhaseText = BoardTrainPhaseName(GameState->TrainPhase);
		const float Now = GameState->GetServerWorldTimeSeconds();
		const int32 TrainRemaining = FMath::CeilToInt(FMath::Max(0.0f, GameState->TrainPhaseEndServerTime - Now));
		return FText::FromString(FString::Printf(TEXT("%s  %s"), *TrainPhaseText, *BoardClock(TrainRemaining)));
	}

	const FString PhaseText = GameState->GetPhaseText();
	FString TimerText = GameState->bPracticeMode ? TEXT("Practice") : (GameState->bTestMode ? TEXT("Test") : BoardClock(GameState->RemainingTime));
	if (GameState->RoundModifier != EBHRoundModifier::None)
	{
		TimerText += FString::Printf(TEXT("  [%s]"), *GameState->GetRoundModifierText());
	}
	return FText::FromString(FString::Printf(TEXT("%s  %s"), *PhaseText, *TimerText));
}

FText SBHClassroomBoard::GetRosterText() const
{
	const ABHGameState* GameState = GetBHGameState();
	if (!GameState)
	{
		return FText::FromString(TEXT("0 connected"));
	}

	int32 Humans = 0;
	int32 Bots = 0;
	int32 Ready = 0;
	for (APlayerState* RawPlayerState : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPlayerState = Cast<ABHPlayerState>(RawPlayerState);
		if (!BHPlayerState)
		{
			continue;
		}
		if (BHPlayerState->IsABot())
		{
			++Bots;
		}
		else
		{
			++Humans;
			if (BHPlayerState->bReady)
			{
				++Ready;
			}
		}
	}

	if (GameState->RoundPhase == EBHRoundPhase::Lobby)
	{
		const int32 NotReady = Humans - Ready;
		FString Msg = FString::Printf(TEXT("%d/%d ready"), Ready, Humans);
		if (NotReady > 0)
		{
			Msg += FString::Printf(TEXT("  (%d to go)"), NotReady);
		}
		if (Bots > 0)
		{
			Msg += FString::Printf(TEXT("  %d bots"), Bots);
		}
		return FText::FromString(Msg);
	}

	if (GameState->RoundPhase == EBHRoundPhase::Prep)
	{
		// Warmup coverage for the host: how many students have tried their full role checklist, so
		// the teacher knows when the class is ready to start the Hunt. Aggregate only (projector-safe,
		// no per-student detail). Read server-side on the host, so owner-only replication is fine.
		int32 WarmedUp = 0;
		for (APlayerState* RawPlayerState : GameState->PlayerArray)
		{
			const ABHPlayerState* BHPlayerState = Cast<ABHPlayerState>(RawPlayerState);
			if (BHPlayerState && !BHPlayerState->IsABot() && BHPlayerState->bWarmupComplete)
			{
				++WarmedUp;
			}
		}
		return FText::FromString(FString::Printf(TEXT("Warmup tried: %d/%d  %d bots"), WarmedUp, Humans, Bots));
	}

	return FText::FromString(FString::Printf(TEXT("%d humans  %d bots"), Humans, Bots));
}

FText SBHClassroomBoard::GetRoleMixText() const
{
	const ABHGameState* GameState = GetBHGameState();
	if (!GameState)
	{
		return FText::FromString(TEXT("No roles"));
	}

	int32 Teachers = 0;
	int32 Students = 0;
	int32 Monitors = 0;
	int32 Spectators = 0;
	int32 Captured = 0;
	int32 Escaped = 0;
	for (APlayerState* RawPlayerState : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPlayerState = Cast<ABHPlayerState>(RawPlayerState);
		if (!BHPlayerState)
		{
			continue;
		}

		if (BHPlayerState->PlayerRole == EBHPlayerRole::Hunter)
		{
			++Teachers;
		}
		else if (BHPlayerState->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			++Monitors;
		}
		else if (BHPlayerState->PlayerRole == EBHPlayerRole::Survivor || BHPlayerState->PlayerRole == EBHPlayerRole::Unassigned)
		{
			++Students;
		}
		else if (BHPlayerState->PlayerRole == EBHPlayerRole::Spectator)
		{
			++Spectators;
		}

		if (BHPlayerState->LifeState == EBHPlayerLifeState::Captured)
		{
			++Captured;
		}
		else if (BHPlayerState->LifeState == EBHPlayerLifeState::Escaped)
		{
			++Escaped;
		}
	}

	return FText::FromString(FString::Printf(TEXT("%d teachers  %d students  %d monitors  %d spectators  %d/%d done"), Teachers, Students, Monitors, Spectators, Escaped, Captured + Escaped));
}

FText SBHClassroomBoard::GetObjectiveText() const
{
	const ABHGameState* GameState = GetBHGameState();
	if (!GameState)
	{
		return FText::FromString(TEXT("Host a classroom session to populate the board."));
	}

	// During the train intermission show the auto-generated class recap instead of
	// the normal objective — useful for teacher-led discussion between rounds.
	if (GameState->RoundPhase == EBHRoundPhase::Intermission && !GameState->TrainRecapOverview.IsEmpty())
	{
		return FText::FromString(GameState->TrainRecapOverview);
	}

	return FText::FromString(FString::Printf(
		TEXT("%s  |  %s"),
		*GameState->GetPublicObjectiveActionText(),
		*GameState->GetObjectiveProgressText()));
}

FText SBHClassroomBoard::GetRevisionText() const
{
	const ABHGameState* GameState = GetBHGameState();
	if (!GameState || !GameState->bRevisionMode)
	{
		return FText::FromString(TEXT("Off"));
	}

	// Count students who hit the contribution gate and those below the class average.
	int32 Contributed = 0;
	int32 BelowAvg = 0;
	int32 StudentCount = 0;
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		const ABHPlayerState* PS = Cast<ABHPlayerState>(Raw);
		if (!PS || PS->IsABot()
			|| PS->PlayerRole == EBHPlayerRole::Hunter
			|| PS->PlayerRole == EBHPlayerRole::Spectator)
		{
			continue;
		}
		++StudentCount;
		if (PS->RevisionStats.ContributionCount >= GameState->RevisionContributionTarget)
		{
			++Contributed;
		}
		if (GameState->RevisionClassMasteryAverage > 0.0f
			&& PS->RevisionStats.MasteryPercent < GameState->RevisionClassMasteryAverage)
		{
			++BelowAvg;
		}
	}

	const FString WeakTopic = FBHRevisionQuestionBank::TopicToString(GameState->RevisionWeakTopic);
	FString Text = FString::Printf(TEXT("%.0f%% class  %s weak  contributed %d/%d  %d below avg"),
		GameState->RevisionClassMasteryAverage, *WeakTopic, Contributed, StudentCount, BelowAvg);

	if (GameState->RevisionReviewTimeRemaining > 0)
	{
		Text += FString::Printf(TEXT("  review %ds"), GameState->RevisionReviewTimeRemaining);
	}

	return FText::FromString(Text);
}

FText SBHClassroomBoard::GetJoinText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (World && World->GetNetMode() == NM_Client)
	{
		return FText::FromString(TEXT("CLIENT VIEW"));
	}

	const UBHGameInstance* BHGI = World ? World->GetGameInstance<UBHGameInstance>() : nullptr;
	const FString JoinAddress = BHGI ? BHGI->GetPreferredClassroomJoinAddress(7777) : FString(TEXT("127.0.0.1:7777"));
	return FText::FromString(FString::Printf(TEXT("JOIN %s"), *JoinAddress));
}
