// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHStaticBlockField.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABHStaticBlockField::ABHStaticBlockField()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.0f);
	SetMinNetUpdateFrequency(0.5f);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	// The generated instanced-mesh children are Static; a Static child attached to a non-Static parent
	// trips the engine "cannot attach ... is static. Aborting" error on host and every client. Make the
	// root Static so the attachment is valid.
	Root->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BlockMesh = CubeMesh.Object;
	}
}

void ABHStaticBlockField::BeginPlay()
{
	Super::BeginPlay();
	RebuildComponents();
}

void ABHStaticBlockField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearGeneratedComponents();
	Super::EndPlay(EndPlayReason);
}

void ABHStaticBlockField::SetBlockSpecs(const TArray<FBHStaticBlockSpec>& InSpecs)
{
	if (!HasAuthority())
	{
		return;
	}

	StaticBlockSpecs = InSpecs;
}

void ABHStaticBlockField::AddBlockSpec(const FBHStaticBlockSpec& Spec)
{
	if (!HasAuthority())
	{
		return;
	}

	StaticBlockSpecs.Add(Spec);
}

void ABHStaticBlockField::ResetBlockSpecs()
{
	if (!HasAuthority())
	{
		return;
	}

	StaticBlockSpecs.Reset();
	ClearGeneratedComponents();
	ForceNetUpdate();
}

void ABHStaticBlockField::FinalizeBuild()
{
	if (!HasAuthority())
	{
		return;
	}

	++BuildVersion;
	RebuildComponents();
	ForceNetUpdate();
}

void ABHStaticBlockField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABHStaticBlockField, StaticBlockSpecs);
	DOREPLIFETIME(ABHStaticBlockField, BuildVersion);
}

void ABHStaticBlockField::OnRep_StaticBlockSpecs()
{
	RebuildComponents();
}

