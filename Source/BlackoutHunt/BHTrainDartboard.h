// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainDartboard.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

// A look-to-aim darts board for the train lobby. TAP to throw a dart where you're aiming (with a little spread,
// so it rewards a steady aim but isn't pixel-perfect). Concentric rings score 50/25/20/15/10/5/1. Per-player
// running totals show on the HUD; recent darts stick in the board. Pure casual skill game.
UCLASS()
class BLACKOUTHUNT_API ABHTrainDartboard : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainDartboard();

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
	void ThrowDart(ABHCharacter* Character, bool bHold);
	static int32 ScoreForRadius(float RadiusCm);

	void ApplyVisuals();
	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Stand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Rings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> DartMarkers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TitleText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> StatusTextRender;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> BoardLight;

	// Recent dart impact points in board-local space (X = horizontal, Z = vertical); newest last.
	UPROPERTY(ReplicatedUsing = OnRep_State)
	TArray<FVector> RecentDarts;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	int32 ActiveThrowerId = -1;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	int32 ActiveTotal = 0;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	int32 ActiveThrows = 0;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	int32 LastScore = 0;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FString ActiveThrowerName;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FLinearColor LastResultAccent = FLinearColor(0.55f, 0.78f, 1.0f, 1.0f);

	// Server-only per-player tallies.
	TMap<int32, int32> TotalByPlayer;
	TMap<int32, int32> ThrowsByPlayer;
};
