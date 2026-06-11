// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainIntermissionManager.h"
#include "BHBlockActor.h"
#include "BHCharacter.h"
#include "BHGameInstance.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "BHTrainBonusQuestionTerminal.h"
#include "BHTrainDisplayActor.h"
#include "BHTrainDoor.h"
#include "BHTrainTunnelMotionActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

namespace
{
// Deterministic bonus-question seed derived from the completed stage index so each
// stage draws a stable but distinct question. Stride/offset are arbitrary primes that
// spread consecutive stage indices apart.
constexpr int32 BHIntermissionSeedStride = 171;
constexpr int32 BHIntermissionSeedOffset = 31;

// Local default for the arrival phase length (seconds). Unlike the other phase
// durations, this is intentionally not driven by UBHGameSettings; keep it scoped here.
constexpr float BHIntermissionDefaultArrivalSeconds = 5.0f;

FLinearColor BHTrainPhaseAccent(EBHTrainPhase Phase)
{
	switch (Phase)
	{
	case EBHTrainPhase::Arrival:
	case EBHTrainPhase::StationStop:
		return FLinearColor(0.18f, 1.0f, 0.50f, 1.0f);
	case EBHTrainPhase::Recap:
		return FLinearColor(0.46f, 0.90f, 0.62f, 1.0f);
	case EBHTrainPhase::BonusQuestion:
		return FLinearColor(1.0f, 0.72f, 0.20f, 1.0f);
	case EBHTrainPhase::Shop:
		return FLinearColor(0.82f, 0.58f, 1.0f, 1.0f);
	case EBHTrainPhase::Departing:
		return FLinearColor(1.0f, 0.26f, 0.14f, 1.0f);
	case EBHTrainPhase::Inactive:
	default:
		return FLinearColor(0.14f, 0.82f, 0.74f, 1.0f);
	}
}

FString BHTrainNonEmpty(const FString& Value, const TCHAR* Fallback)
{
	return Value.IsEmpty() ? FString(Fallback) : Value;
}

FString BHBuildBonusDisplayBody(const ABHGameState* BHGS)
{
	const FString WeakTopic = BHGS
		? FBHRevisionQuestionBank::TopicToString(BHGS->RevisionWeakTopic)
		: FString(TEXT("weak topic"));
	return FString::Printf(
		TEXT("Bonus terminal targets %s.\nSurvivors answer with 1-4 for shop points.\nCorrect answers recover stamina and lower fear.\nWrong answers add pressure but keep the class moving.\nShop opens after this timer."),
		*WeakTopic);
}

FString BHBuildShopDisplayBody()
{
	return TEXT("Use terminals that match your role.\nSurvivors spend question points on chase tools.\nTeachers spend capture points on passive upgrades.\nEach terminal shows cost, charges, effect, cooldown, and balance.\nStation stop opens after the shop timer.");
}

FString BHBuildActivitiesDisplayBody()
{
	return TEXT("Social car is optional.\nReflex arcade: time E on GREEN for small role points.\nMemory cards: press E when the pair matches.\nSnack cart restores stamina and lowers fear.\nDrink cooler tops flashlight, stamina, and nerves.");
}

FString BHBuildBoardingDisplayBody(const FString& DestinationText)
{
	return FString::Printf(
		TEXT("%s\nDoors are open. Board now before departure.\nPlayers still outside are safely pulled aboard when the doors close.\nDeparture starts the next route load."),
		DestinationText.IsEmpty() ? TEXT("Next Stop") : *DestinationText);
}
}

ABHTrainIntermissionManager::ABHTrainIntermissionManager()
{
	bReplicates = true;
	ArrivalSeconds = BHIntermissionDefaultArrivalSeconds;
	RecapSeconds = 35.0f;
	BonusQuestionSeconds = 60.0f;
	ShopSeconds = 45.0f;
	StationStopSeconds = 30.0f;
	DepartureSeconds = 12.0f;
	CurrentPhase = EBHTrainPhase::Inactive;
	CompletedStageIndex = 0;
	NextMapName = TEXT("Substation");
	DestinationText = TEXT("Next Stop: Substation");
	bFinalRecap = false;

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings)
	{
		RecapSeconds = FMath::Max(5, Settings->TrainRecapSeconds);
		BonusQuestionSeconds = FMath::Max(5, Settings->TrainBonusQuestionSeconds);
		ShopSeconds = FMath::Max(5, Settings->TrainShopSeconds);
		StationStopSeconds = FMath::Max(5, Settings->TrainStationStopSeconds);
		DepartureSeconds = FMath::Max(3, Settings->TrainDepartureCountdownSeconds);
	}
}

