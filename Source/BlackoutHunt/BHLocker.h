// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHLocker.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHLocker : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHLocker();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// Low-rate tick (0.5s) used only to re-apply the occupied indicator when the LOCAL viewer's hunter status flips
	// (e.g. a survivor who was seeing occupied lockers is reassigned to Hunter mid-round) -- OnRep_Occupant alone
	// wouldn't catch that, leaving the across-room red tell visible to the now-Teacher.
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void ClearOccupant(ABHCharacter* Character);
	ABHCharacter* GetOccupant() const;

protected:
	UFUNCTION()
	void OnRep_Occupant();

	void ApplyLockerVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftDoorPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightDoorPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> UpperVentLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> UpperVentRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LowerVentLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LowerVentRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LockerHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OccupiedIndicator;

	UPROPERTY(ReplicatedUsing = OnRep_Occupant)
	TObjectPtr<ABHCharacter> Occupant;

	// Cached "the local viewer was an alive Hunter" the last time ApplyLockerVisuals ran, so Tick can detect a flip
	// and re-apply (closing the role-reassignment hole in the occupied-indicator gate).
	bool bLastAppliedViewerIsHunter = false;
};
