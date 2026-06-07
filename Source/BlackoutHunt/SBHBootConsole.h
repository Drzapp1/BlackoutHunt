// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Widgets/SCompoundWidget.h"

class SWidget;

// Cosmetic cold-boot "bare-metal" terminal. Streams a scripted facility-mainframe boot log
// (POST, shader compile, subsystem bring-up, blackout protocol) that ESCALATES INTO A FAKE SYSTEM
// CRASH -- the screen corrupts/glitches, Windows-style error dialogs stack up, then everything snaps
// to black and the console fades its own opacity out to reveal the main menu placed underneath
// (so the start screen "slowly fades in").
//
// Lifecycle handshake with ABHPlayerController:
//   * OnReadyForMenu fires once the screen is fully black -- the owner shows the main menu BENEATH
//     this widget (lower ZOrder) so it is ready when the fade begins.
//   * OnFinished fires when the reveal fade completes -- the owner removes this widget.
// Either path ultimately reaches the menu; a skip (any key/click) jumps straight to OnFinished.
//
// Purely decorative: the lines/crash are scripted flavor, not real engine progress. Shown once per
// process on a clean Standalone launch (see ABHPlayerController::ShowBootConsole) and fully skippable.
// Honors the reduced-flash comfort setting (no strobe/shake -- a calmer static corruption instead).
class SBHBootConsole : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBHBootConsole)
		: _ReducedFlash(false)
	{}
		// Fired once when the screen has gone fully black, just before the reveal fade. The owner should
		// place the main menu underneath this widget (this widget stays on top and fades out over it).
		SLATE_EVENT(FSimpleDelegate, OnReadyForMenu)
		// Fired once when the reveal fade completes or the player skips. The owner removes this widget.
		SLATE_EVENT(FSimpleDelegate, OnFinished)
		// When true (accessibility comfort setting) the strobe, shake and per-frame text scramble are
		// suppressed in favour of a steady, low-contrast corruption.
		SLATE_ARGUMENT(bool, ReducedFlash)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	struct FBHBootLine
	{
		FString Label;
		// Relative time weight; normalized so the whole stream runs for BootScriptDuration seconds.
		float Duration = 1.0f;
		// Progress lines show an animated fill bar + percent; non-progress lines type the label then
		// settle on DoneTag.
		bool bProgress = false;
		// Right-aligned status tag for non-progress lines ("OK", "DEGRADED", "FAIL", ...).
		FString DoneTag;
		// Colour of the status tag (and of the label itself for header lines).
		FLinearColor TagColor = FLinearColor(0.42f, 1.0f, 0.55f, 1.0f);
		// Header lines brighten the whole label (used for the mainframe / failure banners).
		bool bHeader = false;
	};

	void BuildBootScript();
	TSharedRef<SWidget> BuildLogBody();
	TSharedRef<SWidget> BuildCrashDialogLayer();
	TSharedRef<SWidget> MakeCrashDialog(const FString& Title, const FString& Line1, const FString& Line2) const;
	// Fixed-size fill bar built from two tinted borders; the fill width is driven each frame by FracFn.
	TSharedRef<SWidget> MakeFillBar(float Width, float Height, TFunction<float()> FracFn, FLinearColor FillColor) const;
	void RequestSkip();
	// Idempotently end the sequence: zero this widget's opacity and fire OnFinished so the owner removes it.
	void Finish();

	// ---- timeline ----------------------------------------------------------------------------------
	// [0, BootScriptDuration]      boot stream
	// [GlitchStart, BlackStart]    crash: glitch/shake/scramble + stacking error dialogs
	// [BlackStart, FadeStart]      fully black, brief hold (OnReadyForMenu fires here)
	// [FadeStart, TotalDuration]   console opacity 1->0, revealing the menu underneath
	float GlitchStartTime() const { return BootScriptDuration; }
	float BlackStartTime() const { return GlitchStartTime() + GlitchSeconds; }
	float FadeStartTime() const { return BlackStartTime() + BlackHoldSeconds; }
	float TotalDuration() const { return FadeStartTime() + MenuFadeSeconds; }
	bool InGlitch() const;
	bool IsBlackedOut() const;        // Elapsed >= BlackStart: hide all content, show only the black field
	float GlitchProgress() const;     // 0..1 across the glitch window

	// Per-line state derived from Elapsed.
	float LineStartTime(int32 Index) const;
	float LineFraction(int32 Index) const;
	bool IsLineVisible(int32 Index) const;
	float OverallFraction() const;

	FText GetLabelText(int32 Index) const;
	FSlateColor GetLabelColor(int32 Index) const;
	EVisibility GetLineVisibility(int32 Index) const;
	FText GetTagText(int32 Index) const;
	FSlateColor GetTagColor(int32 Index) const;
	FText GetPercentText(int32 Index) const;
	FText GetCursorText() const;
	FText GetMasterPercentText() const;
	FSlateColor GetMasterBarColor() const;

	// Crash / glitch visuals.
	EVisibility GetTerminalVisibility() const;          // collapses when blacked out
	EVisibility GetGlitchFlashVisibility() const;
	FSlateColor GetGlitchFlashColor() const;            // full-screen flicker tint
	EVisibility GetFailureBannerVisibility() const;
	FText GetFailureBannerText() const;
	FSlateColor GetFailureBannerColor() const;
	EVisibility GetCrashDialogVisibility(int32 DialogIndex) const;

	// Corrupts a string with random glyphs, scaling with the glitch progress. Steady (non-flickering)
	// when reduced-flash is on.
	FString GlitchString(const FString& In, int32 Salt) const;

	TArray<FBHBootLine> BootLines;
	TArray<float> LineStartTimes;
	float BootScriptDuration = 5.0f;
	float GlitchSeconds = 2.2f;
	// Dead-air beat on a fully black screen after the crash, before the start screen fades in.
	float BlackHoldSeconds = 2.0f;
	float MenuFadeSeconds = 1.6f;
	// Times (relative to GlitchStart) at which each stacked crash dialog pops in.
	float CrashDialogDelays[3] = { 0.15f, 0.6f, 1.05f };

	// The shaken content group (terminal + banner + dialogs); the static black backdrop sits behind it
	// so the shake never reveals a hard edge.
	TSharedPtr<SWidget> JitterLayer;

	float Elapsed = 0.0f;
	bool bReducedFlash = false;
	bool bMenuRevealFired = false;
	bool bFinished = false;
	FSimpleDelegate OnReadyForMenuDelegate;
	FSimpleDelegate OnFinishedDelegate;
};
