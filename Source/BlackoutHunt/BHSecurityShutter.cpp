// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHSecurityShutter.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ABHSecurityShutter::ABHSecurityShutter()
{
	PrimaryActorTick.bCanEverTick = true;

	bOpen = false;
	CircuitId = 0;
	OpenLiftHeight = 150.0f;
	OpenInterpSpeed = 9.0f;
	ClosedMeshLocation = FVector::ZeroVector;
	InteractionLabel = FText::FromString(TEXT("Security Shutter"));
	SetActorScale3D(FVector(0.18f, 2.4f, 2.6f));

	LeftRail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftRail"));
	LeftRail->SetupAttachment(RootComponent);
	RightRail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightRail"));
	RightRail->SetupAttachment(RootComponent);
	HeaderPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeaderPanel"));
	HeaderPanel->SetupAttachment(RootComponent);
	WarningStripe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripe"));
	WarningStripe->SetupAttachment(Mesh);
	StatusLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(RootComponent);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::RustedMetalMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, true);
	BHPropVisuals::ConfigurePart(LeftRail, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, -55.0f, 0.0f), FRotator::ZeroRotator, FVector(1.35f, 0.055f, 1.08f));
	BHPropVisuals::ConfigurePart(RightRail, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 55.0f, 0.0f), FRotator::ZeroRotator, FVector(1.35f, 0.055f, 1.08f));
	BHPropVisuals::ConfigurePart(HeaderPanel, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 58.0f), FRotator::ZeroRotator, FVector(1.28f, 1.10f, 0.10f));
	BHPropVisuals::ConfigurePart(WarningStripe, BHPropVisuals::CubeMesh(), BHPropVisuals::WarningSignMaterial(), FVector(52.0f, 0.0f, -22.0f), FRotator::ZeroRotator, FVector(0.026f, 0.95f, 0.070f));
	BHPropVisuals::ConfigurePart(StatusLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(58.0f, -45.0f, 47.0f), FRotator::ZeroRotator, FVector(0.075f));
}

void ABHSecurityShutter::BeginPlay()
{
	Super::BeginPlay();
	if (Mesh)
	{
		ClosedMeshLocation = Mesh->GetRelativeLocation();
	}
	ApplyOpenState(true);
}

void ABHSecurityShutter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Mesh)
	{
		return;
	}

	const FVector TargetLocation = GetTargetMeshLocation();
	if (Mesh->GetRelativeLocation().Equals(TargetLocation, 0.25f))
	{
		Mesh->SetRelativeLocation(TargetLocation);
		return;
	}

	Mesh->SetRelativeLocation(FMath::VInterpTo(Mesh->GetRelativeLocation(), TargetLocation, DeltaSeconds, OpenInterpSpeed));
}

void ABHSecurityShutter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHSecurityShutter, bOpen);
	DOREPLIFETIME(ABHSecurityShutter, CircuitId);
}

void ABHSecurityShutter::Configure(int32 NewCircuitId)
{
	CircuitId = NewCircuitId;
}

void ABHSecurityShutter::SetOpen(bool bNewOpen)
{
	TrySetOpen(bNewOpen);
}

bool ABHSecurityShutter::TrySetOpen(bool bNewOpen)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (bOpen == bNewOpen)
	{
		return true;
	}

	if (!bNewOpen && bOpen && !CanCloseWithoutBlockingPawn())
	{
		return false;
	}

	bOpen = bNewOpen;
	ApplyOpenState();
	ForceNetUpdate();
	return true;
}

int32 ABHSecurityShutter::GetCircuitId() const
{
	return CircuitId;
}

bool ABHSecurityShutter::IsOpen() const
{
	return bOpen;
}

bool ABHSecurityShutter::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}
	// In-round control only: no toggling shutters / blinding the CCTV circuit in the lobby or on results.
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHGS
		&& BHGS->RoundPhase != EBHRoundPhase::Lobby
		&& BHGS->RoundPhase != EBHRoundPhase::SurvivorsWin
		&& BHGS->RoundPhase != EBHRoundPhase::HunterWin;
}

