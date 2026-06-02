// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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
	FogSheet,
	// World-aligned (triplanar) variants of Concrete/Plaster. Tiling is in world space, so the texture stays
	// consistent no matter how the cube is scaled or rotated -> large blockout walls / floor / ceiling slabs and
	// angled partitions read as proper concrete/plaster instead of stretched smears. Used by the backrooms maze.
	// Appended at the end so existing baked .umaps keep their serialized enum byte values.
	ConcreteWA,
	PlasterWA,
	// Glowing blue sci-fi panel graphic (orientation accents in the dark hall). Falls back to a solid blue
	// tint if the screen material is missing.
	BluePanel,
	// Cool gray-teal world-aligned concrete: same triplanar tiling as ConcreteWA but a cold industrial tint
	// instead of the backrooms dark-red. Used for the Substation so its concrete reads textured (not stretched)
	// while keeping the substation's cold palette. Appended last to preserve baked .umap enum byte values.
	ConcreteWACool
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
	void SetBlockHiddenInGame(bool bNewHidden);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Block")
	EBHBlockMaterial GetBlockMaterial() const { return BlockMaterial; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_VisualTint();

	UFUNCTION()
	void OnRep_BlockMaterial();

	UFUNCTION()
	void OnRep_BlockState();

	void ApplyVisualStyle();
	void ApplyBlockState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_VisualTint)
	FLinearColor VisualTint;

	UPROPERTY(ReplicatedUsing = OnRep_BlockMaterial)
	EBHBlockMaterial BlockMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_BlockState)
	bool bBlockCollisionEnabled;

	UPROPERTY(ReplicatedUsing = OnRep_BlockState)
	bool bBlockHiddenInGame;
};
