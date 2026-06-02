// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHGameMode.h"

#include "BHEscapeStationManager.h"
#include "BHGameInstance.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// Strip a player name down to a filesystem-safe token for per-student status filenames.
	FString BHFinalSanitizeFileToken(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len());
		for (const TCHAR Ch : In)
		{
			Out.AppendChar((FChar::IsAlnum(Ch) || Ch == TEXT('-') || Ch == TEXT('_')) ? Ch : TEXT('_'));
		}
		Out = Out.Left(40);
		return Out.IsEmpty() ? FString(TEXT("student")) : Out;
	}
}

void ABHGameMode::TravelToTrainIntermission(EBHRoundPhase ResultPhase)
{
	if (!GetWorld())
	{
		return;
	}

	ConvertMonitorsBackToSurvivors(TEXT("subway platform reached"));
	PersistPlayersForTravel();
	const FString TravelURL = BuildTravelOptionsForLevel(TEXT("TrainIntermission"), true, RuntimeStageIndex, ResultPhase);
	GetWorld()->ServerTravel(TravelURL, true);
}

bool ABHGameMode::CompleteTrainIntermission(const FString& NextMapName, bool bFinalRecap)
{
	if (!HasAuthority() || !GetWorld())
	{
		return false;
	}

	// A travel is already committed (e.g. the Departing timer fired moments before a tester shortcut):
	// treat the transition as handled so the caller does not retry, and do not stack a second travel.
	if (bServerTravelInProgress)
	{
		return true;
	}

	ConvertMonitorsBackToSurvivors(TEXT("train departure"));
	PersistPlayersForTravel();

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (bFinalRecap)
	{
		// Final stage no longer routes onward to a new level. The game ends in place: post results, export the
		// class data, and hold in the train (see EnterFinalResultsHold). Persistent progress is intentionally
		// NOT reset here -- the export needs it intact; the eventual host return-to-lobby travel resets it.
		EnterFinalResultsHold();
		return true;
	}

	const int32 NextStageIndex = FMath::Clamp(RuntimeStageIndex + 1, 0, 2);
	if (BHGI)
	{
		BHGI->ClearQuestionAttemptHistory();
		BHGI->SetPersistentStageIndex(NextStageIndex);
	}

	const FString Destination = NextMapName.IsEmpty() ? GetDefaultMapForStage(NextStageIndex) : NextMapName;
	const FString TravelURL = BuildTravelOptionsForLevel(Destination, false, NextStageIndex, EBHRoundPhase::Lobby);
	return RequestServerTravel(TravelURL, true);
}

void ABHGameMode::NotifyFinalEscapeExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	// Credit a partial evacuation: if any survivor already boarded before the window closed, that is a
	// survivor win (matching the standard Hunt resolution), not a clean Teacher win for the stragglers.
	const bool bAnyEscaped = CountEscapedSurvivors() > 0;
	BroadcastStatus(bAnyEscaped
		? TEXT("The evacuation train departed. The students who boarded escaped.")
		: TEXT("The evacuation train departed without the class."), 4.5f);
	EndRound(bAnyEscaped ? EBHRoundPhase::SurvivorsWin : EBHRoundPhase::HunterWin);
}

void ABHGameMode::PersistPlayersForTravel()
{
	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI || !GameState)
	{
		return;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		if (ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS))
		{
			BHGI->PersistTravelPlayerState(BHPS);
		}
	}
}

void ABHGameMode::RestorePlayersAfterTravel(AController* Controller)
{
	ABHPlayerState* BHPS = Controller ? Controller->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	// A legitimate cross-travel relogin always lands in a non-active phase: a fresh round map begins
	// in Lobby (BHGameMode::BeginPlay) and the train map begins in Intermission. Any phase where the
	// round is actually being played and captures are live - Prep (the warmup), Hunt, or FinalEscape -
	// is only ever reached in-place on an already-loaded map, never as a travel landing. So for a fresh
	// late join during one of those phases we keep the Spectator/Captured state PostLogin just assigned
	// instead of letting a stale travel entry from an earlier round resurrect them to a live,
	// capturable, win-counted role.
	const ABHGameState* PhaseGameState = GetGameState<ABHGameState>();
	if (!PhaseGameState && GetWorld())
	{
		PhaseGameState = GetWorld()->GetGameState<ABHGameState>();
	}
	const bool bActiveRoundPhase = PhaseGameState
		&& (PhaseGameState->RoundPhase == EBHRoundPhase::Prep
			|| PhaseGameState->RoundPhase == EBHRoundPhase::Hunt
			|| PhaseGameState->RoundPhase == EBHRoundPhase::FinalEscape);

	bool bRestored = false;
	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		bRestored = BHGI->RestoreTravelPlayerState(BHPS, !bActiveRoundPhase);
	}

	if (bTrainIntermissionLevel)
	{
		if (!bRestored
			|| BHPS->PlayerRole == EBHPlayerRole::Unassigned
			|| BHPS->PlayerRole == EBHPlayerRole::Spectator
			|| BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			BHPS->SetRole(EBHPlayerRole::Survivor);
			BHPS->SetDesiredRole(EBHPlayerRole::Survivor);
			BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		}
		BHPS->SetHiddenInLocker(false);
		BHPS->SetFakeHunterEligible(false);
		BHPS->SetReady(true);
		BHPS->ClearSpectatorSupportState(false);
	}
	else
	{
		ABHGameState* BHGS = GetGameState<ABHGameState>();
		if (!BHGS && GetWorld())
		{
			BHGS = GetWorld()->GetGameState<ABHGameState>();
		}

		if (BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby)
		{
			const bool bWasSpectator = BHPS->PlayerRole == EBHPlayerRole::Spectator;
			BHPS->SetRole(EBHPlayerRole::Unassigned);
			if (bWasSpectator)
			{
				BHPS->SetDesiredRole(EBHPlayerRole::Unassigned);
			}
			BHPS->SetLifeState(EBHPlayerLifeState::Alive);
			BHPS->SetHiddenInLocker(false);
			BHPS->SetFakeHunterEligible(false);
			BHPS->SetReady(false);
			BHPS->ClearSpectatorSupportState(false);
		}
	}
}

