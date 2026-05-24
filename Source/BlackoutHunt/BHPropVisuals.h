#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

namespace BHPropVisuals
{
	UStaticMesh* CubeMesh();
	UStaticMesh* CylinderMesh();
	UStaticMesh* SphereMesh();

	UMaterialInterface* BasicMaterial();
	UMaterialInterface* PaintedMetalMaterial();
	UMaterialInterface* RustedMetalMaterial();
	UMaterialInterface* DiamondPlateMaterial();
	UMaterialInterface* WarningSignMaterial();

	void ConfigurePart(
		UStaticMeshComponent* Component,
		UStaticMesh* MeshAsset,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FRotator& RelativeRotation,
		const FVector& RelativeScale,
		bool bCollisionEnabled = false);

	void TintPart(UStaticMeshComponent* Component, const FLinearColor& Color, float EmissiveStrength = 0.0f);
	void SetPartVisible(UStaticMeshComponent* Component, bool bVisible);
}
