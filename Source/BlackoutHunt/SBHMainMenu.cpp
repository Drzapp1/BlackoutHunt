#include "SBHMainMenu.h"
#include "Animation/AnimSequence.h"
#include "BHAccountSubsystem.h"
#include "BHGameSettings.h"
#include "BHGameInstance.h"
#include "BHGameState.h"
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
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Layout/Visibility.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr int32 MenuDefaultGamePort = 7777;
	constexpr float MenuAvatarPreviewModelScale = 0.56f;
	constexpr float MenuAvatarPreviewCameraDistance = 212.0f;
	constexpr float MenuAvatarPreviewCameraHeight = 46.0f;
	constexpr float MenuAvatarPreviewCameraFov = 34.0f;

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
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

	FSlateFontInfo MenuFont(const int32 Size, const FName Typeface = FName(TEXT("Regular")))
	{
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}

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

		return SNew(SButton)
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
		static const TCHAR* SurvivorNames[] = {
			TEXT("Casual"),
			TEXT("Worker"),
			TEXT("Adventurer"),
			TEXT("Farmer"),
			TEXT("Beach"),
			TEXT("Punk"),
			TEXT("Suit"),
			TEXT("Hazmat")
		};

		return SurvivorNames[FMath::Abs(AvatarIndex) % UE_ARRAY_COUNT(SurvivorNames)];
	}

	const TCHAR* MenuAvatarHeadwearName(int32 HeadwearIndex)
	{
		static const TCHAR* HeadwearNames[] = {
			TEXT("None"),
			TEXT("Cap"),
			TEXT("Glasses"),
			TEXT("Beanie"),
			TEXT("Visor")
		};

		return HeadwearNames[FMath::Abs(HeadwearIndex) % UE_ARRAY_COUNT(HeadwearNames)];
	}

	const TCHAR* MenuAvatarGearName(int32 GearIndex)
	{
		static const TCHAR* GearNames[] = {
			TEXT("None"),
			TEXT("Backpack"),
			TEXT("Flashlight"),
			TEXT("Badge"),
			TEXT("Radio")
		};

		return GearNames[FMath::Abs(GearIndex) % UE_ARRAY_COUNT(GearNames)];
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
					[
						BuildMenuTabButton(EBHMainMenuTab::Settings, FText::FromString(TEXT("Settings")))
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(MainTabSwitcher, SWidgetSwitcher)
					.WidgetIndex(static_cast<int32>(ActiveMenuTab))
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

FReply SBHMainMenu::OnToggleStartCredentialsClicked()
{
	bShowStartCredentials = !bShowStartCredentials;
	return FReply::Handled();
}

bool SBHMainMenu::SupportsKeyboardFocus() const
{
	return true;
}

FReply SBHMainMenu::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		PlayMenuSelectionSound();
	}

	return SCompoundWidget::OnPreviewMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SBHMainMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
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

	if (InKeyEvent.GetKey() == EKeys::B)
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

FReply SBHMainMenu::OnMenuTabClicked(EBHMainMenuTab NewTab)
{
	ActiveMenuTab = NewTab;
	if (MainTabSwitcher.IsValid())
	{
		MainTabSwitcher->SetActiveWidgetIndex(static_cast<int32>(NewTab));
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

FSlateColor SBHMainMenu::GetMenuTabColor(EBHMainMenuTab Tab) const
{
	if (ActiveMenuTab == Tab)
	{
		return FSlateColor(FLinearColor(0.10f, 0.34f, 0.31f, 1.0f));
	}

	return FSlateColor(FLinearColor(0.045f, 0.052f, 0.060f, 0.96f));
}

FSlateColor SBHMainMenu::GetMenuTabTextColor(EBHMainMenuTab Tab) const
{
	return ActiveMenuTab == Tab
		? FSlateColor(FLinearColor(0.90f, 1.0f, 0.96f, 1.0f))
		: FSlateColor(FLinearColor(0.62f, 0.70f, 0.70f, 1.0f));
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

EVisibility SBHMainMenu::GetStartCredentialsVisibility() const
{
	return bShowStartCredentials ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SBHMainMenu::GetStartBackgroundBrush() const
{
	return StartBackgroundTexture.IsValid() ? &StartBackgroundBrush : &StartBackgroundFallbackBrush;
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
					SNew(SButton)
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
					SNew(SButton)
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
	return SNew(SButton)
		.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetMenuTabColor, Tab)))
		.ContentPadding(FMargin(16.0f, 8.0f))
		.OnClicked(this, &SBHMainMenu::OnMenuTabClicked, Tab)
		[
			SNew(STextBlock)
			.Font(MenuFont(12, FName(TEXT("Bold"))))
			.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SBHMainMenu::GetMenuTabTextColor, Tab)))
			.Text(Label)
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
					.Text(FText::FromString(TEXT("HOST / IP / CODE")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SAssignNew(JoinHostTextBox, SEditableTextBox)
					.Text(FText::FromString(MenuExtractHost(SuggestedAddress)))
					.HintText(FText::FromString(TEXT("IP, hostname, tunnel address, or join code")))
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
						.Text(FText::AsNumber(MenuExtractPort(SuggestedAddress)))
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
		FText::FromString(TEXT("COPY JOIN CODE")),
		FText::FromString(TEXT("Copies the current host invite for students or friends.")),
		FLinearColor(0.18f, 0.32f, 0.30f, 1.0f),
		FOnClicked::CreateSP(this, &SBHMainMenu::OnCopyJoinInviteClicked)));
	AddAction(Panel, MenuPlayActionButton(
		FText::FromString(TEXT("JOIN GAME")),
		FText::FromString(TEXT("Connects to the address above.")),
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

	if (bInGame)
	{
		const bool bHostAdmin = CanEditRoles() || bPracticeMode || bTestMode;
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
			FText::FromString(TEXT("LIVE CLASSROOM")),
			FText::FromString(TEXT("LAN host, projector board, all-ready roster, Physics defaults.")),
			FLinearColor(0.16f, 0.42f, 0.37f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostLiveClassroomClicked)));

		TSharedRef<SVerticalBox> QuickStartActions = SNew(SVerticalBox);
		AddAction(QuickStartActions, MenuPlayActionButton(
			FText::FromString(TEXT("PRACTICE LAB")),
			FText::FromString(TEXT("Solo role and modifier testing.")),
			FLinearColor(0.32f, 0.36f, 0.16f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostPracticeClicked)));
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
			FText::FromString(TEXT("Practice And Tests")),
			FText::FromString(TEXT("Solo practice and test modes.")),
			FLinearColor(0.32f, 0.36f, 0.16f, 1.0f),
			QuickStartActions);

		TSharedRef<SVerticalBox> BotActions = SNew(SVerticalBox);
		AddAction(BotActions, MenuPlayActionButton(
			FText::FromString(TEXT("BOT FACILITY")),
			FText::FromString(TEXT("Offline normal round with server bots.")),
			FLinearColor(0.24f, 0.33f, 0.29f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostBotClicked)));
		AddAction(BotActions, MenuPlayActionButton(
			FText::FromString(TEXT("BOT SUBSTATION")),
			FText::FromString(TEXT("Offline normal round on Substation.")),
			FLinearColor(0.20f, 0.28f, 0.38f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostBotSubstationClicked)));
		AddAction(BotActions, MenuPlayActionButton(
			FText::FromString(TEXT("BOT FOGGROUNDS")),
			FText::FromString(TEXT("Offline foggy perimeter round with server bots.")),
			FLinearColor(0.18f, 0.31f, 0.25f, 1.0f),
			FOnClicked::CreateSP(this, &SBHMainMenu::OnHostBotFoggroundsClicked)));
		MenuAddPlayDropdownSection(
			ActionList,
			FText::FromString(TEXT("Bots")),
			FText::FromString(TEXT("Offline bot rounds by map.")),
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
			FText::FromString(TEXT("Join Address")),
			FText::FromString(TEXT("Direct-IP, tunnel address, or invite code.")),
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
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->HostLiveClassroomForMenu(Message);
		StatusText = FText::FromString(Message);
		return FReply::Handled();
	}

	StatusText = FText::FromString(TEXT("No local player controller was available."));
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
			JoinPortTextBox->SetText(FText::AsNumber(MenuExtractPort(SuggestedAddress)));
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
	const FString Address = EnteredAddress.IsEmpty() ? ResolvePreferredAddress() : EnteredAddress;
	const FString NormalizedAddress = FBHNetworkSupport::NormalizeJoinAddress(Address);
	const FString InviteCode = FBHNetworkSupport::MakeJoinInviteCode(NormalizedAddress);
	if (InviteCode.IsEmpty() || NormalizedAddress.IsEmpty())
	{
		StatusText = FText::FromString(TEXT("Enter a valid host/IP, tunnel address, or join code before copying."));
		return FReply::Handled();
	}

	FPlatformApplicationMisc::ClipboardCopy(*InviteCode);
	StatusText = FText::FromString(FString::Printf(
		TEXT("Copied join code for %s. Players paste it into HOST / IP / CODE and press JOIN GAME."),
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
		StatusText = FText::FromString(TEXT("Enter a valid host/IP, tunnel address, or join code first. Examples: 192.168.1.23 port 7777, host.example.com port 7777, a tunnel allocation address and port, or a copied join code."));
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
	SetJoinAddressFields(Endpoint);
	return OnJoinClicked();
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

FReply SBHMainMenu::OnResumeClicked()
{
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

FReply SBHMainMenu::OnTargetScareClicked(TWeakObjectPtr<APlayerState> TargetPlayerState)
{
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

FReply SBHMainMenu::OnAvatarGearClicked(int32 GearIndex)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		FString Message;
		PC->SetAvatarGearForMenu(GearIndex, Message);
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

FReply SBHMainMenu::OnAssignRoleClicked(TWeakObjectPtr<APlayerState> TargetPlayerState, EBHPlayerRole DesiredRole)
{
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->SetDesiredRole(TargetPlayerState.Get(), DesiredRole);
		StatusText = FText::FromString(TEXT("Role assignment sent. Reopen this menu to refresh the list."));
	}

	return FReply::Handled();
}

FReply SBHMainMenu::OnKickPlayerClicked(TWeakObjectPtr<APlayerState> TargetPlayerState)
{
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
	if (ABHPlayerController* PC = PlayerController.Get())
	{
		PC->ReturnToMainMenu();
	}

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
	return FText::FromString(FString::Printf(TEXT("Avatar %d | Role %s | Color %.2f %.2f %.2f"),
		BHPS->AvatarIndex + 1,
		*RoleEnum->GetNameStringByValue(static_cast<int64>(BHPS->PlayerRole)),
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

	return FText::FromString(FString::Printf(TEXT("Round seed %d | %s | Next level: %s%s"),
		BHGS->RoundSeed,
		*BHGS->ObjectiveText,
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

FText SBHMainMenu::GetAvatarSummaryText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return FText::FromString(TEXT("Look 1 | Shirt 1"));
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

	return FText::FromString(FString::Printf(TEXT("%s | Shirt %d | %s"),
		MenuAvatarLookName(BHPS->AvatarIndex),
		MenuNearestAvatarColorIndex(BHPS->AvatarColor) + 1,
		*RoleName));
}

FText SBHMainMenu::GetAvatarModelNameText() const
{
	const ABHPlayerController* PC = PlayerController.Get();
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	const int32 ShirtIndex = BHPS ? MenuNearestAvatarColorIndex(BHPS->AvatarColor) + 1 : 1;
	return FText::FromString(FString::Printf(TEXT("%s | Shirt %d"),
		MenuAvatarModelName(BHPS),
		ShirtIndex));
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
		JoinPortTextBox->SetText(FText::AsNumber(MenuExtractPort(NormalizedAddress)));
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
	UStaticMeshComponent* GearComponent = CreatePreviewPart(TEXT("PreviewGear"));
	UStaticMeshComponent* GearAccentComponent = CreatePreviewPart(TEXT("PreviewGearAccent"));
	UStaticMeshComponent* GearLeftStrapComponent = CreatePreviewPart(TEXT("PreviewGearLeftStrap"));
	UStaticMeshComponent* GearRightStrapComponent = CreatePreviewPart(TEXT("PreviewGearRightStrap"));
	UStaticMeshComponent* GearDetailComponent = CreatePreviewPart(TEXT("PreviewGearDetail"));

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
	AvatarPreviewGearComponent = GearComponent;
	AvatarPreviewGearAccentComponent = GearAccentComponent;
	AvatarPreviewGearLeftStrapComponent = GearLeftStrapComponent;
	AvatarPreviewGearRightStrapComponent = GearRightStrapComponent;
	AvatarPreviewGearDetailComponent = GearDetailComponent;
	AvatarPreviewCaptureComponent = CaptureComponent;
	LastAvatarPreviewMeshPath.Reset();
	LastAvatarPreviewColor = FLinearColor::Transparent;
	LastAvatarPreviewHeadwearIndex = INDEX_NONE;
	LastAvatarPreviewGearIndex = INDEX_NONE;
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
	UStaticMeshComponent* GearComponent = AvatarPreviewGearComponent.Get();
	UStaticMeshComponent* GearAccentComponent = AvatarPreviewGearAccentComponent.Get();
	UStaticMeshComponent* GearLeftStrapComponent = AvatarPreviewGearLeftStrapComponent.Get();
	UStaticMeshComponent* GearRightStrapComponent = AvatarPreviewGearRightStrapComponent.Get();
	UStaticMeshComponent* GearDetailComponent = AvatarPreviewGearDetailComponent.Get();
	constexpr int32 HeadwearIndex = 0;
	constexpr int32 GearIndex = 0;
	const bool bAccessoryChanged = HeadwearIndex != LastAvatarPreviewHeadwearIndex
		|| GearIndex != LastAvatarPreviewGearIndex
		|| bPreviewColorChanged;

	if (bAccessoryChanged)
	{
		UStaticMesh* AccessoryCube = MenuPreviewCubeMesh();
		UStaticMesh* AccessoryCylinder = MenuPreviewCylinderMesh();
		UStaticMesh* AccessorySphere = MenuPreviewSphereMesh();
		UMaterialInterface* AccessoryBasic = MenuPreviewBasicMaterial();
		UMaterialInterface* AccessoryBlack = MenuPreviewMaterial(TEXT("Black"));
		UMaterialInterface* AccessoryBrown = MenuPreviewMaterial(TEXT("Brown2"));
		UMaterialInterface* AccessoryDarkBrown = MenuPreviewMaterial(TEXT("DarkBrown"));
		UMaterialInterface* AccessoryMetal = MenuPreviewMaterial(TEXT("Metal"));
		UMaterialInterface* AccessoryGold = MenuPreviewMaterial(TEXT("Gold"));
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

		UStaticMeshComponent* GearParts[] = {
			GearComponent,
			GearAccentComponent,
			GearLeftStrapComponent,
			GearRightStrapComponent,
			GearDetailComponent
		};
		for (UStaticMeshComponent* Part : GearParts)
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

		switch (GearIndex)
		{
		case 1:
			MenuApplyPreviewPiece(GearComponent, AccessoryCube, AccessoryBrown, FVector(-26.0f, 0.0f, 12.0f), FRotator::ZeroRotator, FVector(0.17f, 0.31f, 0.42f), true);
			MenuApplyPreviewPiece(GearAccentComponent, AccessoryCube, AccessoryDarkBrown, FVector(-10.0f, 0.0f, 34.0f), FRotator::ZeroRotator, FVector(0.055f, 0.32f, 0.075f), true);
			MenuApplyPreviewPiece(GearLeftStrapComponent, AccessoryCube, AccessoryBlack, FVector(9.0f, -15.0f, 18.0f), FRotator::ZeroRotator, FVector(0.035f, 0.022f, 0.32f), true);
			MenuApplyPreviewPiece(GearRightStrapComponent, AccessoryCube, AccessoryBlack, FVector(9.0f, 15.0f, 18.0f), FRotator::ZeroRotator, FVector(0.035f, 0.022f, 0.32f), true);
			MenuApplyPreviewPiece(GearDetailComponent, AccessoryCube, AccessoryGold, FVector(14.0f, 0.0f, 16.0f), FRotator::ZeroRotator, FVector(0.020f, 0.090f, 0.10f), true);
			break;
		case 2:
			MenuApplyPreviewPiece(GearComponent, AccessoryCylinder, AccessoryMetal, FVector(30.0f, 16.0f, 16.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.055f, 0.055f, 0.24f), true);
			MenuApplyPreviewPiece(GearAccentComponent, AccessoryCylinder, AccessoryLight, FVector(42.0f, 16.0f, 16.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.060f, 0.060f, 0.035f), true);
			MenuApplyPreviewPiece(GearLeftStrapComponent, AccessoryCube, AccessoryBlack, FVector(24.0f, 16.0f, 5.0f), FRotator::ZeroRotator, FVector(0.030f, 0.050f, 0.090f), true);
			break;
		case 3:
			MenuApplyPreviewPiece(GearComponent, AccessoryCylinder, AccessoryGold, FVector(28.0f, -16.0f, 25.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.075f, 0.075f, 0.020f), true);
			MenuApplyPreviewPiece(GearAccentComponent, AccessoryCube, AccessoryWhite, FVector(30.0f, -16.0f, 25.0f), FRotator::ZeroRotator, FVector(0.018f, 0.045f, 0.055f), true);
			MenuApplyPreviewPiece(GearDetailComponent, AccessoryCube, AccessoryBlack, FVector(23.0f, -16.0f, 25.0f), FRotator::ZeroRotator, FVector(0.018f, 0.085f, 0.095f), true);
			break;
		case 4:
			MenuApplyPreviewPiece(GearComponent, AccessoryCube, AccessoryBlack, FVector(-20.0f, 20.0f, 26.0f), FRotator::ZeroRotator, FVector(0.060f, 0.14f, 0.20f), true);
			MenuApplyPreviewPiece(GearAccentComponent, AccessoryCube, AccessoryMetal, FVector(-12.0f, 20.0f, 33.0f), FRotator::ZeroRotator, FVector(0.020f, 0.12f, 0.025f), true);
			MenuApplyPreviewPiece(GearLeftStrapComponent, AccessoryCube, AccessoryMetal, FVector(-8.0f, 20.0f, 45.0f), FRotator(0.0f, 0.0f, -12.0f), FVector(0.012f, 0.018f, 0.18f), true);
			MenuApplyPreviewPiece(GearDetailComponent, AccessoryCube, AccessoryLight, FVector(-9.0f, 20.0f, 24.0f), FRotator::ZeroRotator, FVector(0.012f, 0.10f, 0.040f), true);
			break;
		default:
			break;
		}
	}

	LastAvatarPreviewHeadwearIndex = HeadwearIndex;
	LastAvatarPreviewGearIndex = GearIndex;
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
	AvatarPreviewGearComponent.Reset();
	AvatarPreviewGearAccentComponent.Reset();
	AvatarPreviewGearLeftStrapComponent.Reset();
	AvatarPreviewGearRightStrapComponent.Reset();
	AvatarPreviewGearDetailComponent.Reset();
	AvatarPreviewCaptureComponent.Reset();
	AvatarPreviewRenderTarget.Reset();
	AvatarPreviewBrush.SetResourceObject(nullptr);
	LastAvatarPreviewMeshPath.Reset();
	LastAvatarPreviewColor = FLinearColor::Transparent;
	LastAvatarPreviewHeadwearIndex = INDEX_NONE;
	LastAvatarPreviewGearIndex = INDEX_NONE;
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
					SNew(SButton)
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
					SNew(SButton)
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
					SNew(SButton)
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
					SNew(SButton)
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
					SNew(SButton)
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
				SNew(SButton)
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
				SNew(SButton)
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
				SNew(SButton)
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
				SNew(SButton)
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
	for (int32 Index = 0; Index < 8; ++Index)
	{
		LookButtons->AddSlot(Index % 2, Index / 2)
			.Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				SNew(SBox)
				.WidthOverride(98.0f)
				.HeightOverride(28.0f)
				[
					SNew(SButton)
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBHMainMenu::OnAvatarPresetClicked, Index)
					[
						SNew(STextBlock)
						.Font(MenuFont(9, FName(TEXT("Bold"))))
						.Text(FText::FromString(MenuAvatarLookName(Index)))
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
				SNew(SButton)
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
	const UEnum* ModifierEnum = StaticEnum<EBHRoundModifier>();
	const FString ModifierName = BHGS && ModifierEnum
		? ModifierEnum->GetNameStringByValue(static_cast<int64>(BHGS->RoundModifier))
		: FString(TEXT("None"));
	const UEnum* BotDifficultyEnum = StaticEnum<EBHBotDifficulty>();
	const FString BotDifficultyName = BHGS && BotDifficultyEnum
		? BotDifficultyEnum->GetNameStringByValue(static_cast<int64>(BHGS->BotDifficulty))
		: FString(TEXT("Normal"));
	const UEnum* FogPresetEnum = StaticEnum<EBHFogPreset>();
	const FString FogPresetName = BHGS && FogPresetEnum
		? FogPresetEnum->GetNameStringByValue(static_cast<int64>(BHGS->NextFogPreset))
		: FString(TEXT("Heavy"));
	const FString OptionsText = BHGS
		? FString::Printf(TEXT("Teachers %d | Objectives %d | Bots %d %s | Infection %s | Pace %s | Fog %s %s | Modifier %s | Side tasks %d/%d"),
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
			BHGS->SideObjectivesRequired)
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
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
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
						SNew(SButton)
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
						SNew(SButton)
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
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
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
					SNew(SButton)
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
					SNew(SButton)
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
					SNew(SButton)
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
					SNew(SButton)
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
					SNew(SButton).IsEnabled(bCanVote).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogVoteClicked, EBHFogPreset::Light)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Light")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanVote).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogVoteClicked, EBHFogPreset::Heavy)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Heavy")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanVote).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogVoteClicked, EBHFogPreset::Extreme)
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
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogOverrideClicked, EBHFogPreset::Light)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Light")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogOverrideClicked, EBHFogPreset::Heavy)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Heavy")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogOverrideClicked, EBHFogPreset::Extreme)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Extreme")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnFogVoteModeClicked)
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
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnHunterCountClicked, 1)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("1")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnHunterCountClicked, 2)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("2")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnHunterCountClicked, 3)
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
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnObjectiveIntensityClicked, 0)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("0")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnObjectiveIntensityClicked, 1)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("1")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnObjectiveIntensityClicked, 2)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("2")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnObjectiveIntensityClicked, 3)
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
					.Text(FText::FromString(TEXT("Bots")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotCountClicked, 0)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("0")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotCountClicked, 5)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("5")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotCountClicked, 11)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("11")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotDifficultyClicked, EBHBotDifficulty::Easy)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Easy")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotDifficultyClicked, EBHBotDifficulty::Normal)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Normal")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton).IsEnabled(bCanEditHost).ContentPadding(FMargin(7.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnBotDifficultyClicked, EBHBotDifficulty::Hard)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Hard")))
					]
				]
			]
		];
}

