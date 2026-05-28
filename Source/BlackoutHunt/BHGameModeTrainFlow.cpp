#include "BHGameMode.h"

#include "BHEscapeStationManager.h"
#include "BHGameInstance.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"

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

void ABHGameMode::CompleteTrainIntermission(const FString& NextMapName, bool bFinalRecap)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	ConvertMonitorsBackToSurvivors(TEXT("train departure"));
	PersistPlayersForTravel();

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (bFinalRecap)
	{
		if (BHGI)
		{
			BHGI->ClearQuestionAttemptHistory();
			BHGI->ResetPersistentTrainRunProgress();
			BHGI->SetPersistentStageIndex(0);
		}
		const FString TravelURL = BuildTravelOptionsForLevel(TEXT("Facility"), false, 0, EBHRoundPhase::Lobby);
		GetWorld()->ServerTravel(TravelURL, true);
		return;
	}

	const int32 NextStageIndex = FMath::Clamp(RuntimeStageIndex + 1, 0, 2);
	if (BHGI)
	{
		BHGI->ClearQuestionAttemptHistory();
		BHGI->SetPersistentStageIndex(NextStageIndex);
	}

	const FString Destination = NextMapName.IsEmpty() ? GetDefaultMapForStage(NextStageIndex) : NextMapName;
	const FString TravelURL = BuildTravelOptionsForLevel(Destination, false, NextStageIndex, EBHRoundPhase::Lobby);
	GetWorld()->ServerTravel(TravelURL, true);
}

void ABHGameMode::NotifyFinalEscapeExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	BroadcastStatus(TEXT("The evacuation train departed without the class."), 4.5f);
	EndRound(EBHRoundPhase::HunterWin);
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

	bool bRestored = false;
	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		bRestored = BHGI->RestoreTravelPlayerState(BHPS);
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
	if (!HasAuthority() || !IsFinalStage())
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
}

bool ABHGameMode::IsFinalStage() const
{
	return RuntimeStageIndex >= 2 || RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
}