void ABHTrainIntermissionManager::ConfigureIntermission(int32 NewCompletedStageIndex, const FString& NewNextMapName, const FString& NewDestinationText, bool bNewFinalRecap)
{
	CompletedStageIndex = FMath::Max(0, NewCompletedStageIndex);
	NextMapName = NewNextMapName.IsEmpty() ? TEXT("Substation") : NewNextMapName;
	DestinationText = NewDestinationText.IsEmpty() ? FString::Printf(TEXT("Next Stop: %s"), *NextMapName) : NewDestinationText;
	bFinalRecap = bNewFinalRecap;
}

void ABHTrainIntermissionManager::RegisterDoor(ABHTrainDoor* Door)
{
	if (Door)
	{
		Doors.AddUnique(Door);
	}
}

void ABHTrainIntermissionManager::RegisterDisplay(ABHTrainDisplayActor* Display)
{
	if (Display)
	{
		Displays.AddUnique(Display);
	}
}

void ABHTrainIntermissionManager::RegisterTunnelMotion(ABHTrainTunnelMotionActor* TunnelMotion)
{
	if (TunnelMotion)
	{
		TunnelMotionActors.AddUnique(TunnelMotion);
	}
}

void ABHTrainIntermissionManager::RegisterMovingBarrier(ABHBlockActor* VisibleShutter, ABHBlockActor* HiddenBlocker)
{
	if (VisibleShutter)
	{
		MovingShutters.AddUnique(VisibleShutter);
		VisibleShutter->SetBlockHiddenInGame(true);
		VisibleShutter->SetBlockCollisionEnabled(false);
	}
	if (HiddenBlocker)
	{
		MovingHiddenBlockers.AddUnique(HiddenBlocker);
		HiddenBlocker->SetBlockHiddenInGame(true);
		HiddenBlocker->SetBlockCollisionEnabled(false);
	}
}

void ABHTrainIntermissionManager::RegisterBonusTerminal(ABHTrainBonusQuestionTerminal* Terminal)
{
	BonusTerminal = Terminal;
}

EBHTrainPhase ABHTrainIntermissionManager::GetPhase() const
{
	return CurrentPhase;
}

void ABHTrainIntermissionManager::TesterAdvancePhase()
{
	if (!HasAuthority() || CurrentPhase == EBHTrainPhase::Inactive)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	AdvancePhase();
}

void ABHTrainIntermissionManager::TesterFinishIntermission()
{
	if (!HasAuthority())
	{
		return;
	}

	// Drive the finish through the Departing case so it inherits the same retry-on-failure watchdog
	// instead of a single fire-and-forget attempt that could leave the session stuck.
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	CurrentPhase = EBHTrainPhase::Departing;
	AdvancePhase();
}

void ABHTrainIntermissionManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		StartIntermission();
	}
}

void ABHTrainIntermissionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
		GetWorldTimerManager().ClearAllTimersForObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ABHTrainIntermissionManager::StartIntermission()
{
	if (BonusTerminal)
	{
		BonusTerminal->LoadQuestion(SelectBonusTopic(), CompletedStageIndex * BHIntermissionSeedStride + BHIntermissionSeedOffset);
	}

	if (ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr)
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Intermission);
		BHGS->SetRemainingTime(0);
		BHGS->SetExitUnlocked(false);
		BHGS->SetIntermissionLocks(true, false, false);

		if (UBHGameInstance* BHGI = GetWorld()->GetGameInstance<UBHGameInstance>())
		{
			BHGS->SetTrainRecap(
				BHGI->BuildTrainRecapOverview(),
				BHGI->BuildTrainRecapTopics(),
				BHGI->BuildTrainRecapMissedQuestions(),
				BHGI->BuildTrainRecapTips(DestinationText));
		}
	}

	SetTrainDoorsOpen(true);
	SetTunnelMoving(false);
	SetPhase(EBHTrainPhase::Arrival, ArrivalSeconds, TEXT("Doors open. Board now; snacks, drinks, and minigames are optional once aboard."));
}

