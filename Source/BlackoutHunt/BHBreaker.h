#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHBreaker.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHBreaker : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHBreaker();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void SetDirectorActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsDirectorActive() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsRepaired() const;

protected:
	void CompleteRepair();
	void ApplyBreakerVisuals();
	void BroadcastBreakerAudioCue(bool bCompleted);

	UFUNCTION()
	void OnRep_BreakerVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FrontPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WarningBand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GaugeFace;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GaugeNeedle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BreakerLeverA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BreakerLeverB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BreakerLeverC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StatusLightA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StatusLightB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StatusLightC;

	UPROPERTY(ReplicatedUsing = OnRep_BreakerVisuals, BlueprintReadOnly, Category = "Breaker")
	float RepairProgress;

	UPROPERTY(ReplicatedUsing = OnRep_BreakerVisuals, BlueprintReadOnly, Category = "Breaker")
	bool bRepaired;

	UPROPERTY(ReplicatedUsing = OnRep_BreakerVisuals, BlueprintReadOnly, Category = "Breaker")
	bool bDirectorActive;

	UPROPERTY(EditDefaultsOnly, Category = "Breaker")
	float RepairSeconds;

	TSet<TWeakObjectPtr<ABHCharacter>> Repairers;
	float LastNoiseTime;
	float LastHumCueTime;
};
