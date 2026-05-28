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
	NetUpdateFrequency = 1.0f;
	MinNetUpdateFrequency = 0.5f;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

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

	for (int32 Index = 0; Index < StaticBlockSpecs.Num(); ++Index)
	{
		const FBHStaticBlockSpec& Spec = StaticBlockSpecs[Index];
		const FName ComponentName = MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(), TEXT("StaticBlock"));
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, ComponentName);
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
		Component->AddInstance(FTransform(Spec.Rotation, Spec.Location, Spec.Scale), true);
		GeneratedComponents.Add(Component);
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
