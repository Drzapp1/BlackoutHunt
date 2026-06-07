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
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// Palette: terminal green on near-black, amber/red for the facility "warnings", teal accent, and a
	// hot red used once the boot escalates into the crash.
	const FLinearColor BootBackground(0.004f, 0.007f, 0.006f, 1.0f);
	const FLinearColor BootGreen(0.42f, 1.0f, 0.55f, 1.0f);
	const FLinearColor BootLabel(0.55f, 0.86f, 0.62f, 1.0f);
	const FLinearColor BootDim(0.32f, 0.52f, 0.40f, 1.0f);
	const FLinearColor BootAmber(1.0f, 0.74f, 0.22f, 1.0f);
	const FLinearColor BootRed(1.0f, 0.30f, 0.26f, 1.0f);
	const FLinearColor BootTeal(0.34f, 0.95f, 0.88f, 1.0f);

	// Classic OS error-dialog palette (contrasts hard with the green terminal).
	const FLinearColor DlgFrame(0.05f, 0.05f, 0.07f, 1.0f);
	const FLinearColor DlgTitle(0.04f, 0.26f, 0.62f, 1.0f);
	const FLinearColor DlgBody(0.86f, 0.86f, 0.88f, 1.0f);
	const FLinearColor DlgButton(0.80f, 0.80f, 0.83f, 1.0f);
	const FLinearColor DlgText(0.07f, 0.07f, 0.09f, 1.0f);
	const FLinearColor DlgErrorIcon(0.80f, 0.10f, 0.10f, 1.0f);

	constexpr float BootFadeInSeconds = 0.25f;

	const FSlateBrush* BootWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	}

	FSlateFontInfo BootFont(const int32 Size, const FName Typeface = FName(TEXT("Regular")))
	{
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}

	// Cheap integer hash (Murmur-ish finalizer) for deterministic per-(time,char) glitch decisions.
	uint32 BHHash(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}
}

