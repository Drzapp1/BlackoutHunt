// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTypes.h"
#include "BHPowerupShopTerminal.generated.h"

class UTextRenderComponent;
class ABHPlayerState;

UCLASS()
class BLACKOUTHUNT_API ABHPowerupShopTerminal : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHPowerupShopTerminal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	static bool IsRoleAllowedForPowerup(const ABHPlayerState* PlayerState, EBHPowerupType Type);
	static FString BuildShopItemSummary(EBHPowerupType Type);
	static FString BuildPurchaseStatusText(const ABHPlayerState* PlayerState, EBHPowerupType Type);
	void ConfigureShopItem(EBHPowerupType NewPowerupType);

protected:
	UFUNCTION()
	void OnRep_ShopItem();

	void ApplyShopText();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> LabelText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> DetailText;

	UPROPERTY(ReplicatedUsing = OnRep_ShopItem, EditAnywhere, BlueprintReadOnly, Category = "Shop")
	EBHPowerupType PowerupType;
};
