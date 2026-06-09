// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "SBHBootConsole.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
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
	// Pre-roll log flood: enough rows to fill the full screen (the bottom-anchored block overshoots and the
	// oldest rows scroll off the top), and how fast lines scroll up.
	constexpr int32 BHPrerollRowCount = 135;
	constexpr float BHPrerollLinesPerSecond = 26.0f;

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
	SoundWorld = InArgs._SoundWorld;

	BuildBootScript();
	BuildPrerollLines();

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
				.Visibility(this, &SBHBootConsole::GetChromeVisibility)
				[
					SNew(SBorder)
					.BorderImage(BootWhiteBrush())
					.BorderBackgroundColor(FLinearColor(BootTeal.R, BootTeal.G, BootTeal.B, 0.75f))
				]
			]

			// Raw dev/debug log flood that scrolls fast before the structured boot stream. Bottom-anchored so
			// the newest line sits at the bottom edge and the block fills the full screen height (the oldest
			// rows overshoot and scroll off the top).
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			.Padding(40.0f, 0.0f, 40.0f, 14.0f)
			[
				SNew(SBox)
				.Visibility(this, &SBHBootConsole::GetPrerollVisibility)
				[
					BuildPrerollBody()
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

			// Huge red "SYSTEM FAILURE" banner (+ subline) that flickers in during the crash.
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				.Visibility(this, &SBHBootConsole::GetFailureBannerVisibility)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(BootFont(72, FName(TEXT("Bold"))))
					.Justification(ETextJustify::Center)
					.ShadowOffset(FVector2D(4.0f, 4.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
					.Text(this, &SBHBootConsole::GetFailureBannerText)
					.ColorAndOpacity(this, &SBHBootConsole::GetFailureBannerColor)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(BootFont(24, FName(TEXT("Bold"))))
					.Justification(ETextJustify::Center)
					.ShadowOffset(FVector2D(2.0f, 2.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
					.Text(this, &SBHBootConsole::GetFailureSubText)
					.ColorAndOpacity(this, &SBHBootConsole::GetFailureBannerColor)
				]
			]

			// Stacked Windows-style crash dialogs.
			+ SOverlay::Slot()
			[
				BuildCrashDialogLayer()
			]
		]

		// [2] Fake blue-screen-of-death "reboot" beat (its own layer above the terminal).
		+ SOverlay::Slot()
		[
			SAssignNew(BSODLayer, SBox)
			.Visibility(this, &SBHBootConsole::GetBSODVisibility)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			[
				BuildBSODScreen()
			]
		]

		// [3] Full-screen flash/flicker tint on top of everything (suppressed when reduced-flash is on).
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

void SBHBootConsole::BuildPrerollLines()
{
	// Bare-metal cold boot flood: BIOS POST -> bootloader -> kernel dmesg -> systemd units, the way a
	// machine spews text as it comes up. $A/$X/$N are substituted per displayed line with hash-derived
	// hex/numbers so the flood never visibly repeats. (A monotonic [timestamp] is prepended at draw time,
	// so these lines carry no timestamp of their own.)
	const TCHAR* Templates[] = {
		TEXT("AMIBIOS(C) 2026 American Megatrends Inc."),
		TEXT("BIOS Date: 03/12/26  Ver: 2.71.1  Build 0x$X"),
		TEXT("CPU: AMD Ryzen 9 7945HX 16-Core Processor"),
		TEXT("Speed 2500 MHz | Cores 16 | Threads 32 | ucode 0x$X"),
		TEXT("Installed Memory: 32768 MB  DDR5-5600"),
		TEXT("Memory Test : 0x$A . OK"),
		TEXT("Initializing USB Controllers .. Done"),
		TEXT("USB Device(s): 1 Keyboard, 1 Mouse, 3 Storage"),
		TEXT("Auto-Detecting NVMe0 .. Samsung SSD 990 PRO 2TB"),
		TEXT("Auto-Detecting SATA1 .. Not Present"),
		TEXT("PCIe Link Training x16 .. UP"),
		TEXT("ACPI Tables Loaded .. Enabled"),
		TEXT("Secure Boot: Disabled   TPM: 2.0 Present"),
		TEXT("Boot Device: NVMe0   Verifying DMI pool data ...."),
		TEXT("GRUB loading stage 1.5 ."),
		TEXT("GRUB loading, please wait ..."),
		TEXT("Loading Linux 6.8.0-blackout x86_64 ..."),
		TEXT("Loading initial ramdisk (initrd) ..."),
		TEXT("Decompressing kernel image .. ok, booting the kernel."),
		TEXT("Linux version 6.8.0-blackout (root@facility-mainframe)"),
		TEXT("Command line: ro quiet blackout=armed containment=degraded"),
		TEXT("KERNEL supported cpus: GenuineIntel AuthenticAMD"),
		TEXT("x86/fpu: Supporting XSAVE feature 0x$X 'AVX-512'"),
		TEXT("BIOS-e820: [mem 0x$A] usable"),
		TEXT("BIOS-e820: [mem 0x$A] reserved"),
		TEXT("Memory: 32741M/33554M available (kernel code 14336k)"),
		TEXT("SMP: Allowing 32 CPUs, 0 hotplug CPUs"),
		TEXT("smpboot: CPU0 microcode updated early to 0x$X"),
		TEXT("ACPI: Core revision 20230331"),
		TEXT("clocksource: tsc: mask 0x$X max_cycles 0x$X"),
		TEXT("pci 0000:01:00.0: [10de] NVIDIA GeForce RTX 4070 Laptop GPU"),
		TEXT("pci 0000:00:14.0: xHCI Host Controller"),
		TEXT("nvme nvme0: pci function 0000:02:00.0"),
		TEXT("nvme0n1: p1 p2 p3"),
		TEXT("EXT4-fs (nvme0n1p2): mounted filesystem, ordered data mode"),
		TEXT("random: crng init done"),
		TEXT("usb 1-$N: new high-speed USB device using xhci_hcd"),
		TEXT("input: HID Keyboard as /devices/platform/i8042/serio0"),
		TEXT("e1000e 0000:00:1f.6 eth0: NIC Link is Up 1000 Mbps"),
		TEXT("IPv6: ADDRCONF(NETDEV_CHANGE): eth0: link becomes ready"),
		TEXT("systemd[1]: Detected architecture x86-64"),
		TEXT("systemd[1]: Hostname set to facility-mainframe"),
		TEXT("systemd[1]: Reached target Local Encrypted Volumes"),
		TEXT("[  OK  ] Mounted /boot/efi"),
		TEXT("[  OK  ] Mounted /var/facility/cctv"),
		TEXT("[  OK  ] Started Journal Service"),
		TEXT("[  OK  ] Started Network Time Synchronization"),
		TEXT("[  OK  ] Reached target Network"),
		TEXT("[  OK  ] Started Containment Grid Daemon"),
		TEXT("[  OK  ] Started Hunter Telemetry Collector"),
		TEXT("[  OK  ] Reached target Multi-User System"),
		TEXT("         Starting Facility Atmosphere Director ..."),
		TEXT("         Starting Blackout Protocol Watchdog ..."),
		TEXT("[ WARN ] emergency-lighting.service: entered degraded state"),
		TEXT("[ WARN ] containment-field.service: pressure out of range"),
		TEXT("[FAILED] Failed to start Lockdown Override Service"),
		TEXT("0x$A: 4D 5A 90 00 03 00 00 00 04 00 00 00 FF FF"),
		TEXT("0x$A: B8 00 00 00 00 00 00 00 40 00 00 00 00 00"),
		TEXT("kauditd_printk: audit($N): apparmor='STATUS' profile='facility'"),
	};

	PrerollLines.Reset();
	for (const TCHAR* T : Templates)
	{
		PrerollLines.Add(FString(T));
	}
}

TSharedRef<SWidget> SBHBootConsole::BuildPrerollBody()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (int32 Row = 0; Row < BHPrerollRowCount; ++Row)
	{
		Box->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(BootFont(9))
				.ColorAndOpacity_Lambda([this, Row]() { return GetPrerollLineColor(Row); })
				.Text_Lambda([this, Row]() { return GetPrerollLineText(Row); })
			];
	}
	return Box;
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
			.Text(TAttribute<FText>::Create([this]()
			{
				return FText::FromString(bSkipArmed
					? TEXT("firmware 0.8.1   -   cold start   -   press a key AGAIN to skip")
					: TEXT("firmware 0.8.1   -   cold start   -   press a key TWICE to skip"));
			}))
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

	const FDialogSpec Specs[6] = {
		{ TEXT("BlackoutHunt.exe"), TEXT("BlackoutHunt.exe has stopped responding."),
		  TEXT("Windows is searching for a solution to the problem..."), -262.0f, -150.0f },
		{ TEXT("System Error"), TEXT("FATAL: containment breach detected in sector 0x0C."),
		  TEXT("Emergency shutdown initiated."), 182.0f, -108.0f },
		{ TEXT("blackout.sys"), TEXT("Unhandled exception at 0xDEAD00BH."),
		  TEXT("The lights are going out."), -118.0f, -16.0f },
		{ TEXT("winlogon.exe"), TEXT("The memory could not be read."),
		  TEXT("Something is in the dark with you."), 250.0f, 58.0f },
		{ TEXT("CONTAINMENT"), TEXT("Hull integrity 0%. All doors unlocked."),
		  TEXT("It knows that you are here."), -300.0f, 120.0f },
		{ TEXT("blackout.sys"), TEXT("he is already inside."),
		  TEXT("do not turn off your light."), 70.0f, 172.0f },
	};

	for (int32 DialogIndex = 0; DialogIndex < 6; ++DialogIndex)
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

TSharedRef<SWidget> SBHBootConsole::BuildBSODScreen()
{
	const FLinearColor BsodBlue(0.04f, 0.30f, 0.62f, 1.0f);
	const FSlateColor White(FLinearColor::White);
	const FSlateColor Faint(FLinearColor(0.80f, 0.88f, 0.97f, 1.0f));

	// A horizontal corruption bar for the "caught it" beat: it jumps left/right and recolours each frame-step
	// (occasionally a big rip), and is dark/empty otherwise -- digital signal tearing, only during the catch.
	auto MakeTearBar = [this](int32 Idx, float BarHeight) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.HeightOverride(BarHeight)
			.RenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::Create([this, Idx]()
			{
				if (Elapsed < BSODCatchTime() || Elapsed >= BlackStartTime())
				{
					return TOptional<FSlateRenderTransform>();
				}
				const int32 Step = FMath::FloorToInt(Elapsed * 19.0f);
				const float Hx = (static_cast<float>(BHHash(Step * 31 + Idx * 13) & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
				const bool bBig = (BHHash(Step * 9 + Idx) & 0xFF) > 150;
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(Hx * (bBig ? 120.0f : 24.0f), 0.0f)));
			}))
			[
				SNew(SBorder)
				.BorderImage(BootWhiteBrush())
				.BorderBackgroundColor(TAttribute<FSlateColor>::Create([this, Idx]()
				{
					if (Elapsed < BSODCatchTime() || Elapsed >= BlackStartTime())
					{
						return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
					}
					const int32 Step = FMath::FloorToInt(Elapsed * 19.0f);
					if ((BHHash(Step * 7 + Idx * 5) & 0xFF) < 96)
					{
						return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // empty this step
					}
					const float H = static_cast<float>(BHHash(Step * 11 + Idx * 3) & 0xFFFF) / 65535.0f;
					const FLinearColor C = (H < 0.34f) ? FLinearColor(1.0f, 1.0f, 1.0f, 0.85f)
						: (H < 0.60f) ? FLinearColor(0.95f, 0.12f, 0.12f, 0.80f)
						: (H < 0.84f) ? FLinearColor(0.20f, 0.55f, 1.0f, 0.80f)
						: FLinearColor(0.02f, 0.02f, 0.03f, 0.92f);
					return FSlateColor(C);
				}))
			];
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(BootWhiteBrush())
			.BorderBackgroundColor(BsodBlue)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(60.0f, 0.0f, 60.0f, 0.0f)
		[
			SNew(SBox)
			.MaxDesiredWidth(940.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 20.0f)
				[
					SNew(STextBlock)
					.Font(BootFont(96))
					.ColorAndOpacity(White)
					.Text_Lambda([this]() { return GetBSODFaceText(); })
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 14.0f)
				[
					SNew(STextBlock)
					.Font(BootFont(22))
					.ColorAndOpacity(White)
					.AutoWrapText(true)
					.Text(FText::FromString(TEXT("Your facility ran into a problem and needs to restart. We're just collecting some error info, and then it will restart for you.")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 22.0f)
				[
					SNew(STextBlock)
					.Font(BootFont(22, FName(TEXT("Bold"))))
					.ColorAndOpacity(White)
					.Text_Lambda([this]() { return GetBSODProgressText(); })
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(BootFont(13))
					.ColorAndOpacity(Faint)
					.Text(FText::FromString(TEXT("Stop code: FACILITY_CONTAINMENT_FAILURE")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(BootFont(13))
					.ColorAndOpacity(Faint)
					.Text(FText::FromString(TEXT("What failed: blackout.sys")))
				]
			]
		]
		// Broken-screen corruption layer: horizontal tear bars that rip + recolour during the catch beat.
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)
			+ SVerticalBox::Slot().FillHeight(0.55f)[ SNew(SBox) ]
			+ SVerticalBox::Slot().AutoHeight()[ MakeTearBar(0, 22.0f) ]
			+ SVerticalBox::Slot().FillHeight(0.90f)[ SNew(SBox) ]
			+ SVerticalBox::Slot().AutoHeight()[ MakeTearBar(1, 13.0f) ]
			+ SVerticalBox::Slot().FillHeight(0.70f)[ SNew(SBox) ]
			+ SVerticalBox::Slot().AutoHeight()[ MakeTearBar(2, 30.0f) ]
			+ SVerticalBox::Slot().FillHeight(1.20f)[ SNew(SBox) ]
			+ SVerticalBox::Slot().AutoHeight()[ MakeTearBar(3, 11.0f) ]
			+ SVerticalBox::Slot().FillHeight(0.50f)[ SNew(SBox) ]
			+ SVerticalBox::Slot().AutoHeight()[ MakeTearBar(4, 18.0f) ]
			+ SVerticalBox::Slot().FillHeight(1.00f)[ SNew(SBox) ]
		];
}

EVisibility SBHBootConsole::GetBSODVisibility() const
{
	return InBSOD() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText SBHBootConsole::GetBSODProgressText() const
{
	// Discreet "hijack" curve: load normally, FREEZE for a beat, then slowly resume loading. No garble --
	// the only tells are the suspicious pause, the angry face, and the unnaturally slow crawl afterward.
	const float T = Elapsed - BSODStartTime();
	const float RampEnd = 0.9f;     // load 0 -> 62%
	const float FreezeEnd = 1.9f;   // hold at 62% (the freeze)
	const float HoldPct = 0.62f;
	float P;
	if (T < RampEnd)
	{
		P = (T / RampEnd) * HoldPct;
	}
	else if (T < FreezeEnd)
	{
		P = HoldPct; // frozen
	}
	else if (Elapsed < BSODCatchTime())
	{
		// Slow, unnatural crawl 62% -> ~93%.
		const float SlowSpan = FMath::Max(0.1f, (BSODCatchTime() - BSODStartTime()) - FreezeEnd);
		P = HoldPct + FMath::Clamp((T - FreezeEnd) / SlowSpan, 0.0f, 1.0f) * (0.93f - HoldPct);
	}
	else
	{
		P = 1.0f; // caught -- the takeover snaps it straight to 100%
	}
	return FText::FromString(FString::Printf(TEXT("%d%% complete"), FMath::RoundToInt(P * 100.0f)));
}

FText SBHBootConsole::GetBSODFaceText() const
{
	// Sad :( until the freeze hits, then it quietly turns angry >:( -- the discreet "something took over".
	const float T = Elapsed - BSODStartTime();
	if (Elapsed >= BSODCatchTime())
	{
		return FText::FromString(TEXT(">:]")); // caught -- the quiet grin of whatever just took over
	}
	return FText::FromString(T >= 0.9f ? TEXT(">:(") : TEXT(":("));
}

void SBHBootConsole::PlayBootSound(const TCHAR* AssetPath, float Volume, float Pitch)
{
	UWorld* World = SoundWorld.Get();
	if (!World || !AssetPath)
	{
		return;
	}
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, AssetPath))
	{
		UGameplayStatics::PlaySound2D(World, Sound, Volume, Pitch);
	}
}

void SBHBootConsole::UpdateBootSounds()
{
	if (!SoundWorld.IsValid())
	{
		return;
	}

	// Existing project audio repurposed into a machine-boot-then-crash soundscape.
	static const TCHAR* SfxHum    = TEXT("/Game/SecurityCameras/Sounds/Wav_Electricity.Wav_Electricity");
	static const TCHAR* SfxBeep   = TEXT("/Game/BlackoutHunt/Audio/SW_MenuClick.SW_MenuClick");
	static const TCHAR* SfxStatic = TEXT("/Game/SecurityCameras/Sounds/Wav_Static_CCTV.Wav_Static_CCTV");
	static const TCHAR* SfxImpact = TEXT("/Game/SoundsOfHorror/Impacts/CUE/CUE_SOH_IP_03.CUE_SOH_IP_03");
	static const TCHAR* SfxRumble = TEXT("/Game/SoundsOfHorror/Rumbles/CUE/CUE_SOH_RU_04.CUE_SOH_RU_04");

	// Low electric machine hum as the hardware powers up.
	if (!bSfxHum && Elapsed >= 0.15f)
	{
		bSfxHum = true;
		PlayBootSound(SfxHum, 0.30f, 0.80f);
	}

	// Sparse data ticks while the RAW boot log floods during the pre-roll (before the structured asset stream).
	if (Elapsed < PrerollSeconds && Elapsed >= NextBeepTime)
	{
		const uint32 R = BHHash(static_cast<uint32>(FMath::FloorToInt(Elapsed * 1000.0f)));
		NextBeepTime = Elapsed + 0.45f + static_cast<float>(R & 0xFF) / 255.0f * 0.7f;
		PlayBootSound(SfxBeep, 0.14f, 0.85f + static_cast<float>(R & 0x7) * 0.06f);
	}

	// CHECKLIST: as the structured boot stream confirms each asset, tick once per line the instant it completes.
	// Green/teal "OK / ONLINE / DONE" lines (and the progress bars) get a light, crisp confirm tick; amber
	// "DEGRADED / OFFLINE / ARMED" warnings get a low dull blip; and the red "FAIL / NOT RESPONDING / KERNEL
	// PANIC" lines near the end get a heavy, descending fail thunk -- the audible collapse just before the crash.
	while (NextChecklistLine < BootLines.Num() && LineFraction(NextChecklistLine) >= 1.0f)
	{
		const FString DoneTagUpper = BootLines[NextChecklistLine].DoneTag.ToUpper();
		const bool bFail = DoneTagUpper.Contains(TEXT("FAIL")) || DoneTagUpper.Contains(TEXT("RESPONDING")) || DoneTagUpper.Contains(TEXT("PANIC"));
		const bool bWarn = DoneTagUpper.Contains(TEXT("DEGRADED")) || DoneTagUpper.Contains(TEXT("OFFLINE")) || DoneTagUpper.Contains(TEXT("ARMED"));
		if (bFail)
		{
			PlayBootSound(SfxImpact, 0.45f, 0.55f);
			PlayBootSound(SfxStatic, 0.20f, 0.90f);
		}
		else if (bWarn)
		{
			PlayBootSound(SfxBeep, 0.30f, 0.55f);
		}
		else
		{
			PlayBootSound(SfxBeep, 0.22f, 1.18f);
		}
		++NextChecklistLine;
	}

	// SYSTEM FAILURE: a hard impact + a burst of static as it crashes.
	if (!bSfxCrash && Elapsed >= GlitchStartTime())
	{
		bSfxCrash = true;
		PlayBootSound(SfxImpact, 0.95f, 1.0f);
		PlayBootSound(SfxStatic, 0.55f, 1.0f);
	}

	// Each stacked error dialog pops with a sharp tick.
	while (NextDialogSfx < 6 && Elapsed >= GlitchStartTime() + CrashDialogDelays[NextDialogSfx])
	{
		PlayBootSound(SfxBeep, 0.5f, 0.6f);
		++NextDialogSfx;
	}

	// Blue screen: a low static bed underneath it.
	if (!bSfxBsod && Elapsed >= BSODStartTime())
	{
		bSfxBsod = true;
		PlayBootSound(SfxStatic, 0.35f, 0.85f);
	}

	// The "caught it" hijack hit: a glitch impact + a sharp electric snap as the screen breaks.
	if (!bSfxCaught && Elapsed >= BSODCatchTime())
	{
		bSfxCaught = true;
		PlayBootSound(SfxImpact, 0.80f, 0.60f);
		PlayBootSound(SfxHum, 0.60f, 1.50f);
	}

	// Blackout / reboot: a deep rumble as everything cuts to black.
	if (!bSfxBlackout && Elapsed >= BlackStartTime())
	{
		bSfxBlackout = true;
		PlayBootSound(SfxRumble, 0.80f, 0.85f);
	}
}

float SBHBootConsole::FxFlashAlpha() const
{
	if (bReducedFlash)
	{
		return 0.0f;
	}
	// Sharp white blowout at each crash transition: failure onset, BSOD cut, reboot to black.
	const float HitDur = 0.16f;
	const float Hits[3] = { GlitchStartTime(), BSODStartTime(), BlackStartTime() };
	float A = 0.0f;
	for (float T : Hits)
	{
		if (Elapsed >= T && Elapsed < T + HitDur)
		{
			A = FMath::Max(A, 1.0f - (Elapsed - T) / HitDur);
		}
	}
	return A;
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
			// Shake ramps up hard as the glitch progresses.
			const float Mag = 4.0f + 16.0f * GlitchProgress();
			const FVector2D Offset(FMath::FRandRange(-Mag, Mag), FMath::FRandRange(-Mag, Mag));
			JitterLayer->SetRenderTransform(FSlateRenderTransform(Offset));
		}
		else
		{
			JitterLayer->SetRenderTransform(TOptional<FSlateRenderTransform>());
		}
	}

	// Drive the boot SFX (machine hum, crash impact, dialog ticks, the catch hit, the blackout rumble).
	UpdateBootSounds();

	// "Caught it" beat: ~0.55s before the cut to black, the system is hijacked and the BSOD "breaks" like a
	// failing digital signal -- a HARD, stepped tear (the whole frame snaps to choppy offsets, with the odd
	// big horizontal rip), NOT a smooth wobble. The colour-flash tear bars live in BuildBSODScreen.
	if (BSODLayer.IsValid())
	{
		if (InBSOD() && !bReducedFlash && Elapsed >= BSODCatchTime())
		{
			const float W = Elapsed - BSODCatchTime();
			const float Span = FMath::Max(0.1f, BlackStartTime() - BSODCatchTime());
			const float Intensity = FMath::Clamp(1.0f - W / Span, 0.25f, 1.0f);
			const int32 Step = FMath::FloorToInt(Elapsed * 19.0f); // choppy: ~19 discrete steps/sec
			const float Hx = (static_cast<float>(BHHash(Step * 23 + 5) & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
			const float Hy = (static_cast<float>(BHHash(Step * 7 + 11) & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
			const bool bBigRip = (BHHash(Step * 5 + 3) & 0xFF) > 178; // occasional large horizontal rip
			const float Ox = Hx * (bBigRip ? 46.0f : 9.0f) * Intensity;
			const float Oy = Hy * 5.0f * Intensity;
			BSODLayer->SetRenderTransform(FSlateRenderTransform(FVector2D(Ox, Oy)));
		}
		else
		{
			BSODLayer->SetRenderTransform(TOptional<FSlateRenderTransform>());
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
	// Key-only, double-press skip: the first key ARMS the skip (the hint updates); the second key skips.
	if (!bSkipArmed)
	{
		bSkipArmed = true;
		return FReply::Handled();
	}
	RequestSkip();
	return FReply::Handled();
}

FReply SBHBootConsole::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Mouse clicks do NOT skip the intro (skipping is key-only, by design). Consume the click so it has no effect.
	return FReply::Handled();
}

bool SBHBootConsole::InGlitch() const
{
	return Elapsed >= GlitchStartTime() && Elapsed < BSODStartTime();
}

bool SBHBootConsole::InPreroll() const
{
	return Elapsed < PrerollSeconds;
}

bool SBHBootConsole::InBSOD() const
{
	return Elapsed >= BSODStartTime() && Elapsed < BlackStartTime();
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
	return FMath::Clamp((BootElapsed() - Start) / Duration, 0.0f, 1.0f);
}

bool SBHBootConsole::IsLineVisible(int32 Index) const
{
	return BootElapsed() >= LineStartTime(Index) - KINDA_SMALL_NUMBER;
}

float SBHBootConsole::OverallFraction() const
{
	return (BootScriptDuration > KINDA_SMALL_NUMBER) ? FMath::Clamp(BootElapsed() / BootScriptDuration, 0.0f, 1.0f) : 1.0f;
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
	// Visible from the end of the pre-roll through the glitch; gone once the BSOD takes over.
	return (InPreroll() || Elapsed >= BSODStartTime()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

EVisibility SBHBootConsole::GetChromeVisibility() const
{
	// Top scanline: present through pre-roll, boot and glitch, gone once the BSOD takes over.
	return (Elapsed >= BSODStartTime()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

EVisibility SBHBootConsole::GetPrerollVisibility() const
{
	return InPreroll() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText SBHBootConsole::GetPrerollLineText(int32 Row) const
{
	if (PrerollLines.Num() == 0)
	{
		return FText::GetEmpty();
	}
	const int32 Scroll = FMath::Max(0, FMath::FloorToInt(Elapsed * BHPrerollLinesPerSecond));
	const int32 LineIndex = Scroll + Row;
	const uint32 H = BHHash(static_cast<uint32>(LineIndex) * 2654435761u + 17u);
	FString Line = PrerollLines[H % static_cast<uint32>(PrerollLines.Num())];
	if (Line.Contains(TEXT("$A")))
	{
		Line.ReplaceInline(TEXT("$A"), *FString::Printf(TEXT("%08X%08X"), BHHash(H ^ 0xA5u), BHHash(H ^ 0x5Au)));
	}
	if (Line.Contains(TEXT("$X")))
	{
		Line.ReplaceInline(TEXT("$X"), *FString::Printf(TEXT("%08X"), BHHash(H ^ 0x33u)));
	}
	if (Line.Contains(TEXT("$N")))
	{
		Line.ReplaceInline(TEXT("$N"), *FString::FromInt(static_cast<int32>(BHHash(H ^ 0x77u) % 100000u)));
	}
	// Monotonically increasing fake timestamp so the flood reads like a real log.
	const float Ts = 0.42f + LineIndex * 0.019f;
	return FText::FromString(FString::Printf(TEXT("[%08.3f] %s"), Ts, *Line));
}

FSlateColor SBHBootConsole::GetPrerollLineColor(int32 Row) const
{
	FLinearColor Base(0.42f, 0.70f, 0.50f, 1.0f); // dim terminal green
	if (PrerollLines.Num() > 0)
	{
		const int32 Scroll = FMath::Max(0, FMath::FloorToInt(Elapsed * BHPrerollLinesPerSecond));
		const uint32 H = BHHash(static_cast<uint32>(Scroll + Row) * 2654435761u + 17u);
		const FString& Line = PrerollLines[H % static_cast<uint32>(PrerollLines.Num())];
		// Default Contains is case-insensitive: catches "[ WARN ]", "[FAILED]", "kernel panic", etc.
		if (Line.Contains(TEXT("WARN")))
		{
			Base = BootAmber;
		}
		else if (Line.Contains(TEXT("FAIL")) || Line.Contains(TEXT("panic")))
		{
			Base = BootRed;
		}
	}
	// Older lines near the top fade out, so the flood feels like it is scrolling away upward.
	const int32 Denom = FMath::Max(1, BHPrerollRowCount - 1);
	const float Fade = FMath::Clamp(0.25f + 0.75f * (static_cast<float>(Row) / static_cast<float>(Denom)), 0.0f, 1.0f);
	return FSlateColor(FLinearColor(Base.R, Base.G, Base.B, Base.A * Fade));
}

EVisibility SBHBootConsole::GetGlitchFlashVisibility() const
{
	// Active through the glitch and the transition flashes (failure onset / BSOD cut / reboot).
	const bool bActive = !bReducedFlash && Elapsed >= GlitchStartTime() && Elapsed < BlackStartTime() + 0.25f;
	return bActive ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FSlateColor SBHBootConsole::GetGlitchFlashColor() const
{
	// Sharp white blowout at each crash transition takes priority over the glitch flicker.
	const float Hit = FxFlashAlpha();
	if (Hit > 0.0f)
	{
		return FSlateColor(FLinearColor(1.0f, 0.96f, 0.96f, FMath::Min(0.92f, Hit)));
	}
	// Flicker only during the glitch storm; the BSOD stays clean (the hijack is discreet).
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

FText SBHBootConsole::GetFailureSubText() const
{
	return FText::FromString(GlitchString(TEXT("CONTAINMENT LOST"), 7777));
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
	// Dialogs storm in during the glitch and vanish when the BSOD takes over.
	if (Elapsed >= BSODStartTime() || DialogIndex < 0 || DialogIndex >= 6)
	{
		return EVisibility::Collapsed;
	}
	const float Appear = GlitchStartTime() + CrashDialogDelays[DialogIndex];
	return (Elapsed >= Appear) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}
