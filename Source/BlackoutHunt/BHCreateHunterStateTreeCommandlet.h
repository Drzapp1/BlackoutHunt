#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BHCreateHunterStateTreeCommandlet.generated.h"

UCLASS()
class BLACKOUTHUNT_API UBHCreateHunterStateTreeCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBHCreateHunterStateTreeCommandlet(const FObjectInitializer& ObjectInitializer);

	virtual int32 Main(const FString& Params) override;
};
