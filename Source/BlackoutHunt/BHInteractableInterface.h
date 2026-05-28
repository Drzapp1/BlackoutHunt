#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "UObject/Interface.h"
#include "BHInteractableInterface.generated.h"

class ABHCharacter;

UINTERFACE(BlueprintType)
class UBHInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class BLACKOUTHUNT_API IBHInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Blackout Hunt")
	bool CanInteract(ABHCharacter* Character) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Blackout Hunt")
	void BeginInteract(ABHCharacter* Character);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Blackout Hunt")
	void EndInteract(ABHCharacter* Character);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Blackout Hunt")
	FText GetInteractionLabel(ABHCharacter* Character) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Blackout Hunt")
	FBHInteractionPromptInfo GetInteractionPromptInfo(ABHCharacter* Character) const;
};
