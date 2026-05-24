#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHPanicAlarm.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHPanicAlarm : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHPanicAlarm();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	UFUNCTION()
	void OnRep_Used();

	void ApplyAlarmVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> AlarmBell;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PullHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> AlarmLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> AlarmLight;

	UPROPERTY(ReplicatedUsing = OnRep_Used, BlueprintReadOnly, Category = "Alarm")
	bool bUsed;
};
