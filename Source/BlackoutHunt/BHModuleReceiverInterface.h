#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BHModuleReceiverInterface.generated.h"

class ABHCharacter;

UINTERFACE(BlueprintType)
class UBHModuleReceiverInterface : public UInterface
{
	GENERATED_BODY()
};

class BLACKOUTHUNT_API IBHModuleReceiverInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Blackout Hunt|Module")
	void OnModuleActivated(ABHCharacter* Character, FName ModuleId, bool bActive);
};

