#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHAlarmTrap.generated.h"

class UBHSynthComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class BLACKOUTHUNT_API ABHAlarmTrap : public AActor
{
	GENERATED_BODY()

public:
	ABHAlarmTrap();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> TriggerRadius;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBHSynthComponent> Synth;

	float AgeSeconds;
	bool bTriggered;
};
