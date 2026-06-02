// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHEscapeStationManager.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHTrainDisplayActor.h"
#include "BHTrainDoor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

ABHEscapeStationManager::ABHEscapeStationManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	EscapeSeconds = 120.0f;
	CutsceneSeconds = 6.5f;
	HunterReleaseDelaySeconds = 3.5f;
	AntiCampRadius = 520.0f;
	AntiCampGraceSeconds = 4.0f;
	AntiCampPenaltySeconds = 2.5f;
	bTriggered = false;

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings)
	{
		EscapeSeconds = FMath::Max(20, Settings->FinalEscapeSeconds);
		CutsceneSeconds = FMath::Max(1.0f, Settings->FinalEscapeCutsceneSeconds);
		HunterReleaseDelaySeconds = FMath::Max(0.0f, Settings->HunterReleaseDelaySeconds);
		AntiCampRadius = FMath::Max(150.0f, Settings->HunterAntiCampRadius);
		AntiCampGraceSeconds = FMath::Max(0.5f, Settings->HunterAntiCampGraceSeconds);
		AntiCampPenaltySeconds = FMath::Max(0.5f, Settings->HunterAntiCampPenaltySeconds);
	}
}

void ABHEscapeStationManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		TickAntiCamp(DeltaSeconds);
		UpdateStationDisplays();
	}
}

void ABHEscapeStationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(CutsceneTimerHandle);
		GetWorldTimerManager().ClearTimer(EscapeTimerHandle);
		GetWorldTimerManager().ClearAllTimersForObject(this);
	}

	HunterDoorProximitySeconds.Reset();
	HunterPenaltyEndTimes.Reset();
	Super::EndPlay(EndPlayReason);
}

void ABHEscapeStationManager::RegisterEscapeDoor(ABHTrainDoor* Door)
{
	if (Door)
	{
		EscapeDoors.AddUnique(Door);
		Door->ConfigureDoor(true, TEXT("Evacuation Door"));
		Door->SetDoorOpen(false);
	}
}

void ABHEscapeStationManager::RegisterDisplay(ABHTrainDisplayActor* Display)
{
	if (Display)
	{
		Displays.AddUnique(Display);
	}
}

void ABHEscapeStationManager::TriggerFinalEscape()
{
	if (!HasAuthority() || bTriggered || !GetWorld())
	{
		return;
	}

	bTriggered = true;
	const AGameStateBase* BaseGameState = GetWorld()->GetGameState();
	const float Now = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	if (ABHGameState* BHGS = GetWorld()->GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(EBHRoundPhase::FinalEscape);
		BHGS->SetExitUnlocked(true);
		BHGS->SetRemainingTime(FMath::RoundToInt(EscapeSeconds));
		BHGS->SetFinalEscapeState(EBHFinalEscapeState::Cutscene, Now + CutsceneSeconds, Now + CutsceneSeconds + EscapeSeconds, Now + CutsceneSeconds + HunterReleaseDelaySeconds);
		BHGS->SetIntermissionLocks(true, true, true);
		BHGS->SetPresenceState(100.0f, TEXT("Evacuation train authorized."), BHGS->PresencePulse + 1);
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientSetJumpscareInputLocked(true);
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Evacuation train arriving. Control returns in %.0fs; Teacher release is delayed and door camping is disrupted."), CutsceneSeconds), CutsceneSeconds);
		}
	}

	UpdateStationDisplays();
	GetWorldTimerManager().SetTimer(CutsceneTimerHandle, this, &ABHEscapeStationManager::FinishCutscene, CutsceneSeconds, false);
}

bool ABHEscapeStationManager::IsFinalEscapeActive() const
{
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive;
}

bool ABHEscapeStationManager::IsHunterPenaltyActive(const ABHCharacter* Hunter) const
{
	if (!Hunter || !GetWorld())
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (const float* PenaltyEnd = HunterPenaltyEndTimes.Find(TWeakObjectPtr<ABHCharacter>(const_cast<ABHCharacter*>(Hunter))))
	{
		return *PenaltyEnd > Now;
	}
	return false;
}

void ABHEscapeStationManager::OpenEscapeDoors()
{
	for (ABHTrainDoor* Door : EscapeDoors)
	{
		if (Door)
		{
			Door->SetDoorOpen(true);
		}
	}
}

void ABHEscapeStationManager::FinishCutscene()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	OpenEscapeDoors();
	const AGameStateBase* BaseGameState = GetWorld()->GetGameState();
	const float Now = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	if (ABHGameState* BHGS = GetWorld()->GetGameState<ABHGameState>())
	{
		BHGS->SetFinalEscapeState(EBHFinalEscapeState::EscapeActive, Now, Now + EscapeSeconds, Now + HunterReleaseDelaySeconds);
		BHGS->SetIntermissionLocks(false, false, true);
		BHGS->SetRemainingTime(FMath::RoundToInt(EscapeSeconds));
		BHGS->SetDirectorState(BHGS->RoundSeed, TEXT("Reach the evacuation train. Multiple subway doors are open."), BHGS->NextLevelName);
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientSetJumpscareInputLocked(false);
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Reach any green train door. Teacher held %.0fs after unlock; camping a door disrupts capture pressure."), HunterReleaseDelaySeconds), 4.5f);
		}
	}

	GetWorldTimerManager().SetTimer(EscapeTimerHandle, this, &ABHEscapeStationManager::ExpireFinalEscape, EscapeSeconds, false);
}

