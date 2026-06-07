// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "SBHBootConsole.h"
#include "HAL/PlatformMisc.h"
#include "Styling/CoreStyle.h"
#include "Templates/Function.h"
#include "Types/SlateEnums.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// Palette: terminal green on near-black, with amber/red for the facility "warnings" and a teal accent.
	const FLinearColor BootBackground(0.004f, 0.007f, 0.006f, 1.0f);
	const FLinearColor BootGreen(0.42f, 1.0f, 0.55f, 1.0f);
	const FLinearColor BootLabel(0.55f, 0.86f, 0.62f, 1.0f);
	const FLinearColor BootDim(0.32f, 0.52f, 0.40f, 1.0f);
	const FLinearColor BootAmber(1.0f, 0.74f, 0.22f, 1.0f);
	const FLinearColor BootRed(1.0f, 0.40f, 0.34f, 1.0f);
	const FLinearColor BootTeal(0.34f, 0.95f, 0.88f, 1.0f);

	constexpr float BootFadeInSeconds = 0.25f;
	constexpr float BootHoldSeconds = 0.55f;    // dwell on the completed log so "READY" is readable
	constexpr float BootFadeOutSeconds = 0.35f;

	const FSlateBrush* BootWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	}

	FSlateFontInfo BootFont(const int32 Size, const FName Typeface = FName(TEXT("Regular")))
	{
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}
}

void SBHBootConsole::Construct(const FArguments& InArgs)
{
	OnFinishedDelegate = InArgs._OnFinished;
	bReducedFlash = InArgs._ReducedFlash;

	BuildBootScript();

	SetRenderOpacity(0.0f);

	ChildSlot
	[
		SNew(SOverlay)

		// Solid black backdrop.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(BootWhiteBrush())
			.BorderBackgroundColor(BootBackground)
		]

		// Thin teal scanline at the very top, echoing the in-game travel loading screen.
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.HeightOverride(3.0f)
			[
				SNew(SBorder)
				.BorderImage(BootWhiteBrush())
				.BorderBackgroundColor(FLinearColor(BootTeal.R, BootTeal.G, BootTeal.B, 0.75f))
			]
		]

		// Terminal body, anchored top-left like a real console.
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(64.0f, 48.0f, 48.0f, 48.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(640.0f)
			.MaxDesiredWidth(900.0f)
			[
				BuildLogBody()
			]
		]
	];
}

