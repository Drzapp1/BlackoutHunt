#include "SBHClassroomBoard.h"

#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
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

	FString BoardRoleName(const EBHPlayerRole Role)
	{
		switch (Role)
		{
		case EBHPlayerRole::Hunter:
			return TEXT("Teacher");
		case EBHPlayerRole::FakeHunter:
			return TEXT("Monitor");
		case EBHPlayerRole::Survivor:
			return TEXT("Student");
		case EBHPlayerRole::Tester:
			return TEXT("Tester");
		case EBHPlayerRole::Spectator:
			return TEXT("Spectator");
		case EBHPlayerRole::Unassigned:
		default:
			return TEXT("Auto");
		}
	}

	FString BoardLifeName(const EBHPlayerLifeState LifeState)
	{
		switch (LifeState)
		{
		case EBHPlayerLifeState::Captured:
			return TEXT("Captured");
		case EBHPlayerLifeState::Escaped:
			return TEXT("Escaped");
		case EBHPlayerLifeState::Alive:
		default:
			return TEXT("In Play");
		}
	}

	FString BoardStatusName(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return TEXT("Unknown");
		}

		if (GameState && GameState->RoundPhase == EBHRoundPhase::Lobby)
		{
			return PlayerState->bReady ? TEXT("Ready") : TEXT("Waiting");
		}

		return BoardLifeName(PlayerState->LifeState);
	}

	FLinearColor BoardStatusAccent(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return FLinearColor(0.74f, 0.80f, 0.78f, 1.0f);
		}

		if (GameState && GameState->RoundPhase == EBHRoundPhase::Lobby)
		{
			return PlayerState->bReady
				? FLinearColor(0.58f, 0.94f, 0.64f, 1.0f)
				: FLinearColor(1.0f, 0.72f, 0.32f, 1.0f);
		}

		if (PlayerState->LifeState == EBHPlayerLifeState::Captured)
		{
			return FLinearColor(0.94f, 0.32f, 0.28f, 1.0f);
		}
		if (PlayerState->LifeState == EBHPlayerLifeState::Escaped)
		{
			return FLinearColor(0.56f, 0.94f, 0.48f, 1.0f);
		}
		return FLinearColor(0.84f, 0.88f, 0.82f, 1.0f);
	}

	FLinearColor BoardRoleAccent(const ABHPlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return FLinearColor(0.38f, 0.42f, 0.44f, 1.0f);
		}

		if (PlayerState->LifeState == EBHPlayerLifeState::Captured)
		{
			return FLinearColor(0.74f, 0.20f, 0.18f, 1.0f);
		}
		if (PlayerState->LifeState == EBHPlayerLifeState::Escaped)
		{
			return FLinearColor(0.44f, 0.78f, 0.38f, 1.0f);
		}

		switch (PlayerState->PlayerRole)
		{
		case EBHPlayerRole::Hunter:
			return FLinearColor(0.96f, 0.40f, 0.32f, 1.0f);
		case EBHPlayerRole::FakeHunter:
			return FLinearColor(0.95f, 0.58f, 0.20f, 1.0f);
		case EBHPlayerRole::Survivor:
			return FLinearColor(0.28f, 0.74f, 0.68f, 1.0f);
		case EBHPlayerRole::Tester:
			return FLinearColor(0.48f, 0.86f, 1.0f, 1.0f);
		case EBHPlayerRole::Spectator:
			return FLinearColor(0.42f, 0.44f, 0.50f, 1.0f);
		case EBHPlayerRole::Unassigned:
		default:
			return FLinearColor(0.76f, 0.70f, 0.40f, 1.0f);
		}
	}

	FString BoardPlayerName(const ABHPlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return TEXT("Player");
		}

		FString PlayerName = PlayerState->GetPlayerName().IsEmpty() ? TEXT("Player") : PlayerState->GetPlayerName();
		if (PlayerState->IsABot())
		{
			PlayerName += TEXT(" [BOT]");
		}
		return PlayerName;
	}

	FString BoardResolveLocalAddress()
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!SocketSubsystem)
		{
			return TEXT("127.0.0.1:7777");
		}

		bool bCanBindAll = false;
		const TSharedRef<FInternetAddr> LocalAddress = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
		if (!LocalAddress->IsValid())
		{
			return TEXT("127.0.0.1:7777");
		}

		return FString::Printf(TEXT("%s:7777"), *LocalAddress->ToString(false));
	}

	FString BoardProgressText(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return TEXT("-");
		}

		if (GameState && GameState->bRevisionMode)
		{
			const FBHPlayerRevisionStats& Stats = PlayerState->RevisionStats;
			return FString::Printf(
				TEXT("Answers %d/%d  Corrections %d  Contributions %d  Hints %d"),
				Stats.CorrectAnswers,
				Stats.Attempts,
				Stats.CorrectionsCompleted,
				Stats.ContributionCount,
				Stats.HintCount);
		}

		const FString VoteText = PlayerState->MapVote.IsEmpty()
			? FString(TEXT("No map vote"))
			: FString::Printf(TEXT("Map vote: %s"), *PlayerState->MapVote);
		return FString::Printf(TEXT("Queued: %s  %s"), *BoardRoleName(PlayerState->DesiredRole), *VoteText);
	}

	FString BoardMasteryText(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return TEXT("-");
		}

		if (!GameState || !GameState->bRevisionMode)
		{
			return PlayerState->bReady ? TEXT("Ready") : TEXT("Not ready");
		}

		const FBHPlayerRevisionStats& Stats = PlayerState->RevisionStats;
		return FString::Printf(
			TEXT("%.0f%%  F %.0f  E %.0f  W %.0f  En %.0f"),
			Stats.MasteryPercent,
			Stats.ForcesMastery,
			Stats.ElectricityMastery,
			Stats.WavesMastery,
			Stats.EnergyMastery);
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
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 14.0f, 0.0f, 7.0f)
			[
				BuildPlayerHeader()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(BoardWhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.016f, 0.020f, 0.024f, 0.96f))
				.Padding(8.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(PlayerRowsBox, SVerticalBox)
					]
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
	if (!PlayerRowsBox.IsValid())
	{
		return;
	}

	PlayerRowsBox->ClearChildren();

	const ABHGameState* GameState = GetBHGameState();
	if (!GameState)
	{
		PlayerRowsBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(BoardFont(bStandaloneWindow ? 16 : 12))
				.ColorAndOpacity(FLinearColor(0.68f, 0.76f, 0.76f, 1.0f))
				.Text(FText::FromString(TEXT("No active classroom session.")))
			];
		return;
	}

	int32 RowIndex = 0;
	for (APlayerState* RawPlayerState : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPlayerState = Cast<ABHPlayerState>(RawPlayerState);
		if (!BHPlayerState)
		{
			continue;
		}

		PlayerRowsBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				BuildPlayerRow(BHPlayerState, RowIndex)
			];
		++RowIndex;
	}

	if (RowIndex == 0)
	{
		PlayerRowsBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(BoardFont(bStandaloneWindow ? 16 : 12))
				.ColorAndOpacity(FLinearColor(0.68f, 0.76f, 0.76f, 1.0f))
				.Text(FText::FromString(TEXT("Waiting for students to join.")))
			];
	}
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
			TEXT("|%s:%d:%d:%d:%d:%d:%d:%d:%d:%.1f:%.1f:%.1f:%.1f:%.1f"),
			*BoardPlayerName(BHPlayerState),
			BHPlayerState->bReady ? 1 : 0,
			static_cast<int32>(BHPlayerState->PlayerRole),
			static_cast<int32>(BHPlayerState->DesiredRole),
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

