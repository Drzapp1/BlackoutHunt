#include "BHBatteryPickup.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ABHBatteryPickup::ABHBatteryPickup()
{
	InteractionLabel = FText::FromString(TEXT("Take Battery"));
	bConsumed = false;
	RefillAmount = FMath::Max(1.0f, GetDefault<UBHGameSettings>()->BatteryRefillAmount);
	SetActorScale3D(FVector::OneVector);

	PositiveCap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PositiveCap"));
	PositiveCap->SetupAttachment(RootComponent);
	NegativeCap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NegativeCap"));
	NegativeCap->SetupAttachment(RootComponent);
	PositiveTerminal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PositiveTerminal"));
	PositiveTerminal->SetupAttachment(RootComponent);
	LabelBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LabelBand"));
	LabelBand->SetupAttachment(RootComponent);
	ChargeLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargeLight"));
	ChargeLight->SetupAttachment(RootComponent);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f), FVector(0.22f, 0.22f, 0.58f), true);
	BHPropVisuals::ConfigurePart(PositiveCap, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(32.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.235f, 0.235f, 0.055f));
	BHPropVisuals::ConfigurePart(NegativeCap, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(-32.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.235f, 0.235f, 0.055f));
	BHPropVisuals::ConfigurePart(PositiveTerminal, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(40.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.105f, 0.105f, 0.055f));
	BHPropVisuals::ConfigurePart(LabelBand, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -20.5f, 8.0f), FRotator::ZeroRotator, FVector(0.48f, 0.018f, 0.12f));
	BHPropVisuals::ConfigurePart(ChargeLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(-18.0f, -22.0f, 7.5f), FRotator::ZeroRotator, FVector(0.055f));
	ApplyBatteryVisuals();
}

void ABHBatteryPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHBatteryPickup, bConsumed);
}

bool ABHBatteryPickup::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	return !bConsumed && BHPS && BHPS->IsAliveSurvivor();
}

void ABHBatteryPickup::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !CanInteract_Implementation(Character))
	{
		return;
	}

	Character->RefillFlashlight(RefillAmount);
	Character->RecoverStamina(32.0f);
	Character->AddFear(-18.0f);
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->RecordPlaytestTelemetryMarker(TEXT("battery_pickup"), GetActorLocation(), FString::Printf(TEXT("refill=%.0f"), RefillAmount), Character->GetBHPlayerState());
	}
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(TEXT("Battery recovered flashlight, stamina, and nerve."), 2.75f);
	}
	bConsumed = true;
	ApplyConsumedState();
}

FText ABHBatteryPickup::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!bConsumed && (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned))
	{
		return FText::FromString(TEXT("Ready Up First"));
	}

	if (!bConsumed && BHPS && !BHPS->IsAliveSurvivor())
	{
		return FText::FromString(TEXT("Survivor Battery"));
	}

	return bConsumed ? FText::FromString(TEXT("Battery Empty")) : FText::FromString(TEXT("Take Battery"));
}

void ABHBatteryPickup::OnRep_Consumed()
{
	ApplyConsumedState();
}

void ABHBatteryPickup::ApplyConsumedState()
{
	ApplyBatteryVisuals();
	SetActorHiddenInGame(bConsumed);
	SetActorEnableCollision(!bConsumed);
}

void ABHBatteryPickup::ApplyBatteryVisuals()
{
	const FLinearColor BodyColor = bConsumed ? FLinearColor(0.10f, 0.10f, 0.10f, 1.0f) : FLinearColor(0.12f, 0.34f, 0.18f, 1.0f);
	const FLinearColor CapColor = bConsumed ? FLinearColor(0.05f, 0.05f, 0.05f, 1.0f) : FLinearColor(0.78f, 0.70f, 0.52f, 1.0f);
	const FLinearColor LabelColor = bConsumed ? FLinearColor(0.08f, 0.08f, 0.08f, 1.0f) : FLinearColor(0.94f, 0.78f, 0.16f, 1.0f);
	const FLinearColor LightColor = bConsumed ? FLinearColor(0.02f, 0.02f, 0.02f, 1.0f) : FLinearColor(0.38f, 0.95f, 0.78f, 1.0f);

	BHPropVisuals::TintPart(Mesh, BodyColor);
	BHPropVisuals::TintPart(PositiveCap, CapColor);
	BHPropVisuals::TintPart(NegativeCap, CapColor * 0.62f);
	BHPropVisuals::TintPart(PositiveTerminal, FLinearColor(0.92f, 0.86f, 0.62f, 1.0f));
	BHPropVisuals::TintPart(LabelBand, LabelColor);
	BHPropVisuals::TintPart(ChargeLight, LightColor, bConsumed ? 0.0f : 2.4f);
}
