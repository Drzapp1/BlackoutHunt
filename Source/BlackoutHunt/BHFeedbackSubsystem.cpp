#include "BHFeedbackSubsystem.h"

#include "BHAccountSettings.h"
#include "BHAccountSubsystem.h"
#include "BHFeedbackSettings.h"
#include "BHGameInstance.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 BHMaxFeedbackChars = 8000;
	constexpr int32 BHMaxContactChars = 200;
	constexpr int32 BHMaxLogLineChars = 1000;

	FString BHJsonToString(const TSharedRef<FJsonObject>& JsonObject)
	{
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return Output;
	}

	// Mirror of the account subsystem's normalization: trim trailing slashes and default to https://.
	FString BHNormalizeBackendBaseUrl(FString Url)
	{
		Url.TrimStartAndEndInline();
		if (Url.IsEmpty() || Url.Equals(TEXT("http:"), ESearchCase::IgnoreCase) || Url.Equals(TEXT("https:"), ESearchCase::IgnoreCase))
		{
			return FString();
		}

		if (!Url.Contains(TEXT("://")))
		{
			// Default to HTTPS so telemetry/feedback (which includes player identity) is not sent in cleartext.
			Url = FString(TEXT("https://")) + Url;
		}

		while (Url.EndsWith(TEXT("/")))
		{
			Url.LeftChopInline(1);
		}

		return Url;
	}

	ELogVerbosity::Type BHParseVerbosity(const FString& Name)
	{
		if (Name.Equals(TEXT("Error"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("Fatal"), ESearchCase::IgnoreCase))
		{
			return ELogVerbosity::Error;
		}
		if (Name.Equals(TEXT("Display"), ESearchCase::IgnoreCase))
		{
			return ELogVerbosity::Display;
		}
		if (Name.Equals(TEXT("Log"), ESearchCase::IgnoreCase))
		{
			return ELogVerbosity::Log;
		}
		if (Name.Equals(TEXT("Verbose"), ESearchCase::IgnoreCase))
		{
			return ELogVerbosity::Verbose;
		}
		if (Name.Equals(TEXT("VeryVerbose"), ESearchCase::IgnoreCase))
		{
			return ELogVerbosity::VeryVerbose;
		}
		return ELogVerbosity::Warning;
	}

	FString BHRoleToString(EBHPlayerRole Role)
	{
		switch (Role)
		{
		case EBHPlayerRole::Hunter: return TEXT("Hunter");
		case EBHPlayerRole::FakeHunter: return TEXT("FakeHunter");
		case EBHPlayerRole::Survivor: return TEXT("Survivor");
		case EBHPlayerRole::Tester: return TEXT("Tester");
		case EBHPlayerRole::Spectator: return TEXT("Spectator");
		default: return TEXT("Unassigned");
		}
	}

	FString BHPhaseToString(EBHRoundPhase Phase)
	{
		switch (Phase)
		{
		case EBHRoundPhase::Prep: return TEXT("Prep");
		case EBHRoundPhase::Hunt: return TEXT("Hunt");
		case EBHRoundPhase::Intermission: return TEXT("Intermission");
		case EBHRoundPhase::FinalEscape: return TEXT("FinalEscape");
		case EBHRoundPhase::SurvivorsWin: return TEXT("SurvivorsWin");
		case EBHRoundPhase::HunterWin: return TEXT("HunterWin");
		default: return TEXT("Lobby");
		}
	}

	FString BHFeedbackKindToString(EBHFeedbackKind Kind)
	{
		switch (Kind)
		{
		case EBHFeedbackKind::Bug: return TEXT("bug");
		case EBHFeedbackKind::Idea: return TEXT("idea");
		case EBHFeedbackKind::Praise: return TEXT("praise");
		case EBHFeedbackKind::Survey: return TEXT("survey");
		default: return TEXT("other");
		}
	}
}

// ---------------------------------------------------------------------------------------------
// FBHFeedbackLogSink
// ---------------------------------------------------------------------------------------------

FBHFeedbackLogSink::FBHFeedbackLogSink(int32 InCapacity, ELogVerbosity::Type InMinVerbosity)
	: Capacity(FMath::Max(0, InCapacity))
	, MinVerbosity(InMinVerbosity)
{
	if (Capacity > 0)
	{
		Lines.SetNum(Capacity);
	}
}

