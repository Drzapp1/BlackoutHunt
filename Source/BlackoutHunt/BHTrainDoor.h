// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainDoor.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class BLACKOUTHUNT_API ABHTrainDoor : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainDoor();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void ConfigureDoor(bool bNewEscapeDoor, const FString& NewDoorName);
	void SetDoorOpen(bool bNewOpen);
	bool IsDoorOpen() const;
	bool IsEscapeDoor() const;

protected:
	UFUNCTION()
	void OnRep_DoorState();

	UFUNCTION()
	void OnEscapeVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyDoorVisuals(float DeltaSeconds);
	bool CanEscapeThroughDoor(ABHCharacter* Character) const;
	void EscapeCharacter(ABHCharacter* Character);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeaderLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> EscapeVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> DoorBlocker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeaderLightMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_DoorState, BlueprintReadOnly, Category = "Train Door")
	bool bOpen;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Train Door")
	bool bEscapeDoor;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Train Door")
	FString DoorName;

	float DoorOpenAlpha;
};
