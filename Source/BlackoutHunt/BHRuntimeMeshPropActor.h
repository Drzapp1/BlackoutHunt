#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHRuntimeMeshPropActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class BLACKOUTHUNT_API ABHRuntimeMeshPropActor : public AActor
{
	GENERATED_BODY()

public:
	ABHRuntimeMeshPropActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ConfigureProp(
		const FString& NewMeshAssetPath,
		const FString& NewMaterialAssetPath,
		const FVector& NewMeshScale,
		bool bNewCollisionEnabled);

protected:
	UFUNCTION()
	void OnRep_PropVisuals();

	void ApplyPropVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_PropVisuals)
	FString MeshAssetPath;

	UPROPERTY(ReplicatedUsing = OnRep_PropVisuals)
	FString MaterialAssetPath;

	UPROPERTY(ReplicatedUsing = OnRep_PropVisuals)
	FVector MeshScale;

	UPROPERTY(ReplicatedUsing = OnRep_PropVisuals)
	bool bCollisionEnabled;
};