void FBHFeedbackLogSink::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (Capacity <= 0 || Message == nullptr)
	{
		return;
	}

	// SetColor (0x40) is a console-colour control flag outside the verbosity range, so it must be
	// tested before masking; drop it and empty/no-logging records.
	if ((Verbosity & ELogVerbosity::SetColor) != 0)
	{
		return;
	}
	const ELogVerbosity::Type Masked = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
	if (Masked == ELogVerbosity::NoLogging)
	{
		return;
	}
	// Lower numeric verbosity == more severe; keep entries at or above the configured floor.
	if (Masked > MinVerbosity)
	{
		return;
	}

	FString Line = FString::Printf(TEXT("%s: %s"), *Category.ToString(), Message);
	if (Line.Len() > BHMaxLogLineChars)
	{
		Line.LeftInline(BHMaxLogLineChars);
	}

	FScopeLock Lock(&Mutex);
	Lines[Head] = MoveTemp(Line);
	Head = (Head + 1) % Capacity;
	if (Count < Capacity)
	{
		++Count;
	}
}

TArray<FString> FBHFeedbackLogSink::Snapshot() const
{
	FScopeLock Lock(&Mutex);
	TArray<FString> Out;
	Out.Reserve(Count);
	const int32 Start = (Count == Capacity) ? Head : 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Out.Add(Lines[(Start + Index) % Capacity]);
	}
	return Out;
}

// ---------------------------------------------------------------------------------------------
// UBHFeedbackSubsystem
// ---------------------------------------------------------------------------------------------

void UBHFeedbackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	SessionStartSeconds = FPlatformTime::Seconds();
	PerfMinFps = TNumericLimits<float>::Max();
	PerfFpsHistogram.SetNumZeroed(PerfHistogramBins);

	const UBHFeedbackSettings* Settings = GetDefault<UBHFeedbackSettings>();
	const bool bFeedbackEnabled = Settings ? Settings->bEnableFeedback : true;
	if (!bFeedbackEnabled)
	{
		return;
	}

	if (Settings)
	{
		HitchThresholdSeconds = FMath::Max(0.01f, Settings->HitchThresholdMs / 1000.0f);
	}

	// Capture recent log output for diagnostics.
	const int32 LogCapacity = Settings ? Settings->RecentLogLineCount : 200;
	if (LogCapacity > 0 && GLog != nullptr)
	{
		const ELogVerbosity::Type MinVerbosity = BHParseVerbosity(Settings ? Settings->MinimumLogVerbosity : TEXT("Warning"));
		LogSink = MakeUnique<FBHFeedbackLogSink>(LogCapacity, MinVerbosity);
		GLog->AddOutputDevice(LogSink.Get());
	}

	// Sample frame time every frame for the performance summary.
	if (!Settings || Settings->bEnablePerformanceTelemetry)
	{
		PerfTickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UBHFeedbackSubsystem::TickPerf));
	}
}

void UBHFeedbackSubsystem::Deinitialize()
{
	if (PerfTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PerfTickHandle);
		PerfTickHandle.Reset();
	}

	// Remove the log device before destroying it; GLog holds a raw pointer.
	if (LogSink.IsValid() && GLog != nullptr)
	{
		GLog->RemoveOutputDevice(LogSink.Get());
	}
	LogSink.Reset();

	// Note: we intentionally do not start a new HTTP request here. Session diagnostics are
	// delivered at round end while the game is still running; a request started during shutdown
	// would not flush before the process exits.

	Super::Deinitialize();
}

bool UBHFeedbackSubsystem::TickPerf(float DeltaTime)
{
	// The first delta after registration measures time since the ticker was added, not a frame.
	if (bPerfFirstSample)
	{
		bPerfFirstSample = false;
		return true;
	}

	if (DeltaTime <= 0.0f)
	{
		return true;
	}

	const float Fps = 1.0f / DeltaTime;
	++PerfFrameCount;
	PerfFrameTimeSum += DeltaTime;
	PerfMinFps = FMath::Min(PerfMinFps, Fps);
	PerfMaxFps = FMath::Max(PerfMaxFps, Fps);

	const int32 Bin = FMath::Clamp(FMath::RoundToInt(Fps), 0, PerfHistogramBins - 1);
	PerfFpsHistogram[Bin] += 1;

	if (DeltaTime > HitchThresholdSeconds)
	{
		++PerfHitchCount;
	}

	return true; // keep ticking
}

