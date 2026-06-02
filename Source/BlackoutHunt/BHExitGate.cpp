// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHExitGate.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"

ABHExitGate::ABHExitGate()
{
	InteractionLabel = FText::FromString(TEXT("Exit"));
	bDirectorActive = true;
	SetActorScale3D(FVector(2.0f, 0.2f, 2.5f));

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Status beacon that color-codes the gate state and is visible in the dark from across the room.
	// Relative Z is pre-scale: the actor's 2.5x Z scale lifts it to ~1.8m, just above the gate top so
	// the gate body never occludes it.
	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(GetRootComponent());
	StatusLight->SetRelativeLocation(FVector(0.0f, 0.0f, 72.0f));
	StatusLight->SetAttenuationRadius(520.0f);
	StatusLight->SetCastShadows(false);
	StatusLight->SetIntensity(0.0f);
	StatusLight->SetMobility(EComponentMobility::Movable);
}

void ABHExitGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyExitGateVisuals();
}

void ABHExitGate::ApplyExitGateVisuals()
{
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bUnlocked = BHGS && BHGS->bExitUnlocked;

	// 0 inactive (not the exit this round), 1 locked (objectives outstanding), 2 escape-ready.
	const int32 State = !bDirectorActive ? 0 : (bUnlocked ? 2 : 1);

	FLinearColor BodyColor;
	FLinearColor LightColor;
	float BaseIntensity;
	float Glow;
	switch (State)
	{
	case 2: // escape-ready: inviting green, the only state that pulses to pull the eye
		BodyColor = FLinearColor(0.10f, 0.32f, 0.18f, 1.0f);
		LightColor = FLinearColor(0.16f, 1.0f, 0.45f, 1.0f);
		BaseIntensity = 1500.0f;
		Glow = 0.9f;
		break;
	case 1: // locked: steady amber-red warning
		BodyColor = FLinearColor(0.16f, 0.11f, 0.07f, 1.0f);
		LightColor = FLinearColor(1.0f, 0.30f, 0.10f, 1.0f);
		BaseIntensity = 900.0f;
		Glow = 0.22f;
		break;
	default: // inactive: dead, unlit slate
		BodyColor = FLinearColor(0.05f, 0.05f, 0.06f, 1.0f);
		LightColor = FLinearColor(0.20f, 0.20f, 0.22f, 1.0f);
		BaseIntensity = 0.0f;
		Glow = 0.0f;
		break;
	}

	if (State != LastVisualState)
	{
		BHPropVisuals::TintPart(Mesh, BodyColor, Glow);
		LastVisualState = State;
	}

	if (StatusLight)
	{
		const float Pulse = (State == 2 && GetWorld())
			? 0.78f + 0.22f * FMath::Sin(GetWorld()->GetTimeSeconds() * 3.2f)
			: 1.0f;
		StatusLight->SetVisibility(State != 0);
		StatusLight->SetLightColor(LightColor);
		StatusLight->SetIntensity(BaseIntensity * Pulse);
	}
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
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (BHGS && BHGS->bTutorialMode && BHPS && (BHPS->PlayerRole == EBHPlayerRole::FakeHunter || BHPS->IsAliveHunter()))
	{
		return FText::FromString(TEXT("Training Exit"));
	}
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
	else if (BHGS && BHGS->bTutorialMode && (BHPS->PlayerRole == EBHPlayerRole::FakeHunter || BHPS->IsAliveHunter()))
	{
		// In the tutorial the Teacher/Monitor are deliberately sent to this door to finish, and the director
		// completes the lesson on proximity - so don't scold them with "CANNOT ESCAPE"; it just reads as broken.
		Info.DisabledReason = FText::FromString(TEXT("WALK IN TO FINISH"));
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
