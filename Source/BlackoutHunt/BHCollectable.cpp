// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHCollectable.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ABHCollectable::ABHCollectable()
{
	InteractionLabel = FText::FromString(TEXT("Collect Relic"));
	bCollected = false;
	SetActorScale3D(FVector::OneVector);

	Gem = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gem"));
	Gem->SetupAttachment(RootComponent);

	// A small floating relic (tilted plinth + glowing gem) built from primitive meshes -- no content assets needed.
	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector::ZeroVector, FRotator(45.0f, 0.0f, 45.0f), FVector(0.16f, 0.16f, 0.16f), true);
	BHPropVisuals::ConfigurePart(Gem, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 24.0f), FRotator::ZeroRotator, FVector(0.12f));
	ApplyCollectableVisuals();
}

void ABHCollectable::Configure(FName InCollectableId)
{
	CollectableId = InCollectableId;
}

void ABHCollectable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHCollectable, bCollected);
}

bool ABHCollectable::CanInteract_Implementation(ABHCharacter* Character) const
{
	return !bCollected && IsValid(Character);
}

void ABHCollectable::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !CanInteract_Implementation(Character))
	{
		return;
	}

	bCollected = true;
	ApplyCollectedState();
	// Account progress is client-local, so the server asks the owning client to record the find.
	Character->ClientRecordCollectable(CollectableId, TEXT("Relic collected!"));
}

FText ABHCollectable::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return bCollected ? FText::FromString(TEXT("Collected")) : FText::FromString(TEXT("Collect Relic"));
}

void ABHCollectable::OnRep_Collected()
{
	ApplyCollectedState();
}

void ABHCollectable::ApplyCollectedState()
{
	ApplyCollectableVisuals();
	SetActorHiddenInGame(bCollected);
	SetActorEnableCollision(!bCollected);
}

void ABHCollectable::ApplyCollectableVisuals()
{
	BHPropVisuals::TintPart(Mesh, FLinearColor(0.92f, 0.74f, 0.20f, 1.0f), 1.6f);   // amber/gold plinth
	BHPropVisuals::TintPart(Gem, FLinearColor(0.40f, 0.90f, 0.95f, 1.0f), 2.6f);    // glowing cyan gem
}