namespace
{
	// Short, projector-safe coaching tip rotated across intermission legs (#9). The train window is
	// the only safe, no-Hunter time students will actually read, so it teaches the "why". Never
	// reveals answers or hidden positions; classroom-board safe.
	FString BHTrainCoachingTip(int32 LegSeed)
	{
		static const TCHAR* const Tips[] = {
			TEXT("Tip: exits need power AND tasks AND class mastery - the HUD shows what's still missing."),
			TEXT("Tip: when the TEACHER bar climbs, hide in a locker or break line of sight, don't just outrun."),
			TEXT("Tip: high Fear makes sprinting louder - move calmly near danger to stay quiet."),
			TEXT("Tip: question diagrams show the given values, never the answer. Read them, then pick 1-4."),
			TEXT("Tip: Hall Monitors answer at stations first to unlock their hint and trap tools."),
		};
		const int32 Count = UE_ARRAY_COUNT(Tips);
		const int32 Index = ((LegSeed % Count) + Count) % Count;
		return FString(Tips[Index]);
	}
}

void ABHTrainIntermissionManager::SetPhase(EBHTrainPhase NewPhase, float DurationSeconds, const FString& Announcement)
{
	CurrentPhase = NewPhase;
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	if (ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr)
	{
		BHGS->SetTrainState(NewPhase, CompletedStageIndex, Now + FMath::Max(0.0f, DurationSeconds), DestinationText, Announcement);
	}

	if (BonusTerminal)
	{
		BonusTerminal->RefreshDisplay();
	}
	if (!Announcement.IsEmpty() && GetWorld())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
			{
				PC->ClientShowStatusMessage(Announcement, 3.25f);
			}
		}
	}

	UpdateDisplaysForPhase();
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABHTrainIntermissionManager::AdvancePhase, FMath::Max(0.1f, DurationSeconds), false);
}

void ABHTrainIntermissionManager::AdvancePhase()
{
	switch (CurrentPhase)
	{
	case EBHTrainPhase::Arrival:
		AutoBoardPlayers();
		SetTrainDoorsOpen(false);
		SetTunnelMoving(true);
		// On the final stage the train doesn't stop anywhere else, so make the single ride ~1 minute before it
		// pulls into its terminal and the game ends.
		SetPhase(EBHTrainPhase::Recap, bFinalRecap ? FMath::Max(RecapSeconds, 55.0f) : RecapSeconds,
			bFinalRecap
				? FString::Printf(TEXT("Final service. Review the class recap; arriving at the terminal shortly.  %s"), *BHTrainCoachingTip(CompletedStageIndex))
				: FString::Printf(TEXT("Recap boards live. Review summary, weak topics, or try the social-car activities.  %s"), *BHTrainCoachingTip(CompletedStageIndex)));
		break;
	case EBHTrainPhase::Recap:
		if (bFinalRecap)
		{
			// Final stage: arrive at the terminal and END the game in place -- no onward travel. The GameMode
			// posts the top-5 leaderboard + class summary to the boards and exports the class/per-student files.
			// Then we hold here (no phase timer) so the host can review before returning to the lobby.
			// Pull any straggler who wandered out of the carriage during the ride back aboard first, matching
			// the normal StationStop boarding transitions, so nobody is stranded in the tunnel at the results hold.
			AutoBoardPlayers();
			SetTunnelMoving(false);
			SetTrainDoorsOpen(true);
			CurrentPhase = EBHTrainPhase::StationStop;
			if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
			{
				BHGM->EnterFinalResultsHold();
			}
			UpdateDisplaysForPhase();
			GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
		}
		else
		{
			SetPhase(EBHTrainPhase::BonusQuestion, BonusQuestionSeconds, TEXT("Bonus terminal live. Answer weak-topic questions or play optional minigames for small points."));
		}
		break;
	case EBHTrainPhase::BonusQuestion:
		SetPhase(EBHTrainPhase::Shop, ShopSeconds, FString::Printf(TEXT("Shop carriage open. Buy upgrades; snacks, drinks, and minigames stay open while waiting.  %s"), *BHTrainCoachingTip(CompletedStageIndex + 2)));
		break;
	case EBHTrainPhase::Shop:
		SetTunnelMoving(false);
		SetTrainDoorsOpen(true);
		SetPhase(EBHTrainPhase::StationStop, StationStopSeconds, FString::Printf(TEXT("Station stop. Doors open for %s. Board before departure."), *DestinationText));
		break;
	case EBHTrainPhase::StationStop:
		AutoBoardPlayers();
		SetTrainDoorsOpen(false);
		SetTunnelMoving(true);
		SetPhase(EBHTrainPhase::Departing, DepartureSeconds, FString::Printf(TEXT("Boarding closed. Departing for %s; next route loading."), *DestinationText));
		break;
	case EBHTrainPhase::Departing:
		if (!FinishIntermission())
		{
			// The GameMode could not travel yet (transiently null / not authority). Re-arm the phase
			// timer so the Departing step keeps retrying instead of bricking the session at the train.
			GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
			GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABHTrainIntermissionManager::AdvancePhase, 1.0f, false);
		}
		break;
	default:
		break;
	}
}