bool UBHFeedbackSubsystem::IsFeedbackEnabled() const
{
	const UBHFeedbackSettings* Settings = GetDefault<UBHFeedbackSettings>();
	return Settings ? Settings->bEnableFeedback : true;
}

bool UBHFeedbackSubsystem::IsPerformanceTelemetryEnabled() const
{
	const UBHFeedbackSettings* Settings = GetDefault<UBHFeedbackSettings>();
	return Settings ? (Settings->bEnableFeedback && Settings->bEnablePerformanceTelemetry) : true;
}

bool UBHFeedbackSubsystem::ShouldIncludeDiagnosticsByDefault() const
{
	const UBHFeedbackSettings* Settings = GetDefault<UBHFeedbackSettings>();
	return Settings ? Settings->bIncludeDiagnosticsByDefault : true;
}

bool UBHFeedbackSubsystem::IsBackendConfigured() const
{
	return !ResolveBackendBaseUrl().IsEmpty();
}

bool UBHFeedbackSubsystem::ShouldAutoPromptEndOfRoundSurvey() const
{
	const UBHFeedbackSettings* Settings = GetDefault<UBHFeedbackSettings>();
	const bool bAutoPrompt = Settings ? Settings->bAutoPromptEndOfRoundSurvey : true;
	return bEndOfRoundSurveyPending && bAutoPrompt && !bEndOfRoundSurveyShownThisSession && IsFeedbackEnabled();
}

FString UBHFeedbackSubsystem::GetPrivacySummary() const
{
	if (IsBackendConfigured())
	{
		const UBHGameSettings* GameSettings = GetDefault<UBHGameSettings>();
		const bool bClassroom = GameSettings && GameSettings->bClassroomMode;
		if (bClassroom)
		{
			// Classroom builds anonymize identity (see GatherContext): no display name / account id is
			// attached, so the only personal data sent is whatever the student chooses to type.
			return TEXT("Your message is sent over the internet to the feedback service your host/teacher set up. Diagnostics (when included) add frame-rate stats, recent log lines, and PC specs. Your name is NOT attached - the only personal data sent is what you type, so please don't include personal details.");
		}
		return TEXT("Your message is sent over the internet to the feedback service your host configured (not just this PC). It includes your display name plus, when included, your frame-rate stats, recent log lines, and PC specs. No other personal data unless you type it.");
	}
	return TEXT("No feedback server is configured, so submissions are saved to this PC under Saved/Feedback until a server URL is set.");
}

FString UBHFeedbackSubsystem::ResolveBackendBaseUrl() const
{
	const UBHFeedbackSettings* FeedbackSettings = GetDefault<UBHFeedbackSettings>();
	FString Url = FeedbackSettings ? FeedbackSettings->FeedbackBackendBaseUrl : FString();
	if (Url.TrimStartAndEnd().IsEmpty())
	{
		const UBHAccountSettings* AccountSettings = GetDefault<UBHAccountSettings>();
		Url = AccountSettings ? AccountSettings->BackendBaseUrl : FString();
	}
	return BHNormalizeBackendBaseUrl(Url);
}

