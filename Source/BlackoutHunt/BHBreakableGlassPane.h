// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTypes.h"
#include "BHBreakableGlassPane.generated.h"

class USoundBase;

UCLASS()
class BLACKOUTHUNT_API ABHBreakableGlassPane : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHBreakableGlassPane();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void ConfigurePane(const FText& NewLabel, float NewBreakNoiseStrength);
	bool CrackGlass(AActor* InstigatorActor, const FString& Reason);
	bool BreakGlass(AActor* InstigatorActor, const FString& Reason);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Glass")
	EBHBreakableGlassState GetGlassState() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Glass")
	bool IsBroken() const;

protected:
	UFUNCTION()
	void OnRep_GlassState();

	UFUNCTION()
	void OnPaneHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastGlassCue(EBHBreakableGlassState NewState, FVector CueLocation);

	void ApplyGlassVisuals();

	UPROPERTY(ReplicatedUsing = OnRep_GlassState, BlueprintReadOnly, Category = "Glass")
	EBHBreakableGlassState GlassState;

	UPROPERTY(EditDefaultsOnly, Category = "Glass", meta = (ClampMin = "0.0"))
	float CrackDamageThreshold;

	UPROPERTY(EditDefaultsOnly, Category = "Glass", meta = (ClampMin = "0.0"))
	float BreakDamageThreshold;

	UPROPERTY(EditDefaultsOnly, Category = "Glass", meta = (ClampMin = "0.0"))
	float BreakNoiseStrength;

	UPROPERTY(EditDefaultsOnly, Category = "Glass")
	TSoftObjectPtr<USoundBase> BreakSound;
};