void ABHGameMode::ConvertMonitorsBackToSurvivors(const FString& Reason)
{
	if (!GameState)
	{
		return;
	}

	int32 ConvertedCount = 0;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS || BHPS->PlayerRole != EBHPlayerRole::FakeHunter)
		{
			continue;
		}

		BHPS->SetRole(EBHPlayerRole::Survivor);
		BHPS->SetDesiredRole(EBHPlayerRole::Survivor);
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
		BHPS->SetFakeHunterEligible(false);
		++ConvertedCount;
	}

	if (ConvertedCount > 0)
	{
		BroadcastStatus(FString::Printf(TEXT("%d hall monitor(s) returned as survivors for %s."), ConvertedCount, Reason.IsEmpty() ? TEXT("the subway transition") : *Reason), 4.0f);
	}
}

void ABHGameMode::TriggerFinalEscapeIfNeeded()
{
	// The final-escape/train sequence must never fire in the guided tutorial. The tutorial URL pins BHStageIndex=0
	// so IsFinalStage() is already false, but guard bTutorialMode here too as an authoritative backstop in case the
	// stage ever leaks in (e.g. a direct map open without the pin). Scoped to bTutorialMode only, so the practice
	// lab and Test Loop can still exercise the finale.
	if (!HasAuthority() || !IsFinalStage() || bTutorialMode)
	{
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->FinalEscapeState == EBHFinalEscapeState::Cutscene || BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive || BHGS->FinalEscapeState == EBHFinalEscapeState::Departed)
	{
		return;
	}

	ConvertMonitorsBackToSurvivors(TEXT("final subway station"));
	if (EscapeStationManagers.Num() > 0 && EscapeStationManagers[0])
	{
		EscapeStationManagers[0]->TriggerFinalEscape();
		return;
	}

	BHGS->SetRoundPhase(EBHRoundPhase::FinalEscape);
	BHGS->SetExitUnlocked(true);
	BHGS->SetFinalEscapeState(EBHFinalEscapeState::EscapeActive, 0.0f, 0.0f, 0.0f);
	BHGS->SetIntermissionLocks(false, false, false);
	BroadcastStatus(TEXT("Evacuation train authorized. Reach the final platform."), 4.5f);

	// No EscapeStationManager owns an expiry timer on this fallback path, and TickRoundTimer does
	// not evaluate FinalEscape. Without a timeout the match hangs forever if any survivor stays
	// alive and never reaches an exit. Arm the same expiry the manager would, mirroring its bound.
	if (UWorld* World = GetWorld())
	{
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		const float EscapeSeconds = static_cast<float>(FMath::Max(20, Settings ? Settings->FinalEscapeSeconds : 60));
		World->GetTimerManager().SetTimer(
			FinalEscapeFallbackTimerHandle,
			this,
			&ABHGameMode::NotifyFinalEscapeExpired,
			EscapeSeconds,
			false);
	}
}

bool ABHGameMode::IsFinalStage() const
{
	return RuntimeStageIndex >= 2 || RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
}

FString ABHGameMode::BuildFinalLeaderboardText() const
{
	const AGameStateBase* GS = GameState ? static_cast<AGameStateBase*>(GameState) : (GetWorld() ? GetWorld()->GetGameState() : nullptr);
	if (!GS)
	{
		return TEXT("Final results unavailable.");
	}

	struct FFinalEntry
	{
		FString Name;
		int32 Score;
	};
	TArray<FFinalEntry> Entries;
	for (APlayerState* RawPS : GS->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Spectator)
		{
			continue;
		}
		// Combined score: question mastery + hunter/capture points (survival is reflected in the points earned
		// while alive in the chase). Highest first.
		const int32 Score = BHPS->QuestionPoints + BHPS->HunterPoints;
		Entries.Add({ BHPS->GetPlayerName(), Score });
	}
	if (Entries.IsEmpty())
	{
		return TEXT("No students recorded this session.");
	}
	Entries.Sort([](const FFinalEntry& A, const FFinalEntry& B) { return A.Score > B.Score; });

	FString Out = TEXT("TOP 5 - COMBINED SCORE\n");
	const int32 Count = FMath::Min(5, Entries.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Out += FString::Printf(TEXT("%d. %s  -  %d pts\n"), Index + 1, *Entries[Index].Name, Entries[Index].Score);
	}
	Out += FString::Printf(TEXT("Class size: %d. Service ends at this station."), Entries.Num());
	return Out;
}

