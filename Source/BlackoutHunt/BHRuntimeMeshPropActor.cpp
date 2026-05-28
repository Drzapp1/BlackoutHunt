#include "BHRuntimeMeshPropActor.h"

#include "BHPropVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

ABHRuntimeMeshPropActor::ABHRuntimeMeshPropActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(0.5f);
	SetMinNetUpdateFrequency(0.1f);
	NetPriority = 0.5f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCanEverAffectNavigation(true);

	MeshScale = FVector::OneVector;
	bCollisionEnabled = true;
	ApplyPropVisuals();
}

void ABHRuntimeMeshPropActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABHRuntimeMeshPropActor, MeshAssetPath);
	DOREPLIFETIME(ABHRuntimeMeshPropActor, MaterialAssetPath);
	DOREPLIFETIME(ABHRuntimeMeshPropActor, MeshScale);
	DOREPLIFETIME(ABHRuntimeMeshPropActor, bCollisionEnabled);
}

void ABHRuntimeMeshPropActor::ConfigureProp(
	const FString& NewMeshAssetPath,
	const FString& NewMaterialAssetPath,
	const FVector& NewMeshScale,
	bool bNewCollisionEnabled)
{
	MeshAssetPath = NewMeshAssetPath;
	MaterialAssetPath = NewMaterialAssetPath;
	MeshScale = NewMeshScale;
	bCollisionEnabled = bNewCollisionEnabled;
	ApplyPropVisuals();
	ForceNetUpdate();
}

void ABHRuntimeMeshPropActor::OnRep_PropVisuals()
{
	ApplyPropVisuals();
}

void ABHRuntimeMeshPropActor::ApplyPropVisuals()
{
	if (!Mesh)
	{
		return;
	}

	UStaticMesh* MeshAsset = MeshAssetPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *MeshAssetPath);
	const bool bUsingImportedMesh = MeshAsset != nullptr;
	if (!MeshAsset)
	{
		MeshAsset = BHPropVisuals::CubeMesh();
	}

	Mesh->SetStaticMesh(MeshAsset);
	Mesh->SetRelativeScale3D(MeshScale);
	Mesh->SetCollisionEnabled(bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Mesh->SetCollisionResponseToAllChannels(bCollisionEnabled ? ECR_Block : ECR_Ignore);
	Mesh->SetCanEverAffectNavigation(bCollisionEnabled);
	Mesh->SetGenerateOverlapEvents(false);

	if (!MaterialAssetPath.IsEmpty())
	{
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialAssetPath))
		{
			const int32 MaterialSlots = FMath::Max(1, Mesh->GetNumMaterials());
			for (int32 Slot = 0; Slot < MaterialSlots; ++Slot)
			{
				Mesh->SetMaterial(Slot, Material);
			}
			return;
		}
	}

	if (!bUsingImportedMesh)
	{
		Mesh->SetMaterial(0, BHPropVisuals::PaintedMetalMaterial());
	}
}
