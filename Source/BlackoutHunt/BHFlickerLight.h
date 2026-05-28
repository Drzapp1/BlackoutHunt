#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "BHFlickerLight.generated.h"

class UPointLightComponent;

UCLASS()
class BLACKOUTHUNT_API ABHFlickerLight : public AActor
{
	GENERATED_BODY()

public:
	ABHFlickerLight();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Configure(int32 NewCircuitId, const FLinearColor& NewColor, float NewBaseIntensity, float NewRadius);
	void SetPowered(bool bNewPowered);
	void TriggerFlickerBurst(float DurationSeconds, float IntensityMultiplier, const FLinearColor& BurstColor, float BurstSpeed, bool bForcePowered = true);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	int32 GetCircuitId() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsPowered() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsFlickerBurstActive() const;

protected:
	UFUNCTION()
	void OnRep_Powered();

	UFUNCTION()
	void OnRep_LightConfig();

	UFUNCTION()
	void OnRep_FlickerBurst();

	void EndFlickerBurst();
	void ApplyLightState();
	void ApplyLightConfig();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(ReplicatedUsing = OnRep_LightConfig, EditDefaultsOnly, Category = "Light")
	float BaseIntensity;

	UPROPERTY(EditDefaultsOnly, Category = "Light")
	float FlickerAmount;

	UPROPERTY(EditDefaultsOnly, Category = "Light")
	float FlickerSpeed;

	UPROPERTY(ReplicatedUsing = OnRep_LightConfig, EditDefaultsOnly, Category = "Light")
	FLinearColor LightColor;

	UPROPERTY(ReplicatedUsing = OnRep_LightConfig, EditDefaultsOnly, Category = "Light")
	float LightRadius;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Light")
	int32 CircuitId;

	UPROPERTY(ReplicatedUsing = OnRep_Powered, BlueprintReadOnly, Category = "Light")
	bool bPowered;

	UPROPERTY(ReplicatedUsing = OnRep_FlickerBurst, BlueprintReadOnly, Category = "Light")
	bool bFlickerBurstActive;

	UPROPERTY(ReplicatedUsing = OnRep_FlickerBurst, BlueprintReadOnly, Category = "Light")
	float FlickerBurstEndTime;

	UPROPERTY(ReplicatedUsing = OnRep_FlickerBurst, BlueprintReadOnly, Category = "Light")
	float FlickerBurstIntensityMultiplier;

	UPROPERTY(ReplicatedUsing = OnRep_FlickerBurst, BlueprintReadOnly, Category = "Light")
	float FlickerBurstSpeed;

	UPROPERTY(ReplicatedUsing = OnRep_FlickerBurst, BlueprintReadOnly, Category = "Light")
	FLinearColor FlickerBurstColor;

	UPROPERTY(ReplicatedUsing = OnRep_FlickerBurst, BlueprintReadOnly, Category = "Light")
	bool bFlickerBurstForcePowered;

	float Phase;
	float FlickerBurstRemainingSeconds;
	FTimerHandle FlickerBurstTimerHandle;
};
