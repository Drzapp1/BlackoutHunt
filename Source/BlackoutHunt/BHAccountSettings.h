#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BHAccountSettings.generated.h"

UCLASS(Config=Game, DefaultConfig)
class BLACKOUTHUNT_API UBHAccountSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Accounts")
	FString BackendBaseUrl;

	UPROPERTY(Config, EditAnywhere, Category = "Accounts")
	bool bEnableExternalAccountLogin = false;

	UPROPERTY(Config, EditAnywhere, Category = "Accounts")
	float LoginPollSeconds = 2.0f;
};
