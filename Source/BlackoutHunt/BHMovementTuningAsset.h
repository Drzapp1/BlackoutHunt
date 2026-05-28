#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "Engine/DataAsset.h"
#include "BHMovementTuningAsset.generated.h"

UCLASS(BlueprintType)
class BLACKOUTHUNT_API UBHMovementTuningAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UBHMovementTuningAsset();

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	FBHMovementRoleTuning ResolveRoleTuning(EBHPlayerRole Role) const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	FBHMovementSpecialTuning ResolveSpecialTuning(EBHPlayerRole Role, EBHMovementSpecialState State) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	TArray<FBHMovementRoleTuning> RoleTunings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	FBHMovementAnimationProfile AnimationProfile;
};