TSharedRef<SWidget> SBHClassroomBoard::BuildPlayerHeader() const
{
	return SNew(SBorder)
		.BorderImage(BoardWhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.055f, 0.065f, 0.072f, 0.98f))
		.Padding(FMargin(10.0f, 7.0f))
		[
			SNew(SGridPanel)
			+ SGridPanel::Slot(0, 0).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 250.0f : 180.0f)
				[
					SNew(STextBlock).Font(BoardFont(bStandaloneWindow ? 12 : 9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.74f, 0.84f, 0.84f, 1.0f)).Text(FText::FromString(TEXT("Player")))
				]
			]
			+ SGridPanel::Slot(1, 0).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 120.0f : 90.0f)
				[
					SNew(STextBlock).Font(BoardFont(bStandaloneWindow ? 12 : 9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.74f, 0.84f, 0.84f, 1.0f)).Text(FText::FromString(TEXT("Role")))
				]
			]
			+ SGridPanel::Slot(2, 0).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 130.0f : 95.0f)
				[
					SNew(STextBlock).Font(BoardFont(bStandaloneWindow ? 12 : 9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.74f, 0.84f, 0.84f, 1.0f)).Text(FText::FromString(TEXT("Status")))
				]
			]
			+ SGridPanel::Slot(3, 0).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 410.0f : 300.0f)
				[
					SNew(STextBlock).Font(BoardFont(bStandaloneWindow ? 12 : 9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.74f, 0.84f, 0.84f, 1.0f)).Text(FText::FromString(TEXT("Progress")))
				]
			]
			+ SGridPanel::Slot(4, 0)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 230.0f : 170.0f)
				[
					SNew(STextBlock).Font(BoardFont(bStandaloneWindow ? 12 : 9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.74f, 0.84f, 0.84f, 1.0f)).Text(FText::FromString(TEXT("Mastery")))
				]
			]
		];
}

