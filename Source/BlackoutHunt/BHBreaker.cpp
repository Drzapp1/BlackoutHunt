// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHBreaker.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
bool BHBreakerAllowsWarmup(const ABHGameState* GameState)
{
	return GameState
		&& GameState->RoundPhase == EBHRoundPhase::Prep
		&& !GameState->bPracticeMode
		&& !GameState->bTestMode;
}
}

ABHBreaker::ABHBreaker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;
	InteractionLabel = FText::FromString(TEXT("Repair Breaker"));
	RepairProgress = 0.0f;
	bRepaired = false;
	bDirectorActive = true;
	RepairSeconds = 6.0f;
	LastNoiseTime = -999.0f;
	LastHumCueTime = -999.0f;
	SetActorScale3D(FVector::OneVector);

	FrontPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontPanel"));
	FrontPanel->SetupAttachment(RootComponent);
	WarningBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningBand"));
	WarningBand->SetupAttachment(RootComponent);
	GaugeFace = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GaugeFace"));
	GaugeFace->SetupAttachment(RootComponent);
	GaugeNeedle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GaugeNeedle"));
	GaugeNeedle->SetupAttachment(RootComponent);
	BreakerLeverA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BreakerLeverA"));
	BreakerLeverA->SetupAttachment(RootComponent);
	BreakerLeverB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BreakerLeverB"));
	BreakerLeverB->SetupAttachment(RootComponent);
	BreakerLeverC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BreakerLeverC"));
	BreakerLeverC->SetupAttachment(RootComponent);
	StatusLightA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLightA"));
	StatusLightA->SetupAttachment(RootComponent);
	StatusLightB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLightB"));
	StatusLightB->SetupAttachment(RootComponent);
	StatusLightC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLightC"));
	StatusLightC->SetupAttachment(RootComponent);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(0.86f, 0.30f, 1.32f), true);
	BHPropVisuals::ConfigurePart(FrontPanel, BHPropVisuals::CubeMesh(), BHPropVisuals::DiamondPlateMaterial(), FVector(44.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(0.035f, 0.245f, 1.08f));
	BHPropVisuals::ConfigurePart(WarningBand, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(47.0f, 0.0f, 54.0f), FRotator::ZeroRotator, FVector(0.018f, 0.265f, 0.085f));
	BHPropVisuals::ConfigurePart(GaugeFace, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(49.0f, 0.0f, 20.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.18f, 0.18f, 0.025f));
	BHPropVisuals::ConfigurePart(GaugeNeedle, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(51.5f, 0.0f, 20.0f), FRotator(0.0f, 0.0f, -55.0f), FVector(0.018f, 0.025f, 0.145f));
	BHPropVisuals::ConfigurePart(BreakerLeverA, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(50.5f, -15.0f, -28.0f), FRotator(0.0f, 0.0f, -22.0f), FVector(0.03f, 0.035f, 0.28f));
	BHPropVisuals::ConfigurePart(BreakerLeverB, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(50.5f, 0.0f, -28.0f), FRotator(0.0f, 0.0f, -22.0f), FVector(0.03f, 0.035f, 0.28f));
	BHPropVisuals::ConfigurePart(BreakerLeverC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(50.5f, 15.0f, -28.0f), FRotator(0.0f, 0.0f, -22.0f), FVector(0.03f, 0.035f, 0.28f));
	BHPropVisuals::ConfigurePart(StatusLightA, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(52.0f, -20.0f, 42.0f), FRotator::ZeroRotator, FVector(0.055f));
	BHPropVisuals::ConfigurePart(StatusLightB, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(52.0f, 0.0f, 42.0f), FRotator::ZeroRotator, FVector(0.055f));
	BHPropVisuals::ConfigurePart(StatusLightC, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(52.0f, 20.0f, 42.0f), FRotator::ZeroRotator, FVector(0.055f));
	ApplyBreakerVisuals();
}

