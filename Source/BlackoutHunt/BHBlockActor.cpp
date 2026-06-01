// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHBlockActor.h"
#include "Async/Async.h"
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
	case EBHBlockMaterial::ConcreteWA:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_ConcreteWA.M_BH_ConcreteWA");
		break;
	case EBHBlockMaterial::ConcreteWACool:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_ConcreteWA_Cool.M_BH_ConcreteWA_Cool");
		break;
	case EBHBlockMaterial::PlasterWA:
		Path = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_PlasterWA.M_BH_PlasterWA");
		break;
	case EBHBlockMaterial::BluePanel:
		Path = TEXT("/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel1.MI_ScreenPanel1");
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
	// World-aligned variants: do NOT fall back to the plain M_BH_Concrete/M_BH_Plaster (the plain concrete is
	// the bright-red one). If the WA asset fails to load, leave the result null so ApplyVisualStyle paints the
	// block with its (grey) VisualTint instead of a red surface. One-time diagnostic so we can see, in the game
	// log, whether the WA material actually resolves at runtime or silently isn't loading.
	if (Material == EBHBlockMaterial::ConcreteWA || Material == EBHBlockMaterial::PlasterWA || Material == EBHBlockMaterial::ConcreteWACool)
	{
		const TCHAR* WAName = Material == EBHBlockMaterial::ConcreteWA ? TEXT("ConcreteWA")
			: (Material == EBHBlockMaterial::PlasterWA ? TEXT("PlasterWA") : TEXT("ConcreteWACool"));
		UE_LOG(LogTemp, Warning, TEXT("[BHBlock] WA material %s resolved to: %s"),
			WAName,
			LoadedMaterial ? *GetNameSafe(LoadedMaterial) : TEXT("NULL -> grey VisualTint fallback"));
	}
	// Blue panel: prefer a solid blue if the sci-fi screen panel is missing; otherwise leave null so
	// ApplyVisualStyle paints the block with its (blue) VisualTint as a last resort.
	if (!LoadedMaterial && Material == EBHBlockMaterial::BluePanel)
	{
		LoadedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BFHorror/DemoAssets/Materials/MI_Solid_Blue.MI_Solid_Blue"));
	}
	MaterialCache.Add(Material, LoadedMaterial);
	return LoadedMaterial;
}
}

ABHBlockActor::ABHBlockActor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.0f);
	SetMinNetUpdateFrequency(0.25f);
	NetPriority = 0.5f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCanEverAffectNavigation(true);
	VisualTint = FLinearColor(0.38f, 0.42f, 0.45f, 1.0f);
	BlockMaterial = EBHBlockMaterial::Tinted;
	bBlockCollisionEnabled = true;
	bBlockHiddenInGame = false;

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

void ABHBlockActor::BeginPlay()
{
	Super::BeginPlay();

	// Map-placed (authored) instances are never configured through the spawn-time setters, and the authored
	// export strips dynamic materials to their parent before save, so re-apply visuals + collision/hidden
	// state from the serialized properties on every authority. Without this a listen-server host (the
	// teacher) renders authored dynamic blocks as flat, untinted base material because it never receives an
	// OnRep for its own placed actors. Idempotent for the procedural path (setters already ran).
	ApplyVisualStyle();
	ApplyBlockState();
}

void ABHBlockActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHBlockActor, VisualTint);
	DOREPLIFETIME(ABHBlockActor, BlockMaterial);
	DOREPLIFETIME(ABHBlockActor, bBlockCollisionEnabled);
	DOREPLIFETIME(ABHBlockActor, bBlockHiddenInGame);
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
	if (!HasAuthority())
	{
		return;
	}

	bBlockCollisionEnabled = bEnabled;
	ApplyBlockState();
}

void ABHBlockActor::SetBlockHiddenInGame(bool bNewHidden)
{
	if (!HasAuthority())
	{
		return;
	}

	bBlockHiddenInGame = bNewHidden;
	ApplyBlockState();
}

void ABHBlockActor::OnRep_VisualTint()
{
	ApplyVisualStyle();
}

void ABHBlockActor::OnRep_BlockMaterial()
{
	ApplyVisualStyle();
}

void ABHBlockActor::OnRep_BlockState()
{
	ApplyBlockState();
}

void ABHBlockActor::ApplyBlockState()
{
	SetActorHiddenInGame(bBlockHiddenInGame);
	if (Mesh)
	{
		Mesh->SetCollisionEnabled(bBlockCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(bBlockCollisionEnabled);
	}
}

void ABHBlockActor::ApplyVisualStyle()
{
	if (!Mesh)
	{
		return;
	}

	// A block placed in a cooked .umap is constructed on the async loading thread; creating/mutating the
	// fog dynamic-material instance there asserts (must be game thread). Defer to the game thread when we are
	// off it, then re-run. Game-thread construction (procedural spawns, CDOs) and runtime calls run inline.
	if (!IsInGameThread())
	{
		TWeakObjectPtr<ABHBlockActor> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (ABHBlockActor* GameThreadActor = WeakThis.Get())
			{
				GameThreadActor->ApplyVisualStyle();
			}
		});
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
