#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHEscapeStationManager.generated.h"

class ABHCharacter;
class ABHTrainDisplayActor;
class ABHTrainDoor;

UCLASS()
class BLACKOUTHUNT_API ABHEscapeStationManager : public AActor
{
	GENERATED_BODY()

public:
	ABHEscapeStationManager();

	virtual void Tick(float DeltaSeconds) override;

	void RegisterEscapeDoor(ABHTrainDoor* Door);
	void RegisterDisplay(ABHTrainDisplayActor* Display);
	void TriggerFinalEscape();
	bool IsFinalEscapeActive() const;
	bool IsHunterPenaltyActive(const ABHCharacter* Hunter) const;

protected:
	void OpenEscapeDoors();
	void FinishCutscene();
	void ExpireFinalEscape();
	void UpdateStationDisplays();
	void TickAntiCamp(float DeltaSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Escape")
	float EscapeSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Escape")
	float CutsceneSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Escape")
	float HunterReleaseDelaySeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Escape")
	float AntiCampRadius;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Escape")
	float AntiCampGraceSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Final Escape")
	float AntiCampPenaltySeconds;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHTrainDoor>> EscapeDoors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHTrainDisplayActor>> Displays;

	TMap<TWeakObjectPtr<ABHCharacter>, float> HunterDoorProximitySeconds;
	TMap<TWeakObjectPtr<ABHCharacter>, float> HunterPenaltyEndTimes;
	FTimerHandle CutsceneTimerHandle;
	FTimerHandle EscapeTimerHandle;
	bool bTriggered;
};
