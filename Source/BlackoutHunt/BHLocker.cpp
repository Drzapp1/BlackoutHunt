// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHLocker.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
bool BHLockerAllowsWarmupSearch(const ABHGameState* GameState)
{
	return GameState
		&& GameState->RoundPhase == EBHRoundPhase::Prep
		&& !GameState->bPracticeMode
		&& !GameState->bTestMode;
}
}

ABHLocker::ABHLocker()
{
	InteractionLabel = FText::FromString(TEXT("Locker"));
	SetActorScale3D(FVector::OneVector);
	Occupant = nullptr;

	// Tick at a low rate (0.5s) purely to re-evaluate the occupied indicator's per-viewer visibility when the local
	// player's hunter status changes without an occupancy change (role reassignment). Cheap; early-outs when empty.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;

	LeftDoorPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoorPanel"));
	LeftDoorPanel->SetupAttachment(RootComponent);
	RightDoorPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoorPanel"));
	RightDoorPanel->SetupAttachment(RootComponent);
	UpperVentLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpperVentLeft"));
	UpperVentLeft->SetupAttachment(RootComponent);
	UpperVentRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpperVentRight"));
	UpperVentRight->SetupAttachment(RootComponent);
	LowerVentLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerVentLeft"));
	LowerVentLeft->SetupAttachment(RootComponent);
	LowerVentRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerVentRight"));
	LowerVentRight->SetupAttachment(RootComponent);
	LockerHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LockerHandle"));
	LockerHandle->SetupAttachment(RootComponent);
	OccupiedIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OccupiedIndicator"));
	OccupiedIndicator->SetupAttachment(RootComponent);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.78f, 0.56f, 2.32f), true);
	BHPropVisuals::ConfigurePart(LeftDoorPanel, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(-19.0f, -30.0f, 1.0f), FRotator::ZeroRotator, FVector(0.34f, 0.035f, 2.03f));
	BHPropVisuals::ConfigurePart(RightDoorPanel, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(19.0f, -30.0f, 1.0f), FRotator::ZeroRotator, FVector(0.34f, 0.035f, 2.03f));
	BHPropVisuals::ConfigurePart(UpperVentLeft, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(-19.0f, -33.0f, 60.0f), FRotator::ZeroRotator, FVector(0.22f, 0.014f, 0.035f));
	BHPropVisuals::ConfigurePart(UpperVentRight, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(19.0f, -33.0f, 60.0f), FRotator::ZeroRotator, FVector(0.22f, 0.014f, 0.035f));
	BHPropVisuals::ConfigurePart(LowerVentLeft, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(-19.0f, -33.0f, -58.0f), FRotator::ZeroRotator, FVector(0.22f, 0.014f, 0.035f));
	BHPropVisuals::ConfigurePart(LowerVentRight, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(19.0f, -33.0f, -58.0f), FRotator::ZeroRotator, FVector(0.22f, 0.014f, 0.035f));
	BHPropVisuals::ConfigurePart(LockerHandle, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(5.0f, -35.0f, -3.0f), FRotator::ZeroRotator, FVector(0.045f, 0.025f, 0.50f));
	BHPropVisuals::ConfigurePart(OccupiedIndicator, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(-5.0f, -36.0f, 86.0f), FRotator::ZeroRotator, FVector(0.045f));
	ApplyLockerVisuals();
}

void ABHLocker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHLocker, Occupant);
}

bool ABHLocker::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS)
	{
		return false;
	}

	if (BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		// Match BeginInteract, which only searches an *occupied* locker. Without the Occupant!=nullptr
		// guard an empty locker showed the Tester a "search" prompt that then fell through and did nothing.
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		return BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && Occupant != nullptr && Occupant != Character;
	}

	if (BHPS->IsAliveHunter())
	{
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		return BHLockerAllowsWarmupSearch(BHGS) && Occupant != nullptr;
	}

	return BHPS->IsAliveSurvivor() && Occupant == nullptr;
}

void ABHLocker::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	ABHPlayerState* BHPS = Character->GetPlayerState<ABHPlayerState>();
	if (!BHPS)
	{
		BHPS = Character->GetBHPlayerState();
	}
	if (!BHPS)
	{
		return;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (BHPS->PlayerRole == EBHPlayerRole::Tester && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && Occupant && Occupant != Character)
	{
		if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
		{
			BHGM->RecordPlaytestTelemetryMarker(TEXT("locker_searched"), GetActorLocation(), TEXT("tester_search"), BHPS, Occupant->GetBHPlayerState());
			BHGM->NotifySurvivorCaptured(Occupant, Character);
		}
		Occupant = nullptr;
		ApplyLockerVisuals();
		return;
	}

	if (BHPS->IsAliveHunter())
	{
		const ABHGameState* LockerGameState = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		if (BHLockerAllowsWarmupSearch(LockerGameState) && Occupant)
		{
			if (ABHPlayerController* HunterPC = Cast<ABHPlayerController>(Character->GetController()))
			{
				HunterPC->ClientShowStatusMessage(TEXT("Warmup locker search: this survivor would be found. Hunt start resets everyone."), 3.0f);
			}
			if (ABHPlayerController* OccupantPC = Cast<ABHPlayerController>(Occupant->GetController()))
			{
				OccupantPC->ClientShowStatusMessage(TEXT("Warmup locker search found you. You stay in play until Hunt starts."), 3.0f);
			}
		}
		return;
	}

	if (BHPS->IsAliveSurvivor() && Occupant == nullptr)
	{
		Occupant = Character;
		ApplyLockerVisuals();
		Character->EnterLocker(this);
		const ABHGameState* LockerGameState = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		if (!BHLockerAllowsWarmupSearch(LockerGameState))
		{
			if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
			{
				BHGM->RecordPlaytestTelemetryMarker(TEXT("locker_entered"), GetActorLocation(), TEXT("hide"), BHPS);
			}
		}
	}
}

