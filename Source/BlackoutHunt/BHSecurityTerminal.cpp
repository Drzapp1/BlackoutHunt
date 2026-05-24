#include "BHSecurityTerminal.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHPlayerState.h"
#include "Components/StaticMeshComponent.h"

ABHSecurityTerminal::ABHSecurityTerminal()
{
	CircuitId = 0;
	TerminalLabel = FText::FromString(TEXT("Open Security Shutters"));
	InteractionLabel = TerminalLabel;
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

void ABHSecurityTerminal::Configure(int32 NewCircuitId, const FText& NewLabel)
{
	CircuitId = NewCircuitId;
	TerminalLabel = NewLabel;
	InteractionLabel = TerminalLabel;
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
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->OpenSecurityCircuit(CircuitId);
		}
	}
}

FText ABHSecurityTerminal::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return TerminalLabel;
}