void SBHBootConsole::Construct(const FArguments& InArgs)
{
	OnReadyForMenuDelegate = InArgs._OnReadyForMenu;
	OnFinishedDelegate = InArgs._OnFinished;
	bReducedFlash = InArgs._ReducedFlash;

	BuildBootScript();

	SetRenderOpacity(0.0f);

	ChildSlot
	[
		SNew(SOverlay)

		// [0] Static black backdrop, full-bleed and never transformed -- the screen shake moves the
		// content layer over this, so a jitter never reveals a hard edge.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(BootWhiteBrush())
			.BorderBackgroundColor(BootBackground)
		]

		// [1] Shaken content group: scanline + terminal + failure banner + crash dialogs.
		+ SOverlay::Slot()
		[
			SAssignNew(JitterLayer, SOverlay)

			// Thin teal scanline at the top, echoing the in-game travel loading screen.
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			[
				SNew(SBox)
				.HeightOverride(3.0f)
				.Visibility(this, &SBHBootConsole::GetTerminalVisibility)
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
				.Visibility(this, &SBHBootConsole::GetTerminalVisibility)
				.MinDesiredWidth(640.0f)
				.MaxDesiredWidth(900.0f)
				[
					BuildLogBody()
				]
			]

			// Huge red "SYSTEM FAILURE" banner that flickers in during the crash.
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Visibility(this, &SBHBootConsole::GetFailureBannerVisibility)
				.Font(BootFont(48, FName(TEXT("Bold"))))
				.Justification(ETextJustify::Center)
				.ShadowOffset(FVector2D(3.0f, 3.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
				.Text(this, &SBHBootConsole::GetFailureBannerText)
				.ColorAndOpacity(this, &SBHBootConsole::GetFailureBannerColor)
			]

			// Stacked Windows-style crash dialogs.
			+ SOverlay::Slot()
			[
				BuildCrashDialogLayer()
			]
		]

		// [2] Full-screen flicker tint on top of everything (suppressed when reduced-flash is on).
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(this, &SBHBootConsole::GetGlitchFlashVisibility)
			.BorderImage(BootWhiteBrush())
			.BorderBackgroundColor(this, &SBHBootConsole::GetGlitchFlashColor)
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
	Add(TEXT("compiling shaders"), 2.0f, true, FString(), BootTeal);
	Add(TEXT("linking material graphs"), 1.0f, true, FString(), BootGreen);
	Add(TEXT("loading audio banks"), 0.6f, false, TEXT("OK"), BootGreen);
	Add(TEXT("starting online subsystem"), 0.6f, false, TEXT("OK"), BootGreen);
	Add(TEXT("building navigation mesh"), 0.6f, false, TEXT("OK"), BootGreen);
	Add(TEXT("loading hunter behavior trees"), 0.7f, false, TEXT("OK"), BootGreen);
	Add(TEXT("priming jumpscare cache"), 0.7f, false, TEXT("OK"), BootGreen);
	Add(TEXT("FACILITY MAINFRAME"), 0.7f, false, TEXT("LINK"), BootTeal, true);
	Add(TEXT("cctv surveillance grid"), 0.6f, false, TEXT("ONLINE"), BootGreen);
	Add(TEXT("breaker network 6/6"), 0.6f, false, TEXT("NOMINAL"), BootGreen);
	Add(TEXT("containment field"), 0.7f, false, TEXT("DEGRADED"), BootAmber);
	Add(TEXT("emergency lighting"), 0.7f, false, TEXT("OFFLINE"), BootAmber);
	Add(TEXT("blackout protocol"), 0.8f, false, TEXT("ARMED"), BootRed);
	// The boot does not "succeed" -- it falls over into the crash sequence.
	Add(TEXT("ALL SYSTEMS"), 0.7f, false, TEXT("FAIL"), BootRed, true);
	Add(TEXT("blackouthunt.exe"), 0.5f, false, TEXT("NOT RESPONDING"), BootRed);
	Add(TEXT("!! KERNEL PANIC"), 0.6f, false, TEXT("0x000000BH"), BootRed, true);

	// Normalize the relative durations so the whole stream always runs for BootScriptDuration seconds,
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
					// Fills turn red once the boot starts crashing.
					.BorderBackgroundColor_Lambda([this, FillColor]()
					{
						return FSlateColor(InGlitch() ? BootRed : FillColor);
					})
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

	// One row per boot line: label (left, fills) + status (right). All content is attribute-bound to
	// Elapsed, so the rows animate (and later corrupt) every frame without rebuilding the Slate tree.
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
						.ColorAndOpacity_Lambda([this, Index]() { return GetTagColor(Index); })
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
			.ColorAndOpacity_Lambda([this]() { return GetMasterBarColor(); })
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
				.ColorAndOpacity_Lambda([this]() { return GetMasterBarColor(); })
				.Text_Lambda([this]() { return GetMasterPercentText(); })
			]
		];

	return Root;
}

TSharedRef<SWidget> SBHBootConsole::BuildCrashDialogLayer()
{
	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);

	struct FDialogSpec
	{
		FString Title;
		FString Line1;
		FString Line2;
		float OffsetX;
		float OffsetY;
	};

	const FDialogSpec Specs[3] = {
		{ TEXT("BlackoutHunt.exe"), TEXT("BlackoutHunt.exe has stopped working."),
		  TEXT("Windows is checking for a solution to the problem..."), -156.0f, -94.0f },
		{ TEXT("System Error"), TEXT("FATAL: containment breach detected in sector 0x0C."),
		  TEXT("The facility was shut down to prevent damage."), 46.0f, 4.0f },
		{ TEXT("blackout.sys"), TEXT("he is already inside."),
		  TEXT("do not turn off your light."), -86.0f, 98.0f },
	};

	for (int32 DialogIndex = 0; DialogIndex < 3; ++DialogIndex)
	{
		const FDialogSpec& Spec = Specs[DialogIndex];
		Canvas->AddSlot()
			.Anchors(FAnchors(0.5f, 0.5f))
			.Alignment(FVector2D(0.5f, 0.5f))
			.AutoSize(true)
			.Offset(FMargin(Spec.OffsetX, Spec.OffsetY, 0.0f, 0.0f))
			[
				SNew(SBox)
				.Visibility_Lambda([this, DialogIndex]() { return GetCrashDialogVisibility(DialogIndex); })
				[
					MakeCrashDialog(Spec.Title, Spec.Line1, Spec.Line2)
				]
			];
	}

	return Canvas;
}

