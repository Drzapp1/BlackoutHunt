#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "Containers/Ticker.h"
#include "HAL/CriticalSection.h"
#include "HttpFwd.h"
#include "Misc/OutputDevice.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHFeedbackSubsystem.generated.h"

class FJsonObject;

UENUM(BlueprintType)
enum class EBHFeedbackKind : uint8
{
	Bug UMETA(DisplayName = "Bug report"),
	Idea UMETA(DisplayName = "Feature idea"),
	Praise UMETA(DisplayName = "What you liked"),
	Other UMETA(DisplayName = "Other"),
	Survey UMETA(DisplayName = "End-of-round survey")
};

// Thread-safe ring buffer that captures recent log output for diagnostics. GLog can call
// Serialize from any thread, so all access is guarded by a critical section.
class FBHFeedbackLogSink : public FOutputDevice
{
public:
	FBHFeedbackLogSink(int32 InCapacity, ELogVerbosity::Type InMinVerbosity);

	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

	// Returns the captured lines oldest-first.
	TArray<FString> Snapshot() const;

private:
	mutable FCriticalSection Mutex;
	TArray<FString> Lines;
	int32 Capacity = 0;
	int32 Head = 0;  // index of the next slot to write
	int32 Count = 0; // number of valid entries (<= Capacity)
	ELogVerbosity::Type MinVerbosity = ELogVerbosity::Warning;
};

// One finished round, accumulated into the session telemetry summary.
struct FBHFeedbackRoundResult
{
	FString Role;
	FString Result;
	FString Level;
	bool bSurvived = false;
	FString AtUtc;
};

// Collects in-game feedback (bug reports, feature ideas, end-of-round surveys) and, with
// consent, lightweight performance + recent-log diagnostics, and submits them to the configured
// backend so they reach the project owner instead of staying on the player's machine. Falls back
// to a local Saved/Feedback file when no backend is configured or a submission fails.
UCLASS()
class BLACKOUTHUNT_API UBHFeedbackSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Submit a typed-in bug report / feature idea / praise / other. Returns false (with OutMessage)
	// when there is nothing to send (e.g. an empty message). The network call is asynchronous.
	bool SubmitFeedback(EBHFeedbackKind Kind, const FString& Message, int32 Rating, const FString& Contact, bool bIncludeDiagnostics, FString& OutMessage);

	// Submit the quick end-of-round survey. Comment is optional.
	bool SubmitRoundSurvey(int32 OverallRating, const FString& Difficulty, bool bWouldRecommend, const FString& Comment, FString& OutMessage);

	// Record a finished round into the session telemetry summary and arm the survey prompt.
	void RecordRoundResult(EBHPlayerRole Role, EBHPlayerLifeState LifeState, EBHRoundPhase ResultPhase);

	// Send the accumulated performance + recent-log session summary (no-op if telemetry is off).
	bool SubmitSessionTelemetry(const FString& Reason, FString& OutMessage);

	bool IsFeedbackEnabled() const;
	bool IsPerformanceTelemetryEnabled() const;
	bool IsBackendConfigured() const;
	bool ShouldIncludeDiagnosticsByDefault() const;
	bool IsEndOfRoundSurveyPending() const { return bEndOfRoundSurveyPending; }
	void ClearEndOfRoundSurveyPending() { bEndOfRoundSurveyPending = false; }
	// Arm the survey prompt without recording a telemetry round result. Used by the round-phase
	// transition path, which may fire independently of the ClientRecordRoundResult RPC.
	void ArmEndOfRoundSurvey() { bEndOfRoundSurveyPending = true; }
	// True when a round just ended, the auto-prompt setting is on, and we have not yet prompted this session.
	bool ShouldAutoPromptEndOfRoundSurvey() const;
	void MarkEndOfRoundSurveyShown() { bEndOfRoundSurveyShownThisSession = true; }
	const FString& GetLastStatusMessage() const { return LastStatusMessage; }
	FString GetPrivacySummary() const;

	UFUNCTION(Exec)
	void FeedbackSend(const FString& Message);

	UFUNCTION(Exec)
	void FeedbackSendDiagnostics();

private:
	bool TickPerf(float DeltaTime);
	FString ResolveBackendBaseUrl() const;
	void GatherContext(const TSharedRef<FJsonObject>& Out) const;
	TSharedRef<FJsonObject> BuildDeviceJson() const;
	TSharedRef<FJsonObject> BuildPerfJson() const;
	TSharedRef<FJsonObject> BuildGraphicsJson() const;
	TSharedRef<FJsonObject> BuildDiagnosticsJson(bool bIncludeLogs) const;
	// Posts Body to BaseUrl + PathSuffix, or saves locally when no backend is configured.
	void PostJson(const FString& PathSuffix, const TSharedRef<FJsonObject>& Body, const FString& SubmittingMessage, const FString& SuccessMessage, const FString& FallbackTag);
	void HandlePostComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString FallbackJson, FString FallbackTag, FString SuccessMessage);
	bool SaveLocalFallback(const FString& Tag, const FString& JsonString, FString& OutPath) const;
	void SetStatus(const FString& Message);

	FString SessionId;
	double SessionStartSeconds = 0.0;

	// Per-frame performance sampling.
	FTSTicker::FDelegateHandle PerfTickHandle;
	bool bPerfFirstSample = true;
	int64 PerfFrameCount = 0;
	double PerfFrameTimeSum = 0.0;
	float PerfMinFps = 0.0f;
	float PerfMaxFps = 0.0f;
	int32 PerfHitchCount = 0;
	float HitchThresholdSeconds = 0.1f;
	static constexpr int32 PerfHistogramBins = 256;
	TArray<int32> PerfFpsHistogram;

	TUniquePtr<FBHFeedbackLogSink> LogSink;
	TArray<FBHFeedbackRoundResult> RoundResults;

	FString LastStatusMessage;
	bool bEndOfRoundSurveyPending = false;
	bool bEndOfRoundSurveyShownThisSession = false;
};
