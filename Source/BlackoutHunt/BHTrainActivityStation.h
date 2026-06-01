#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTypes.h"
#include "BHTrainActivityStation.generated.h"

class ABHPlayerState;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EBHTrainActivityType : uint8
{
	SnackCart UMETA(DisplayName = "Snack Cart"),
	DrinkCooler UMETA(DisplayName = "Drink Cooler"),
	ReflexArcade UMETA(DisplayName = "Reflex Arcade"),
	MemoryTable UMETA(DisplayName = "Memory Table")
};

UCLASS()
class BLACKOUTHUNT_API ABHTrainActivityStation : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainActivityStation();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void ConfigureActivity(EBHTrainActivityType NewActivityType, int32 NewActivitySeed);

	static FString GetActivityTitle(EBHTrainActivityType Type);
	static FString BuildActivitySummary(EBHTrainActivityType Type);
	static bool IsPassengerEligibleForActivity(const ABHPlayerState* PlayerState);

protected:
	UFUNCTION()
	void OnRep_ActivityState();

	float GetServerTimeSeconds() const;
	float GetCooldownDuration() const;
	float GetCooldownRemaining(const ABHCharacter* Character) const;
	int32 GetReflexCursorSlot(float ServerTime) const;
	int32 GetReflexTargetSlot() const;
	void GetMemoryCardSlots(float ServerTime, int32& OutLeftSlot, int32& OutRightSlot) const;
	int32 GetDisplayedActivityRound() const;
	FString BuildLiveActivityBody() const;
	FString BuildPromptRiskText(const ABHCharacter* Character) const;
	FString AwardActivityPoints(ABHPlayerState* PlayerState, int32 Points) const;
	void ApplyActivityVisuals();
	void RefreshActivityText();
	void ShowResult(ABHCharacter* Character, const FString& ResultText, const FLinearColor& ResultAccent);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ScreenPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CounterTop;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> AccentStrip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PropA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PropB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PropC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TitleText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> DetailText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> AccentLight;

	UPROPERTY(ReplicatedUsing = OnRep_ActivityState, EditAnywhere, BlueprintReadOnly, Category = "Train Activity")
	EBHTrainActivityType ActivityType;

	UPROPERTY(ReplicatedUsing = OnRep_ActivityState, EditAnywhere, BlueprintReadOnly, Category = "Train Activity")
	int32 ActivitySeed;

	UPROPERTY(ReplicatedUsing = OnRep_ActivityState, BlueprintReadOnly, Category = "Train Activity")
	int32 ActivityUseCounter;

	UPROPERTY(ReplicatedUsing = OnRep_ActivityState, BlueprintReadOnly, Category = "Train Activity")
	FString LastResultText;

	UPROPERTY(ReplicatedUsing = OnRep_ActivityState, BlueprintReadOnly, Category = "Train Activity")
	FLinearColor LastResultAccent;

	TMap<int32, float> LastUseTimeByPlayerId;
};