void ABHBreaker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bDirectorActive || bRepaired || Repairers.Num() == 0)
	{
		if (HasAuthority())
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && !BHBreakerAllowsWarmup(BHGS)))
	{
		Repairers.Empty();
		SetActorTickEnabled(false);
		return;
	}

	for (auto It = Repairers.CreateIterator(); It; ++It)
	{
		const ABHCharacter* Repairer = It->Get();
		// Drop repairers whose pawn is gone, and also those who can no longer act
		// (e.g. captured mid-repair): a captured survivor is still IsValid() but must
		// stop contributing to the repair speed scaled by Repairers.Num() below.
		const ABHPlayerState* RepairerPS = Repairer ? Repairer->GetBHPlayerState() : nullptr;
		if (!Repairer || !RepairerPS || !RepairerPS->IsAliveSurvivor())
		{
			It.RemoveCurrent();
		}
	}

	if (Repairers.Num() == 0)
	{
		SetActorTickEnabled(false);
		return;
	}

	const float RepairRate = FMath::Max(0.1f, RepairSeconds);
	RepairProgress = FMath::Clamp(RepairProgress + (DeltaSeconds * Repairers.Num() / RepairRate), 0.0f, 1.0f);
	ApplyBreakerVisuals();

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastHumCueTime >= 2.4f)
	{
		BroadcastBreakerAudioCue(false);
	}

	if (RepairProgress >= 1.0f)
	{
		CompleteRepair();
	}
}

void ABHBreaker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHBreaker, RepairProgress);
	DOREPLIFETIME(ABHBreaker, bRepaired);
	DOREPLIFETIME(ABHBreaker, bDirectorActive);
}

bool ABHBreaker::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return bDirectorActive && BHPS && BHGS && (BHGS->RoundPhase == EBHRoundPhase::Hunt || BHBreakerAllowsWarmup(BHGS)) && BHPS->IsAliveSurvivor() && !bRepaired;
}

void ABHBreaker::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority() && CanInteract_Implementation(Character))
	{
		Repairers.Add(Character);
		SetActorTickEnabled(true);
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Now - LastNoiseTime > 2.0f)
		{
			LastNoiseTime = Now;
			const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
			if (!BHBreakerAllowsWarmup(BHGS))
			{
				if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
				{
					BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("breaker repair"));
				}
			}
		}
	}
}

void ABHBreaker::EndInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority())
	{
		Repairers.Remove(Character);
		if (Repairers.Num() == 0)
		{
			SetActorTickEnabled(false);
		}
	}
}

FText ABHBreaker::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	if (!bDirectorActive)
	{
		return FText::FromString(TEXT("Dead Breaker"));
	}

	if (bRepaired)
	{
		return FText::FromString(TEXT("Breaker Repaired"));
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		return FText::FromString(TEXT("Ready Up First"));
	}

	if (!BHPS->IsAliveSurvivor())
	{
		return FText::FromString(TEXT("Survivor Objective"));
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (BHBreakerAllowsWarmup(BHGS))
	{
		return FText::FromString(FString::Printf(TEXT("Warmup Repair Breaker %d%%"), FMath::RoundToInt(RepairProgress * 100.0f)));
	}

	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return FText::FromString(TEXT("Repair During Hunt"));
	}

	return FText::FromString(FString::Printf(TEXT("Repair Breaker %d%%"), FMath::RoundToInt(RepairProgress * 100.0f)));
}

FBHInteractionPromptInfo ABHBreaker::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = RepairSeconds;
	Info.Progress = RepairProgress;
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bRoleWarmup = BHBreakerAllowsWarmup(BHGS);
	Info.RiskText = FText::FromString(bRoleWarmup ? TEXT("WARMUP") : TEXT("NOISY"));
	if (!bDirectorActive)
	{
		Info.DisabledReason = FText::FromString(TEXT("INACTIVE THIS ROUND"));
	}
	else if (bRepaired)
	{
		Info.DisabledReason = FText::FromString(TEXT("DONE"));
	}
	else if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		Info.DisabledReason = FText::FromString(TEXT("READY UP FIRST"));
	}
	else if (!BHPS->IsAliveSurvivor())
	{
		Info.DisabledReason = FText::FromString(BHPS->PlayerRole == EBHPlayerRole::FakeHunter ? TEXT("HALL MONITORS CANNOT REPAIR") : TEXT("SURVIVOR REPAIR ONLY"));
	}
	else if (!BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && !bRoleWarmup))
	{
		Info.DisabledReason = FText::FromString(TEXT("HUNT PHASE ONLY"));
	}
	else
	{
		Info.DisabledReason = FText::FromString(TEXT("LOCKED"));
	}
	Info.bNoisy = !bRoleWarmup;
	Info.bDangerous = !bRoleWarmup;
	return Info;
}

void ABHBreaker::SetDirectorActive(bool bNewActive)
{
	bDirectorActive = bNewActive;
	bRepaired = false;
	Repairers.Empty();
	RepairProgress = 0.0f;
	SetActorTickEnabled(false);
	ApplyBreakerVisuals();
}

bool ABHBreaker::IsDirectorActive() const
{
	return bDirectorActive;
}

bool ABHBreaker::IsRepaired() const
{
	return bRepaired;
}

