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

	// True when this volume is currently sheltering the character: the character overlaps the gap AND is in a
	// state that may legitimately use it (alive Survivor/Tester in a low-profile pose, per CanCharacterUseCrawlSpace).
	// The Teacher's capture path queries this to treat a sheltering Survivor as unreachable, mirroring locker immunity.
	bool IsCharacterSheltering(const ABHCharacter* Character) const;

#if WITH_DEV_AUTOMATION_TESTS
	bool DebugCanCharacterUseCrawlSpace(const ABHCharacter* Character) const;
	// Test hook for the auto-prone-or-eject decision: true == admitted (already sheltering or just auto-proned),
	// false == will be ejected. Used to prove the Teacher is never admitted and a survivor is auto-proned in.
	bool DebugTryAdmitOrAutoProneForTest(ABHCharacter* Character) { return TryAdmitOrAutoProne(Character); }
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool CanCharacterUseCrawlSpace(const ABHCharacter* Character) const;
	// Role/life gate only (alive Survivor/Tester), WITHOUT the low-profile pose check -- a character that may use
	// the crawl once it is in a sheltering pose. CanCharacterUseCrawlSpace == this AND a low-profile pose.
	bool IsCharacterEligibleSurvivor(const ABHCharacter* Character) const;
	// Returns true if the character now shelters (already low-profile, or just auto-proned into cover); false if it
	// must be ejected. Auto-prones an eligible survivor at the mouth so a fleeing player flows in instead of being
	// bounced off the lip. The Teacher / Hall Monitor / dead are never auto-proned -- they always eject.
	bool TryAdmitOrAutoProne(ABHCharacter* Character);
	void QueueRejectCharacter(ABHCharacter* Character);
	void RejectCharacter(ABHCharacter* Character);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Volume;

	TMap<TWeakObjectPtr<ABHCharacter>, FIntPoint> ActiveRejectDirections;
};