void ABHGameMode::ExportFinalClassResults(FString& OutSummary)
{
	TArray<FString> Notes;

	// 1) Class-wide telemetry CSV (existing exporter; writes to Saved/PlaytestTelemetry and logs internally).
	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		FString TelemetryMsg;
		const bool bOk = BHGI->ExportPlaytestTelemetry(TelemetryMsg, /*bClearAfterExport*/ false);
		Notes.Add(FString::Printf(TEXT("Class CSV: %s"), *TelemetryMsg));
		UE_LOG(LogTemp, Display, TEXT("[BH Final] Class telemetry export %s: %s"), bOk ? TEXT("OK") : TEXT("not written"), *TelemetryMsg);
	}

	// 2) One per-student status file (mastery / score / topics) to Saved/ClassResults.
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ClassResults")));
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));

	const auto RoleName = [](EBHPlayerRole R) -> const TCHAR*
	{
		switch (R)
		{
		case EBHPlayerRole::Hunter:     return TEXT("Teacher");
		case EBHPlayerRole::Survivor:   return TEXT("Survivor");
		case EBHPlayerRole::FakeHunter: return TEXT("Hall Monitor");
		case EBHPlayerRole::Tester:     return TEXT("Tester");
		case EBHPlayerRole::Spectator:  return TEXT("Spectator");
		default:                        return TEXT("Unassigned");
		}
	};

	int32 Written = 0;
	const AGameStateBase* GS = GameState ? static_cast<AGameStateBase*>(GameState) : (GetWorld() ? GetWorld()->GetGameState() : nullptr);
	if (GS)
	{
		for (APlayerState* RawPS : GS->PlayerArray)
		{
			const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
			if (!BHPS)
			{
				continue;
			}
			const FString SafeName = BHFinalSanitizeFileToken(BHPS->GetPlayerName());
			const FString Path = FPaths::Combine(Dir, FString::Printf(TEXT("Student_%s_%s.txt"), *SafeName, *Stamp));
			const FString Body = FString::Printf(
				TEXT("Student: %s\nRole: %s\nFinal map / stage: %s / %d\n\nQuestion points (this run): %d\nLifetime question points: %d\nHunter/capture points (this run): %d\nLifetime hunter points: %d\nCombined score: %d\nMissed-question review queue: %d\n\nExported (UTC): %s\n"),
				*BHPS->GetPlayerName(),
				RoleName(BHPS->PlayerRole),
				*RuntimeLevelName,
				RuntimeStageIndex,
				BHPS->QuestionPoints,
				BHPS->LifetimeQuestionPoints,
				BHPS->HunterPoints,
				BHPS->LifetimeHunterPoints,
				BHPS->QuestionPoints + BHPS->HunterPoints,
				BHPS->RevisionReviewQueue.Num(),
				*FDateTime::UtcNow().ToIso8601());
			if (FFileHelper::SaveStringToFile(Body, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				++Written;
				UE_LOG(LogTemp, Display, TEXT("[BH Final] Wrote student status file: %s"), *Path);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[BH Final] FAILED to write student status file: %s"), *Path);
			}
		}
	}
	Notes.Add(FString::Printf(TEXT("Per-student files: %d written to %s"), Written, *Dir));
	OutSummary = FString::Join(Notes, TEXT(" | "));
}

void ABHGameMode::EnterFinalResultsHold()
{
	if (!HasAuthority() || !GetWorld() || bFinalResultsHold)
	{
		return;
	}
	bFinalResultsHold = true;

	ConvertMonitorsBackToSurvivors(TEXT("final station"));

	const FString Leaderboard = BuildFinalLeaderboardText();
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Intermission);
		BHGS->SetExitUnlocked(false);
		// Captures stay disabled but movement is free, so the class can walk the carriage and read the boards.
		BHGS->SetIntermissionLocks(true, false, false);
		BHGS->SetTrainState(EBHTrainPhase::StationStop, RuntimeStageIndex, 0.0f, TEXT("Final Station"), TEXT("Final station. Service ends here."));
		// The FINAL LEADERBOARD board reads TrainRecapOverview; put the top-5 there with the class summary.
		BHGS->SetTrainRecap(
			Leaderboard,
			TEXT("Class results are final. Teacher: progress (class CSV) and per-student status files have been saved to the Saved folder."),
			BHGS->TrainRecapMissedQuestions,
			TEXT("Host: return to the lobby from the host menu once everyone has reviewed the results."));
	}

	FString ExportSummary;
	ExportFinalClassResults(ExportSummary);

	BroadcastStatus(TEXT("Final station reached. Top 5 posted on the boards; class progress exported. The game has ended."), 6.0f);
	UE_LOG(LogTemp, Display, TEXT("[BH Final] Game ended in the train (no onward travel). %s"), *ExportSummary);
}