void ABHTrainIntermissionManager::UpdateDisplaysForPhase()
{
	ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	for (int32 Index = 0; Index < Displays.Num(); ++Index)
	{
		ABHTrainDisplayActor* Display = Displays[Index];
		if (!Display)
		{
			continue;
		}

		FString Header = TEXT("BLACKOUT TRANSIT");
		FString Body = DestinationText;
		FLinearColor Accent = BHTrainPhaseAccent(CurrentPhase);
		if (Index % 5 == 0)
		{
			Display->ConfigureTrainPhaseDisplay(DestinationText, Accent);
			continue;
		}

		if (BHGS)
		{
			switch (Index % 5)
			{
			case 1:
				Header = TEXT("CLASS OVERVIEW");
				Body = BHTrainNonEmpty(BHGS->TrainRecapOverview, TEXT("Class recap syncing. Check this board again in a moment."));
				Accent = FLinearColor(0.46f, 0.90f, 0.62f, 1.0f);
				break;
			case 2:
				if (CurrentPhase == EBHTrainPhase::BonusQuestion)
				{
					Header = TEXT("BONUS TERMINAL");
					Body = BHBuildBonusDisplayBody(BHGS);
					Accent = FLinearColor(1.0f, 0.72f, 0.20f, 1.0f);
				}
				else
				{
					Header = TEXT("TOPICS");
					Body = BHTrainNonEmpty(BHGS->TrainRecapTopics, TEXT("Topic rollup syncing. Bonus questions will use the current weak topic."));
					Accent = FLinearColor(0.46f, 0.72f, 1.0f, 1.0f);
				}
				break;
			case 3:
				if (bFinalRecap)
				{
					Header = TEXT("FINAL LEADERBOARD");
					Body = BHTrainNonEmpty(BHGS->TrainRecapOverview, TEXT("Final recap syncing. Train service ends at this station."));
					Accent = FLinearColor(1.0f, 0.76f, 0.24f, 1.0f);
				}
				else if (CurrentPhase == EBHTrainPhase::StationStop || CurrentPhase == EBHTrainPhase::Departing)
				{
					Header = TEXT("BOARDING CALL");
					Body = CurrentPhase == EBHTrainPhase::StationStop
						? BHBuildBoardingDisplayBody(DestinationText)
						: FString::Printf(TEXT("%s\nDoors are closed. Hold position while the next route loads."), DestinationText.IsEmpty() ? TEXT("Next Stop") : *DestinationText);
					Accent = CurrentPhase == EBHTrainPhase::StationStop ? FLinearColor(0.18f, 1.0f, 0.50f, 1.0f) : FLinearColor(1.0f, 0.26f, 0.14f, 1.0f);
				}
				else
				{
					Header = TEXT("SHOP / POWERUPS");
					Body = BHBuildShopDisplayBody();
					Accent = FLinearColor(0.82f, 0.58f, 1.0f, 1.0f);
				}
				break;
			case 4:
			default:
				Header = TEXT("NEXT ROUTE");
				if (CurrentPhase == EBHTrainPhase::Recap)
				{
					Body = BHTrainNonEmpty(BHGS->TrainRecapMissedQuestions, TEXT("No missed question data yet. Keep answering revision nodes to fill this board."));
					Header = TEXT("MISSED QUESTIONS");
					Accent = FLinearColor(1.0f, 0.48f, 0.30f, 1.0f);
				}
				else if (!bFinalRecap && (CurrentPhase == EBHTrainPhase::BonusQuestion || CurrentPhase == EBHTrainPhase::Shop))
				{
					Header = TEXT("SOCIAL CAR");
					Body = BHBuildActivitiesDisplayBody();
					Accent = FLinearColor(0.36f, 1.0f, 0.56f, 1.0f);
				}
				else
				{
					Body = BHTrainNonEmpty(BHGS->TrainRecapTips, TEXT("Review, answer bonus questions, buy upgrades, and board before departure."));
					Accent = FLinearColor(0.78f, 0.58f, 1.0f, 1.0f);
				}
				break;
			}
		}

		Display->ConfigureDisplay(Header, Body, Accent);
	}
}