void ABHEscapeStationManager::ExpireFinalEscape()
{
	if (!HasAuthority())
	{
		return;
	}

	if (ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr)
	{
		if (BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive)
		{
			BHGS->SetFinalEscapeState(EBHFinalEscapeState::Failed, 0.0f, 0.0f, 0.0f);
		}
	}

	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->NotifyFinalEscapeExpired();
	}
}

void ABHEscapeStationManager::UpdateStationDisplays()
{
	ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS)
	{
		return;
	}

	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	const int32 Countdown = FMath::Max(0, FMath::CeilToInt(BHGS->FinalEscapeEndServerTime - Now));
	FString Header = TEXT("FINAL TERMINAL LOCKED");
	FString Body = TEXT("Answer integrity threshold required.");
	FLinearColor Accent(1.0f, 0.16f, 0.10f, 1.0f);
	if (BHGS->FinalEscapeState == EBHFinalEscapeState::Cutscene)
	{
		Header = TEXT("EVACUATION TRAIN ARRIVING");
		const int32 ControlCountdown = FMath::Max(0, FMath::CeilToInt(BHGS->FinalEscapeCutsceneEndServerTime - Now));
		const int32 HunterReleaseCountdown = FMath::Max(0, FMath::CeilToInt(BHGS->HunterReleaseServerTime - Now));
		Body = FString::Printf(TEXT("Doors unlocking in %02d:%02d.\nTeacher release in %02d:%02d.\nGet ready to sprint to a green doorway."),
			ControlCountdown / 60,
			ControlCountdown % 60,
			HunterReleaseCountdown / 60,
			HunterReleaseCountdown % 60);
		Accent = FLinearColor(1.0f, 0.74f, 0.22f, 1.0f);
	}
	else if (BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive)
	{
		Header = TEXT("TRAIN DEPARTS");
		const int32 HunterReleaseCountdown = FMath::Max(0, FMath::CeilToInt(BHGS->HunterReleaseServerTime - Now));
		const FString HunterLine = BHGS->bHunterInputFrozen
			? FString::Printf(TEXT("Teacher held %02d:%02d."), HunterReleaseCountdown / 60, HunterReleaseCountdown % 60)
			: FString(TEXT("Teacher active. Door camping disrupts capture."));
		Body = FString::Printf(TEXT("%02d:%02d\nUse any green subway door.\n%s\nSurvivors board; others pressure routes."), Countdown / 60, Countdown % 60, *HunterLine);
		Accent = FLinearColor(0.20f, 1.0f, 0.48f, 1.0f);
	}
	else if (BHGS->FinalEscapeState == EBHFinalEscapeState::Departed)
	{
		Header = TEXT("TRAIN DEPARTED");
		Body = TEXT("Survivors boarded the evacuation train.");
		Accent = FLinearColor(0.36f, 0.86f, 1.0f, 1.0f);
	}

	if (Header == LastDisplayHeader && Body == LastDisplayBody && Accent.Equals(LastDisplayAccent))
	{
		return;
	}
	LastDisplayHeader = Header;
	LastDisplayBody = Body;
	LastDisplayAccent = Accent;

	for (ABHTrainDisplayActor* Display : Displays)
	{
		if (Display)
		{
			Display->ConfigureDisplay(Header, Body, Accent);
		}
	}
}

void ABHEscapeStationManager::TickAntiCamp(float DeltaSeconds)
{
	ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || BHGS->FinalEscapeState != EBHFinalEscapeState::EscapeActive)
	{
		HunterDoorProximitySeconds.Reset();
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Drop entries whose hunter has gone away or whose penalty has already elapsed so the
	// map does not accumulate stale keys for the duration of an active escape.
	for (auto It = HunterPenaltyEndTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value() <= Now)
		{
			It.RemoveCurrent();
		}
	}

	if (BHGS->bHunterInputFrozen)
	{
		const AGameStateBase* BaseGameState = GetWorld()->GetGameState();
		const float ServerNow = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : Now;
		if (ServerNow >= BHGS->HunterReleaseServerTime)
		{
			BHGS->SetIntermissionLocks(false, false, false);
		}
	}

	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Hunter = *It;
		const ABHPlayerState* HunterPS = Hunter ? Hunter->GetBHPlayerState() : nullptr;
		if (!Hunter || !HunterPS || !HunterPS->IsAliveHunter())
		{
			continue;
		}

		float NearestDoorDistSq = TNumericLimits<float>::Max();
		for (ABHTrainDoor* Door : EscapeDoors)
		{
			if (Door && Door->IsDoorOpen())
			{
				NearestDoorDistSq = FMath::Min(NearestDoorDistSq, FVector::DistSquared2D(Hunter->GetActorLocation(), Door->GetActorLocation()));
			}
		}

		const bool bNearDoor = NearestDoorDistSq <= FMath::Square(AntiCampRadius);
		float& ProximitySeconds = HunterDoorProximitySeconds.FindOrAdd(Hunter);
		ProximitySeconds = bNearDoor ? ProximitySeconds + DeltaSeconds : FMath::Max(0.0f, ProximitySeconds - DeltaSeconds * 1.8f);
		if (ProximitySeconds >= AntiCampGraceSeconds)
		{
			HunterPenaltyEndTimes.FindOrAdd(Hunter) = Now + AntiCampPenaltySeconds;
			ProximitySeconds = AntiCampGraceSeconds * 0.45f;
			Hunter->RecoverStamina(-22.0f);
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(Hunter->GetController()))
			{
				PC->ClientShowStatusMessage(TEXT("Move off the evacuation door. Capture pressure disrupted."), 2.5f);
			}
		}
	}
}
