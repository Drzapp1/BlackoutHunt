#include "BHPowerupShopTerminal.h"
#include "BHCharacter.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPowerupLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
FString BHShopRoleText(bool bTeacherPowerup)
{
	return bTeacherPowerup ? TEXT("Teacher-only") : TEXT("Survivor-only");
}

FString BHShopCurrencyText(bool bTeacherPowerup)
{
	return bTeacherPowerup ? TEXT("capture pts") : TEXT("question pts");
}

FString BHShopEffectText(const FBHPowerupDefinition& Definition, bool bTeacherPowerup)
{
	const FString TimingText = Definition.DurationSeconds > 0.0f
		? FString::Printf(TEXT("Effect %.0fs"), Definition.DurationSeconds)
		: (bTeacherPowerup ? FString(TEXT("Passive upgrade")) : FString(TEXT("Single-use effect")));
	const FString CooldownText = Definition.CooldownSeconds > 0.0f
		? FString::Printf(TEXT("Cooldown %.0fs"), Definition.CooldownSeconds)
		: FString(TEXT("No cooldown"));
	return FString::Printf(TEXT("%s / %s"), *TimingText, *CooldownText);
}
}

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
	return IsRoleAllowedForPowerup(BHPS, PowerupType) && BHGS
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
	if (!BHPS || !FBHPowerupLibrary::GetDefinition(PowerupType, Definition))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("Shop terminal offline. Try another terminal."), 2.5f);
		}
		return;
	}

	const bool bTeacherPowerup = FBHPowerupLibrary::IsTeacherPowerup(PowerupType);
	if (!IsRoleAllowedForPowerup(BHPS, PowerupType))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(BuildPurchaseStatusText(BHPS, PowerupType), 2.75f);
		}
		return;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || (BHGS->TrainPhase != EBHTrainPhase::Shop && BHGS->TrainPhase != EBHTrainPhase::StationStop))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("Shop locked. It opens after the bonus question phase."), 2.5f);
		}
		return;
	}

	const int32 CurrentCharges = BHPS->GetPowerupCharges(PowerupType);
	if (CurrentCharges >= Definition.MaxCharges)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(BuildPurchaseStatusText(BHPS, PowerupType), 2.5f);
		}
		return;
	}

	const bool bSpent = bTeacherPowerup
		? BHPS->SpendHunterPoints(Definition.Cost)
		: BHPS->SpendQuestionPoints(Definition.Cost);
	if (!bSpent)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(BuildPurchaseStatusText(BHPS, PowerupType), 2.75f);
		}
		return;
	}

	BHPS->AddPowerupCharge(PowerupType, Definition.MaxCharges);
	if (PC)
	{
		PC->ClientShowStatusMessage(FString::Printf(TEXT("Purchased %s for %d %s. Charges: %d/%d. Balance: %d. %s."),
			*Definition.Name,
			Definition.Cost,
			*BHShopCurrencyText(bTeacherPowerup),
			BHPS->GetPowerupCharges(PowerupType),
			Definition.MaxCharges,
			bTeacherPowerup ? BHPS->HunterPoints : BHPS->QuestionPoints,
			*BHShopEffectText(Definition, bTeacherPowerup)), 3.25f);
	}
}

FText ABHPowerupShopTerminal::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	FBHPowerupDefinition Definition;
	if (!FBHPowerupLibrary::GetDefinition(PowerupType, Definition))
	{
		return FText::FromString(TEXT("Shop Offline"));
	}
	const bool bTeacherPowerup = FBHPowerupLibrary::IsTeacherPowerup(PowerupType);
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || (BHGS->TrainPhase != EBHTrainPhase::Shop && BHGS->TrainPhase != EBHTrainPhase::StationStop))
	{
		return FText::FromString(FString::Printf(TEXT("Shop Locked - %s"), *Definition.Name));
	}
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!IsRoleAllowedForPowerup(BHPS, PowerupType))
	{
		return FText::FromString(FString::Printf(TEXT("%s: %s"), *BHShopRoleText(bTeacherPowerup), *Definition.Name));
	}
	return FText::FromString(FString::Printf(TEXT("Buy %s (%d %s)"), *Definition.Name, Definition.Cost, *BHShopCurrencyText(bTeacherPowerup)));
}

