#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "TimerManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "BHSecurityMonitor.generated.h"

class ABHSecurityCamera;
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
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	ABHSecurityCamera* FindBestCameraFor(ABHCharacter* Character) const;
	void RefreshLiveFeed();
	void ReleaseLiveFeedCamera();
	int32 ResolveLiveFeedResolution() const;
	void ApplyStaticScreenVisual();

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
	FTimerHandle LiveFeedRefreshTimerHandle;
};