TSharedRef<SWidget> SBHBootConsole::MakeCrashDialog(const FString& Title, const FString& Line1, const FString& Line2) const
{
	auto DialogButton = [](const FString& Label) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.MinDesiredWidth(78.0f)
			.HeightOverride(24.0f)
			[
				SNew(SBorder)
				.BorderImage(BootWhiteBrush())
				.BorderBackgroundColor(DlgButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(BootFont(10))
					.ColorAndOpacity(FSlateColor(DlgText))
					.Text(FText::FromString(Label))
				]
			];
	};

	return SNew(SBox)
		.WidthOverride(440.0f)
		[
			// 1px dark frame around the window.
			SNew(SBorder)
			.BorderImage(BootWhiteBrush())
			.BorderBackgroundColor(DlgFrame)
			.Padding(FMargin(1.0f))
			[
				SNew(SVerticalBox)

				// Title bar.
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(BootWhiteBrush())
					.BorderBackgroundColor(DlgTitle)
					.Padding(FMargin(8.0f, 4.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(BootFont(11, FName(TEXT("Bold"))))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
							.Text(FText::FromString(Title))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(BootFont(11, FName(TEXT("Bold"))))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.88f, 0.95f, 1.0f)))
							.Text(FText::FromString(TEXT("[ X ]")))
						]
					]
				]

				// Body: error icon + message.
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(BootWhiteBrush())
					.BorderBackgroundColor(DlgBody)
					.Padding(FMargin(14.0f, 16.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Top)
						.Padding(0.0f, 0.0f, 14.0f, 0.0f)
						[
							SNew(SBox)
							.WidthOverride(34.0f)
							.HeightOverride(34.0f)
							[
								SNew(SBorder)
								.BorderImage(BootWhiteBrush())
								.BorderBackgroundColor(DlgErrorIcon)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Font(BootFont(18, FName(TEXT("Bold"))))
									.ColorAndOpacity(FSlateColor(FLinearColor::White))
									.Text(FText::FromString(TEXT("X")))
								]
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 5.0f)
							[
								SNew(STextBlock)
								.Font(BootFont(11, FName(TEXT("Bold"))))
								.ColorAndOpacity(FSlateColor(DlgText))
								.AutoWrapText(true)
								.Text(FText::FromString(Line1))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Font(BootFont(10))
								.ColorAndOpacity(FSlateColor(DlgText))
								.AutoWrapText(true)
								.Text(FText::FromString(Line2))
							]
						]
					]
				]

				// Button row.
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(BootWhiteBrush())
					.BorderBackgroundColor(DlgBody)
					.Padding(FMargin(10.0f, 8.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNullWidget::NullWidget
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							DialogButton(TEXT("Debug"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							DialogButton(TEXT("Close"))
						]
					]
				]
			]
		];
}

