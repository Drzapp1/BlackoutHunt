#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "TimerManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "BHSecurityMonitor.generated.h"

class ABHSecurityCamera;
class ABHCharacter;
class UMaterialInstanceDynamic;

UCLASS()
class BLACKOUTHUNT_API ABHSecurityMonitor : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHSecurityMonitor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

#if WITH_DEV_AUTOMATION_TESTS
	void DebugPruneActiveInspections();
#endif

protected:
	ABHSecurityCamera* GetCurrentSignalCamera() const;
	ABHSecurityCamera* FindBestCameraFor(ABHCharacter* Character) const;
	void RefreshLiveFeed();
	void ReleaseLiveFeedCamera();
	int32 ResolveLiveFeedResolution() const;
	void ApplyStaticScreenVisual();
	void PruneActiveInspections();
	bool IsActiveInspectionStillValid(const ABHCharacter* Character, const ABHSecurityCamera* Camera) const;
	bool TrySendHallMonitorFalsePing(ABHCharacter* Character, ABHSecurityCamera* Camera);
	FVector ResolveFalsePingLocation(const ABHCharacter* Character, const ABHSecurityCamera* Camera) const;
	void PruneFalsePingCooldowns(float Now);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Screen;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ScanBar;

	UPROPERTY(EditAnywhere, Category = "Security")
	float MonitorRange;

	UPROPERTY(EditAnywhere, Category = "Security")
	bool bLiveFeedEnabled;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LiveFeedMaterial;

	TWeakObjectPtr<ABHSecurityCamera> LiveFeedCamera;
	TMap<TWeakObjectPtr<ABHCharacter>, TWeakObjectPtr<ABHSecurityCamera>> ActiveInspectionCameras;
	TMap<TWeakObjectPtr<ABHCharacter>, float> LastFalsePingTimes;
	float LastFalsePingPruneTime;
	FTimerHandle LiveFeedRefreshTimerHandle;
	FTimerHandle ActiveInspectionPruneTimerHandle;
};