TSharedRef<SWidget> SBHClassroomBoard::BuildPlayerRow(const ABHPlayerState* PlayerState, int32 RowIndex) const
{
	const ABHGameState* GameState = GetBHGameState();
	const FLinearColor AccentColor = BoardRoleAccent(PlayerState);
	const FLinearColor RowColor = (RowIndex % 2 == 0)
		? FLinearColor(0.028f, 0.034f, 0.038f, 0.98f)
		: FLinearColor(0.038f, 0.044f, 0.048f, 0.98f);

	return SNew(SBorder)
		.BorderImage(BoardWhiteBrush())
		.BorderBackgroundColor(RowColor)
		.Padding(8.0f)
		[
			SNew(SGridPanel)
			+ SGridPanel::Slot(0, 0).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 250.0f : 180.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(BoardFont(bStandaloneWindow ? 15 : 11, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.90f, 0.96f, 0.94f, 1.0f))
					.Text(FText::FromString(BoardPlayerName(PlayerState)))
				]
			]
			+ SGridPanel::Slot(1, 0).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 120.0f : 90.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(BoardFont(bStandaloneWindow ? 14 : 10, FName(TEXT("Bold"))))
					.ColorAndOpacity(AccentColor)
					.Text(FText::FromString(BoardRoleName(PlayerState ? PlayerState->PlayerRole : EBHPlayerRole::Unassigned)))
				]
			]
			+ SGridPanel::Slot(2, 0).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 130.0f : 95.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(BoardFont(bStandaloneWindow ? 14 : 10, FName(TEXT("Bold"))))
					.ColorAndOpacity(BoardStatusAccent(GameState, PlayerState))
					.Text(FText::FromString(BoardStatusName(GameState, PlayerState)))
				]
			]
			+ SGridPanel::Slot(3, 0).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 410.0f : 300.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(BoardFont(bStandaloneWindow ? 13 : 9))
					.ColorAndOpacity(FLinearColor(0.74f, 0.82f, 0.80f, 1.0f))
					.Text(FText::FromString(BoardProgressText(GameState, PlayerState)))
				]
			]
			+ SGridPanel::Slot(4, 0)
			[
				SNew(SBox).WidthOverride(bStandaloneWindow ? 230.0f : 170.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(BoardFont(bStandaloneWindow ? 13 : 9, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.88f, 0.86f, 0.62f, 1.0f))
					.Text(FText::FromString(BoardMasteryText(GameState, PlayerState)))
				]
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

	const FString PhaseText = GameState->GetPhaseText();
	const FString TimerText = GameState->bPracticeMode ? TEXT("Practice") : (GameState->bTestMode ? TEXT("Test") : BoardClock(GameState->RemainingTime));
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
		return FText::FromString(FString::Printf(TEXT("%d/%d humans ready  %d bots"), Ready, Humans, Bots));
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

		if (BHPlayerState->LifeState == EBHPlayerLifeState::Captured)
		{
			++Captured;
		}
		else if (BHPlayerState->LifeState == EBHPlayerLifeState::Escaped)
		{
			++Escaped;
		}
	}

	return FText::FromString(FString::Printf(TEXT("%d teachers  %d students  %d monitors  %d/%d done"), Teachers, Students, Monitors, Escaped, Captured + Escaped));
}

FText SBHClassroomBoard::GetObjectiveText() const
{
	const ABHGameState* GameState = GetBHGameState();
	if (!GameState)
	{
		return FText::FromString(TEXT("Host a classroom session to populate the board."));
	}

	const FString Objective = GameState->ObjectiveText.IsEmpty() ? TEXT("Restore power and reach the exit.") : GameState->ObjectiveText;
	const FString ExitText = GameState->bExitUnlocked ? TEXT("Exit open") : TEXT("Exit locked");
	return FText::FromString(FString::Printf(
		TEXT("%s  |  Power %d/%d  Side Tasks %d/%d  |  %s"),
		*Objective,
		GameState->BreakersCompleted,
		GameState->BreakersRequired,
		GameState->SideObjectivesCompleted,
		GameState->SideObjectivesRequired,
		*ExitText));
}

FText SBHClassroomBoard::GetRevisionText() const
{
	const ABHGameState* GameState = GetBHGameState();
	if (!GameState || !GameState->bRevisionMode)
	{
		return FText::FromString(TEXT("Off"));
	}

	const FString WeakTopic = FBHRevisionQuestionBank::TopicToString(GameState->RevisionWeakTopic);
	if (GameState->RevisionReviewTimeRemaining > 0)
	{
		return FText::FromString(FString::Printf(TEXT("%.0f%% class  %s weak  review %ds"), GameState->RevisionClassMasteryAverage, *WeakTopic, GameState->RevisionReviewTimeRemaining));
	}

	return FText::FromString(FString::Printf(TEXT("%.0f%% class  %s weak"), GameState->RevisionClassMasteryAverage, *WeakTopic));
}

FText SBHClassroomBoard::GetJoinText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (World && World->GetNetMode() == NM_Client)
	{
		return FText::FromString(TEXT("CLIENT VIEW"));
	}

	return FText::FromString(FString::Printf(TEXT("JOIN %s"), *BoardResolveLocalAddress()));
}
