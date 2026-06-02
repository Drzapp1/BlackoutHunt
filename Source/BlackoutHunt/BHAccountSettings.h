// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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
