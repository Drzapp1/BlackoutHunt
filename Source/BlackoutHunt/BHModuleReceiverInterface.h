// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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

