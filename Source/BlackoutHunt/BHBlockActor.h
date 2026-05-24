#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHBlockActor.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBHBlockMaterial : uint8
{
	Tinted,
	Concrete,
	Plaster,
	RustedMetal,
	DiamondPlate,
	PaintedMetal,
	Tiles,
	WarningSign,
	FogSheet
};

UCLASS()
class BLACKOUTHUNT_API ABHBlockActor : public AActor
{
	GENERATED_BODY()

public:
	ABHBlockActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetVisualTint(const FLinearColor& NewTint);
	void SetBlockMaterial(EBHBlockMaterial NewMaterial);
	void SetBlockCollisionEnabled(bool bEnabled);

protected:
	UFUNCTION()
	void OnRep_VisualTint();

	UFUNCTION()
	void OnRep_BlockMaterial();

	void ApplyVisualStyle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_VisualTint)
	FLinearColor VisualTint;

	UPROPERTY(ReplicatedUsing = OnRep_BlockMaterial)
	EBHBlockMaterial BlockMaterial;
};