void ABHTrainIntermissionManager::SetTrainDoorsOpen(bool bOpen)
{
	for (ABHTrainDoor* Door : Doors)
	{
		if (Door)
		{
			Door->SetDoorOpen(bOpen);
		}
	}
}

void ABHTrainIntermissionManager::SetTunnelMoving(bool bMoving)
{
	for (ABHTrainTunnelMotionActor* TunnelMotion : TunnelMotionActors)
	{
		if (TunnelMotion)
		{
			TunnelMotion->SetMoving(bMoving);
		}
	}
	SetMovingBarriersClosed(bMoving);
}

void ABHTrainIntermissionManager::SetMovingBarriersClosed(bool bClosed)
{
	for (ABHBlockActor* Shutter : MovingShutters)
	{
		if (Shutter)
		{
			Shutter->SetBlockHiddenInGame(!bClosed);
			Shutter->SetBlockCollisionEnabled(bClosed);
		}
	}
	for (ABHBlockActor* Blocker : MovingHiddenBlockers)
	{
		if (Blocker)
		{
			Blocker->SetBlockHiddenInGame(true);
			Blocker->SetBlockCollisionEnabled(bClosed);
		}
	}
}

void ABHTrainIntermissionManager::AutoBoardPlayers()
{
	if (!GetWorld())
	{
		return;
	}

	int32 Index = 0;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		ABHCharacter* Character = Controller ? Cast<ABHCharacter>(Controller->GetPawn()) : nullptr;
		if (!Character)
		{
			continue;
		}

		// Continuity: only pull in players who are actually OUTSIDE the carriage (up on the roof, fallen into the
		// trench, etc.). A player already standing inside keeps their exact spot, so the doors-close transition no
		// longer yanks ("respawns") everyone right as the train starts moving and the ride sway blends in.
		// The X band must cover the WHOLE seven-car interior: the tube runs +/-5250 with lounge cars centred at
		// +/-4500 (ABHGameMode::BuildTrainIntermissionLevel), so a band any narrower treats every lounge sitter
		// as "outside" and teleports them mid-train at each doors-close. 5230 insets the 18cm end-cap walls.
		const FVector Location = Character->GetActorLocation();
		const bool bAlreadyAboard =
			FMath::Abs(Location.Y) <= 290.0f &&
			Location.X >= -5230.0f && Location.X <= 5230.0f &&
			Location.Z >= 40.0f && Location.Z <= 330.0f;
		if (bAlreadyAboard)
		{
			continue;
		}

		const FVector SafeLocation(CompletedStageIndex * 25.0f - 450.0f + Index * 95.0f, -160.0f + (Index % 4) * 105.0f, 124.0f);
		Character->SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);
		++Index;
	}
}

bool ABHTrainIntermissionManager::FinishIntermission()
{
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		return BHGM->CompleteTrainIntermission(NextMapName, bFinalRecap);
	}
	return false;
}

EBHPhysicsTopic ABHTrainIntermissionManager::SelectBonusTopic() const
{
	if (const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr)
	{
		return BHGS->RevisionWeakTopic;
	}
	return EBHPhysicsTopic::ForcesAndMotion;
}
