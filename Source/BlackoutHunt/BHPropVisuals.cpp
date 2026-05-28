#include "BHPropVisuals.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
UStaticMesh* LoadStaticMesh(const TCHAR* Path)
{
	return LoadObject<UStaticMesh>(nullptr, Path);
}

UMaterialInterface* LoadMaterial(const TCHAR* Path)
{
	return LoadObject<UMaterialInterface>(nullptr, Path);
}
}

namespace BHPropVisuals
{
UStaticMesh* CubeMesh()
{
	static UStaticMesh* Mesh = LoadStaticMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	return Mesh;
}

UStaticMesh* CylinderMesh()
{
	static UStaticMesh* Mesh = LoadStaticMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	return Mesh;
}

UStaticMesh* SphereMesh()
{
	static UStaticMesh* Mesh = LoadStaticMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	return Mesh;
}

UMaterialInterface* BasicMaterial()
{
	static UMaterialInterface* Material = LoadMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	return Material;
}

UMaterialInterface* PaintedMetalMaterial()
{
	static UMaterialInterface* Material = LoadMaterial(TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_PaintedMetal.M_BH_PaintedMetal"));
	return Material ? Material : BasicMaterial();
}

UMaterialInterface* RustedMetalMaterial()
{
	static UMaterialInterface* Material = LoadMaterial(TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_RustedMetal.M_BH_RustedMetal"));
	return Material ? Material : BasicMaterial();
}

UMaterialInterface* DiamondPlateMaterial()
{
	static UMaterialInterface* Material = LoadMaterial(TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_DiamondPlate.M_BH_DiamondPlate"));
	return Material ? Material : BasicMaterial();
}

UMaterialInterface* WarningSignMaterial()
{
	static UMaterialInterface* Material = LoadMaterial(TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_WarningSign.M_BH_WarningSign"));
	return Material ? Material : BasicMaterial();
}

UMaterialInterface* ReadableTextMaterial()
{
	static UMaterialInterface* Material = LoadMaterial(TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
	return Material;
}

void ConfigurePart(
	UStaticMeshComponent* Component,
	UStaticMesh* MeshAsset,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FRotator& RelativeRotation,
	const FVector& RelativeScale,
	bool bCollisionEnabled)
{
	if (!Component)
	{
		return;
	}

	if (Component->IsRegistered() && Component->Mobility == EComponentMobility::Static)
	{
		Component->SetMobility(EComponentMobility::Movable);
	}

	Component->SetStaticMesh(MeshAsset);
	Component->SetRelativeLocation(RelativeLocation);
	Component->SetRelativeRotation(RelativeRotation);
	Component->SetRelativeScale3D(RelativeScale);
	Component->SetCollisionEnabled(bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(bCollisionEnabled);
	if (Material)
	{
		UMaterialInterface* CurrentMaterial = Component->GetMaterial(0);
		const bool bCurrentIsDynamicTint = CurrentMaterial && CurrentMaterial->IsA<UMaterialInstanceDynamic>();
		const bool bTargetIsBasic = Material == BasicMaterial();
		if (!CurrentMaterial || (!bCurrentIsDynamicTint && CurrentMaterial != Material) || (bCurrentIsDynamicTint && !bTargetIsBasic))
		{
			Component->SetMaterial(0, Material);
		}
	}
}

void TintPart(UStaticMeshComponent* Component, const FLinearColor& Color, float EmissiveStrength)
{
	if (!Component)
	{
		return;
	}

	if (!Component->GetMaterial(0))
	{
		Component->SetMaterial(0, BasicMaterial());
	}

	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0));
	if (!DynamicMaterial)
	{
		DynamicMaterial = Component->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
		DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * EmissiveStrength);
	}
}

void SetPartVisible(UStaticMeshComponent* Component, bool bVisible)
{
	if (!Component)
	{
		return;
	}

	Component->SetVisibility(bVisible, true);
	Component->SetHiddenInGame(!bVisible);
}

void ConfigureReadableText(
	UTextRenderComponent* Component,
	const FVector& RelativeLocation,
	const FRotator& RelativeRotation,
	float WorldSize,
	const FColor& Color)
{
	if (!Component)
	{
		return;
	}

	Component->SetRelativeLocation(RelativeLocation);
	Component->SetRelativeRotation(RelativeRotation);
	Component->SetUsingAbsoluteScale(true);
	Component->SetWorldScale3D(FVector::OneVector);
	Component->SetHorizontalAlignment(EHTA_Left);
	Component->SetVerticalAlignment(EVRTA_TextTop);
	Component->SetWorldSize(WorldSize);
	Component->SetTextRenderColor(Color);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
	Component->SetReceivesDecals(false);
	Component->SetTranslucentSortPriority(12);
	Component->SetBoundsScale(2.0f);
	if (UMaterialInterface* TextMaterial = ReadableTextMaterial())
	{
		Component->SetTextMaterial(TextMaterial);
	}
}
}
