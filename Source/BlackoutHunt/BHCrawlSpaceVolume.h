// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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

	// Box extent this gate was configured with. The authored-map export records this (with the actor
	// transform) into the ABHLevelMarker so the gate can be rebuilt if a later hand-edit of the baked
	// .umap drops the volume actor.
	FVector GetConfiguredExtent() const;

#if WITH_DEV_AUTOMATION_TESTS
	bool DebugCanCharacterUseCrawlSpace(const ABHCharacter* Character) const;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool CanCharacterUseCrawlSpace(const ABHCharacter* Character) const;
	void QueueRejectCharacter(ABHCharacter* Character);
	void RejectCharacter(ABHCharacter* Character);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Volume;

	TMap<TWeakObjectPtr<ABHCharacter>, FIntPoint> ActiveRejectDirections;
};