void SBHBootConsole::BuildBootScript()
{
	auto Add = [this](const FString& Label, float Duration, bool bProgress, const FString& DoneTag, const FLinearColor& TagColor, bool bHeader = false)
	{
		FBHBootLine Line;
		Line.Label = Label;
		Line.Duration = Duration;
		Line.bProgress = bProgress;
		Line.DoneTag = DoneTag;
		Line.TagColor = TagColor;
		Line.bHeader = bHeader;
		BootLines.Add(MoveTemp(Line));
	};

	FString GpuBrand = FPlatformMisc::GetPrimaryGPUBrand();
	GpuBrand.TrimStartAndEndInline();
	if (GpuBrand.IsEmpty())
	{
		GpuBrand = TEXT("generic display adapter");
	}
	else if (GpuBrand.Len() > 34)
	{
		GpuBrand = GpuBrand.Left(31) + TEXT("...");
	}
	const int32 CoreCount = FMath::Max(1, FPlatformMisc::NumberOfCores());

	Add(TEXT("power-on self test"), 0.9f, false, TEXT("OK"), BootGreen);
	Add(FString::Printf(TEXT("cpu cores online: %d"), CoreCount), 0.6f, false, TEXT("OK"), BootGreen);
	Add(TEXT("system memory map"), 0.7f, false, TEXT("OK"), BootGreen);
	Add(TEXT("scanning display adapter"), 0.7f, false, TEXT("OK"), BootGreen);
	Add(FString::Printf(TEXT("gpu: %s"), *GpuBrand), 0.6f, false, TEXT("OK"), BootGreen);
	Add(TEXT("initializing RHI :: DirectX 11 (SM5)"), 0.8f, false, TEXT("OK"), BootGreen);
	Add(TEXT("mounting pak archives"), 1.1f, true, FString(), BootGreen);
	Add(TEXT("compiling shaders"), 2.2f, true, FString(), BootTeal);
	Add(TEXT("linking material graphs"), 1.0f, true, FString(), BootGreen);
	Add(TEXT("loading audio banks"), 0.7f, false, TEXT("OK"), BootGreen);
	Add(TEXT("starting online subsystem"), 0.7f, false, TEXT("OK"), BootGreen);
	Add(TEXT("building navigation mesh"), 0.7f, false, TEXT("OK"), BootGreen);
	Add(TEXT("loading hunter behavior trees"), 0.8f, false, TEXT("OK"), BootGreen);
	Add(TEXT("priming jumpscare cache"), 0.8f, false, TEXT("OK"), BootGreen);
	Add(TEXT("FACILITY MAINFRAME"), 0.8f, false, TEXT("LINK"), BootTeal, true);
	Add(TEXT("cctv surveillance grid"), 0.7f, false, TEXT("ONLINE"), BootGreen);
	Add(TEXT("breaker network 6/6"), 0.7f, false, TEXT("NOMINAL"), BootGreen);
	Add(TEXT("containment field"), 0.8f, false, TEXT("DEGRADED"), BootAmber);
	Add(TEXT("emergency lighting"), 0.8f, false, TEXT("OFFLINE"), BootAmber);
	Add(TEXT("blackout protocol"), 1.0f, false, TEXT("ARMED"), BootRed);
	Add(TEXT("ALL SYSTEMS"), 1.0f, false, TEXT("READY"), BootTeal, true);

	// Normalize the relative durations so the whole script always runs for BootScriptDuration seconds,
	// then bake the cumulative start time of each line.
	float RawTotal = 0.0f;
	for (const FBHBootLine& Line : BootLines)
	{
		RawTotal += FMath::Max(0.05f, Line.Duration);
	}
	const float Scale = (RawTotal > KINDA_SMALL_NUMBER) ? (BootScriptDuration / RawTotal) : 1.0f;

	LineStartTimes.Reset(BootLines.Num());
	float Cursor = 0.0f;
	for (FBHBootLine& Line : BootLines)
	{
		Line.Duration = FMath::Max(0.05f, Line.Duration) * Scale;
		LineStartTimes.Add(Cursor);
		Cursor += Line.Duration;
	}
}

TSharedRef<SWidget> SBHBootConsole::MakeFillBar(float Width, float Height, TFunction<float()> FracFn, FLinearColor FillColor) const
{
	return SNew(SBox)
		.WidthOverride(Width)
		.HeightOverride(Height)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(BootWhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.06f, 0.10f, 0.08f, 0.95f))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride_Lambda([Width, FracFn]()
				{
					return FOptionalSize(Width * FMath::Clamp(FracFn(), 0.0f, 1.0f));
				})
				[
					SNew(SBorder)
					.BorderImage(BootWhiteBrush())
					.BorderBackgroundColor(FillColor)
				]
			]
		];
}

