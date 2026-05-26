#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHCCTVZone.generated.h"

class ABHCharacter;
class ABHSecurityCamera;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS()
class BLACKOUTHUNT_API ABHCCTVZone : public AActor
{
	GENERATED_BODY()

public:
	ABHCCTVZone();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ConfigureZone(ABHSecurityCamera* NewCamera, int32 NewCircuitId, const FString& NewAlertLabel, const FVector& NewBoxExtent, bool bNewVisible);
	void SetZoneEnabled(bool bNewEnabled);
	void SetZoneVisible(bool bNewVisible);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Security")
	int32 GetCircuitId() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Security")
	bool IsZoneEnabled() const;

protected:
	UFUNCTION()
	void OnRep_ZoneState();

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryAlertForActor(AActor* OtherActor);
	void ApplyZoneVisuals();
	void RefreshMarkerScale(const FVector& BoxExtent);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> ZoneTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ZonePlate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WarningStripeA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WarningStripeB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WarningStripeC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WarningStripeD;

	UPROPERTY(ReplicatedUsing = OnRep_ZoneState)
	TObjectPtr<ABHSecurityCamera> LinkedCamera;

	UPROPERTY(ReplicatedUsing = OnRep_ZoneState, EditAnywhere, Category = "Security")
	bool bZoneEnabled;

	UPROPERTY(ReplicatedUsing = OnRep_ZoneState, EditAnywhere, Category = "Security")
	bool bZoneVisible;

	UPROPERTY(Replicated, EditAnywhere, Category = "Security")
	int32 CircuitId;

	UPROPERTY(Replicated, EditAnywhere, Category = "Security")
	FString AlertLabel;
};
