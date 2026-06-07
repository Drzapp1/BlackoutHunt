// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Widgets/SCompoundWidget.h"

class SWidget;

// Cosmetic cold-boot "bare-metal" terminal. Streams a scripted facility-mainframe boot log
// (POST, shader compile, subsystem bring-up, blackout protocol) with a master loading bar, then
// fires OnFinished so the owner can hand off to the main menu. Purely decorative: the lines are
// scripted flavor, not real engine progress. Shown once per process on a clean Standalone launch
// (see ABHPlayerController::ShowBootConsole) and skippable with any key or click.
class SBHBootConsole : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBHBootConsole)
		: _ReducedFlash(false)
	{}
		// Fired once when the sequence completes naturally or the player skips it. The owner removes
		// the widget and shows the menu; this widget never tears itself down.
		SLATE_EVENT(FSimpleDelegate, OnFinished)
		// When true (accessibility comfort setting) the blinking cursor and spinner are held steady.
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
		// Relative time weight; normalized so the whole script runs for BootScriptDuration seconds.
		float Duration = 1.0f;
		// Progress lines show an animated fill bar + percent; non-progress lines type the label then
		// settle on DoneTag.
		bool bProgress = false;
		// Right-aligned status tag for non-progress lines ("OK", "DEGRADED", "READY", ...).
		FString DoneTag;
		// Colour of the status tag (and of the label itself for header lines).
		FLinearColor TagColor = FLinearColor(0.42f, 1.0f, 0.55f, 1.0f);
		// Header lines brighten the whole label (used for the mainframe / "all systems" banners).
		bool bHeader = false;
	};

	void BuildBootScript();
	TSharedRef<SWidget> BuildLogBody();
	// Fixed-size fill bar built from two tinted borders; the fill width is driven each frame by FracFn.
	// Glyph-independent (no block characters), so it renders identically regardless of the loaded font.
	TSharedRef<SWidget> MakeFillBar(float Width, float Height, TFunction<float()> FracFn, FLinearColor FillColor) const;
	void Finish();

	// Per-line state derived from Elapsed.
	float LineStartTime(int32 Index) const;
	float LineFraction(int32 Index) const;       // 0..1 typing/fill progress for this line
	bool IsLineVisible(int32 Index) const;        // has the stream reached this line yet
	float OverallFraction() const;                // master loading bar 0..1

	FText GetLabelText(int32 Index) const;
	FSlateColor GetLabelColor(int32 Index) const;
	EVisibility GetLineVisibility(int32 Index) const;
	FText GetTagText(int32 Index) const;          // spinner while active, DoneTag once complete
	FSlateColor GetTagColor(int32 Index) const;
	FText GetPercentText(int32 Index) const;      // for progress lines
	FText GetCursorText() const;                  // blinking prompt cursor
	FText GetMasterPercentText() const;

	TArray<FBHBootLine> BootLines;
	// Cumulative start time (seconds) of each line, computed in BuildBootScript.
	TArray<float> LineStartTimes;
	// End of the scripted typing phase (seconds); fixed regardless of how many lines.
	float BootScriptDuration = 5.0f;

	float Elapsed = 0.0f;
	bool bReducedFlash = false;
	bool bFinished = false;
	FSimpleDelegate OnFinishedDelegate;
};
