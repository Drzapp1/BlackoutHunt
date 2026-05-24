#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHSecurityShutter.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHSecurityShutter : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHSecurityShutter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Configure(int32 NewCircuitId);
	void SetOpen(bool bNewOpen);
	int32 GetCircuitId() const;

protected:
	UFUNCTION()
	void OnRep_Open();

	void ApplyOpenState();

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen;

	UPROPERTY(Replicated)
	int32 CircuitId;
};
