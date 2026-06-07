// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "SBHMainMenu.h"
#include "Animation/AnimSequence.h"
#include "BHAccountSubsystem.h"
#include "BHFeedbackSubsystem.h"
#include "BHGameSettings.h"
#include "BHGameInstance.h"
#include "BHGameState.h"
#include "BHJumpscareVariantLibrary.h"
#include "BHLessonPreset.h"
#include "BHNetworkSupport.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "SBHClassroomBoard.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Layout/Visibility.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TWeakObjectPtr<ABHPlayerController> GMenuButtonSoundController;

	constexpr int32 MenuDefaultGamePort = 7777;
	constexpr float MenuAvatarPreviewModelScale = 0.56f;
	constexpr float MenuAvatarPreviewCameraDistance = 212.0f;
	constexpr float MenuAvatarPreviewCameraHeight = 46.0f;
	constexpr float MenuAvatarPreviewCameraFov = 34.0f;

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	}

	FString MenuSpectatorRolePreferenceName(EBHPlayerRole Role)
	{
		switch (Role)
		{
		case EBHPlayerRole::Hunter:
			return TEXT("Teacher");
		case EBHPlayerRole::Survivor:
			return TEXT("Survivor");
		case EBHPlayerRole::FakeHunter:
			return TEXT("Hall Monitor");
		case EBHPlayerRole::Unassigned:
		default:
			return TEXT("No preference");
		}
	}

	FString MenuCleanRawJoinAddress(FString Address)
	{
		Address.TrimStartAndEndInline();
		Address.ReplaceInline(TEXT(" "), TEXT(""));
		Address.RemoveFromStart(TEXT("http://"), ESearchCase::IgnoreCase);
		Address.RemoveFromStart(TEXT("https://"), ESearchCase::IgnoreCase);

		int32 StripIndex = INDEX_NONE;
		const TCHAR Delimiters[] = { TCHAR('/'), TCHAR('?'), TCHAR('#') };
		for (const TCHAR Delimiter : Delimiters)
		{
			int32 CandidateIndex = INDEX_NONE;
			if (Address.FindChar(Delimiter, CandidateIndex)
				&& (StripIndex == INDEX_NONE || CandidateIndex < StripIndex))
			{
				StripIndex = CandidateIndex;
			}
		}

		if (StripIndex != INDEX_NONE)
		{
			Address = Address.Left(StripIndex);
		}

		return Address;
	}

	FString MenuCleanJoinAddress(FString Address)
	{
		return FBHNetworkSupport::NormalizeJoinAddress(Address);
	}

	bool MenuUrlOptionEnabled(const UWorld* World, const TCHAR* OptionName)
	{
		if (!World || !OptionName)
		{
			return false;
		}

		FString Value = World->URL.GetOption(OptionName, TEXT(""));
		Value.TrimStartAndEndInline();
		return Value.Equals(TEXT("1"))
			|| Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("on"), ESearchCase::IgnoreCase);
	}

	bool MenuTryParsePort(const FString& PortText, int32& OutPort)
	{
		FString TrimmedPort = PortText.TrimStartAndEnd();
		if (TrimmedPort.IsEmpty())
		{
			OutPort = MenuDefaultGamePort;
			return true;
		}

		for (const TCHAR Ch : TrimmedPort)
		{
			if (!FChar::IsDigit(Ch))
			{
				OutPort = MenuDefaultGamePort;
				return false;
			}
		}

		const int32 ParsedPort = FCString::Atoi(*TrimmedPort);
		if (ParsedPort < 1 || ParsedPort > 65535)
		{
			OutPort = MenuDefaultGamePort;
			return false;
		}

		OutPort = ParsedPort;
		return true;
	}

	FText MenuPortText(const int32 Port)
	{
		return FText::FromString(FString::FromInt(FMath::Clamp(Port <= 0 ? MenuDefaultGamePort : Port, 1, 65535)));
	}

	bool MenuHasSingleColon(const FString& Address, int32& OutColonIndex)
	{
		OutColonIndex = INDEX_NONE;
		if (!Address.FindChar(TEXT(':'), OutColonIndex))
		{
			return false;
		}

		int32 LastColonIndex = INDEX_NONE;
		Address.FindLastChar(TEXT(':'), LastColonIndex);
		return LastColonIndex == OutColonIndex;
	}

	bool MenuLooksLikeJoinInvite(const FString& Address)
	{
		return Address.Find(TEXT("BH1:"), ESearchCase::IgnoreCase) != INDEX_NONE
			|| Address.TrimStartAndEnd().StartsWith(TEXT("blackouthunt://join/"), ESearchCase::IgnoreCase);
	}

	bool MenuEntryHasExplicitPort(const FString& Address)
	{
		if (MenuLooksLikeJoinInvite(Address))
		{
			return !FBHNetworkSupport::NormalizeJoinAddress(Address).IsEmpty();
		}

		const FString CleanAddress = MenuCleanRawJoinAddress(Address);
		if (CleanAddress.StartsWith(TEXT("[")) && CleanAddress.Contains(TEXT("]:")))
		{
			return true;
		}

		int32 ColonIndex = INDEX_NONE;
		return MenuHasSingleColon(CleanAddress, ColonIndex);
	}

	FString MenuExtractHost(FString Address)
	{
		Address = MenuCleanJoinAddress(Address);
		if (Address.StartsWith(TEXT("[")))
		{
			int32 BracketIndex = INDEX_NONE;
			if (Address.FindChar(TEXT(']'), BracketIndex))
			{
				return Address.Mid(1, BracketIndex - 1);
			}
		}

		int32 ColonIndex = INDEX_NONE;
		if (MenuHasSingleColon(Address, ColonIndex))
		{
			return Address.Left(ColonIndex);
		}

		return Address;
	}

	int32 MenuExtractPort(FString Address)
	{
		Address = MenuCleanJoinAddress(Address);

		if (Address.StartsWith(TEXT("[")))
		{
			int32 BracketIndex = INDEX_NONE;
			if (Address.FindChar(TEXT(']'), BracketIndex) && Address.IsValidIndex(BracketIndex + 1) && Address[BracketIndex + 1] == TEXT(':'))
			{
				int32 ParsedPort = MenuDefaultGamePort;
				MenuTryParsePort(Address.Mid(BracketIndex + 2), ParsedPort);
				return ParsedPort;
			}
		}

		int32 ColonIndex = INDEX_NONE;
		if (MenuHasSingleColon(Address, ColonIndex))
		{
			int32 ParsedPort = MenuDefaultGamePort;
			MenuTryParsePort(Address.Mid(ColonIndex + 1), ParsedPort);
			return ParsedPort;
		}

		return MenuDefaultGamePort;
	}

	FString MenuBuildJoinAddress(const FString& Host, int32 Port)
	{
		FString CleanHost = MenuExtractHost(Host);
		CleanHost.TrimStartAndEndInline();
		if (CleanHost.IsEmpty())
		{
			return FString();
		}

		Port = FMath::Clamp(Port <= 0 ? MenuDefaultGamePort : Port, 1, 65535);
		if (CleanHost.Contains(TEXT(":")) && !CleanHost.StartsWith(TEXT("[")))
		{
			return FString::Printf(TEXT("[%s]:%d"), *CleanHost, Port);
		}

		return FString::Printf(TEXT("%s:%d"), *CleanHost, Port);
	}

	FString MenuNormalizeClassroomMapName(FString LevelName)
	{
		LevelName.TrimStartAndEndInline();
		if (LevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
		{
			return TEXT("Substation");
		}
		if (LevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || LevelName.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
		{
			return TEXT("Foggrounds");
		}
		return TEXT("Facility");
	}

	bool MenuRunbookPhaseIsAtLeastHunt(const EBHRoundPhase Phase)
	{
		return Phase == EBHRoundPhase::Hunt
			|| Phase == EBHRoundPhase::FinalEscape
			|| Phase == EBHRoundPhase::Intermission
			|| Phase == EBHRoundPhase::SurvivorsWin
			|| Phase == EBHRoundPhase::HunterWin;
	}

	bool MenuRunbookPhaseIsAfterSetup(const EBHRoundPhase Phase)
	{
		return Phase == EBHRoundPhase::Prep || MenuRunbookPhaseIsAtLeastHunt(Phase);
	}

	bool MenuRunbookPhaseIsPostRound(const EBHRoundPhase Phase)
	{
		return Phase == EBHRoundPhase::Intermission
			|| Phase == EBHRoundPhase::SurvivorsWin
			|| Phase == EBHRoundPhase::HunterWin;
	}

	FString MenuRunbookPhaseText(const ABHGameState* GameState)
	{
		return GameState ? GameState->GetPhaseText() : FString(TEXT("No session"));
	}

	struct FBHMenuRunbookRosterCounts
	{
		int32 HumanPlayers = 0;
		int32 HumanReady = 0;
		int32 HumanQueuedRoles = 0;
		int32 HumanTeacherQueued = 0;
		int32 ActiveTeachers = 0;
	};

	FBHMenuRunbookRosterCounts MenuBuildRunbookRosterCounts(const ABHGameState* GameState)
	{
		FBHMenuRunbookRosterCounts Counts;
		if (!GameState)
		{
			return Counts;
		}

		for (APlayerState* RawPlayerState : GameState->PlayerArray)
		{
			const ABHPlayerState* PlayerState = Cast<ABHPlayerState>(RawPlayerState);
			if (!PlayerState || PlayerState->IsABot())
			{
				continue;
			}

			++Counts.HumanPlayers;
			if (PlayerState->bReady)
			{
				++Counts.HumanReady;
			}
			if (PlayerState->DesiredRole != EBHPlayerRole::Unassigned)
			{
				++Counts.HumanQueuedRoles;
			}
			if (PlayerState->DesiredRole == EBHPlayerRole::Hunter)
			{
				++Counts.HumanTeacherQueued;
			}
			if (PlayerState->PlayerRole == EBHPlayerRole::Hunter)
			{
				++Counts.ActiveTeachers;
			}
		}

		return Counts;
	}

	FLinearColor MenuRunbookStatusCompleteColor()
	{
		return FLinearColor(0.48f, 0.86f, 0.60f, 1.0f);
	}

	FLinearColor MenuRunbookStatusWaitingColor()
	{
		return FLinearColor(0.94f, 0.72f, 0.38f, 1.0f);
	}

	FLinearColor MenuRunbookStatusNeutralColor()
	{
		return FLinearColor(0.62f, 0.74f, 0.72f, 1.0f);
	}

	FLinearColor MenuRunbookStatusBlockedColor()
	{
		return FLinearColor(0.86f, 0.58f, 0.34f, 1.0f);
	}

	int32 MenuAllRevisionTopicMask()
	{
		return FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::ForcesAndMotion)
			| FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Electricity)
			| FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Waves)
			| FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Energy);
	}

	FSlateFontInfo MenuFont(const int32 Size, const FName Typeface = FName(TEXT("Regular")))
	{
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}

	class SBHMenuButton : public SButton
	{
	public:
		SLATE_BEGIN_ARGS(SBHMenuButton)
			: _ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Button")))
			, _HAlign(HAlign_Fill)
			, _VAlign(VAlign_Fill)
			, _ContentPadding(FMargin(4.0f, 2.0f))
			, _ContentScale(FVector2D(1.0f, 1.0f))
			, _ButtonColorAndOpacity(FLinearColor::White)
			, _ForegroundColor(FSlateColor::UseStyle())
			, _IsFocusable(true)
		{
		}

			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
			SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
			SLATE_ARGUMENT(EVerticalAlignment, VAlign)
			SLATE_ATTRIBUTE(FMargin, ContentPadding)
			SLATE_ATTRIBUTE(FMargin, NormalPaddingOverride)
			SLATE_ATTRIBUTE(FMargin, PressedPaddingOverride)
			SLATE_EVENT(FOnClicked, OnClicked)
			SLATE_EVENT(FSimpleDelegate, OnPressed)
			SLATE_EVENT(FSimpleDelegate, OnReleased)
			SLATE_EVENT(FSimpleDelegate, OnHovered)
			SLATE_EVENT(FSimpleDelegate, OnUnhovered)
			SLATE_ATTRIBUTE(FVector2D, ContentScale)
			SLATE_ATTRIBUTE(FSlateColor, ButtonColorAndOpacity)
			SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)
			SLATE_ARGUMENT(bool, IsFocusable)
			SLATE_ARGUMENT(TOptional<FSlateSound>, PressedSoundOverride)
			SLATE_ARGUMENT(TOptional<FSlateSound>, ClickedSoundOverride)
			SLATE_ARGUMENT(TOptional<FSlateSound>, HoveredSoundOverride)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			const TSharedRef<bool> bPressed = MakeShared<bool>(false);
			const TSharedRef<bool> bHovered = MakeShared<bool>(false);
			const FSimpleDelegate ExternalOnPressed = InArgs._OnPressed;
			const FSimpleDelegate ExternalOnReleased = InArgs._OnReleased;
			const FSimpleDelegate ExternalOnHovered = InArgs._OnHovered;
			const FSimpleDelegate ExternalOnUnhovered = InArgs._OnUnhovered;
			const FOnClicked ExternalOnClicked = InArgs._OnClicked;

			SButton::Construct(SButton::FArguments()
				.ButtonStyle(InArgs._ButtonStyle)
				.HAlign(InArgs._HAlign)
				.VAlign(InArgs._VAlign)
				.ContentPadding(InArgs._ContentPadding)
				.NormalPaddingOverride(InArgs._NormalPaddingOverride)
				.PressedPaddingOverride(InArgs._PressedPaddingOverride)
				.OnPressed(FSimpleDelegate::CreateLambda([bPressed, ExternalOnPressed]()
				{
					*bPressed = true;
					ExternalOnPressed.ExecuteIfBound();
				}))
				.OnReleased(FSimpleDelegate::CreateLambda([bPressed, ExternalOnReleased]()
				{
					*bPressed = false;
					ExternalOnReleased.ExecuteIfBound();
				}))
				.OnHovered(FSimpleDelegate::CreateLambda([bHovered, ExternalOnHovered]()
				{
					*bHovered = true;
					ExternalOnHovered.ExecuteIfBound();
				}))
				.OnUnhovered(FSimpleDelegate::CreateLambda([bHovered, ExternalOnUnhovered]()
				{
					*bHovered = false;
					ExternalOnUnhovered.ExecuteIfBound();
				}))
				.OnClicked(FOnClicked::CreateLambda([ExternalOnClicked]()
				{
					if (ABHPlayerController* PC = GMenuButtonSoundController.Get())
					{
						PC->PlayMenuSelectionSound();
					}

					return ExternalOnClicked.IsBound() ? ExternalOnClicked.Execute() : FReply::Handled();
				}))
				.ContentScale(InArgs._ContentScale)
				.ButtonColorAndOpacity(InArgs._ButtonColorAndOpacity)
				.ForegroundColor(InArgs._ForegroundColor)
				.IsFocusable(InArgs._IsFocusable)
				.PressedSoundOverride(FSlateSound())
				.ClickedSoundOverride(FSlateSound())
				.HoveredSoundOverride(FSlateSound())
				.RenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::Create(
					TAttribute<TOptional<FSlateRenderTransform>>::FGetter::CreateLambda([bPressed, bHovered]()
					{
						const float Scale = *bPressed ? 1.035f : (*bHovered ? 1.012f : 1.0f);
						return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2D(Scale)));
					})))
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				[
					InArgs._Content.Widget
				]);
		}
	};

	FString MenuKeyDisplayName(const FKey& Key)
	{
		if (!Key.IsValid())
		{
			return TEXT("-");
		}

		const FText DisplayName = Key.GetDisplayName();
		return DisplayName.IsEmpty() ? Key.GetFName().ToString() : DisplayName.ToString();
	}

	void MenuAppendUniqueKey(TArray<FString>& Keys, const FString& KeyName)
	{
		if (!KeyName.IsEmpty() && !Keys.Contains(KeyName))
		{
			Keys.Add(KeyName);
		}
	}

	FString MenuDescribeActionChord(const FInputActionKeyMapping& Mapping)
	{
		TArray<FString> Parts;
		if (Mapping.bCtrl)
		{
			Parts.Add(TEXT("Ctrl"));
		}
		if (Mapping.bAlt)
		{
			Parts.Add(TEXT("Alt"));
		}
		if (Mapping.bShift)
		{
			Parts.Add(TEXT("Shift"));
		}
		if (Mapping.bCmd)
		{
			Parts.Add(TEXT("Cmd"));
		}

		Parts.Add(MenuKeyDisplayName(Mapping.Key));
		return FString::Join(Parts, TEXT("+"));
	}

	FString MenuDescribeActionBinding(const FName ActionName, const TCHAR* Fallback)
	{
		TArray<FString> Keys;
		if (const UInputSettings* InputSettings = GetDefault<UInputSettings>())
		{
			TArray<FInputActionKeyMapping> Mappings;
			InputSettings->GetActionMappingByName(ActionName, Mappings);
			for (const FInputActionKeyMapping& Mapping : Mappings)
			{
				MenuAppendUniqueKey(Keys, MenuDescribeActionChord(Mapping));
			}
		}

		return Keys.Num() > 0 ? FString::Join(Keys, TEXT(" / ")) : FString(Fallback);
	}

	FString MenuDescribeAxisBinding(const FName AxisName, float RequiredSign, const TCHAR* Fallback)
	{
		TArray<FString> Keys;
		if (const UInputSettings* InputSettings = GetDefault<UInputSettings>())
		{
			TArray<FInputAxisKeyMapping> Mappings;
			InputSettings->GetAxisMappingByName(AxisName, Mappings);
			for (const FInputAxisKeyMapping& Mapping : Mappings)
			{
				const bool bMatchesSign = RequiredSign >= 0.0f ? Mapping.Scale > 0.0f : Mapping.Scale < 0.0f;
				if (bMatchesSign)
				{
					MenuAppendUniqueKey(Keys, MenuKeyDisplayName(Mapping.Key));
				}
			}
		}

		return Keys.Num() > 0 ? FString::Join(Keys, TEXT(" / ")) : FString(Fallback);
	}

	TSharedRef<SWidget> MenuControlRow(const FText& Label, const FString& Keys, const FText& Detail)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.026f, 0.034f, 0.038f, 0.92f))
			.Padding(FMargin(9.0f, 7.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(150.0f)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.88f, 0.96f, 0.92f, 1.0f))
						.Text(Label)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(FLinearColor(0.070f, 0.088f, 0.094f, 1.0f))
					.Padding(FMargin(8.0f, 4.0f))
					[
						SNew(SBox)
						.WidthOverride(160.0f)
						[
							SNew(STextBlock)
							.Font(MenuFont(9, FName(TEXT("Bold"))))
							.ColorAndOpacity(FLinearColor(0.92f, 0.98f, 0.94f, 1.0f))
							.Text(FText::FromString(Keys))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(FLinearColor(0.64f, 0.74f, 0.73f, 1.0f))
					.Text(Detail)
				]
			];
	}

	FString MenuRevisionDifficultyMixText(EBHRevisionDifficultyMix DifficultyMix)
	{
		switch (DifficultyMix)
		{
		case EBHRevisionDifficultyMix::Easy:
			return TEXT("Easy");
		case EBHRevisionDifficultyMix::Hard:
			return TEXT("Hard");
		case EBHRevisionDifficultyMix::Adaptive:
			return TEXT("Adaptive");
		case EBHRevisionDifficultyMix::Balanced:
		default:
			return TEXT("Balanced");
		}
	}

	FString MenuRevisionScareIntensityText(int32 ScareIntensity)
	{
		switch (FMath::Clamp(ScareIntensity, 0, 3))
		{
		case 0:
			return TEXT("Off");
		case 1:
			return TEXT("Low");
		case 3:
			return TEXT("Chaos");
		case 2:
		default:
			return TEXT("Horror");
		}
	}

	FString MenuRevisionTopicMaskText(int32 TopicMask)
	{
		const int32 ForcesBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::ForcesAndMotion);
		const int32 ElectricityBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Electricity);
		const int32 WavesBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Waves);
		const int32 EnergyBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Energy);
		const int32 AllMask = ForcesBit | ElectricityBit | WavesBit | EnergyBit;
		const int32 Mask = TopicMask & AllMask;
		if (Mask == 0)
		{
			return TEXT("No topics");
		}
		if (Mask == AllMask)
		{
			return TEXT("All topics");
		}

		TArray<FString> Topics;
		if ((Mask & ForcesBit) != 0)
		{
			Topics.Add(TEXT("Forces"));
		}
		if ((Mask & ElectricityBit) != 0)
		{
			Topics.Add(TEXT("Electricity"));
		}
		if ((Mask & WavesBit) != 0)
		{
			Topics.Add(TEXT("Waves"));
		}
		if ((Mask & EnergyBit) != 0)
		{
			Topics.Add(TEXT("Energy"));
		}
		return FString::Join(Topics, TEXT(", "));
	}

	TSharedRef<SWidget> MenuPlayActionButton(
		const FText& Label,
		const FText& Description,
		const FLinearColor& AccentColor,
		const FOnClicked& OnClicked,
		const TAttribute<bool>& IsEnabled = TAttribute<bool>(true))
	{
		TSharedRef<SVerticalBox> ButtonContent = SNew(SVerticalBox);
		ButtonContent->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(12, FName(TEXT("Bold"))))
				.Text(Label)
			];

		if (!Description.IsEmpty())
		{
			ButtonContent->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(FLinearColor(0.76f, 0.84f, 0.82f, 1.0f))
					.Text(Description)
				];
		}

		return SNew(SBHMenuButton)
			.IsEnabled(IsEnabled)
			.ButtonColorAndOpacity(AccentColor)
			.ContentPadding(FMargin(12.0f, Description.IsEmpty() ? 8.0f : 9.0f))
			.OnClicked(OnClicked)
			[
				ButtonContent
			];
	}

	TSharedRef<SWidget> MenuPlayDropdownSection(
		const FText& Label,
		const FText& Summary,
		const FLinearColor& AccentColor,
		const TSharedRef<SWidget>& MenuContent)
	{
		return SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.AllowAnimatedTransition(true)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.040f, 0.048f, 0.056f, 0.98f))
			.BodyBorderImage(WhiteBrush())
			.BodyBorderBackgroundColor(FLinearColor(0.018f, 0.022f, 0.026f, 0.98f))
			.HeaderPadding(FMargin(12.0f, 8.0f))
			.Padding(FMargin(10.0f, 8.0f, 10.0f, 10.0f))
			.HeaderContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(5.0f)
					.HeightOverride(34.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(AccentColor)
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(MenuFont(13, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.92f, 0.98f, 0.94f, 1.0f))
						.Text(Label)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Font(MenuFont(10))
						.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.70f, 1.0f))
						.Text(Summary)
					]
				]
			]
			.BodyContent()
			[
				MenuContent
			];
	}

	void MenuAddPlayDropdownSection(
		const TSharedRef<SVerticalBox>& ActionList,
		const FText& Label,
		const FText& Summary,
		const FLinearColor& AccentColor,
		const TSharedRef<SWidget>& MenuContent)
	{
		ActionList->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				MenuPlayDropdownSection(Label, Summary, AccentColor, MenuContent)
			];
	}

	TSharedRef<SWidget> MenuGuideCallout(const FText& Label, const FText& Body, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.030f, 0.037f, 0.043f, 0.96f))
			.Padding(10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(5.0f)
						.HeightOverride(22.0f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(AccentColor)
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(MenuFont(12, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.90f, 0.98f, 0.94f, 1.0f))
						.Text(Label)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(FLinearColor(0.66f, 0.76f, 0.75f, 1.0f))
					.Text(Body)
				]
			];
	}

	TSharedRef<SWidget> MenuGuideKeyPill(const FText& Key, const FText& Body, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.024f, 0.030f, 0.036f, 0.94f))
			.Padding(FMargin(8.0f, 6.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(AccentColor)
					.Padding(FMargin(8.0f, 4.0f))
					[
						SNew(STextBlock)
						.Font(MenuFont(9, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.96f, 1.0f, 0.96f, 1.0f))
						.Text(Key)
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.77f, 1.0f))
					.Text(Body)
				]
			];
	}

	TSharedRef<SWidget> MenuGuideActionTile(const FText& Key, const FText& Label, const FText& Detail, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.018f, 0.024f, 0.030f, 0.96f))
			.Padding(9.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 7.0f, 0.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(AccentColor)
						.Padding(FMargin(8.0f, 4.0f))
						[
							SNew(STextBlock)
							.Font(MenuFont(10, FName(TEXT("Bold"))))
							.ColorAndOpacity(FLinearColor(0.98f, 1.0f, 0.96f, 1.0f))
							.Text(Key)
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(MenuFont(11, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.92f, 0.98f, 0.95f, 1.0f))
						.Text(Label)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(9))
					.ColorAndOpacity(FLinearColor(0.62f, 0.72f, 0.72f, 1.0f))
					.Text(Detail)
				]
			];
	}

	TSharedRef<SWidget> MenuGuideAnnotation(const FText& Label, const FText& Body, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.010f, 0.014f, 0.018f, 0.96f))
			.Padding(FMargin(9.0f, 7.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(9, FName(TEXT("Bold"))))
					.ColorAndOpacity(AccentColor)
					.Text(Label)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(8))
					.ColorAndOpacity(FLinearColor(0.72f, 0.80f, 0.78f, 1.0f))
					.Text(Body)
				]
			];
	}

	TSharedRef<SWidget> MenuGuideHudMeter(const FText& Label, const FText& Value, const float Fill, const FLinearColor& AccentColor)
	{
		const float FillWidth = FMath::Clamp(Fill, 0.0f, 1.0f) * 112.0f;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(7, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.68f, 0.76f, 0.74f, 1.0f))
					.Text(Label)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(MenuFont(7, FName(TEXT("Bold"))))
					.ColorAndOpacity(AccentColor)
					.Text(Value)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 4.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBox)
					.WidthOverride(112.0f)
					.HeightOverride(5.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.018f, 0.022f, 0.024f, 1.0f))
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
					.WidthOverride(FMath::Max(2.0f, FillWidth))
					.HeightOverride(5.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(AccentColor)
					]
				]
			];
	}

	TSharedRef<SWidget> MenuGuideFlowStep(const int32 Step, const FText& Label, const FText& Detail, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.022f, 0.030f, 0.035f, 0.96f))
			.Padding(10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(AccentColor)
						.Padding(FMargin(8.0f, 4.0f))
						[
							SNew(STextBlock)
							.Font(MenuFont(11, FName(TEXT("Bold"))))
							.ColorAndOpacity(FLinearColor(0.98f, 1.0f, 0.96f, 1.0f))
							.Text(FText::FromString(FString::Printf(TEXT("%d"), Step)))
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(MenuFont(11, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.92f, 0.98f, 0.94f, 1.0f))
						.Text(Label)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(9))
					.ColorAndOpacity(FLinearColor(0.66f, 0.76f, 0.74f, 1.0f))
					.Text(Detail)
				]
			];
	}

	TSharedRef<SWidget> MenuGuideChecklistItem(const FText& Text, const FLinearColor& AccentColor)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(0.0f, 1.0f, 7.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(AccentColor)
				.Padding(FMargin(4.0f, 1.0f))
				[
					SNew(STextBlock)
					.Font(MenuFont(8, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.98f, 1.0f, 0.96f, 1.0f))
					.Text(FText::FromString(TEXT(">")))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(9))
				.ColorAndOpacity(FLinearColor(0.70f, 0.80f, 0.78f, 1.0f))
				.Text(Text)
			];
	}

	TSharedRef<SWidget> MenuGuideRoleCard(const FText& Label, const FText& Tag, const TArray<FText>& Items, const FLinearColor& AccentColor)
	{
		TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
		for (const FText& Item : Items)
		{
			List->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					MenuGuideChecklistItem(Item, AccentColor)
				];
		}

		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.020f, 0.026f, 0.032f, 0.96f))
			.Padding(11.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Font(MenuFont(13, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.94f, 0.99f, 0.95f, 1.0f))
						.Text(Label)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.26f))
						.Padding(FMargin(7.0f, 3.0f))
						[
							SNew(STextBlock)
							.Font(MenuFont(8, FName(TEXT("Bold"))))
							.ColorAndOpacity(AccentColor)
							.Text(Tag)
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 9.0f, 0.0f, 0.0f)
				[
					List
				]
			];
	}

	TSharedRef<SWidget> MenuGuidePicturePart(float X, float Y, float W, float H, const FLinearColor& Color)
	{
		(void)X;
		(void)Y;
		return SNew(SBox)
			.WidthOverride(W)
			.HeightOverride(H)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(Color)
			];
	}

	TSharedRef<SWidget> MenuGuideObjectPicture(const FName Kind, const FLinearColor& AccentColor)
	{
		TSharedRef<SOverlay> Picture = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.012f, 0.016f, 0.018f, 1.0f))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(0.0f)
			[
				MenuGuidePicturePart(0.0f, 0.0f, 190.0f, 112.0f, FLinearColor(0.04f, 0.045f, 0.046f, 0.24f))
			];

		auto AddPart = [&Picture](float X, float Y, float W, float H, const FLinearColor& Color)
		{
			Picture->AddSlot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(X, Y, 0.0f, 0.0f)
				[
					MenuGuidePicturePart(X, Y, W, H, Color)
				];
		};

		auto AddLabel = [&Picture](float X, float Y, const FText& Label, const FLinearColor& Color, int32 Size = 8)
		{
			Picture->AddSlot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(X, Y, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(Size, FName(TEXT("Bold"))))
					.ColorAndOpacity(Color)
					.Text(Label)
				];
		};

		if (Kind == FName(TEXT("Station")))
		{
			AddPart(52.0f, 18.0f, 72.0f, 72.0f, FLinearColor(0.17f, 0.20f, 0.22f, 1.0f));
			AddPart(66.0f, 28.0f, 44.0f, 26.0f, FLinearColor(0.015f, 0.022f, 0.026f, 1.0f));
			AddPart(70.0f, 32.0f, 36.0f, 4.0f, AccentColor);
			AddPart(70.0f, 40.0f, 20.0f, 4.0f, FLinearColor(0.70f, 0.78f, 0.76f, 1.0f));
			AddPart(67.0f, 63.0f, 10.0f, 8.0f, FLinearColor(0.04f, 0.05f, 0.055f, 1.0f));
			AddPart(82.0f, 63.0f, 10.0f, 8.0f, FLinearColor(0.04f, 0.05f, 0.055f, 1.0f));
			AddPart(97.0f, 63.0f, 10.0f, 8.0f, FLinearColor(0.04f, 0.05f, 0.055f, 1.0f));
			AddPart(114.0f, 24.0f, 8.0f, 8.0f, AccentColor);
			AddPart(126.0f, 88.0f, 44.0f, 6.0f, FLinearColor(0.55f, 0.10f, 0.06f, 1.0f));
			AddPart(146.0f, 70.0f, 6.0f, 42.0f, FLinearColor(0.13f, 0.05f, 0.04f, 1.0f));
			AddPart(25.0f, 72.0f, 48.0f, 10.0f, FLinearColor(0.32f, 0.34f, 0.30f, 1.0f));
			AddPart(35.0f, 58.0f, 28.0f, 28.0f, FLinearColor(0.74f, 0.78f, 0.52f, 1.0f));
			AddLabel(70.0f, 77.0f, FText::FromString(TEXT("1-4")), AccentColor);
		}
		else if (Kind == FName(TEXT("Breaker")))
		{
			AddPart(60.0f, 14.0f, 68.0f, 88.0f, FLinearColor(0.18f, 0.20f, 0.22f, 1.0f));
			AddPart(65.0f, 20.0f, 58.0f, 78.0f, FLinearColor(0.26f, 0.27f, 0.26f, 1.0f));
			AddPart(68.0f, 30.0f, 52.0f, 9.0f, FLinearColor(0.86f, 0.62f, 0.14f, 1.0f));
			AddPart(84.0f, 45.0f, 20.0f, 20.0f, FLinearColor(0.06f, 0.07f, 0.075f, 1.0f));
			AddPart(93.0f, 50.0f, 3.0f, 15.0f, AccentColor);
			AddPart(76.0f, 76.0f, 6.0f, 20.0f, FLinearColor(0.08f, 0.09f, 0.10f, 1.0f));
			AddPart(92.0f, 76.0f, 6.0f, 20.0f, FLinearColor(0.08f, 0.09f, 0.10f, 1.0f));
			AddPart(108.0f, 76.0f, 6.0f, 20.0f, FLinearColor(0.08f, 0.09f, 0.10f, 1.0f));
			AddPart(70.0f, 22.0f, 6.0f, 6.0f, FLinearColor(0.80f, 0.12f, 0.08f, 1.0f));
			AddPart(82.0f, 22.0f, 6.0f, 6.0f, FLinearColor(0.80f, 0.12f, 0.08f, 1.0f));
			AddPart(94.0f, 22.0f, 6.0f, 6.0f, FLinearColor(0.80f, 0.12f, 0.08f, 1.0f));
		}
		else if (Kind == FName(TEXT("Battery")))
		{
			AddPart(34.0f, 46.0f, 118.0f, 31.0f, FLinearColor(0.12f, 0.34f, 0.18f, 1.0f));
			AddPart(22.0f, 43.0f, 18.0f, 37.0f, FLinearColor(0.48f, 0.44f, 0.32f, 1.0f));
			AddPart(150.0f, 43.0f, 18.0f, 37.0f, FLinearColor(0.78f, 0.70f, 0.52f, 1.0f));
			AddPart(168.0f, 52.0f, 9.0f, 18.0f, FLinearColor(0.92f, 0.86f, 0.62f, 1.0f));
			AddPart(74.0f, 43.0f, 52.0f, 37.0f, FLinearColor(0.94f, 0.78f, 0.16f, 1.0f));
			AddPart(48.0f, 53.0f, 11.0f, 11.0f, FLinearColor(0.38f, 0.95f, 0.78f, 1.0f));
			AddLabel(85.0f, 55.0f, FText::FromString(TEXT("BAT")), FLinearColor(0.02f, 0.03f, 0.025f, 1.0f), 10);
		}
		else if (Kind == FName(TEXT("Locker")))
		{
			AddPart(54.0f, 10.0f, 80.0f, 96.0f, FLinearColor(0.18f, 0.22f, 0.25f, 1.0f));
			AddPart(58.0f, 16.0f, 34.0f, 84.0f, FLinearColor(0.11f, 0.13f, 0.15f, 1.0f));
			AddPart(96.0f, 16.0f, 34.0f, 84.0f, FLinearColor(0.11f, 0.13f, 0.15f, 1.0f));
			AddPart(65.0f, 29.0f, 20.0f, 3.0f, FLinearColor(0.52f, 0.58f, 0.58f, 1.0f));
			AddPart(103.0f, 29.0f, 20.0f, 3.0f, FLinearColor(0.52f, 0.58f, 0.58f, 1.0f));
			AddPart(65.0f, 76.0f, 20.0f, 3.0f, FLinearColor(0.52f, 0.58f, 0.58f, 1.0f));
			AddPart(103.0f, 76.0f, 20.0f, 3.0f, FLinearColor(0.52f, 0.58f, 0.58f, 1.0f));
			AddPart(92.0f, 53.0f, 6.0f, 18.0f, AccentColor);
			AddPart(82.0f, 18.0f, 8.0f, 8.0f, FLinearColor(0.08f, 0.75f, 0.52f, 1.0f));
		}
		else if (Kind == FName(TEXT("Exit")))
		{
			AddPart(50.0f, 24.0f, 92.0f, 76.0f, FLinearColor(0.10f, 0.12f, 0.13f, 1.0f));
			AddPart(60.0f, 34.0f, 72.0f, 66.0f, FLinearColor(0.018f, 0.020f, 0.022f, 1.0f));
			AddPart(44.0f, 13.0f, 104.0f, 18.0f, AccentColor);
			AddLabel(80.0f, 15.0f, FText::FromString(TEXT("EXIT")), FLinearColor(0.02f, 0.035f, 0.025f, 1.0f), 10);
			AddPart(136.0f, 48.0f, 12.0f, 28.0f, FLinearColor(0.80f, 0.86f, 0.74f, 1.0f));
		}
		else if (Kind == FName(TEXT("CCTV")))
		{
			AddPart(28.0f, 30.0f, 78.0f, 46.0f, FLinearColor(0.10f, 0.12f, 0.13f, 1.0f));
			AddPart(34.0f, 36.0f, 66.0f, 34.0f, FLinearColor(0.015f, 0.022f, 0.026f, 1.0f));
			AddPart(44.0f, 43.0f, 46.0f, 3.0f, AccentColor);
			AddPart(44.0f, 51.0f, 38.0f, 3.0f, FLinearColor(0.48f, 0.90f, 0.86f, 1.0f));
			AddPart(44.0f, 59.0f, 24.0f, 3.0f, FLinearColor(0.48f, 0.90f, 0.86f, 0.62f));
			AddPart(122.0f, 26.0f, 36.0f, 20.0f, FLinearColor(0.18f, 0.20f, 0.22f, 1.0f));
			AddPart(158.0f, 32.0f, 18.0f, 9.0f, FLinearColor(0.08f, 0.09f, 0.10f, 1.0f));
			AddPart(133.0f, 46.0f, 8.0f, 34.0f, FLinearColor(0.14f, 0.15f, 0.16f, 1.0f));
			AddPart(112.0f, 80.0f, 44.0f, 6.0f, AccentColor);
			AddLabel(39.0f, 78.0f, FText::FromString(TEXT("MOTION")), AccentColor, 8);
		}
		else if (Kind == FName(TEXT("Train")))
		{
			AddPart(18.0f, 54.0f, 152.0f, 34.0f, FLinearColor(0.13f, 0.15f, 0.16f, 1.0f));
			AddPart(26.0f, 43.0f, 136.0f, 18.0f, FLinearColor(0.20f, 0.22f, 0.23f, 1.0f));
			AddPart(34.0f, 47.0f, 25.0f, 10.0f, AccentColor);
			AddPart(66.0f, 47.0f, 25.0f, 10.0f, AccentColor);
			AddPart(98.0f, 47.0f, 25.0f, 10.0f, AccentColor);
			AddPart(130.0f, 47.0f, 25.0f, 10.0f, AccentColor);
			AddPart(46.0f, 87.0f, 20.0f, 12.0f, FLinearColor(0.03f, 0.035f, 0.038f, 1.0f));
			AddPart(124.0f, 87.0f, 20.0f, 12.0f, FLinearColor(0.03f, 0.035f, 0.038f, 1.0f));
			AddPart(68.0f, 26.0f, 54.0f, 12.0f, FLinearColor(0.015f, 0.022f, 0.026f, 1.0f));
			AddLabel(77.0f, 27.0f, FText::FromString(TEXT("SHOP")), AccentColor, 8);
		}
		else if (Kind == FName(TEXT("Glass")))
		{
			AddPart(58.0f, 16.0f, 72.0f, 88.0f, FLinearColor(0.34f, 0.76f, 0.82f, 0.30f));
			AddPart(63.0f, 20.0f, 62.0f, 80.0f, FLinearColor(0.04f, 0.08f, 0.09f, 0.72f));
			AddPart(66.0f, 29.0f, 52.0f, 2.0f, AccentColor);
			AddPart(82.0f, 31.0f, 2.0f, 36.0f, AccentColor);
			AddPart(83.0f, 66.0f, 32.0f, 2.0f, AccentColor);
			AddPart(114.0f, 48.0f, 2.0f, 31.0f, AccentColor);
			AddPart(34.0f, 84.0f, 118.0f, 7.0f, FLinearColor(0.10f, 0.11f, 0.12f, 1.0f));
			AddLabel(72.0f, 93.0f, FText::FromString(TEXT("LOUD")), FLinearColor(0.92f, 0.28f, 0.20f, 1.0f), 9);
		}
		else
		{
			AddPart(80.0f, 60.0f, 36.0f, 30.0f, FLinearColor(0.16f, 0.16f, 0.18f, 1.0f));
			AddPart(88.0f, 46.0f, 20.0f, 16.0f, AccentColor);
			AddPart(72.0f, 37.0f, 52.0f, 3.0f, FLinearColor(0.90f, 0.18f, 0.12f, 1.0f));
			AddPart(60.0f, 28.0f, 74.0f, 2.0f, FLinearColor(0.90f, 0.18f, 0.12f, 0.72f));
			AddPart(49.0f, 18.0f, 96.0f, 1.0f, FLinearColor(0.90f, 0.18f, 0.12f, 0.48f));
			AddLabel(78.0f, 91.0f, FText::FromString(TEXT("G")), AccentColor, 10);
		}

		return SNew(SBox)
			.HeightOverride(112.0f)
			[
				Picture
			];
	}

	TSharedRef<SWidget> MenuGuideObjectCard(const FName Kind, const FText& Label, const FText& Detail, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.018f, 0.024f, 0.028f, 0.96f))
			.Padding(9.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MenuGuideObjectPicture(Kind, AccentColor)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(11, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.93f, 0.98f, 0.95f, 1.0f))
					.Text(Label)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(8))
					.ColorAndOpacity(FLinearColor(0.64f, 0.74f, 0.72f, 1.0f))
					.Text(Detail)
				]
			];
	}

	TSharedRef<SWidget> MenuGuideQuestionNodeRender(const FName Kind, const FText& Topic, const FText& Prompt, const FText& Footer, const FLinearColor& AccentColor)
	{
		TSharedRef<SOverlay> Render = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.010f, 0.014f, 0.017f, 1.0f))
			];

		auto AddPart = [&Render](float X, float Y, float W, float H, const FLinearColor& Color)
		{
			Render->AddSlot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(X, Y, 0.0f, 0.0f)
				[
					MenuGuidePicturePart(X, Y, W, H, Color)
				];
		};

		const FLinearColor ShadowColor(0.003f, 0.004f, 0.005f, 0.78f);
		const FLinearColor BodyColor(0.15f, 0.17f, 0.18f, 1.0f);
		const FLinearColor BodySideColor(0.08f, 0.09f, 0.10f, 1.0f);
		const FLinearColor ScreenColor(0.015f, 0.025f, 0.030f, 1.0f);

		AddPart(42.0f, 94.0f, 118.0f, 8.0f, ShadowColor);
		if (Kind == FName(TEXT("Valve")))
		{
			AddPart(46.0f, 50.0f, 84.0f, 42.0f, FLinearColor(0.23f, 0.14f, 0.10f, 1.0f));
			AddPart(130.0f, 58.0f, 18.0f, 34.0f, BodySideColor);
			AddPart(54.0f, 56.0f, 54.0f, 8.0f, FLinearColor(0.20f, 0.18f, 0.15f, 1.0f));
			AddPart(108.0f, 36.0f, 10.0f, 42.0f, FLinearColor(0.12f, 0.07f, 0.05f, 1.0f));
			AddPart(100.0f, 34.0f, 28.0f, 5.0f, AccentColor);
			AddPart(58.0f, 72.0f, 16.0f, 12.0f, AccentColor);
		}
		else if (Kind == FName(TEXT("Antenna")))
		{
			AddPart(92.0f, 24.0f, 8.0f, 70.0f, BodyColor);
			AddPart(70.0f, 78.0f, 52.0f, 18.0f, BodySideColor);
			AddPart(58.0f, 42.0f, 50.0f, 8.0f, AccentColor);
			AddPart(122.0f, 34.0f, 28.0f, 22.0f, FLinearColor(0.11f, 0.18f, 0.20f, 1.0f));
			AddPart(132.0f, 38.0f, 12.0f, 14.0f, AccentColor);
			AddPart(86.0f, 16.0f, 20.0f, 10.0f, AccentColor);
		}
		else if (Kind == FName(TEXT("Energy")))
		{
			AddPart(46.0f, 46.0f, 90.0f, 48.0f, FLinearColor(0.22f, 0.15f, 0.10f, 1.0f));
			AddPart(136.0f, 56.0f, 16.0f, 38.0f, BodySideColor);
			AddPart(58.0f, 56.0f, 52.0f, 24.0f, FLinearColor(0.20f, 0.22f, 0.20f, 1.0f));
			AddPart(62.0f, 61.0f, 44.0f, 4.0f, FLinearColor(0.82f, 0.78f, 0.64f, 1.0f));
			AddPart(114.0f, 54.0f, 15.0f, 28.0f, FLinearColor(0.16f, 0.82f, 0.42f, 1.0f));
			AddPart(136.0f, 38.0f, 9.0f, 9.0f, AccentColor);
		}
		else if (Kind == FName(TEXT("TrainBonus")))
		{
			AddPart(30.0f, 34.0f, 132.0f, 54.0f, BodyColor);
			AddPart(162.0f, 42.0f, 12.0f, 46.0f, BodySideColor);
			AddPart(42.0f, 42.0f, 108.0f, 32.0f, ScreenColor);
			AddPart(52.0f, 80.0f, 34.0f, 12.0f, AccentColor);
			AddPart(94.0f, 80.0f, 34.0f, 12.0f, AccentColor);
		}
		else
		{
			AddPart(46.0f, 34.0f, 88.0f, 60.0f, BodyColor);
			AddPart(134.0f, 45.0f, 16.0f, 49.0f, BodySideColor);
			AddPart(58.0f, 44.0f, 58.0f, 28.0f, ScreenColor);
			AddPart(62.0f, 50.0f, 46.0f, 4.0f, AccentColor);
			AddPart(62.0f, 60.0f, 34.0f, 4.0f, FLinearColor(0.64f, 0.78f, 0.76f, 1.0f));
			AddPart(120.0f, 42.0f, 10.0f, 10.0f, AccentColor);
			AddPart(62.0f, 78.0f, 10.0f, 9.0f, BodySideColor);
			AddPart(78.0f, 78.0f, 10.0f, 9.0f, BodySideColor);
			AddPart(94.0f, 78.0f, 10.0f, 9.0f, BodySideColor);
		}

		Render->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(0.0f, 10.0f, 9.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(116.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(FLinearColor(0.018f, 0.024f, 0.028f, 0.96f))
					.Padding(FMargin(7.0f, 5.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Font(MenuFont(8, FName(TEXT("Bold"))))
							.ColorAndOpacity(AccentColor)
							.Text(Topic)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Font(MenuFont(8))
							.ColorAndOpacity(FLinearColor(0.82f, 0.90f, 0.86f, 1.0f))
							.Text(Prompt)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 5.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Font(MenuFont(7, FName(TEXT("Bold"))))
							.ColorAndOpacity(FLinearColor(0.96f, 0.86f, 0.62f, 1.0f))
							.Text(Footer)
						]
					]
				]
			];

		return SNew(SBox)
			.HeightOverride(122.0f)
			[
				Render
			];
	}

	TSharedRef<SWidget> MenuGuideQuestionNodeCard(const FName Kind, const FText& Label, const FText& Topic, const FText& Prompt, const FText& Footer, const FText& Detail, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.018f, 0.024f, 0.028f, 0.96f))
			.Padding(9.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MenuGuideQuestionNodeRender(Kind, Topic, Prompt, Footer, AccentColor)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(11, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.93f, 0.98f, 0.95f, 1.0f))
					.Text(Label)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(8))
					.ColorAndOpacity(FLinearColor(0.64f, 0.74f, 0.72f, 1.0f))
					.Text(Detail)
				]
			];
	}

	TSharedRef<SWidget> MenuGuidePhotoBackdrop(const FName Kind, const FLinearColor& AccentColor)
	{
		const FLinearColor DangerAccent(0.92f, 0.22f, 0.14f, 1.0f);
		const FLinearColor ObjectiveAccent(0.94f, 0.60f, 0.18f, 1.0f);
		const FLinearColor SafeAccent(0.34f, 0.92f, 0.64f, 1.0f);
		const FLinearColor BlueAccent(0.42f, 0.74f, 0.92f, 1.0f);

		TSharedRef<SOverlay> Picture = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.006f, 0.008f, 0.010f, 1.0f))
			]
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(0.28f)
				[
					SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.020f, 0.025f, 0.028f, 1.0f))
				]
				+ SVerticalBox::Slot().FillHeight(0.42f)
				[
					SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.012f, 0.015f, 0.017f, 1.0f))
				]
				+ SVerticalBox::Slot().FillHeight(0.30f)
				[
					SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.045f, 0.042f, 0.035f, 1.0f))
				]
			];

		auto AddPart = [&Picture](float X, float Y, float W, float H, const FLinearColor& Color)
		{
			Picture->AddSlot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(X, Y, 0.0f, 0.0f)
				[
					MenuGuidePicturePart(X, Y, W, H, Color)
				];
		};

		auto AddText = [&Picture](float X, float Y, const FString& Label, const FLinearColor& Color, int32 Size = 8)
		{
			Picture->AddSlot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(X, Y, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(Size, FName(TEXT("Bold"))))
					.ColorAndOpacity(Color)
					.Text(FText::FromString(Label))
				];
		};

		auto AddToast = [&Picture](float X, float Y, float W, const FString& Label, const FLinearColor& Color)
		{
			Picture->AddSlot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(X, Y, 0.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(W)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.020f, 0.020f, 0.022f, 0.94f))
						.Padding(FMargin(7.0f, 5.0f))
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Font(MenuFont(8, FName(TEXT("Bold"))))
							.ColorAndOpacity(Color)
							.Text(FText::FromString(Label))
						]
					]
				];
		};

		if (Kind == FName(TEXT("Locker")))
		{
			AddPart(156.0f, 30.0f, 56.0f, 94.0f, FLinearColor(0.86f, 0.18f, 0.10f, 1.0f));
			AddPart(188.0f, 36.0f, 46.0f, 84.0f, FLinearColor(0.86f, 0.90f, 0.88f, 1.0f));
			AddPart(197.0f, 59.0f, 20.0f, 4.0f, FLinearColor(0.22f, 0.24f, 0.25f, 1.0f));
			AddPart(199.0f, 101.0f, 24.0f, 4.0f, FLinearColor(0.18f, 0.19f, 0.20f, 1.0f));
			AddPart(18.0f, 114.0f, 86.0f, 5.0f, FLinearColor(0.74f, 0.80f, 0.58f, 1.0f));
			AddPart(18.0f, 126.0f, 66.0f, 5.0f, DangerAccent);
			AddPart(18.0f, 138.0f, 72.0f, 5.0f, DangerAccent);
			AddToast(82.0f, 74.0f, 94.0f, TEXT("E / HIDE"), FLinearColor(0.96f, 0.88f, 0.64f, 1.0f));
			AddText(18.0f, 96.0f, TEXT("STAM / FEAR / DREAD"), DangerAccent, 7);
		}
		else if (Kind == FName(TEXT("Watched")))
		{
			AddPart(72.0f, 34.0f, 66.0f, 96.0f, FLinearColor(0.58f, 0.06f, 0.04f, 1.0f));
			AddPart(142.0f, 28.0f, 62.0f, 96.0f, FLinearColor(0.72f, 0.74f, 0.70f, 1.0f));
			AddPart(54.0f, 120.0f, 120.0f, 5.0f, FLinearColor(0.76f, 0.68f, 0.22f, 1.0f));
			AddPart(176.0f, 55.0f, 20.0f, 4.0f, FLinearColor(0.18f, 0.18f, 0.18f, 1.0f));
			AddToast(78.0f, 100.0f, 132.0f, TEXT("Something sees you."), FLinearColor(0.96f, 0.86f, 0.62f, 1.0f));
			AddText(16.0f, 12.0f, TEXT("LINE OF SIGHT WARNING"), DangerAccent, 8);
		}
		else if (Kind == FName(TEXT("Path")))
		{
			AddPart(0.0f, 0.0f, 260.0f, 150.0f, FLinearColor(0.42f, 0.02f, 0.00f, 0.74f));
			AddPart(18.0f, 56.0f, 152.0f, 46.0f, FLinearColor(0.95f, 0.24f, 0.08f, 0.72f));
			AddText(112.0f, 88.0f, TEXT("PATH DETECTED"), DangerAccent, 10);
			AddPart(172.0f, 20.0f, 72.0f, 72.0f, FLinearColor(0.04f, 0.05f, 0.045f, 1.0f));
			AddPart(204.0f, 52.0f, 8.0f, 8.0f, DangerAccent);
			AddPart(182.0f, 84.0f, 7.0f, 7.0f, ObjectiveAccent);
			AddPart(220.0f, 74.0f, 7.0f, 7.0f, BlueAccent);
			AddPart(212.0f, 58.0f, 46.0f, 2.0f, DangerAccent);
			AddPart(214.0f, 60.0f, 42.0f, 2.0f, DangerAccent);
			AddToast(72.0f, 118.0f, 130.0f, TEXT("The Teacher chose you."), FLinearColor(0.96f, 0.84f, 0.66f, 1.0f));
		}
		else if (Kind == FName(TEXT("Sound")))
		{
			AddPart(0.0f, 0.0f, 260.0f, 150.0f, FLinearColor(0.02f, 0.025f, 0.028f, 1.0f));
			AddPart(24.0f, 104.0f, 210.0f, 3.0f, FLinearColor(0.20f, 0.24f, 0.24f, 1.0f));
			AddPart(48.0f, 75.0f, 16.0f, 34.0f, FLinearColor(0.78f, 0.82f, 0.68f, 1.0f));
			AddPart(70.0f, 80.0f, 30.0f, 4.0f, FLinearColor(0.90f, 0.90f, 0.82f, 1.0f));
			AddPart(112.0f, 74.0f, 42.0f, 34.0f, ObjectiveAccent);
			AddPart(126.0f, 52.0f, 4.0f, 24.0f, FLinearColor(0.94f, 0.94f, 0.86f, 1.0f));
			AddPart(188.0f, 54.0f, 22.0f, 50.0f, FLinearColor(0.48f, 0.08f, 0.06f, 1.0f));
			AddPart(195.0f, 42.0f, 10.0f, 12.0f, FLinearColor(0.72f, 0.60f, 0.48f, 1.0f));
			AddPart(90.0f, 88.0f, 12.0f, 2.0f, DangerAccent);
			AddPart(80.0f, 80.0f, 28.0f, 2.0f, DangerAccent);
			AddPart(70.0f, 72.0f, 44.0f, 2.0f, DangerAccent);
			AddPart(154.0f, 82.0f, 12.0f, 2.0f, DangerAccent);
			AddPart(158.0f, 72.0f, 28.0f, 2.0f, DangerAccent);
			AddPart(162.0f, 62.0f, 44.0f, 2.0f, DangerAccent);
			AddText(30.0f, 18.0f, TEXT("NOISE TRAVELS"), DangerAccent, 10);
			AddText(38.0f, 58.0f, TEXT("run"), FLinearColor(0.96f, 0.86f, 0.62f, 1.0f), 8);
			AddText(112.0f, 40.0f, TEXT("repair"), FLinearColor(0.96f, 0.86f, 0.62f, 1.0f), 8);
			AddText(190.0f, 28.0f, TEXT("Teacher"), DangerAccent, 8);
			AddToast(42.0f, 118.0f, 180.0f, TEXT("Footsteps, repairs, panic, decoys"), FLinearColor(0.96f, 0.86f, 0.62f, 1.0f));
		}
		else if (Kind == FName(TEXT("Monitor")))
		{
			AddPart(112.0f, 38.0f, 36.0f, 78.0f, FLinearColor(0.18f, 0.20f, 0.16f, 1.0f));
			AddPart(118.0f, 26.0f, 24.0f, 20.0f, FLinearColor(0.66f, 0.56f, 0.45f, 1.0f));
			AddPart(104.0f, 66.0f, 50.0f, 4.0f, FLinearColor(0.74f, 0.68f, 0.24f, 1.0f));
			AddPart(54.0f, 98.0f, 46.0f, 20.0f, FLinearColor(0.90f, 0.26f, 0.14f, 1.0f));
			AddPart(67.0f, 80.0f, 4.0f, 24.0f, FLinearColor(0.92f, 0.92f, 0.82f, 1.0f));
			AddText(122.0f, 14.0f, TEXT("BOT 10"), FLinearColor(0.80f, 0.86f, 0.76f, 1.0f), 8);
			AddToast(18.0f, 118.0f, 224.0f, TEXT("Hall monitor: traps + hints, no capture."), FLinearColor(0.96f, 0.86f, 0.62f, 1.0f));
			AddText(54.0f, 66.0f, TEXT("G"), AccentColor, 11);
			AddText(168.0f, 54.0f, TEXT("Q / R"), AccentColor, 10);
		}
		else if (Kind == FName(TEXT("Work")))
		{
			AddPart(42.0f, 42.0f, 58.0f, 80.0f, ObjectiveAccent);
			AddPart(88.0f, 50.0f, 46.0f, 64.0f, FLinearColor(0.86f, 0.88f, 0.84f, 1.0f));
			AddPart(112.0f, 24.0f, 56.0f, 84.0f, FLinearColor(0.22f, 0.24f, 0.24f, 1.0f));
			AddPart(121.0f, 35.0f, 38.0f, 8.0f, FLinearColor(0.84f, 0.66f, 0.18f, 1.0f));
			AddPart(136.0f, 52.0f, 8.0f, 28.0f, FLinearColor(0.94f, 0.94f, 0.86f, 1.0f));
			AddPart(180.0f, 93.0f, 48.0f, 20.0f, FLinearColor(0.18f, 0.44f, 0.24f, 1.0f));
			AddPart(218.0f, 88.0f, 12.0f, 30.0f, FLinearColor(0.90f, 0.78f, 0.44f, 1.0f));
			AddToast(36.0f, 122.0f, 170.0f, TEXT("1-4 answer, then hold E"), FLinearColor(0.96f, 0.84f, 0.58f, 1.0f));
			AddText(38.0f, 20.0f, TEXT("STATION / BREAKER / BATTERY"), ObjectiveAccent, 8);
		}
		else if (Kind == FName(TEXT("Question")))
		{
			AddPart(100.0f, 30.0f, 5.0f, 102.0f, FLinearColor(0.94f, 0.20f, 0.18f, 1.0f));
			AddPart(78.0f, 70.0f, 60.0f, 4.0f, FLinearColor(0.92f, 0.92f, 0.86f, 1.0f));
			AddPart(92.0f, 94.0f, 34.0f, 4.0f, FLinearColor(0.96f, 0.76f, 0.22f, 1.0f));
			AddPart(98.0f, 20.0f, 12.0f, 12.0f, FLinearColor(0.96f, 0.90f, 0.60f, 1.0f));
			AddToast(24.0f, 92.0f, 212.0f, TEXT("WAVES CHECKPOINT: answer with 1-4"), FLinearColor(0.96f, 0.86f, 0.62f, 1.0f));
			AddText(132.0f, 42.0f, TEXT("Question terminals block progress"), AccentColor, 8);
		}
		else if (Kind == FName(TEXT("Teacher")))
		{
			AddPart(0.0f, 0.0f, 260.0f, 150.0f, FLinearColor(0.18f, 0.02f, 0.00f, 0.64f));
			AddPart(22.0f, 116.0f, 210.0f, 4.0f, FLinearColor(0.20f, 0.21f, 0.20f, 1.0f));
			AddPart(152.0f, 52.0f, 32.0f, 66.0f, DangerAccent);
			AddPart(159.0f, 36.0f, 18.0f, 18.0f, FLinearColor(0.82f, 0.62f, 0.48f, 1.0f));
			AddPart(158.0f, 78.0f, 52.0f, 3.0f, DangerAccent);
			AddPart(180.0f, 112.0f, 12.0f, 24.0f, DangerAccent);
			AddPart(144.0f, 112.0f, 12.0f, 24.0f, DangerAccent);
			AddPart(58.0f, 60.0f, 56.0f, 60.0f, FLinearColor(0.08f, 0.09f, 0.10f, 1.0f));
			AddText(155.0f, 28.0f, TEXT("HUNTER"), DangerAccent, 9);
			AddText(140.0f, 82.0f, TEXT("PATH DETECTED"), DangerAccent, 8);
			AddToast(30.0f, 124.0f, 184.0f, TEXT("Visible meter: break line of sight now."), FLinearColor(0.96f, 0.84f, 0.66f, 1.0f));
		}
		else if (Kind == FName(TEXT("Decoy")))
		{
			AddPart(0.0f, 0.0f, 260.0f, 150.0f, FLinearColor(0.012f, 0.018f, 0.020f, 1.0f));
			AddPart(36.0f, 112.0f, 190.0f, 4.0f, FLinearColor(0.20f, 0.23f, 0.23f, 1.0f));
			AddPart(102.0f, 74.0f, 48.0f, 34.0f, FLinearColor(0.88f, 0.20f, 0.14f, 1.0f));
			AddPart(122.0f, 45.0f, 6.0f, 34.0f, FLinearColor(0.96f, 0.96f, 0.86f, 1.0f));
			AddPart(90.0f, 70.0f, 10.0f, 10.0f, FLinearColor(0.70f, 0.94f, 0.96f, 1.0f));
			AddPart(72.0f, 64.0f, 4.0f, 4.0f, FLinearColor(0.70f, 0.94f, 0.96f, 0.78f));
			AddPart(154.0f, 62.0f, 12.0f, 2.0f, AccentColor);
			AddPart(162.0f, 55.0f, 24.0f, 2.0f, AccentColor);
			AddPart(170.0f, 48.0f, 36.0f, 2.0f, AccentColor);
			AddText(92.0f, 36.0f, TEXT("NOISE DECOY"), AccentColor, 9);
			AddToast(56.0f, 121.0f, 154.0f, TEXT("Drop it after turning away."), FLinearColor(0.96f, 0.86f, 0.62f, 1.0f));
		}
		else if (Kind == FName(TEXT("Avatar")))
		{
			AddPart(0.0f, 0.0f, 260.0f, 150.0f, FLinearColor(0.17f, 0.18f, 0.21f, 1.0f));
			AddPart(0.0f, 0.0f, 96.0f, 150.0f, FLinearColor(0.20f, 0.32f, 0.36f, 1.0f));
			AddPart(42.0f, 36.0f, 18.0f, 18.0f, FLinearColor(0.86f, 0.78f, 0.64f, 1.0f));
			AddPart(36.0f, 56.0f, 30.0f, 54.0f, FLinearColor(0.54f, 0.70f, 0.76f, 1.0f));
			AddPart(30.0f, 64.0f, 7.0f, 42.0f, FLinearColor(0.92f, 0.88f, 0.76f, 1.0f));
			AddPart(66.0f, 64.0f, 7.0f, 42.0f, FLinearColor(0.92f, 0.88f, 0.76f, 1.0f));
			AddPart(40.0f, 108.0f, 8.0f, 30.0f, FLinearColor(0.54f, 0.70f, 0.76f, 1.0f));
			AddPart(54.0f, 108.0f, 8.0f, 30.0f, FLinearColor(0.54f, 0.70f, 0.76f, 1.0f));
			AddPart(116.0f, 18.0f, 60.0f, 22.0f, FLinearColor(0.04f, 0.05f, 0.05f, 1.0f));
			AddPart(184.0f, 18.0f, 58.0f, 22.0f, FLinearColor(0.04f, 0.05f, 0.05f, 1.0f));
			AddPart(116.0f, 62.0f, 34.0f, 22.0f, FLinearColor(0.52f, 0.78f, 0.86f, 1.0f));
			AddPart(156.0f, 62.0f, 34.0f, 22.0f, FLinearColor(0.82f, 0.58f, 0.28f, 1.0f));
			AddPart(196.0f, 62.0f, 34.0f, 22.0f, FLinearColor(0.54f, 0.70f, 0.36f, 1.0f));
			AddText(116.0f, 48.0f, TEXT("SHIRT"), FLinearColor(0.90f, 0.94f, 0.92f, 1.0f), 10);
			AddText(116.0f, 96.0f, TEXT("HEADWEAR"), FLinearColor(0.90f, 0.94f, 0.92f, 1.0f), 10);
			AddToast(104.0f, 118.0f, 124.0f, TEXT("Looks only; roles still matter."), FLinearColor(0.96f, 0.86f, 0.62f, 1.0f));
		}
		else
		{
			AddPart(40.0f, 38.0f, 180.0f, 86.0f, FLinearColor(0.14f, 0.16f, 0.16f, 1.0f));
			AddPart(52.0f, 28.0f, 156.0f, 16.0f, SafeAccent);
			AddPart(64.0f, 46.0f, 132.0f, 70.0f, FLinearColor(0.018f, 0.024f, 0.024f, 1.0f));
			AddPart(28.0f, 36.0f, 20.0f, 84.0f, FLinearColor(0.84f, 0.22f, 0.14f, 1.0f));
			AddPart(212.0f, 36.0f, 20.0f, 84.0f, FLinearColor(0.84f, 0.22f, 0.14f, 1.0f));
			AddText(98.0f, 31.0f, TEXT("EXIT"), FLinearColor(0.02f, 0.04f, 0.03f, 1.0f), 10);
			AddToast(70.0f, 112.0f, 128.0f, TEXT("EXIT OPEN: hold E"), FLinearColor(0.84f, 1.0f, 0.72f, 1.0f));
		}

		return SNew(SBox)
			.WidthOverride(260.0f)
			.HeightOverride(150.0f)
			[
				Picture
			];
	}

	TSharedRef<SWidget> MenuGuidePhotoSlide(const FName Kind, const FText& Label, const FText& Body, const FText& Tag, const FLinearColor& AccentColor)
	{
		return SNew(SBox)
			.WidthOverride(300.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.014f, 0.020f, 0.024f, 0.98f))
				.Padding(10.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MenuGuidePhotoBackdrop(Kind, AccentColor)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 9.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Font(MenuFont(12, FName(TEXT("Bold"))))
							.ColorAndOpacity(FLinearColor(0.93f, 0.98f, 0.95f, 1.0f))
							.Text(Label)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.25f))
							.Padding(FMargin(7.0f, 3.0f))
							[
								SNew(STextBlock)
								.Font(MenuFont(8, FName(TEXT("Bold"))))
								.ColorAndOpacity(AccentColor)
								.Text(Tag)
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Font(MenuFont(9))
						.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.76f, 1.0f))
						.Text(Body)
					]
				]
			];
	}

	TSharedRef<SWidget> MenuGuidePhotoSlideDeck()
	{
		TSharedRef<SScrollBox> Slides = SNew(SScrollBox)
			.Orientation(Orient_Horizontal);

		auto AddSlide = [&Slides](const TSharedRef<SWidget>& Slide)
		{
			Slides->AddSlot()
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					Slide
				];
		};

		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Locker")),
			FText::FromString(TEXT("Locker Pressure")),
			FText::FromString(TEXT("Lockers break sight, and for a beat after you leave one the Teacher cannot grab you. Leave when the meter clears; camping builds dread.")),
			FText::FromString(TEXT("HIDE")),
			FLinearColor(0.58f, 0.72f, 0.96f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Watched")),
			FText::FromString(TEXT("Something Sees You")),
			FText::FromString(TEXT("Sight and CCTV warnings mean your route is exposed. Turn a corner, go prone, or open the security circuit.")),
			FText::FromString(TEXT("SIGHT")),
			FLinearColor(0.94f, 0.34f, 0.22f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Path")),
			FText::FromString(TEXT("Teacher Chose You")),
			FText::FromString(TEXT("PATH DETECTED and red map cones mean active pressure. Cut across rooms; do not run straight down the marked route.")),
			FText::FromString(TEXT("MAP")),
			FLinearColor(0.94f, 0.24f, 0.16f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Teacher")),
			FText::FromString(TEXT("Teacher In Sight")),
			FText::FromString(TEXT("When the visible meter fills, stop staring and create cover. Roll, slide, door-slam, or flashlight-stagger the swing window.")),
			FText::FromString(TEXT("CHASE")),
			FLinearColor(0.94f, 0.24f, 0.16f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Sound")),
			FText::FromString(TEXT("Sound Gives You Away")),
			FText::FromString(TEXT("The Teacher can hear repairs, running, panic breathing, and decoys. Crouch or pause when danger is close.")),
			FText::FromString(TEXT("LISTEN")),
			FLinearColor(0.92f, 0.32f, 0.18f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Decoy")),
			FText::FromString(TEXT("Decoy Sound")),
			FText::FromString(TEXT("A decoy works best after a turn or door. Make the noise point down the route you are not taking.")),
			FText::FromString(TEXT("G")),
			FLinearColor(0.84f, 0.30f, 0.80f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Monitor")),
			FText::FromString(TEXT("Hall Monitor Return")),
			FText::FromString(TEXT("Caught players can return as Hall Monitors: traps, real hints, and aimed false markers, but no captures. Classroom answers unlock tools.")),
			FText::FromString(TEXT("TOOLS")),
			FLinearColor(0.92f, 0.58f, 0.18f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Work")),
			FText::FromString(TEXT("Tasks And Pickups")),
			FText::FromString(TEXT("Orange stations and breakers progress the round. Batteries refill flashlight, stamina, and nerve.")),
			FText::FromString(TEXT("WORK")),
			FLinearColor(0.90f, 0.66f, 0.22f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Question")),
			FText::FromString(TEXT("Checkpoint Questions")),
			FText::FromString(TEXT("Question panels pause progress. Read the prompt, press 1-4, then resume work or earn train shop points.")),
			FText::FromString(TEXT("1-4")),
			FLinearColor(0.86f, 0.70f, 0.30f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Work")),
			FText::FromString(TEXT("Train Stop And Shop")),
			FText::FromString(TEXT("Between stages, read recap boards, answer the bonus terminal, buy role upgrades, then board before departure.")),
			FText::FromString(TEXT("TRAIN")),
			FLinearColor(0.44f, 0.80f, 0.92f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Exit")),
			FText::FromString(TEXT("Final Exit")),
			FText::FromString(TEXT("When EXIT OPEN appears, stop collecting extras. Reach the gate or subway door and hold E.")),
			FText::FromString(TEXT("ESCAPE")),
			FLinearColor(0.38f, 0.94f, 0.62f, 1.0f)));
		AddSlide(MenuGuidePhotoSlide(
			FName(TEXT("Avatar")),
			FText::FromString(TEXT("Avatar Setup")),
			FText::FromString(TEXT("Cosmetics help players recognize themselves in the lobby and screenshots. Gameplay power still comes from role and shop tools.")),
			FText::FromString(TEXT("LOOK")),
			FLinearColor(0.52f, 0.78f, 0.86f, 1.0f)));

		return SNew(SBox)
			.HeightOverride(306.0f)
			[
				Slides
			];
	}

	TSharedRef<SWidget> MenuGuideHighlight(const FText& Label, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.24f))
			.Padding(FMargin(8.0f, 5.0f))
			[
				SNew(STextBlock)
				.Font(MenuFont(9, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.98f, 0.94f, 0.74f, 1.0f))
				.Text(Label)
			];
	}

	TSharedRef<SWidget> MenuGuideHudMock()
	{
		const FLinearColor SurvivorAccent(0.22f, 0.72f, 0.66f, 1.0f);
		const FLinearColor ObjectiveAccent(0.92f, 0.66f, 0.24f, 1.0f);
		const FLinearColor DangerAccent(0.90f, 0.22f, 0.14f, 1.0f);
		const FLinearColor MapAccent(0.56f, 0.70f, 0.94f, 1.0f);

		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.009f, 0.012f, 0.016f, 0.98f))
			.Padding(10.0f)
			[
				SNew(SBox)
				.HeightOverride(350.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.018f, 0.022f, 0.026f, 1.0f))
					]
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(0.22f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.032f, 0.038f, 0.043f, 1.0f))
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.25f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.024f, 0.030f, 0.034f, 1.0f))
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.53f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.014f, 0.018f, 0.020f, 1.0f))
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					.Padding(14.0f, 14.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(252.0f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.005f, 0.008f, 0.010f, 0.76f))
							.Padding(9.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Font(MenuFont(12, FName(TEXT("Bold"))))
									.ColorAndOpacity(SurvivorAccent)
									.Text(FText::FromString(TEXT("08:42 / EXIT SHUT")))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 4.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.AutoWrapText(true)
									.Font(MenuFont(9))
									.ColorAndOpacity(FLinearColor(0.86f, 0.82f, 0.66f, 1.0f))
									.Text(FText::FromString(TEXT("RESTORE POWER AND REACH THE EXIT")))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 5.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Font(MenuFont(8, FName(TEXT("Bold"))))
									.ColorAndOpacity(FLinearColor(0.65f, 0.60f, 0.52f, 1.0f))
									.Text(FText::FromString(TEXT("POWER 1/2  TASKS 2/4  PRESENCE 38")))
								]
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Top)
					.Padding(0.0f, 14.0f, 14.0f, 0.0f)
					[
						MenuGuideHighlight(FText::FromString(TEXT("ROLE: SURVIVOR")), ObjectiveAccent)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(28.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(286.0f)
						.HeightOverride(214.0f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.040f, 0.034f, 0.024f, 0.96f))
							.Padding(12.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Font(MenuFont(12, FName(TEXT("Bold"))))
									.ColorAndOpacity(ObjectiveAccent)
									.Text(FText::FromString(TEXT("FORCES / EASY  1/3")))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 7.0f, 0.0f, 6.0f)
								[
									SNew(STextBlock)
									.AutoWrapText(true)
									.Font(MenuFont(10))
									.ColorAndOpacity(FLinearColor(0.88f, 0.86f, 0.74f, 1.0f))
									.Text(FText::FromString(TEXT("Which force slows a sliding box?")))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									MenuGuideHighlight(FText::FromString(TEXT("1 friction     2 gravity")), ObjectiveAccent)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 5.0f, 0.0f, 0.0f)
								[
									MenuGuideHighlight(FText::FromString(TEXT("3 magnetism    4 weight")), ObjectiveAccent)
								]
								+ SVerticalBox::Slot()
								.FillHeight(1.0f)
								[
									SNew(SSpacer)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.AutoWrapText(true)
									.Font(MenuFont(8))
									.ColorAndOpacity(FLinearColor(0.72f, 0.70f, 0.60f, 1.0f))
									.Text(FText::FromString(TEXT("Answer first. When solved, hold E to finish the station.")))
								]
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(190.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Font(MenuFont(30, FName(TEXT("Bold"))))
							.ColorAndOpacity(SurvivorAccent)
							.Text(FText::FromString(TEXT("+")))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.06f, 0.08f, 0.08f, 0.82f))
							.Padding(FMargin(11.0f, 6.0f))
							[
								SNew(STextBlock)
								.Font(MenuFont(10, FName(TEXT("Bold"))))
								.ColorAndOpacity(FLinearColor(0.96f, 0.88f, 0.60f, 1.0f))
								.Text(FText::FromString(TEXT("E  HOLD INTERACT")))
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(0.0f, 24.0f, 24.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(184.0f)
						.HeightOverride(196.0f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.010f, 0.014f, 0.014f, 0.88f))
							.Padding(10.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Font(MenuFont(11, FName(TEXT("Bold"))))
									.ColorAndOpacity(MapAccent)
									.Text(FText::FromString(TEXT("M / I  HEAT MAP")))
								]
								+ SVerticalBox::Slot()
								.FillHeight(1.0f)
								.Padding(0.0f, 8.0f)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SBorder)
										.BorderImage(WhiteBrush())
										.BorderBackgroundColor(FLinearColor(0.018f, 0.022f, 0.020f, 1.0f))
									]
									+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
									[
										MenuGuideHighlight(FText::FromString(TEXT("YOU")), SurvivorAccent)
									]
									+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(18.0f, 18.0f, 0.0f, 0.0f)
									[
										MenuGuideHighlight(FText::FromString(TEXT("STN")), ObjectiveAccent)
									]
									+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 14.0f, 18.0f)
									[
										MenuGuideHighlight(FText::FromString(TEXT("EXIT")), FLinearColor(0.40f, 1.0f, 0.62f, 1.0f))
									]
									+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.0f, 26.0f, 22.0f, 0.0f)
									[
										MenuGuideHighlight(FText::FromString(TEXT("!!!")), DangerAccent)
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Font(MenuFont(8))
									.ColorAndOpacity(FLinearColor(0.66f, 0.74f, 0.76f, 1.0f))
									.Text(FText::FromString(TEXT("Orange station. Green exit. Red threat.")))
								]
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Bottom)
					.Padding(16.0f, 0.0f, 0.0f, 14.0f)
					[
						SNew(SBox)
						.WidthOverride(142.0f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.004f, 0.006f, 0.008f, 0.78f))
							.Padding(8.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									MenuGuideHudMeter(FText::FromString(TEXT("BATTERY")), FText::FromString(TEXT("74")), 0.74f, FLinearColor(0.80f, 0.82f, 0.70f, 1.0f))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MenuGuideHudMeter(FText::FromString(TEXT("TEACHER")), FText::FromString(TEXT("NEAR 28m")), 0.48f, DangerAccent)
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MenuGuideHudMeter(FText::FromString(TEXT("STAM")), FText::FromString(TEXT("62")), 0.62f, FLinearColor(0.70f, 0.84f, 0.48f, 1.0f))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MenuGuideHudMeter(FText::FromString(TEXT("FEAR")), FText::FromString(TEXT("31")), 0.31f, DangerAccent)
								]
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(24.0f, 22.0f, 0.0f, 0.0f)
					[
						MenuGuideAnnotation(
							FText::FromString(TEXT("<< TIMER + EXIT")),
							FText::FromString(TEXT("EXIT SHUT means finish power, tasks, and stations.")),
							SurvivorAccent)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(0.0f, 112.0f, 230.0f, 0.0f)
					[
						MenuGuideAnnotation(
							FText::FromString(TEXT("QUESTION PANEL >>")),
							FText::FromString(TEXT("Press 1-4 while looking at the station.")),
							ObjectiveAccent)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Bottom)
					.Padding(178.0f, 0.0f, 0.0f, 38.0f)
					[
						MenuGuideAnnotation(
							FText::FromString(TEXT("<< INTERACT")),
							FText::FromString(TEXT("Hold E for work. Tap E to leave lockers.")),
							FLinearColor(0.96f, 0.82f, 0.44f, 1.0f))
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(0.0f, 0.0f, 226.0f, 16.0f)
					[
						MenuGuideAnnotation(
							FText::FromString(TEXT("METERS >>")),
							FText::FromString(TEXT("High Teacher, Fear, or Dread means break sight and slow down.")),
							DangerAccent)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Top)
					.Padding(0.0f, 76.0f, 220.0f, 0.0f)
					[
						MenuGuideAnnotation(
							FText::FromString(TEXT("MAP >>")),
							FText::FromString(TEXT("M or I. Find stations, exits, and danger pulses.")),
							MapAccent)
					]
				]
			];
	}

	TSharedRef<SWidget> MenuGuideHudScreenshot(const FSlateBrush* ScreenshotBrush)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.008f, 0.012f, 0.014f, 0.98f))
			.Padding(8.0f)
			[
				SNew(SBox)
				.HeightOverride(470.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					.StretchDirection(EStretchDirection::Both)
					[
						SNew(SImage)
						.Image(ScreenshotBrush)
						.ColorAndOpacity(FLinearColor::White)
					]
				]
			];
	}

	FLinearColor MenuAvatarPaletteColor(int32 Index)
	{
		static const FLinearColor Palette[] = {
			FLinearColor(0.22f, 0.58f, 0.74f, 1.0f),
			FLinearColor(0.78f, 0.46f, 0.18f, 1.0f),
			FLinearColor(0.46f, 0.72f, 0.28f, 1.0f),
			FLinearColor(0.70f, 0.30f, 0.42f, 1.0f),
			FLinearColor(0.52f, 0.44f, 0.86f, 1.0f),
			FLinearColor(0.84f, 0.75f, 0.24f, 1.0f),
			FLinearColor(0.28f, 0.68f, 0.62f, 1.0f),
			FLinearColor(0.76f, 0.76f, 0.80f, 1.0f)
		};

		return Palette[FMath::Abs(Index) % UE_ARRAY_COUNT(Palette)];
	}

	FLinearColor MenuAvatarColorLerp(const FLinearColor& A, const FLinearColor& B, float Alpha)
	{
		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return FLinearColor(
			FMath::Lerp(A.R, B.R, ClampedAlpha),
			FMath::Lerp(A.G, B.G, ClampedAlpha),
			FMath::Lerp(A.B, B.B, ClampedAlpha),
			1.0f);
	}

	FLinearColor MenuAvatarColorScale(const FLinearColor& Color, float Scale)
	{
		return FLinearColor(
			FMath::Clamp(Color.R * Scale, 0.0f, 1.0f),
			FMath::Clamp(Color.G * Scale, 0.0f, 1.0f),
			FMath::Clamp(Color.B * Scale, 0.0f, 1.0f),
			1.0f);
	}

	FVector MenuAvatarPreviewTransformValue(const FVector& Value)
	{
		return Value * MenuAvatarPreviewModelScale;
	}

	UStaticMesh* MenuPreviewCubeMesh()
	{
		static UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		return Mesh;
	}

	UStaticMesh* MenuPreviewCylinderMesh()
	{
		static UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		return Mesh;
	}

	UStaticMesh* MenuPreviewSphereMesh()
	{
		static UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		return Mesh;
	}

	UMaterialInterface* MenuPreviewBasicMaterial()
	{
		static UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		return Material;
	}

	UMaterialInterface* MenuQuaterniusMaterial(const TCHAR* AssetName)
	{
		const FString Name(AssetName);
		const FString Path = FString::Printf(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/%s.%s"), *Name, *Name);
		return LoadObject<UMaterialInterface>(nullptr, *Path);
	}

	UMaterialInterface* MenuPreviewMaterial(const TCHAR* AssetName)
	{
		if (UMaterialInterface* Material = MenuQuaterniusMaterial(AssetName))
		{
			return Material;
		}

		return MenuPreviewBasicMaterial();
	}

	void MenuApplyPreviewPiece(
		UStaticMeshComponent* Part,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Scale,
		bool bVisible)
	{
		if (!Part)
		{
			return;
		}

		if (Mesh)
		{
			Part->SetStaticMesh(Mesh);
		}
		if (Material)
		{
			Part->SetMaterial(0, Material);
		}

		Part->SetRelativeLocation(MenuAvatarPreviewTransformValue(Location));
		Part->SetRelativeRotation(Rotation);
		Part->SetRelativeScale3D(MenuAvatarPreviewTransformValue(Scale));
		Part->SetVisibility(bVisible, true);
		Part->SetHiddenInGame(!bVisible);
	}

	FLinearColor MenuAvatarSkinColor(int32 AvatarIndex)
	{
		static const FLinearColor SkinTones[] = {
			FLinearColor(0.82f, 0.58f, 0.42f, 1.0f),
			FLinearColor(0.92f, 0.72f, 0.56f, 1.0f),
			FLinearColor(0.58f, 0.38f, 0.28f, 1.0f),
			FLinearColor(0.74f, 0.49f, 0.34f, 1.0f),
			FLinearColor(0.96f, 0.80f, 0.64f, 1.0f),
			FLinearColor(0.47f, 0.30f, 0.22f, 1.0f),
			FLinearColor(0.88f, 0.65f, 0.48f, 1.0f),
			FLinearColor(0.67f, 0.44f, 0.32f, 1.0f)
		};

		return SkinTones[FMath::Abs(AvatarIndex) % UE_ARRAY_COUNT(SkinTones)];
	}

	FLinearColor MenuAvatarHairColor(int32 AvatarIndex)
	{
		static const FLinearColor HairTones[] = {
			FLinearColor(0.06f, 0.045f, 0.035f, 1.0f),
			FLinearColor(0.16f, 0.10f, 0.055f, 1.0f),
			FLinearColor(0.32f, 0.22f, 0.12f, 1.0f),
			FLinearColor(0.08f, 0.075f, 0.07f, 1.0f),
			FLinearColor(0.42f, 0.35f, 0.24f, 1.0f),
			FLinearColor(0.18f, 0.12f, 0.09f, 1.0f),
			FLinearColor(0.55f, 0.46f, 0.34f, 1.0f),
			FLinearColor(0.10f, 0.09f, 0.12f, 1.0f)
		};

		return HairTones[FMath::Abs(AvatarIndex) % UE_ARRAY_COUNT(HairTones)];
	}

	int32 MenuNearestAvatarColorIndex(const FLinearColor& Color)
	{
		int32 BestIndex = 0;
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FLinearColor Candidate = MenuAvatarPaletteColor(Index);
			const float Distance =
				FMath::Square(Color.R - Candidate.R)
				+ FMath::Square(Color.G - Candidate.G)
				+ FMath::Square(Color.B - Candidate.B);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestIndex = Index;
			}
		}
		return BestIndex;
	}

	UMaterialInterface* MenuQuaterniusPaletteMaterial(const FLinearColor& Color)
	{
		static const TCHAR* MaterialPaths[] = {
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/LightBlue.LightBlue"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Brown2.Brown2"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Green.Green"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Red.Red"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Purple.Purple"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/Gold.Gold"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/LightGreen.LightGreen"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/White.White")
		};

		return LoadObject<UMaterialInterface>(nullptr, MaterialPaths[MenuNearestAvatarColorIndex(Color) % UE_ARRAY_COUNT(MaterialPaths)]);
	}

	bool MenuShouldPreserveQuaterniusMaterial(UMaterialInterface* Material)
	{
		const FString Name = Material ? Material->GetName() : FString();
		return Name.Contains(TEXT("Skin"))
			|| Name.Contains(TEXT("Hair"))
			|| Name.Contains(TEXT("Eye"))
			|| Name.Contains(TEXT("Eyebrow"))
			|| Name.Contains(TEXT("Moustache"));
	}

	void MenuApplyQuaterniusPalette(USkeletalMeshComponent* MeshComponent, const FLinearColor& Color)
	{
		if (!MeshComponent)
		{
			return;
		}

		UMaterialInterface* PaletteMaterial = MenuQuaterniusPaletteMaterial(Color);
		if (!PaletteMaterial)
		{
			return;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			if (!MenuShouldPreserveQuaterniusMaterial(MeshComponent->GetMaterial(MaterialIndex)))
			{
				MeshComponent->SetMaterial(MaterialIndex, PaletteMaterial);
			}
		}
	}

	const TCHAR* MenuAvatarLookName(int32 AvatarIndex)
	{
		return BHCosmeticItemName(EBHCosmeticCategory::Outfit, AvatarIndex);
	}

	const TCHAR* MenuAvatarHeadwearName(int32 HeadwearIndex)
	{
		return BHCosmeticItemName(EBHCosmeticCategory::Headwear, HeadwearIndex);
	}

	const TCHAR* MenuAvatarMeshPath(const ABHPlayerState* BHPS)
	{
		static const TCHAR* SurvivorMeshes[] = {
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Casual_2.SK_BH_Q_Casual_2"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Worker.SK_BH_Q_Worker"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Adventurer.SK_BH_Q_Adventurer"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Farmer.SK_BH_Q_Farmer"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Beach.SK_BH_Q_Beach"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Punk.SK_BH_Q_Punk"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Suit.SK_BH_Q_Suit"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Spacesuit.SK_BH_Q_Spacesuit")
		};
		static const TCHAR* HunterMeshes[] = {
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Swat.SK_BH_Q_Swat"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Suit.SK_BH_Q_Suit"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Spacesuit.SK_BH_Q_Spacesuit"),
			TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_King.SK_BH_Q_King")
		};

		const int32 AvatarIndex = BHPS ? FMath::Abs(BHPS->AvatarIndex) : 0;
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			return HunterMeshes[AvatarIndex % UE_ARRAY_COUNT(HunterMeshes)];
		}
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Suit.SK_BH_Q_Suit");
		}
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
		{
			return TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Meshes/SK_BH_Q_Adventurer.SK_BH_Q_Adventurer");
		}

		return SurvivorMeshes[AvatarIndex % UE_ARRAY_COUNT(SurvivorMeshes)];
	}

	const TCHAR* MenuAvatarModelName(const ABHPlayerState* BHPS)
	{
		static const TCHAR* SurvivorModelNames[] = {
			TEXT("Quaternius Casual"),
			TEXT("Quaternius Worker"),
			TEXT("Quaternius Adventurer"),
			TEXT("Quaternius Farmer"),
			TEXT("Quaternius Beach"),
			TEXT("Quaternius Punk"),
			TEXT("Quaternius Suit"),
			TEXT("Quaternius Spacesuit")
		};
		static const TCHAR* HunterNames[] = {
			TEXT("Quaternius Swat"),
			TEXT("Quaternius Suit"),
			TEXT("Quaternius Spacesuit"),
			TEXT("Quaternius King")
		};

		const int32 AvatarIndex = BHPS ? FMath::Abs(BHPS->AvatarIndex) : 0;
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			return HunterNames[AvatarIndex % UE_ARRAY_COUNT(HunterNames)];
		}
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			return TEXT("Quaternius Suit");
		}
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
		{
			return TEXT("Quaternius Adventurer");
		}
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
		{
			return TEXT("Spectator Preview");
		}

		return SurvivorModelNames[AvatarIndex % UE_ARRAY_COUNT(SurvivorModelNames)];
	}

}

SBHMainMenu::~SBHMainMenu()
{
	if (GMenuButtonSoundController == PlayerController)
	{
		GMenuButtonSoundController.Reset();
	}

	DestroyAvatarPreviewScene();
}

void SBHMainMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	UpdateAvatarPreviewMesh();
}

void SBHMainMenu::Construct(const FArguments& InArgs)
{
	PlayerController = InArgs._PlayerController;
	GMenuButtonSoundController = PlayerController;
	SuggestedAddress = ResolvePreferredAddress();
	StatusText = FText::FromString(TEXT("Choose a map, join a host, or browse online lobbies."));

	const bool bInGame = IsInNetworkedGame();
	const bool bPracticeMode = IsPracticeMode();
	const bool bTestMode = IsTestMode();
	ActiveMenuTab = EBHMainMenuTab::Play;
	bShowingStartScreen = !bInGame;
	bShowStartCredentials = false;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FBHLessonPreset SelectedPreset;
		FString PresetMessage;
		if (PC->GetSelectedLessonPresetForMenu(SelectedPreset, PresetMessage))
		{
			SelectedLiveClassroomMap = FBHLessonPresetStore::NormalizeMapName(SelectedPreset.MapName);
		}

		const UWorld* World = PC->GetWorld();
		const bool bLiveClassroomHost = MenuUrlOptionEnabled(World, TEXT("BHLiveClassroom=")) && World && World->GetNetMode() == NM_ListenServer;
		const bool bRemovedByHost = MenuUrlOptionEnabled(World, TEXT("BHRemovedByHost="));
		if (bLiveClassroomHost)
		{
			ActiveMenuTab = EBHMainMenuTab::Classroom;
			bShowingStartScreen = false;
			StatusText = FText::FromString(TEXT("Live Classroom is hosting. Share the JOIN address, assign roles, and kick blockers if needed."));
		}
		else if (bRemovedByHost)
		{
			bShowingStartScreen = false;
			StatusText = FText::FromString(TEXT("You were removed from the classroom lobby by the host. You can rejoin if invited."));
		}
	}

	// Feedback form defaults. If a round just ended, open straight to the survey.
	PendingFeedbackKind = EBHFeedbackKind::Bug;
	SurveyDifficulty = TEXT("just_right");
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		bIncludeDiagnostics = PC->ShouldIncludeFeedbackDiagnosticsByDefaultForMenu();
		if (PC->IsEndOfRoundSurveyPendingForMenu())
		{
			ActiveMenuTab = EBHMainMenuTab::Feedback;
			bShowingStartScreen = false;
		}
	}

	StartBackgroundFallbackBrush = FSlateBrush();
	StartBackgroundFallbackBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	StartBackgroundBrush = FSlateBrush();
	StartBackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	StartBackgroundBrush.ImageType = ESlateBrushImageType::FullColor;
	StartBackgroundBrush.Tiling = ESlateBrushTileType::NoTile;
	StartBackgroundBrush.Mirroring = ESlateBrushMirrorType::NoMirror;
	StartBackgroundBrush.ImageSize = FVector2D(1672.0f, 941.0f);

	const FString BackgroundPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BlackoutHunt/Art/UI/T_BH_StartScreen_Background.png"));
	if (FPaths::FileExists(BackgroundPath))
	{
		if (UTexture2D* BackgroundTexture = FImageUtils::ImportFileAsTexture2D(BackgroundPath))
		{
			StartBackgroundTexture.Reset(BackgroundTexture);
			StartBackgroundBrush.SetResourceObject(BackgroundTexture);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to import start screen background from %s"), *BackgroundPath);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Start screen background file is missing: %s"), *BackgroundPath);
	}

	GuideHudFallbackBrush = *WhiteBrush();
	GuideHudFallbackBrush.DrawAs = ESlateBrushDrawType::Box;
	GuideHudFallbackBrush.TintColor = FSlateColor(FLinearColor(0.012f, 0.018f, 0.020f, 1.0f));
	GuideHudFallbackBrush.ImageSize = FVector2D(1600.0f, 520.0f);
	GuideHudBrush = FSlateBrush();
	GuideHudBrush.DrawAs = ESlateBrushDrawType::Image;
	GuideHudBrush.ImageType = ESlateBrushImageType::FullColor;
	GuideHudBrush.Tiling = ESlateBrushTileType::NoTile;
	GuideHudBrush.Mirroring = ESlateBrushMirrorType::NoMirror;
	GuideHudBrush.ImageSize = FVector2D(1600.0f, 520.0f);

	const FString GuideHudPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BlackoutHunt/Art/UI/T_BH_Guide_HUD_Annotated.png"));
	if (FPaths::FileExists(GuideHudPath))
	{
		if (UTexture2D* ImportedGuideHudTexture = FImageUtils::ImportFileAsTexture2D(GuideHudPath))
		{
			GuideHudTexture.Reset(ImportedGuideHudTexture);
			GuideHudBrush.SetResourceObject(ImportedGuideHudTexture);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to import guide HUD screenshot from %s"), *GuideHudPath);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Guide HUD screenshot file is missing: %s"), *GuideHudPath);
	}

	TSharedRef<SVerticalBox> ActionList = SNew(SVerticalBox);
	BuildPlayActionList(ActionList, bInGame, bPracticeMode, bTestMode);

	ChildSlot
	[
		SNew(SWidgetSwitcher)
		.WidgetIndex(TAttribute<int32>::Create(TAttribute<int32>::FGetter::CreateSP(this, &SBHMainMenu::GetRootWidgetIndex)))
		+ SWidgetSwitcher::Slot()
		[
			BuildStartScreen()
		]
		+ SWidgetSwitcher::Slot()
		[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.010f, 0.012f, 0.016f, 0.98f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.HeightOverride(5.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.22f, 0.82f, 0.74f, 0.75f))
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			SNew(SBox)
			.HeightOverride(2.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.95f, 0.46f, 0.24f, 0.42f))
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(42.0f, 30.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(40, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.80f, 1.0f, 0.94f, 1.0f))
				.ShadowOffset(FVector2D(2.0f, 2.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
				.Text(FText::FromString(TEXT("BLACKOUT HUNT")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(3.0f, 2.0f, 0.0f, 14.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(13))
				.ColorAndOpacity(FLinearColor(0.72f, 0.80f, 0.78f, 1.0f))
				.Text(FText::FromString(TEXT("Survive the blackout. Hunt the signal.")))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Play, bInGame ? FText::FromString(TEXT("Actions")) : FText::FromString(TEXT("Play")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Guide, FText::FromString(TEXT("Guide")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Classroom, FText::FromString(TEXT("Classroom")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Character, FText::FromString(TEXT("Character")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Achievements, FText::FromString(TEXT("Awards")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Match, FText::FromString(TEXT("Match")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Network, FText::FromString(TEXT("Network")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Account, FText::FromString(TEXT("Account")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Controls, FText::FromString(TEXT("Controls")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildMenuTabButton(EBHMainMenuTab::Settings, FText::FromString(TEXT("Settings")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						BuildMenuTabButton(EBHMainMenuTab::Feedback, FText::FromString(TEXT("Feedback")))
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(MainTabSwitcher, SWidgetSwitcher)
					.WidgetIndex(MenuTabToWidgetIndex(ActiveMenuTab))
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.020f, 0.024f, 0.030f, 0.96f))
						.Padding(16.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Font(MenuFont(18, FName(TEXT("Bold"))))
								.ColorAndOpacity(FLinearColor(0.96f, 0.92f, 0.82f, 1.0f))
								.Text(bInGame ? FText::FromString(TEXT("Current Game")) : FText::FromString(TEXT("Play")))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 4.0f, 0.0f, 12.0f)
							[
								SNew(STextBlock)
								.AutoWrapText(true)
								.Font(MenuFont(12))
								.ColorAndOpacity(FLinearColor(0.58f, 0.66f, 0.66f, 1.0f))
								.Text(bInGame
									? FText::FromString(TEXT("Session actions grouped by section."))
									: FText::FromString(TEXT("Game modes, hosting, online, and join controls grouped by section.")))
							]
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									ActionList
								]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 12.0f, 0.0f, 0.0f)
							[
								BuildStatusPanel()
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									BuildGuidePanel()
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									BuildStatusPanel()
								]
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								BuildClassroomPanel()
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									BuildControlsPanel()
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									BuildStatusPanel()
								]
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									BuildCharacterCustomizationPanel()
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 12.0f, 0.0f, 0.0f)
								[
									SNew(SBorder)
									.Visibility(bInGame ? EVisibility::Visible : EVisibility::Collapsed)
									.BorderImage(WhiteBrush())
									.BorderBackgroundColor(FLinearColor(0.036f, 0.043f, 0.050f, 0.95f))
									.Padding(12.0f)
									[
										SNew(STextBlock)
										.AutoWrapText(true)
										.Font(MenuFont(12))
										.ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.82f, 1.0f))
										.Text(this, &SBHMainMenu::GetPlayerIdentityText)
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 12.0f, 0.0f, 0.0f)
								[
									BuildStatusPanel()
								]
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Font(MenuFont(18, FName(TEXT("Bold"))))
									.ColorAndOpacity(FLinearColor(0.90f, 0.95f, 0.93f, 1.0f))
									.Text(bInGame ? FText::FromString(TEXT("Round")) : FText::FromString(TEXT("Match Setup")))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 8.0f)
								[
									SNew(SSeparator)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									SNew(STextBlock)
									.AutoWrapText(true)
									.Font(MenuFont(12))
									.ColorAndOpacity(FLinearColor(0.73f, 0.80f, 0.80f, 1.0f))
									.Text(bInGame
										? FText::FromString(TEXT("Review round state, practice tools, and host-only role assignment."))
										: FText::FromString(TEXT("Round options become active after hosting or joining.")))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									SNew(SBox)
									.Visibility((bPracticeMode || bTestMode) ? EVisibility::Visible : EVisibility::Collapsed)
									[
										BuildPracticePanel()
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									SNew(SBorder)
									.Visibility(bInGame ? EVisibility::Visible : EVisibility::Collapsed)
									.BorderImage(WhiteBrush())
									.BorderBackgroundColor(FLinearColor(0.030f, 0.038f, 0.044f, 0.95f))
									.Padding(12.0f)
									[
										SNew(STextBlock)
										.AutoWrapText(true)
										.Font(MenuFont(12))
										.ColorAndOpacity(FLinearColor(0.74f, 0.82f, 0.78f, 1.0f))
										.Text(this, &SBHMainMenu::GetRoundDirectorText)
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									BuildRoundOptionsPanel()
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									SNew(SBox)
									.Visibility((bInGame && !bPracticeMode && !bTestMode) ? EVisibility::Visible : EVisibility::Collapsed)
									[
										BuildRoleAssignmentPanel()
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									BuildStatusPanel()
								]
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								BuildNetworkPanel()
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									BuildAccountPanel()
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									BuildStatusPanel()
								]
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 12.0f)
								[
									BuildGraphicsPanel()
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									BuildStatusPanel()
								]
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								BuildFeedbackPanel()
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(FLinearColor(0.026f, 0.032f, 0.038f, 0.94f))
						.Padding(16.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(AchievementsPanelContainer, SBox)
								[
									BuildAchievementsPanel()
								]
							]
						]
					]
				]
			]
		]
		]
	];
}

FReply SBHMainMenu::OnStartClicked()
{
	bShowingStartScreen = false;
	bShowStartCredentials = false;
	StatusText = FText::FromString(TEXT("Choose a map, join a host, or browse online lobbies."));
	return OnMenuTabClicked(EBHMainMenuTab::Play);
}

FReply SBHMainMenu::OnStartAccountClicked()
{
	bShowingStartScreen = false;
	bShowStartCredentials = false;
	StatusText = FText::FromString(TEXT("Sign in locally, use Guest, or connect a browser account."));
	return OnMenuTabClicked(EBHMainMenuTab::Account);
}

FReply SBHMainMenu::OnStartGuideClicked()
{
	bShowingStartScreen = false;
	bShowStartCredentials = false;
	StatusText = FText::FromString(TEXT("Read the quick guide, then start a test or bot round from Play."));
	return OnMenuTabClicked(EBHMainMenuTab::Guide);
}

FReply SBHMainMenu::OnToggleStartCredentialsClicked()
{
	bShowStartCredentials = !bShowStartCredentials;
	return FReply::Handled();
}

bool SBHMainMenu::SupportsKeyboardFocus() const
{
	return true;
}

FReply SBHMainMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Easter egg: the Konami code. Tracked on every key without consuming it (so normal menu nav still
	// works); only the final key is consumed, to reveal a hidden line. Cosmetic; gated by bh.EasterEggs.
	if (BHAreEasterEggsEnabled())
	{
		static const FKey KonamiSequence[] = {
			EKeys::Up, EKeys::Up, EKeys::Down, EKeys::Down,
			EKeys::Left, EKeys::Right, EKeys::Left, EKeys::Right,
			EKeys::B, EKeys::A
		};
		const FKey Pressed = InKeyEvent.GetKey();
		if (Pressed == KonamiSequence[KonamiProgress])
		{
			++KonamiProgress;
			if (KonamiProgress >= static_cast<int32>(UE_ARRAY_COUNT(KonamiSequence)))
			{
				KonamiProgress = 0;
				StatusText = FText::FromString(TEXT("Up Up Down Down Left Right Left Right B A  --  the faculty sees you. no cheats here, just a quiet nod. well played."));
				if (ABHPlayerController* KonamiPC = PlayerController.Get())
				{
					if (UBHAccountSubsystem* Account = KonamiPC->GetGameInstance() ? KonamiPC->GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
					{
						Account->UnlockAchievement(FName(TEXT("codebreaker"))); // unlocks the Arcade tint
					}
				}
				return FReply::Handled();
			}
		}
		else
		{
			KonamiProgress = (Pressed == KonamiSequence[0]) ? 1 : 0;
		}
	}

	if (bShowingStartScreen)
	{
		if (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::SpaceBar)
		{
			return OnStartClicked();
		}

		if (InKeyEvent.GetKey() == EKeys::Escape && bShowStartCredentials)
		{
			bShowStartCredentials = false;
			return FReply::Handled();
		}
	}

	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (IsInNetworkedGame())
		{
			return OnResumeClicked();
		}

		StatusText = FText::FromString(TEXT("Host or join a direct-IP game to start."));
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::B && CanOpenClassroomBoard())
	{
		return OnOpenClassroomBoardClicked();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SBHMainMenu::PlayMenuSelectionSound() const
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->PlayMenuSelectionSound();
	}
}

void SBHMainMenu::RefreshLessonPresetList()
{
	if (!LessonPresetListBox.IsValid())
	{
		return;
	}

	LessonPresetListBox->ClearChildren();

	ABHPlayerController* PC = PlayerController.Get();
	TArray<FBHLessonPreset> Presets;
	FString Message;
	if (!PC || !PC->GetLessonPresetsForMenu(Presets, Message))
	{
		LessonPresetListBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(9))
				.ColorAndOpacity(FLinearColor(0.86f, 0.62f, 0.40f, 1.0f))
				.Text(FText::FromString(Message.IsEmpty() ? TEXT("Only built-in lesson presets are available.") : Message))
			];
	}

	for (const FBHLessonPreset& RawPreset : Presets)
	{
		const FBHLessonPreset Preset = FBHLessonPresetStore::ValidatePreset(RawPreset);
		const FString PresetId = Preset.Id;
		const FString Label = FString::Printf(TEXT("Apply: %s"), *Preset.DisplayName);
		const FString SourceLabel = Preset.bBuiltin ? TEXT("Built-in") : TEXT("Custom");
		const FString Detail = FString::Printf(TEXT("%s | %s | %s | Targets %.0f/%.0f | %d min | Scare %d | %s | Bots %d %s"),
			*SourceLabel,
			*FBHLessonPresetStore::TopicMaskToText(Preset.TopicMask),
			*FBHLessonPresetStore::DifficultyMixToString(Preset.DifficultyMix),
			Preset.ClassThreshold,
			Preset.IndividualThreshold,
			FMath::Max(1, FMath::RoundToInt(static_cast<float>(Preset.RoundSeconds) / 60.0f)),
			Preset.ScareIntensity,
			*Preset.MapName,
			Preset.BotCount,
			*FBHLessonPresetStore::BotDifficultyToString(Preset.BotDifficulty));

		LessonPresetListBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 5.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 7.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanManageLessonPresets)))
					.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetLessonPresetButtonColor, PresetId)))
					.ContentPadding(FMargin(8.0f, 5.0f))
					.OnClicked(this, &SBHMainMenu::OnLessonPresetClicked, PresetId)
					[
						SNew(STextBlock)
						.Font(MenuFont(9, FName(TEXT("Bold"))))
						.Text(FText::FromString(Label))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(9))
					.ColorAndOpacity(FLinearColor(0.58f, 0.68f, 0.68f, 1.0f))
					.Text(FText::FromString(Detail))
				]
			];
	}
}

FReply SBHMainMenu::OnMenuTabClicked(EBHMainMenuTab NewTab)
{
	ActiveMenuTab = NewTab;
	if (MainTabSwitcher.IsValid())
	{
		MainTabSwitcher->SetActiveWidgetIndex(MenuTabToWidgetIndex(NewTab));
	}

	// Opening the Awards tab: rebuild it so the "NEW" markers + profile/mastery reflect the latest progress,
	// then mark all current unlocks seen so the markers and the tab count clear next time.
	if (NewTab == EBHMainMenuTab::Achievements)
	{
		if (AchievementsPanelContainer.IsValid())
		{
			AchievementsPanelContainer->SetContent(BuildAchievementsPanel());
		}
		if (ABHPlayerController* PC = PlayerController.Get())
		{
			if (UGameInstance* GameInstance = PC->GetGameInstance())
			{
				if (UBHAccountSubsystem* AccountSubsystem = GameInstance->GetSubsystem<UBHAccountSubsystem>())
				{
					AccountSubsystem->MarkAchievementsSeen();
				}
			}
		}
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnOpenClassroomBoardClicked()
{
	FString Message;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		if (PC->OpenClassroomBoardForMenu(Message))
		{
			StatusText = FText::FromString(Message);
			return FReply::Handled();
		}
	}

	if (Message.IsEmpty())
	{
		Message = TEXT("Only the host machine can open the classroom board.");
	}
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FReply SBHMainMenu::OnOpenClassroomSupportFolderClicked()
{
	FString Message;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->OpenClassroomSupportFolderForMenu(Message);
	}

	if (Message.IsEmpty())
	{
		Message = TEXT("Only the host machine can open the classroom support folder.");
	}
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FReply SBHMainMenu::OnOpenClassroomLogFolderClicked()
{
	FString Message;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->OpenClassroomLogFolderForMenu(Message);
	}

	if (Message.IsEmpty())
	{
		Message = TEXT("Only the host machine can open classroom logs.");
	}
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FReply SBHMainMenu::OnOpenClassroomPackageFolderClicked()
{
	FString Message;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->OpenClassroomPackageFolderForMenu(Message);
	}

	if (Message.IsEmpty())
	{
		Message = TEXT("Only the host machine can open the classroom package folder.");
	}
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FReply SBHMainMenu::OnOpenClassroomDeploymentGuideClicked()
{
	FString Message;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->OpenClassroomDeploymentGuideForMenu(Message);
	}

	if (Message.IsEmpty())
	{
		Message = TEXT("Only the host machine can open the classroom deployment guide.");
	}
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FReply SBHMainMenu::OnCreateClassroomSupportBundleClicked()
{
	FString Message;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->CreateClassroomSupportBundleForMenu(Message);
	}

	if (Message.IsEmpty())
	{
		Message = TEXT("Only the host machine can create a classroom support bundle.");
	}
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FReply SBHMainMenu::OnCopyClassroomJoinCodeClicked()
{
	FString Message;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->CopyClassroomJoinCodeForMenu(Message);
	}

	if (Message.IsEmpty())
	{
		Message = TEXT("Only the host machine can copy the classroom join code.");
	}
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FReply SBHMainMenu::OnRefreshClassroomPreflightClicked()
{
	FString Message;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->RefreshClassroomPreflightForMenu(Message);
	}

	if (Message.IsEmpty())
	{
		Message = TEXT("Only the host machine can refresh classroom preflight.");
	}
	StatusText = FText::FromString(Message);
	return FReply::Handled();
}

FSlateColor SBHMainMenu::GetMenuTabColor(EBHMainMenuTab Tab) const
{
	if (ActiveMenuTab == Tab)
	{
		return FSlateColor(FLinearColor(0.10f, 0.34f, 0.31f, 1.0f));
	}

	return FSlateColor(FLinearColor(0.045f, 0.052f, 0.060f, 0.96f));
}

FText SBHMainMenu::GetMenuTabLabel(EBHMainMenuTab Tab, FText BaseLabel) const
{
	if (Tab == EBHMainMenuTab::Achievements)
	{
		const ABHPlayerController* PC = PlayerController.Get();
		const UGameInstance* GameInstance = PC ? PC->GetGameInstance() : nullptr;
		const UBHAccountSubsystem* AccountSubsystem = GameInstance ? GameInstance->GetSubsystem<UBHAccountSubsystem>() : nullptr;
		const int32 Unseen = AccountSubsystem ? AccountSubsystem->GetUnseenAchievementCount() : 0;
		if (Unseen > 0)
		{
			return FText::FromString(FString::Printf(TEXT("%s  (%d)"), *BaseLabel.ToString(), Unseen));
		}
	}
	return BaseLabel;
}

FSlateColor SBHMainMenu::GetMenuTabTextColor(EBHMainMenuTab Tab) const
{
	return ActiveMenuTab == Tab
		? FSlateColor(FLinearColor(0.90f, 1.0f, 0.96f, 1.0f))
		: FSlateColor(FLinearColor(0.62f, 0.70f, 0.70f, 1.0f));
}

int32 SBHMainMenu::MenuTabToWidgetIndex(EBHMainMenuTab Tab)
{
	switch (Tab)
	{
	case EBHMainMenuTab::Play:
		return 0;
	case EBHMainMenuTab::Guide:
		return 1;
	case EBHMainMenuTab::Classroom:
		return 2;
	case EBHMainMenuTab::Controls:
		return 3;
	case EBHMainMenuTab::Character:
		return 4;
	case EBHMainMenuTab::Match:
		return 5;
	case EBHMainMenuTab::Network:
		return 6;
	case EBHMainMenuTab::Account:
		return 7;
	case EBHMainMenuTab::Settings:
		return 8;
	case EBHMainMenuTab::Feedback:
		return 9;
	case EBHMainMenuTab::Achievements:
		return 10;
	default:
		return 8;
	}
}

namespace
{
	const FLinearColor BHFeedbackSelectedColor(0.10f, 0.34f, 0.31f, 1.0f);
	const FLinearColor BHFeedbackRatingColor(0.42f, 0.34f, 0.10f, 1.0f);
	const FLinearColor BHFeedbackUnselectedColor(0.045f, 0.052f, 0.060f, 0.96f);
}

FSlateColor SBHMainMenu::GetFeedbackKindButtonColor(EBHFeedbackKind Kind) const
{
	return FSlateColor(PendingFeedbackKind == Kind ? BHFeedbackSelectedColor : BHFeedbackUnselectedColor);
}

FSlateColor SBHMainMenu::GetFeedbackRatingButtonColor(int32 Rating) const
{
	return FSlateColor((PendingFeedbackRating > 0 && Rating <= PendingFeedbackRating) ? BHFeedbackRatingColor : BHFeedbackUnselectedColor);
}

FSlateColor SBHMainMenu::GetSurveyOverallButtonColor(int32 Overall) const
{
	return FSlateColor((SurveyOverallRating > 0 && Overall <= SurveyOverallRating) ? BHFeedbackRatingColor : BHFeedbackUnselectedColor);
}

FSlateColor SBHMainMenu::GetSurveyDifficultyButtonColor(FString Difficulty) const
{
	return FSlateColor(SurveyDifficulty.Equals(Difficulty) ? BHFeedbackSelectedColor : BHFeedbackUnselectedColor);
}

FSlateColor SBHMainMenu::GetSurveyRecommendButtonColor(bool bRecommend) const
{
	return FSlateColor(bSurveyWouldRecommend == bRecommend ? BHFeedbackSelectedColor : BHFeedbackUnselectedColor);
}

FText SBHMainMenu::GetFeedbackStatusText() const
{
	return StatusText;
}

ECheckBoxState SBHMainMenu::GetIncludeDiagnosticsState() const
{
	return bIncludeDiagnostics ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SBHMainMenu::OnIncludeDiagnosticsChanged(ECheckBoxState NewState)
{
	bIncludeDiagnostics = (NewState == ECheckBoxState::Checked);
}

FReply SBHMainMenu::OnFeedbackKindClicked(EBHFeedbackKind Kind)
{
	PendingFeedbackKind = Kind;
	PlayMenuSelectionSound();
	return FReply::Handled();
}

FReply SBHMainMenu::OnFeedbackRatingClicked(int32 Rating)
{
	// Click the active rating again to clear it.
	PendingFeedbackRating = (PendingFeedbackRating == Rating) ? 0 : Rating;
	PlayMenuSelectionSound();
	return FReply::Handled();
}

FReply SBHMainMenu::OnSubmitFeedbackClicked()
{
	const FString Message = FeedbackMessageTextBox.IsValid() ? FeedbackMessageTextBox->GetText().ToString() : FString();
	const FString Contact = FeedbackContactTextBox.IsValid() ? FeedbackContactTextBox->GetText().ToString() : FString();

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString OutMessage;
		const bool bSubmitted = PC->SubmitFeedbackForMenu(PendingFeedbackKind, Message, PendingFeedbackRating, Contact, bIncludeDiagnostics, OutMessage);
		StatusText = FText::FromString(OutMessage);
		if (bSubmitted)
		{
			if (FeedbackMessageTextBox.IsValid())
			{
				FeedbackMessageTextBox->SetText(FText::GetEmpty());
			}
			PendingFeedbackRating = 0;
		}
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	PlayMenuSelectionSound();
	return FReply::Handled();
}

FReply SBHMainMenu::OnSurveyOverallClicked(int32 Overall)
{
	SurveyOverallRating = Overall;
	PlayMenuSelectionSound();
	return FReply::Handled();
}

FReply SBHMainMenu::OnSurveyDifficultyClicked(FString Difficulty)
{
	SurveyDifficulty = Difficulty;
	PlayMenuSelectionSound();
	return FReply::Handled();
}

FReply SBHMainMenu::OnSurveyRecommendClicked(bool bRecommend)
{
	bSurveyWouldRecommend = bRecommend;
	PlayMenuSelectionSound();
	return FReply::Handled();
}

FReply SBHMainMenu::OnSubmitSurveyClicked()
{
	const FString Comment = SurveyCommentTextBox.IsValid() ? SurveyCommentTextBox->GetText().ToString() : FString();

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString OutMessage;
		PC->SubmitRoundSurveyForMenu(SurveyOverallRating, SurveyDifficulty, bSurveyWouldRecommend, Comment, OutMessage);
		StatusText = FText::FromString(OutMessage);
		if (SurveyCommentTextBox.IsValid())
		{
			SurveyCommentTextBox->SetText(FText::GetEmpty());
		}
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	PlayMenuSelectionSound();
	return FReply::Handled();
}

FReply SBHMainMenu::OnDismissSurveyClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->ClearEndOfRoundSurveyPendingForMenu();
	}
	StatusText = FText::FromString(TEXT("Survey dismissed. You can still send feedback any time from this tab."));
	PlayMenuSelectionSound();
	return FReply::Handled();
}

TSharedRef<SWidget> SBHMainMenu::BuildEndOfRoundSurveyPanel()
{
	auto OverallButton = [this](int32 Overall) -> TSharedRef<SWidget>
	{
		return SNew(SBHMenuButton)
			.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetSurveyOverallButtonColor, Overall)))
			.ContentPadding(FMargin(10.0f, 6.0f))
			.OnClicked(this, &SBHMainMenu::OnSurveyOverallClicked, Overall)
			[
				SNew(STextBlock)
				.Font(MenuFont(12, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.96f, 0.92f, 0.70f, 1.0f))
				.Text(FText::AsNumber(Overall))
			];
	};

	auto DifficultyButton = [this](const FString& Value, const FText& Label) -> TSharedRef<SWidget>
	{
		return SNew(SBHMenuButton)
			.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetSurveyDifficultyButtonColor, Value)))
			.ContentPadding(FMargin(10.0f, 6.0f))
			.OnClicked(this, &SBHMainMenu::OnSurveyDifficultyClicked, Value)
			[
				SNew(STextBlock)
				.Font(MenuFont(11, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.92f, 0.95f, 0.95f, 1.0f))
				.Text(Label)
			];
	};

	auto RecommendButton = [this](bool bRecommend, const FText& Label) -> TSharedRef<SWidget>
	{
		return SNew(SBHMenuButton)
			.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetSurveyRecommendButtonColor, bRecommend)))
			.ContentPadding(FMargin(10.0f, 6.0f))
			.OnClicked(this, &SBHMainMenu::OnSurveyRecommendClicked, bRecommend)
			[
				SNew(STextBlock)
				.Font(MenuFont(11, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.92f, 0.95f, 0.95f, 1.0f))
				.Text(Label)
			];
	};

	TSharedRef<SHorizontalBox> OverallRow = SNew(SHorizontalBox);
	for (int32 Score = 1; Score <= 5; ++Score)
	{
		OverallRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ OverallButton(Score) ];
	}

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.05f, 0.10f, 0.14f, 0.96f))
		.Padding(14.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(16, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.80f, 0.92f, 1.0f, 1.0f))
				.Text(FText::FromString(TEXT("Quick survey - how was that round?")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(11))
				.ColorAndOpacity(FLinearColor(0.62f, 0.74f, 0.80f, 1.0f))
				.Text(FText::FromString(TEXT("Takes about 20 seconds. It helps tune difficulty and fix problems.")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
				.Text(FText::FromString(TEXT("OVERALL (1-5)")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				OverallRow
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
				.Text(FText::FromString(TEXT("DIFFICULTY")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)[ DifficultyButton(TEXT("too_easy"), FText::FromString(TEXT("Too easy"))) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)[ DifficultyButton(TEXT("just_right"), FText::FromString(TEXT("Just right"))) ]
				+ SHorizontalBox::Slot().AutoWidth()[ DifficultyButton(TEXT("too_hard"), FText::FromString(TEXT("Too hard"))) ]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
				.Text(FText::FromString(TEXT("WOULD YOU PLAY AGAIN?")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)[ RecommendButton(true, FText::FromString(TEXT("Yes"))) ]
				+ SHorizontalBox::Slot().AutoWidth()[ RecommendButton(false, FText::FromString(TEXT("Not really"))) ]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBox)
				.HeightOverride(70.0f)
				[
					SAssignNew(SurveyCommentTextBox, SMultiLineEditableTextBox)
					.HintText(FText::FromString(TEXT("Anything confusing, buggy, or fun? (optional)")))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.40f, 0.32f, 1.0f)))
					.ContentPadding(FMargin(16.0f, 8.0f))
					.OnClicked(this, &SBHMainMenu::OnSubmitSurveyClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(12, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.95f, 1.0f, 0.97f, 1.0f))
						.Text(FText::FromString(TEXT("SEND SURVEY")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBHMenuButton)
					.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.10f, 0.11f, 0.13f, 0.96f)))
					.ContentPadding(FMargin(16.0f, 8.0f))
					.OnClicked(this, &SBHMainMenu::OnDismissSurveyClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(12, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.70f, 0.74f, 0.76f, 1.0f))
						.Text(FText::FromString(TEXT("NO THANKS")))
					]
				]
			]
		];
}

namespace
{
	// Difficulty -> tier colour (Bronze..Mythic) used for the badge spine + difficulty pips.
	FLinearColor BHAchievementTierColor(int32 Difficulty)
	{
		switch (FMath::Clamp(Difficulty, 1, 5))
		{
		case 1:  return FLinearColor(0.62f, 0.44f, 0.26f, 1.0f); // Bronze
		case 2:  return FLinearColor(0.70f, 0.72f, 0.78f, 1.0f); // Silver
		case 3:  return FLinearColor(0.90f, 0.72f, 0.28f, 1.0f); // Gold
		case 4:  return FLinearColor(0.40f, 0.84f, 0.92f, 1.0f); // Platinum
		default: return FLinearColor(0.74f, 0.52f, 0.98f, 1.0f); // Mythic (5 stars)
		}
	}
	FString BHAchievementTierName(int32 Difficulty)
	{
		switch (FMath::Clamp(Difficulty, 1, 5))
		{
		case 1:  return TEXT("BRONZE");
		case 2:  return TEXT("SILVER");
		case 3:  return TEXT("GOLD");
		case 4:  return TEXT("PLATINUM");
		default: return TEXT("MYTHIC");
		}
	}

	// XP -> school-themed rank. Fills the current/next rank XP thresholds (for a progress-to-next bar).
	FString BHRankForXP(int32 XP, int32& OutCurrentThreshold, int32& OutNextThreshold)
	{
		struct FRankTier { int32 Threshold; const TCHAR* Name; };
		static const FRankTier Ranks[] = {
			{ 0, TEXT("Freshman") },
			{ 150, TEXT("Sophomore") },
			{ 400, TEXT("Junior") },
			{ 800, TEXT("Senior") },
			{ 1500, TEXT("Honor Roll") },
			{ 2800, TEXT("Dean's List") },
			{ 5000, TEXT("Valedictorian") }
		};
		const int32 Num = UE_ARRAY_COUNT(Ranks);
		int32 Idx = 0;
		for (int32 i = 0; i < Num; ++i)
		{
			if (XP >= Ranks[i].Threshold)
			{
				Idx = i;
			}
		}
		OutCurrentThreshold = Ranks[Idx].Threshold;
		OutNextThreshold = (Idx + 1 < Num) ? Ranks[Idx + 1].Threshold : Ranks[Idx].Threshold;
		return Ranks[Idx].Name;
	}
}

TSharedRef<SWidget> SBHMainMenu::BuildAchievementsPanel()
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UGameInstance* GameInstance = PC ? PC->GetGameInstance() : nullptr;
	const UBHAccountSubsystem* AccountSubsystem = GameInstance ? GameInstance->GetSubsystem<UBHAccountSubsystem>() : nullptr;

	TArray<FBHAchievementDisplay> Achievements;
	int32 Earned = 0;
	int32 Total = 0;
	if (AccountSubsystem)
	{
		Achievements = AccountSubsystem->GetAchievementsForDisplay();
		AccountSubsystem->GetAchievementCounts(Earned, Total);
	}

	// Profile header: rank from XP + lifetime stats.
	int32 XP = 0, Rounds = 0, HunterWins = 0, SurvivorWins = 0, Escapes = 0, BestStreak = 0;
	if (AccountSubsystem)
	{
		const FBHAccountProgress& Prog = AccountSubsystem->GetProgress();
		XP = Prog.XP;
		Rounds = Prog.RoundsPlayed;
		HunterWins = Prog.HunterWins;
		SurvivorWins = Prog.SurvivorWins;
		Escapes = Prog.Escapes;
		BestStreak = Prog.BestWinStreak;
	}
	int32 RankCurrent = 0;
	int32 RankNext = 0;
	const FString RankName = BHRankForXP(XP, RankCurrent, RankNext);
	const float RankPct = (RankNext > RankCurrent) ? FMath::Clamp(static_cast<float>(XP - RankCurrent) / static_cast<float>(RankNext - RankCurrent), 0.0f, 1.0f) : 1.0f;
	const FString RankXpStr = (RankNext > RankCurrent) ? FString::Printf(TEXT("%d / %d XP"), XP, RankNext) : FString::Printf(TEXT("%d XP  (max rank)"), XP);
	const FString StatsStr = FString::Printf(TEXT("%d rounds  -  %d wins (%dT / %dS)  -  %d escapes  -  best streak %d  -  %d/%d awards"), Rounds, HunterWins + SurvivorWins, HunterWins, SurvivorWins, Escapes, BestStreak, Earned, Total);

	// Easiest-first so the list reads as a difficulty ramp; within a tier, earned badges float up.
	Achievements.Sort([](const FBHAchievementDisplay& A, const FBHAchievementDisplay& B)
	{
		if (A.Difficulty != B.Difficulty) { return A.Difficulty < B.Difficulty; }
		if (A.bUnlocked != B.bUnlocked) { return A.bUnlocked && !B.bUnlocked; }
		return A.Title < B.Title;
	});

	TSharedRef<SVerticalBox> BadgeList = SNew(SVerticalBox);
	for (const FBHAchievementDisplay& Ach : Achievements)
	{
		const bool bSecret = Ach.bHidden && !Ach.bUnlocked;
		const FLinearColor TierColor = BHAchievementTierColor(Ach.Difficulty);
		const FLinearColor TitleColor = Ach.bUnlocked ? FLinearColor(0.94f, 0.92f, 0.80f, 1.0f) : FLinearColor(0.56f, 0.59f, 0.63f, 1.0f);

		// Difficulty meter: five pips, the first N filled with the tier colour.
		TSharedRef<SHorizontalBox> Meter = SNew(SHorizontalBox);
		for (int32 Pip = 0; Pip < 5; ++Pip)
		{
			const bool bFilled = Pip < Ach.Difficulty;
			Meter->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(10.0f).HeightOverride(10.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(bFilled ? TierColor : FLinearColor(0.16f, 0.17f, 0.19f, 1.0f))
				]
			];
		}

		const FString TitleStr = bSecret ? FString(TEXT("??? (hidden)")) : Ach.Title;
		const FString DescStr = bSecret ? FString(TEXT("A secret achievement. Keep playing to discover it.")) : Ach.Description;
		const FString RewardStr = Ach.RewardLabel.IsEmpty() ? FString() : (FString(TEXT("Reward: ")) + Ach.RewardLabel);
		const FString StateStr = Ach.bUnlocked ? (Ach.bIsNew ? FString(TEXT("NEW!")) : FString(TEXT("EARNED"))) : (bSecret ? FString(TEXT("???")) : BHAchievementTierName(Ach.Difficulty));
		const FLinearColor StateColor = Ach.bUnlocked ? (Ach.bIsNew ? FLinearColor(1.0f, 0.86f, 0.30f, 1.0f) : FLinearColor(0.40f, 0.86f, 0.52f, 1.0f)) : TierColor;
		const float ProgressPct = Ach.ProgressTarget > 0 ? static_cast<float>(Ach.ProgressCurrent) / static_cast<float>(Ach.ProgressTarget) : 0.0f;
		const FString ProgressStr = FString::Printf(TEXT("%d / %d"), Ach.ProgressCurrent, Ach.ProgressTarget);
		const bool bShowProgress = Ach.ProgressTarget > 0 && !Ach.bUnlocked && !bSecret;

		BadgeList->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(Ach.bUnlocked ? FLinearColor(0.055f, 0.078f, 0.066f, 0.95f) : FLinearColor(0.040f, 0.044f, 0.052f, 0.95f))
			.Padding(9.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 9.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(5.0f)
					[
						SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(TierColor)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							Meter
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(MenuFont(11, FName(TEXT("Bold"))))
							.ColorAndOpacity(TitleColor)
							.Text(FText::FromString(TitleStr))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Font(MenuFont(9))
						.ColorAndOpacity(FLinearColor(0.66f, 0.70f, 0.74f, 1.0f))
						.Text(FText::FromString(DescStr))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Visibility(RewardStr.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						.Font(MenuFont(9, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.74f, 1.0f))
						.Text(FText::FromString(RewardStr))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.Visibility(bShowProgress ? EVisibility::Visible : EVisibility::Collapsed)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SBox).HeightOverride(7.0f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(FMath::Max(ProgressPct, 0.0001f))
									[
										SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(TierColor)
									]
									+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - ProgressPct, 0.0001f))
									[
										SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.13f, 0.14f, 0.16f, 1.0f))
									]
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(MenuFont(8)).ColorAndOpacity(FLinearColor(0.62f, 0.66f, 0.70f, 1.0f)).Text(FText::FromString(ProgressStr))
							]
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(9, FName(TEXT("Bold"))))
					.ColorAndOpacity(StateColor)
					.Text(FText::FromString(StateStr))
				]
			]
		];
	}

	// Per-topic physics mastery (from in-round answers; persisted in the account).
	static const TCHAR* BHTopicNames[4] = { TEXT("Forces & Motion"), TEXT("Electricity"), TEXT("Waves"), TEXT("Energy") };
	TSharedRef<SVerticalBox> MasteryRows = SNew(SVerticalBox);
	for (int32 TopicIdx = 0; TopicIdx < 4; ++TopicIdx)
	{
		const int32 TopicTotal = (AccountSubsystem && AccountSubsystem->GetProgress().TopicAnswerCounts.IsValidIndex(TopicIdx)) ? AccountSubsystem->GetProgress().TopicAnswerCounts[TopicIdx] : 0;
		const int32 TopicCorrect = (AccountSubsystem && AccountSubsystem->GetProgress().TopicCorrectCounts.IsValidIndex(TopicIdx)) ? AccountSubsystem->GetProgress().TopicCorrectCounts[TopicIdx] : 0;
		const float TopicPct = TopicTotal > 0 ? static_cast<float>(TopicCorrect) / static_cast<float>(TopicTotal) : 0.0f;
		const FLinearColor BarColor = TopicTotal == 0
			? FLinearColor(0.30f, 0.32f, 0.35f, 1.0f)
			: (TopicPct >= 0.7f ? FLinearColor(0.36f, 0.82f, 0.46f, 1.0f)
				: (TopicPct >= 0.4f ? FLinearColor(0.88f, 0.74f, 0.30f, 1.0f) : FLinearColor(0.88f, 0.44f, 0.36f, 1.0f)));
		const FString TopicStat = TopicTotal > 0 ? FString::Printf(TEXT("%d/%d  %.0f%%"), TopicCorrect, TopicTotal, TopicPct * 100.0f) : FString(TEXT("--"));
		MasteryRows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(108.0f)
				[
					SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.78f, 0.82f, 0.86f, 1.0f)).Text(FText::FromString(BHTopicNames[TopicIdx]))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBox).HeightOverride(9.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(FMath::Max(TopicPct, 0.0001f))
					[
						SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(BarColor)
					]
					+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - TopicPct, 0.0001f))
					[
						SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.13f, 0.14f, 0.16f, 1.0f))
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(70.0f)
				[
					SNew(STextBlock).Font(MenuFont(9)).ColorAndOpacity(FLinearColor(0.70f, 0.74f, 0.78f, 1.0f)).Text(FText::FromString(TopicStat))
				]
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.034f, 0.034f, 0.044f, 0.95f))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.06f, 0.07f, 0.085f, 0.95f))
				.Padding(9.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(MenuFont(15, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.95f, 0.86f, 0.55f, 1.0f)).Text(FText::FromString(RankName))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(MenuFont(9)).ColorAndOpacity(FLinearColor(0.70f, 0.74f, 0.78f, 1.0f)).Text(FText::FromString(RankXpStr))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 6.0f)
					[
						SNew(SBox).HeightOverride(8.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(FMath::Max(RankPct, 0.0001f))
							[
								SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.42f, 0.66f, 0.92f, 1.0f))
							]
							+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - RankPct, 0.0001f))
							[
								SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.13f, 0.14f, 0.16f, 1.0f))
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).AutoWrapText(true).Font(MenuFont(9)).ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.76f, 1.0f)).Text(FText::FromString(StatsStr))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.92f, 0.88f, 0.74f, 1.0f))
				.Text(FText::FromString(TEXT("Achievements")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 9.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.66f, 0.72f, 0.78f, 1.0f))
				.Text(FText::FromString(FString::Printf(TEXT("Earned %d of %d. Each badge shows its difficulty (more pips = harder, colour-coded by tier) and the cosmetic it unlocks. Hidden badges stay secret until you find them."), Earned, Total)))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(11, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.82f, 0.88f, 0.80f, 1.0f))
				.Text(FText::FromString(TEXT("Your physics mastery (from in-round answers)")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 11.0f)
			[
				MasteryRows
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BadgeList
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildFeedbackPanel()
{
	auto KindButton = [this](EBHFeedbackKind Kind, const FText& Label) -> TSharedRef<SWidget>
	{
		return SNew(SBHMenuButton)
			.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetFeedbackKindButtonColor, Kind)))
			.ContentPadding(FMargin(12.0f, 6.0f))
			.OnClicked(this, &SBHMainMenu::OnFeedbackKindClicked, Kind)
			[
				SNew(STextBlock)
				.Font(MenuFont(11, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.92f, 0.95f, 0.95f, 1.0f))
				.Text(Label)
			];
	};

	auto RatingButton = [this](int32 Rating) -> TSharedRef<SWidget>
	{
		return SNew(SBHMenuButton)
			.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetFeedbackRatingButtonColor, Rating)))
			.ContentPadding(FMargin(10.0f, 6.0f))
			.OnClicked(this, &SBHMainMenu::OnFeedbackRatingClicked, Rating)
			[
				SNew(STextBlock)
				.Font(MenuFont(12, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.96f, 0.92f, 0.70f, 1.0f))
				.Text(FText::AsNumber(Rating))
			];
	};

	TSharedRef<SHorizontalBox> KindRow = SNew(SHorizontalBox);
	KindRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[ KindButton(EBHFeedbackKind::Bug, FText::FromString(TEXT("Bug"))) ];
	KindRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[ KindButton(EBHFeedbackKind::Idea, FText::FromString(TEXT("Feature idea"))) ];
	KindRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[ KindButton(EBHFeedbackKind::Praise, FText::FromString(TEXT("Liked it"))) ];
	KindRow->AddSlot().AutoWidth()[ KindButton(EBHFeedbackKind::Other, FText::FromString(TEXT("Other"))) ];

	TSharedRef<SHorizontalBox> RatingRow = SNew(SHorizontalBox);
	for (int32 Score = 1; Score <= 5; ++Score)
	{
		RatingRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ RatingButton(Score) ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(MenuFont(18, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.96f, 0.92f, 0.82f, 1.0f))
			.Text(FText::FromString(TEXT("Feedback")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 12.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(MenuFont(12))
			.ColorAndOpacity(FLinearColor(0.58f, 0.66f, 0.66f, 1.0f))
			.Text(FText::FromString(TEXT("Report a bug, suggest an idea, or tell us what you liked. It goes straight to the developer.")))
		]
		// End-of-round survey: shown after a round, hidden once sent or dismissed.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBox)
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([this]()
			{
				const bool bPending = PlayerController.IsValid() && PlayerController->IsEndOfRoundSurveyPendingForMenu();
				return bPending ? EVisibility::Visible : EVisibility::Collapsed;
			})))
			[
				BuildEndOfRoundSurveyPanel()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Font(MenuFont(10, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
			.Text(FText::FromString(TEXT("WHAT'S THIS ABOUT?")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			KindRow
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Font(MenuFont(10, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
			.Text(FText::FromString(TEXT("YOUR MESSAGE")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			SNew(SBox)
			.HeightOverride(120.0f)
			[
				SAssignNew(FeedbackMessageTextBox, SMultiLineEditableTextBox)
				.HintText(FText::FromString(TEXT("Describe the bug, the idea, or what happened...")))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
				.Text(FText::FromString(TEXT("RATING (OPTIONAL)")))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				RatingRow
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Font(MenuFont(10, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
			.Text(FText::FromString(TEXT("CONTACT (OPTIONAL, IF YOU WANT A REPLY)")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			SAssignNew(FeedbackContactTextBox, SEditableTextBox)
			.HintText(FText::FromString(TEXT("Email or username (optional)")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SCheckBox)
			.IsChecked(this, &SBHMainMenu::GetIncludeDiagnosticsState)
			.OnCheckStateChanged(this, &SBHMainMenu::OnIncludeDiagnosticsChanged)
			[
				SNew(STextBlock)
				.Font(MenuFont(11))
				.ColorAndOpacity(FLinearColor(0.80f, 0.86f, 0.86f, 1.0f))
				.Text(FText::FromString(TEXT("Include diagnostics (frame-rate stats, recent log lines, PC specs)")))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(MenuFont(10))
			.ColorAndOpacity(FLinearColor(0.52f, 0.58f, 0.60f, 1.0f))
			.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([this]()
			{
				if (const ABHPlayerController* PC = PlayerController.Get())
				{
					return FText::FromString(PC->GetFeedbackPrivacySummaryForMenu());
				}
				return FText::GetEmpty();
			})))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SBHMenuButton)
			.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.40f, 0.32f, 1.0f)))
			.ContentPadding(FMargin(18.0f, 9.0f))
			.HAlign(HAlign_Center)
			.OnClicked(this, &SBHMainMenu::OnSubmitFeedbackClicked)
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.95f, 1.0f, 0.97f, 1.0f))
				.Text(FText::FromString(TEXT("SEND FEEDBACK")))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.030f, 0.038f, 0.044f, 0.95f))
			.Padding(12.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(11))
				.ColorAndOpacity(FLinearColor(0.74f, 0.82f, 0.78f, 1.0f))
				.Text(this, &SBHMainMenu::GetFeedbackStatusText)
			]
		];
}

float SBHMainMenu::GetMasterVolumeValue() const
{
	if (const ABHPlayerController* PC = PlayerController.Get())
	{
		return PC->GetMasterVolumeForMenu();
	}

	return 1.0f;
}

float SBHMainMenu::GetMusicVolumeValue() const
{
	if (const ABHPlayerController* PC = PlayerController.Get())
	{
		return PC->GetMusicVolumeForMenu();
	}

	return 0.85f;
}

float SBHMainMenu::GetUiVolumeValue() const
{
	if (const ABHPlayerController* PC = PlayerController.Get())
	{
		return PC->GetUiVolumeForMenu();
	}

	return 0.9f;
}

FText SBHMainMenu::GetMasterVolumeText() const
{
	return FText::FromString(FString::Printf(TEXT("Master %d%%"), FMath::RoundToInt(GetMasterVolumeValue() * 100.0f)));
}

FText SBHMainMenu::GetMusicVolumeText() const
{
	return FText::FromString(FString::Printf(TEXT("Music %d%%"), FMath::RoundToInt(GetMusicVolumeValue() * 100.0f)));
}

FText SBHMainMenu::GetUiVolumeText() const
{
	return FText::FromString(FString::Printf(TEXT("UI %d%%"), FMath::RoundToInt(GetUiVolumeValue() * 100.0f)));
}

FText SBHMainMenu::GetGraphicsSummaryText() const
{
	if (const ABHPlayerController* PC = PlayerController.Get())
	{
		return FText::FromString(PC->GetGraphicsSummaryForMenu());
	}

	return FText::FromString(TEXT("Graphics unavailable."));
}

EVisibility SBHMainMenu::GetStartCredentialsVisibility() const
{
	return bShowStartCredentials ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SBHMainMenu::GetStartBackgroundBrush() const
{
	return StartBackgroundTexture.IsValid() ? &StartBackgroundBrush : &StartBackgroundFallbackBrush;
}

const FSlateBrush* SBHMainMenu::GetGuideHudBrush() const
{
	return GuideHudTexture.IsValid() ? &GuideHudBrush : &GuideHudFallbackBrush;
}

int32 SBHMainMenu::GetRootWidgetIndex() const
{
	return bShowingStartScreen ? 0 : 1;
}

TSharedRef<SWidget> SBHMainMenu::BuildStartScreen()
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.006f, 0.005f, 0.006f, 1.0f))
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(TAttribute<const FSlateBrush*>::Create(TAttribute<const FSlateBrush*>::FGetter::CreateSP(this, &SBHMainMenu::GetStartBackgroundBrush)))
			.ColorAndOpacity(FLinearColor(0.94f, 0.91f, 0.88f, 0.96f))
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.24f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.HeightOverride(8.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.62f, 0.02f, 0.015f, 0.80f))
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			SNew(SBox)
			.HeightOverride(120.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.05f, 0.0f, 0.0f, 0.72f))
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(56.0f, 48.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBox)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(52, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.93f, 0.86f, 0.78f, 1.0f))
				.ShadowOffset(FVector2D(3.0f, 3.0f))
				.ShadowColorAndOpacity(FLinearColor(0.35f, 0.0f, 0.0f, 1.0f))
				.Text(FText::FromString(TEXT("BLACKOUT HUNT")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 4.0f, 0.0f, 22.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(15))
				.ColorAndOpacity(FLinearColor(0.78f, 0.64f, 0.56f, 1.0f))
				.Text(FText::FromString(TEXT("The lights are out. Something is listening.")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.ButtonColorAndOpacity(FLinearColor(0.42f, 0.03f, 0.025f, 1.0f))
					.ContentPadding(FMargin(30.0f, 15.0f))
					.OnClicked(this, &SBHMainMenu::OnStartClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(22, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("START")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SBHMenuButton)
						.ButtonColorAndOpacity(FLinearColor(0.12f, 0.25f, 0.24f, 0.98f))
						.ContentPadding(FMargin(22.0f, 13.0f))
						.OnClicked(this, &SBHMainMenu::OnStartGuideClicked)
						[
							SNew(STextBlock)
							.Font(MenuFont(13, FName(TEXT("Bold"))))
							.Text(FText::FromString(TEXT("HOW TO PLAY")))
						]
					]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.ButtonColorAndOpacity(FLinearColor(0.070f, 0.075f, 0.080f, 0.96f))
					.ContentPadding(FMargin(18.0f, 13.0f))
					.OnClicked(this, &SBHMainMenu::OnToggleStartCredentialsClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(13, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("CREDENTIALS")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 18.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(440.0f)
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBHMainMenu::GetStartCredentialsVisibility)))
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(FLinearColor(0.018f, 0.020f, 0.022f, 0.94f))
					.Padding(14.0f)
					[
						BuildLocalCredentialPanel(true)
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.25f)
			[
				SNew(SBox)
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.Padding(56.0f, 0.0f, 56.0f, 22.0f)
		[
			BuildStatusPanel()
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildMenuTabButton(EBHMainMenuTab Tab, const FText& Label)
{
	return SNew(SBHMenuButton)
		.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetMenuTabColor, Tab)))
		.ContentPadding(FMargin(16.0f, 8.0f))
		.OnClicked(this, &SBHMainMenu::OnMenuTabClicked, Tab)
		[
			SNew(STextBlock)
			.Font(MenuFont(12, FName(TEXT("Bold"))))
			.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetMenuTabTextColor, Tab)))
			.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetMenuTabLabel, Tab, Label)))
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildPlayJoinAddressPanel()
{
	TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
				.Text(FText::FromString(TEXT("YOUR LOBBY NAME")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SAssignNew(JoinNameTextBox, SEditableTextBox)
				.Text(GetDefaultJoinNameText())
				.HintText(FText::FromString(TEXT("Enter your name before joining")))
				.SelectAllTextWhenFocused(true)
			]
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
					.Text(FText::FromString(TEXT("LAN HOST / IP / CODE")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SAssignNew(JoinHostTextBox, SEditableTextBox)
					.Text(FText::FromString(MenuExtractHost(SuggestedAddress)))
					.HintText(FText::FromString(TEXT("LAN IP, hostname, or LAN join code")))
					.SelectAllTextWhenFocused(true)
					.OnTextCommitted(this, &SBHMainMenu::OnAddressCommitted)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(88.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.72f, 1.0f))
						.Text(FText::FromString(TEXT("PORT")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SAssignNew(JoinPortTextBox, SEditableTextBox)
						.Text(MenuPortText(MenuExtractPort(SuggestedAddress)))
						.HintText(FText::FromString(TEXT("7777")))
						.SelectAllTextWhenFocused(true)
						.OnTextCommitted(this, &SBHMainMenu::OnAddressCommitted)
					]
				]
			]
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			BuildClassroomJoinListPanel()
		];

	auto AddAction = [](const TSharedRef<SVerticalBox>& Group, const TSharedRef<SWidget>& Action)
	{
		Group->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				Action
			];
	};

	AddAction(Panel, MenuPlayActionButton(
		FText::FromString(TEXT("COPY LAN JOIN CODE")),
		FText::FromString(TEXT("Copies the current LAN host invite.")),
		FLinearColor(0.18f, 0.32f, 0.30f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnCopyJoinInviteClicked)));
	AddAction(Panel, MenuPlayActionButton(
		FText::FromString(TEXT("JOIN LAN GAME")),
		FText::FromString(TEXT("Connects to the LAN address above.")),
		FLinearColor(0.24f, 0.30f, 0.42f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJoinClicked)));
	AddAction(Panel, MenuPlayActionButton(
		FText::FromString(TEXT("JOIN LOCAL TEST")),
		FText::FromString(TEXT("Uses the local test address for same-machine checks.")),
		FLinearColor(0.18f, 0.20f, 0.24f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJoinLocalClicked)));

	return Panel;
}

TSharedRef<SWidget> SBHMainMenu::BuildClassroomJoinListPanel()
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);

	List->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			SNew(STextBlock)
			.Font(MenuFont(10, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.80f, 1.0f))
			.Text(FText::FromString(TEXT("CLASSROOM JOIN LIST")))
		];

	TArray<FString> Endpoints;
	if (const UBHGameSettings* Settings = GetDefault<UBHGameSettings>())
	{
		Endpoints = Settings->ClassroomJoinEndpoints;
	}
	if (Endpoints.IsEmpty())
	{
		List->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.58f, 0.66f, 0.67f, 1.0f))
				.Text(FText::FromString(TEXT("No saved classroom endpoints are configured.")))
			];
		return List;
	}

	for (const FString& Endpoint : Endpoints)
	{
		const FString NormalizedEndpoint = FBHNetworkSupport::NormalizeJoinAddress(Endpoint);
		if (NormalizedEndpoint.IsEmpty())
		{
			continue;
		}

		List->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				MenuPlayActionButton(
					FText::FromString(NormalizedEndpoint),
					FText::FromString(TEXT("One-click join for the shipped classroom tunnel.")),
					FLinearColor(0.20f, 0.34f, 0.32f, 1.0f),
					FOnClicked::CreateSP(this, &SBHMainMenu::OnJoinSavedEndpointClicked, NormalizedEndpoint))
			];
	}

	return List;
}

void SBHMainMenu::BuildPlayActionList(TSharedRef<SVerticalBox> ActionList, bool bInGame, bool bPracticeMode, bool bTestMode)
{
	auto AddAction = [](const TSharedRef<SVerticalBox>& Group, const TSharedRef<SWidget>& Action)
	{
		Group->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				Action
			];
	};

	AddAction(ActionList, MenuPlayActionButton(
		FText::FromString(TEXT("HOW TO PLAY")),
		FText::FromString(TEXT("Open the annotated HUD guide, first controls, and role walkthroughs.")),
		FLinearColor(0.12f, 0.31f, 0.29f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnMenuTabClicked, EBHMainMenuTab::Guide)));

	if (bInGame)
	{
		const ABHPlayerController* PC = PlayerController.Get();
		const UWorld* World = PC ? PC->GetWorld() : nullptr;
		const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
		const bool bCanEndWarmup = PC && PC->IsLocalController() && World && World->GetNetMode() == NM_ListenServer && BHGS && BHGS->RoundPhase == EBHRoundPhase::Prep;
		const bool bHostAdmin = CanEditRoles() || bCanEndWarmup || bPracticeMode || bTestMode;
		const ABHPlayerState* LocalPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		const bool bLocalSpectator = LocalPS && LocalPS->PlayerRole == EBHPlayerRole::Spectator;
		TSharedRef<SVerticalBox> SessionActions = SNew(SVerticalBox);
		AddAction(SessionActions, MenuPlayActionButton(
			FText::FromString(TEXT("RESUME GAME")),
			FText::FromString(TEXT("Return to the active round.")),
			FLinearColor(0.12f, 0.38f, 0.34f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnResumeClicked)));
		if (!bPracticeMode && !bTestMode)
		{
			AddAction(SessionActions, MenuPlayActionButton(
				GetReadyButtonText(),
				FText::FromString(TEXT("Press this when your name appears in the lobby roster.")),
				FLinearColor(0.32f, 0.28f, 0.12f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnReadyClicked),
				TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanReadyFromMenu))));
		}
		if (bCanEndWarmup)
		{
			AddAction(SessionActions, MenuPlayActionButton(
				FText::FromString(TEXT("START HUNT NOW")),
				FText::FromString(TEXT("Host-only: ends role warmup, clears practice props, and resets players for the live round.")),
				FLinearColor(0.38f, 0.22f, 0.12f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnForceStartClicked)));
		}
		if (CanOpenClassroomBoard())
		{
			AddAction(SessionActions, MenuPlayActionButton(
				FText::FromString(TEXT("OPEN CLASSROOM BOARD")),
				FText::FromString(TEXT("Shows the projector-ready classroom view.")),
				FLinearColor(0.18f, 0.36f, 0.34f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenClassroomBoardClicked)));
		}
		if (bPracticeMode || bTestMode)
		{
			AddAction(SessionActions, MenuPlayActionButton(
				bPracticeMode ? FText::FromString(TEXT("REFRESH PRACTICE ROUND")) : FText::FromString(TEXT("REFRESH TEST ROUND")),
				FText::FromString(TEXT("Resets the current practice or test round.")),
				FLinearColor(0.22f, 0.34f, 0.18f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnPracticeRefreshClicked)));
		}
		AddAction(SessionActions, MenuPlayActionButton(
			FText::FromString(TEXT("LEAVE TO MAIN MENU")),
			FText::FromString(TEXT("Disconnects from the current session.")),
			FLinearColor(0.22f, 0.23f, 0.27f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnDisconnectClicked)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Session")),
			FText::FromString(TEXT("Resume, board, refresh, or leave.")),
			FLinearColor(0.12f, 0.38f, 0.34f, 1.0f),
			SessionActions);

		if (bLocalSpectator)
		{
			TSharedRef<SVerticalBox> SpectatorActions = SNew(SVerticalBox);
			AddAction(SpectatorActions, MenuPlayActionButton(
				FText::FromString(TEXT("SEND TEAM SUPPORT")),
				FText::FromString(TEXT("Sends a harmless, non-location team encouragement.")),
				FLinearColor(0.20f, 0.24f, 0.42f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnSpectatorEncourageClicked)));
			AddAction(SpectatorActions, MenuPlayActionButton(
				FText::FromString(TEXT("REQUEST TEACHER NEXT")),
				FText::FromString(TEXT("Adds a host-approved next-round role preference.")),
				FLinearColor(0.34f, 0.16f, 0.14f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnSpectatorRolePreferenceClicked, EBHPlayerRole::Hunter)));
			AddAction(SpectatorActions, MenuPlayActionButton(
				FText::FromString(TEXT("REQUEST SURVIVOR NEXT")),
				FText::FromString(TEXT("Adds a host-approved next-round role preference.")),
				FLinearColor(0.14f, 0.34f, 0.30f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnSpectatorRolePreferenceClicked, EBHPlayerRole::Survivor)));
			AddAction(SpectatorActions, MenuPlayActionButton(
				FText::FromString(TEXT("REQUEST MONITOR NEXT")),
				FText::FromString(TEXT("Adds a host-approved next-round role preference.")),
				FLinearColor(0.34f, 0.24f, 0.12f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnSpectatorRolePreferenceClicked, EBHPlayerRole::FakeHunter)));
			AddAction(SpectatorActions, MenuPlayActionButton(
				FText::FromString(TEXT("CLEAR ROLE REQUEST")),
				FText::FromString(TEXT("Clears the preference shown to the host.")),
				FLinearColor(0.18f, 0.19f, 0.22f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnSpectatorRolePreferenceClicked, EBHPlayerRole::Unassigned)));
			MenuAddPlayDropdownSection(
				ActionList,
				FText::FromString(TEXT("Spectator Support")),
				FText::FromString(FString::Printf(TEXT("Preference: %s"), *MenuSpectatorRolePreferenceName(LocalPS->SpectatorRolePreference))),
				FLinearColor(0.20f, 0.24f, 0.42f, 1.0f),
				SpectatorActions);
		}

		if (bHostAdmin)
		{
			TSharedRef<SVerticalBox> LevelActions = SNew(SVerticalBox);
			AddAction(LevelActions, MenuPlayActionButton(
				FText::FromString(TEXT("RESTART PHYSICS CLASSROOM")),
				FText::FromString(TEXT("Restarts the 10-minute revision escape round.")),
				FLinearColor(0.30f, 0.25f, 0.42f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnHostPhysicsClassroomClicked)));
			AddAction(LevelActions, MenuPlayActionButton(
				FText::FromString(TEXT("NEXT LEVEL: FACILITY")),
				FText::FromString(TEXT("Queues Facility as the next map.")),
				FLinearColor(0.18f, 0.28f, 0.36f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnNextFacilityClicked)));
			AddAction(LevelActions, MenuPlayActionButton(
				FText::FromString(TEXT("NEXT LEVEL: SUBSTATION")),
				FText::FromString(TEXT("Queues Substation as the next map.")),
				FLinearColor(0.14f, 0.25f, 0.39f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnNextSubstationClicked)));
			AddAction(LevelActions, MenuPlayActionButton(
				FText::FromString(TEXT("NEXT LEVEL: FOGGROUNDS")),
				FText::FromString(TEXT("Queues Foggrounds as the next map.")),
				FLinearColor(0.13f, 0.28f, 0.24f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnNextFoggroundsClicked)));
			MenuAddPlayDropdownSection(
				ActionList,
				FText::FromString(TEXT("Level Select")),
				FText::FromString(TEXT("Classroom and next-map controls.")),
				FLinearColor(0.18f, 0.28f, 0.36f, 1.0f),
				LevelActions);

			TSharedRef<SVerticalBox> TestActions = SNew(SVerticalBox);
			AddAction(TestActions, MenuPlayActionButton(
				FText::FromString(TEXT("TEST ROUND")),
				FText::FromString(TEXT("Tester role, all mechanics, no timer.")),
				FLinearColor(0.18f, 0.34f, 0.38f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnHostTestRoundClicked)));
			AddAction(TestActions, MenuPlayActionButton(
				FText::FromString(TEXT("TEST SUBSTATION")),
				FText::FromString(TEXT("Runs the test mode on Substation.")),
				FLinearColor(0.15f, 0.27f, 0.40f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnHostSubstationTestRoundClicked)));
			AddAction(TestActions, MenuPlayActionButton(
				FText::FromString(TEXT("TEST FOGGROUNDS")),
				FText::FromString(TEXT("Runs the test mode on Foggrounds.")),
				FLinearColor(0.16f, 0.31f, 0.24f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnHostFoggroundsTestRoundClicked)));
			MenuAddPlayDropdownSection(
				ActionList,
				FText::FromString(TEXT("Testing")),
				FText::FromString(TEXT("Fast test rounds by map.")),
				FLinearColor(0.18f, 0.34f, 0.38f, 1.0f),
				TestActions);
		}

		TSharedRef<SVerticalBox> ProfileActions = SNew(SVerticalBox);
		AddAction(ProfileActions, MenuPlayActionButton(
			FText::FromString(TEXT("CYCLE AVATAR COLOR")),
			FText::FromString(TEXT("Rotates the current avatar color.")),
			FLinearColor(0.27f, 0.21f, 0.36f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnAvatarClicked)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Profile")),
			FText::FromString(TEXT("Quick character action.")),
			FLinearColor(0.27f, 0.21f, 0.36f, 1.0f),
			ProfileActions);
	}
	else
	{
		AddAction(ActionList, MenuPlayActionButton(
			FText::FromString(TEXT("LIVE CLASSROOM: FACILITY")),
			FText::FromString(TEXT("Playit classroom host, projector board, all-ready roster, Physics defaults.")),
			FLinearColor(0.16f, 0.42f, 0.37f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostLiveClassroomMapClicked, FString(TEXT("Facility")))));
		AddAction(ActionList, MenuPlayActionButton(
			FText::FromString(TEXT("LIVE CLASSROOM: SUBSTATION")),
			FText::FromString(TEXT("Playit classroom on the Substation map.")),
			FLinearColor(0.14f, 0.28f, 0.42f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostLiveClassroomMapClicked, FString(TEXT("Substation")))));
		AddAction(ActionList, MenuPlayActionButton(
			FText::FromString(TEXT("LIVE CLASSROOM: FOGGROUNDS")),
			FText::FromString(TEXT("Playit classroom on the larger fog map.")),
			FLinearColor(0.14f, 0.36f, 0.27f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostLiveClassroomMapClicked, FString(TEXT("Foggrounds")))));

		AddAction(ActionList, MenuPlayActionButton(
			FText::FromString(TEXT("TUTORIAL: LEARN TO PLAY")),
			FText::FromString(TEXT("Full solo course - Survivor, then Teacher, then Hall Monitor, back to back. No host needed.")),
			FLinearColor(0.20f, 0.40f, 0.30f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostTutorialClicked)));

		// Jump straight to one role's tutorial (plays just that lesson, then returns to the menu).
		TSharedRef<SVerticalBox> TutorialActions = SNew(SVerticalBox);
		AddAction(TutorialActions, MenuPlayActionButton(
			FText::FromString(TEXT("SURVIVOR TUTORIAL")),
			FText::FromString(TEXT("Move, flashlight, hide, crawl, fix a breaker, answer questions, escape a Teacher.")),
			FLinearColor(0.20f, 0.40f, 0.30f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostTutorialPhaseClicked, EBHTutorialPhase::Survivor, false)));
		AddAction(TutorialActions, MenuPlayActionButton(
			FText::FromString(TEXT("TEACHER TUTORIAL")),
			FText::FromString(TEXT("Play the Hunter - scan, chase and capture a student, black out the lights.")),
			FLinearColor(0.42f, 0.22f, 0.20f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostTutorialPhaseClicked, EBHTutorialPhase::Teacher, false)));
		AddAction(TutorialActions, MenuPlayActionButton(
			FText::FromString(TEXT("HALL MONITOR TUTORIAL")),
			FText::FromString(TEXT("Play the monitor - real hints, false markers, and traps to mislead the Teacher.")),
			FLinearColor(0.30f, 0.26f, 0.44f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostTutorialPhaseClicked, EBHTutorialPhase::Monitor, false)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Tutorial: one role")),
			FText::FromString(TEXT("Practise a single role's tutorial.")),
			FLinearColor(0.20f, 0.40f, 0.30f, 1.0f),
			TutorialActions);

		TSharedRef<SVerticalBox> QuickStartActions = SNew(SVerticalBox);
		AddAction(QuickStartActions, MenuPlayActionButton(
			FText::FromString(TEXT("TEST ROUND")),
			FText::FromString(TEXT("Tester role, all mechanics, no timer.")),
			FLinearColor(0.18f, 0.34f, 0.38f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostTestRoundClicked)));
		AddAction(QuickStartActions, MenuPlayActionButton(
			FText::FromString(TEXT("TEST SUBSTATION")),
			FText::FromString(TEXT("Runs the test mode on Substation.")),
			FLinearColor(0.15f, 0.27f, 0.40f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostSubstationTestRoundClicked)));
		AddAction(QuickStartActions, MenuPlayActionButton(
			FText::FromString(TEXT("TEST FOGGROUNDS")),
			FText::FromString(TEXT("Runs the test mode on Foggrounds.")),
			FLinearColor(0.16f, 0.31f, 0.24f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostFoggroundsTestRoundClicked)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Test Rounds")),
			FText::FromString(TEXT("Tester mode by map.")),
			FLinearColor(0.18f, 0.34f, 0.38f, 1.0f),
			QuickStartActions);

		TSharedRef<SVerticalBox> BotActions = SNew(SVerticalBox);
		AddAction(BotActions, MenuPlayActionButton(
			FText::FromString(TEXT("BOT FACILITY")),
			FText::FromString(TEXT("Lobby filled with bots - friends can still join.")),
			FLinearColor(0.24f, 0.33f, 0.29f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostBotClicked)));
		AddAction(BotActions, MenuPlayActionButton(
			FText::FromString(TEXT("BOT SUBSTATION")),
			FText::FromString(TEXT("Substation lobby with bots - friends can still join.")),
			FLinearColor(0.20f, 0.28f, 0.38f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostBotSubstationClicked)));
		AddAction(BotActions, MenuPlayActionButton(
			FText::FromString(TEXT("BOT FOGGROUNDS")),
			FText::FromString(TEXT("Foggrounds lobby with bots - friends can still join.")),
			FLinearColor(0.18f, 0.31f, 0.25f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostBotFoggroundsClicked)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Bots")),
			FText::FromString(TEXT("Bot lobbies by map - others can still join.")),
			FLinearColor(0.24f, 0.33f, 0.29f, 1.0f),
			BotActions);

		TSharedRef<SVerticalBox> HostActions = SNew(SVerticalBox);
		AddAction(HostActions, MenuPlayActionButton(
			FText::FromString(TEXT("HOST FACILITY")),
			FText::FromString(TEXT("Direct-IP listen server on Facility.")),
			FLinearColor(0.13f, 0.42f, 0.36f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostClicked)));
		AddAction(HostActions, MenuPlayActionButton(
			FText::FromString(TEXT("HOST SUBSTATION")),
			FText::FromString(TEXT("Direct-IP listen server on Substation.")),
			FLinearColor(0.14f, 0.30f, 0.42f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostSubstationClicked)));
		AddAction(HostActions, MenuPlayActionButton(
			FText::FromString(TEXT("HOST FOGGROUNDS")),
			FText::FromString(TEXT("Direct-IP listen server on Foggrounds.")),
			FLinearColor(0.13f, 0.31f, 0.24f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostFoggroundsClicked)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Host LAN")),
			FText::FromString(TEXT("Direct-IP hosting by map.")),
			FLinearColor(0.13f, 0.42f, 0.36f, 1.0f),
			HostActions);

		TSharedRef<SVerticalBox> OnlineActions = SNew(SVerticalBox);
		AddAction(OnlineActions, MenuPlayActionButton(
			FText::FromString(TEXT("HOST ONLINE FACILITY")),
			FText::FromString(TEXT("Creates an online Facility lobby.")),
			FLinearColor(0.28f, 0.33f, 0.50f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostOnlineClicked)));
		AddAction(OnlineActions, MenuPlayActionButton(
			FText::FromString(TEXT("HOST ONLINE SUBSTATION")),
			FText::FromString(TEXT("Creates an online Substation lobby.")),
			FLinearColor(0.22f, 0.27f, 0.42f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostOnlineSubstationClicked)));
		AddAction(OnlineActions, MenuPlayActionButton(
			FText::FromString(TEXT("HOST ONLINE FOGGROUNDS")),
			FText::FromString(TEXT("Creates an online Foggrounds lobby.")),
			FLinearColor(0.18f, 0.30f, 0.25f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostOnlineFoggroundsClicked)));
		AddAction(OnlineActions, MenuPlayActionButton(
			FText::FromString(TEXT("FIND ONLINE SESSIONS")),
			FText::FromString(TEXT("Refreshes the online lobby list.")),
			FLinearColor(0.18f, 0.24f, 0.34f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnFindOnlineSessionsClicked)));
		AddAction(OnlineActions, MenuPlayActionButton(
			FText::FromString(TEXT("JOIN FIRST ONLINE SESSION")),
			FText::FromString(TEXT("Joins the first lobby returned by the search.")),
			FLinearColor(0.20f, 0.36f, 0.30f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnJoinFirstOnlineSessionClicked),
			TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanJoinFirstOnlineSession))));
		AddAction(OnlineActions, MenuPlayActionButton(
			FText::FromString(TEXT("CLEAR ONLINE SESSION")),
			FText::FromString(TEXT("Destroys the current online session handle.")),
			FLinearColor(0.16f, 0.17f, 0.21f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnDestroyOnlineSessionClicked)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Online Lobbies")),
			FText::FromString(TEXT("Host, find, join, or clear online sessions.")),
			FLinearColor(0.28f, 0.33f, 0.50f, 1.0f),
			OnlineActions);

		TSharedRef<SVerticalBox> ConnectionActions = SNew(SVerticalBox);
		AddAction(ConnectionActions, MenuPlayActionButton(
			FText::FromString(TEXT("START INTERNET TUNNEL")),
			FText::FromString(TEXT("Router-free UDP relay for hosts.")),
			FLinearColor(0.26f, 0.31f, 0.18f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnStartInternetTunnelClicked)));
		AddAction(ConnectionActions, MenuPlayActionButton(
			FText::FromString(TEXT("OPEN TUNNEL SETUP")),
			FText::FromString(TEXT("Opens the tunnel setup workflow.")),
			FLinearColor(0.18f, 0.22f, 0.18f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenInternetTunnelSetupClicked)));
		AddAction(ConnectionActions, MenuPlayActionButton(
			FText::FromString(TEXT("STOP INTERNET TUNNEL")),
			FText::FromString(TEXT("Stops the active internet tunnel.")),
			FLinearColor(0.14f, 0.16f, 0.14f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnStopInternetTunnelClicked)));
		AddAction(ConnectionActions, MenuPlayActionButton(
			FText::FromString(TEXT("CREATE GAME HOTSPOT")),
			FText::FromString(TEXT("Dedicated Wi-Fi for blocked classroom LANs.")),
			FLinearColor(0.31f, 0.24f, 0.10f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnCreateHotspotClicked)));
		AddAction(ConnectionActions, MenuPlayActionButton(
			FText::FromString(TEXT("STOP GAME HOTSPOT")),
			FText::FromString(TEXT("Stops the BlackoutHunt hotspot.")),
			FLinearColor(0.18f, 0.17f, 0.15f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnStopHotspotClicked)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Connection Tools")),
			FText::FromString(TEXT("Tunnels and hotspot controls.")),
			FLinearColor(0.26f, 0.31f, 0.18f, 1.0f),
			ConnectionActions);

		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("LAN Join Address")),
			FText::FromString(TEXT("Direct-IP or LAN invite code.")),
			FLinearColor(0.24f, 0.30f, 0.42f, 1.0f),
			BuildPlayJoinAddressPanel());
	}

	TSharedRef<SVerticalBox> SystemActions = SNew(SVerticalBox);
	AddAction(SystemActions, MenuPlayActionButton(
		FText::FromString(TEXT("QUIT")),
		FText::FromString(TEXT("Exit the game.")),
		FLinearColor(0.28f, 0.10f, 0.12f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnQuitClicked)));
	MenuAddPlayDropdownSection(
		ActionList,
		FText::FromString(TEXT("System")),
		FText::FromString(TEXT("Exit controls.")),
		FLinearColor(0.28f, 0.10f, 0.12f, 1.0f),
		SystemActions);
}

FReply SBHMainMenu::OnHostClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->HideMainMenu();
		PC->HostGame();
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostSubstationClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->HideMainMenu();
		PC->HostSubstationGame();
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostFoggroundsClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->HideMainMenu();
		PC->HostFoggroundsGame();
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostPracticeClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostPracticeGameForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostTutorialClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostTutorialGameForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostTutorialPhaseClicked(EBHTutorialPhase StartPhase, bool bChain)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostTutorialPhaseForMenu(StartPhase, bChain, Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostTestRoundClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostTestRoundForMenu(TEXT("Facility"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostSubstationTestRoundClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostTestRoundForMenu(TEXT("Substation"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostFoggroundsTestRoundClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostTestRoundForMenu(TEXT("Foggrounds"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostPhysicsClassroomClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostPhysicsClassroomForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostLiveClassroomClicked()
{
	return OnHostSelectedLiveClassroomClicked();
}

FReply SBHMainMenu::OnHostLiveClassroomMapClicked(FString LevelName)
{
	SelectedLiveClassroomMap = MenuNormalizeClassroomMapName(LevelName);
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostLiveClassroomForMenu(SelectedLiveClassroomMap, Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostSelectedLiveClassroomClicked()
{
	return OnHostLiveClassroomMapClicked(SelectedLiveClassroomMap);
}

FReply SBHMainMenu::OnSelectLiveClassroomMapClicked(FString LevelName)
{
	SelectedLiveClassroomMap = MenuNormalizeClassroomMapName(LevelName);
	StatusText = FText::FromString(FString::Printf(TEXT("Live Classroom map: %s."), *SelectedLiveClassroomMap));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostBotClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostBotGameForMenu(TEXT("Facility"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostBotSubstationClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostBotGameForMenu(TEXT("Substation"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostBotFoggroundsClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostBotGameForMenu(TEXT("Foggrounds"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostOnlineClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostOnlineGameForMenu(TEXT("Facility"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostOnlineSubstationClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostOnlineGameForMenu(TEXT("Substation"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnHostOnlineFoggroundsClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostOnlineGameForMenu(TEXT("Foggrounds"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnFindOnlineSessionsClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->FindOnlineGamesForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnJoinFirstOnlineSessionClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->JoinOnlineGameForMenu(0, Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnDestroyOnlineSessionClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->DestroyOnlineSessionForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnGuestAccountClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ContinueAsGuestForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnGoogleLoginClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->BeginAccountLoginForMenu(TEXT("google"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnMicrosoftLoginClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->BeginAccountLoginForMenu(TEXT("microsoft"), Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnSyncAccountClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SyncAccountForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnSignOutAccountClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SignOutAccountForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnCreateLocalCredentialClicked(bool bFromStartScreen)
{
	const TSharedPtr<SEditableTextBox> UsernameBox = bFromStartScreen ? StartLocalUsernameTextBox : LocalUsernameTextBox;
	const TSharedPtr<SEditableTextBox> PasswordBox = bFromStartScreen ? StartLocalPasswordTextBox : LocalPasswordTextBox;
	const FString Username = UsernameBox.IsValid() ? UsernameBox->GetText().ToString() : FString();
	const FString Password = PasswordBox.IsValid() ? PasswordBox->GetText().ToString() : FString();

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->CreateOrUpdateLocalCredentialForMenu(Username, Password, Message);
		StatusText = FText::FromString(Message);
		if (PasswordBox.IsValid())
		{
			PasswordBox->SetText(FText::GetEmpty());
		}
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnLoginLocalCredentialClicked(bool bFromStartScreen)
{
	const TSharedPtr<SEditableTextBox> UsernameBox = bFromStartScreen ? StartLocalUsernameTextBox : LocalUsernameTextBox;
	const TSharedPtr<SEditableTextBox> PasswordBox = bFromStartScreen ? StartLocalPasswordTextBox : LocalPasswordTextBox;
	const FString Username = UsernameBox.IsValid() ? UsernameBox->GetText().ToString() : FString();
	const FString Password = PasswordBox.IsValid() ? PasswordBox->GetText().ToString() : FString();

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->LoginLocalCredentialForMenu(Username, Password, Message);
		StatusText = FText::FromString(Message);
		if (PasswordBox.IsValid())
		{
			PasswordBox->SetText(FText::GetEmpty());
		}
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnForgetLocalCredentialClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ForgetLocalCredentialForMenu(Message);
		StatusText = FText::FromString(Message);
		if (LocalPasswordTextBox.IsValid())
		{
			LocalPasswordTextBox->SetText(FText::GetEmpty());
		}
		if (StartLocalPasswordTextBox.IsValid())
		{
			StartLocalPasswordTextBox->SetText(FText::GetEmpty());
		}
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnResetLocalClassroomDataClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ResetLocalClassroomDataForMenu(Message);
		StatusText = FText::FromString(Message);
		if (LocalPasswordTextBox.IsValid())
		{
			LocalPasswordTextBox->SetText(FText::GetEmpty());
		}
		if (StartLocalPasswordTextBox.IsValid())
		{
			StartLocalPasswordTextBox->SetText(FText::GetEmpty());
		}
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnCreateHotspotClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->CreateGameHotspotForMenu(Message);
		StatusText = FText::FromString(Message);
		SuggestedAddress = ResolveLocalAddress();

		if (AddressTextBox.IsValid())
		{
			AddressTextBox->SetText(FText::FromString(SuggestedAddress));
		}

		if (JoinHostTextBox.IsValid())
		{
			JoinHostTextBox->SetText(FText::FromString(MenuExtractHost(SuggestedAddress)));
		}

		if (JoinPortTextBox.IsValid())
		{
			JoinPortTextBox->SetText(MenuPortText(MenuExtractPort(SuggestedAddress)));
		}

		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnStopHotspotClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->StopGameHotspotForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnStartInternetTunnelClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->StartInternetTunnelForMenu(Message, GetEnteredPort());
		StatusText = FText::FromString(Message);
		SetJoinAddressFields(ResolvePreferredAddress());
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnOpenInternetTunnelSetupClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->OpenInternetTunnelSetupForMenu(Message, GetEnteredPort());
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnStopInternetTunnelClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->StopInternetTunnelForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnCopyJoinInviteClicked()
{
	const FString EnteredAddress = GetEnteredAddress();
	const FString Address = EnteredAddress.IsEmpty() ? ResolveLocalAddress() : EnteredAddress;
	const FString NormalizedAddress = FBHNetworkSupport::NormalizeJoinAddress(Address);
	const FString InviteCode = FBHNetworkSupport::MakeJoinInviteCode(NormalizedAddress);
	if (InviteCode.IsEmpty() || NormalizedAddress.IsEmpty())
	{
		StatusText = FText::FromString(TEXT("Enter a valid LAN host/IP or LAN join code before copying."));
		return FReply::Handled();
	}

	FPlatformApplicationMisc::ClipboardCopy(*InviteCode);
	StatusText = FText::FromString(FString::Printf(
		TEXT("Copied LAN join code for %s. Players paste it into LAN HOST / IP / CODE and press JOIN LAN GAME."),
		*NormalizedAddress));
	return FReply::Handled();
}

FReply SBHMainMenu::OnJoinClicked()
{
	if (!EnsureJoinDisplayName())
	{
		return FReply::Handled();
	}

	if (JoinHostTextBox.IsValid() && JoinPortTextBox.IsValid())
	{
		const bool bHostIncludesPort = MenuEntryHasExplicitPort(JoinHostTextBox->GetText().ToString());

		int32 ParsedPort = MenuDefaultGamePort;
		if (!bHostIncludesPort && !MenuTryParsePort(JoinPortTextBox->GetText().ToString(), ParsedPort))
		{
			StatusText = FText::FromString(TEXT("Port must be a number from 1 to 65535."));
			return FReply::Handled();
		}
	}

	const FString Address = GetEnteredAddress();
	const FString NormalizedAddress = FBHNetworkSupport::NormalizeJoinAddress(Address);
	if (NormalizedAddress.IsEmpty())
	{
		StatusText = FText::FromString(TEXT("Enter a valid LAN host/IP or LAN join code first. Examples: 192.168.1.23 port 7777, host.example.com port 7777, or a copied LAN join code."));
		return FReply::Handled();
	}

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->HideMainMenu();
		PC->JoinGame(NormalizedAddress);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnJoinSavedEndpointClicked(FString Endpoint)
{
	const FString NormalizedEndpoint = FBHNetworkSupport::NormalizeJoinAddress(Endpoint);
	SetJoinAddressFields(NormalizedEndpoint);
	if (NormalizedEndpoint.IsEmpty())
	{
		StatusText = FText::FromString(TEXT("Saved classroom endpoint is not a valid host:port address."));
		return FReply::Handled();
	}

	if (!EnsureJoinDisplayName())
	{
		return FReply::Handled();
	}

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->HideMainMenu();
		PC->JoinGame(NormalizedEndpoint);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnJoinLocalClicked()
{
	if (!EnsureJoinDisplayName())
	{
		return FReply::Handled();
	}

	if (AddressTextBox.IsValid())
	{
		AddressTextBox->SetText(FText::FromString(TEXT("127.0.0.1:7777")));
	}

	if (JoinHostTextBox.IsValid())
	{
		JoinHostTextBox->SetText(FText::FromString(TEXT("127.0.0.1")));
	}

	if (JoinPortTextBox.IsValid())
	{
		JoinPortTextBox->SetText(FText::FromString(TEXT("7777")));
	}

	return OnJoinClicked();
}

FReply SBHMainMenu::OnReadyClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ToggleReadyForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnSpectatorEncourageClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SendSpectatorEncouragementForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnSpectatorRolePreferenceClicked(EBHPlayerRole DesiredRole)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->QueueSpectatorRolePreferenceForMenu(DesiredRole, Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
	return FReply::Handled();
}

FReply SBHMainMenu::OnResumeClicked()
{
	// Clear any half-completed LEAVE confirmation so a stale prompt never carries into a later open.
	bLeaveConfirmPending = false;
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->HideMainMenu();
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnForceStartClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->HideMainMenu();
		PC->ForceStartRound();
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnNextFacilityClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetNextLevelForMenu(TEXT("Facility"), Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnNextSubstationClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetNextLevelForMenu(TEXT("Substation"), Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnNextFoggroundsClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetNextLevelForMenu(TEXT("Foggrounds"), Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnVoteFacilityClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->VoteMapForMenu(TEXT("Facility"), Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnVoteSubstationClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->VoteMapForMenu(TEXT("Substation"), Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnVoteFoggroundsClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->VoteMapForMenu(TEXT("Foggrounds"), Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnFogVoteClicked(EBHFogPreset FogPreset)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->VoteFogPresetForMenu(FogPreset, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnFogOverrideClicked(EBHFogPreset FogPreset)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetFogPresetOverrideForMenu(FogPreset, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnFogVoteModeClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->UseFogPresetVotesForMenu(Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnHunterCountClicked(int32 HunterCount)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetHunterCountForMenu(HunterCount, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnObjectiveIntensityClicked(int32 Intensity)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetObjectiveIntensityForMenu(Intensity, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnBotCountClicked(int32 BotCount)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetBotCountForMenu(BotCount, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnBotCountStepClicked(int32 Delta)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		int32 Current = 0;
		if (UWorld* World = PC->GetWorld())
		{
			if (ABHGameState* BHGS = World->GetGameState<ABHGameState>())
			{
				Current = BHGS->TargetBotCount;
			}
		}
		FString Message;
		PC->SetBotCountForMenu(FMath::Clamp(Current + Delta, 0, 11), Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnFillBotsClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->FillBotsForMenu(Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnBotDifficultyClicked(EBHBotDifficulty Difficulty)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetBotDifficultyForMenu(Difficulty, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnToggleInfectionClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ToggleInfectionModeForMenu(Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnTogglePaceClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->TogglePaceModeForMenu(Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnPracticeRoleClicked(EBHPlayerRole NewRole)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetPracticeRoleForMenu(NewRole, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnPracticeModifierClicked(EBHRoundModifier NewModifier)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetPracticeModifierForMenu(NewModifier, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnPracticeRefreshClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->RefreshPracticeRoundForMenu(Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnPracticeJumpscareClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->TriggerPracticeJumpscareForMenu(Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnJumpscareVariantClicked(FString VariantToken)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->TriggerJumpscareVariantForMenu(VariantToken, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnOpenAtmosphereConsoleClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		StatusText = FText::FromString(TEXT("Opening atmosphere tuning."));
		PC->ShowAtmosphereConsole();
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAtmosphereTestClicked(FString Command)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->RunAtmosphereTestForMenu(Command, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnTesterResourcesClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->TesterGrantTrainResources();
		StatusText = FText::FromString(TEXT("Tester shortcut sent: resources."));
	}
	return FReply::Handled();
}

FReply SBHMainMenu::OnTesterTrainClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		StatusText = FText::FromString(TEXT("Tester shortcut sent: train intermission."));
		PC->TesterOpenTrainIntermission();
	}
	return FReply::Handled();
}

FReply SBHMainMenu::OnTesterTrainPhaseClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		StatusText = FText::FromString(TEXT("Tester shortcut sent: train phase."));
		PC->TesterAdvanceTrainPhase();
	}
	return FReply::Handled();
}

FReply SBHMainMenu::OnTesterFinalStationClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		StatusText = FText::FromString(TEXT("Tester shortcut sent: final station."));
		PC->TesterLoadFinalStation();
	}
	return FReply::Handled();
}

FReply SBHMainMenu::OnTesterFinalEscapeClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->TesterTriggerFinalEscape();
		StatusText = FText::FromString(TEXT("Tester shortcut sent: final escape."));
	}
	return FReply::Handled();
}

FReply SBHMainMenu::OnTesterFinalRecapClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		StatusText = FText::FromString(TEXT("Tester shortcut sent: final recap."));
		PC->TesterForceFinalRecap();
	}
	return FReply::Handled();
}

FReply SBHMainMenu::OnTargetScareClicked(TWeakObjectPtr<APlayerState> TargetPlayerState)
{
	if (!TargetPlayerState.IsValid())
	{
		StatusText = FText::FromString(TEXT("That player already left."));
		return FReply::Handled();
	}

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->TriggerTargetedJumpscareForMenu(TargetPlayerState.Get(), Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAvatarClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->CycleAvatarForMenu(Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAvatarPresetClicked(int32 AvatarIndex)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetAvatarForMenu(AvatarIndex, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAvatarColorClicked(int32 ColorIndex)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetAvatarColorForMenu(ColorIndex, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAvatarHeadwearClicked(int32 HeadwearIndex)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetAvatarHeadwearForMenu(HeadwearIndex, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAvatarTitleClicked(int32 TitleIndex)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetTitleForMenu(TitleIndex, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAvatarEmblemClicked(int32 EmblemIndex)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetEmblemForMenu(EmblemIndex, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnGraphicsPresetClicked(int32 Quality)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ApplyGraphicsPresetForMenu(Quality, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAutoGraphicsClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ApplyAutoGraphicsForMenu(Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAdaptiveGraphicsClicked(bool bEnabled)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetAdaptiveGraphicsForMenu(bEnabled, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnAdaptiveFrameRateGoalClicked(int32 FpsGoal)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetAdaptiveFrameRateGoalForMenu(FpsGoal, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnRenderScaleClicked(int32 Percent)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ApplyRenderScaleForMenu(Percent, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnTextureQualityClicked(int32 Quality)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ApplyTextureQualityForMenu(Quality, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnShadowQualityClicked(int32 Quality)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ApplyShadowQualityForMenu(Quality, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnEffectsQualityClicked(int32 Quality)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ApplyEffectsQualityForMenu(Quality, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnResolutionClicked(int32 Width, int32 Height, bool bFullscreen)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ApplyResolutionForMenu(Width, Height, bFullscreen, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnFrameRateClicked(int32 FrameRateLimit)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ApplyFrameRateLimitForMenu(FrameRateLimit, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnComfortOptionClicked(FName OptionName, bool bEnabled)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetComfortOptionForMenu(OptionName, bEnabled, Message);
		StatusText = FText::FromString(Message);
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnRevisionTopicsClicked(int32 TopicMask)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetPhysicsTopicsForMenu(FString::FromInt(FMath::Clamp(TopicMask, 1, 15)), Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnRevisionDifficultyClicked(EBHRevisionDifficultyMix DifficultyMix)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetRevisionDifficultyMixForMenu(DifficultyMix, Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnRevisionThresholdClicked(int32 ClassPercent, int32 IndividualPercent)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetRevisionThresholdsForMenu(static_cast<float>(ClassPercent), static_cast<float>(IndividualPercent), Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnRevisionScareIntensityClicked(int32 Intensity)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetScareIntensityForMenu(Intensity, Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnLessonPresetClicked(FString PresetId)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		const bool bApplied = PC->ApplyLessonPresetForMenu(PresetId, Message);
		StatusText = FText::FromString(Message);
		if (bApplied)
		{
			FBHLessonPreset Preset;
			FString PresetMessage;
			PC->GetSelectedLessonPresetForMenu(Preset, PresetMessage);
			SelectedLiveClassroomMap = FBHLessonPresetStore::NormalizeMapName(Preset.MapName);
			RefreshLessonPresetList();
		}
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnSaveLessonPresetClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		const FString PresetName = LessonPresetNameTextBox.IsValid()
			? LessonPresetNameTextBox->GetText().ToString()
			: FString();
		FString Message;
		const bool bSaved = PC->SaveCurrentLessonPresetForMenu(PresetName, SelectedLiveClassroomMap, Message);
		StatusText = FText::FromString(Message);
		if (bSaved)
		{
			if (LessonPresetNameTextBox.IsValid())
			{
				LessonPresetNameTextBox->SetText(FText::GetEmpty());
			}
			RefreshLessonPresetList();
		}
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnGenerateManualQuestionSetClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Path;
		FString Message;
		PC->GenerateManualQuestionSetForMenu(12, Path, Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnForceReviewClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ForceReviewForMenu(Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnRevisionStatusClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->RevisionStatusForMenu(Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnExportRevisionReportClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ExportRevisionReportForMenu(Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnExportPlaytestTelemetryClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->ExportPlaytestTelemetryForMenu(Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

void SBHMainMenu::OnMasterVolumeChanged(float Volume)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->SetMasterVolumeForMenu(Volume);
	}
}

void SBHMainMenu::OnMusicVolumeChanged(float Volume)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->SetMusicVolumeForMenu(Volume);
	}
}

void SBHMainMenu::OnUiVolumeChanged(float Volume)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->SetUiVolumeForMenu(Volume);
	}
}

namespace
{
	// HUD slider clamp ranges, kept in sync with ABHPlayerController::SetHudScalarOptionForMenu.
	constexpr float BHHudScaleMin = 0.75f;
	constexpr float BHHudScaleMax = 1.5f;
	constexpr float BHHudPanelOpacityMin = 0.35f;
	constexpr float BHHudPanelOpacityMax = 1.0f;
}

float SBHMainMenu::GetHudScaleValue() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const float Scale = PC ? PC->GetHudScalarOptionForMenu(FName(TEXT("HudScale"))) : 1.0f;
	return FMath::Clamp((Scale - BHHudScaleMin) / (BHHudScaleMax - BHHudScaleMin), 0.0f, 1.0f);
}

FText SBHMainMenu::GetHudScaleText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const float Scale = PC ? PC->GetHudScalarOptionForMenu(FName(TEXT("HudScale"))) : 1.0f;
	return FText::FromString(FString::Printf(TEXT("HUD Size %d%%"), FMath::RoundToInt(Scale * 100.0f)));
}

void SBHMainMenu::OnHudScaleChanged(float SliderValue)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		const float Scale = FMath::Lerp(BHHudScaleMin, BHHudScaleMax, FMath::Clamp(SliderValue, 0.0f, 1.0f));
		FString Message;
		PC->SetHudScalarOptionForMenu(FName(TEXT("HudScale")), Scale, Message);
	}
}

float SBHMainMenu::GetHudPanelOpacityValue() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const float Opacity = PC ? PC->GetHudScalarOptionForMenu(FName(TEXT("HudPanelOpacity"))) : 1.0f;
	return FMath::Clamp((Opacity - BHHudPanelOpacityMin) / (BHHudPanelOpacityMax - BHHudPanelOpacityMin), 0.0f, 1.0f);
}

FText SBHMainMenu::GetHudPanelOpacityText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const float Opacity = PC ? PC->GetHudScalarOptionForMenu(FName(TEXT("HudPanelOpacity"))) : 1.0f;
	return FText::FromString(FString::Printf(TEXT("Panel Opacity %d%%"), FMath::RoundToInt(Opacity * 100.0f)));
}

void SBHMainMenu::OnHudPanelOpacityChanged(float SliderValue)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		const float Opacity = FMath::Lerp(BHHudPanelOpacityMin, BHHudPanelOpacityMax, FMath::Clamp(SliderValue, 0.0f, 1.0f));
		FString Message;
		PC->SetHudScalarOptionForMenu(FName(TEXT("HudPanelOpacity")), Opacity, Message);
	}
}

FReply SBHMainMenu::OnAssignRoleClicked(TWeakObjectPtr<APlayerState> TargetPlayerState, EBHPlayerRole DesiredRole)
{
	if (!TargetPlayerState.IsValid())
	{
		StatusText = FText::FromString(TEXT("That player already left."));
		return FReply::Handled();
	}

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->SetDesiredRole(TargetPlayerState.Get(), DesiredRole);
		StatusText = FText::FromString(TEXT("Role assignment sent. Reopen this menu to refresh the list."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnKickPlayerClicked(TWeakObjectPtr<APlayerState> TargetPlayerState)
{
	if (!TargetPlayerState.IsValid())
	{
		StatusText = FText::FromString(TEXT("That player already left."));
		return FReply::Handled();
	}

	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->KickPlayerForMenu(TargetPlayerState.Get(), Message);
		StatusText = FText::FromString(Message);
	}
	else
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnDisconnectClicked()
{
	ABHPlayerController* PC = PlayerController.Get();
	if (!PC)
	{
		return FReply::Handled();
	}

	// On a listen-server host, leaving tears down the session for every connected student, so require
	// a confirm step. Non-host clients (NM_Client, or Standalone after a bounce) keep one-click leave.
	const UWorld* World = PC->GetWorld();
	const bool bIsListenServerHost = World && World->GetNetMode() == NM_ListenServer;
	if (bIsListenServerHost && !bLeaveConfirmPending)
	{
		bLeaveConfirmPending = true;
		StatusText = FText::FromString(TEXT("Leaving ends the class session for everyone. Press LEAVE TO MAIN MENU again to confirm, or RESUME GAME to stay."));
		return FReply::Handled();
	}

	bLeaveConfirmPending = false;
	PC->ReturnToMainMenu();
	return FReply::Handled();
}

FReply SBHMainMenu::OnQuitClicked()
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->QuitGame();
	}

	return FReply::Handled();
}

void SBHMainMenu::OnAddressCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		OnJoinClicked();
	}
}

FText SBHMainMenu::GetStatusText() const
{
	// A pending host LEAVE confirmation must outrank the sticky network/account status messages
	// (set during hosting/sign-in and never cleared); otherwise the confirm prompt is masked and the
	// host, seeing no visible change, presses LEAVE again and tears down the whole class session.
	if (bLeaveConfirmPending)
	{
		return StatusText;
	}

	const ABHPlayerController* PC = PlayerController.Get();
	const UGameInstance* GameInstance = PC ? PC->GetGameInstance() : nullptr;
	const UBHAccountSubsystem* AccountSubsystem = GameInstance ? GameInstance->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (AccountSubsystem && !AccountSubsystem->GetLastAccountMessage().IsEmpty())
	{
		return FText::FromString(AccountSubsystem->GetLastAccountMessage());
	}

	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const UBHGameInstance* BHGI = World ? World->GetGameInstance<UBHGameInstance>() : nullptr;
	if (BHGI && !BHGI->GetLastNetworkMessage().IsEmpty())
	{
		return FText::FromString(BHGI->GetLastNetworkMessage());
	}

	return StatusText;
}

FText SBHMainMenu::GetAccountText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UGameInstance* GameInstance = PC ? PC->GetGameInstance() : nullptr;
	const UBHAccountSubsystem* AccountSubsystem = GameInstance ? GameInstance->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		return FText::FromString(TEXT("Unavailable"));
	}

	FString AccountText = AccountSubsystem->GetAccountSummary();
	if (AccountSubsystem->IsLoginPending())
	{
		AccountText += TEXT("\nLogin pending");
	}
	return FText::FromString(AccountText);
}

FText SBHMainMenu::GetLocalCredentialStatusText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UGameInstance* GameInstance = PC ? PC->GetGameInstance() : nullptr;
	const UBHAccountSubsystem* AccountSubsystem = GameInstance ? GameInstance->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		return FText::FromString(TEXT("Local credentials unavailable."));
	}

	return AccountSubsystem->HasLocalCredential()
		? FText::FromString(TEXT("Encrypted credential file found on this device."))
		: FText::FromString(TEXT("No encrypted local credentials saved yet."));
}

FText SBHMainMenu::GetSuggestedAddressText() const
{
	return FText::FromString(FString::Printf(TEXT("Suggested host address: %s"), *ResolvePreferredAddress()));
}

FText SBHMainMenu::GetDefaultJoinNameText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	if (!PC || !PC->HasUsefulLocalDisplayNameForMenu())
	{
		return FText::GetEmpty();
	}

	return FText::FromString(PC->GetLocalDisplayNameForMenu());
}

FText SBHMainMenu::GetReadyButtonText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	return FText::FromString(BHPS && BHPS->bReady ? TEXT("CANCEL READY") : TEXT("READY FOR ROUND"));
}

FText SBHMainMenu::GetPlayerIdentityText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return FText::FromString(TEXT("Avatar: unavailable until you join or host a match."));
	}

	const UEnum* RoleEnum = StaticEnum<EBHPlayerRole>();
	const FString RoleName = RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(BHPS->PlayerRole)) : FString(TEXT("Unassigned"));
	return FText::FromString(FString::Printf(TEXT("Avatar %d | Role %s | Color %.2f %.2f %.2f"),
		BHPS->AvatarIndex + 1,
		*RoleName,
		BHPS->AvatarColor.R,
		BHPS->AvatarColor.G,
		BHPS->AvatarColor.B));
}

FText SBHMainMenu::GetRoundDirectorText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS)
	{
		return FText::FromString(TEXT("Round variation: unavailable."));
	}

	FString BotText;
	if (BHGS->bBotMode)
	{
		const UEnum* BotDifficultyEnum = StaticEnum<EBHBotDifficulty>();
		const FString DifficultyName = BotDifficultyEnum ? BotDifficultyEnum->GetNameStringByValue(static_cast<int64>(BHGS->BotDifficulty)) : FString(TEXT("Normal"));
		BotText = FString::Printf(TEXT(" | Bot mode: %d %s"), BHGS->TargetBotCount, *DifficultyName);
	}

	return FText::FromString(FString::Printf(TEXT("Round seed %d | %s | %s | Next level: %s%s"),
		BHGS->RoundSeed,
		*BHGS->GetPublicObjectiveActionText(),
		*BHGS->GetObjectiveProgressText(),
		BHGS->NextLevelName.IsEmpty() ? TEXT("Facility") : *BHGS->NextLevelName,
		*BotText));
}

FText SBHMainMenu::GetOnlineSessionBrowserText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const UBHGameInstance* BHGI = World ? World->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		return FText::FromString(TEXT("Online subsystem: unavailable."));
	}

	const TArray<FBHOnlineSessionSummary>& Sessions = BHGI->GetOnlineSessionSummaries();
	const FString SubsystemName = BHGI->GetOnlineSubsystemName();
	FString BrowserText = FString::Printf(TEXT("Subsystem: %s"), *SubsystemName);
	if (SubsystemName.Equals(TEXT("Null"), ESearchCase::IgnoreCase))
	{
		BrowserText += TEXT("\nNull sessions are local/dev only. Use EOS or Steam for relay lobbies, or use the internet tunnel fallback.");
	}
	else if (SubsystemName.Equals(TEXT("EOS"), ESearchCase::IgnoreCase) || SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase))
	{
		BrowserText += TEXT("\nRelay-capable provider selected. Host Online and Find Online Sessions are the preferred internet path.");
	}

	if (BHGI->IsOnlineSessionBusy())
	{
		BrowserText += TEXT("\nOperation in progress...");
	}

	if (Sessions.IsEmpty())
	{
		BrowserText += TEXT("\nNo sessions listed. Use Find Online Sessions.");
		return FText::FromString(BrowserText);
	}

	for (int32 Index = 0; Index < Sessions.Num(); ++Index)
	{
		const FBHOnlineSessionSummary& Session = Sessions[Index];
		BrowserText += FString::Printf(
			TEXT("\n%d: %s | %s | %d/%d | %d ms"),
			Index,
			*Session.HostName,
			*Session.LevelName,
			Session.CurrentPlayers,
			Session.MaxPlayers,
			Session.PingMs);
	}

	return FText::FromString(BrowserText);
}

bool SBHMainMenu::IsCosmeticUnlockedForMenu(EBHCosmeticCategory Category, int32 Index) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UBHAccountSubsystem* AccountSubsystem = GameInstance ? GameInstance->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	return !AccountSubsystem || AccountSubsystem->IsCosmeticUnlocked(Category, Index);
}

FText SBHMainMenu::GetCosmeticProgressText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UBHAccountSubsystem* AccountSubsystem = GameInstance ? GameInstance->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	const int32 XP = AccountSubsystem ? AccountSubsystem->GetProgress().XP : 0;

	int32 BestRequiredXP = TNumericLimits<int32>::Max();
	const TCHAR* BestName = nullptr;
	const EBHCosmeticCategory Categories[] = {
		EBHCosmeticCategory::Outfit,
		EBHCosmeticCategory::Headwear
	};
	for (EBHCosmeticCategory Category : Categories)
	{
		for (int32 Index = 0; Index <= BHCosmeticMaxIndex(Category); ++Index)
		{
			const int32 RequiredXP = BHCosmeticRequiredXP(Category, Index);
			if (RequiredXP > XP && RequiredXP < BestRequiredXP)
			{
				BestRequiredXP = RequiredXP;
				BestName = BHCosmeticItemName(Category, Index);
			}
		}
	}

	if (BestName)
	{
		return FText::FromString(FString::Printf(TEXT("Local XP: %d | Next cosmetic: %s at %d XP"), XP, BestName, BestRequiredXP));
	}

	return FText::FromString(FString::Printf(TEXT("Local XP: %d | All cosmetics unlocked"), XP));
}

FText SBHMainMenu::GetCosmeticButtonText(EBHCosmeticCategory Category, int32 Index) const
{
	if (IsCosmeticUnlockedForMenu(Category, Index))
	{
		return FText::FromString(BHCosmeticItemName(Category, Index));
	}

	return FText::FromString(FString::Printf(
		TEXT("%s %dXP"),
		BHCosmeticItemName(Category, Index),
		BHCosmeticRequiredXP(Category, Index)));
}

FSlateColor SBHMainMenu::GetCosmeticButtonColor(EBHCosmeticCategory Category, int32 Index) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	bool bSelected = false;
	if (BHPS)
	{
		switch (Category)
		{
		case EBHCosmeticCategory::Outfit:
			bSelected = BHCosmeticClampIndex(Category, BHPS->AvatarIndex) == BHCosmeticClampIndex(Category, Index);
			break;
		case EBHCosmeticCategory::ShirtColor:
			bSelected = MenuNearestAvatarColorIndex(BHPS->AvatarColor) == BHCosmeticClampIndex(Category, Index);
			break;
		case EBHCosmeticCategory::Headwear:
			bSelected = BHCosmeticClampIndex(Category, BHPS->AvatarHeadwearIndex) == BHCosmeticClampIndex(Category, Index);
			break;
		case EBHCosmeticCategory::Title:
			bSelected = BHCosmeticClampIndex(Category, BHPS->SelectedTitleIndex) == BHCosmeticClampIndex(Category, Index);
			break;
		case EBHCosmeticCategory::Emblem:
			bSelected = BHCosmeticClampIndex(Category, BHPS->SelectedEmblemIndex) == BHCosmeticClampIndex(Category, Index);
			break;
		default:
			break;
		}
	}

	if (!IsCosmeticUnlockedForMenu(Category, Index))
	{
		return FSlateColor(FLinearColor(0.055f, 0.060f, 0.066f, 1.0f));
	}

	return FSlateColor(bSelected ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

FText SBHMainMenu::GetAvatarSummaryText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return FText::FromString(TEXT("Casual | Blue | None"));
	}

	FString RoleName = TEXT("Unassigned");
	if (BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		RoleName = TEXT("Teacher");
	}
	else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		RoleName = TEXT("Hall Monitor");
	}
	else if (BHPS->PlayerRole == EBHPlayerRole::Survivor)
	{
		RoleName = TEXT("Survivor");
	}
	else if (BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		RoleName = TEXT("Tester");
	}
	else if (BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		RoleName = TEXT("Spectator");
	}

	return FText::FromString(FString::Printf(TEXT("%s | %s | %s | %s"),
		MenuAvatarLookName(BHPS->AvatarIndex),
		BHCosmeticItemName(EBHCosmeticCategory::ShirtColor, MenuNearestAvatarColorIndex(BHPS->AvatarColor)),
		MenuAvatarHeadwearName(BHPS->AvatarHeadwearIndex),
		*RoleName));
}

FText SBHMainMenu::GetAvatarModelNameText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	const int32 ShirtIndex = BHPS ? MenuNearestAvatarColorIndex(BHPS->AvatarColor) + 1 : 1;
	return FText::FromString(FString::Printf(TEXT("%s | Shirt %d | %s"),
		MenuAvatarModelName(BHPS),
		ShirtIndex,
		MenuAvatarHeadwearName(BHPS ? BHPS->AvatarHeadwearIndex : 0)));
}

FSlateColor SBHMainMenu::GetAvatarShirtColor() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	FLinearColor ShirtColor = BHPS ? BHPS->AvatarColor : MenuAvatarPaletteColor(0);
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		ShirtColor = FLinearColor(0.36f, 0.045f, 0.035f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		ShirtColor = FLinearColor(0.45f, 0.19f, 0.06f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		ShirtColor = FLinearColor(0.08f, 0.42f, 0.48f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		ShirtColor = FLinearColor(0.14f, 0.14f, 0.16f, 1.0f);
	}
	return FSlateColor(ShirtColor);
}

FSlateColor SBHMainMenu::GetAvatarChestColor() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	FLinearColor ShirtColor = BHPS ? BHPS->AvatarColor : MenuAvatarPaletteColor(0);
	FLinearColor ChestColor = MenuAvatarColorScale(ShirtColor, 1.12f);
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		ChestColor = FLinearColor(0.62f, 0.10f, 0.08f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		ChestColor = FLinearColor(0.72f, 0.34f, 0.10f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		ChestColor = FLinearColor(0.12f, 0.72f, 0.78f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		ChestColor = FLinearColor(0.20f, 0.20f, 0.23f, 1.0f);
	}
	return FSlateColor(ChestColor);
}

FSlateColor SBHMainMenu::GetAvatarPantsColor() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	FLinearColor ShirtColor = BHPS ? BHPS->AvatarColor : MenuAvatarPaletteColor(0);
	FLinearColor PantsColor = MenuAvatarColorLerp(ShirtColor, FLinearColor(0.035f, 0.04f, 0.048f, 1.0f), 0.62f);
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		PantsColor = FLinearColor(0.025f, 0.025f, 0.030f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		PantsColor = FLinearColor(0.06f, 0.055f, 0.045f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		PantsColor = FLinearColor(0.025f, 0.075f, 0.09f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		PantsColor = FLinearColor(0.07f, 0.07f, 0.08f, 1.0f);
	}
	return FSlateColor(PantsColor);
}

FSlateColor SBHMainMenu::GetAvatarSkinColor() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	const int32 AvatarIndex = BHPS ? BHPS->AvatarIndex : 0;
	FLinearColor SkinColor = MenuAvatarSkinColor(AvatarIndex);
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		SkinColor = MenuAvatarColorLerp(SkinColor, FLinearColor(0.82f, 0.80f, 0.76f, 1.0f), 0.42f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		SkinColor = MenuAvatarColorLerp(SkinColor, FLinearColor(0.78f, 0.96f, 0.98f, 1.0f), 0.16f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		SkinColor = FLinearColor(0.28f, 0.28f, 0.30f, 1.0f);
	}
	return FSlateColor(SkinColor);
}

FSlateColor SBHMainMenu::GetAvatarHairColor() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	const int32 AvatarIndex = BHPS ? BHPS->AvatarIndex : 0;
	FLinearColor HairColor = MenuAvatarHairColor(AvatarIndex);
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		HairColor = FLinearColor(0.025f, 0.020f, 0.018f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		HairColor = FLinearColor(0.08f, 0.08f, 0.09f, 1.0f);
	}
	return FSlateColor(HairColor);
}

FSlateColor SBHMainMenu::GetAvatarBadgeColor() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	FLinearColor ShirtColor = BHPS ? BHPS->AvatarColor : MenuAvatarPaletteColor(0);
	FLinearColor BadgeColor = MenuAvatarColorLerp(ShirtColor, FLinearColor::White, 0.34f);
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		BadgeColor = FLinearColor(1.0f, 0.08f, 0.04f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		BadgeColor = FLinearColor(1.0f, 0.52f, 0.08f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		BadgeColor = FLinearColor(0.45f, 1.0f, 0.95f, 1.0f);
	}
	else if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		BadgeColor = FLinearColor(0.24f, 0.24f, 0.28f, 1.0f);
	}
	return FSlateColor(BadgeColor);
}

FSlateColor SBHMainMenu::GetAvatarPreviewAccentColor() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		return FSlateColor(FLinearColor(0.16f, 0.030f, 0.026f, 1.0f));
	}
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		return FSlateColor(FLinearColor(0.18f, 0.070f, 0.030f, 1.0f));
	}
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		return FSlateColor(FLinearColor(0.20f, 0.82f, 0.88f, 1.0f));
	}
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		return FSlateColor(FLinearColor(0.28f, 0.28f, 0.32f, 1.0f));
	}

	return FSlateColor(BHPS ? BHPS->AvatarColor : MenuAvatarPaletteColor(0));
}

bool SBHMainMenu::CanJoinFirstOnlineSession() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const UBHGameInstance* BHGI = World ? World->GetGameInstance<UBHGameInstance>() : nullptr;
	return BHGI && !BHGI->IsOnlineSessionBusy() && !BHGI->GetOnlineSessionSummaries().IsEmpty();
}

bool SBHMainMenu::IsInNetworkedGame() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	return World && World->GetNetMode() != NM_Standalone;
}

bool SBHMainMenu::IsPracticeMode() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	return BHGS && BHGS->bPracticeMode;
}

bool SBHMainMenu::IsTestMode() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	return BHGS && BHGS->bTestMode;
}

bool SBHMainMenu::CanEditRoles() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	return PC && World && PC->IsLocalController() && World->GetNetMode() == NM_ListenServer && BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby;
}

bool SBHMainMenu::CanUseClassroomHostControls() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	return PC && PC->CanUseClassroomHostControlsForMenu();
}

bool SBHMainMenu::CanManageLessonPresets() const
{
	return CanUseClassroomHostControls();
}

bool SBHMainMenu::CanEditRevisionControls() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	return PC && World && PC->IsLocalController() && World->GetNetMode() == NM_ListenServer && BHGS && BHGS->bRevisionMode;
}

bool SBHMainMenu::CanOpenClassroomBoard() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!PC || !World || !PC->IsLocalController())
	{
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	return NetMode == NM_ListenServer || NetMode == NM_Standalone;
}

bool SBHMainMenu::CanReadyFromMenu() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	return PC && PC->IsLocalController() && BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby && IsInNetworkedGame() && !IsPracticeMode() && !IsTestMode();
}

FString SBHMainMenu::GetEnteredAddress() const
{
	if (JoinHostTextBox.IsValid())
	{
		const FString HostText = JoinHostTextBox->GetText().ToString();
		if (MenuEntryHasExplicitPort(HostText) || MenuLooksLikeJoinInvite(HostText))
		{
			return FBHNetworkSupport::NormalizeJoinAddress(HostText);
		}
	}

	const FString Host = GetEnteredHost();
	if (!Host.IsEmpty())
	{
		return MenuBuildJoinAddress(Host, GetEnteredPort());
	}

	return NormalizeAddress(AddressTextBox.IsValid() ? AddressTextBox->GetText().ToString() : FString());
}

FString SBHMainMenu::GetEnteredHost() const
{
	if (JoinHostTextBox.IsValid())
	{
		return MenuExtractHost(JoinHostTextBox->GetText().ToString());
	}

	if (AddressTextBox.IsValid())
	{
		return MenuExtractHost(AddressTextBox->GetText().ToString());
	}

	return FString();
}

int32 SBHMainMenu::GetEnteredPort() const
{
	if (JoinHostTextBox.IsValid())
	{
		const FString HostText = JoinHostTextBox->GetText().ToString();
		if (MenuEntryHasExplicitPort(HostText))
		{
			return MenuExtractPort(HostText);
		}
	}

	if (JoinPortTextBox.IsValid())
	{
		const FString PortText = JoinPortTextBox->GetText().ToString().TrimStartAndEnd();
		int32 ParsedPort = MenuDefaultGamePort;
		if (MenuTryParsePort(PortText, ParsedPort))
		{
			return ParsedPort;
		}
	}

	if (AddressTextBox.IsValid())
	{
		return MenuExtractPort(AddressTextBox->GetText().ToString());
	}

	return MenuDefaultGamePort;
}

bool SBHMainMenu::EnsureJoinDisplayName()
{
	ABHPlayerController* PC = PlayerController.Get();
	if (!PC)
	{
		StatusText = FText::FromString(TEXT("No local player controller was available."));
		return false;
	}

	if (PC->HasUsefulLocalDisplayNameForMenu())
	{
		PC->PushLocalDisplayNameToServer();
		return true;
	}

	const FString CandidateName = JoinNameTextBox.IsValid() ? JoinNameTextBox->GetText().ToString() : FString();
	FString Message;
	if (!PC->SetLocalDisplayNameForMenu(CandidateName, Message))
	{
		StatusText = FText::FromString(TEXT("Enter your in-game name before joining so the host can see who you are."));
		if (JoinNameTextBox.IsValid())
		{
			FSlateApplication::Get().SetKeyboardFocus(JoinNameTextBox);
		}
		return false;
	}

	StatusText = FText::FromString(Message);
	return true;
}

void SBHMainMenu::SetJoinAddressFields(const FString& Address)
{
	const FString NormalizedAddress = FBHNetworkSupport::NormalizeJoinAddress(Address);
	if (NormalizedAddress.IsEmpty())
	{
		return;
	}

	SuggestedAddress = NormalizedAddress;
	if (AddressTextBox.IsValid())
	{
		AddressTextBox->SetText(FText::FromString(NormalizedAddress));
	}
	if (JoinHostTextBox.IsValid())
	{
		JoinHostTextBox->SetText(FText::FromString(MenuExtractHost(NormalizedAddress)));
	}
	if (JoinPortTextBox.IsValid())
	{
		JoinPortTextBox->SetText(MenuPortText(MenuExtractPort(NormalizedAddress)));
	}
}

FString SBHMainMenu::ResolvePreferredAddress() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const UBHGameInstance* BHGI = World ? World->GetGameInstance<UBHGameInstance>() : nullptr;
	if (BHGI)
	{
		return BHGI->GetPreferredJoinAddress(MenuDefaultGamePort);
	}

	return ResolveLocalAddress();
}

FText SBHMainMenu::GetLiveClassroomMapText() const
{
	const FString MapName = MenuNormalizeClassroomMapName(SelectedLiveClassroomMap);
	return FText::FromString(FString::Printf(TEXT("Selected map: %s"), *MapName));
}

FSlateColor SBHMainMenu::GetLiveClassroomMapButtonColor(FString LevelName) const
{
	const FString NormalizedLevel = MenuNormalizeClassroomMapName(LevelName);
	const FString SelectedLevel = MenuNormalizeClassroomMapName(SelectedLiveClassroomMap);
	return FSlateColor(NormalizedLevel.Equals(SelectedLevel, ESearchCase::CaseSensitive)
		? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f)
		: FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

EVisibility SBHMainMenu::GetClassroomHostOnlyVisibility() const
{
	return CanUseClassroomHostControls() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SBHMainMenu::GetClassroomPreflightStatusText() const
{
	if (const ABHPlayerController* PC = PlayerController.Get())
	{
		return FText::FromString(PC->GetClassroomPreflightSummaryForMenu().ReadyStatus);
	}

	return FText::FromString(TEXT("Preflight unavailable"));
}

FText SBHMainMenu::GetClassroomPreflightText() const
{
	if (const ABHPlayerController* PC = PlayerController.Get())
	{
		return FText::FromString(PC->GetClassroomPreflightSummaryForMenu().DetailText);
	}

	return FText::FromString(TEXT("No local player controller was available."));
}

FSlateColor SBHMainMenu::GetClassroomPreflightStatusColor() const
{
	if (const ABHPlayerController* PC = PlayerController.Get())
	{
		return FSlateColor(PC->GetClassroomPreflightSummaryForMenu().bReadyForClassroom
			? FLinearColor(0.46f, 0.86f, 0.62f, 1.0f)
			: FLinearColor(0.94f, 0.66f, 0.38f, 1.0f));
	}

	return FSlateColor(FLinearColor(0.86f, 0.58f, 0.34f, 1.0f));
}

FText SBHMainMenu::GetClassroomRunbookStepStatusText(EBHClassroomRunbookStep Step) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const UBHGameInstance* BHGI = World ? World->GetGameInstance<UBHGameInstance>() : nullptr;
	const FBHMenuRunbookRosterCounts RosterCounts = MenuBuildRunbookRosterCounts(BHGS);

	if (!CanUseClassroomHostControls())
	{
		return FText::FromString(TEXT("Host machine only"));
	}

	switch (Step)
	{
	case EBHClassroomRunbookStep::Preflight:
		if (PC)
		{
			const FBHClassroomPreflightSummary Summary = PC->GetClassroomPreflightSummaryForMenu();
			return FText::FromString(Summary.ReadyStatus.IsEmpty() ? FString(TEXT("Check preflight")) : Summary.ReadyStatus);
		}
		return FText::FromString(TEXT("Preflight unavailable"));
	case EBHClassroomRunbookStep::MapPreset:
	{
		FString MapName = MenuNormalizeClassroomMapName(SelectedLiveClassroomMap);
		if (BHGS && BHGS->bRevisionMode)
		{
			MapName = MenuNormalizeClassroomMapName(!BHGS->NextLevelName.IsEmpty() ? BHGS->NextLevelName : BHGS->ActiveLevelName);
		}

		FBHLessonPreset Preset;
		FString Message;
		if (PC && PC->GetSelectedLessonPresetForMenu(Preset, Message))
		{
			return FText::FromString(FString::Printf(TEXT("%s / %s"), *MapName, *Preset.DisplayName));
		}
		return FText::FromString(FString::Printf(TEXT("%s / choose preset"), *MapName));
	}
	case EBHClassroomRunbookStep::ManualQuestions:
		return FText::FromString(TEXT("Optional backup from selected preset"));
	case EBHClassroomRunbookStep::Tunnel:
	{
		const FString Address = BHGI ? BHGI->GetPreferredClassroomJoinAddress(MenuDefaultGamePort) : FString();
		return FText::FromString(Address.IsEmpty()
			? FString(TEXT("No join endpoint yet"))
			: FString::Printf(TEXT("Endpoint ready: %s"), *Address));
	}
	case EBHClassroomRunbookStep::Students:
		if (!BHGS)
		{
			return FText::FromString(TEXT("Host Live Classroom first"));
		}
		if (RosterCounts.HumanPlayers <= 1)
		{
			return FText::FromString(TEXT("Waiting for students"));
		}
		return FText::FromString(FString::Printf(TEXT("%d connected, %d ready"), RosterCounts.HumanPlayers, RosterCounts.HumanReady));
	case EBHClassroomRunbookStep::Roles:
		if (!BHGS)
		{
			return FText::FromString(TEXT("Host Live Classroom first"));
		}
		if (BHGS->RoundPhase != EBHRoundPhase::Lobby)
		{
			return FText::FromString(RosterCounts.ActiveTeachers > 0
				? FString::Printf(TEXT("%d Teacher active"), RosterCounts.ActiveTeachers)
				: FString(TEXT("Roles active")));
		}
		if (RosterCounts.HumanTeacherQueued > 0)
		{
			return FText::FromString(FString::Printf(TEXT("%d Teacher queued"), RosterCounts.HumanTeacherQueued));
		}
		if (RosterCounts.HumanQueuedRoles > 0)
		{
			return FText::FromString(FString::Printf(TEXT("%d roles queued"), RosterCounts.HumanQueuedRoles));
		}
		return FText::FromString(TEXT("Auto roles if unchanged"));
	case EBHClassroomRunbookStep::ReadyGate:
		if (!BHGS)
		{
			return FText::FromString(TEXT("Host Live Classroom first"));
		}
		if (BHGS->RoundPhase != EBHRoundPhase::Lobby)
		{
			return FText::FromString(FString::Printf(TEXT("Gate cleared: %s"), *MenuRunbookPhaseText(BHGS)));
		}
		return FText::FromString(FString::Printf(TEXT("%d/%d ready"), RosterCounts.HumanReady, RosterCounts.HumanPlayers));
	case EBHClassroomRunbookStep::Warmup:
		if (!BHGS)
		{
			return FText::FromString(TEXT("Starts after ready gate"));
		}
		if (BHGS->RoundPhase == EBHRoundPhase::Prep)
		{
			return FText::FromString(TEXT("Warmup active"));
		}
		return FText::FromString(MenuRunbookPhaseIsAfterSetup(BHGS->RoundPhase) ? FString(TEXT("Complete")) : FString(TEXT("Starts when all ready")));
	case EBHClassroomRunbookStep::Hunt:
		if (!BHGS)
		{
			return FText::FromString(TEXT("Not started"));
		}
		if (BHGS->RoundPhase == EBHRoundPhase::Prep)
		{
			return FText::FromString(TEXT("Ready to start Hunt"));
		}
		if (BHGS->RoundPhase == EBHRoundPhase::Hunt)
		{
			return FText::FromString(TEXT("Hunt live"));
		}
		if (BHGS->RoundPhase == EBHRoundPhase::FinalEscape)
		{
			return FText::FromString(TEXT("Final escape"));
		}
		if (MenuRunbookPhaseIsPostRound(BHGS->RoundPhase))
		{
			return FText::FromString(TEXT("Round complete"));
		}
		return FText::FromString(TEXT("Not started"));
	case EBHClassroomRunbookStep::Board:
		return FText::FromString(CanOpenClassroomBoard() ? TEXT("Projector view available") : TEXT("Host only"));
	case EBHClassroomRunbookStep::Export:
		if (!BHGS || !BHGS->bRevisionMode)
		{
			return FText::FromString(TEXT("Host Live Classroom first"));
		}
		if (MenuRunbookPhaseIsPostRound(BHGS->RoundPhase))
		{
			return FText::FromString(TEXT("Report export ready"));
		}
		if (MenuRunbookPhaseIsAtLeastHunt(BHGS->RoundPhase))
		{
			return FText::FromString(TEXT("Collecting round data"));
		}
		return FText::FromString(TEXT("Available after play"));
	default:
		return FText::FromString(TEXT("Pending"));
	}
}

FSlateColor SBHMainMenu::GetClassroomRunbookStepStatusColor(EBHClassroomRunbookStep Step) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const UBHGameInstance* BHGI = World ? World->GetGameInstance<UBHGameInstance>() : nullptr;
	const FBHMenuRunbookRosterCounts RosterCounts = MenuBuildRunbookRosterCounts(BHGS);

	if (!CanUseClassroomHostControls())
	{
		return FSlateColor(MenuRunbookStatusBlockedColor());
	}

	switch (Step)
	{
	case EBHClassroomRunbookStep::Preflight:
		return FSlateColor(PC && PC->GetClassroomPreflightSummaryForMenu().bReadyForClassroom
			? MenuRunbookStatusCompleteColor()
			: MenuRunbookStatusWaitingColor());
	case EBHClassroomRunbookStep::MapPreset:
	{
		FBHLessonPreset Preset;
		FString Message;
		return FSlateColor(PC && PC->GetSelectedLessonPresetForMenu(Preset, Message)
			? MenuRunbookStatusCompleteColor()
			: MenuRunbookStatusWaitingColor());
	}
	case EBHClassroomRunbookStep::ManualQuestions:
		return FSlateColor(MenuRunbookStatusNeutralColor());
	case EBHClassroomRunbookStep::Tunnel:
		return FSlateColor(BHGI && !BHGI->GetPreferredClassroomJoinAddress(MenuDefaultGamePort).IsEmpty()
			? MenuRunbookStatusCompleteColor()
			: MenuRunbookStatusWaitingColor());
	case EBHClassroomRunbookStep::Students:
		return FSlateColor(BHGS && RosterCounts.HumanPlayers > 1
			? MenuRunbookStatusCompleteColor()
			: MenuRunbookStatusWaitingColor());
	case EBHClassroomRunbookStep::Roles:
		if (!BHGS)
		{
			return FSlateColor(MenuRunbookStatusWaitingColor());
		}
		return FSlateColor(BHGS->RoundPhase != EBHRoundPhase::Lobby || RosterCounts.HumanQueuedRoles > 0
			? MenuRunbookStatusCompleteColor()
			: MenuRunbookStatusNeutralColor());
	case EBHClassroomRunbookStep::ReadyGate:
		if (!BHGS)
		{
			return FSlateColor(MenuRunbookStatusWaitingColor());
		}
		return FSlateColor(BHGS->RoundPhase != EBHRoundPhase::Lobby || (RosterCounts.HumanPlayers > 0 && RosterCounts.HumanReady == RosterCounts.HumanPlayers)
			? MenuRunbookStatusCompleteColor()
			: MenuRunbookStatusWaitingColor());
	case EBHClassroomRunbookStep::Warmup:
		return FSlateColor(BHGS && MenuRunbookPhaseIsAfterSetup(BHGS->RoundPhase)
			? MenuRunbookStatusCompleteColor()
			: MenuRunbookStatusWaitingColor());
	case EBHClassroomRunbookStep::Hunt:
		return FSlateColor(BHGS && MenuRunbookPhaseIsAtLeastHunt(BHGS->RoundPhase)
			? MenuRunbookStatusCompleteColor()
			: MenuRunbookStatusNeutralColor());
	case EBHClassroomRunbookStep::Board:
		return FSlateColor(CanOpenClassroomBoard() ? MenuRunbookStatusCompleteColor() : MenuRunbookStatusWaitingColor());
	case EBHClassroomRunbookStep::Export:
		if (CanRunbookExportReport())
		{
			return FSlateColor(MenuRunbookStatusCompleteColor());
		}
		return FSlateColor(BHGS && BHGS->bRevisionMode ? MenuRunbookStatusNeutralColor() : MenuRunbookStatusWaitingColor());
	default:
		return FSlateColor(MenuRunbookStatusNeutralColor());
	}
}

bool SBHMainMenu::CanRunbookStartHunt() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	return CanUseClassroomHostControls()
		&& PC
		&& PC->IsLocalController()
		&& World
		&& World->GetNetMode() == NM_ListenServer
		&& BHGS
		&& BHGS->RoundPhase == EBHRoundPhase::Prep;
}

bool SBHMainMenu::CanRunbookExportReport() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	return CanEditRevisionControls()
		&& PC
		&& PC->IsLocalController()
		&& World
		&& World->GetNetMode() == NM_ListenServer
		&& BHGS
		&& MenuRunbookPhaseIsPostRound(BHGS->RoundPhase);
}

EVisibility SBHMainMenu::GetRevisionControlsVisibility() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	return BHGS && BHGS->bRevisionMode && PC && PC->IsLocalController() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SBHMainMenu::GetRevisionControlsSummaryText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		return FText::FromString(TEXT("Host Live Classroom to enable question controls."));
	}

	return FText::FromString(FString::Printf(TEXT("Focus %s | Complexity %s | Targets %.0f/%.0f | Scare %s"),
		*MenuRevisionTopicMaskText(BHGS->RevisionTopicMask),
		*MenuRevisionDifficultyMixText(BHGS->RevisionDifficultyMix),
		BHGS->RevisionClassThreshold,
		BHGS->RevisionIndividualThreshold,
		*MenuRevisionScareIntensityText(BHGS->RevisionScareIntensity)));
}

FSlateColor SBHMainMenu::GetRevisionControlsSummaryColor() const
{
	return FSlateColor(CanEditRevisionControls()
		? FLinearColor(0.62f, 0.74f, 0.72f, 1.0f)
		: FLinearColor(0.86f, 0.62f, 0.40f, 1.0f));
}

FSlateColor SBHMainMenu::GetRevisionTopicButtonColor(int32 TopicMask) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const int32 AllTopicsMask = MenuAllRevisionTopicMask();
	const int32 CurrentTopicMask = BHGS ? BHGS->RevisionTopicMask : AllTopicsMask;
	const bool bAllSelected = (CurrentTopicMask & AllTopicsMask) == AllTopicsMask;
	const bool bSelected = TopicMask == AllTopicsMask ? bAllSelected : ((CurrentTopicMask & TopicMask) != 0 && !bAllSelected);
	return FSlateColor(bSelected ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

FSlateColor SBHMainMenu::GetRevisionDifficultyButtonColor(EBHRevisionDifficultyMix DifficultyMix) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const EBHRevisionDifficultyMix CurrentDifficultyMix = BHGS ? BHGS->RevisionDifficultyMix : EBHRevisionDifficultyMix::Adaptive;
	return FSlateColor(CurrentDifficultyMix == DifficultyMix ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

FSlateColor SBHMainMenu::GetRevisionThresholdButtonColor(int32 ClassPercent, int32 IndividualPercent) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const int32 CurrentClassThreshold = FMath::RoundToInt(BHGS ? BHGS->RevisionClassThreshold : 70.0f);
	const int32 CurrentIndividualThreshold = FMath::RoundToInt(BHGS ? BHGS->RevisionIndividualThreshold : 50.0f);
	const bool bSelected = CurrentClassThreshold == ClassPercent && CurrentIndividualThreshold == IndividualPercent;
	return FSlateColor(bSelected ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

FSlateColor SBHMainMenu::GetRevisionScareButtonColor(int32 Intensity) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const int32 CurrentScareIntensity = BHGS ? BHGS->RevisionScareIntensity : 2;
	return FSlateColor(CurrentScareIntensity == Intensity ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

EVisibility SBHMainMenu::GetLessonPresetPanelVisibility() const
{
	return CanManageLessonPresets() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SBHMainMenu::GetLessonPresetSummaryText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	if (!PC)
	{
		return FText::FromString(TEXT("Lesson presets unavailable."));
	}

	FBHLessonPreset Preset;
	FString Message;
	const bool bFoundPreset = PC->GetSelectedLessonPresetForMenu(Preset, Message);
	FString Summary = FBHLessonPresetStore::DescribePreset(Preset);
	if (!bFoundPreset && !Message.IsEmpty())
	{
		Summary += FString::Printf(TEXT(" (%s)"), *Message);
	}
	return FText::FromString(Summary);
}

FSlateColor SBHMainMenu::GetLessonPresetSummaryColor() const
{
	return FSlateColor(CanManageLessonPresets()
		? FLinearColor(0.62f, 0.74f, 0.72f, 1.0f)
		: FLinearColor(0.86f, 0.62f, 0.40f, 1.0f));
}

FSlateColor SBHMainMenu::GetLessonPresetButtonColor(FString PresetId) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const FString SelectedId = PC ? PC->GetSelectedLessonPresetIdForMenu() : FString();
	return FSlateColor(SelectedId.Equals(PresetId, ESearchCase::IgnoreCase)
		? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f)
		: FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

FSlateColor SBHMainMenu::GetGraphicsBoolOptionButtonColor(FName OptionName, bool bValue) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	bool bSelected = false;
	if (PC)
	{
		if (OptionName == TEXT("Auto"))
		{
			bSelected = PC->IsAutoGraphicsEnabledForMenu() == bValue;
		}
		else if (OptionName == TEXT("Adaptive"))
		{
			bSelected = PC->IsAdaptiveGraphicsEnabledForMenu() == bValue;
		}
	}

	return FSlateColor(bSelected ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

FSlateColor SBHMainMenu::GetComfortBoolOptionButtonColor(FName OptionName, bool bValue) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const bool bSelected = PC && PC->IsComfortOptionEnabledForMenu(OptionName) == bValue;
	return FSlateColor(bSelected ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

FSlateColor SBHMainMenu::GetGraphicsOptionButtonColor(FName OptionName, int32 Value) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	bool bSelected = false;
	if (PC)
	{
		if (OptionName == TEXT("Preset"))
		{
			bSelected = !PC->IsAutoGraphicsEnabledForMenu() && PC->GetGraphicsPresetQualityForMenu() == Value;
		}
		else if (OptionName == TEXT("AdaptiveFps"))
		{
			bSelected = PC->GetGraphicsAdaptiveFpsGoalForMenu() == Value;
		}
		else if (OptionName == TEXT("RenderScale"))
		{
			bSelected = PC->GetGraphicsRenderScalePercentForMenu() == Value;
		}
		else if (OptionName == TEXT("FrameLimit"))
		{
			bSelected = PC->GetGraphicsFrameRateLimitForMenu() == Value;
		}
		else if (OptionName == TEXT("Texture"))
		{
			bSelected = PC->GetGraphicsTextureQualityForMenu() == Value;
		}
		else if (OptionName == TEXT("Shadow"))
		{
			bSelected = PC->GetGraphicsShadowQualityForMenu() == Value;
		}
		else if (OptionName == TEXT("Effects"))
		{
			bSelected = PC->GetGraphicsEffectsQualityForMenu() == Value;
		}
	}

	return FSlateColor(bSelected ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

FSlateColor SBHMainMenu::GetGraphicsResolutionButtonColor(int32 Width, int32 Height, bool bFullscreen) const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const bool bSelected = PC && PC->IsGraphicsResolutionSelectedForMenu(Width, Height, bFullscreen);
	return FSlateColor(bSelected ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f));
}

void SBHMainMenu::EnsureAvatarPreviewScene()
{
	ABHPlayerController* PC = PlayerController.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	if (!AvatarPreviewRenderTarget.IsValid())
	{
		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
		RenderTarget->ClearColor = FLinearColor(0.014f, 0.018f, 0.022f, 1.0f);
		RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		RenderTarget->InitAutoFormat(512, 768);
		RenderTarget->UpdateResourceImmediate(true);

		AvatarPreviewRenderTarget.Reset(RenderTarget);
		AvatarPreviewBrush = FSlateBrush();
		AvatarPreviewBrush.DrawAs = ESlateBrushDrawType::Image;
		AvatarPreviewBrush.ImageSize = FVector2D(154.0f, 208.0f);
		AvatarPreviewBrush.SetResourceObject(RenderTarget);
	}

	if (AvatarPreviewActor.IsValid())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("BHMenuAvatarPreview"));

	AActor* PreviewActor = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		FVector(0.0f, 0.0f, 50000.0f),
		FRotator::ZeroRotator,
		SpawnParams);
	if (!PreviewActor)
	{
		return;
	}

	PreviewActor->SetActorEnableCollision(false);
	PreviewActor->SetCanBeDamaged(false);
	PreviewActor->SetActorTickEnabled(false);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BasicMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	USceneComponent* RootComponent = NewObject<USceneComponent>(PreviewActor, TEXT("PreviewRoot"), RF_Transient);
	RootComponent->Mobility = EComponentMobility::Movable;
	PreviewActor->AddInstanceComponent(RootComponent);
	PreviewActor->SetRootComponent(RootComponent);
	RootComponent->RegisterComponent();

	USkeletalMeshComponent* MeshComponent = NewObject<USkeletalMeshComponent>(PreviewActor, TEXT("PreviewMesh"), RF_Transient);
	MeshComponent->Mobility = EComponentMobility::Movable;
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCastShadow(false);
	MeshComponent->SetRelativeLocation(FVector::ZeroVector);
	MeshComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	MeshComponent->SetRelativeScale3D(FVector(MenuAvatarPreviewModelScale));
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	MeshComponent->bReceivesDecals = false;
	PreviewActor->AddInstanceComponent(MeshComponent);
	MeshComponent->RegisterComponent();

	auto CreatePreviewPart = [&](const TCHAR* Name)
	{
		UStaticMeshComponent* PartComponent = NewObject<UStaticMeshComponent>(PreviewActor, FName(Name), RF_Transient);
		PartComponent->Mobility = EComponentMobility::Movable;
		PartComponent->SetupAttachment(RootComponent);
		PartComponent->SetStaticMesh(CubeMesh);
		PartComponent->SetMaterial(0, BasicMaterial);
		PartComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartComponent->SetGenerateOverlapEvents(false);
		PartComponent->SetCastShadow(false);
		PartComponent->SetHiddenInGame(true);
		PreviewActor->AddInstanceComponent(PartComponent);
		PartComponent->RegisterComponent();
		return PartComponent;
	};

	UStaticMeshComponent* HeadwearComponent = CreatePreviewPart(TEXT("PreviewHeadwear"));
	UStaticMeshComponent* HeadwearAccentComponent = CreatePreviewPart(TEXT("PreviewHeadwearAccent"));
	UStaticMeshComponent* HeadwearDetailComponent = CreatePreviewPart(TEXT("PreviewHeadwearDetail"));

	UPointLightComponent* LightComponent = NewObject<UPointLightComponent>(PreviewActor, TEXT("PreviewLight"), RF_Transient);
	LightComponent->Mobility = EComponentMobility::Movable;
	LightComponent->SetupAttachment(RootComponent);
	LightComponent->SetRelativeLocation(FVector(-140.0f, -90.0f, 170.0f));
	LightComponent->SetIntensity(6500.0f);
	LightComponent->SetAttenuationRadius(650.0f);
	PreviewActor->AddInstanceComponent(LightComponent);
	LightComponent->RegisterComponent();

	USceneCaptureComponent2D* CaptureComponent = NewObject<USceneCaptureComponent2D>(PreviewActor, TEXT("PreviewCapture"), RF_Transient);
	CaptureComponent->Mobility = EComponentMobility::Movable;
	CaptureComponent->SetupAttachment(RootComponent);
	CaptureComponent->TextureTarget = AvatarPreviewRenderTarget.Get();
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->FOVAngle = MenuAvatarPreviewCameraFov;
	CaptureComponent->PostProcessBlendWeight = 0.0f;
	CaptureComponent->bMainViewFamily = false;
	CaptureComponent->bMainViewResolution = false;
	CaptureComponent->bMainViewCamera = false;
	CaptureComponent->bInheritMainViewCameraPostProcessSettings = false;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->SetRelativeLocation(FVector(-MenuAvatarPreviewCameraDistance, 0.0f, MenuAvatarPreviewCameraHeight));
	CaptureComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	PreviewActor->AddInstanceComponent(CaptureComponent);
	CaptureComponent->RegisterComponent();
	CaptureComponent->ClearShowOnlyComponents();
	CaptureComponent->ShowOnlyActorComponents(PreviewActor);

	AvatarPreviewActor = PreviewActor;
	AvatarPreviewMeshComponent = MeshComponent;
	AvatarPreviewHeadwearComponent = HeadwearComponent;
	AvatarPreviewHeadwearAccentComponent = HeadwearAccentComponent;
	AvatarPreviewHeadwearDetailComponent = HeadwearDetailComponent;
	AvatarPreviewCaptureComponent = CaptureComponent;
	LastAvatarPreviewMeshPath.Reset();
	LastAvatarPreviewColor = FLinearColor::Transparent;
	LastAvatarPreviewHeadwearIndex = INDEX_NONE;
	CaptureComponent->CaptureScene();
}

void SBHMainMenu::UpdateAvatarPreviewMesh()
{
	EnsureAvatarPreviewScene();

	USkeletalMeshComponent* MeshComponent = AvatarPreviewMeshComponent.Get();
	USceneCaptureComponent2D* CaptureComponent = AvatarPreviewCaptureComponent.Get();
	if (!MeshComponent || !CaptureComponent)
	{
		return;
	}

	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	const FString MeshPath = MenuAvatarMeshPath(BHPS);
	bool bPreviewMeshChanged = false;
	if (LastAvatarPreviewMeshPath != MeshPath)
	{
		if (USkeletalMesh* PreviewMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath))
		{
			MeshComponent->SetSkeletalMesh(PreviewMesh);
			MeshComponent->EmptyOverrideMaterials();
			MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (UAnimSequence* IdleAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Idle.A_BH_Q_Idle")))
			{
				MeshComponent->PlayAnimation(IdleAnimation, true);
			}
			LastAvatarPreviewMeshPath = MeshPath;
			LastAvatarPreviewColor = FLinearColor::Transparent;
			bPreviewMeshChanged = true;
		}
	}

	const FLinearColor PreviewColor = BHPS ? BHPS->AvatarColor : MenuAvatarPaletteColor(0);
	const bool bPreviewColorChanged = bPreviewMeshChanged || !LastAvatarPreviewColor.Equals(PreviewColor);
	if (bPreviewColorChanged)
	{
		if (!bPreviewMeshChanged)
		{
			MeshComponent->EmptyOverrideMaterials();
		}
		MenuApplyQuaterniusPalette(MeshComponent, PreviewColor);
		LastAvatarPreviewColor = PreviewColor;
	}

	UStaticMeshComponent* HeadwearComponent = AvatarPreviewHeadwearComponent.Get();
	UStaticMeshComponent* HeadwearAccentComponent = AvatarPreviewHeadwearAccentComponent.Get();
	UStaticMeshComponent* HeadwearDetailComponent = AvatarPreviewHeadwearDetailComponent.Get();
	const int32 HeadwearIndex = BHPS ? BHCosmeticClampIndex(EBHCosmeticCategory::Headwear, BHPS->AvatarHeadwearIndex) : 0;
	const bool bAccessoryChanged = HeadwearIndex != LastAvatarPreviewHeadwearIndex
		|| bPreviewColorChanged;

	if (bAccessoryChanged)
	{
		UStaticMesh* AccessoryCube = MenuPreviewCubeMesh();
		UStaticMesh* AccessoryCylinder = MenuPreviewCylinderMesh();
		UStaticMesh* AccessorySphere = MenuPreviewSphereMesh();
		UMaterialInterface* AccessoryBasic = MenuPreviewBasicMaterial();
		UMaterialInterface* AccessoryBlack = MenuPreviewMaterial(TEXT("Black"));
		UMaterialInterface* AccessoryDarkBrown = MenuPreviewMaterial(TEXT("DarkBrown"));
		UMaterialInterface* AccessoryMetal = MenuPreviewMaterial(TEXT("Metal"));
		UMaterialInterface* AccessoryLight = MenuPreviewMaterial(TEXT("LightBlue"));
		UMaterialInterface* AccessoryWhite = MenuPreviewMaterial(TEXT("White"));
		UMaterialInterface* AccessoryColor = MenuQuaterniusPaletteMaterial(PreviewColor);
		if (!AccessoryColor)
		{
			AccessoryColor = AccessoryLight;
		}

		UStaticMeshComponent* HeadwearParts[] = {
			HeadwearComponent,
			HeadwearAccentComponent,
			HeadwearDetailComponent
		};
		for (UStaticMeshComponent* Part : HeadwearParts)
		{
			MenuApplyPreviewPiece(Part, AccessoryCube, AccessoryBasic, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, false);
		}

		switch (HeadwearIndex)
		{
		case 1:
			MenuApplyPreviewPiece(HeadwearComponent, AccessoryCylinder, AccessoryColor, FVector(7.0f, 0.0f, 74.0f), FRotator::ZeroRotator, FVector(0.30f, 0.30f, 0.075f), true);
			MenuApplyPreviewPiece(HeadwearAccentComponent, AccessoryCube, AccessoryColor, FVector(23.0f, 0.0f, 68.0f), FRotator::ZeroRotator, FVector(0.12f, 0.28f, 0.025f), true);
			MenuApplyPreviewPiece(HeadwearDetailComponent, AccessoryCube, AccessoryWhite, FVector(6.0f, 0.0f, 78.0f), FRotator::ZeroRotator, FVector(0.055f, 0.055f, 0.018f), true);
			break;
		case 2:
			MenuApplyPreviewPiece(HeadwearComponent, AccessoryCube, AccessoryBlack, FVector(23.0f, -8.0f, 60.0f), FRotator::ZeroRotator, FVector(0.025f, 0.070f, 0.035f), true);
			MenuApplyPreviewPiece(HeadwearAccentComponent, AccessoryCube, AccessoryBlack, FVector(23.0f, 8.0f, 60.0f), FRotator::ZeroRotator, FVector(0.025f, 0.070f, 0.035f), true);
			MenuApplyPreviewPiece(HeadwearDetailComponent, AccessoryCube, AccessoryMetal, FVector(24.0f, 0.0f, 60.0f), FRotator::ZeroRotator, FVector(0.018f, 0.050f, 0.014f), true);
			break;
		case 3:
			MenuApplyPreviewPiece(HeadwearComponent, AccessoryCylinder, AccessoryColor, FVector(6.0f, 0.0f, 77.0f), FRotator::ZeroRotator, FVector(0.32f, 0.32f, 0.095f), true);
			MenuApplyPreviewPiece(HeadwearAccentComponent, AccessoryCube, AccessoryDarkBrown, FVector(13.0f, 0.0f, 68.0f), FRotator::ZeroRotator, FVector(0.050f, 0.32f, 0.035f), true);
			MenuApplyPreviewPiece(HeadwearDetailComponent, AccessorySphere, AccessoryColor, FVector(3.0f, 0.0f, 85.0f), FRotator::ZeroRotator, FVector(0.055f, 0.055f, 0.055f), true);
			break;
		case 4:
			MenuApplyPreviewPiece(HeadwearComponent, AccessoryCube, AccessoryBlack, FVector(11.0f, 0.0f, 67.0f), FRotator::ZeroRotator, FVector(0.045f, 0.40f, 0.080f), true);
			MenuApplyPreviewPiece(HeadwearAccentComponent, AccessoryCube, AccessoryLight, FVector(23.0f, 0.0f, 64.0f), FRotator::ZeroRotator, FVector(0.055f, 0.42f, 0.115f), true);
			MenuApplyPreviewPiece(HeadwearDetailComponent, AccessoryCube, AccessoryMetal, FVector(24.0f, 0.0f, 57.0f), FRotator::ZeroRotator, FVector(0.018f, 0.36f, 0.012f), true);
			break;
		default:
			break;
		}
	}

	LastAvatarPreviewHeadwearIndex = HeadwearIndex;
	CaptureComponent->CaptureScene();
}

void SBHMainMenu::DestroyAvatarPreviewScene()
{
	if (AActor* PreviewActor = AvatarPreviewActor.Get())
	{
		PreviewActor->Destroy();
	}

	AvatarPreviewActor.Reset();
	AvatarPreviewMeshComponent.Reset();
	AvatarPreviewHeadwearComponent.Reset();
	AvatarPreviewHeadwearAccentComponent.Reset();
	AvatarPreviewHeadwearDetailComponent.Reset();
	AvatarPreviewCaptureComponent.Reset();
	AvatarPreviewRenderTarget.Reset();
	AvatarPreviewBrush.SetResourceObject(nullptr);
	LastAvatarPreviewMeshPath.Reset();
	LastAvatarPreviewColor = FLinearColor::Transparent;
	LastAvatarPreviewHeadwearIndex = INDEX_NONE;
}

TSharedRef<SWidget> SBHMainMenu::BuildAccountPanel()
{
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.038f, 0.046f, 0.052f, 0.95f))
		.Padding(12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
				.Text(FText::FromString(TEXT("Account")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 7.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.67f, 0.75f, 0.76f, 1.0f))
				.Text(this, &SBHMainMenu::GetAccountText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnGuestAccountClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Guest")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnGoogleLoginClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Google")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnMicrosoftLoginClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Microsoft")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnSyncAccountClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Sync")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBHMenuButton)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnSignOutAccountClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Sign Out")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 12.0f)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildLocalCredentialPanel(false)
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildLocalCredentialPanel(bool bForStartScreen)
{
	TSharedPtr<SEditableTextBox>& UsernameBox = bForStartScreen ? StartLocalUsernameTextBox : LocalUsernameTextBox;
	TSharedPtr<SEditableTextBox>& PasswordBox = bForStartScreen ? StartLocalPasswordTextBox : LocalPasswordTextBox;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(MenuFont(bForStartScreen ? 14 : 13, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.90f, 0.83f, 0.74f, 1.0f))
			.Text(FText::FromString(TEXT("Local Credentials")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 10.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(MenuFont(10))
			.ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.72f, 1.0f))
			.Text(this, &SBHMainMenu::GetLocalCredentialStatusText)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.64f, 0.70f, 0.70f, 1.0f))
				.Text(FText::FromString(TEXT("USERNAME")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SAssignNew(UsernameBox, SEditableTextBox)
				.HintText(FText::FromString(TEXT("local player name")))
				.SelectAllTextWhenFocused(true)
				.MaximumLength(32)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.64f, 0.70f, 0.70f, 1.0f))
				.Text(FText::FromString(TEXT("PASSWORD")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SAssignNew(PasswordBox, SEditableTextBox)
				.HintText(FText::FromString(TEXT("8+ characters")))
				.IsPassword(true)
				.SelectAllTextWhenFocused(true)
				.MaximumLength(128)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.ButtonColorAndOpacity(FLinearColor(0.16f, 0.30f, 0.24f, 1.0f))
				.ContentPadding(FMargin(8.0f, 5.0f))
				.OnClicked(this, &SBHMainMenu::OnLoginLocalCredentialClicked, bForStartScreen)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.Text(FText::FromString(TEXT("Login")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.ButtonColorAndOpacity(FLinearColor(0.24f, 0.20f, 0.13f, 1.0f))
				.ContentPadding(FMargin(8.0f, 5.0f))
				.OnClicked(this, &SBHMainMenu::OnCreateLocalCredentialClicked, bForStartScreen)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.Text(FText::FromString(TEXT("Save")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.ButtonColorAndOpacity(FLinearColor(0.20f, 0.08f, 0.08f, 1.0f))
				.ContentPadding(FMargin(8.0f, 5.0f))
				.OnClicked(this, &SBHMainMenu::OnForgetLocalCredentialClicked)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.Text(FText::FromString(TEXT("Forget")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBHMenuButton)
				.ButtonColorAndOpacity(FLinearColor(0.20f, 0.08f, 0.08f, 1.0f))
				.ContentPadding(FMargin(8.0f, 5.0f))
				.OnClicked(this, &SBHMainMenu::OnResetLocalClassroomDataClicked)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.Text(FText::FromString(TEXT("Reset")))
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildNetworkPanel()
{
	const bool bInGame = IsInNetworkedGame();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SBorder)
			.Visibility(!bInGame ? EVisibility::Visible : EVisibility::Collapsed)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.036f, 0.044f, 0.052f, 0.95f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
					.Text(this, &SBHMainMenu::GetSuggestedAddressText)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(11))
					.ColorAndOpacity(FLinearColor(0.58f, 0.66f, 0.67f, 1.0f))
					.Text(FText::FromString(TEXT("Best path: online sessions through EOS or Steam relay. Fallback: start an internet tunnel and share its allocation address. Direct IP still works when UDP 7777 reaches the host.")))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SBorder)
			.Visibility(!bInGame ? EVisibility::Visible : EVisibility::Collapsed)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.040f, 0.049f, 0.058f, 0.95f))
			.Padding(14.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(15, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
					.Text(FText::FromString(TEXT("Online Session Browser")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(12))
					.ColorAndOpacity(FLinearColor(0.66f, 0.74f, 0.75f, 1.0f))
					.Text(this, &SBHMainMenu::GetOnlineSessionBrowserText)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildStatusPanel()
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildStatusPanel()
{
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.08f, 0.09f, 0.10f, 0.95f))
		.Padding(14.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(MenuFont(13))
			.ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.82f, 1.0f))
			.Text(this, &SBHMainMenu::GetStatusText)
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildAvatarPreview()
{
	EnsureAvatarPreviewScene();
	UpdateAvatarPreviewMesh();

	return SNew(SBox)
		.WidthOverride(190.0f)
		.HeightOverride(272.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.018f, 0.022f, 0.026f, 1.0f))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(this, &SBHMainMenu::GetAvatarPreviewAccentColor)
					.Padding(FMargin(8.0f, 5.0f))
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.98f, 1.0f, 0.96f, 1.0f))
						.Text(this, &SBHMainMenu::GetAvatarModelNameText)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(FLinearColor(0.010f, 0.013f, 0.016f, 1.0f))
					.Padding(0.0f)
					[
						SNew(SImage)
						.Image(&AvatarPreviewBrush)
					]
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildCharacterCustomizationPanel()
{
	TSharedRef<SGridPanel> LookButtons = SNew(SGridPanel);
	for (int32 Index = 0; Index <= BHCosmeticMaxIndex(EBHCosmeticCategory::Outfit); ++Index)
	{
		LookButtons->AddSlot(Index % 2, Index / 2)
			.Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				SNew(SBox)
				.WidthOverride(118.0f)
				.HeightOverride(30.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(this, &SBHMainMenu::IsCosmeticUnlockedForMenu, EBHCosmeticCategory::Outfit, Index)
					.ButtonColorAndOpacity(this, &SBHMainMenu::GetCosmeticButtonColor, EBHCosmeticCategory::Outfit, Index)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnAvatarPresetClicked, Index)
					[
						SNew(STextBlock)
						.Font(MenuFont(8, FName(TEXT("Bold"))))
						.Text(this, &SBHMainMenu::GetCosmeticButtonText, EBHCosmeticCategory::Outfit, Index)
					]
				]
			];
	}

	TSharedRef<SHorizontalBox> ColorButtons = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FLinearColor Color = MenuAvatarPaletteColor(Index);
		ColorButtons->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 5.0f, 0.0f)
			[
					SNew(SBHMenuButton)
					.ButtonColorAndOpacity(Color)
					.ContentPadding(FMargin(4.0f, 3.0f))
					.OnClicked(this, &SBHMainMenu::OnAvatarColorClicked, Index)
				[
					SNew(SBox)
					.WidthOverride(18.0f)
					.HeightOverride(14.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(Color)
					]
				]
			];
	}

	TSharedRef<SGridPanel> HeadwearButtons = SNew(SGridPanel);
	for (int32 Index = 0; Index <= BHCosmeticMaxIndex(EBHCosmeticCategory::Headwear); ++Index)
	{
		HeadwearButtons->AddSlot(Index % 2, Index / 2)
			.Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				SNew(SBox)
				.WidthOverride(118.0f)
				.HeightOverride(30.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(this, &SBHMainMenu::IsCosmeticUnlockedForMenu, EBHCosmeticCategory::Headwear, Index)
					.ButtonColorAndOpacity(this, &SBHMainMenu::GetCosmeticButtonColor, EBHCosmeticCategory::Headwear, Index)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnAvatarHeadwearClicked, Index)
					[
						SNew(STextBlock)
						.Font(MenuFont(8, FName(TEXT("Bold"))))
						.Text(this, &SBHMainMenu::GetCosmeticButtonText, EBHCosmeticCategory::Headwear, Index)
					]
				]
			];
	}

	TSharedRef<SVerticalBox> TitleButtons = SNew(SVerticalBox);
	for (int32 Index = 0; Index <= BHCosmeticMaxIndex(EBHCosmeticCategory::Title); ++Index)
	{
		TitleButtons->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SBox)
				.HeightOverride(28.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(this, &SBHMainMenu::IsCosmeticUnlockedForMenu, EBHCosmeticCategory::Title, Index)
					.ButtonColorAndOpacity(this, &SBHMainMenu::GetCosmeticButtonColor, EBHCosmeticCategory::Title, Index)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnAvatarTitleClicked, Index)
					[
						SNew(STextBlock)
						.Font(MenuFont(8, FName(TEXT("Bold"))))
						.Text(this, &SBHMainMenu::GetCosmeticButtonText, EBHCosmeticCategory::Title, Index)
					]
				]
			];
	}

	TSharedRef<SGridPanel> EmblemButtons = SNew(SGridPanel);
	for (int32 Index = 0; Index <= BHCosmeticMaxIndex(EBHCosmeticCategory::Emblem); ++Index)
	{
		EmblemButtons->AddSlot(Index % 2, Index / 2)
			.Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				SNew(SBox)
				.WidthOverride(118.0f)
				.HeightOverride(30.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(this, &SBHMainMenu::IsCosmeticUnlockedForMenu, EBHCosmeticCategory::Emblem, Index)
					.ButtonColorAndOpacity(this, &SBHMainMenu::GetCosmeticButtonColor, EBHCosmeticCategory::Emblem, Index)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnAvatarEmblemClicked, Index)
					[
						SNew(STextBlock)
						.Font(MenuFont(8, FName(TEXT("Bold"))))
						.Text(this, &SBHMainMenu::GetCosmeticButtonText, EBHCosmeticCategory::Emblem, Index)
					]
				]
			];
	}

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.034f, 0.034f, 0.044f, 0.95f))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.88f, 0.86f, 0.98f, 1.0f))
				.Text(FText::FromString(TEXT("Character")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.80f, 1.0f))
				.Text(this, &SBHMainMenu::GetAvatarSummaryText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(9))
				.ColorAndOpacity(FLinearColor(0.62f, 0.74f, 0.72f, 1.0f))
				.Text(this, &SBHMainMenu::GetCosmeticProgressText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 9.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(11, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.82f, 0.90f, 0.88f, 1.0f))
				.Text(this, &SBHMainMenu::GetAvatarModelNameText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					BuildAvatarPreview()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.78f, 0.84f, 0.88f, 1.0f))
						.Text(FText::FromString(TEXT("Outfit")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 5.0f, 0.0f, 12.0f)
					[
						LookButtons
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.78f, 0.84f, 0.88f, 1.0f))
						.Text(FText::FromString(TEXT("Shirt")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						ColorButtons
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.78f, 0.84f, 0.88f, 1.0f))
						.Text(FText::FromString(TEXT("Headwear")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 5.0f, 0.0f, 6.0f)
					[
						HeadwearButtons
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.86f, 0.80f, 0.60f, 1.0f))
						.Text(FText::FromString(TEXT("Title (shown on your nameplate)")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 5.0f, 0.0f, 6.0f)
					[
						TitleButtons
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.86f, 0.80f, 0.60f, 1.0f))
						.Text(FText::FromString(TEXT("Emblem (nameplate badge)")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 5.0f, 0.0f, 6.0f)
					[
						EmblemButtons
					]
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildRoundOptionsPanel()
{
	ABHPlayerController* PC = PlayerController.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const bool bCanVote = IsInNetworkedGame();
	const bool bCanEditHost = CanEditRoles() || IsPracticeMode();
	const FString ModifierName = BHGS ? BHGS->GetRoundModifierText() : FString(TEXT("None"));
	const FString ModifierHint = BHGS ? BHGS->GetRoundModifierHint() : FString(TEXT("Standard map rules."));
	const UEnum* BotDifficultyEnum = StaticEnum<EBHBotDifficulty>();
	const FString BotDifficultyName = BHGS && BotDifficultyEnum
		? BotDifficultyEnum->GetNameStringByValue(static_cast<int64>(BHGS->BotDifficulty))
		: FString(TEXT("Normal"));
	const UEnum* FogPresetEnum = StaticEnum<EBHFogPreset>();
	const FString FogPresetName = BHGS && FogPresetEnum
		? FogPresetEnum->GetNameStringByValue(static_cast<int64>(BHGS->NextFogPreset))
		: FString(TEXT("Heavy"));
	const FString OptionsText = BHGS
		? FString::Printf(TEXT("Teachers %d | Objectives %d | Bots %d %s | Infection %s | Pace %s | Fog %s %s | Modifier %s | Side tasks %d/%d | %s"),
			BHGS->TargetHunterCount,
			BHGS->ObjectiveIntensity,
			BHGS->TargetBotCount,
			*BotDifficultyName,
			BHGS->bInfectionMode ? TEXT("On") : TEXT("Off"),
			BHGS->bPartyPace ? TEXT("Party") : TEXT("Slow"),
			*FogPresetName,
			BHGS->bFogPresetOverride ? TEXT("(Host)") : TEXT("(Vote)"),
			*ModifierName,
			BHGS->SideObjectivesCompleted,
			BHGS->SideObjectivesRequired,
			*BHGS->GetPublicObjectiveActionText())
		: FString(TEXT("Round options are available after hosting or joining."));

	if (!bCanEditHost)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.035f, 0.043f, 0.050f, 0.95f))
			.Padding(10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
					.Text(FText::FromString(TEXT("Round Status")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 5.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(FLinearColor(0.66f, 0.74f, 0.75f, 1.0f))
					.Text(FText::FromString(OptionsText))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(9))
					.ColorAndOpacity(FLinearColor(0.58f, 0.68f, 0.68f, 0.92f))
					.Text(FText::FromString(FString::Printf(TEXT("Modifier effect: %s"), *ModifierHint)))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SBHMenuButton)
						.IsEnabled(bCanVote)
						.ContentPadding(FMargin(7.0f, 4.0f))
						.OnClicked(this, &SBHMainMenu::OnVoteFacilityClicked)
						[
							SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Facility")))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SBHMenuButton)
						.IsEnabled(bCanVote)
						.ContentPadding(FMargin(7.0f, 4.0f))
						.OnClicked(this, &SBHMainMenu::OnVoteSubstationClicked)
						[
							SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Substation")))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBHMenuButton)
						.IsEnabled(bCanVote)
						.ContentPadding(FMargin(7.0f, 4.0f))
						.OnClicked(this, &SBHMainMenu::OnVoteFoggroundsClicked)
						[
							SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Foggrounds")))
						]
					]
				]
			];
	}

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.035f, 0.043f, 0.050f, 0.95f))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
				.Text(FText::FromString(TEXT("Round Options")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.66f, 0.74f, 0.75f, 1.0f))
				.Text(FText::FromString(OptionsText))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(9))
				.ColorAndOpacity(FLinearColor(0.58f, 0.68f, 0.68f, 0.92f))
				.Text(FText::FromString(FString::Printf(TEXT("Modifier effect: %s"), *ModifierHint)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(bCanVote)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnVoteFacilityClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Facility")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(bCanVote)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnVoteSubstationClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Substation")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(bCanVote)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnVoteFoggroundsClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Foggrounds")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(bCanEditHost)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnToggleInfectionClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Infection")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBHMenuButton)
					.IsEnabled(bCanEditHost)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnTogglePaceClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(10, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("Pace")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.72f, 0.80f, 0.80f, 1.0f))
					.Text(FText::FromString(TEXT("Fog Vote")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanVote).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogVoteClicked, EBHFogPreset::Light)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Light")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanVote).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogVoteClicked, EBHFogPreset::Heavy)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Heavy")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanVote).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogVoteClicked, EBHFogPreset::Extreme)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Extreme")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.72f, 0.80f, 0.80f, 1.0f))
					.Text(FText::FromString(TEXT("Host Fog")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogOverrideClicked, EBHFogPreset::Light)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Light")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogOverrideClicked, EBHFogPreset::Heavy)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Heavy")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogOverrideClicked, EBHFogPreset::Extreme)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Extreme")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogVoteModeClicked)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Votes")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.72f, 0.80f, 0.80f, 1.0f))
					.Text(FText::FromString(TEXT("Teachers")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnHunterCountClicked, 1)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("1")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnHunterCountClicked, 2)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("2")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnHunterCountClicked, 3)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("3")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.72f, 0.80f, 0.80f, 1.0f))
					.Text(FText::FromString(TEXT("Objectives")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnObjectiveIntensityClicked, 0)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("0")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnObjectiveIntensityClicked, 1)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("1")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnObjectiveIntensityClicked, 2)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("2")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnObjectiveIntensityClicked, 3)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("3")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.72f, 0.80f, 0.80f, 1.0f))
					.Text_Lambda([this]() -> FText
					{
						int32 Count = 0;
						if (ABHPlayerController* StepPC = PlayerController.Get())
						{
							if (UWorld* StepWorld = StepPC->GetWorld())
							{
								if (ABHGameState* StepGS = StepWorld->GetGameState<ABHGameState>())
								{
									Count = StepGS->TargetBotCount;
								}
							}
						}
						return FText::FromString(FString::Printf(TEXT("Bots %d/11"), Count));
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(9.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotCountStepClicked, -1)
					[
						SNew(STextBlock).Font(MenuFont(11, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("-")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(9.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotCountStepClicked, 1)
					[
						SNew(STextBlock).Font(MenuFont(11, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("+")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFillBotsClicked)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Fill")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotCountClicked, 0)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Clear")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotDifficultyClicked, EBHBotDifficulty::Easy)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Easy")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotDifficultyClicked, EBHBotDifficulty::Normal)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Normal")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBHMenuButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotDifficultyClicked, EBHBotDifficulty::Hard)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Hard")))
					]
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildLessonPresetPanel()
{
	TSharedRef<SVerticalBox> PresetList = SAssignNew(LessonPresetListBox, SVerticalBox);
	RefreshLessonPresetList();

	const ABHPlayerController* PC = PlayerController.Get();
	const FString StoragePath = PC ? PC->GetLessonPresetStoragePathForMenu() : FBHLessonPresetStore::GetStoragePath();

	return SNew(SBox)
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBHMainMenu::GetLessonPresetPanelVisibility)))
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.032f, 0.044f, 0.050f, 0.95f))
			.Padding(10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.84f, 0.94f, 0.90f, 1.0f))
					.Text(FText::FromString(TEXT("Lesson Presets")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 5.0f, 0.0f, 9.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetLessonPresetSummaryColor)))
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetLessonPresetSummaryText)))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					PresetList
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 7.0f, 0.0f)
					[
						SAssignNew(LessonPresetNameTextBox, SEditableTextBox)
						.MinDesiredWidth(220.0f)
						.HintText(FText::FromString(TEXT("Preset name")))
						.SelectAllTextWhenFocused(true)
						.Font(MenuFont(10))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBHMenuButton)
						.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanManageLessonPresets)))
						.ButtonColorAndOpacity(FLinearColor(0.16f, 0.38f, 0.34f, 1.0f))
						.ContentPadding(FMargin(9.0f, 5.0f))
						.OnClicked(this, &SBHMainMenu::OnSaveLessonPresetClicked)
						[
							SNew(STextBlock)
							.Font(MenuFont(10, FName(TEXT("Bold"))))
							.Text(FText::FromString(TEXT("Save Current")))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SBHMenuButton)
						.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanManageLessonPresets)))
						.ButtonColorAndOpacity(FLinearColor(0.14f, 0.27f, 0.40f, 1.0f))
						.ContentPadding(FMargin(9.0f, 5.0f))
						.OnClicked(this, &SBHMainMenu::OnGenerateManualQuestionSetClicked)
						[
							SNew(STextBlock)
							.Font(MenuFont(10, FName(TEXT("Bold"))))
							.Text(FText::FromString(TEXT("Generate 12Q")))
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(8))
					.ColorAndOpacity(FLinearColor(0.48f, 0.58f, 0.58f, 1.0f))
					.Text(FText::FromString(FString::Printf(TEXT("Local file: %s"), *StoragePath)))
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildRevisionControlsPanel()
{
	const int32 ForcesBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::ForcesAndMotion);
	const int32 ElectricityBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Electricity);
	const int32 WavesBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Waves);
	const int32 EnergyBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Energy);
	const int32 AllTopicsMask = MenuAllRevisionTopicMask();

	auto AddOptionButton = [this](const TSharedRef<SHorizontalBox>& Row, const FString& Label, const TAttribute<FSlateColor>& ButtonColor, const FOnClicked& OnClicked)
	{
		Row->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 5.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanEditRevisionControls)))
				.ButtonColorAndOpacity(ButtonColor)
				.ContentPadding(FMargin(7.0f, 4.0f))
				.OnClicked(OnClicked)
				[
					SNew(STextBlock)
					.Font(MenuFont(9, FName(TEXT("Bold"))))
					.Text(FText::FromString(Label))
				]
			];
	};

	TSharedRef<SHorizontalBox> TopicRow = SNew(SHorizontalBox);
	AddOptionButton(TopicRow, TEXT("All"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionTopicButtonColor, AllTopicsMask)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, AllTopicsMask));
	AddOptionButton(TopicRow, TEXT("Forces"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionTopicButtonColor, ForcesBit)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, ForcesBit));
	AddOptionButton(TopicRow, TEXT("Electricity"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionTopicButtonColor, ElectricityBit)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, ElectricityBit));
	AddOptionButton(TopicRow, TEXT("Waves"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionTopicButtonColor, WavesBit)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, WavesBit));
	AddOptionButton(TopicRow, TEXT("Energy"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionTopicButtonColor, EnergyBit)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, EnergyBit));

	TSharedRef<SHorizontalBox> DifficultyRow = SNew(SHorizontalBox);
	AddOptionButton(DifficultyRow, TEXT("Easy"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionDifficultyButtonColor, EBHRevisionDifficultyMix::Easy)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionDifficultyClicked, EBHRevisionDifficultyMix::Easy));
	AddOptionButton(DifficultyRow, TEXT("Balanced"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionDifficultyButtonColor, EBHRevisionDifficultyMix::Balanced)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionDifficultyClicked, EBHRevisionDifficultyMix::Balanced));
	AddOptionButton(DifficultyRow, TEXT("Adaptive"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionDifficultyButtonColor, EBHRevisionDifficultyMix::Adaptive)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionDifficultyClicked, EBHRevisionDifficultyMix::Adaptive));
	AddOptionButton(DifficultyRow, TEXT("Hard"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionDifficultyButtonColor, EBHRevisionDifficultyMix::Hard)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionDifficultyClicked, EBHRevisionDifficultyMix::Hard));

	TSharedRef<SHorizontalBox> ThresholdRow = SNew(SHorizontalBox);
	AddOptionButton(ThresholdRow, TEXT("Support 60/40"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionThresholdButtonColor, 60, 40)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionThresholdClicked, 60, 40));
	AddOptionButton(ThresholdRow, TEXT("Default 70/50"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionThresholdButtonColor, 70, 50)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionThresholdClicked, 70, 50));
	AddOptionButton(ThresholdRow, TEXT("Stretch 80/60"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionThresholdButtonColor, 80, 60)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionThresholdClicked, 80, 60));

	TSharedRef<SHorizontalBox> ScareRow = SNew(SHorizontalBox);
	AddOptionButton(ScareRow, TEXT("Off"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionScareButtonColor, 0)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionScareIntensityClicked, 0));
	AddOptionButton(ScareRow, TEXT("Low"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionScareButtonColor, 1)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionScareIntensityClicked, 1));
	AddOptionButton(ScareRow, TEXT("Horror"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionScareButtonColor, 2)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionScareIntensityClicked, 2));
	AddOptionButton(ScareRow, TEXT("Chaos"), TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionScareButtonColor, 3)), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionScareIntensityClicked, 3));

	TSharedRef<SHorizontalBox> AdminRow = SNew(SHorizontalBox);
	AddOptionButton(AdminRow, TEXT("Show Status"), TAttribute<FSlateColor>(FSlateColor(FLinearColor(0.12f, 0.15f, 0.17f, 1.0f))), FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionStatusClicked));
	AddOptionButton(AdminRow, TEXT("Force 60s Review"), TAttribute<FSlateColor>(FSlateColor(FLinearColor(0.12f, 0.15f, 0.17f, 1.0f))), FOnClicked::CreateSP(this, &SBHMainMenu::OnForceReviewClicked));
	AddOptionButton(AdminRow, TEXT("Export Report"), TAttribute<FSlateColor>(FSlateColor(FLinearColor(0.12f, 0.15f, 0.17f, 1.0f))), FOnClicked::CreateSP(this, &SBHMainMenu::OnExportRevisionReportClicked));
	AddOptionButton(AdminRow, TEXT("Export Heatmap"), TAttribute<FSlateColor>(FSlateColor(FLinearColor(0.12f, 0.15f, 0.17f, 1.0f))), FOnClicked::CreateSP(this, &SBHMainMenu::OnExportPlaytestTelemetryClicked));

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.032f, 0.044f, 0.050f, 0.95f))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.94f, 0.90f, 1.0f))
				.Text(FText::FromString(TEXT("Question Controls")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionControlsSummaryColor)))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionControlsSummaryText)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 7.0f)
			[
				SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.80f, 1.0f)).Text(FText::FromString(TEXT("Focus")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 9.0f)
			[
				TopicRow
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 7.0f)
			[
				SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.80f, 1.0f)).Text(FText::FromString(TEXT("Complexity")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 9.0f)
			[
				DifficultyRow
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 7.0f)
			[
				SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.80f, 1.0f)).Text(FText::FromString(TEXT("Targets")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 9.0f)
			[
				ThresholdRow
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 7.0f)
			[
				SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.80f, 1.0f)).Text(FText::FromString(TEXT("Scare")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 9.0f)
			[
				ScareRow
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				AdminRow
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildClassroomPreflightPanel()
{
	TSharedRef<SVerticalBox> ActionRows = SNew(SVerticalBox);
	TSharedRef<SHorizontalBox> PrimaryActionRow = SNew(SHorizontalBox);
	TSharedRef<SHorizontalBox> FileActionRow = SNew(SHorizontalBox);
	auto AddPreflightAction = [this](const TSharedRef<SHorizontalBox>& Row, const FString& Label, const FLinearColor& Color, const FOnClicked& OnClicked)
	{
		Row->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.IsEnabled(this, &SBHMainMenu::CanUseClassroomHostControls)
				.ButtonColorAndOpacity(Color)
				.ContentPadding(FMargin(8.0f, 5.0f))
				.OnClicked(OnClicked)
				[
					SNew(STextBlock)
					.Font(MenuFont(9, FName(TEXT("Bold"))))
					.Text(FText::FromString(Label))
				]
			];
	};

	AddPreflightAction(PrimaryActionRow, TEXT("Refresh"), FLinearColor(0.14f, 0.30f, 0.34f, 1.0f), FOnClicked::CreateSP(this, &SBHMainMenu::OnRefreshClassroomPreflightClicked));
	AddPreflightAction(PrimaryActionRow, TEXT("Copy Join Code"), FLinearColor(0.16f, 0.36f, 0.32f, 1.0f), FOnClicked::CreateSP(this, &SBHMainMenu::OnCopyClassroomJoinCodeClicked));
	AddPreflightAction(PrimaryActionRow, TEXT("Start Playit"), FLinearColor(0.26f, 0.31f, 0.18f, 1.0f), FOnClicked::CreateSP(this, &SBHMainMenu::OnStartInternetTunnelClicked));
	AddPreflightAction(FileActionRow, TEXT("Open Logs"), FLinearColor(0.14f, 0.18f, 0.24f, 1.0f), FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenClassroomLogFolderClicked));
	AddPreflightAction(FileActionRow, TEXT("Open Package"), FLinearColor(0.14f, 0.18f, 0.24f, 1.0f), FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenClassroomPackageFolderClicked));
	AddPreflightAction(FileActionRow, TEXT("Support Folder"), FLinearColor(0.14f, 0.18f, 0.24f, 1.0f), FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenClassroomSupportFolderClicked));
	AddPreflightAction(FileActionRow, TEXT("Support Bundle"), FLinearColor(0.18f, 0.26f, 0.34f, 1.0f), FOnClicked::CreateSP(this, &SBHMainMenu::OnCreateClassroomSupportBundleClicked));
	AddPreflightAction(FileActionRow, TEXT("Deployment Guide"), FLinearColor(0.20f, 0.24f, 0.30f, 1.0f), FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenClassroomDeploymentGuideClicked));

	ActionRows->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			PrimaryActionRow
		];
	ActionRows->AddSlot()
		.AutoHeight()
		[
			FileActionRow
		];

	return SNew(SBox)
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBHMainMenu::GetClassroomHostOnlyVisibility)))
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.026f, 0.038f, 0.044f, 0.96f))
			.Padding(10.0f)
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
						SNew(STextBlock)
						.Font(MenuFont(13, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.84f, 0.94f, 0.90f, 1.0f))
						.Text(FText::FromString(TEXT("Host Preflight")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(MenuFont(11, FName(TEXT("Bold"))))
						.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetClassroomPreflightStatusColor)))
						.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetClassroomPreflightStatusText)))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 8.0f)
				[
					ActionRows
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.MaxDesiredHeight(190.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Font(MenuFont(9))
							.ColorAndOpacity(FLinearColor(0.66f, 0.76f, 0.75f, 1.0f))
							.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetClassroomPreflightText)))
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildClassroomRunbookStep(int32 StepNumber, EBHClassroomRunbookStep Step, const FText& Title, const FText& ActionLabel, const FOnClicked& OnClicked, TAttribute<bool> ActionEnabled)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 9.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(28.0f)
			.HeightOverride(28.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.11f, 0.20f, 0.21f, 1.0f))
				.Padding(FMargin(4.0f, 3.0f))
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.Font(MenuFont(9, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.88f, 0.96f, 0.92f, 1.0f))
					.Text(FText::FromString(FString::Printf(TEXT("%02d"), FMath::Max(1, StepNumber))))
				]
			]
		];

	Row->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.94f, 0.90f, 1.0f))
				.Text(Title)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(9))
				.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetClassroomRunbookStepStatusColor, Step)))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetClassroomRunbookStepStatusText, Step)))
			]
		];

	if (!ActionLabel.IsEmpty())
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.IsEnabled(ActionEnabled)
				.ButtonColorAndOpacity(FLinearColor(0.14f, 0.29f, 0.29f, 1.0f))
				.ContentPadding(FMargin(8.0f, 5.0f))
				.OnClicked(OnClicked)
				[
					SNew(STextBlock)
					.Font(MenuFont(9, FName(TEXT("Bold"))))
					.Text(ActionLabel)
				]
			];
	}

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.026f, 0.036f, 0.040f, 0.96f))
		.Padding(FMargin(8.0f, 7.0f))
		[
			Row
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildClassroomRunbookPanel()
{
	TSharedRef<SVerticalBox> StepList = SNew(SVerticalBox);
	const TAttribute<bool> HostControlsEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanUseClassroomHostControls));
	const TAttribute<bool> LessonControlsEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanManageLessonPresets));
	const TAttribute<bool> BoardEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanOpenClassroomBoard));
	const TAttribute<bool> StartHuntEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanRunbookStartHunt));
	const TAttribute<bool> ExportEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SBHMainMenu::CanRunbookExportReport));

	auto AddStep = [this, &StepList](int32 StepNumber, EBHClassroomRunbookStep Step, const TCHAR* Title, const TCHAR* ActionLabel, const FOnClicked& OnClicked, TAttribute<bool> ActionEnabled)
	{
		StepList->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				BuildClassroomRunbookStep(
					StepNumber,
					Step,
					FText::FromString(FString(Title)),
					ActionLabel && FCString::Strlen(ActionLabel) > 0 ? FText::FromString(FString(ActionLabel)) : FText::GetEmpty(),
					OnClicked,
					ActionEnabled)
			];
	};

	AddStep(1, EBHClassroomRunbookStep::Preflight, TEXT("Check Host Preflight"), TEXT("Refresh"), FOnClicked::CreateSP(this, &SBHMainMenu::OnRefreshClassroomPreflightClicked), HostControlsEnabled);
	AddStep(2, EBHClassroomRunbookStep::MapPreset, TEXT("Choose map and lesson"), TEXT("Host Live"), FOnClicked::CreateSP(this, &SBHMainMenu::OnHostSelectedLiveClassroomClicked), HostControlsEnabled);
	AddStep(3, EBHClassroomRunbookStep::ManualQuestions, TEXT("Prepare backup questions"), TEXT("Generate 12Q"), FOnClicked::CreateSP(this, &SBHMainMenu::OnGenerateManualQuestionSetClicked), LessonControlsEnabled);
	AddStep(4, EBHClassroomRunbookStep::Tunnel, TEXT("Verify tunnel or join code"), TEXT("Copy Code"), FOnClicked::CreateSP(this, &SBHMainMenu::OnCopyClassroomJoinCodeClicked), HostControlsEnabled);
	AddStep(5, EBHClassroomRunbookStep::Students, TEXT("Confirm students joined"), TEXT(""), FOnClicked(), TAttribute<bool>(false));
	AddStep(6, EBHClassroomRunbookStep::Roles, TEXT("Assign or approve roles"), TEXT(""), FOnClicked(), TAttribute<bool>(false));
	AddStep(7, EBHClassroomRunbookStep::ReadyGate, TEXT("Confirm ready gate"), TEXT(""), FOnClicked(), TAttribute<bool>(false));
	AddStep(8, EBHClassroomRunbookStep::Warmup, TEXT("Run role warmup"), TEXT("Start Hunt"), FOnClicked::CreateSP(this, &SBHMainMenu::OnForceStartClicked), StartHuntEnabled);
	AddStep(9, EBHClassroomRunbookStep::Hunt, TEXT("Run the live Hunt"), TEXT("Open Board"), FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenClassroomBoardClicked), BoardEnabled);
	AddStep(10, EBHClassroomRunbookStep::Board, TEXT("Projector board"), TEXT("Open Board"), FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenClassroomBoardClicked), BoardEnabled);
	AddStep(11, EBHClassroomRunbookStep::Export, TEXT("Post-round export"), TEXT("Export Report"), FOnClicked::CreateSP(this, &SBHMainMenu::OnExportRevisionReportClicked), ExportEnabled);

	return SNew(SBox)
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBHMainMenu::GetClassroomHostOnlyVisibility)))
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.028f, 0.044f, 0.046f, 0.96f))
			.Padding(10.0f)
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
						SNew(STextBlock)
						.Font(MenuFont(13, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.84f, 0.94f, 0.90f, 1.0f))
						.Text(FText::FromString(TEXT("Run Class")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(MenuFont(9, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.58f, 0.72f, 0.70f, 1.0f))
						.Text(FText::FromString(TEXT("Host checklist")))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 7.0f, 0.0f, 0.0f)
				[
					StepList
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildClassroomPanel()
{
	const bool bShowLobbyRoster = IsInNetworkedGame() && !IsPracticeMode() && !IsTestMode();

	TSharedRef<SHorizontalBox> LiveMapRow = SNew(SHorizontalBox);
	auto AddLiveMapButton = [this](const TSharedRef<SHorizontalBox>& Row, const FString& LevelName)
	{
		Row->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.IsEnabled(this, &SBHMainMenu::CanUseClassroomHostControls)
				.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetLiveClassroomMapButtonColor, LevelName)))
				.ContentPadding(FMargin(8.0f, 5.0f))
				.OnClicked(this, &SBHMainMenu::OnSelectLiveClassroomMapClicked, LevelName)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.Text(FText::FromString(LevelName))
				]
			];
	};
	AddLiveMapButton(LiveMapRow, TEXT("Facility"));
	AddLiveMapButton(LiveMapRow, TEXT("Substation"));
	AddLiveMapButton(LiveMapRow, TEXT("Foggrounds"));

	TSharedRef<SHorizontalBox> NextMapRow = SNew(SHorizontalBox);
	auto AddNextMapButton = [this](const TSharedRef<SHorizontalBox>& Row, const FString& Label, const FOnClicked& OnClicked)
	{
		Row->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.IsEnabled(this, &SBHMainMenu::CanEditRoles)
				.ContentPadding(FMargin(8.0f, 5.0f))
				.OnClicked(OnClicked)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.Text(FText::FromString(Label))
				]
			];
	};
	AddNextMapButton(NextMapRow, TEXT("Next Facility"), FOnClicked::CreateSP(this, &SBHMainMenu::OnNextFacilityClicked));
	AddNextMapButton(NextMapRow, TEXT("Next Substation"), FOnClicked::CreateSP(this, &SBHMainMenu::OnNextSubstationClicked));
	AddNextMapButton(NextMapRow, TEXT("Next Foggrounds"), FOnClicked::CreateSP(this, &SBHMainMenu::OnNextFoggroundsClicked));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(MenuFont(18, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.90f, 0.98f, 0.94f, 1.0f))
			.Text(FText::FromString(TEXT("Classroom")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 12.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(MenuFont(12))
			.ColorAndOpacity(FLinearColor(0.70f, 0.80f, 0.80f, 1.0f))
			.Text(FText::FromString(TEXT("Projector-ready session status for live classes.")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildClassroomRunbookPanel()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildClassroomPreflightPanel()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.030f, 0.050f, 0.048f, 0.95f))
			.Padding(10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(12, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.82f, 0.94f, 0.88f, 1.0f))
					.Text(FText::FromString(TEXT("Live Setup")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 7.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(FLinearColor(0.64f, 0.76f, 0.74f, 1.0f))
					.Text(FText::FromString(TEXT("Live Classroom uses the configured Playit tunnel first. LAN hosting stays under Host LAN.")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Font(MenuFont(10, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.70f, 0.82f, 0.80f, 1.0f))
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetLiveClassroomMapText)))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 7.0f)
				[
					LiveMapRow
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SBHMenuButton)
					.IsEnabled(this, &SBHMainMenu::CanUseClassroomHostControls)
					.ButtonColorAndOpacity(FLinearColor(0.16f, 0.42f, 0.37f, 1.0f))
					.ContentPadding(FMargin(10.0f, 6.0f))
					.OnClicked(this, &SBHMainMenu::OnHostSelectedLiveClassroomClicked)
					[
						SNew(STextBlock)
						.Font(MenuFont(11, FName(TEXT("Bold"))))
						.Text(FText::FromString(TEXT("HOST / RESTART LIVE CLASSROOM")))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 7.0f)
				[
					NextMapRow
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 7.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(FLinearColor(0.64f, 0.76f, 0.74f, 1.0f))
					.Text(FText::FromString(TEXT("For school PCs, ask students to choose Low 4GB or 720p Windowed if needed.")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SBHMenuButton)
						.ContentPadding(FMargin(8.0f, 5.0f))
						.OnClicked(this, &SBHMainMenu::OnGraphicsPresetClicked, 0)
						[
							SNew(STextBlock)
							.Font(MenuFont(10, FName(TEXT("Bold"))))
							.Text(FText::FromString(TEXT("Low 4GB")))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBHMenuButton)
						.ContentPadding(FMargin(8.0f, 5.0f))
						.OnClicked(this, &SBHMainMenu::OnResolutionClicked, 1280, 720, false)
						[
							SNew(STextBlock)
							.Font(MenuFont(10, FName(TEXT("Bold"))))
							.Text(FText::FromString(TEXT("720p Windowed")))
						]
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildLessonPresetPanel()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SBox)
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBHMainMenu::GetRevisionControlsVisibility)))
			[
				BuildRevisionControlsPanel()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.IsEnabled(this, &SBHMainMenu::CanOpenClassroomBoard)
				.ButtonColorAndOpacity(FLinearColor(0.12f, 0.36f, 0.32f, 1.0f))
				.ContentPadding(FMargin(12.0f, 7.0f))
				.OnClicked(this, &SBHMainMenu::OnOpenClassroomBoardClicked)
				[
					SNew(STextBlock)
					.Font(MenuFont(12, FName(TEXT("Bold"))))
					.Text(FText::FromString(TEXT("Open Projector Window")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.IsEnabled(this, &SBHMainMenu::CanUseClassroomHostControls)
				.ButtonColorAndOpacity(FLinearColor(0.18f, 0.26f, 0.34f, 1.0f))
				.ContentPadding(FMargin(12.0f, 7.0f))
				.OnClicked(this, &SBHMainMenu::OnOpenClassroomSupportFolderClicked)
				[
					SNew(STextBlock)
					.Font(MenuFont(12, FName(TEXT("Bold"))))
					.Text(FText::FromString(TEXT("Open Support Folder")))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(CanUseClassroomHostControls()
					? FLinearColor(0.62f, 0.72f, 0.72f, 1.0f)
					: FLinearColor(0.86f, 0.58f, 0.34f, 1.0f))
				.Text(CanUseClassroomHostControls()
					? FText::FromString(TEXT("Projector and support bundle folders are available on the host machine."))
					: FText::FromString(TEXT("Host-only classroom setup is not available on remote student clients.")))
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBox)
			.MinDesiredHeight(460.0f)
			[
				SNew(SBHClassroomBoard)
				.PlayerController(PlayerController)
				.bStandaloneWindow(false)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.Visibility(bShowLobbyRoster ? EVisibility::Visible : EVisibility::Collapsed)
			[
				BuildRoleAssignmentPanel()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			BuildStatusPanel()
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildGuidePanel()
{
	TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);

	Panel->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(19, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.92f, 0.98f, 0.94f, 1.0f))
				.Text(FText::FromString(TEXT("How To Survive Your First Round")))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.12f, 0.31f, 0.29f, 1.0f))
				.Padding(FMargin(9.0f, 4.0f))
				[
					SNew(STextBlock)
					.Font(MenuFont(9, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.95f, 1.0f, 0.96f, 1.0f))
					.Text(FText::FromString(TEXT("60 SECOND GUIDE")))
				]
			]
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 12.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(MenuFont(12))
			.ColorAndOpacity(FLinearColor(0.70f, 0.80f, 0.80f, 1.0f))
			.Text(FText::FromString(TEXT("Warm up your role, answer orange stations with 1-4, hold E on solved work, respect CCTV/noise warnings, use train stops for bonus points and shop upgrades, then board the final escape.")))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.024f, 0.032f, 0.038f, 0.96f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.96f, 0.86f, 0.62f, 1.0f))
					.Text(FText::FromString(TEXT("Actual annotated HUD screenshot")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					MenuGuideHudScreenshot(GetGuideHudBrush())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 7.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(9))
					.ColorAndOpacity(FLinearColor(0.64f, 0.74f, 0.72f, 1.0f))
					.Text(FText::FromString(TEXT("Real packaged screenshot with callouts. The examples below stay readable in menus while matching the same HUD cues.")))
				]
			]
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.024f, 0.032f, 0.038f, 0.96f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.96f, 0.86f, 0.62f, 1.0f))
					.Text(FText::FromString(TEXT("Field screenshot examples")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 5.0f, 0.0f, 9.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(10))
					.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.76f, 1.0f))
					.Text(FText::FromString(TEXT("These cards mirror the important cues from current field screenshots: lockers, path detection, Teacher sight, decoys, CCTV, Hall Monitors, tasks, questions, train stops, batteries, cosmetics, and final escape.")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MenuGuidePhotoSlideDeck()
				]
			]
		];

	TSharedRef<SHorizontalBox> PressureRow = SNew(SHorizontalBox);
	PressureRow->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideCallout(
				FText::FromString(TEXT("Fear is panic")),
				FText::FromString(TEXT("High fear cuts sprint performance, burns stamina faster, slows recovery, and makes panic noise more likely.")),
				FLinearColor(0.92f, 0.28f, 0.20f, 1.0f))
		];
	PressureRow->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideCallout(
				FText::FromString(TEXT("Dread is strain")),
				FText::FromString(TEXT("High dread slows movement, tightens your flashlight reach, shakes control, and can make hiding or camping too long betray you.")),
				FLinearColor(0.78f, 0.18f, 0.14f, 1.0f))
		];
	PressureRow->AddSlot()
		.FillWidth(1.0f)
		[
			MenuGuideCallout(
				FText::FromString(TEXT("Reset pressure")),
				FText::FromString(TEXT("Break line of sight, stop sprinting, let the Teacher meter clear, then move before the locker becomes the problem.")),
				FLinearColor(0.36f, 0.72f, 0.62f, 1.0f))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			PressureRow
		];

	TSharedRef<SGridPanel> ObjectGrid = SNew(SGridPanel);
	ObjectGrid->AddSlot(0, 0)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideObjectCard(
				FName(TEXT("Station")),
				FText::FromString(TEXT("Task Station")),
				FText::FromString(TEXT("Orange classroom station. Look at it, press 1-4, then hold E when solved work asks for a hold.")),
				FLinearColor(0.92f, 0.58f, 0.18f, 1.0f))
		];
	ObjectGrid->AddSlot(1, 0)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideObjectCard(
				FName(TEXT("Breaker")),
				FText::FromString(TEXT("Breaker Box")),
				FText::FromString(TEXT("Grey wall box with levers. Hold E to repair power, open security shutters, or blind a CCTV circuit.")),
				FLinearColor(0.86f, 0.62f, 0.14f, 1.0f))
		];
	ObjectGrid->AddSlot(2, 0)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			MenuGuideObjectCard(
				FName(TEXT("Battery")),
				FText::FromString(TEXT("Battery Pickup")),
				FText::FromString(TEXT("Green cylinder with yellow label. Restores flashlight, stamina, and nerve for longer chases.")),
				FLinearColor(0.38f, 0.95f, 0.78f, 1.0f))
		];
	ObjectGrid->AddSlot(0, 1)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideObjectCard(
				FName(TEXT("Locker")),
				FText::FromString(TEXT("Locker")),
				FText::FromString(TEXT("Short sight reset and a panic button: for about 2s after you pop out the Teacher cannot grab you, and you exit at a random spot in front. Dread still climbs if you camp it.")),
				FLinearColor(0.56f, 0.70f, 0.94f, 1.0f))
		];
	ObjectGrid->AddSlot(1, 1)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideObjectCard(
				FName(TEXT("Exit")),
				FText::FromString(TEXT("Exit / Subway")),
				FText::FromString(TEXT("Gate or final train door. When escape opens, hold E and leave before the Teacher locks the route.")),
				FLinearColor(0.40f, 1.0f, 0.62f, 1.0f))
		];
	ObjectGrid->AddSlot(2, 1)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			MenuGuideObjectCard(
				FName(TEXT("Decoy")),
				FText::FromString(TEXT("Decoy / Trap")),
				FText::FromString(TEXT("G drops a Survivor sound decoy or Hall Monitor trap. Q/R handle real hints and false routes.")),
				FLinearColor(0.84f, 0.30f, 0.80f, 1.0f))
		];
	ObjectGrid->AddSlot(0, 2)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideObjectCard(
				FName(TEXT("CCTV")),
				FText::FromString(TEXT("CCTV / Monitor")),
				FText::FromString(TEXT("Cameras reveal motion. Break sight, go prone, open shutters, or spoof false pings as Hall Monitor.")),
				FLinearColor(0.44f, 0.80f, 0.92f, 1.0f))
		];
	ObjectGrid->AddSlot(1, 2)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideObjectCard(
				FName(TEXT("Train")),
				FText::FromString(TEXT("Train Shop")),
				FText::FromString(TEXT("Intermission stop. Read recap, answer the bonus terminal, buy upgrades, then board before departure.")),
				FLinearColor(0.60f, 0.82f, 0.92f, 1.0f))
		];
	ObjectGrid->AddSlot(2, 2)
		[
			MenuGuideObjectCard(
				FName(TEXT("Glass")),
				FText::FromString(TEXT("Glass / Hard Landings")),
				FText::FromString(TEXT("Glass, metal, hard landings, and panic breathing are loud. Soft routes and crouch keep pressure lower.")),
				FLinearColor(0.92f, 0.28f, 0.20f, 1.0f))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.026f, 0.034f, 0.040f, 0.95f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.88f, 0.96f, 0.92f, 1.0f))
					.Text(FText::FromString(TEXT("Spot these in the room")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 9.0f, 0.0f, 0.0f)
				[
					ObjectGrid
				]
			]
		];

	TSharedRef<SGridPanel> QuestionNodeGrid = SNew(SGridPanel);
	QuestionNodeGrid->AddSlot(0, 0)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideQuestionNodeCard(
				FName(TEXT("Terminal")),
				FText::FromString(TEXT("Circuit Breaker Node")),
				FText::FromString(TEXT("ELECTRICITY")),
				FText::FromString(TEXT("Which change raises current?")),
				FText::FromString(TEXT("1-4 answer first")),
				FText::FromString(TEXT("Active nodes glow orange and block work until the question is answered.")),
				FLinearColor(0.08f, 0.76f, 0.92f, 1.0f))
		];
	QuestionNodeGrid->AddSlot(1, 0)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideQuestionNodeCard(
				FName(TEXT("Valve")),
				FText::FromString(TEXT("Counterweight Lock")),
				FText::FromString(TEXT("FORCES")),
				FText::FromString(TEXT("What balances this load?")),
				FText::FromString(TEXT("then HOLD E")),
				FText::FromString(TEXT("After the answer is accepted, the same node becomes a hold-E physical task.")),
				FLinearColor(0.86f, 0.50f, 0.10f, 1.0f))
		];
	QuestionNodeGrid->AddSlot(0, 1)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideQuestionNodeCard(
				FName(TEXT("Antenna")),
				FText::FromString(TEXT("Signal Receiver")),
				FText::FromString(TEXT("WAVES")),
				FText::FromString(TEXT("Pick the longest wavelength.")),
				FText::FromString(TEXT("team progress")),
				FText::FromString(TEXT("Revision nodes can chain questions toward team mastery before the room clears.")),
				FLinearColor(0.60f, 0.84f, 0.92f, 1.0f))
		];
	QuestionNodeGrid->AddSlot(1, 1)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideQuestionNodeCard(
				FName(TEXT("TrainBonus")),
				FText::FromString(TEXT("Train Bonus Terminal")),
				FText::FromString(TEXT("BONUS +PTS")),
				FText::FromString(TEXT("Answer while the train phase is live.")),
				FText::FromString(TEXT("1-4 for shop pts")),
				FText::FromString(TEXT("Correct bonus answers award shop points, stamina recovery, and fear relief.")),
				FLinearColor(0.96f, 0.78f, 0.22f, 1.0f))
		];
	QuestionNodeGrid->AddSlot(2, 0)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			MenuGuideQuestionNodeCard(
				FName(TEXT("Energy")),
				FText::FromString(TEXT("Energy Store")),
				FText::FromString(TEXT("ENERGY")),
				FText::FromString(TEXT("Where did energy transfer?")),
				FText::FromString(TEXT("green means done")),
				FText::FromString(TEXT("Completed nodes switch from orange pressure to green route progress.")),
				FLinearColor(0.96f, 0.46f, 0.12f, 1.0f))
		];
	QuestionNodeGrid->AddSlot(2, 1)
		[
			MenuGuideQuestionNodeCard(
				FName(TEXT("Terminal")),
				FText::FromString(TEXT("Feedback State")),
				FText::FromString(TEXT("EXPLANATION")),
				FText::FromString(TEXT("Read the short feedback, then move.")),
				FText::FromString(TEXT("do not camp node")),
				FText::FromString(TEXT("Wrong answers add pressure; correct answers steady the class and unlock progress.")),
				FLinearColor(0.34f, 0.92f, 0.64f, 1.0f))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.024f, 0.030f, 0.036f, 0.96f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.96f, 0.86f, 0.62f, 1.0f))
					.Text(FText::FromString(TEXT("Question node examples")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(9))
					.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.76f, 1.0f))
					.Text(FText::FromString(TEXT("These compact renders are based on the real question-node props and states: answer prompt, solved hold-E work, team revision progress, train bonus, completion, and feedback.")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 9.0f, 0.0f, 0.0f)
				[
					QuestionNodeGrid
				]
			]
		];

	TSharedRef<SGridPanel> KeyGrid = SNew(SGridPanel);
	KeyGrid->AddSlot(0, 0)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideActionTile(FText::FromString(TEXT("WASD / Shift")), FText::FromString(TEXT("Move")), FText::FromString(TEXT("Sprint is loud. Air steering is modest, so line up jumps before leaving the floor.")), FLinearColor(0.20f, 0.38f, 0.40f, 1.0f))
		];
	KeyGrid->AddSlot(1, 0)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideActionTile(FText::FromString(TEXT("Space")), FText::FromString(TEXT("Jump / Hop")), FText::FromString(TEXT("Tap Shift to top speed, then chain hops to keep sprint speed for less stamina. Buffer the next jump just before you land.")), FLinearColor(0.22f, 0.46f, 0.32f, 1.0f))
		];
	KeyGrid->AddSlot(2, 0)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			MenuGuideActionTile(FText::FromString(TEXT("Ctrl / Alt")), FText::FromString(TEXT("Low Moves")), FText::FromString(TEXT("Ctrl crouches or sprint-rolls. Alt toggles prone or sprint-slides; Alt+Space dives.")), FLinearColor(0.42f, 0.30f, 0.12f, 1.0f))
		];
	KeyGrid->AddSlot(0, 1)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideActionTile(FText::FromString(TEXT("E")), FText::FromString(TEXT("Use Things")), FText::FromString(TEXT("Hold for work, exits, shops, monitors, shutters, and breakers. Tap to leave lockers.")), FLinearColor(0.42f, 0.40f, 0.20f, 1.0f))
		];
	KeyGrid->AddSlot(1, 1)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideActionTile(FText::FromString(TEXT("1-4")), FText::FromString(TEXT("Answer")), FText::FromString(TEXT("Station and train bonus answers use number keys while the prompt is live.")), FLinearColor(0.30f, 0.30f, 0.42f, 1.0f))
		];
	KeyGrid->AddSlot(2, 1)
		[
			MenuGuideActionTile(FText::FromString(TEXT("F / F1-F6")), FText::FromString(TEXT("Light / Powerups")), FText::FromString(TEXT("F toggles light. F1-F6 spend shop powerups: stamina, sprint, light, hint, decoy, door rush.")), FLinearColor(0.36f, 0.18f, 0.18f, 1.0f))
		];
	KeyGrid->AddSlot(0, 2)
		.Padding(0.0f, 8.0f, 8.0f, 0.0f)
		[
			MenuGuideActionTile(FText::FromString(TEXT("G")), FText::FromString(TEXT("Fake Trail")), FText::FromString(TEXT("Survivor decoy or Hall Monitor trap, depending on role and unlocks.")), FLinearColor(0.30f, 0.30f, 0.42f, 1.0f))
		];
	KeyGrid->AddSlot(1, 2)
		.Padding(0.0f, 8.0f, 8.0f, 0.0f)
		[
			MenuGuideActionTile(FText::FromString(TEXT("Q / R")), FText::FromString(TEXT("Role Powers")), FText::FromString(TEXT("Teacher scan/blackout. Hall Monitor real hint, aimed false marker, and CCTV spoof.")), FLinearColor(0.36f, 0.18f, 0.18f, 1.0f))
		];
	KeyGrid->AddSlot(2, 2)
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			MenuGuideActionTile(FText::FromString(TEXT("B / H / T-Y-U")), FText::FromString(TEXT("Class Tools")), FText::FromString(TEXT("B host board. H sends spectator support; T/Y/U queue next-round role requests.")), FLinearColor(0.22f, 0.32f, 0.44f, 1.0f))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.026f, 0.034f, 0.040f, 0.95f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.88f, 0.96f, 0.92f, 1.0f))
					.Text(FText::FromString(TEXT("Buttons that matter first")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 9.0f, 0.0f, 0.0f)
				[
					KeyGrid
				]
			]
		];

	TSharedRef<SHorizontalBox> FlowRow = SNew(SHorizontalBox);
	FlowRow->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideFlowStep(
				1,
				FText::FromString(TEXT("Warm up role")),
				FText::FromString(TEXT("Practice Lab and Role Warmup let players try movement, powers, answers, and captures before the hunt.")),
				FLinearColor(0.92f, 0.58f, 0.18f, 1.0f))
		];
	FlowRow->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideFlowStep(
				2,
				FText::FromString(TEXT("Answer and repair")),
				FText::FromString(TEXT("Press 1-4 for questions, then hold E for work, breakers, shutters, shops, or exits.")),
				FLinearColor(0.22f, 0.62f, 0.54f, 1.0f))
		];
	FlowRow->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideFlowStep(
				3,
				FText::FromString(TEXT("Read threats")),
				FText::FromString(TEXT("Teacher meter, CCTV pings, loud surfaces, fear, and dread mean break sight or go low before pressure spikes.")),
				FLinearColor(0.90f, 0.22f, 0.14f, 1.0f))
		];
	FlowRow->AddSlot()
		.FillWidth(1.0f)
		[
			MenuGuideFlowStep(
				4,
				FText::FromString(TEXT("Train to final")),
				FText::FromString(TEXT("At train stops, answer bonus questions and buy upgrades. At final escape, board fast.")),
				FLinearColor(0.40f, 1.0f, 0.62f, 1.0f))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.024f, 0.032f, 0.038f, 0.96f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.96f, 0.86f, 0.62f, 1.0f))
					.Text(FText::FromString(TEXT("The loop")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 9.0f, 0.0f, 0.0f)
				[
					FlowRow
				]
			]
		];

	const TArray<FText> SurvivorItems = {
		FText::FromString(TEXT("Answer stations with 1-4, then hold E for solved work, breakers, shutters, shops, and exits.")),
		FText::FromString(TEXT("Use crouch, prone, slide, roll, dive, lockers, and door slams to break Teacher timing.")),
		FText::FromString(TEXT("Aim flashlight during the Teacher swing window to stagger if you have battery.")),
		FText::FromString(TEXT("Spend train shop points on stamina, sprint, light, hint, decoy, and door-rush powerups."))
	};
	const TArray<FText> TeacherItems = {
		FText::FromString(TEXT("Mouse1 starts an axe swing with windup and recovery; visible Survivors can dodge the timing.")),
		FText::FromString(TEXT("Q scan points you toward Survivor pressure; R blackout drains comfort and route safety.")),
		FText::FromString(TEXT("Security monitors turn CCTV motion into stronger temporary locks.")),
		FText::FromString(TEXT("Capture points buy scan focus, blackout surge, and patrol intel upgrades at train stops."))
	};
	const TArray<FText> MonitorItems = {
		FText::FromString(TEXT("You look scary, but cannot capture.")),
		FText::FromString(TEXT("G drops traps after tools unlock; place them on chase exits and objective routes.")),
		FText::FromString(TEXT("Q gives real hints; R aims false corridor markers to pull the Teacher off-route.")),
		FText::FromString(TEXT("Use security monitors to spoof false CCTV motion when contribution unlocks it.")),
		FText::FromString(TEXT("In classroom rounds, answer-team work unlocks stronger support tools."))
	};

	TSharedRef<SHorizontalBox> RoleCards = SNew(SHorizontalBox);
	RoleCards->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Survivor")),
				FText::FromString(TEXT("OBJECTIVE")),
				SurvivorItems,
				FLinearColor(0.20f, 0.62f, 0.54f, 1.0f))
		];
	RoleCards->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Teacher")),
				FText::FromString(TEXT("HUNTER")),
				TeacherItems,
				FLinearColor(0.56f, 0.12f, 0.10f, 1.0f))
		];
	RoleCards->AddSlot()
		.FillWidth(1.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Hall Monitor")),
				FText::FromString(TEXT("MISDIRECTION")),
				MonitorItems,
				FLinearColor(0.62f, 0.36f, 0.10f, 1.0f))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.026f, 0.034f, 0.040f, 0.95f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.88f, 0.96f, 0.92f, 1.0f))
					.Text(FText::FromString(TEXT("Pick up your role fast")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 9.0f, 0.0f, 0.0f)
				[
					RoleCards
				]
			]
		];

	const TArray<FText> ExpertMovementItems = {
		FText::FromString(TEXT("Bunny hop to travel: tap Shift to reach sprint speed, then chain jumps. Airborne the per-second sprint drain stops, so a clean chain holds top speed for less stamina than holding Shift, letting you outlast the chase.")),
		FText::FromString(TEXT("Buffer each hop by tapping Space just before you land; miss the rhythm and the jumps only bleed stamina with no speed gain.")),
		FText::FromString(TEXT("Air steering is limited: line up before takeoff, then strafe/look only for small corrections.")),
		FText::FromString(TEXT("Sprint+Ctrl rolls through capture timing windows and fits corner/doorway dodges.")),
		FText::FromString(TEXT("Sprint+Alt slides farther and lower; holding Alt can leave you prone, but it is not silent.")),
		FText::FromString(TEXT("Alt prone is slow, quiet, and low-visibility; Space+Alt while moving commits to a dive escape."))
	};
	const TArray<FText> ExpertSoundItems = {
		FText::FromString(TEXT("Running, repairs, hard landings, panic breathing, detention events, and glass are Teacher-readable clues.")),
		FText::FromString(TEXT("Metal, tile, wet, gravel, and glass surfaces carry farther than soft routes.")),
		FText::FromString(TEXT("Drop decoys after changing direction so the sound points away from the real route.")),
		FText::FromString(TEXT("Crouch, prone, or pause when the Teacher meter is near instead of broadcasting footsteps."))
	};
	const TArray<FText> ExpertSurvivorItems = {
		FText::FromString(TEXT("Counter CCTV by breaking line of sight, going prone, opening shutters, or baiting a false route.")),
		FText::FromString(TEXT("Cornered? Duck into a locker and pop straight back out: for about 2 seconds after you leave one the Teacher cannot capture you, and you exit at an unpredictable spot to break the chase.")),
		FText::FromString(TEXT("Commit to hold-E work only after checking sound, CCTV, and Teacher meter pressure.")),
		FText::FromString(TEXT("Flashlight stagger, door slam, roll, slide, prone, and dive all matter during axe timing.")),
		FText::FromString(TEXT("At final escape the Teacher is slowed, so a straight sprint to the train usually beats them; drop optional pickups and use Door Rush or Sprint Burst to board."))
	};
	const TArray<FText> ExpertTeacherItems = {
		FText::FromString(TEXT("Swing when survivors are committed to work, doors, ladders, slides, or bad landings.")),
		FText::FromString(TEXT("Use Q scan and security monitor locks to pick a lane before sprinting into a chase.")),
		FText::FromString(TEXT("Use R blackout to make survivors spend battery, stamina, and nerve before contact.")),
		FText::FromString(TEXT("Train upgrades stack: scan focus, blackout surge, and patrol intel improve pressure reads."))
	};
	const TArray<FText> ExpertMonitorItems = {
		FText::FromString(TEXT("Place traps at exits from safe rooms, not in the middle of open space.")),
		FText::FromString(TEXT("Send real hints when the Teacher is nearby; aim false markers down routes to pull pressure off teammates.")),
		FText::FromString(TEXT("Spoof CCTV from monitors to make the Teacher chase a fake motion ping.")),
		FText::FromString(TEXT("You cannot capture, so your job is timing, misdirection, and route denial."))
	};
	const TArray<FText> ExpertClassroomItems = {
		FText::FromString(TEXT("Lesson Presets set topic, difficulty, map, bots, mastery goals, duration, and comfort defaults.")),
		FText::FromString(TEXT("Train recap and bonus questions are where teams convert knowledge into shop points.")),
		FText::FromString(TEXT("Spectators use H for safe support and T/Y/U to request next-round roles.")),
		FText::FromString(TEXT("Fast escape still needs mastery progress, so split tasks and questions deliberately."))
	};

	TSharedRef<SGridPanel> ExpertGrid = SNew(SGridPanel);
	ExpertGrid->AddSlot(0, 0)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Advanced Movement")),
				FText::FromString(TEXT("NOISE")),
				ExpertMovementItems,
				FLinearColor(0.34f, 0.72f, 0.62f, 1.0f))
		];
	ExpertGrid->AddSlot(1, 0)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Sound Discipline")),
				FText::FromString(TEXT("STEALTH")),
				ExpertSoundItems,
				FLinearColor(0.92f, 0.42f, 0.18f, 1.0f))
		];
	ExpertGrid->AddSlot(2, 0)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Survivor Route")),
				FText::FromString(TEXT("TEMPO")),
				ExpertSurvivorItems,
				FLinearColor(0.22f, 0.62f, 0.54f, 1.0f))
		];
	ExpertGrid->AddSlot(0, 1)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Teacher Hunt")),
				FText::FromString(TEXT("PRESSURE")),
				ExpertTeacherItems,
				FLinearColor(0.64f, 0.14f, 0.10f, 1.0f))
		];
	ExpertGrid->AddSlot(1, 1)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Hall Monitor Plays")),
				FText::FromString(TEXT("MISLEAD")),
				ExpertMonitorItems,
				FLinearColor(0.72f, 0.42f, 0.12f, 1.0f))
		];
	ExpertGrid->AddSlot(2, 1)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Classroom Wins")),
				FText::FromString(TEXT("MASTERY")),
				ExpertClassroomItems,
				FLinearColor(0.56f, 0.70f, 0.94f, 1.0f))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			MenuPlayDropdownSection(
				FText::FromString(TEXT("Expert Guide")),
				FText::FromString(TEXT("Advanced movement, sound discipline, role pressure, and classroom scoring tips for players who know the basics.")),
				FLinearColor(0.84f, 0.56f, 0.22f, 1.0f),
				ExpertGrid)
		];

	const TArray<FText> HostSetupItems = {
		FText::FromString(TEXT("Host Preflight checks endpoint, loopback, tunnel, RHI, package, log, and support-bundle paths.")),
		FText::FromString(TEXT("Copy Join Code, Start Tunnel, Support Bundle, and deployment links are host-only controls.")),
		FText::FromString(TEXT("Classroom Board opens on the host for projector-safe roster, mastery, and round status."))
	};
	const TArray<FText> HostPresetItems = {
		FText::FromString(TEXT("Lesson Presets can apply topic mix, difficulty, map, bot count, scare level, round time, and goals.")),
		FText::FromString(TEXT("Role Warmup clears practice props and resets players when the host starts the live hunt.")),
		FText::FromString(TEXT("Bot Facility is the safest pressure test before real students join."))
	};
	const TArray<FText> HostReportItems = {
		FText::FromString(TEXT("CSV and heatmap exports stay local teacher/developer actions.")),
		FText::FromString(TEXT("Comfort settings cover reduced jumpscares, flash, camera shake, captions, and high-contrast HUD.")),
		FText::FromString(TEXT("Student clients should not see host admin, test, tunnel, or support-bundle tools."))
	};

	TSharedRef<SGridPanel> HostGuideGrid = SNew(SGridPanel);
	HostGuideGrid->AddSlot(0, 0)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Host Setup")),
				FText::FromString(TEXT("PREFLIGHT")),
				HostSetupItems,
				FLinearColor(0.44f, 0.80f, 0.92f, 1.0f))
		];
	HostGuideGrid->AddSlot(1, 0)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Lesson Flow")),
				FText::FromString(TEXT("PRESETS")),
				HostPresetItems,
				FLinearColor(0.62f, 0.72f, 0.36f, 1.0f))
		];
	HostGuideGrid->AddSlot(2, 0)
		[
			MenuGuideRoleCard(
				FText::FromString(TEXT("Reports And Comfort")),
				FText::FromString(TEXT("SAFE")),
				HostReportItems,
				FLinearColor(0.70f, 0.54f, 0.86f, 1.0f))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			MenuPlayDropdownSection(
				FText::FromString(TEXT("Classroom Host Quick Guide")),
				FText::FromString(TEXT("Host-only setup, lesson presets, reporting, and comfort systems for classroom builds.")),
				FLinearColor(0.42f, 0.70f, 0.84f, 1.0f),
				HostGuideGrid)
		];

	Panel->AddSlot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.036f, 0.040f, 0.046f, 0.94f))
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(MenuFont(13, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.96f, 0.92f, 0.76f, 1.0f))
					.Text(FText::FromString(TEXT("Best first round")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(MenuFont(11))
					.ColorAndOpacity(FLinearColor(0.70f, 0.78f, 0.76f, 1.0f))
					.Text(FText::FromString(TEXT("Start with Practice Lab or Role Warmup so everyone tries movement, powers, answers, and captures safely. Then use Bot Facility for pressure; in live classroom, run Preflight and apply the lesson preset before ready-up.")))
				]
			]
		];

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.034f, 0.040f, 0.048f, 0.95f))
		.Padding(10.0f)
		[
			Panel
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildControlsPanel()
{
	const FString LookBinding = FString::Printf(TEXT("%s + %s / Arrow Keys"),
		*MenuDescribeAxisBinding(FName(TEXT("Turn")), 1.0f, TEXT("Mouse X")),
		*MenuDescribeAxisBinding(FName(TEXT("LookUp")), -1.0f, TEXT("Mouse Y")));

	TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);

	auto AddSectionTitle = [&Panel](const TCHAR* Title)
	{
		Panel->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
				.Text(FText::FromString(Title))
			];
	};

	auto AddControl = [&Panel](const TCHAR* Label, const FString& Keys, const TCHAR* Detail)
	{
		Panel->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				MenuControlRow(FText::FromString(Label), Keys, FText::FromString(Detail))
			];
	};

	Panel->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(MenuFont(18, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.90f, 0.98f, 0.94f, 1.0f))
			.Text(FText::FromString(TEXT("Controls")))
		];

	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(MenuFont(12))
			.ColorAndOpacity(FLinearColor(0.70f, 0.80f, 0.80f, 1.0f))
			.Text(FText::FromString(TEXT("Current keyboard and mouse bindings, including role, shop, spectator, and host-only controls.")))
		];

	AddSectionTitle(TEXT("Movement"));
	AddControl(TEXT("Move Forward"), MenuDescribeAxisBinding(FName(TEXT("MoveForward")), 1.0f, TEXT("W")), TEXT("Walk forward."));
	AddControl(TEXT("Move Back"), MenuDescribeAxisBinding(FName(TEXT("MoveForward")), -1.0f, TEXT("S")), TEXT("Back away."));
	AddControl(TEXT("Move Right"), MenuDescribeAxisBinding(FName(TEXT("MoveRight")), 1.0f, TEXT("D")), TEXT("Strafe right."));
	AddControl(TEXT("Move Left"), MenuDescribeAxisBinding(FName(TEXT("MoveRight")), -1.0f, TEXT("A")), TEXT("Strafe left."));
	AddControl(TEXT("Look"), LookBinding, TEXT("Turn and look up or down."));
	AddControl(TEXT("Jump / Bunny Hop"), MenuDescribeActionBinding(FName(TEXT("Jump")), TEXT("Space")), TEXT("Jump; after tapping Shift to top speed, chain hops to hold sprint speed for less stamina. Buffer near landing. Air steering is limited."));
	AddControl(TEXT("Sprint"), MenuDescribeActionBinding(FName(TEXT("Sprint")), TEXT("Left Shift")), TEXT("Hold while moving; faster, louder, and stamina-limited."));
	AddControl(TEXT("Crouch / Roll"), MenuDescribeActionBinding(FName(TEXT("Crouch")), TEXT("Left Ctrl")), TEXT("Crouch normally; press while sprinting to roll through timing windows."));
	AddControl(TEXT("Prone / Slide"), MenuDescribeActionBinding(FName(TEXT("Prone")), TEXT("Left Alt")), TEXT("Tap to toggle prone; hold while sprinting to slide, and keep held to end low."));
	AddControl(TEXT("Dive"), FString::Printf(TEXT("%s + %s"), *MenuDescribeActionBinding(FName(TEXT("Jump")), TEXT("Space")), *MenuDescribeActionBinding(FName(TEXT("Prone")), TEXT("Left Alt"))), TEXT("Hold prone, move, then press jump to dive; costs stamina and cooldown."));

	AddSectionTitle(TEXT("Survivor And Tools"));
	AddControl(TEXT("Interact"), MenuDescribeActionBinding(FName(TEXT("Interact")), TEXT("E")), TEXT("Use doors, lockers, breakers, shutters, exits, shops, monitors, glass, and stations."));
	AddControl(TEXT("Flashlight"), MenuDescribeActionBinding(FName(TEXT("Flashlight")), TEXT("F")), TEXT("Toggle the flashlight; aim during Teacher windup to stagger if battery remains."));
	AddControl(TEXT("Decoy / Trap"), MenuDescribeActionBinding(FName(TEXT("Decoy")), TEXT("G")), TEXT("Survivor drops a noise decoy; Hall Monitor drops a trap after unlock."));
	AddControl(TEXT("Role Scan / Hint"), MenuDescribeActionBinding(FName(TEXT("Scan")), TEXT("Q")), TEXT("Teacher heartbeat scan, or Hall Monitor real hint."));
	AddControl(TEXT("Role Power / False Marker"), MenuDescribeActionBinding(FName(TEXT("HunterPower")), TEXT("R")), TEXT("Teacher blackout, or Hall Monitor aimed false marker."));
	AddControl(TEXT("Powerup Slots"), TEXT("F1-F6"), TEXT("Use shop powerups: stamina, sprint, light, hint, decoy sound, and door rush."));
	AddControl(TEXT("Security Feed"), TEXT("Hold E on monitor"), TEXT("Teacher tracks camera feed, Hall Monitor spoofs CCTV motion, others can watch only."));
	AddControl(TEXT("Map"), MenuDescribeActionBinding(FName(TEXT("Map")), TEXT("M / I")), TEXT("Raise or lower the HUD map."));
	AddControl(TEXT("Answer Choice"), TEXT("1-4 / Numpad 1-4"), TEXT("Submit classroom station and train bonus answers."));
	AddControl(TEXT("Node Marker"), MenuDescribeActionBinding(FName(TEXT("NodeMarker")), TEXT("N")), TEXT("Pin a routing marker to the nearest active question node for 8 seconds."));

	AddSectionTitle(TEXT("Teacher, Spectator, And Host"));
	AddControl(TEXT("Capture"), MenuDescribeActionBinding(FName(TEXT("Capture")), TEXT("Left Mouse Button")), TEXT("Start a close axe swing; survivors can dodge, blind, slam doors, or use timed special moves."));
	AddControl(TEXT("Ready"), MenuDescribeActionBinding(FName(TEXT("Ready")), TEXT("Enter")), TEXT("Toggle lobby readiness."));
	AddControl(TEXT("Menu"), MenuDescribeActionBinding(FName(TEXT("Menu")), TEXT("Escape")), TEXT("Open or close this menu."));
	AddControl(TEXT("Classroom Board"), MenuDescribeActionBinding(FName(TEXT("ClassroomBoard")), TEXT("B")), TEXT("Open the projector board on the host."));
	AddControl(TEXT("Spectator Support"), MenuDescribeActionBinding(FName(TEXT("SpectatorEncourage")), TEXT("H")), TEXT("Late spectators can send a safe team encouragement."));
	AddControl(TEXT("Spectator Role Request"), FString::Printf(TEXT("%s / %s / %s"),
		*MenuDescribeActionBinding(FName(TEXT("SpectatorQueueTeacher")), TEXT("T")),
		*MenuDescribeActionBinding(FName(TEXT("SpectatorQueueSurvivor")), TEXT("Y")),
		*MenuDescribeActionBinding(FName(TEXT("SpectatorQueueMonitor")), TEXT("U"))), TEXT("Late spectators can request Teacher, Survivor, or Monitor next round."));
	AddControl(TEXT("Cycle Reticle"), MenuDescribeActionBinding(FName(TEXT("CycleCrosshair")), TEXT("V")), TEXT("Change the HUD reticle style."));
	AddControl(TEXT("Force Start"), MenuDescribeActionBinding(FName(TEXT("ForceStartRound")), TEXT("F10")), TEXT("Host-only lobby start or warmup-end shortcut."));
	AddControl(TEXT("Tester Train"), TEXT("Home / Numpad 6"), TEXT("Open the train intermission in Test Round."));
	AddControl(TEXT("Tester Final Station"), TEXT("End / Numpad 8"), TEXT("Load the Foggrounds final-station route in Test Round."));
	AddControl(TEXT("Tester Final Escape"), TEXT("PageDown / Numpad 9"), TEXT("Trigger final subway escape in Test Round."));

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.034f, 0.040f, 0.048f, 0.95f))
		.Padding(10.0f)
		[
			Panel
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildGraphicsPanel()
{
	TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);
	bool bHasGraphicsPanelTitle = false;

	auto AddTitle = [&](const TCHAR* Title)
	{
		Panel->AddSlot()
			.AutoHeight()
			.Padding(0.0f, bHasGraphicsPanelTitle ? 12.0f : 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
				.Text(FText::FromString(Title))
			];
		bHasGraphicsPanelTitle = true;
	};

	auto AddButton = [this](const TSharedRef<SHorizontalBox>& Row, const FString& Label, const FOnClicked& Clicked, const TAttribute<FSlateColor>& ButtonColor)
	{
		Row->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBHMenuButton)
				.ContentPadding(FMargin(7.0f, 4.0f))
				.OnClicked(Clicked)
				.ButtonColorAndOpacity(ButtonColor)
				[
					SNew(STextBlock)
					.Font(MenuFont(9, FName(TEXT("Bold"))))
					.Text(FText::FromString(Label))
				]
			];
	};

	auto AddButtonRow = [&](const TCHAR* Label) -> TSharedRef<SHorizontalBox>
	{
		TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(92.0f)
					[
						SNew(STextBlock)
						.Font(MenuFont(9, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.74f, 0.82f, 0.80f, 1.0f))
						.Text(FText::FromString(Label))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Buttons
				]
			];
		return Buttons;
	};

	auto AddSliderRow = [&](const TAttribute<FText>& Label, const TAttribute<float>& Value, const FOnFloatValueChanged& OnChanged)
	{
		Panel->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(92.0f)
					[
						SNew(STextBlock)
						.Font(MenuFont(9, FName(TEXT("Bold"))))
						.ColorAndOpacity(FLinearColor(0.74f, 0.82f, 0.80f, 1.0f))
						.Text(Label)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(240.0f)
					[
						SNew(SSlider)
						.Value(Value)
						.OnValueChanged(OnChanged)
					]
				]
		];
	};

	auto BoolColor = [this](const TCHAR* OptionName, bool bValue)
	{
		return TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetGraphicsBoolOptionButtonColor, FName(OptionName), bValue));
	};

	auto ComfortBoolColor = [this](const TCHAR* OptionName, bool bValue)
	{
		return TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetComfortBoolOptionButtonColor, FName(OptionName), bValue));
	};

	auto OptionColor = [this](const TCHAR* OptionName, int32 Value)
	{
		return TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetGraphicsOptionButtonColor, FName(OptionName), Value));
	};

	auto ResolutionColor = [this](int32 Width, int32 Height, bool bFullscreen)
	{
		return TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetGraphicsResolutionButtonColor, Width, Height, bFullscreen));
	};

	AddTitle(TEXT("Audio"));
	AddSliderRow(
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetMasterVolumeText)),
		TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(this, &SBHMainMenu::GetMasterVolumeValue)),
		FOnFloatValueChanged::CreateSP(this, &SBHMainMenu::OnMasterVolumeChanged));
	AddSliderRow(
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetMusicVolumeText)),
		TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(this, &SBHMainMenu::GetMusicVolumeValue)),
		FOnFloatValueChanged::CreateSP(this, &SBHMainMenu::OnMusicVolumeChanged));
	AddSliderRow(
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetUiVolumeText)),
		TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(this, &SBHMainMenu::GetUiVolumeValue)),
		FOnFloatValueChanged::CreateSP(this, &SBHMainMenu::OnUiVolumeChanged));

	AddTitle(TEXT("Comfort"));
	TSharedRef<SHorizontalBox> ScareComfortRow = AddButtonRow(TEXT("Scares"));
	AddButton(ScareComfortRow, TEXT("Reduced"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ReducedJumpscares")), true), ComfortBoolColor(TEXT("ReducedJumpscares"), true));
	AddButton(ScareComfortRow, TEXT("Full"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ReducedJumpscares")), false), ComfortBoolColor(TEXT("ReducedJumpscares"), false));

	TSharedRef<SHorizontalBox> FlashComfortRow = AddButtonRow(TEXT("Flash"));
	AddButton(FlashComfortRow, TEXT("Reduced"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ReducedFlash")), true), ComfortBoolColor(TEXT("ReducedFlash"), true));
	AddButton(FlashComfortRow, TEXT("Full"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ReducedFlash")), false), ComfortBoolColor(TEXT("ReducedFlash"), false));

	TSharedRef<SHorizontalBox> ShakeComfortRow = AddButtonRow(TEXT("Shake"));
	AddButton(ShakeComfortRow, TEXT("Reduced"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ReducedCameraShake")), true), ComfortBoolColor(TEXT("ReducedCameraShake"), true));
	AddButton(ShakeComfortRow, TEXT("Full"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ReducedCameraShake")), false), ComfortBoolColor(TEXT("ReducedCameraShake"), false));

	TSharedRef<SHorizontalBox> CaptionRow = AddButtonRow(TEXT("Captions"));
	AddButton(CaptionRow, TEXT("On"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("Captions")), true), ComfortBoolColor(TEXT("Captions"), true));
	AddButton(CaptionRow, TEXT("Off"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("Captions")), false), ComfortBoolColor(TEXT("Captions"), false));

	TSharedRef<SHorizontalBox> ContrastRow = AddButtonRow(TEXT("HUD"));
	AddButton(ContrastRow, TEXT("High Contrast"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("HighContrastHud")), true), ComfortBoolColor(TEXT("HighContrastHud"), true));
	AddButton(ContrastRow, TEXT("Standard"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("HighContrastHud")), false), ComfortBoolColor(TEXT("HighContrastHud"), false));

	AddTitle(TEXT("HUD"));
	AddSliderRow(
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetHudScaleText)),
		TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(this, &SBHMainMenu::GetHudScaleValue)),
		FOnFloatValueChanged::CreateSP(this, &SBHMainMenu::OnHudScaleChanged));
	AddSliderRow(
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBHMainMenu::GetHudPanelOpacityText)),
		TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(this, &SBHMainMenu::GetHudPanelOpacityValue)),
		FOnFloatValueChanged::CreateSP(this, &SBHMainMenu::OnHudPanelOpacityChanged));

	TSharedRef<SHorizontalBox> ColorblindRow = AddButtonRow(TEXT("Colorblind"));
	AddButton(ColorblindRow, TEXT("On"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ColorblindHud")), true), ComfortBoolColor(TEXT("ColorblindHud"), true));
	AddButton(ColorblindRow, TEXT("Off"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ColorblindHud")), false), ComfortBoolColor(TEXT("ColorblindHud"), false));

	TSharedRef<SHorizontalBox> MinimapRow = AddButtonRow(TEXT("Minimap"));
	AddButton(MinimapRow, TEXT("Show"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowMinimap")), true), ComfortBoolColor(TEXT("ShowMinimap"), true));
	AddButton(MinimapRow, TEXT("Hide"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowMinimap")), false), ComfortBoolColor(TEXT("ShowMinimap"), false));

	TSharedRef<SHorizontalBox> NameplateRow = AddButtonRow(TEXT("Nameplates"));
	AddButton(NameplateRow, TEXT("Show"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowNameplates")), true), ComfortBoolColor(TEXT("ShowNameplates"), true));
	AddButton(NameplateRow, TEXT("Hide"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowNameplates")), false), ComfortBoolColor(TEXT("ShowNameplates"), false));

	TSharedRef<SHorizontalBox> VitalsRow = AddButtonRow(TEXT("Vitals"));
	AddButton(VitalsRow, TEXT("Show"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowVitals")), true), ComfortBoolColor(TEXT("ShowVitals"), true));
	AddButton(VitalsRow, TEXT("Hide"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowVitals")), false), ComfortBoolColor(TEXT("ShowVitals"), false));

	TSharedRef<SHorizontalBox> ObjectivesRow = AddButtonRow(TEXT("Objectives"));
	AddButton(ObjectivesRow, TEXT("Show"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowObjectiveBeats")), true), ComfortBoolColor(TEXT("ShowObjectiveBeats"), true));
	AddButton(ObjectivesRow, TEXT("Hide"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowObjectiveBeats")), false), ComfortBoolColor(TEXT("ShowObjectiveBeats"), false));

	TSharedRef<SHorizontalBox> ThreatArrowRow = AddButtonRow(TEXT("Threat Arrow"));
	AddButton(ThreatArrowRow, TEXT("Show"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowThreatArrow")), true), ComfortBoolColor(TEXT("ShowThreatArrow"), true));
	AddButton(ThreatArrowRow, TEXT("Hide"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowThreatArrow")), false), ComfortBoolColor(TEXT("ShowThreatArrow"), false));

	TSharedRef<SHorizontalBox> CrosshairAlertRow = AddButtonRow(TEXT("Crosshair Alert"));
	AddButton(CrosshairAlertRow, TEXT("Show"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowCrosshairDanger")), true), ComfortBoolColor(TEXT("ShowCrosshairDanger"), true));
	AddButton(CrosshairAlertRow, TEXT("Hide"), FOnClicked::CreateSP(this, &SBHMainMenu::OnComfortOptionClicked, FName(TEXT("ShowCrosshairDanger")), false), ComfortBoolColor(TEXT("ShowCrosshairDanger"), false));

	AddTitle(TEXT("Local Display"));
	Panel->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(MenuFont(10))
			.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.70f, 1.0f))
			.Text(this, &SBHMainMenu::GetGraphicsSummaryText)
		];

	TSharedRef<SHorizontalBox> PresetRow = AddButtonRow(TEXT("Preset"));
	AddButton(PresetRow, TEXT("Auto"), FOnClicked::CreateSP(this, &SBHMainMenu::OnAutoGraphicsClicked), BoolColor(TEXT("Auto"), true));
	AddButton(PresetRow, TEXT("Low 4GB"), FOnClicked::CreateSP(this, &SBHMainMenu::OnGraphicsPresetClicked, 0), OptionColor(TEXT("Preset"), 0));
	AddButton(PresetRow, TEXT("Med"), FOnClicked::CreateSP(this, &SBHMainMenu::OnGraphicsPresetClicked, 1), OptionColor(TEXT("Preset"), 1));
	AddButton(PresetRow, TEXT("High"), FOnClicked::CreateSP(this, &SBHMainMenu::OnGraphicsPresetClicked, 2), OptionColor(TEXT("Preset"), 2));
	AddButton(PresetRow, TEXT("Ultra"), FOnClicked::CreateSP(this, &SBHMainMenu::OnGraphicsPresetClicked, 3), OptionColor(TEXT("Preset"), 3));

	TSharedRef<SHorizontalBox> AdaptiveRow = AddButtonRow(TEXT("Adaptive"));
	AddButton(AdaptiveRow, TEXT("On"), FOnClicked::CreateSP(this, &SBHMainMenu::OnAdaptiveGraphicsClicked, true), BoolColor(TEXT("Adaptive"), true));
	AddButton(AdaptiveRow, TEXT("Off"), FOnClicked::CreateSP(this, &SBHMainMenu::OnAdaptiveGraphicsClicked, false), BoolColor(TEXT("Adaptive"), false));

	TSharedRef<SHorizontalBox> FpsGoalRow = AddButtonRow(TEXT("FPS Goal"));
	AddButton(FpsGoalRow, TEXT("30"), FOnClicked::CreateSP(this, &SBHMainMenu::OnAdaptiveFrameRateGoalClicked, 30), OptionColor(TEXT("AdaptiveFps"), 30));
	AddButton(FpsGoalRow, TEXT("45"), FOnClicked::CreateSP(this, &SBHMainMenu::OnAdaptiveFrameRateGoalClicked, 45), OptionColor(TEXT("AdaptiveFps"), 45));
	AddButton(FpsGoalRow, TEXT("60"), FOnClicked::CreateSP(this, &SBHMainMenu::OnAdaptiveFrameRateGoalClicked, 60), OptionColor(TEXT("AdaptiveFps"), 60));
	AddButton(FpsGoalRow, TEXT("90"), FOnClicked::CreateSP(this, &SBHMainMenu::OnAdaptiveFrameRateGoalClicked, 90), OptionColor(TEXT("AdaptiveFps"), 90));
	AddButton(FpsGoalRow, TEXT("120"), FOnClicked::CreateSP(this, &SBHMainMenu::OnAdaptiveFrameRateGoalClicked, 120), OptionColor(TEXT("AdaptiveFps"), 120));

	TSharedRef<SHorizontalBox> RenderScaleRow = AddButtonRow(TEXT("Scale"));
	AddButton(RenderScaleRow, TEXT("50%"), FOnClicked::CreateSP(this, &SBHMainMenu::OnRenderScaleClicked, 50), OptionColor(TEXT("RenderScale"), 50));
	AddButton(RenderScaleRow, TEXT("60%"), FOnClicked::CreateSP(this, &SBHMainMenu::OnRenderScaleClicked, 60), OptionColor(TEXT("RenderScale"), 60));
	AddButton(RenderScaleRow, TEXT("67%"), FOnClicked::CreateSP(this, &SBHMainMenu::OnRenderScaleClicked, 67), OptionColor(TEXT("RenderScale"), 67));
	AddButton(RenderScaleRow, TEXT("75%"), FOnClicked::CreateSP(this, &SBHMainMenu::OnRenderScaleClicked, 75), OptionColor(TEXT("RenderScale"), 75));
	AddButton(RenderScaleRow, TEXT("85%"), FOnClicked::CreateSP(this, &SBHMainMenu::OnRenderScaleClicked, 85), OptionColor(TEXT("RenderScale"), 85));
	AddButton(RenderScaleRow, TEXT("100%"), FOnClicked::CreateSP(this, &SBHMainMenu::OnRenderScaleClicked, 100), OptionColor(TEXT("RenderScale"), 100));

	TSharedRef<SHorizontalBox> ResolutionRow = AddButtonRow(TEXT("Resolution"));
	AddButton(ResolutionRow, TEXT("720 W"), FOnClicked::CreateSP(this, &SBHMainMenu::OnResolutionClicked, 1280, 720, false), ResolutionColor(1280, 720, false));
	AddButton(ResolutionRow, TEXT("1080 W"), FOnClicked::CreateSP(this, &SBHMainMenu::OnResolutionClicked, 1920, 1080, false), ResolutionColor(1920, 1080, false));
	AddButton(ResolutionRow, TEXT("1080 F"), FOnClicked::CreateSP(this, &SBHMainMenu::OnResolutionClicked, 1920, 1080, true), ResolutionColor(1920, 1080, true));
	AddButton(ResolutionRow, TEXT("1440 F"), FOnClicked::CreateSP(this, &SBHMainMenu::OnResolutionClicked, 2560, 1440, true), ResolutionColor(2560, 1440, true));

	TSharedRef<SHorizontalBox> FpsRow = AddButtonRow(TEXT("FPS Limit"));
	AddButton(FpsRow, TEXT("30"), FOnClicked::CreateSP(this, &SBHMainMenu::OnFrameRateClicked, 30), OptionColor(TEXT("FrameLimit"), 30));
	AddButton(FpsRow, TEXT("45"), FOnClicked::CreateSP(this, &SBHMainMenu::OnFrameRateClicked, 45), OptionColor(TEXT("FrameLimit"), 45));
	AddButton(FpsRow, TEXT("60"), FOnClicked::CreateSP(this, &SBHMainMenu::OnFrameRateClicked, 60), OptionColor(TEXT("FrameLimit"), 60));
	AddButton(FpsRow, TEXT("120"), FOnClicked::CreateSP(this, &SBHMainMenu::OnFrameRateClicked, 120), OptionColor(TEXT("FrameLimit"), 120));
	AddButton(FpsRow, TEXT("144"), FOnClicked::CreateSP(this, &SBHMainMenu::OnFrameRateClicked, 144), OptionColor(TEXT("FrameLimit"), 144));
	AddButton(FpsRow, TEXT("Free"), FOnClicked::CreateSP(this, &SBHMainMenu::OnFrameRateClicked, 0), OptionColor(TEXT("FrameLimit"), 0));

	TSharedRef<SHorizontalBox> TextureRow = AddButtonRow(TEXT("Texture"));
	AddButton(TextureRow, TEXT("Low"), FOnClicked::CreateSP(this, &SBHMainMenu::OnTextureQualityClicked, 0), OptionColor(TEXT("Texture"), 0));
	AddButton(TextureRow, TEXT("Med"), FOnClicked::CreateSP(this, &SBHMainMenu::OnTextureQualityClicked, 1), OptionColor(TEXT("Texture"), 1));
	AddButton(TextureRow, TEXT("High"), FOnClicked::CreateSP(this, &SBHMainMenu::OnTextureQualityClicked, 2), OptionColor(TEXT("Texture"), 2));
	AddButton(TextureRow, TEXT("Ultra"), FOnClicked::CreateSP(this, &SBHMainMenu::OnTextureQualityClicked, 3), OptionColor(TEXT("Texture"), 3));

	TSharedRef<SHorizontalBox> ShadowRow = AddButtonRow(TEXT("Shadows"));
	AddButton(ShadowRow, TEXT("Low"), FOnClicked::CreateSP(this, &SBHMainMenu::OnShadowQualityClicked, 0), OptionColor(TEXT("Shadow"), 0));
	AddButton(ShadowRow, TEXT("Med"), FOnClicked::CreateSP(this, &SBHMainMenu::OnShadowQualityClicked, 1), OptionColor(TEXT("Shadow"), 1));
	AddButton(ShadowRow, TEXT("High"), FOnClicked::CreateSP(this, &SBHMainMenu::OnShadowQualityClicked, 2), OptionColor(TEXT("Shadow"), 2));
	AddButton(ShadowRow, TEXT("Ultra"), FOnClicked::CreateSP(this, &SBHMainMenu::OnShadowQualityClicked, 3), OptionColor(TEXT("Shadow"), 3));

	TSharedRef<SHorizontalBox> EffectsRow = AddButtonRow(TEXT("Effects"));
	AddButton(EffectsRow, TEXT("Low"), FOnClicked::CreateSP(this, &SBHMainMenu::OnEffectsQualityClicked, 0), OptionColor(TEXT("Effects"), 0));
	AddButton(EffectsRow, TEXT("Med"), FOnClicked::CreateSP(this, &SBHMainMenu::OnEffectsQualityClicked, 1), OptionColor(TEXT("Effects"), 1));
	AddButton(EffectsRow, TEXT("High"), FOnClicked::CreateSP(this, &SBHMainMenu::OnEffectsQualityClicked, 2), OptionColor(TEXT("Effects"), 2));
	AddButton(EffectsRow, TEXT("Ultra"), FOnClicked::CreateSP(this, &SBHMainMenu::OnEffectsQualityClicked, 3), OptionColor(TEXT("Effects"), 3));

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.034f, 0.040f, 0.048f, 0.95f))
		.Padding(10.0f)
		[
			Panel
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildTestCommandPanel()
{
	auto AddCommand = [](const TSharedRef<SVerticalBox>& List, const TSharedRef<SWidget>& Command)
	{
		List->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				Command
			];
	};

	TSharedRef<SVerticalBox> JumpscareCommands = SNew(SVerticalBox);
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("RANDOM MONSTER SCARE")),
		FText::FromString(TEXT("Runs the current director monster scare path.")),
		FLinearColor(0.36f, 0.12f, 0.10f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnPracticeJumpscareClicked)));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("MONSTER IN YOUR FACE")),
		FText::FromString(TEXT("Slams a close-up creature into the camera with a scream and flash (immediate).")),
		FLinearColor(0.52f, 0.06f, 0.06f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("face")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("CORNER PEEK")),
		FText::FromString(TEXT("A figure leans out from a wall edge nearby; it ducks back when you look at it.")),
		FLinearColor(0.30f, 0.14f, 0.20f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("peek")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("SOMETHING BEHIND YOU")),
		FText::FromString(TEXT("Locks movement with a directive; turn around to spring the payoff jumpscare.")),
		FLinearColor(0.44f, 0.08f, 0.12f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("behind")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("SCP-096 REVEAL")),
		FText::FromString(TEXT("Appears far off with a faint red glow, pauses, then sprints in screaming and cuts the lights.")),
		FLinearColor(0.40f, 0.06f, 0.06f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("scp096")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("SCP-096 (REAL MODEL)")),
		FText::FromString(TEXT("The genuine SCP-096 creature on the same reveal -> pause -> charge -> blackout flow.")),
		FLinearColor(0.34f, 0.05f, 0.05f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("realscp")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("SUPER JUMPSCARE CHAIN")),
		FText::FromString(TEXT("Peek, screen-crossing sprint, final charge, then face close-up.")),
		FLinearColor(0.48f, 0.08f, 0.08f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("super")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("SUPER CHAIN 1")),
		FText::FromString(TEXT("Force Fab jumpscare 1 through the full chain.")),
		FLinearColor(0.42f, 0.10f, 0.09f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("super1")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("SUPER CHAIN 2")),
		FText::FromString(TEXT("Force Fab jumpscare 2 through the full chain.")),
		FLinearColor(0.40f, 0.09f, 0.13f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("super2")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("SUPER CHAIN 3")),
		FText::FromString(TEXT("Force Fab jumpscare 3 through the full chain.")),
		FLinearColor(0.34f, 0.10f, 0.16f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("super3")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("ALL JUMPSCARE VARIANTS")),
		FText::FromString(TEXT("Queues every configured variant in order with a short gap.")),
		FLinearColor(0.42f, 0.16f, 0.12f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("all")))));
	AddCommand(JumpscareCommands, MenuPlayActionButton(
		FText::FromString(TEXT("PRINT VARIANT LIST")),
		FText::FromString(TEXT("Shows the configured variant IDs in the status overlay and log.")),
		FLinearColor(0.18f, 0.24f, 0.30f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("list")))));

	const TArray<FBHJumpscareVariant> Variants = GetResolvedJumpscareVariants();
	if (!Variants.IsEmpty())
	{
		for (int32 Index = 0; Index < Variants.Num(); ++Index)
		{
			const FBHJumpscareVariant& Variant = Variants[Index];
			const FString VariantId = Variant.VariantId.IsNone() ? FString::FromInt(Index + 1) : Variant.VariantId.ToString();
			const FString DisplayName = Variant.DisplayName.IsEmpty() ? VariantId : Variant.DisplayName;
			const FString AssetState = (!Variant.VisualActorClass.IsNull() || !Variant.SkeletalMesh.IsNull() || !Variant.StaticMesh.IsNull())
				? TEXT("configured visual assets")
				: TEXT("proxy fallback visuals");
			AddCommand(JumpscareCommands, MenuPlayActionButton(
				FText::FromString(FString::Printf(TEXT("%d. %s"), Index + 1, *DisplayName)),
				FText::FromString(FString::Printf(TEXT("Force variant ID %s. Uses %s if imported."), *VariantId, *AssetState)),
				FLinearColor(0.30f, 0.12f, 0.13f, 1.0f),
				FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, VariantId)));
		}
	}
	else
	{
		AddCommand(JumpscareCommands, MenuPlayActionButton(
			FText::FromString(TEXT("PROXY FALLBACK")),
			FText::FromString(TEXT("No configured variants were found; fires the fallback monster.")),
			FLinearColor(0.30f, 0.12f, 0.13f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnJumpscareVariantClicked, FString(TEXT("ProxyRed")))));
	}

	TSharedRef<SVerticalBox> AtmosphereCommands = SNew(SVerticalBox);
	AddCommand(AtmosphereCommands, MenuPlayActionButton(
		FText::FromString(TEXT("OPEN TUNING PANEL")),
		FText::FromString(TEXT("Opens the visual fog, sky, post-process, and flashlight slider panel.")),
		FLinearColor(0.18f, 0.32f, 0.38f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnOpenAtmosphereConsoleClicked)));
	AddCommand(AtmosphereCommands, MenuPlayActionButton(
		FText::FromString(TEXT("AMBIENT SCARE")),
		FText::FromString(TEXT("Triggers a local ambient scare cue.")),
		FLinearColor(0.16f, 0.27f, 0.31f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnAtmosphereTestClicked, FString(TEXT("ambient")))));
	AddCommand(AtmosphereCommands, MenuPlayActionButton(
		FText::FromString(TEXT("MONSTER CHARGE")),
		FText::FromString(TEXT("Runs the atmosphere director's monster charge event.")),
		FLinearColor(0.28f, 0.12f, 0.11f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnAtmosphereTestClicked, FString(TEXT("charge")))));
	AddCommand(AtmosphereCommands, MenuPlayActionButton(
		FText::FromString(TEXT("BLACKOUT PULSE")),
		FText::FromString(TEXT("Cuts and restores local light pressure around the tester.")),
		FLinearColor(0.12f, 0.16f, 0.24f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnAtmosphereTestClicked, FString(TEXT("blackout")))));
	AddCommand(AtmosphereCommands, MenuPlayActionButton(
		FText::FromString(TEXT("CCTV GLITCH")),
		FText::FromString(TEXT("Fires the camera-feed distortion cue.")),
		FLinearColor(0.16f, 0.22f, 0.25f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnAtmosphereTestClicked, FString(TEXT("cctv")))));
	AddCommand(AtmosphereCommands, MenuPlayActionButton(
		FText::FromString(TEXT("FOOTSTEP NOISE")),
		FText::FromString(TEXT("Reports noise stimulus for atmosphere and bot systems.")),
		FLinearColor(0.18f, 0.22f, 0.16f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnAtmosphereTestClicked, FString(TEXT("footstep")))));
	AddCommand(AtmosphereCommands, MenuPlayActionButton(
		FText::FromString(TEXT("BOT MEMORY REPORT")),
		FText::FromString(TEXT("Shows bot/debug memory without closing this menu.")),
		FLinearColor(0.17f, 0.20f, 0.24f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnAtmosphereTestClicked, FString(TEXT("bots")))));

	TSharedRef<SVerticalBox> RoundCommands = SNew(SVerticalBox);
	AddCommand(RoundCommands, MenuPlayActionButton(
		FText::FromString(TEXT("REFRESH TEST ROUND")),
		FText::FromString(TEXT("Resets the active test layout and objectives.")),
		FLinearColor(0.22f, 0.34f, 0.18f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnPracticeRefreshClicked)));
	AddCommand(RoundCommands, MenuPlayActionButton(
		FText::FromString(TEXT("GRANT RESOURCES")),
		FText::FromString(TEXT("Gives tester points and train powerups.")),
		FLinearColor(0.24f, 0.30f, 0.15f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnTesterResourcesClicked)));
	AddCommand(RoundCommands, MenuPlayActionButton(
		FText::FromString(TEXT("OPEN TRAIN")),
		FText::FromString(TEXT("Moves to the subway intermission.")),
		FLinearColor(0.20f, 0.28f, 0.34f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnTesterTrainClicked)));
	AddCommand(RoundCommands, MenuPlayActionButton(
		FText::FromString(TEXT("NEXT TRAIN PHASE")),
		FText::FromString(TEXT("Advances the current train intermission phase.")),
		FLinearColor(0.18f, 0.26f, 0.32f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnTesterTrainPhaseClicked)));
	AddCommand(RoundCommands, MenuPlayActionButton(
		FText::FromString(TEXT("LOAD FINAL STATION")),
		FText::FromString(TEXT("Loads the Foggrounds final station test route.")),
		FLinearColor(0.18f, 0.31f, 0.24f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnTesterFinalStationClicked)));
	AddCommand(RoundCommands, MenuPlayActionButton(
		FText::FromString(TEXT("TRIGGER FINAL ESCAPE")),
		FText::FromString(TEXT("Unlocks the final escape event.")),
		FLinearColor(0.26f, 0.22f, 0.13f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnTesterFinalEscapeClicked)));
	AddCommand(RoundCommands, MenuPlayActionButton(
		FText::FromString(TEXT("FORCE FINAL RECAP")),
		FText::FromString(TEXT("Finishes the intermission and opens final recap.")),
		FLinearColor(0.24f, 0.20f, 0.28f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnTesterFinalRecapClicked)));

	TSharedRef<SVerticalBox> CommandList = SNew(SVerticalBox);
	MenuAddPlayDropdownSection(
		CommandList,
		FText::FromString(TEXT("Jumpscare Variants")),
		FText::FromString(TEXT("Force one scare, queue all variants, or print IDs.")),
		FLinearColor(0.36f, 0.12f, 0.10f, 1.0f),
		JumpscareCommands);
	MenuAddPlayDropdownSection(
		CommandList,
		FText::FromString(TEXT("Atmosphere Probes")),
		FText::FromString(TEXT("Fire director cues without typing console commands.")),
		FLinearColor(0.16f, 0.27f, 0.31f, 1.0f),
		AtmosphereCommands);
	MenuAddPlayDropdownSection(
		CommandList,
		FText::FromString(TEXT("Round And Train")),
		FText::FromString(TEXT("Refresh, resources, train phases, and final escape.")),
		FLinearColor(0.24f, 0.30f, 0.15f, 1.0f),
		RoundCommands);

	return SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.AllowAnimatedTransition(true)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.036f, 0.044f, 0.050f, 0.98f))
		.BodyBorderImage(WhiteBrush())
		.BodyBorderBackgroundColor(FLinearColor(0.014f, 0.018f, 0.022f, 0.98f))
		.HeaderPadding(FMargin(12.0f, 8.0f))
		.Padding(FMargin(10.0f))
		.HeaderContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.92f, 0.98f, 0.94f, 1.0f))
				.Text(FText::FromString(TEXT("Test Commands")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.70f, 1.0f))
				.Text(FText::FromString(TEXT("Expandable, scrollable replacement for console-only tester commands.")))
			]
		]
		.BodyContent()
		[
			SNew(SBox)
			.MaxDesiredHeight(460.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					CommandList
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildPracticePanel()
{
	const ABHPlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	const bool bTestMode = BHGS && BHGS->bTestMode;

	FString RoleName = TEXT("Unassigned");
	if (BHPS)
	{
		RoleName = BHPS->PlayerRole == EBHPlayerRole::Tester ? TEXT("Tester")
			: (BHPS->PlayerRole == EBHPlayerRole::Hunter ? TEXT("Teacher")
				: (BHPS->PlayerRole == EBHPlayerRole::FakeHunter ? TEXT("Hall Monitor") : TEXT("Survivor")));
	}

	const FString ModifierName = BHGS ? BHGS->GetRoundModifierText() : FString(TEXT("None"));
	const FString ModifierHint = BHGS ? BHGS->GetRoundModifierHint() : FString(TEXT("Standard map rules."));

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.050f, 0.056f, 0.034f, 0.95f))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.94f, 0.90f, 0.58f, 1.0f))
				.Text(FText::FromString(bTestMode ? TEXT("Test Round") : TEXT("Practice Lab")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.78f, 0.80f, 0.66f, 1.0f))
				.Text(FText::FromString(bTestMode
					? FString::Printf(TEXT("Role %s | All mechanics | Timer locked open"), *RoleName)
					: FString::Printf(TEXT("Role %s | Modifier %s | Timer locked open"), *RoleName, *ModifierName)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(9))
				.ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.58f, 0.92f))
				.Text(FText::FromString(ModifierHint))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBox)
				.Visibility(bTestMode ? EVisibility::Visible : EVisibility::Collapsed)
				[
					BuildTestCommandPanel()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility(bTestMode ? EVisibility::Collapsed : EVisibility::Visible)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnPracticeRoleClicked, EBHPlayerRole::Survivor)
					[
						SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Survivor")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnPracticeRoleClicked, EBHPlayerRole::Hunter)
					[
						SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Teacher")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnPracticeRoleClicked, EBHPlayerRole::FakeHunter)
					[
						SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Monitor")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(bTestMode ? EVisibility::Collapsed : EVisibility::Visible)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::None)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("None")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::LightsOut)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Lights")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::LoudFooting)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Loud")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::JammedDoors)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Doors")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::DeadCCTV)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("CCTV")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBHMenuButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::PanicSurge)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Panic")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 7.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(bTestMode ? EVisibility::Collapsed : EVisibility::Visible)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBHMenuButton)
					.ButtonColorAndOpacity(FLinearColor(0.20f, 0.30f, 0.17f, 1.0f))
					.ContentPadding(FMargin(8.0f, 5.0f))
					.OnClicked(this, &SBHMainMenu::OnPracticeRefreshClicked)
					[
						SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Refresh Tasks")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBHMenuButton)
					.ButtonColorAndOpacity(FLinearColor(0.36f, 0.12f, 0.10f, 1.0f))
					.ContentPadding(FMargin(8.0f, 5.0f))
					.OnClicked(this, &SBHMainMenu::OnPracticeJumpscareClicked)
					[
						SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Monster Scare")))
					]
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildRoleAssignmentPanel()
{
	ABHPlayerController* PC = PlayerController.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const bool bCanEditRoles = CanEditRoles();
	const ABHPlayerState* LocalPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	const bool bCanTargetScare = BHGS && PC && BHGS->RoundPhase == EBHRoundPhase::Hunt && (PC->HasAuthority() || (LocalPS && LocalPS->IsAliveHunter()));

	TSharedRef<SVerticalBox> PlayerRows = SNew(SVerticalBox);
	if (BHGS)
	{
		const UEnum* RoleEnum = StaticEnum<EBHPlayerRole>();
		for (APlayerState* RawPS : BHGS->PlayerArray)
		{
			ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
			if (!BHPS)
			{
				continue;
			}

			FString PlayerName = BHPS->GetPlayerName().IsEmpty() ? TEXT("Player") : BHPS->GetPlayerName();
			if (BHPS->IsABot())
			{
				PlayerName += TEXT(" [BOT]");
			}
			const FString DesiredRole = RoleEnum->GetNameStringByValue(static_cast<int64>(BHPS->DesiredRole));
			const bool bHasSpectatorPreference = BHPS->SpectatorRolePreference != EBHPlayerRole::Unassigned;
			const FString SpectatorPreference = bHasSpectatorPreference
				? FString::Printf(TEXT("  |  Pref: %s"), *MenuSpectatorRolePreferenceName(BHPS->SpectatorRolePreference))
				: FString();
			const FString ReadyStatus = BHPS->bReady ? TEXT("Ready") : TEXT("Waiting");
			const FLinearColor RowBackground = BHPS->bReady
				? FLinearColor(0.040f, 0.075f, 0.060f, 0.95f)
				: FLinearColor(0.082f, 0.061f, 0.036f, 0.95f);
			const FLinearColor ReadyTextColor = BHPS->bReady
				? FLinearColor(0.70f, 0.95f, 0.76f, 1.0f)
				: FLinearColor(0.96f, 0.76f, 0.45f, 1.0f);
			const bool bCanKickPlayer = bCanEditRoles && PC && LocalPS && BHPS != LocalPS && !BHPS->IsABot();
			const TWeakObjectPtr<APlayerState> WeakPS(BHPS);

			PlayerRows->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(RowBackground)
					.Padding(8.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(MenuFont(11, FName(TEXT("Bold"))))
							.ColorAndOpacity(ReadyTextColor)
							.Text(FText::FromString(FString::Printf(TEXT("%s  |  %s  |  Queued: %s%s"), *PlayerName, *ReadyStatus, *DesiredRole, *SpectatorPreference)))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(SBox)
							.Visibility((bCanEditRoles || bCanTargetScare) ? EVisibility::Visible : EVisibility::Collapsed)
							[
								SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SBHMenuButton)
								.IsEnabled(bCanEditRoles)
								.ContentPadding(FMargin(7.0f, 4.0f))
								.OnClicked(this, &SBHMainMenu::OnAssignRoleClicked, WeakPS, EBHPlayerRole::Hunter)
								[
									SNew(STextBlock)
									.Font(MenuFont(10, FName(TEXT("Bold"))))
									.Text(FText::FromString(TEXT("Teacher")))
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SBHMenuButton)
								.IsEnabled(bCanEditRoles)
								.ContentPadding(FMargin(7.0f, 4.0f))
								.OnClicked(this, &SBHMainMenu::OnAssignRoleClicked, WeakPS, EBHPlayerRole::Survivor)
								[
									SNew(STextBlock)
									.Font(MenuFont(10, FName(TEXT("Bold"))))
									.Text(FText::FromString(TEXT("Survivor")))
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SBHMenuButton)
								.IsEnabled(bCanEditRoles)
								.ContentPadding(FMargin(7.0f, 4.0f))
								.OnClicked(this, &SBHMainMenu::OnAssignRoleClicked, WeakPS, EBHPlayerRole::FakeHunter)
								[
									SNew(STextBlock)
									.Font(MenuFont(10, FName(TEXT("Bold"))))
									.Text(FText::FromString(TEXT("Monitor")))
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SBHMenuButton)
								.IsEnabled(bCanEditRoles)
								.ContentPadding(FMargin(7.0f, 4.0f))
								.OnClicked(this, &SBHMainMenu::OnAssignRoleClicked, WeakPS, EBHPlayerRole::Unassigned)
								[
									SNew(STextBlock)
									.Font(MenuFont(10, FName(TEXT("Bold"))))
									.Text(FText::FromString(TEXT("Auto")))
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SBHMenuButton)
								.Visibility((bCanEditRoles && bHasSpectatorPreference) ? EVisibility::Visible : EVisibility::Collapsed)
								.ContentPadding(FMargin(7.0f, 4.0f))
								.OnClicked(this, &SBHMainMenu::OnAssignRoleClicked, WeakPS, BHPS->SpectatorRolePreference)
								[
									SNew(STextBlock)
									.Font(MenuFont(10, FName(TEXT("Bold"))))
									.Text(FText::FromString(TEXT("Use Pref")))
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SBHMenuButton)
								.Visibility(bCanKickPlayer ? EVisibility::Visible : EVisibility::Collapsed)
								.ButtonColorAndOpacity(FLinearColor(0.44f, 0.12f, 0.10f, 1.0f))
								.ContentPadding(FMargin(7.0f, 4.0f))
								.OnClicked(this, &SBHMainMenu::OnKickPlayerClicked, WeakPS)
								[
									SNew(STextBlock)
									.Font(MenuFont(10, FName(TEXT("Bold"))))
									.Text(FText::FromString(TEXT("Kick")))
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBHMenuButton)
								.IsEnabled(bCanTargetScare)
								.ButtonColorAndOpacity(FLinearColor(0.34f, 0.10f, 0.12f, 1.0f))
								.ContentPadding(FMargin(7.0f, 4.0f))
								.OnClicked(this, &SBHMainMenu::OnTargetScareClicked, WeakPS)
								[
									SNew(STextBlock)
									.Font(MenuFont(10, FName(TEXT("Bold"))))
									.Text(FText::FromString(TEXT("Scare")))
								]
							]
							]
						]
					]
				];
		}
	}

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.035f, 0.043f, 0.046f, 0.95f))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
				.Text(FText::FromString(TEXT("Lobby Role Assignment")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.70f, 1.0f))
				.Text(bCanEditRoles
					? FText::FromString(TEXT("Host can queue roles while the lobby is open. Every connected player must ready; kick blockers from this roster if needed. If nobody is Teacher, the first player becomes Teacher."))
					: FText::FromString(TEXT("Only the host machine can edit roles, and only while the round is in the lobby.")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				PlayerRows
			]
		];
}

FString SBHMainMenu::ResolveLocalAddress()
{
	return FBHNetworkSupport::ResolveLocalJoinAddress(MenuDefaultGamePort);
}

FString SBHMainMenu::NormalizeAddress(FString Address)
{
	return FBHNetworkSupport::NormalizeJoinAddress(Address);
}
