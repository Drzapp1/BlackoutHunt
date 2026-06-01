#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHButtonModule.generated.h"

class USoundBase;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBHButtonModuleMode : uint8
{
	Press UMETA(DisplayName = "Press"),
	Hold UMETA(DisplayName = "Hold"),
	Toggle UMETA(DisplayName = "Toggle"),
	OneShot UMETA(DisplayName = "One Shot")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBHButtonModuleActivated, ABHCharacter*, Character, FName, ModuleId, bool, bActive);

UCLASS()
class BLACKOUTHUNT_API ABHButtonModule : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHButtonModule();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Blackout Hunt|Module")
	void ConfigureModule(FName NewModuleId, EBHButtonModuleMode NewMode, const FText& NewLabel, float NewHoldSeconds, float NewCooldownSeconds);

	UFUNCTION(BlueprintCallable, Category = "Blackout Hunt|Module")
	void AddLinkedReceiver(AActor* Receiver);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Module")
	bool IsActive() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Module")
	bool IsConsumed() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Module")
	float GetHoldProgress() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Module")
	float GetRemainingCooldownSeconds() const;

	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	UPROPERTY(BlueprintAssignable, Category = "Blackout Hunt|Module")
	FBHButtonModuleActivated OnModuleActivated;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Blackout Hunt|Module")
	void ReceiveModuleActivated(ABHCharacter* Character, FName ActivatedModuleId, bool bNewActive);

	UFUNCTION()
	void OnRep_ModuleState();

	// Plays the press SFX on every machine (server + all clients). ActivateModule runs server-only and
	// a Press-mode button toggles bActive true->false within one call, so OnRep cannot be relied on to
	// fire the cue remotely; an unreliable multicast guarantees clients hear the press, not just the host.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayPressSound();

	void ActivateModule(ABHCharacter* Character);
	void ApplyModuleVisuals();
	float GetServerTimeSeconds() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonBaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonRingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StatusLightMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	FName ModuleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	EBHButtonModuleMode Mode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	FText ModuleLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module", meta = (ClampMin = "0.0"))
	float HoldSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module", meta = (ClampMin = "0.0"))
	float CooldownSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module")
	TObjectPtr<USoundBase> PressSound;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Module")
	TArray<TObjectPtr<AActor>> LinkedReceivers;

	UPROPERTY(ReplicatedUsing = OnRep_ModuleState, BlueprintReadOnly, Category = "Module")
	bool bActive;

	UPROPERTY(ReplicatedUsing = OnRep_ModuleState, BlueprintReadOnly, Category = "Module")
	bool bConsumed;

	UPROPERTY(ReplicatedUsing = OnRep_ModuleState, BlueprintReadOnly, Category = "Module")
	float HoldProgress;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Module")
	float CooldownEndServerTime;

	UPROPERTY()
	TWeakObjectPtr<ABHCharacter> ActiveInteractor;
};