void ABHBreaker::OnRep_BreakerVisuals()
{
	ApplyBreakerVisuals();
}

void ABHBreaker::ApplyBreakerVisuals()
{
	const bool bWorking = bDirectorActive && !bRepaired;
	const FLinearColor WarningColor = bDirectorActive ? FLinearColor(0.90f, 0.64f, 0.08f, 1.0f) : FLinearColor(0.08f, 0.08f, 0.07f, 1.0f);
	const FLinearColor GaugeColor = bDirectorActive ? FLinearColor(0.05f, 0.08f, 0.09f, 1.0f) : FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);
	const FLinearColor NeedleColor = bRepaired ? FLinearColor(0.18f, 0.94f, 0.42f, 1.0f) : bWorking ? FLinearColor(0.88f, 0.64f, 0.18f, 1.0f) : FLinearColor(0.10f, 0.10f, 0.10f, 1.0f);
	const FLinearColor LeverColor = bWorking ? FLinearColor(0.14f, 0.16f, 0.17f, 1.0f) : FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
	const FLinearColor LightColor = !bDirectorActive
		? FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)
		: bRepaired
			? FLinearColor(0.10f, 0.95f, 0.36f, 1.0f)
			: FLinearColor(0.92f, 0.24f, 0.12f, 1.0f);
	const float Emissive = bDirectorActive ? 2.6f : 0.0f;
	const float NeedleRoll = FMath::Lerp(-55.0f, 55.0f, RepairProgress);
	const float LeverRoll = bRepaired ? 24.0f : -22.0f;

	if (GaugeNeedle)
	{
		GaugeNeedle->SetRelativeRotation(FRotator(0.0f, 0.0f, NeedleRoll));
	}
	if (BreakerLeverA)
	{
		BreakerLeverA->SetRelativeRotation(FRotator(0.0f, 0.0f, LeverRoll));
	}
	if (BreakerLeverB)
	{
		BreakerLeverB->SetRelativeRotation(FRotator(0.0f, 0.0f, bRepaired ? 24.0f : -8.0f));
	}
	if (BreakerLeverC)
	{
		BreakerLeverC->SetRelativeRotation(FRotator(0.0f, 0.0f, LeverRoll));
	}

	BHPropVisuals::TintPart(WarningBand, WarningColor);
	BHPropVisuals::TintPart(GaugeFace, GaugeColor);
	BHPropVisuals::TintPart(GaugeNeedle, NeedleColor, Emissive);
	BHPropVisuals::TintPart(BreakerLeverA, LeverColor);
	BHPropVisuals::TintPart(BreakerLeverB, LeverColor);
	BHPropVisuals::TintPart(BreakerLeverC, LeverColor);
	BHPropVisuals::TintPart(StatusLightA, LightColor, Emissive);
	BHPropVisuals::TintPart(StatusLightB, LightColor * (bRepaired ? 1.0f : 0.45f), Emissive);
	BHPropVisuals::TintPart(StatusLightC, bWorking ? FLinearColor(0.90f, 0.58f, 0.10f, 1.0f) : LightColor, Emissive);
}

void ABHBreaker::BroadcastBreakerAudioCue(bool bCompleted)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings)
	{
		return;
	}

	FBHGameplayAudioCue Cue;
	Cue.AudioAsset = bCompleted ? Settings->BreakerCompleteSound : Settings->BreakerHumSound;
	Cue.Location = GetActorLocation() + FVector(0.0f, 0.0f, 62.0f);
	Cue.Caption = bCompleted ? TEXT("Breaker locks in.") : TEXT("Breaker hums under load.");
	Cue.Volume = bCompleted ? 0.54f : FMath::Lerp(0.24f, 0.40f, RepairProgress);
	Cue.Pitch = bCompleted ? 1.05f : FMath::Lerp(0.88f, 1.08f, RepairProgress);
	Cue.MaxAudibleDistance = bCompleted ? 2600.0f : 1900.0f;
	Cue.CaptionSeconds = bCompleted ? 1.5f : 1.1f;
	Cue.bSpatial = true;

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		LastHumCueTime = GetWorld()->GetTimeSeconds();
		BHGM->BroadcastGameplayAudioCue(Cue);
	}
}

void ABHBreaker::CompleteRepair()
{
	bRepaired = true;
	RepairProgress = 1.0f;
	Repairers.Empty();
	SetActorTickEnabled(false);
	ApplyBreakerVisuals();
	BroadcastBreakerAudioCue(true);

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		if (BHBreakerAllowsWarmup(BHGS))
		{
			return;
		}
		BHGM->NotifyBreakerRepaired(GetActorLocation());
	}
}