void SBHBootConsole::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bFinished)
	{
		return;
	}

	Elapsed += InDeltaTime;

	// Whole-widget opacity: fade in at the start, hold, then fade out at the end to reveal the menu
	// the owner placed underneath us.
	const float Fs = FadeStartTime();
	if (Elapsed < BootFadeInSeconds)
	{
		SetRenderOpacity(FMath::Clamp(Elapsed / BootFadeInSeconds, 0.0f, 1.0f));
	}
	else if (Elapsed >= Fs)
	{
		SetRenderOpacity(FMath::Clamp(1.0f - (Elapsed - Fs) / MenuFadeSeconds, 0.0f, 1.0f));
	}
	else
	{
		SetRenderOpacity(1.0f);
	}

	// Screen shake during the crash (suppressed for reduced-flash).
	if (JitterLayer.IsValid())
	{
		if (InGlitch() && !bReducedFlash)
		{
			const float Mag = 3.0f + 8.0f * GlitchProgress();
			const FVector2D Offset(FMath::FRandRange(-Mag, Mag), FMath::FRandRange(-Mag, Mag));
			JitterLayer->SetRenderTransform(FSlateRenderTransform(Offset));
		}
		else
		{
			JitterLayer->SetRenderTransform(TOptional<FSlateRenderTransform>());
		}
	}

	// Once the screen is fully black, ask the owner to place the menu underneath us; we then fade out
	// over it. Removal happens at TotalDuration (or on skip) via OnFinished.
	if (!bMenuRevealFired && Elapsed >= BlackStartTime())
	{
		bMenuRevealFired = true;
		SetVisibility(EVisibility::HitTestInvisible); // let the revealed menu take input during the fade
		OnReadyForMenuDelegate.ExecuteIfBound();
	}

	if (Elapsed >= TotalDuration())
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

void SBHBootConsole::RequestSkip()
{
	// Jump straight to the menu. The owner's finish handler shows it if the reveal has not happened yet.
	Finish();
}

FReply SBHBootConsole::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	RequestSkip();
	return FReply::Handled();
}

FReply SBHBootConsole::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	RequestSkip();
	return FReply::Handled();
}

bool SBHBootConsole::InGlitch() const
{
	return Elapsed >= GlitchStartTime() && Elapsed < BlackStartTime();
}

bool SBHBootConsole::IsBlackedOut() const
{
	return Elapsed >= BlackStartTime();
}