FText ABHLocker::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
		{
			return FText::FromString(TEXT("Tester Locker"));
		}
		return Occupant && Occupant != Character ? FText::FromString(TEXT("Tester Search")) : FText::FromString(TEXT("Tester Hide"));
	}

	if (BHPS && BHPS->IsAliveHunter())
	{
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		if (BHLockerAllowsWarmupSearch(BHGS) && Occupant)
		{
			return FText::FromString(TEXT("Warmup Search Locker"));
		}
		return Occupant ? FText::FromString(TEXT("Hidden From Teacher")) : FText::FromString(TEXT("Hiding Spot"));
	}

	if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		return FText::FromString(TEXT("Ready Up First"));
	}

	if (!BHPS->IsAliveSurvivor())
	{
		return FText::FromString(TEXT("Survivor Hiding Spot"));
	}

	return Occupant ? FText::FromString(TEXT("Occupied")) : FText::FromString(TEXT("Hide"));
}

FBHInteractionPromptInfo ABHLocker::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	if (Info.bCanInteract)
	{
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		Info.RiskText = FText::FromString(BHLockerAllowsWarmupSearch(BHGS) ? TEXT("WARMUP") : (Occupant && Occupant != Character ? TEXT("SEARCH") : TEXT("HIDE QUIETLY")));
		return Info;
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		Info.DisabledReason = FText::FromString(TEXT("READY UP FIRST"));
	}
	else if (BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		Info.DisabledReason = FText::FromString(TEXT("OUT OF PLAY"));
	}
	else if (BHPS->IsAliveHunter())
	{
		Info.DisabledReason = Occupant ? FText::FromString(TEXT("USE CAPTURE TO SEARCH")) : FText::FromString(TEXT("TEACHER CANNOT HIDE"));
	}
	else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		Info.DisabledReason = FText::FromString(TEXT("HALL MONITORS CANNOT HIDE"));
	}
	else if (Occupant)
	{
		Info.DisabledReason = FText::FromString(TEXT("OCCUPIED"));
	}
	else
	{
		Info.DisabledReason = FText::FromString(TEXT("LOCKED"));
	}
	return Info;
}

void ABHLocker::ClearOccupant(ABHCharacter* Character)
{
	if (HasAuthority() && Occupant == Character)
	{
		Occupant = nullptr;
		ApplyLockerVisuals();
	}
}

ABHCharacter* ABHLocker::GetOccupant() const
{
	return Occupant;
}

void ABHLocker::OnRep_Occupant()
{
	ApplyLockerVisuals();
}

void ABHLocker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Only matters while occupied (that's when the gated red indicator is shown). Re-apply if the local viewer's
	// hunter status flipped since the last apply (role reassignment) so a survivor-turned-Teacher stops seeing the
	// across-room "occupied" glow that OnRep_Occupant set while they were still a survivor.
	if (Occupant == nullptr)
	{
		return;
	}
	const APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ABHPlayerState* LocalPS = LocalPC ? LocalPC->GetPlayerState<ABHPlayerState>() : nullptr;
	const bool bNowHunter = LocalPS && LocalPS->IsAliveHunter();
	if (bNowHunter != bLastAppliedViewerIsHunter)
	{
		ApplyLockerVisuals();
	}
}

void ABHLocker::ApplyLockerVisuals()
{
	const bool bOccupied = Occupant != nullptr;
	// The bright-red "occupied" indicator must NOT be visible to the alive Teacher: this runs client-side via
	// OnRep_Occupant, and showing the glow to everyone broadcast exactly which locker a hidden survivor was in
	// (the pawn is correctly hidden, but the locker lit up) -- defeating the core hide mechanic across the room.
	// Gate it: survivors and dead/spectating players still see it (teammate convenience), but the hunting Teacher
	// only gets the fair, look-at-it "Hidden From Teacher" interaction label, not a free across-the-room tell.
	const APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ABHPlayerState* LocalPS = LocalPC ? LocalPC->GetPlayerState<ABHPlayerState>() : nullptr;
	const bool bLocalIsAliveHunter = LocalPS && LocalPS->IsAliveHunter();
	bLastAppliedViewerIsHunter = bLocalIsAliveHunter; // so Tick can detect a later role flip and re-apply
	const bool bRevealOccupied = bOccupied && !bLocalIsAliveHunter;

	const FLinearColor DoorColor = bRevealOccupied ? FLinearColor(0.13f, 0.16f, 0.17f, 1.0f) : FLinearColor(0.18f, 0.24f, 0.27f, 1.0f);
	const FLinearColor VentColor(0.018f, 0.020f, 0.022f, 1.0f);
	const FLinearColor HandleColor(0.72f, 0.68f, 0.56f, 1.0f);
	const FLinearColor IndicatorColor = bRevealOccupied ? FLinearColor(0.95f, 0.12f, 0.06f, 1.0f) : FLinearColor(0.10f, 0.36f, 0.26f, 1.0f);

	BHPropVisuals::TintPart(LeftDoorPanel, DoorColor);
	BHPropVisuals::TintPart(RightDoorPanel, DoorColor * 0.85f);
	BHPropVisuals::TintPart(UpperVentLeft, VentColor);
	BHPropVisuals::TintPart(UpperVentRight, VentColor);
	BHPropVisuals::TintPart(LowerVentLeft, VentColor);
	BHPropVisuals::TintPart(LowerVentRight, VentColor);
	BHPropVisuals::TintPart(LockerHandle, HandleColor);
	BHPropVisuals::TintPart(OccupiedIndicator, IndicatorColor, bRevealOccupied ? 2.2f : 0.4f);
}
