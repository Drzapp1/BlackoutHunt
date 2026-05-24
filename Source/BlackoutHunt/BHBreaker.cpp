#include "BHBreaker.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

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
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		Repairers.Empty();
		SetActorTickEnabled(false);
		return;
	}

	for (auto It = Repairers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
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
	return bDirectorActive && BHPS && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && BHPS->IsAliveSurvivor() && !bRepaired;
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
			if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
			{
				BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("breaker repair"));
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
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return FText::FromString(TEXT("Repair During Hunt"));
	}

	return FText::FromString(FString::Printf(TEXT("Repair Breaker %d%%"), FMath::RoundToInt(RepairProgress * 100.0f)));
}

void ABHBreaker::SetDirectorActive(bool bNewActive)
{
	bDirectorActive = bNewActive;
	if (!bDirectorActive)
	{
		Repairers.Empty();
		RepairProgress = 0.0f;
		SetActorTickEnabled(false);
	}
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

void ABHBreaker::CompleteRepair()
{
	bRepaired = true;
	RepairProgress = 1.0f;
	Repairers.Empty();
	SetActorTickEnabled(false);
	ApplyBreakerVisuals();

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifyBreakerRepaired(GetActorLocation());
	}
}
