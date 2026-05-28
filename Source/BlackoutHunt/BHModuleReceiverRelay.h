#pragma once

#include "CoreMinimal.h"
#include "BHModuleReceiverInterface.h"
#include "GameFramework/Actor.h"
#include "BHModuleReceiverRelay.generated.h"

class ABHCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBHModuleReceiverRelayActivated, ABHCharacter*, Character, FName, ModuleId, bool, bActive);

UCLASS(Blueprintable)
class BLACKOUTHUNT_API ABHModuleReceiverRelay : public AActor, public IBHModuleReceiverInterface
{
	GENERATED_BODY()

public:
	ABHModuleReceiverRelay();

	virtual void OnModuleActivated_Implementation(ABHCharacter* Character, FName ModuleId, bool bActive) override;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Module")
	FName GetLastModuleId() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Module")
	bool GetLastActiveState() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Module")
	int32 GetActivationCount() const;

	UPROPERTY(BlueprintAssignable, Category = "Blackout Hunt|Module")
	FBHModuleReceiverRelayActivated OnRelayActivated;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	FName LastModuleId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	bool bLastActiveState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	int32 ActivationCount;
};