TSharedRef<SWidget> SBHBootConsole::BuildLogBody()
{
	TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);

	// Banner.
	Root->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Font(BootFont(20, FName(TEXT("Bold"))))
			.ColorAndOpacity(FSlateColor(BootTeal))
			.Text(FText::FromString(TEXT("BLACKOUT HUNT  //  FACILITY MAINFRAME")))
		];
	Root->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			SNew(STextBlock)
			.Font(BootFont(10))
			.ColorAndOpacity(FSlateColor(BootDim))
			.Text(FText::FromString(TEXT("firmware 0.8.1   -   cold start   -   press any key to skip")))
		];
	Root->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SBox)
			.HeightOverride(1.0f)
			[
				SNew(SBorder)
				.BorderImage(BootWhiteBrush())
				.BorderBackgroundColor(FLinearColor(BootGreen.R, BootGreen.G, BootGreen.B, 0.22f))
			]
		];

	// One row per boot line. Each row is label (left, fills) + status (right): a fill bar + percent for
	// progress lines, or a spinner-then-tag for the rest. All content is attribute-bound to Elapsed, so
	// the rows animate every frame without rebuilding the Slate tree.
	for (int32 Index = 0; Index < BootLines.Num(); ++Index)
	{
		const FBHBootLine& Line = BootLines[Index];

		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(BootFont(Line.bHeader ? 13 : 12, Line.bHeader ? FName(TEXT("Bold")) : FName(TEXT("Regular"))))
				.ColorAndOpacity_Lambda([this, Index]() { return GetLabelColor(Index); })
				.Text_Lambda([this, Index]() { return GetLabelText(Index); })
			];

		if (Line.bProgress)
		{
			Row->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.0f, 0.0f, 8.0f, 0.0f)
				[
					MakeFillBar(160.0f, 12.0f, [this, Index]() { return LineFraction(Index); }, Line.TagColor)
				];
			Row->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(46.0f)
					[
						SNew(STextBlock)
						.Font(BootFont(12))
						.Justification(ETextJustify::Right)
						.ColorAndOpacity(FSlateColor(Line.TagColor))
						.Text_Lambda([this, Index]() { return GetPercentText(Index); })
					]
				];
		}
		else
		{
			Row->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(BootFont(12, FName(TEXT("Bold"))))
					.Justification(ETextJustify::Right)
					.ColorAndOpacity_Lambda([this, Index]() { return GetTagColor(Index); })
					.Text_Lambda([this, Index]() { return GetTagText(Index); })
				];
		}

		Root->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 1.0f, 0.0f, 1.0f)
			[
				SNew(SBox)
				.Visibility_Lambda([this, Index]() { return GetLineVisibility(Index); })
				[
					Row
				]
			];
	}

	// Blinking prompt cursor under the stream.
	Root->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 14.0f)
		[
			SNew(STextBlock)
			.Font(BootFont(12, FName(TEXT("Bold"))))
			.ColorAndOpacity(FSlateColor(BootGreen))
			.Text_Lambda([this]() { return GetCursorText(); })
		];

	// Master loading bar + percent.
	Root->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				MakeFillBar(460.0f, 16.0f, [this]() { return OverallFraction(); }, BootGreen)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(BootFont(14, FName(TEXT("Bold"))))
				.ColorAndOpacity(FSlateColor(BootGreen))
				.Text_Lambda([this]() { return GetMasterPercentText(); })
			]
		];

	return Root;
}

void SBHBootConsole::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bFinished)
	{
		return;
	}

	Elapsed += InDeltaTime;

	const float FadeOutStart = BootScriptDuration + BootHoldSeconds;
	const float TotalDuration = FadeOutStart + BootFadeOutSeconds;

	// Fade in, hold, then fade out.
	if (Elapsed < BootFadeInSeconds)
	{
		SetRenderOpacity(FMath::Clamp(Elapsed / BootFadeInSeconds, 0.0f, 1.0f));
	}
	else if (Elapsed >= FadeOutStart)
	{
		SetRenderOpacity(FMath::Clamp(1.0f - (Elapsed - FadeOutStart) / BootFadeOutSeconds, 0.0f, 1.0f));
	}
	else
	{
		SetRenderOpacity(1.0f);
	}

	if (Elapsed >= TotalDuration)
	{
		Finish();
	}
}

void SBHBootConsole::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	SetRenderOpacity(0.0f);
	// Copy then fire: the handler tears this widget down, so touch no members afterwards.
	FSimpleDelegate Delegate = OnFinishedDelegate;
	Delegate.ExecuteIfBound();
}

FReply SBHBootConsole::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	Finish();
	return FReply::Handled();
}

FReply SBHBootConsole::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Finish();
	return FReply::Handled();
}

