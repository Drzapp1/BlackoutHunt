// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainSlotMachine.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

// A single-player slot machine for the train lobby. TAP to spin three reels for cosmetic credits (free re-buy
// so nobody gets stuck). The machine face shows the reels for everyone; the spinner sees their credits + result
// on their HUD and a toast. Pure fun, no effect on the round.
UCLASS()
class BLACKOUTHUNT_API ABHTrainSlotMachine : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainSlotMachine();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const;

protected:
	UFUNCTION()
	void OnRep_State();

	float GetServerTimeSeconds() const;
	void Spin(ABHCharacter* Character);
	static FString SymbolGlyph(int32 Symbol);
	static int32 EvaluatePayout(int32 A, int32 B, int32 C, int32 Bet);

	void ApplyVisuals();
	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Screen;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Lever;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TitleText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> ReelText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MachineLight;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	TArray<int32> Reels;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	int32 ActiveSpinnerId = -1;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	int32 ActiveCredits = 0;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FString LastResultText;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FLinearColor LastResultAccent = FLinearColor(0.95f, 0.85f, 0.4f, 1.0f);

	// Server-only: per-player credit balances (cosmetic).
	TMap<int32, int32> CreditsByPlayer;
};
