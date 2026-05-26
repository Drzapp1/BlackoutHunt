#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "GameFramework/Actor.h"
#include "BHTrainIntermissionManager.generated.h"

class ABHTrainBonusQuestionTerminal;
class ABHTrainDisplayActor;
class ABHTrainDoor;
class ABHTrainTunnelMotionActor;

UCLASS()
class BLACKOUTHUNT_API ABHTrainIntermissionManager : public AActor
{
	GENERATED_BODY()

public:
	ABHTrainIntermissionManager();

	void ConfigureIntermission(int32 NewCompletedStageIndex, const FString& NewNextMapName, const FString& NewDestinationText, bool bNewFinalRecap);
	void RegisterDoor(ABHTrainDoor* Door);
	void RegisterDisplay(ABHTrainDisplayActor* Display);
	void RegisterTunnelMotion(ABHTrainTunnelMotionActor* TunnelMotion);
	void RegisterBonusTerminal(ABHTrainBonusQuestionTerminal* Terminal);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Train")
	EBHTrainPhase GetPhase() const;

	void TesterAdvancePhase();
	void TesterFinishIntermission();

protected:
	virtual void BeginPlay() override;

	void StartIntermission();
	void SetPhase(EBHTrainPhase NewPhase, float DurationSeconds, const FString& Announcement);
	void AdvancePhase();
	void UpdateDisplaysForPhase();
	void SetTrainDoorsOpen(bool bOpen);
	void SetTunnelMoving(bool bMoving);
	void AutoBoardPlayers();
	void FinishIntermission();
	EBHPhysicsTopic SelectBonusTopic() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float ArrivalSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float RecapSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float BonusQuestionSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float ShopSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float StationStopSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float DepartureSeconds;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHTrainDoor>> Doors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHTrainDisplayActor>> Displays;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHTrainTunnelMotionActor>> TunnelMotionActors;

	UPROPERTY(Transient)
	TObjectPtr<ABHTrainBonusQuestionTerminal> BonusTerminal;

	EBHTrainPhase CurrentPhase;
	int32 CompletedStageIndex;
	FString NextMapName;
	FString DestinationText;
	bool bFinalRecap;
	FTimerHandle PhaseTimerHandle;
};
