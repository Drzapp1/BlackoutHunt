// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainCat.generated.h"

class UStaticMeshComponent;

// A low-poly train cat that wanders the lobby and can be petted (interact). Pure comfort/charm: it ambles
// between random spots near its home, sits when you pet it, and purrs. Server-authoritative wander with
// replicated movement so everyone sees the same cat.
UCLASS()
class BLACKOUTHUNT_API ABHTrainCat : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainCat();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

protected:
	UFUNCTION()
	void OnRep_Coat();

	void PickNewTarget();
	void ApplyCatVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EarL;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EarR;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Tail;

	// 0 orange tabby, 1 black, 2 grey, 3 white. Replicated so all clients see the same coat.
	UPROPERTY(ReplicatedUsing = OnRep_Coat)
	uint8 CoatIndex = 0;

	UPROPERTY(Replicated)
	int32 PetCount = 0;

	// Server-only wander state.
	FVector HomeLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	float IdleTimer = 0.0f;
	float SitTimer = 0.0f;
	bool bHomeSet = false;
};