TSharedRef<SWidget> SBHMainMenu::BuildRevisionControlsPanel()
{
	ABHPlayerController* PC = PlayerController.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;

	const int32 ForcesBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::ForcesAndMotion);
	const int32 ElectricityBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Electricity);
	const int32 WavesBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Waves);
	const int32 EnergyBit = FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Energy);
	const int32 AllTopicsMask = ForcesBit | ElectricityBit | WavesBit | EnergyBit;
	const int32 CurrentTopicMask = BHGS ? BHGS->RevisionTopicMask : AllTopicsMask;
	const EBHRevisionDifficultyMix CurrentDifficultyMix = BHGS ? BHGS->RevisionDifficultyMix : EBHRevisionDifficultyMix::Adaptive;
	const float CurrentClassThreshold = BHGS ? BHGS->RevisionClassThreshold : 70.0f;
	const float CurrentIndividualThreshold = BHGS ? BHGS->RevisionIndividualThreshold : 50.0f;
	const int32 CurrentScareIntensity = BHGS ? BHGS->RevisionScareIntensity : 2;
	const bool bRevisionSession = BHGS && BHGS->bRevisionMode;
	const bool bCanEditRevision = PC && World && PC->IsLocalController() && World->GetNetMode() == NM_ListenServer && bRevisionSession;

	const FString Summary = bRevisionSession
		? FString::Printf(TEXT("Focus %s | Complexity %s | Targets %.0f/%.0f | Scare %s"),
			*MenuRevisionTopicMaskText(CurrentTopicMask),
			*MenuRevisionDifficultyMixText(CurrentDifficultyMix),
			CurrentClassThreshold,
			CurrentIndividualThreshold,
			*MenuRevisionScareIntensityText(CurrentScareIntensity))
		: FString(TEXT("Host Live Classroom to enable question controls."));

	auto AddOptionButton = [this, bCanEditRevision](const TSharedRef<SHorizontalBox>& Row, const FString& Label, bool bSelected, const FOnClicked& OnClicked)
	{
		Row->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 5.0f, 0.0f)
			[
				SNew(SButton)
				.IsEnabled(bCanEditRevision)
				.ButtonColorAndOpacity(bSelected ? FLinearColor(0.18f, 0.44f, 0.38f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.17f, 1.0f))
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
	AddOptionButton(TopicRow, TEXT("All"), (CurrentTopicMask & AllTopicsMask) == AllTopicsMask, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, AllTopicsMask));
	AddOptionButton(TopicRow, TEXT("Forces"), (CurrentTopicMask & ForcesBit) != 0 && (CurrentTopicMask & AllTopicsMask) != AllTopicsMask, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, ForcesBit));
	AddOptionButton(TopicRow, TEXT("Electricity"), (CurrentTopicMask & ElectricityBit) != 0 && (CurrentTopicMask & AllTopicsMask) != AllTopicsMask, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, ElectricityBit));
	AddOptionButton(TopicRow, TEXT("Waves"), (CurrentTopicMask & WavesBit) != 0 && (CurrentTopicMask & AllTopicsMask) != AllTopicsMask, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, WavesBit));
	AddOptionButton(TopicRow, TEXT("Energy"), (CurrentTopicMask & EnergyBit) != 0 && (CurrentTopicMask & AllTopicsMask) != AllTopicsMask, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionTopicsClicked, EnergyBit));

	TSharedRef<SHorizontalBox> DifficultyRow = SNew(SHorizontalBox);
	AddOptionButton(DifficultyRow, TEXT("Easy"), CurrentDifficultyMix == EBHRevisionDifficultyMix::Easy, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionDifficultyClicked, EBHRevisionDifficultyMix::Easy));
	AddOptionButton(DifficultyRow, TEXT("Balanced"), CurrentDifficultyMix == EBHRevisionDifficultyMix::Balanced, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionDifficultyClicked, EBHRevisionDifficultyMix::Balanced));
	AddOptionButton(DifficultyRow, TEXT("Adaptive"), CurrentDifficultyMix == EBHRevisionDifficultyMix::Adaptive, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionDifficultyClicked, EBHRevisionDifficultyMix::Adaptive));
	AddOptionButton(DifficultyRow, TEXT("Hard"), CurrentDifficultyMix == EBHRevisionDifficultyMix::Hard, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionDifficultyClicked, EBHRevisionDifficultyMix::Hard));

	const int32 RoundedClassThreshold = FMath::RoundToInt(CurrentClassThreshold);
	const int32 RoundedIndividualThreshold = FMath::RoundToInt(CurrentIndividualThreshold);
	TSharedRef<SHorizontalBox> ThresholdRow = SNew(SHorizontalBox);
	AddOptionButton(ThresholdRow, TEXT("Support 60/40"), RoundedClassThreshold == 60 && RoundedIndividualThreshold == 40, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionThresholdClicked, 60, 40));
	AddOptionButton(ThresholdRow, TEXT("Default 70/50"), RoundedClassThreshold == 70 && RoundedIndividualThreshold == 50, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionThresholdClicked, 70, 50));
	AddOptionButton(ThresholdRow, TEXT("Stretch 80/60"), RoundedClassThreshold == 80 && RoundedIndividualThreshold == 60, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionThresholdClicked, 80, 60));

	TSharedRef<SHorizontalBox> ScareRow = SNew(SHorizontalBox);
	AddOptionButton(ScareRow, TEXT("Off"), CurrentScareIntensity == 0, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionScareIntensityClicked, 0));
	AddOptionButton(ScareRow, TEXT("Low"), CurrentScareIntensity == 1, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionScareIntensityClicked, 1));
	AddOptionButton(ScareRow, TEXT("Horror"), CurrentScareIntensity == 2, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionScareIntensityClicked, 2));
	AddOptionButton(ScareRow, TEXT("Chaos"), CurrentScareIntensity == 3, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionScareIntensityClicked, 3));

	TSharedRef<SHorizontalBox> AdminRow = SNew(SHorizontalBox);
	AddOptionButton(AdminRow, TEXT("Show Status"), false, FOnClicked::CreateSP(this, &SBHMainMenu::OnRevisionStatusClicked));
	AddOptionButton(AdminRow, TEXT("Force 60s Review"), false, FOnClicked::CreateSP(this, &SBHMainMenu::OnForceReviewClicked));

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
				.ColorAndOpacity(bCanEditRevision ? FLinearColor(0.62f, 0.74f, 0.72f, 1.0f) : FLinearColor(0.86f, 0.62f, 0.40f, 1.0f))
				.Text(FText::FromString(Summary))
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

