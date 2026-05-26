#include "BHLocker.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ABHLocker::ABHLocker()
{
	InteractionLabel = FText::FromString(TEXT("Locker"));
	SetActorScale3D(FVector::OneVector);
	Occupant = nullptr;

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
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		return BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && (Occupant != Character);
	}

	if (BHPS->IsAliveHunter())
	{
		return false;
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
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->NotifySurvivorCaptured(Occupant);
		}
		Occupant = nullptr;
		ApplyLockerVisuals();
		return;
	}

	if (BHPS->IsAliveHunter())
	{
		return;
	}

	if (BHPS->IsAliveSurvivor() && Occupant == nullptr)
	{
		Occupant = Character;
		ApplyLockerVisuals();
		Character->EnterLocker(this);
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

void ABHLocker::ApplyLockerVisuals()
{
	const bool bOccupied = Occupant != nullptr;
	const FLinearColor DoorColor = bOccupied ? FLinearColor(0.13f, 0.16f, 0.17f, 1.0f) : FLinearColor(0.18f, 0.24f, 0.27f, 1.0f);
	const FLinearColor VentColor(0.018f, 0.020f, 0.022f, 1.0f);
	const FLinearColor HandleColor(0.72f, 0.68f, 0.56f, 1.0f);
	const FLinearColor IndicatorColor = bOccupied ? FLinearColor(0.95f, 0.12f, 0.06f, 1.0f) : FLinearColor(0.10f, 0.36f, 0.26f, 1.0f);

	BHPropVisuals::TintPart(LeftDoorPanel, DoorColor);
	BHPropVisuals::TintPart(RightDoorPanel, DoorColor * 0.85f);
	BHPropVisuals::TintPart(UpperVentLeft, VentColor);
	BHPropVisuals::TintPart(UpperVentRight, VentColor);
	BHPropVisuals::TintPart(LowerVentLeft, VentColor);
	BHPropVisuals::TintPart(LowerVentRight, VentColor);
	BHPropVisuals::TintPart(LockerHandle, HandleColor);
	BHPropVisuals::TintPart(OccupiedIndicator, IndicatorColor, bOccupied ? 2.2f : 0.4f);
}