void ABHStaticBlockField::RebuildComponents()
{
	ClearGeneratedComponents();

	if (!BlockMesh)
	{
		BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	if (!BlockMesh)
	{
		return;
	}

	// Batch blocks into one InstancedStaticMeshComponent per distinct render/collision signature rather
	// than one component per block. A full level can be ~1800 blocks; a component-per-block defeats
	// instancing and registers ~1800 components on every client (huge draw-call/registration cost and a
	// long synchronous hitch when the replicated spec array arrives). Blocks sharing material/collision/
	// hidden state batch into a single component; the per-tint materials key on tint as well so distinct
	// dynamic materials are not forced to share one component.
	TMap<FString, UInstancedStaticMeshComponent*> ComponentsByKey;

	for (int32 Index = 0; Index < StaticBlockSpecs.Num(); ++Index)
	{
		const FBHStaticBlockSpec& Spec = StaticBlockSpecs[Index];

		// Reject degenerate transforms: a NaN (from corrupt/hostile replication or an upstream math
		// error) or a fully-zero scale yields invisible-but-colliding geometry and trips render/physics
		// ensures. Skip rather than feed a bad transform to AddInstance.
		if (Spec.Location.ContainsNaN() || Spec.Scale.ContainsNaN() || Spec.Rotation.ContainsNaN()
			|| Spec.Scale.GetAbsMax() <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const bool bPerTintMaterial = Spec.Material == EBHBlockMaterial::FogSheet || Spec.Material == EBHBlockMaterial::Tinted;
		FString Key = FString::Printf(TEXT("M%d_C%d_H%d"), static_cast<int32>(Spec.Material), Spec.bCollides ? 1 : 0, Spec.bHidden ? 1 : 0);
		if (bPerTintMaterial)
		{
			Key += FString::Printf(TEXT("_T%d_%d_%d_%d"),
				FMath::RoundToInt(Spec.Tint.R * 255.0f),
				FMath::RoundToInt(Spec.Tint.G * 255.0f),
				FMath::RoundToInt(Spec.Tint.B * 255.0f),
				FMath::RoundToInt(Spec.Tint.A * 255.0f));
		}

		UInstancedStaticMeshComponent* Component = ComponentsByKey.FindRef(Key);
		if (!Component)
		{
			const FName ComponentName = MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(), TEXT("StaticBlock"));
			Component = NewObject<UInstancedStaticMeshComponent>(this, ComponentName);
			if (!Component)
			{
				continue;
			}

			Component->SetStaticMesh(BlockMesh);
			Component->SetupAttachment(GetRootComponent());
			Component->SetMobility(EComponentMobility::Static);
			Component->SetCollisionEnabled(Spec.bCollides ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			Component->SetCollisionProfileName(Spec.bCollides ? TEXT("BlockAll") : TEXT("NoCollision"));
			Component->SetCanEverAffectNavigation(Spec.bCollides);
			Component->SetCastShadow(Spec.Material != EBHBlockMaterial::FogSheet);
			Component->SetReceivesDecals(Spec.Material != EBHBlockMaterial::FogSheet);
			Component->SetTranslucentSortPriority(Spec.Material == EBHBlockMaterial::FogSheet ? 10 : 0);
			Component->SetHiddenInGame(Spec.bHidden);
			Component->SetVisibility(!Spec.bHidden, true);
			ApplyMaterial(Component, Spec);
			AddInstanceComponent(Component);
			Component->RegisterComponent();
			ComponentsByKey.Add(Key, Component);
			GeneratedComponents.Add(Component);
		}

		Component->AddInstance(FTransform(Spec.Rotation, Spec.Location, Spec.Scale), true);
	}
}

void ABHStaticBlockField::ClearGeneratedComponents()
{
	for (UInstancedStaticMeshComponent* Component : GeneratedComponents)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		Component->ClearInstances();
		Component->DestroyComponent();
	}

	GeneratedComponents.Reset();
}

UMaterialInterface* ABHStaticBlockField::ResolveBaseMaterial(EBHBlockMaterial Material) const
{
	const TCHAR* MaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	switch (Material)
	{
	case EBHBlockMaterial::Concrete:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Concrete.M_BH_Concrete");
		break;
	case EBHBlockMaterial::Plaster:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Plaster.M_BH_Plaster");
		break;
	case EBHBlockMaterial::ConcreteWA:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_ConcreteWA.M_BH_ConcreteWA");
		break;
	case EBHBlockMaterial::ConcreteWACool:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_ConcreteWA_Cool.M_BH_ConcreteWA_Cool");
		break;
	case EBHBlockMaterial::PlasterWA:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_PlasterWA.M_BH_PlasterWA");
		break;
	case EBHBlockMaterial::BluePanel:
		MaterialPath = TEXT("/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel1.MI_ScreenPanel1");
		break;
	case EBHBlockMaterial::RustedMetal:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_RustedMetal.M_BH_RustedMetal");
		break;
	case EBHBlockMaterial::DiamondPlate:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_DiamondPlate.M_BH_DiamondPlate");
		break;
	case EBHBlockMaterial::PaintedMetal:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_PaintedMetal.M_BH_PaintedMetal");
		break;
	case EBHBlockMaterial::Tiles:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Tiles.M_BH_Tiles");
		break;
	case EBHBlockMaterial::WarningSign:
		MaterialPath = TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_WarningSign.M_BH_WarningSign");
		break;
	case EBHBlockMaterial::FogSheet:
		MaterialPath = TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent");
		break;
	case EBHBlockMaterial::Tinted:
	default:
		MaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
		break;
	}

	UMaterialInterface* LoadedMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	if (!LoadedMaterial && Material == EBHBlockMaterial::FogSheet)
	{
		LoadedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineVolumetrics/Fogsheet/Materials/M_EV_FogSheet_2sided_Master_Addi.M_EV_FogSheet_2sided_Master_Addi"));
	}
	return LoadedMaterial;
}

void ABHStaticBlockField::ApplyMaterial(UInstancedStaticMeshComponent* Component, const FBHStaticBlockSpec& Spec) const
{
	if (!Component)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = ResolveBaseMaterial(Spec.Material);
	if (!BaseMaterial)
	{
		return;
	}

	if (Spec.Material != EBHBlockMaterial::FogSheet && Spec.Material != EBHBlockMaterial::Tinted)
	{
		Component->SetMaterial(0, BaseMaterial);
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
	if (!DynamicMaterial)
	{
		Component->SetMaterial(0, BaseMaterial);
		return;
	}

	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Spec.Tint);
	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Spec.Tint);
	if (Spec.Material == EBHBlockMaterial::FogSheet)
	{
		const FLinearColor FogTint(Spec.Tint.R, Spec.Tint.G, Spec.Tint.B, FMath::Clamp(Spec.Tint.A, 0.0f, 1.0f));
		DynamicMaterial->SetVectorParameterValue(TEXT("Tint"), FogTint);
		DynamicMaterial->SetVectorParameterValue(TEXT("FogColor"), FogTint);
		DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), FogTint * 0.12f);
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), FogTint.A);
		DynamicMaterial->SetScalarParameterValue(TEXT("Alpha"), FogTint.A);
		DynamicMaterial->SetScalarParameterValue(TEXT("Density"), FMath::Lerp(0.45f, 1.8f, FogTint.A));
		DynamicMaterial->SetScalarParameterValue(TEXT("Brightness"), 0.85f);
	}

	Component->SetMaterial(0, DynamicMaterial);
}
