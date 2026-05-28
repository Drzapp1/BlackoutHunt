#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "Animation/AnimInstance.h"
#include "BHMovementAnimInstance.generated.h"

UCLASS(Blueprintable, BlueprintType, Transient)
class BLACKOUTHUNT_API UBHMovementAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Blackout Hunt|Movement")
	void SetBlackoutMovementState(EBHMovementSpecialState NewAuthoritativeState, EBHMovementSpecialState NewCosmeticState, float NewSpeed2D, bool bNewProneMoving, bool bNewGrounded, float NewFailurePulse);

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Movement")
	EBHMovementSpecialState AuthoritativeSpecialState = EBHMovementSpecialState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Movement")
	EBHMovementSpecialState CosmeticSpecialState = EBHMovementSpecialState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Movement")
	EBHMovementSpecialState VisualSpecialState = EBHMovementSpecialState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Movement")
	float Speed2D = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Movement")
	bool bProneMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Movement")
	bool bGrounded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Movement")
	float MovementFailurePulse = 0.0f;
};

