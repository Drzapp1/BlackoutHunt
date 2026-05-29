#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BHFeedbackSettings.generated.h"

// Project settings for in-game feedback (bug reports / feature ideas / surveys) and
// performance + log telemetry. Editable in Project Settings and via Config/DefaultGame.ini
// under [/Script/BlackoutHunt.BHFeedbackSettings].
UCLASS(Config = Game, DefaultConfig)
class BLACKOUTHUNT_API UBHFeedbackSettings : public UObject
{
	GENERATED_BODY()

public:
	// Optional override for the feedback/telemetry endpoint. When empty, the feedback
	// subsystem uses UBHAccountSettings::BackendBaseUrl (the shared account backend).
	UPROPERTY(Config, EditAnywhere, Category = "Feedback")
	FString FeedbackBackendBaseUrl;

	// Master switch for the whole feedback + telemetry feature.
	UPROPERTY(Config, EditAnywhere, Category = "Feedback")
	bool bEnableFeedback = true;

	// Collect a per-session FPS/hitch summary, recent log tail, and device info, and submit it
	// to the backend (at round end and on exit). Disable to send only manually-typed feedback.
	UPROPERTY(Config, EditAnywhere, Category = "Feedback")
	bool bEnablePerformanceTelemetry = true;

	// Whether the "Include diagnostics" box on the feedback form starts ticked.
	UPROPERTY(Config, EditAnywhere, Category = "Feedback")
	bool bIncludeDiagnosticsByDefault = true;

	// Auto-open the quick end-of-round survey when a round finishes (once per session).
	UPROPERTY(Config, EditAnywhere, Category = "Feedback")
	bool bAutoPromptEndOfRoundSurvey = true;

	// How many recent log lines to keep in the in-memory ring buffer for diagnostics.
	UPROPERTY(Config, EditAnywhere, Category = "Feedback", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 RecentLogLineCount = 200;

	// Minimum log verbosity captured into the diagnostics ring buffer: one of
	// "Error", "Warning", "Display", "Log", "Verbose", "VeryVerbose". Anything more verbose is ignored.
	UPROPERTY(Config, EditAnywhere, Category = "Feedback")
	FString MinimumLogVerbosity = TEXT("Warning");

	// A single frame slower than this many milliseconds counts as a hitch in the perf summary.
	UPROPERTY(Config, EditAnywhere, Category = "Feedback", meta = (ClampMin = "10", ClampMax = "1000"))
	float HitchThresholdMs = 100.0f;
};