float SBHBootConsole::LineStartTime(int32 Index) const
{
	return LineStartTimes.IsValidIndex(Index) ? LineStartTimes[Index] : 0.0f;
}

float SBHBootConsole::LineFraction(int32 Index) const
{
	if (!BootLines.IsValidIndex(Index))
	{
		return 0.0f;
	}
	const float Start = LineStartTime(Index);
	const float Duration = FMath::Max(0.05f, BootLines[Index].Duration);
	return FMath::Clamp((Elapsed - Start) / Duration, 0.0f, 1.0f);
}

bool SBHBootConsole::IsLineVisible(int32 Index) const
{
	return Elapsed >= LineStartTime(Index) - KINDA_SMALL_NUMBER;
}

float SBHBootConsole::OverallFraction() const
{
	return (BootScriptDuration > KINDA_SMALL_NUMBER) ? FMath::Clamp(Elapsed / BootScriptDuration, 0.0f, 1.0f) : 1.0f;
}

FText SBHBootConsole::GetLabelText(int32 Index) const
{
	if (!BootLines.IsValidIndex(Index) || !IsLineVisible(Index))
	{
		return FText::GetEmpty();
	}
	const FBHBootLine& Line = BootLines[Index];
	// Progress lines show the full label immediately (the fill bar is the animation); other lines type
	// the label out over their slice for a terminal feel.
	if (Line.bProgress)
	{
		return FText::FromString(Line.Label);
	}
	const int32 Total = Line.Label.Len();
	const int32 Shown = FMath::Clamp(FMath::RoundToInt(LineFraction(Index) * Total), 0, Total);
	return FText::FromString(Line.Label.Left(Shown));
}

FSlateColor SBHBootConsole::GetLabelColor(int32 Index) const
{
	if (BootLines.IsValidIndex(Index) && BootLines[Index].bHeader)
	{
		return FSlateColor(BootLines[Index].TagColor);
	}
	return FSlateColor(BootLabel);
}

EVisibility SBHBootConsole::GetLineVisibility(int32 Index) const
{
	return IsLineVisible(Index) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText SBHBootConsole::GetTagText(int32 Index) const
{
	if (!BootLines.IsValidIndex(Index) || !IsLineVisible(Index))
	{
		return FText::GetEmpty();
	}
	const FBHBootLine& Line = BootLines[Index];
	if (LineFraction(Index) >= 1.0f)
	{
		return FText::FromString(FString::Printf(TEXT("[ %s ]"), *Line.DoneTag));
	}
	// Still working: show a spinner (steady glyph when reduced-flash comfort is on).
	if (bReducedFlash)
	{
		return FText::FromString(TEXT(".."));
	}
	static const TCHAR Spinner[] = { TEXT('|'), TEXT('/'), TEXT('-'), TEXT('\\') };
	const int32 Frame = FMath::Abs(FMath::FloorToInt(Elapsed / 0.08f)) % 4;
	return FText::FromString(FString::Chr(Spinner[Frame]));
}

FSlateColor SBHBootConsole::GetTagColor(int32 Index) const
{
	if (!BootLines.IsValidIndex(Index))
	{
		return FSlateColor(BootDim);
	}
	return FSlateColor(LineFraction(Index) >= 1.0f ? BootLines[Index].TagColor : BootDim);
}

FText SBHBootConsole::GetPercentText(int32 Index) const
{
	if (!BootLines.IsValidIndex(Index) || !IsLineVisible(Index))
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("%3d%%"), FMath::RoundToInt(LineFraction(Index) * 100.0f)));
}

FText SBHBootConsole::GetCursorText() const
{
	const bool bOn = bReducedFlash || (FMath::Fmod(Elapsed, 0.9f) < 0.5f);
	return FText::FromString(bOn ? TEXT("> _") : TEXT(">"));
}

FText SBHBootConsole::GetMasterPercentText() const
{
	return FText::FromString(FString::Printf(TEXT("%3d%%"), FMath::RoundToInt(OverallFraction() * 100.0f)));
}