void ABHSecurityShutter::BeginInteract_Implementation(ABHCharacter* Character)
{
	// Anti-grief: require alive + in-round (CanInteract) AND throttle to one toggle/sec per shutter,
	// mirroring its sibling terminal — a circuit-wired shutter toggles the SAME circuit-wide state the
	// terminal does, so leaving this direct path ungated would just move the spam one prop over.
	const float NowToggleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (!HasAuthority() || !CanInteract_Implementation(Character) || (NowToggleTime - LastToggleServerTime < 1.0f))
	{
		return;
	}
	LastToggleServerTime = NowToggleTime;

	if (CircuitId > 0)
	{
		if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
		{
			const bool bOpeningCircuit = !IsOpen();
			const bool bToggled = BHGM->ToggleSecurityCircuit(CircuitId);
			if (Character)
			{
				if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
				{
					if (bToggled)
					{
						PC->ClientShowStatusMessage(bOpeningCircuit
							? TEXT("Security shutters open. CCTV circuit blinded.")
							: TEXT("Security shutters closed. CCTV circuit rearmed."),
							2.8f);
					}
					else
					{
						PC->ClientShowStatusMessage(TEXT("Security shutter blocked."), 2.5f);
					}
				}
			}
		}
		return;
	}

	if (!TrySetOpen(!bOpen) && Character)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(TEXT("Security shutter blocked."), 2.5f);
		}
	}
}

FText ABHSecurityShutter::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return bOpen
		? FText::FromString(TEXT("Close Security Shutter"))
		: FText::FromString(TEXT("Open Security Shutter"));
}

void ABHSecurityShutter::OnRep_Open()
{
	ApplyOpenState();
}

void ABHSecurityShutter::ApplyOpenState(bool bSnapMesh)
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (bSnapMesh && Mesh)
	{
		Mesh->SetRelativeLocation(GetTargetMeshLocation());
	}
	ApplyCollisionState();
	ApplyStatusVisuals();
}

FVector ABHSecurityShutter::GetTargetMeshLocation() const
{
	return ClosedMeshLocation + FVector(0.0f, 0.0f, bOpen ? OpenLiftHeight : 0.0f);
}

bool ABHSecurityShutter::CanCloseWithoutBlockingPawn() const
{
	if (!GetWorld() || !Mesh)
	{
		return true;
	}

	const FVector ClosedCenter = GetActorTransform().TransformPosition(ClosedMeshLocation);
	const FQuat ActorRotation = GetActorQuat();
	const FVector MeshHalfExtent = Mesh->GetComponentScale().GetAbs() * 50.0f;

	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		const ABHCharacter* Character = *It;
		const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
		if (!Character || !BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 42.0f;
		const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.0f;
		const FVector LocalDelta = ActorRotation.UnrotateVector(Character->GetActorLocation() - ClosedCenter);
		const FVector ExpandedExtent = MeshHalfExtent + FVector(CapsuleRadius + 18.0f, CapsuleRadius + 18.0f, CapsuleHalfHeight);
		if (FMath::Abs(LocalDelta.X) <= ExpandedExtent.X
			&& FMath::Abs(LocalDelta.Y) <= ExpandedExtent.Y
			&& FMath::Abs(LocalDelta.Z) <= ExpandedExtent.Z)
		{
			return false;
		}
	}

	return true;
}

void ABHSecurityShutter::ApplyCollisionState()
{
	if (!Mesh)
	{
		return;
	}

	if (bOpen)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	else
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	}
}

void ABHSecurityShutter::ApplyStatusVisuals()
{
	BHPropVisuals::TintPart(StatusLight,
		bOpen ? FLinearColor(0.12f, 0.95f, 0.45f, 1.0f) : FLinearColor(0.95f, 0.12f, 0.055f, 1.0f),
		bOpen ? 2.5f : 2.0f);
	BHPropVisuals::TintPart(WarningStripe,
		bOpen ? FLinearColor(0.18f, 0.82f, 0.48f, 1.0f) : FLinearColor(0.95f, 0.64f, 0.08f, 1.0f),
		bOpen ? 0.65f : 0.25f);
}