void UBHFeedbackSubsystem::GatherContext(const TSharedRef<FJsonObject>& Out) const
{
	FString AppVersion;
	if (!GConfig || !GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), AppVersion, GGameIni) || AppVersion.IsEmpty())
	{
		AppVersion = FApp::GetBuildVersion();
	}

	Out->SetStringField(TEXT("app_version"), AppVersion);
	Out->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
	Out->SetStringField(TEXT("platform"), UGameplayStatics::GetPlatformName());
	Out->SetStringField(TEXT("session_id"), SessionId);
	Out->SetNumberField(TEXT("play_seconds"), FMath::FloorToInt(FPlatformTime::Seconds() - SessionStartSeconds));

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UBHAccountSubsystem* Account = GI->GetSubsystem<UBHAccountSubsystem>())
		{
			const FBHAccountProfile& Profile = Account->GetProfile();
			const UBHGameSettings* GameSettings = GetDefault<UBHGameSettings>();
			const bool bClassroom = GameSettings && GameSettings->bClassroomMode;
			if (bClassroom)
			{
				// Classroom builds host minors: never attach the real lobby display name to a payload that
				// can leave the machine (the manual feedback/survey form ships this context off-machine).
				// Use the anonymous, session-scoped tag the auto-telemetry path already uses so the host can
				// still distinguish reporters without exposing a student's name. This upholds the privacy
				// promise shown in GetPrivacySummary ("Your name is NOT attached").
				FString AnonymousPlayerId = SessionId;
				if (UBHGameInstance* BHGI = Cast<UBHGameInstance>(GetGameInstance()))
				{
					const APlayerController* PC = GI->GetFirstLocalPlayerController(GI->GetWorld());
					const APlayerState* PlayerState = PC ? PC->PlayerState : nullptr;
					const FString Tag = BHGI->GetAnonymousTelemetryPlayerTag(PlayerState);
					if (!Tag.IsEmpty())
					{
						AnonymousPlayerId = Tag;
					}
				}
				Out->SetStringField(TEXT("player_name"), TEXT(""));
				Out->SetStringField(TEXT("player_id"), AnonymousPlayerId);
			}
			else
			{
				Out->SetStringField(TEXT("player_name"), Profile.DisplayName);
				Out->SetStringField(TEXT("player_id"), Profile.PlayerId);
			}
		}

		if (const UWorld* World = GI->GetWorld())
		{
			FString LevelName;
			if (const ABHGameState* GameState = World->GetGameState<ABHGameState>())
			{
				LevelName = GameState->ActiveLevelName;
				Out->SetStringField(TEXT("round_phase"), BHPhaseToString(GameState->RoundPhase));
			}
			if (LevelName.IsEmpty())
			{
				LevelName = World->GetMapName();
				LevelName.RemoveFromStart(World->StreamingLevelsPrefix);
			}
			Out->SetStringField(TEXT("level"), LevelName);

			if (const APlayerController* PC = GI->GetFirstLocalPlayerController(World))
			{
				if (const ABHPlayerState* PlayerState = PC->GetPlayerState<ABHPlayerState>())
				{
					Out->SetStringField(TEXT("role"), BHRoleToString(PlayerState->PlayerRole));
				}
			}
		}
	}
}

TSharedRef<FJsonObject> UBHFeedbackSubsystem::BuildDeviceJson() const
{
	TSharedRef<FJsonObject> Device = MakeShared<FJsonObject>();
	Device->SetStringField(TEXT("cpu"), FPlatformMisc::GetCPUBrand());
	Device->SetStringField(TEXT("gpu"), FPlatformMisc::GetPrimaryGPUBrand());
	Device->SetStringField(TEXT("os"), FPlatformMisc::GetOSVersion());
	Device->SetNumberField(TEXT("ram_gb"), static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB));
	return Device;
}

TSharedRef<FJsonObject> UBHFeedbackSubsystem::BuildPerfJson() const
{
	TSharedRef<FJsonObject> Perf = MakeShared<FJsonObject>();
	const int32 AvgFps = (PerfFrameTimeSum > 0.0) ? FMath::RoundToInt(static_cast<double>(PerfFrameCount) / PerfFrameTimeSum) : 0;

	int32 P1Low = 0;
	if (PerfFrameCount > 0)
	{
		const int64 Target = FMath::Max<int64>(1, PerfFrameCount / 100);
		int64 Accum = 0;
		for (int32 Bin = 0; Bin < PerfHistogramBins; ++Bin)
		{
			Accum += PerfFpsHistogram[Bin];
			if (Accum >= Target)
			{
				P1Low = Bin;
				break;
			}
		}
	}

	Perf->SetNumberField(TEXT("samples"), static_cast<double>(PerfFrameCount));
	Perf->SetNumberField(TEXT("session_seconds"), FMath::FloorToInt(FPlatformTime::Seconds() - SessionStartSeconds));
	Perf->SetNumberField(TEXT("avg_fps"), AvgFps);
	Perf->SetNumberField(TEXT("min_fps"), (PerfFrameCount > 0) ? FMath::RoundToInt(PerfMinFps) : 0);
	Perf->SetNumberField(TEXT("max_fps"), FMath::RoundToInt(PerfMaxFps));
	Perf->SetNumberField(TEXT("p1_low_fps"), P1Low);
	Perf->SetNumberField(TEXT("hitches"), PerfHitchCount);
	return Perf;
}