float SBHBootConsole::GlitchProgress() const
{
	if (GlitchSeconds <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}
	return FMath::Clamp((Elapsed - GlitchStartTime()) / GlitchSeconds, 0.0f, 1.0f);
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

FString SBHBootConsole::GlitchString(const FString& In, int32 Salt) const
{
	if (!InGlitch() || In.IsEmpty())
	{
		return In;
	}

	const float Intensity = FMath::Clamp(0.12f + 0.7f * GlitchProgress(), 0.0f, 0.95f);
	// Quantize time so the corruption changes a few times per second; freeze it for reduced-flash.
	const int32 Bucket = bReducedFlash ? 11 : FMath::FloorToInt(Elapsed / 0.07f);

	static const TCHAR Charset[] = TEXT("!@#$%&*<>?/=+|01ABCDEF#%&");
	const int32 CharsetLen = UE_ARRAY_COUNT(Charset) - 1;

	FString Out = In;
	for (int32 i = 0; i < Out.Len(); ++i)
	{
		if (Out[i] == TEXT(' '))
		{
			continue;
		}
		const uint32 H = BHHash(static_cast<uint32>(Bucket * 73856093) ^ static_cast<uint32>(i * 19349663) ^ static_cast<uint32>(Salt * 83492791));
		if ((H % 1000u) < static_cast<uint32>(Intensity * 1000.0f))
		{
			Out[i] = Charset[(H >> 8) % static_cast<uint32>(CharsetLen)];
		}
	}
	return Out;
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
	FString Base;
	if (Line.bProgress)
	{
		Base = Line.Label;
	}
	else
	{
		const int32 Total = Line.Label.Len();
		const int32 Shown = FMath::Clamp(FMath::RoundToInt(LineFraction(Index) * Total), 0, Total);
		Base = Line.Label.Left(Shown);
	}
	return FText::FromString(GlitchString(Base, Index * 7 + 13));
}

FSlateColor SBHBootConsole::GetLabelColor(int32 Index) const
{
	FLinearColor Base = (BootLines.IsValidIndex(Index) && BootLines[Index].bHeader)
		? BootLines[Index].TagColor
		: BootLabel;
	if (InGlitch())
	{
		Base = FMath::Lerp(Base, BootRed, 0.6f);
	}
	return FSlateColor(Base);
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
		return FText::FromString(GlitchString(FString::Printf(TEXT("[ %s ]"), *Line.DoneTag), Index * 13 + 5));
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
	if (InGlitch())
	{
		return FSlateColor(BootRed);
	}
	return FSlateColor(LineFraction(Index) >= 1.0f ? BootLines[Index].TagColor : BootDim);
}

FText SBHBootConsole::GetPercentText(int32 Index) const
{
	if (!BootLines.IsValidIndex(Index) || !IsLineVisible(Index))
	{
		return FText::GetEmpty();
	}
	if (InGlitch())
	{
		return FText::FromString(TEXT("ERR"));
	}
	return FText::FromString(FString::Printf(TEXT("%3d%%"), FMath::RoundToInt(LineFraction(Index) * 100.0f)));
}

FText SBHBootConsole::GetCursorText() const
{
	if (InGlitch())
	{
		return FText::FromString(TEXT("> HALTED"));
	}
	const bool bOn = bReducedFlash || (FMath::Fmod(Elapsed, 0.9f) < 0.5f);
	return FText::FromString(bOn ? TEXT("> _") : TEXT(">"));
}

FText SBHBootConsole::GetMasterPercentText() const
{
	if (InGlitch())
	{
		return FText::FromString(TEXT("FAIL"));
	}
	return FText::FromString(FString::Printf(TEXT("%3d%%"), FMath::RoundToInt(OverallFraction() * 100.0f)));
}

FSlateColor SBHBootConsole::GetMasterBarColor() const
{
	return FSlateColor(InGlitch() ? BootRed : BootGreen);
}

EVisibility SBHBootConsole::GetTerminalVisibility() const
{
	return IsBlackedOut() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

EVisibility SBHBootConsole::GetGlitchFlashVisibility() const
{
	return (InGlitch() && !bReducedFlash) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FSlateColor SBHBootConsole::GetGlitchFlashColor() const
{
	if (!InGlitch())
	{
		return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	}

	// Mostly faint, with occasional brighter red (and rare white) flashes that intensify as the crash
	// progresses. Alpha is capped so it never fully whites out the screen.
	const int32 Bucket = FMath::FloorToInt(Elapsed / 0.05f);
	const uint32 H = BHHash(static_cast<uint32>(Bucket * 2654435761u));
	const float P = GlitchProgress();

	float Alpha = 0.02f;
	FLinearColor Tint = BootRed;
	if ((H % 11u) == 0u)
	{
		Alpha = 0.22f + 0.10f * P;
		Tint = FLinearColor(0.95f, 0.95f, 1.0f, 1.0f); // rare white blow-out
	}
	else if ((H % 4u) == 0u)
	{
		Alpha = 0.10f + 0.14f * P;
	}
	return FSlateColor(FLinearColor(Tint.R, Tint.G, Tint.B, Alpha));
}

EVisibility SBHBootConsole::GetFailureBannerVisibility() const
{
	return InGlitch() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText SBHBootConsole::GetFailureBannerText() const
{
	return FText::FromString(GlitchString(TEXT("SYSTEM FAILURE"), 4242));
}

FSlateColor SBHBootConsole::GetFailureBannerColor() const
{
	if (bReducedFlash)
	{
		return FSlateColor(BootRed);
	}
	// Flicker the banner alpha so it strobes against the corruption.
	const int32 Bucket = FMath::FloorToInt(Elapsed / 0.06f);
	const uint32 H = BHHash(static_cast<uint32>(Bucket * 40503u) ^ 0x9e3779b9u);
	const float Alpha = ((H % 5u) == 0u) ? 0.35f : 1.0f;
	return FSlateColor(FLinearColor(BootRed.R, BootRed.G, BootRed.B, Alpha));
}

EVisibility SBHBootConsole::GetCrashDialogVisibility(int32 DialogIndex) const
{
	if (IsBlackedOut() || DialogIndex < 0 || DialogIndex >= 3)
	{
		return EVisibility::Collapsed;
	}
	const float Appear = GlitchStartTime() + CrashDialogDelays[DialogIndex];
	return (Elapsed >= Appear) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}
