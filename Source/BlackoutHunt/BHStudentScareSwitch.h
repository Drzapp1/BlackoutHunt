// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHStudentScareSwitch.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHStudentScareSwitch : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHStudentScareSwitch();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Configure(int32 NewSeverity, const FString& NewScareTitle, float NewCooldownSeconds);

	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	UFUNCTION()
	void OnRep_ScareState();

	void ApplyScareVisuals();
	float GetRemainingCooldownSeconds() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SwitchPlate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SwitchLever;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SeverityLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CooldownLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TitleBadge;

	UPROPERTY(ReplicatedUsing = OnRep_ScareState, EditAnywhere, Category = "Scare")
	int32 Severity;

	UPROPERTY(ReplicatedUsing = OnRep_ScareState, EditAnywhere, Category = "Scare")
	FString ScareTitle;

	UPROPERTY(ReplicatedUsing = OnRep_ScareState, EditAnywhere, Category = "Scare")
	float CooldownEndServerTime;

	UPROPERTY(Replicated, EditAnywhere, Category = "Scare")
	float CooldownSeconds;
};
