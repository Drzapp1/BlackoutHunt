#include "BHBlockActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
UMaterialInterface* LoadBlockMaterial(EBHBlockMaterial Material)
{
	static TMap<EBHBlockMaterial, TWeakObjectPtr<UMaterialInterface>> MaterialCache;
	if (TWeakObjectPtr<UMaterialInterface>* CachedMaterial = MaterialCache.Find(Material))
	{
		if (CachedMaterial->IsValid())
		{
			return CachedMaterial->Get();
		}
	}

	const TCHAR* Path = nullptr;
	switch (Material)
	{
	case EBHBlockMaterial::Concrete:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Concrete.M_BH_Concrete");
		break;
	case EBHBlockMaterial::Plaster:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Plaster.M_BH_Plaster");
		break;
	case EBHBlockMaterial::RustedMetal:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_RustedMetal.M_BH_RustedMetal");
		break;
	case EBHBlockMaterial::DiamondPlate:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_DiamondPlate.M_BH_DiamondPlate");
		break;
	case EBHBlockMaterial::PaintedMetal:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_PaintedMetal.M_BH_PaintedMetal");
		break;
	case EBHBlockMaterial::Tiles:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Tiles.M_BH_Tiles");
		break;
	case EBHBlockMaterial::WarningSign:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_WarningSign.M_BH_WarningSign");
		break;
	case EBHBlockMaterial::FogSheet:
		Path = TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent");
		break;
	default:
		break;
	}

	UMaterialInterface* LoadedMaterial = Path ? LoadObject<UMaterialInterface>(nullptr, Path) : nullptr;
	if (!LoadedMaterial && Material == EBHBlockMaterial::FogSheet)
	{
		LoadedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineVolumetrics/Fogsheet/Materials/M_EV_FogSheet_2sided_Master_Addi.M_EV_FogSheet_2sided_Master_Addi"));
	}
	MaterialCache.Add(Material, LoadedMaterial);
	return LoadedMaterial;
}
}

ABHBlockActor::ABHBlockActor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	NetUpdateFrequency = 1.0f;
	MinNetUpdateFrequency = 0.25f;
	NetPriority = 0.5f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCanEverAffectNavigation(true);
	VisualTint = FLinearColor(0.38f, 0.42f, 0.45f, 1.0f);
	BlockMaterial = EBHBlockMaterial::Tinted;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void ABHBlockActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHBlockActor, VisualTint);
	DOREPLIFETIME(ABHBlockActor, BlockMaterial);
}

void ABHBlockActor::SetVisualTint(const FLinearColor& NewTint)
{
	VisualTint = NewTint;
	ApplyVisualStyle();
}

void ABHBlockActor::SetBlockMaterial(EBHBlockMaterial NewMaterial)
{
	BlockMaterial = NewMaterial;
	ApplyVisualStyle();
}

void ABHBlockActor::SetBlockCollisionEnabled(bool bEnabled)
{
	if (Mesh)
	{
		Mesh->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(bEnabled);
	}
}

void ABHBlockActor::OnRep_VisualTint()
{
	ApplyVisualStyle();
}

void ABHBlockActor::OnRep_BlockMaterial()
{
	ApplyVisualStyle();
}

void ABHBlockActor::ApplyVisualStyle()
{
	if (!Mesh)
	{
		return;
	}

	const bool bFogSheet = BlockMaterial == EBHBlockMaterial::FogSheet;
	Mesh->SetCastShadow(!bFogSheet);
	Mesh->SetReceivesDecals(!bFogSheet);
	Mesh->SetTranslucentSortPriority(bFogSheet ? 10 : 0);

	if (UMaterialInterface* Material = LoadBlockMaterial(BlockMaterial))
	{
		if (!bFogSheet)
		{
			Mesh->SetMaterial(0, Material);
			return;
		}

		UMaterialInstanceDynamic* FogMaterial = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Material);
		if (FogMaterial)
		{
			const FLinearColor FogTint(VisualTint.R, VisualTint.G, VisualTint.B, FMath::Clamp(VisualTint.A, 0.0f, 1.0f));
			FogMaterial->SetVectorParameterValue(TEXT("Color"), FogTint);
			FogMaterial->SetVectorParameterValue(TEXT("BaseColor"), FogTint);
			FogMaterial->SetVectorParameterValue(TEXT("Tint"), FogTint);
			FogMaterial->SetVectorParameterValue(TEXT("FogColor"), FogTint);
			FogMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), FogTint * 0.12f);
			FogMaterial->SetScalarParameterValue(TEXT("Opacity"), FogTint.A);
			FogMaterial->SetScalarParameterValue(TEXT("Alpha"), FogTint.A);
			FogMaterial->SetScalarParameterValue(TEXT("Density"), FMath::Lerp(0.45f, 1.8f, FogTint.A));
			FogMaterial->SetScalarParameterValue(TEXT("Brightness"), 0.85f);
		}
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), VisualTint);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), VisualTint);
	}
}