TSharedRef<FJsonObject> UBHFeedbackSubsystem::BuildGraphicsJson() const
{
	TSharedRef<FJsonObject> Gr = MakeShared<FJsonObject>();

	if (GEngine)
	{
		if (const UGameUserSettings* GUS = GEngine->GetGameUserSettings())
		{
			const FIntPoint Res = GUS->GetScreenResolution();
			Gr->SetStringField(TEXT("resolution"), FString::Printf(TEXT("%dx%d"), Res.X, Res.Y));

			switch (GUS->GetFullscreenMode())
			{
			case EWindowMode::Fullscreen:        Gr->SetStringField(TEXT("window_mode"), TEXT("Fullscreen")); break;
			case EWindowMode::WindowedFullscreen: Gr->SetStringField(TEXT("window_mode"), TEXT("Borderless")); break;
			default:                             Gr->SetStringField(TEXT("window_mode"), TEXT("Windowed")); break;
			}

			Gr->SetBoolField(TEXT("vsync"), GUS->IsVSyncEnabled());
			Gr->SetNumberField(TEXT("fps_limit"), GUS->GetFrameRateLimit());
			// -1 = custom (individual levels don't match any preset), 0=Low, 1=Med, 2=High, 3=Epic
			Gr->SetNumberField(TEXT("quality_preset"), GUS->GetOverallScalabilityLevel());
			Gr->SetNumberField(TEXT("q_texture"),    GUS->GetTextureQuality());
			Gr->SetNumberField(TEXT("q_shadow"),     GUS->GetShadowQuality());
			Gr->SetNumberField(TEXT("q_effects"),    GUS->GetVisualEffectQuality());
			Gr->SetNumberField(TEXT("q_view_dist"),  GUS->GetViewDistanceQuality());
			Gr->SetNumberField(TEXT("q_foliage"),    GUS->GetFoliageQuality());
			Gr->SetNumberField(TEXT("q_postprocess"),GUS->GetPostProcessingQuality());
			Gr->SetNumberField(TEXT("q_shading"),    GUS->GetShadingQuality());
		}
	}

	// AA method from the CVar — more precise than the scalability-level version in GameUserSettings.
	const IConsoleVariable* AAVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
	if (AAVar)
	{
		FString AAStr;
		switch (AAVar->GetInt())
		{
		case 0: AAStr = TEXT("None"); break;
		case 1: AAStr = TEXT("FXAA"); break;
		case 2: AAStr = TEXT("TAA");  break;
		case 3: AAStr = TEXT("MSAA"); break;
		case 4: AAStr = TEXT("TSR");  break;
		default: AAStr = FString::FromInt(AAVar->GetInt()); break;
		}
		Gr->SetStringField(TEXT("anti_aliasing"), AAStr);
	}

	// Actual render resolution percentage (may differ from GameUserSettings when TSR/DLSS adjusts it).
	const IConsoleVariable* ScrPctVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	if (ScrPctVar)
	{
		Gr->SetNumberField(TEXT("screen_pct"), FMath::RoundToInt(ScrPctVar->GetFloat()));
	}

	return Gr;
}

TSharedRef<FJsonObject> UBHFeedbackSubsystem::BuildDiagnosticsJson(bool bIncludeLogs) const
{
	TSharedRef<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	Diagnostics->SetObjectField(TEXT("device"), BuildDeviceJson());
	Diagnostics->SetObjectField(TEXT("perf"), BuildPerfJson());
	Diagnostics->SetObjectField(TEXT("graphics"), BuildGraphicsJson());

	if (bIncludeLogs && LogSink.IsValid())
	{
		const TArray<FString> Logs = LogSink->Snapshot();
		TArray<TSharedPtr<FJsonValue>> LogValues;
		LogValues.Reserve(Logs.Num());
		for (const FString& Line : Logs)
		{
			LogValues.Add(MakeShared<FJsonValueString>(Line));
		}
		Diagnostics->SetArrayField(TEXT("recent_logs"), LogValues);
	}

	return Diagnostics;
}

