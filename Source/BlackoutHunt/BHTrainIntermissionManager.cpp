#include "BHTrainIntermissionManager.h"
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

ABHTrainIntermissionManager::ABHTrainIntermissionManager()
{
	bReplicates = true;
	ArrivalSeconds = 5.0f;
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

	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	FinishIntermission();
}

void ABHTrainIntermissionManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		StartIntermission();
	}
}

void ABHTrainIntermissionManager::StartIntermission()
{
	if (BonusTerminal)
	{
		BonusTerminal->LoadQuestion(SelectBonusTopic(), CompletedStageIndex * 171 + 31);
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
	SetPhase(EBHTrainPhase::Arrival, ArrivalSeconds, TEXT("Doors open. Board the remediation train."));
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
		SetPhase(EBHTrainPhase::Recap, RecapSeconds, TEXT("Class performance review in progress."));
		break;
	case EBHTrainPhase::Recap:
		if (bFinalRecap)
		{
			SetTunnelMoving(false);
			SetTrainDoorsOpen(true);
			SetPhase(EBHTrainPhase::StationStop, StationStopSeconds, TEXT("Final leaderboard posted. Train service ends here."));
		}
		else
		{
			SetPhase(EBHTrainPhase::BonusQuestion, BonusQuestionSeconds, TEXT("Weak areas detected. Bonus questions are now live."));
		}
		break;
	case EBHTrainPhase::BonusQuestion:
		SetPhase(EBHTrainPhase::Shop, ShopSeconds, TEXT("Shop carriage unlocked. Spend earned question points."));
		break;
	case EBHTrainPhase::Shop:
		SetTunnelMoving(false);
		SetTrainDoorsOpen(true);
		SetPhase(EBHTrainPhase::StationStop, StationStopSeconds, DestinationText);
		break;
	case EBHTrainPhase::StationStop:
		AutoBoardPlayers();
		SetTrainDoorsOpen(false);
		SetTunnelMoving(true);
		SetPhase(EBHTrainPhase::Departing, DepartureSeconds, TEXT("Boarding closed. Departing."));
		break;
	case EBHTrainPhase::Departing:
		FinishIntermission();
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
		FLinearColor Accent(0.14f, 0.82f, 0.74f, 1.0f);
		if (BHGS)
		{
			switch (Index % 5)
			{
			case 0:
				Header = TEXT("CLASS OVERVIEW");
				Body = BHGS->TrainRecapOverview;
				Accent = FLinearColor(0.46f, 0.90f, 0.62f, 1.0f);
				break;
			case 1:
				Header = TEXT("TOPICS");
				Body = BHGS->TrainRecapTopics;
				Accent = FLinearColor(0.46f, 0.72f, 1.0f, 1.0f);
				break;
			case 2:
				Header = TEXT("MISSED QUESTIONS");
				Body = BHGS->TrainRecapMissedQuestions;
				Accent = FLinearColor(1.0f, 0.48f, 0.30f, 1.0f);
				break;
			case 3:
				Header = bFinalRecap ? TEXT("LEADERBOARD") : TEXT("SHOP / POWERUPS");
				Body = bFinalRecap ? BHGS->TrainRecapOverview : TEXT("Stamina Boost, Sprint Burst, Light Boost, Question Hint, Decoy Sound, and Door Rush are available in the next carriage.");
				Accent = FLinearColor(1.0f, 0.76f, 0.24f, 1.0f);
				break;
			case 4:
			default:
				Header = TEXT("NEXT ROUTE");
				Body = BHGS->TrainRecapTips;
				Accent = FLinearColor(0.78f, 0.58f, 1.0f, 1.0f);
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

		const FVector SafeLocation(CompletedStageIndex * 25.0f - 450.0f + Index * 95.0f, -160.0f + (Index % 4) * 105.0f, 124.0f);
		Character->SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);
		++Index;
	}
}

void ABHTrainIntermissionManager::FinishIntermission()
{
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->CompleteTrainIntermission(NextMapName, bFinalRecap);
	}
}

EBHPhysicsTopic ABHTrainIntermissionManager::SelectBonusTopic() const
{
	if (const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr)
	{
		return BHGS->RevisionWeakTopic;
	}
	return EBHPhysicsTopic::ForcesAndMotion;
}
