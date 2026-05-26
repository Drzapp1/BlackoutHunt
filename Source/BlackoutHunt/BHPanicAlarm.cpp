#include "BHPanicAlarm.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ABHPanicAlarm::ABHPanicAlarm()
{
	InteractionLabel = FText::FromString(TEXT("Pull Alarm"));
	bUsed = false;
	SetActorScale3D(FVector::OneVector);

	AlarmBell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AlarmBell"));
	AlarmBell->SetupAttachment(RootComponent);
	PullHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PullHandle"));
	PullHandle->SetupAttachment(RootComponent);
	AlarmLabel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AlarmLabel"));
	AlarmLabel->SetupAttachment(RootComponent);
	AlarmLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AlarmLight"));
	AlarmLight->SetupAttachment(RootComponent);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.45f, 0.16f, 0.72f), true);
	BHPropVisuals::ConfigurePart(AlarmBell, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(30.0f, 0.0f, 22.0f), FRotator::ZeroRotator, FVector(0.17f, 0.12f, 0.17f));
	BHPropVisuals::ConfigurePart(PullHandle, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(30.0f, 0.0f, -23.0f), FRotator(0.0f, 0.0f, -10.0f), FVector(0.035f, 0.16f, 0.075f));
	BHPropVisuals::ConfigurePart(AlarmLabel, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(30.5f, 0.0f, 3.0f), FRotator::ZeroRotator, FVector(0.018f, 0.13f, 0.09f));
	BHPropVisuals::ConfigurePart(AlarmLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(31.0f, -10.5f, 37.0f), FRotator::ZeroRotator, FVector(0.045f));
	ApplyAlarmVisuals();
}

void ABHPanicAlarm::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHPanicAlarm, bUsed);
}

bool ABHPanicAlarm::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return !bUsed && BHPS && BHPS->IsAliveSurvivor() && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt;
}

void ABHPanicAlarm::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !CanInteract_Implementation(Character))
	{
		return;
	}

	bUsed = true;
	ApplyAlarmVisuals();

	Character->AddFear(12.0f);
	Character->AddDread(18.0f);
	if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
	{
		PC->ClientShowStatusMessage(TEXT("Alarm pulled. The Teacher heard it."), 2.75f);
	}

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("pulled alarm"));
	}
}

FText ABHPanicAlarm::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	if (bUsed)
	{
		return FText::FromString(TEXT("Alarm Spent"));
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHPS || !BHPS->IsAliveSurvivor())
	{
		return FText::FromString(TEXT("Survivor Alarm"));
	}
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return FText::FromString(TEXT("Alarm During Hunt"));
	}

	return FText::FromString(TEXT("Pull Alarm"));
}

void ABHPanicAlarm::OnRep_Used()
{
	ApplyAlarmVisuals();
}

void ABHPanicAlarm::ApplyAlarmVisuals()
{
	if (!Mesh)
	{
		return;
	}

	const FLinearColor BodyColor = bUsed ? FLinearColor(0.10f, 0.08f, 0.08f, 1.0f) : FLinearColor(0.78f, 0.035f, 0.025f, 1.0f);
	const FLinearColor BellColor = bUsed ? FLinearColor(0.05f, 0.04f, 0.04f, 1.0f) : FLinearColor(0.92f, 0.08f, 0.06f, 1.0f);
	const FLinearColor HandleColor = bUsed ? FLinearColor(0.16f, 0.14f, 0.13f, 1.0f) : FLinearColor(0.96f, 0.74f, 0.18f, 1.0f);
	const FLinearColor LightColor = bUsed ? FLinearColor(0.03f, 0.02f, 0.02f, 1.0f) : FLinearColor(1.0f, 0.12f, 0.06f, 1.0f);

	if (PullHandle)
	{
		PullHandle->SetRelativeLocation(bUsed ? FVector(31.0f, 0.0f, -33.0f) : FVector(30.0f, 0.0f, -23.0f));
		PullHandle->SetRelativeRotation(bUsed ? FRotator(0.0f, 0.0f, 18.0f) : FRotator(0.0f, 0.0f, -10.0f));
	}

	BHPropVisuals::TintPart(Mesh, BodyColor);
	BHPropVisuals::TintPart(AlarmBell, BellColor);
	BHPropVisuals::TintPart(PullHandle, HandleColor);
	BHPropVisuals::TintPart(AlarmLabel, FLinearColor(0.98f, 0.90f, 0.72f, 1.0f));
	BHPropVisuals::TintPart(AlarmLight, LightColor, bUsed ? 0.0f : 2.5f);
}