bool UBHFeedbackSubsystem::SubmitFeedback(EBHFeedbackKind Kind, const FString& Message, int32 Rating, const FString& Contact, bool bIncludeDiagnostics, FString& OutMessage)
{
	if (!IsFeedbackEnabled())
	{
		OutMessage = TEXT("Feedback is turned off in settings.");
		return false;
	}

	FString CleanMessage = Message.TrimStartAndEnd();
	if (CleanMessage.Len() > BHMaxFeedbackChars)
	{
		CleanMessage.LeftInline(BHMaxFeedbackChars);
	}

	if (Kind != EBHFeedbackKind::Survey && CleanMessage.IsEmpty())
	{
		OutMessage = TEXT("Type a short message before sending.");
		SetStatus(OutMessage);
		return false;
	}

	FString CleanContact = Contact.TrimStartAndEnd();
	if (CleanContact.Len() > BHMaxContactChars)
	{
		CleanContact.LeftInline(BHMaxContactChars);
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("kind"), BHFeedbackKindToString(Kind));
	Root->SetStringField(TEXT("message"), CleanMessage);
	Root->SetNumberField(TEXT("rating"), FMath::Clamp(Rating, 0, 5));
	Root->SetStringField(TEXT("contact"), CleanContact);

	TSharedRef<FJsonObject> Context = MakeShared<FJsonObject>();
	GatherContext(Context);
	Root->SetObjectField(TEXT("context"), Context);

	if (bIncludeDiagnostics)
	{
		Root->SetObjectField(TEXT("diagnostics"), BuildDiagnosticsJson(true));
	}

	PostJson(TEXT("/feedback"), Root, TEXT("Sending your feedback..."), TEXT("Thanks! Your feedback was sent."), TEXT("feedback"));
	OutMessage = LastStatusMessage;
	return true;
}

bool UBHFeedbackSubsystem::SubmitRoundSurvey(int32 OverallRating, const FString& Difficulty, bool bWouldRecommend, const FString& Comment, FString& OutMessage)
{
	if (!IsFeedbackEnabled())
	{
		OutMessage = TEXT("Feedback is turned off in settings.");
		return false;
	}

	FString CleanComment = Comment.TrimStartAndEnd();
	if (CleanComment.Len() > BHMaxFeedbackChars)
	{
		CleanComment.LeftInline(BHMaxFeedbackChars);
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("kind"), TEXT("survey"));
	Root->SetStringField(TEXT("message"), CleanComment);
	Root->SetNumberField(TEXT("rating"), FMath::Clamp(OverallRating, 0, 5));

	TSharedRef<FJsonObject> Survey = MakeShared<FJsonObject>();
	Survey->SetNumberField(TEXT("overall"), FMath::Clamp(OverallRating, 0, 5));
	Survey->SetStringField(TEXT("difficulty"), Difficulty);
	Survey->SetBoolField(TEXT("would_recommend"), bWouldRecommend);
	if (RoundResults.Num() > 0)
	{
		const FBHFeedbackRoundResult& Last = RoundResults.Last();
		Survey->SetStringField(TEXT("round_role"), Last.Role);
		Survey->SetStringField(TEXT("round_result"), Last.Result);
		Survey->SetBoolField(TEXT("survived"), Last.bSurvived);
	}
	Root->SetObjectField(TEXT("survey"), Survey);

	TSharedRef<FJsonObject> Context = MakeShared<FJsonObject>();
	GatherContext(Context);
	Root->SetObjectField(TEXT("context"), Context);

	if (IsPerformanceTelemetryEnabled())
	{
		Root->SetObjectField(TEXT("diagnostics"), BuildDiagnosticsJson(false));
	}

	bEndOfRoundSurveyPending = false;
	PostJson(TEXT("/feedback"), Root, TEXT("Sending your survey..."), TEXT("Thanks for the feedback!"), TEXT("survey"));
	OutMessage = LastStatusMessage;
	return true;
}

