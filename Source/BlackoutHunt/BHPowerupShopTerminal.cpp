#include "BHPowerupShopTerminal.h"
#include "BHCharacter.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPowerupLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"

ABHPowerupShopTerminal::ABHPowerupShopTerminal()
{
	InteractionLabel = FText::FromString(TEXT("Buy Powerup"));
	PowerupType = EBHPowerupType::StaminaBoost;
	SetActorScale3D(FVector(0.75f, 0.32f, 1.25f));

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelText"));
	LabelText->SetupAttachment(SceneRoot);
	LabelText->SetRelativeLocation(FVector(-58.0f, -58.0f, 78.0f));
	LabelText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	LabelText->SetHorizontalAlignment(EHTA_Left);
	LabelText->SetWorldSize(18.0f);
	LabelText->SetTextRenderColor(FColor(255, 198, 74));

	DetailText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DetailText"));
	DetailText->SetupAttachment(SceneRoot);
	DetailText->SetRelativeLocation(FVector(-58.0f, -59.0f, 43.0f));
	DetailText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	DetailText->SetHorizontalAlignment(EHTA_Left);
	DetailText->SetWorldSize(10.5f);
	DetailText->SetTextRenderColor(FColor(224, 242, 232));
}

void ABHPowerupShopTerminal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHPowerupShopTerminal, PowerupType);
}

bool ABHPowerupShopTerminal::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHPS && BHPS->IsAliveSurvivor() && BHGS
		&& (BHGS->TrainPhase == EBHTrainPhase::Shop || BHGS->TrainPhase == EBHTrainPhase::StationStop);
}

void ABHPowerupShopTerminal::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController());
	FBHPowerupDefinition Definition;
	if (!BHPS || !BHPS->IsAliveSurvivor() || !FBHPowerupLibrary::GetDefinition(PowerupType, Definition))
	{
		return;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || (BHGS->TrainPhase != EBHTrainPhase::Shop && BHGS->TrainPhase != EBHTrainPhase::StationStop))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("Shop terminal locked until the shop carriage opens."), 2.5f);
		}
		return;
	}

	if (BHPS->GetPowerupCharges(PowerupType) >= Definition.MaxCharges)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("%s is already at max charges."), *Definition.Name), 2.5f);
		}
		return;
	}

	if (!BHPS->SpendQuestionPoints(Definition.Cost))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Need %d points for %s. You have %d."), Definition.Cost, *Definition.Name, BHPS->QuestionPoints), 2.75f);
		}
		return;
	}

	BHPS->AddPowerupCharge(PowerupType, Definition.MaxCharges);
	if (PC)
	{
		PC->ClientShowStatusMessage(FString::Printf(TEXT("Purchased %s. Charges: %d/%d."), *Definition.Name, BHPS->GetPowerupCharges(PowerupType), Definition.MaxCharges), 3.0f);
	}
}

FText ABHPowerupShopTerminal::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	FBHPowerupDefinition Definition;
	if (!FBHPowerupLibrary::GetDefinition(PowerupType, Definition))
	{
		return FText::FromString(TEXT("Shop Offline"));
	}
	return FText::FromString(FString::Printf(TEXT("Buy %s (%d pts)"), *Definition.Name, Definition.Cost));
}

void ABHPowerupShopTerminal::ConfigureShopItem(EBHPowerupType NewPowerupType)
{
	PowerupType = NewPowerupType;
	ApplyShopText();
}

void ABHPowerupShopTerminal::OnRep_ShopItem()
{
	ApplyShopText();
}

void ABHPowerupShopTerminal::ApplyShopText()
{
	FBHPowerupDefinition Definition;
	if (!FBHPowerupLibrary::GetDefinition(PowerupType, Definition))
	{
		return;
	}

	if (LabelText)
	{
		LabelText->SetText(FText::FromString(FString::Printf(TEXT("%s\n%d PTS"), *Definition.Name.ToUpper(), Definition.Cost)));
	}
	if (DetailText)
	{
		DetailText->SetText(FText::FromString(FString::Printf(TEXT("%s\nCharges %d  Cooldown %.0fs"), *Definition.Description.Left(110), Definition.MaxCharges, Definition.CooldownSeconds)));
	}
}
