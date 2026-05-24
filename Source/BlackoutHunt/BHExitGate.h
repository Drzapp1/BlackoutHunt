#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHExitGate.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHExitGate : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHExitGate();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

	void SetDirectorActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsDirectorActive() const;

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Exit")
	bool bDirectorActive;
};