void UBHFeedbackSubsystem::RecordRoundResult(EBHPlayerRole Role, EBHPlayerLifeState LifeState, EBHRoundPhase ResultPhase)
{
	FBHFeedbackRoundResult Result;
	Result.Role = BHRoleToString(Role);
	Result.Result = BHPhaseToString(ResultPhase);
	Result.bSurvived = (LifeState == EBHPlayerLifeState::Escaped) || (LifeState == EBHPlayerLifeState::Alive);
	Result.AtUtc = FDateTime::UtcNow().ToIso8601();

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWorld* World = GI->GetWorld())
		{
			if (const ABHGameState* GameState = World->GetGameState<ABHGameState>())
			{
				Result.Level = GameState->ActiveLevelName;
			}
		}
	}

	RoundResults.Add(Result);
	// Bound the history so a long-lived host session of many back-to-back rounds cannot grow this
	// unbounded (mirrors the caps on QuestionAttemptHistory / PlaytestTelemetryEvents in BHGameInstance).
	constexpr int32 MaxRetainedRoundResults = 64;
	if (RoundResults.Num() > MaxRetainedRoundResults)
	{
		RoundResults.RemoveAt(0, RoundResults.Num() - MaxRetainedRoundResults);
	}
	bEndOfRoundSurveyPending = true;

	// Deliver the session diagnostics now, while the game is still running, so they are not lost
	// on exit. The server stores each post; the latest one supersedes earlier partial summaries.
	if (IsPerformanceTelemetryEnabled())
	{
		FString IgnoredMessage;
		SubmitSessionTelemetry(TEXT("round_end"), IgnoredMessage);
	}
}

bool UBHFeedbackSubsystem::SubmitSessionTelemetry(const FString& Reason, FString& OutMessage)
{
	if (!IsPerformanceTelemetryEnabled())
	{
		OutMessage = TEXT("Performance telemetry is turned off in settings.");
		return false;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("reason"), Reason);

	TSharedRef<FJsonObject> Context = MakeShared<FJsonObject>();
	GatherContext(Context);
	// The telemetry route reads these identity fields at the top level rather than from a
	// context object, so promote them from the gathered context (only those that are present).
	Root->SetStringField(TEXT("session_id"), SessionId);
	// Privacy: this is the AUTO round-end telemetry path, which has no per-submission consent gate
	// (unlike the manual feedback form). Never promote the real "player_name" here, and replace the
	// real "player_id" with an anonymous, session-scoped tag so we don't auto-broadcast student
	// identities. The manual feedback/survey paths still send the full GatherContext identity.
	for (const TCHAR* Field : { TEXT("app_version"), TEXT("engine_version"), TEXT("platform") })
	{
		FString Value;
		if (Context->TryGetStringField(Field, Value))
		{
			Root->SetStringField(Field, Value);
		}
	}

	// Anonymous player id: prefer the GameInstance's anonymous per-session player tag (e.g. "P001"),
	// falling back to the anonymous session id when no local player state is available.
	FString AnonymousPlayerId = SessionId;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UBHGameInstance* BHGI = Cast<UBHGameInstance>(GI))
		{
			const APlayerController* PC = GI->GetFirstLocalPlayerController(GI->GetWorld());
			const APlayerState* PlayerState = PC ? PC->PlayerState : nullptr;
			const FString Tag = BHGI->GetAnonymousTelemetryPlayerTag(PlayerState);
			if (!Tag.IsEmpty())
			{
				AnonymousPlayerId = Tag;
			}
		}
	}
	Root->SetStringField(TEXT("player_id"), AnonymousPlayerId);

	// The telemetry route reads device/perf/recent_logs at the top level rather than nested in a
	// "diagnostics" object, so build them directly here.
	Root->SetObjectField(TEXT("device"), BuildDeviceJson());
	Root->SetObjectField(TEXT("perf"), BuildPerfJson());
	if (LogSink.IsValid())
	{
		const TArray<FString> Logs = LogSink->Snapshot();
		TArray<TSharedPtr<FJsonValue>> LogValues;
		LogValues.Reserve(Logs.Num());
		for (const FString& Line : Logs)
		{
			LogValues.Add(MakeShared<FJsonValueString>(Line));
		}
		Root->SetArrayField(TEXT("recent_logs"), LogValues);
	}

	TArray<TSharedPtr<FJsonValue>> RoundValues;
	for (const FBHFeedbackRoundResult& Round : RoundResults)
	{
		TSharedRef<FJsonObject> RoundJson = MakeShared<FJsonObject>();
		RoundJson->SetStringField(TEXT("role"), Round.Role);
		RoundJson->SetStringField(TEXT("result"), Round.Result);
		RoundJson->SetStringField(TEXT("level"), Round.Level);
		RoundJson->SetBoolField(TEXT("survived"), Round.bSurvived);
		RoundJson->SetStringField(TEXT("at"), Round.AtUtc);
		RoundValues.Add(MakeShared<FJsonValueObject>(RoundJson));
	}
	Root->SetArrayField(TEXT("round_results"), RoundValues);

	PostJson(TEXT("/telemetry/session"), Root, TEXT("Sending session diagnostics..."), TEXT("Diagnostics sent."), TEXT("telemetry"));
	OutMessage = LastStatusMessage;
	return true;
}

