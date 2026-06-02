// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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