TSharedRef<SWidget> SBHMainMenu::BuildClassroomPanel()
{
	ABHPlayerController* PC = PlayerController.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const bool bShowLobbyRoster = IsInNetworkedGame() && !IsPracticeMode() && !IsTestMode();
	const bool bShowRevisionControls = BHGS && BHGS->bRevisionMode && CanEditRoles();

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
						SNew(SButton)
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
						SNew(SButton)
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
			SNew(SBox)
			.Visibility(bShowRevisionControls ? EVisibility::Visible : EVisibility::Collapsed)
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
				SNew(SButton)
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
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(CanOpenClassroomBoard()
					? FLinearColor(0.62f, 0.72f, 0.72f, 1.0f)
					: FLinearColor(0.86f, 0.58f, 0.34f, 1.0f))
				.Text(CanOpenClassroomBoard()
					? FText::FromString(TEXT("Separate window is available on the host machine."))
					: FText::FromString(TEXT("Host a listen-server session to enable the projector window.")))
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
			.Text(FText::FromString(TEXT("Current keyboard and mouse bindings.")))
		];

	AddSectionTitle(TEXT("Movement"));
	AddControl(TEXT("Move Forward"), MenuDescribeAxisBinding(FName(TEXT("MoveForward")), 1.0f, TEXT("W")), TEXT("Walk forward."));
	AddControl(TEXT("Move Back"), MenuDescribeAxisBinding(FName(TEXT("MoveForward")), -1.0f, TEXT("S")), TEXT("Back away."));
	AddControl(TEXT("Move Right"), MenuDescribeAxisBinding(FName(TEXT("MoveRight")), 1.0f, TEXT("D")), TEXT("Strafe right."));
	AddControl(TEXT("Move Left"), MenuDescribeAxisBinding(FName(TEXT("MoveRight")), -1.0f, TEXT("A")), TEXT("Strafe left."));
	AddControl(TEXT("Look"), LookBinding, TEXT("Turn and look up or down."));
	AddControl(TEXT("Sprint"), MenuDescribeActionBinding(FName(TEXT("Sprint")), TEXT("Left Shift")), TEXT("Hold while moving."));
	AddControl(TEXT("Crouch"), MenuDescribeActionBinding(FName(TEXT("Crouch")), TEXT("Left Ctrl")), TEXT("Hold to stay low."));

	AddSectionTitle(TEXT("Survivor"));
	AddControl(TEXT("Interact"), MenuDescribeActionBinding(FName(TEXT("Interact")), TEXT("E")), TEXT("Use doors, lockers, breakers, exits, and stations."));
	AddControl(TEXT("Flashlight"), MenuDescribeActionBinding(FName(TEXT("Flashlight")), TEXT("F")), TEXT("Toggle the flashlight."));
	AddControl(TEXT("Decoy"), MenuDescribeActionBinding(FName(TEXT("Decoy")), TEXT("G")), TEXT("Drop a noise decoy."));
	AddControl(TEXT("Scan"), MenuDescribeActionBinding(FName(TEXT("Scan")), TEXT("Q")), TEXT("Pulse the scanner."));
	AddControl(TEXT("Map"), MenuDescribeActionBinding(FName(TEXT("Map")), TEXT("M / I")), TEXT("Raise or lower the HUD map."));
	AddControl(TEXT("Answer Choice"), TEXT("1-4 / Numpad 1-4"), TEXT("Submit classroom station answers."));

	AddSectionTitle(TEXT("Hunter And Session"));
	AddControl(TEXT("Capture"), MenuDescribeActionBinding(FName(TEXT("Capture")), TEXT("Left Mouse Button")), TEXT("Attempt a close-range capture."));
	AddControl(TEXT("Hunter Power"), MenuDescribeActionBinding(FName(TEXT("HunterPower")), TEXT("R")), TEXT("Use the active hunter ability."));
	AddControl(TEXT("Ready"), MenuDescribeActionBinding(FName(TEXT("Ready")), TEXT("Enter")), TEXT("Toggle lobby readiness."));
	AddControl(TEXT("Menu"), MenuDescribeActionBinding(FName(TEXT("Menu")), TEXT("Escape")), TEXT("Open or close this menu."));
	AddControl(TEXT("Classroom Board"), MenuDescribeActionBinding(FName(TEXT("ClassroomBoard")), TEXT("B")), TEXT("Open the projector board on the host."));
	AddControl(TEXT("Cycle Reticle"), MenuDescribeActionBinding(FName(TEXT("CycleCrosshair")), TEXT("V")), TEXT("Change the HUD reticle style."));
	AddControl(TEXT("Force Start"), MenuDescribeActionBinding(FName(TEXT("ForceStartRound")), TEXT("F10")), TEXT("Host-only lobby start shortcut."));

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
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.034f, 0.040f, 0.048f, 0.95f))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
				.Text(FText::FromString(TEXT("Audio")))
			]
			+ SVerticalBox::Slot()
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
						.Text(this, &SBHMainMenu::GetMasterVolumeText)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(240.0f)
					[
						SNew(SSlider)
						.Value(this, &SBHMainMenu::GetMasterVolumeValue)
						.OnValueChanged(this, &SBHMainMenu::OnMasterVolumeChanged)
					]
				]
			]
			+ SVerticalBox::Slot()
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
						.Text(this, &SBHMainMenu::GetMusicVolumeText)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(240.0f)
					[
						SNew(SSlider)
						.Value(this, &SBHMainMenu::GetMusicVolumeValue)
						.OnValueChanged(this, &SBHMainMenu::OnMusicVolumeChanged)
					]
				]
			]
			+ SVerticalBox::Slot()
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
						.Text(this, &SBHMainMenu::GetUiVolumeText)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(240.0f)
					[
						SNew(SSlider)
						.Value(this, &SBHMainMenu::GetUiVolumeValue)
						.OnValueChanged(this, &SBHMainMenu::OnUiVolumeChanged)
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(MenuFont(13, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.84f, 0.92f, 0.89f, 1.0f))
				.Text(FText::FromString(TEXT("Local Display")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(MenuFont(10))
				.ColorAndOpacity(FLinearColor(0.62f, 0.70f, 0.70f, 1.0f))
				.Text(FText::FromString(TEXT("These buttons only change this machine.")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnGraphicsPresetClicked, 0)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Low 4GB")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnGraphicsPresetClicked, 1)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Med")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnGraphicsPresetClicked, 2)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("High 16GB")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnGraphicsPresetClicked, 3)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Ultra")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnResolutionClicked, 1280, 720, false)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("720 W")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnResolutionClicked, 1920, 1080, false)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("1080 W")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnResolutionClicked, 1920, 1080, true)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("1080 F")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnResolutionClicked, 2560, 1440, true)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("1440 F")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnFrameRateClicked, 30)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("30")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnFrameRateClicked, 60)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("60")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnFrameRateClicked, 120)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("120")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnFrameRateClicked, 144)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("144")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnFrameRateClicked, 0)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Free")))
					]
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

	const UEnum* ModifierEnum = StaticEnum<EBHRoundModifier>();
	const FString ModifierName = BHGS && ModifierEnum
		? ModifierEnum->GetNameStringByValue(static_cast<int64>(BHGS->RoundModifier))
		: FString(TEXT("None"));

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
			[
				SNew(SHorizontalBox)
				.Visibility(bTestMode ? EVisibility::Collapsed : EVisibility::Visible)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnPracticeRoleClicked, EBHPlayerRole::Survivor)
					[
						SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Survivor")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnPracticeRoleClicked, EBHPlayerRole::Hunter)
					[
						SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Teacher")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).ContentPadding(FMargin(7.0f, 4.0f)).OnClicked(this, &SBHMainMenu::OnPracticeRoleClicked, EBHPlayerRole::FakeHunter)
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
					SNew(SButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::None)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("None")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::LightsOut)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Lights")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::LoudFooting)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Loud")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::JammedDoors)
					[
						SNew(STextBlock).Font(MenuFont(9, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Doors")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).ContentPadding(FMargin(6.0f, 3.0f)).OnClicked(this, &SBHMainMenu::OnPracticeModifierClicked, EBHRoundModifier::PanicSurge)
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
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonColorAndOpacity(FLinearColor(0.20f, 0.30f, 0.17f, 1.0f))
					.ContentPadding(FMargin(8.0f, 5.0f))
					.OnClicked(this, &SBHMainMenu::OnPracticeRefreshClicked)
					[
						SNew(STextBlock).Font(MenuFont(10, FName(TEXT("Bold")))).Text(FText::FromString(TEXT("Refresh Tasks")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
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
							.Text(FText::FromString(FString::Printf(TEXT("%s  |  %s  |  Queued: %s"), *PlayerName, *ReadyStatus, *DesiredRole)))
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
								SNew(SButton)
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
								SNew(SButton)
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
								SNew(SButton)
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
								SNew(SButton)
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
								SNew(SButton)
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
								SNew(SButton)
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