void UBHFeedbackSubsystem::PostJson(const FString& PathSuffix, const TSharedRef<FJsonObject>& Body, const FString& SubmittingMessage, const FString& SuccessMessage, const FString& FallbackTag)
{
	const FString JsonString = BHJsonToString(Body);
	const FString BaseUrl = ResolveBackendBaseUrl();

	if (BaseUrl.IsEmpty())
	{
		// No server configured: keep the submission locally so it is not lost.
		FString SavedPath;
		if (SaveLocalFallback(FallbackTag, JsonString, SavedPath))
		{
			SetStatus(FString::Printf(TEXT("No feedback server configured. Saved locally to %s"), *SavedPath));
		}
		else
		{
			SetStatus(TEXT("No feedback server configured and the local copy could not be saved."));
		}
		return;
	}

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl + PathSuffix);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetContentAsString(JsonString);
	Request->OnProcessRequestComplete().BindUObject(this, &UBHFeedbackSubsystem::HandlePostComplete, JsonString, FallbackTag, SuccessMessage);
	Request->SetTimeout(10.0f); // Bound flaky-Wi-Fi hangs; engine default is ~300s.
	Request->ProcessRequest();

	SetStatus(SubmittingMessage);
}

void UBHFeedbackSubsystem::HandlePostComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString FallbackJson, FString FallbackTag, FString SuccessMessage)
{
	const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
	if (bWasSuccessful && Response.IsValid() && Code >= 200 && Code < 300)
	{
		SetStatus(SuccessMessage);
		return;
	}

	// Delivery failed: keep a local copy so nothing is lost, and report it.
	FString SavedPath;
	if (SaveLocalFallback(FallbackTag, FallbackJson, SavedPath))
	{
		SetStatus(FString::Printf(TEXT("Could not reach the feedback server (code %d). Saved locally to %s"), Code, *SavedPath));
	}
	else
	{
		SetStatus(FString::Printf(TEXT("Could not reach the feedback server (code %d)."), Code));
	}
}

bool UBHFeedbackSubsystem::SaveLocalFallback(const FString& Tag, const FString& JsonString, FString& OutPath) const
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Feedback"));
	IFileManager::Get().MakeDirectory(*Directory, /*Tree=*/true);
	const FString FileName = FString::Printf(TEXT("%s-%s-%s.json"),
		*Tag,
		*FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	OutPath = FPaths::Combine(Directory, FileName);
	return FFileHelper::SaveStringToFile(JsonString, *OutPath);
}

void UBHFeedbackSubsystem::SetStatus(const FString& Message)
{
	LastStatusMessage = Message;
}

void UBHFeedbackSubsystem::FeedbackSend(const FString& Message)
{
	FString OutMessage;
	SubmitFeedback(EBHFeedbackKind::Other, Message, 0, FString(), ShouldIncludeDiagnosticsByDefault(), OutMessage);
	UE_LOG(LogTemp, Display, TEXT("[Feedback] %s"), *OutMessage);
}

void UBHFeedbackSubsystem::FeedbackSendDiagnostics()
{
	FString OutMessage;
	SubmitSessionTelemetry(TEXT("manual"), OutMessage);
	UE_LOG(LogTemp, Display, TEXT("[Feedback] %s"), *OutMessage);
}
