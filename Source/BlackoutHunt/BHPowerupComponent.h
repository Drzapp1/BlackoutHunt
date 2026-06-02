// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "Components/ActorComponent.h"
#include "BHPowerupComponent.generated.h"

class ABHCharacter;

UCLASS(ClassGroup = (BlackoutHunt), meta = (BlueprintSpawnableComponent))
class BLACKOUTHUNT_API UBHPowerupComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBHPowerupComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool ServerUsePowerup(EBHPowerupType Type);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Powerups")
	float GetSprintSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Powerups")
	float GetStaminaRecoveryMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Powerups")
	float GetStaminaDrainMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Powerups")
	float GetFlashlightMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Powerups")
	bool HasActiveQuestionHint() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Powerups")
	bool HasActiveDoorRush() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Powerups")
	float GetQuestionHintEndServerTime() const;

private:
	float GetServerTime() const;
	ABHCharacter* GetBHOwner() const;
	bool IsActiveUntil(float EndServerTime) const;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Powerups", meta = (AllowPrivateAccess = "true"))
	float StaminaBoostEndServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Powerups", meta = (AllowPrivateAccess = "true"))
	float SprintBurstEndServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Powerups", meta = (AllowPrivateAccess = "true"))
	float FlashlightBoostEndServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Powerups", meta = (AllowPrivateAccess = "true"))
	float QuestionHintEndServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Powerups", meta = (AllowPrivateAccess = "true"))
	float DoorRushEndServerTime;
};