FBHInteractionPromptInfo ABHPowerupShopTerminal::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.25f;

	FBHPowerupDefinition Definition;
	if (!FBHPowerupLibrary::GetDefinition(PowerupType, Definition))
	{
		Info.DisabledReason = FText::FromString(TEXT("SHOP OFFLINE"));
		return Info;
	}

	const bool bTeacherPowerup = FBHPowerupLibrary::IsTeacherPowerup(PowerupType);
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const int32 CurrentPoints = BHPS ? (bTeacherPowerup ? BHPS->HunterPoints : BHPS->QuestionPoints) : 0;
	const int32 CurrentCharges = BHPS ? BHPS->GetPowerupCharges(PowerupType) : 0;
	if (Info.bCanInteract)
	{
		if (CurrentCharges >= Definition.MaxCharges)
		{
			Info.RiskText = FText::FromString(FString::Printf(TEXT("FULL / CHARGES %d/%d / YOU %d"), CurrentCharges, Definition.MaxCharges, CurrentPoints));
		}
		else if (CurrentPoints < Definition.Cost)
		{
			Info.RiskText = FText::FromString(FString::Printf(TEXT("NEED %d MORE / YOU %d / CHARGES %d/%d"),
				Definition.Cost - CurrentPoints,
				CurrentPoints,
				CurrentCharges,
				Definition.MaxCharges));
		}
		else
		{
			Info.RiskText = FText::FromString(FString::Printf(TEXT("COST %d / YOU %d / CHARGES %d/%d"),
				Definition.Cost,
				CurrentPoints,
				CurrentCharges,
				Definition.MaxCharges));
		}
		return Info;
	}

	if (!BHGS || (BHGS->TrainPhase != EBHTrainPhase::Shop && BHGS->TrainPhase != EBHTrainPhase::StationStop))
	{
		Info.DisabledReason = FText::FromString(TEXT("SHOP OPENS LATER"));
	}
	else if (!BHPS)
	{
		Info.DisabledReason = FText::FromString(TEXT("NO PLAYER STATE"));
	}
	else if (!IsRoleAllowedForPowerup(BHPS, PowerupType))
	{
		Info.DisabledReason = FText::FromString(BHShopRoleText(bTeacherPowerup).ToUpper());
	}
	else
	{
		Info.DisabledReason = FText::FromString(TEXT("SHOP LOCKED"));
	}
	return Info;
}

bool ABHPowerupShopTerminal::IsRoleAllowedForPowerup(const ABHPlayerState* PlayerState, EBHPowerupType Type)
{
	if (!PlayerState)
	{
		return false;
	}

	return FBHPowerupLibrary::IsTeacherPowerup(Type)
		? PlayerState->IsAliveHunter()
		: PlayerState->IsAliveSurvivor();
}

FString ABHPowerupShopTerminal::BuildShopItemSummary(EBHPowerupType Type)
{
	FBHPowerupDefinition Definition;
	if (!FBHPowerupLibrary::GetDefinition(Type, Definition))
	{
		return TEXT("Shop item offline.");
	}

	const bool bTeacherPowerup = FBHPowerupLibrary::IsTeacherPowerup(Type);
	return FString::Printf(TEXT("%s\nCost %d %s / Max charges %d\n%s\nPurchase shows balance and failure reason."),
		*Definition.Description.Left(110),
		Definition.Cost,
		*BHShopCurrencyText(bTeacherPowerup),
		Definition.MaxCharges,
		*BHShopEffectText(Definition, bTeacherPowerup));
}

FString ABHPowerupShopTerminal::BuildPurchaseStatusText(const ABHPlayerState* PlayerState, EBHPowerupType Type)
{
	FBHPowerupDefinition Definition;
	if (!FBHPowerupLibrary::GetDefinition(Type, Definition))
	{
		return TEXT("Shop item offline.");
	}

	const bool bTeacherPowerup = FBHPowerupLibrary::IsTeacherPowerup(Type);
	const FString RoleText = BHShopRoleText(bTeacherPowerup);
	const FString CurrencyText = BHShopCurrencyText(bTeacherPowerup);
	if (!PlayerState)
	{
		return FString::Printf(TEXT("%s item. Cost %d %s. Player state not ready."),
			*RoleText,
			Definition.Cost,
			*CurrencyText);
	}
	if (!IsRoleAllowedForPowerup(PlayerState, Type))
	{
		return FString::Printf(TEXT("%s: %s. Use a terminal for your current role."),
			*RoleText,
			*Definition.Name);
	}

	const int32 Balance = bTeacherPowerup ? PlayerState->HunterPoints : PlayerState->QuestionPoints;
	const int32 CurrentCharges = PlayerState->GetPowerupCharges(Type);
	if (CurrentCharges >= Definition.MaxCharges)
	{
		return FString::Printf(TEXT("%s already full. Charges %d/%d. Balance %d %s."),
			*Definition.Name,
			CurrentCharges,
			Definition.MaxCharges,
			Balance,
			*CurrencyText);
	}
	if (Balance < Definition.Cost)
	{
		return FString::Printf(TEXT("Need %d more %s for %s. Cost %d, you have %d. Charges %d/%d."),
			Definition.Cost - Balance,
			*CurrencyText,
			*Definition.Name,
			Definition.Cost,
			Balance,
			CurrentCharges,
			Definition.MaxCharges);
	}

	return FString::Printf(TEXT("%s ready. Cost %d %s, you have %d. Buy for charge %d/%d."),
		*Definition.Name,
		Definition.Cost,
		*CurrencyText,
		Balance,
		FMath::Min(CurrentCharges + 1, Definition.MaxCharges),
		Definition.MaxCharges);
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
		LabelText->SetText(FText::FromString(FString::Printf(TEXT("%s\n%d %s\n%s"),
			*Definition.Name.ToUpper(),
			Definition.Cost,
			*BHShopCurrencyText(FBHPowerupLibrary::IsTeacherPowerup(PowerupType)).ToUpper(),
			*BHShopRoleText(FBHPowerupLibrary::IsTeacherPowerup(PowerupType)).ToUpper())));
	}
	if (DetailText)
	{
		DetailText->SetText(FText::FromString(BuildShopItemSummary(PowerupType)));
	}
}
