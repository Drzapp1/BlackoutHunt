#include "BHSecurityTerminal.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHSecurityShutter.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

ABHSecurityTerminal::ABHSecurityTerminal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	CircuitId = 0;
	TerminalLabel = FText::FromString(TEXT("Toggle Security Shutters"));
	InteractionLabel = TerminalLabel;
	bHasCachedCircuitState = false;
	bCachedCircuitWillOpen = true;
	SetActorScale3D(FVector::OneVector);

	TerminalScreen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalScreen"));
	TerminalScreen->SetupAttachment(RootComponent);
	KeypadBackplate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeypadBackplate"));
	KeypadBackplate->SetupAttachment(RootComponent);
	AccessLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccessLight"));
	AccessLight->SetupAttachment(RootComponent);

	for (int32 Index = 0; Index < 9; ++Index)
	{
		UStaticMeshComponent* Button = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("KeypadButton%d"), Index));
		Button->SetupAttachment(RootComponent);
		KeypadButtons.Add(Button);
	}

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.76f, 0.24f, 0.90f), true);
	BHPropVisuals::ConfigurePart(TerminalScreen, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(40.0f, 0.0f, 18.0f), FRotator::ZeroRotator, FVector(0.022f, 0.17f, 0.24f));
	BHPropVisuals::ConfigurePart(KeypadBackplate, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(40.5f, 0.0f, -22.0f), FRotator::ZeroRotator, FVector(0.018f, 0.18f, 0.20f));
	BHPropVisuals::ConfigurePart(AccessLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(42.0f, -17.0f, 40.0f), FRotator::ZeroRotator, FVector(0.050f));

	for (int32 Index = 0; Index < KeypadButtons.Num(); ++Index)
	{
		const int32 Row = Index / 3;
		const int32 Column = Index % 3;
		const FVector Location(43.0f, -10.0f + Column * 10.0f, -32.0f + Row * 10.0f);
		BHPropVisuals::ConfigurePart(KeypadButtons[Index], BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), Location, FRotator::ZeroRotator, FVector(0.016f, 0.036f, 0.036f));
		BHPropVisuals::TintPart(KeypadButtons[Index], FLinearColor(0.05f, 0.06f, 0.065f, 1.0f));
	}

	BHPropVisuals::TintPart(TerminalScreen, FLinearColor(0.08f, 0.78f, 0.92f, 1.0f), 2.2f);
	BHPropVisuals::TintPart(KeypadBackplate, FLinearColor(0.018f, 0.022f, 0.024f, 1.0f));
	BHPropVisuals::TintPart(AccessLight, FLinearColor(0.18f, 0.98f, 0.52f, 1.0f), 2.0f);
}

void ABHSecurityTerminal::BeginPlay()
{
	Super::BeginPlay();
	RefreshCircuitVisuals();
}

void ABHSecurityTerminal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshCircuitVisuals();
}

void ABHSecurityTerminal::Configure(int32 NewCircuitId, const FText& NewLabel)
{
	CircuitId = NewCircuitId;
	TerminalLabel = NewLabel;
	InteractionLabel = TerminalLabel;
	bHasCachedCircuitState = false;
}

bool ABHSecurityTerminal::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHSecurityTerminal::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority())
	{
		if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
		{
			if (!BHGM->ToggleSecurityCircuit(CircuitId) && Character)
			{
				if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
				{
					PC->ClientShowStatusMessage(TEXT("Security shutter blocked."), 2.5f);
				}
			}
		}
	}
	RefreshCircuitVisuals();
}

FText ABHSecurityTerminal::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const bool bWillOpen = CircuitHasClosedShutter();
	if (TerminalLabel.IsEmpty())
	{
		return bWillOpen
			? FText::FromString(TEXT("Open Security Shutters"))
			: FText::FromString(TEXT("Close Security Shutters"));
	}

	FString LabelText = TerminalLabel.ToString();
	LabelText.RemoveFromStart(TEXT("Toggle "), ESearchCase::IgnoreCase);
	return FText::FromString(FString::Printf(TEXT("%s %s"),
		bWillOpen ? TEXT("Open") : TEXT("Close"),
		*LabelText));
}

bool ABHSecurityTerminal::CircuitHasClosedShutter() const
{
	if (!GetWorld() || CircuitId <= 0)
	{
		return true;
	}

	bool bFoundCircuitShutter = false;
	for (TActorIterator<ABHSecurityShutter> It(GetWorld()); It; ++It)
	{
		const ABHSecurityShutter* Shutter = *It;
		if (!Shutter || Shutter->GetCircuitId() != CircuitId)
		{
			continue;
		}

		bFoundCircuitShutter = true;
		if (!Shutter->IsOpen())
		{
			return true;
		}
	}

	return !bFoundCircuitShutter;
}

void ABHSecurityTerminal::RefreshCircuitVisuals()
{
	const bool bWillOpen = CircuitHasClosedShutter();
	if (bHasCachedCircuitState && bCachedCircuitWillOpen == bWillOpen)
	{
		return;
	}

	bHasCachedCircuitState = true;
	bCachedCircuitWillOpen = bWillOpen;
	BHPropVisuals::TintPart(AccessLight,
		bWillOpen ? FLinearColor(0.18f, 0.98f, 0.52f, 1.0f) : FLinearColor(0.98f, 0.55f, 0.12f, 1.0f),
		bWillOpen ? 2.0f : 1.7f);
	BHPropVisuals::TintPart(TerminalScreen,
		bWillOpen ? FLinearColor(0.08f, 0.78f, 0.92f, 1.0f) : FLinearColor(0.95f, 0.48f, 0.12f, 1.0f),
		2.2f);
}
