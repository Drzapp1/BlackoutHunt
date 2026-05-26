#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHTrainTunnelMotionActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class BLACKOUTHUNT_API ABHTrainTunnelMotionActor : public AActor
{
	GENERATED_BODY()

public:
	ABHTrainTunnelMotionActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetMoving(bool bNewMoving);
	void ConfigureMotion(float NewLoopLength, float NewSpeed);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> LightStrips;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Train")
	bool bMoving;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float LoopLength;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float MotionSpeed;

	float MotionOffset;
};
