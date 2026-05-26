#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHCrawlSpaceVolume.generated.h"

class ABHCharacter;
class UBoxComponent;

UCLASS()
class BLACKOUTHUNT_API ABHCrawlSpaceVolume : public AActor
{
	GENERATED_BODY()

public:
	ABHCrawlSpaceVolume();

	virtual void Tick(float DeltaSeconds) override;

	void Configure(const FVector& NewBoxExtent);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool CanCharacterUseCrawlSpace(const ABHCharacter* Character) const;
	void RejectCharacter(ABHCharacter* Character);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Volume;

	TMap<TWeakObjectPtr<ABHCharacter>, FIntPoint> ActiveRejectDirections;
};
