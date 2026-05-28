#include "BHExitGate.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Net/UnrealNetwork.h"

ABHExitGate::ABHExitGate()
{
	InteractionLabel = FText::FromString(TEXT("Exit"));
	bDirectorActive = true;
	SetActorScale3D(FVector(2.0f, 0.2f, 2.5f));
}

void ABHExitGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHExitGate, bDirectorActive);
}

bool ABHExitGate::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return bDirectorActive && BHPS && BHGS && BHPS->IsAliveSurvivor() && BHGS->bExitUnlocked;
}

void ABHExitGate::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority() && CanInteract_Implementation(Character))
	{
		if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
		{
			BHGM->RecordPlaytestTelemetryMarker(TEXT("exit_route_choice"), GetActorLocation(), TEXT("escape_gate"), Character ? Character->GetBHPlayerState() : nullptr);
			BHGM->NotifySurvivorEscaped(Character);
		}
	}
}

FText ABHExitGate::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	if (!bDirectorActive)
	{
		return FText::FromString(TEXT("Inactive Exit"));
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return (BHGS && BHGS->bExitUnlocked) ? FText::FromString(TEXT("Escape")) : FText::FromString(TEXT("Exit Locked"));
}

FBHInteractionPromptInfo ABHExitGate::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("ESCAPE NOW"));
		return Info;
	}

	if (!bDirectorActive)
	{
		Info.DisabledReason = FText::FromString(TEXT("INACTIVE THIS ROUND"));
	}
	else if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		Info.DisabledReason = FText::FromString(TEXT("READY UP FIRST"));
	}
	else if (BHPS->PlayerRole == EBHPlayerRole::Spectator)
	{
		Info.DisabledReason = FText::FromString(TEXT("SPECTATOR SUPPORT ONLY"));
	}
	else if (BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		Info.DisabledReason = FText::FromString(TEXT("YOU ARE OUT OF PLAY"));
	}
	else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		Info.DisabledReason = FText::FromString(TEXT("HALL MONITORS CANNOT ESCAPE"));
	}
	else if (BHPS->IsAliveHunter())
	{
		Info.DisabledReason = FText::FromString(TEXT("TEACHER CANNOT ESCAPE"));
	}
	else if (!BHGS || !BHGS->bExitUnlocked)
	{
		if (BHGS && BHGS->bRevisionMode)
		{
			const int32 ContributionTarget = FMath::Clamp(BHGS->RevisionContributionTarget, 1, 4);
			if (BHGS->RevisionClassMasteryAverage < BHGS->RevisionClassThreshold)
			{
				Info.DisabledReason = FText::FromString(FString::Printf(TEXT("CLASS MASTERY %.0f/%.0f"), BHGS->RevisionClassMasteryAverage, BHGS->RevisionClassThreshold));
			}
			else if (BHPS->RevisionStats.MasteryPercent < BHGS->RevisionIndividualThreshold)
			{
				Info.DisabledReason = FText::FromString(FString::Printf(TEXT("YOUR MASTERY %.0f/%.0f"), BHPS->RevisionStats.MasteryPercent, BHGS->RevisionIndividualThreshold));
			}
			else if (BHPS->RevisionStats.ContributionCount < ContributionTarget)
			{
				Info.DisabledReason = FText::FromString(FString::Printf(TEXT("CONTRIBUTE %d/%d"), BHPS->RevisionStats.ContributionCount, ContributionTarget));
			}
			else
			{
				Info.DisabledReason = FText::FromString(TEXT("WAIT FOR CLASS GATE"));
			}
		}
		else if (BHGS)
		{
			const int32 RemainingBreakers = FMath::Max(0, BHGS->BreakersRequired - BHGS->BreakersCompleted);
			const int32 RemainingStations = FMath::Max(0, BHGS->SideObjectivesRequired - BHGS->SideObjectivesCompleted);
			Info.DisabledReason = (RemainingBreakers > 0 || RemainingStations > 0)
				? FText::FromString(FString::Printf(TEXT("FINISH POWER %d / TASKS %d"), RemainingBreakers, RemainingStations))
				: FText::FromString(TEXT("EXIT OPENING"));
		}
		else
		{
			Info.DisabledReason = FText::FromString(TEXT("EXIT LOCKED"));
		}
	}
	else
	{
		Info.DisabledReason = FText::FromString(TEXT("SURVIVOR EXIT ONLY"));
	}
	return Info;
}

void ABHExitGate::SetDirectorActive(bool bNewActive)
{
	bDirectorActive = bNewActive;
}

bool ABHExitGate::IsDirectorActive() const
{
	return bDirectorActive;
}
