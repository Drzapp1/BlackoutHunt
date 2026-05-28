#include "BHInteractableActor.h"
#include "BHCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABHInteractableActor::ABHInteractableActor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(4.0f);
	SetMinNetUpdateFrequency(0.5f);
	NetPriority = 0.8f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	InteractionLabel = FText::FromString(TEXT("Interact"));
}

bool ABHInteractableActor::CanInteract_Implementation(ABHCharacter* Character) const
{
	return IsValid(Character);
}

void ABHInteractableActor::BeginInteract_Implementation(ABHCharacter* Character)
{
}

void ABHInteractableActor::EndInteract_Implementation(ABHCharacter* Character)
{
}

FText ABHInteractableActor::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return InteractionLabel;
}

FBHInteractionPromptInfo ABHInteractableActor::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	if (!Info.bCanInteract)
	{
		Info.DisabledReason = FText::FromString(TEXT("LOCKED"));
	}
	return Info;
}
