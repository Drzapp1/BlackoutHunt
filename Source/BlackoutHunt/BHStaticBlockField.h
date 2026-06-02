// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHBlockActor.h"
#include "GameFramework/Actor.h"
#include "BHStaticBlockField.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FBHStaticBlockSpec
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FVector Scale = FVector::OneVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY()
	EBHBlockMaterial Material = EBHBlockMaterial::Tinted;

	UPROPERTY()
	bool bCollides = true;

	UPROPERTY()
	bool bHidden = false;
};

UCLASS()
class BLACKOUTHUNT_API ABHStaticBlockField : public AActor
{
	GENERATED_BODY()

public:
	ABHStaticBlockField();

	void SetBlockSpecs(const TArray<FBHStaticBlockSpec>& InSpecs);
	void AddBlockSpec(const FBHStaticBlockSpec& Spec);
	void ResetBlockSpecs();
	void FinalizeBuild();

	int32 GetStaticBlockCount() const { return StaticBlockSpecs.Num(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_StaticBlockSpecs)
	TArray<FBHStaticBlockSpec> StaticBlockSpecs;

	UPROPERTY(ReplicatedUsing = OnRep_StaticBlockSpecs)
	int32 BuildVersion = 0;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BlockMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> GeneratedComponents;

	UFUNCTION()
	void OnRep_StaticBlockSpecs();

	void RebuildComponents();
	void ClearGeneratedComponents();
	UMaterialInterface* ResolveBaseMaterial(EBHBlockMaterial Material) const;
	void ApplyMaterial(UInstancedStaticMeshComponent* Component, const FBHStaticBlockSpec& Spec) const;
};
