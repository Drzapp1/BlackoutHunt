// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainFireplace.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr int32 BHFireFlames = 4;
	const FLinearColor FireWarm(1.0f, 0.55f, 0.18f, 1.0f);
}

ABHTrainFireplace::ABHTrainFireplace()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Fireplace"));

	Hearth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hearth"));
	Hearth->SetupAttachment(SceneRoot);
	Logs = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Logs"));
	Logs->SetupAttachment(SceneRoot);

	Flames.Reserve(BHFireFlames);
	for (int32 Index = 0; Index < BHFireFlames; ++Index)
	{
		UStaticMeshComponent* Flame = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Flame_%d"), Index));
		Flame->SetupAttachment(SceneRoot);
		Flames.Add(Flame);
	}

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(SceneRoot);
	FireLight->SetRelativeLocation(FVector(0.0f, -20.0f, 60.0f));
	FireLight->SetIntensity(90.0f);
	FireLight->SetAttenuationRadius(650.0f);
	FireLight->SetLightColor(FireWarm);
	FireLight->SetCastShadows(false);

	ApplyVisuals();
}

void ABHTrainFireplace::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainFireplace, bLit);
}

void ABHTrainFireplace::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		if (!Flames[Index])
		{
			continue;
		}
		if (!bLit)
		{
			BHPropVisuals::SetPartVisible(Flames[Index], false);
			continue;
		}
		BHPropVisuals::SetPartVisible(Flames[Index], true);
		const float Phase = Index * 1.7f;
		const float Flick = 0.7f + 0.3f * FMath::Sin(T * 11.0f + Phase) * FMath::Sin(T * 6.3f + Phase * 2.0f);
		const float H = 0.30f + 0.18f * Flick + 0.05f * Index;
		const float Sway = 3.0f * FMath::Sin(T * 4.0f + Phase);
		Flames[Index]->SetRelativeLocation(FVector(Sway + (Index - 1.5f) * 9.0f, -18.0f, 40.0f + H * 50.0f));
		Flames[Index]->SetRelativeScale3D(FVector(0.07f, 0.07f, FMath::Max(0.05f, H)));
		BHPropVisuals::TintPart(Flames[Index], FLinearColor(1.0f, 0.4f + 0.3f * Flick, 0.12f, 1.0f), 1.0f);
	}

	if (FireLight)
	{
		const float Flick = 0.8f + 0.2f * FMath::Sin(T * 13.0f) * FMath::Sin(T * 7.7f);
		FireLight->SetIntensity(bLit ? 70.0f + 50.0f * Flick : 0.0f);
	}
}

bool ABHTrainFireplace::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainFireplace::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	bLit = !bLit;
	ForceNetUpdate();
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(bLit ? TEXT("You stoke the fire. It crackles warmly.") : TEXT("You douse the fire."), 2.0f);
	}
}

FText ABHTrainFireplace::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(bLit ? TEXT("Douse the fire") : TEXT("Light the fire"));
}

void ABHTrainFireplace::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetRelativeScale3D(FVector(1.0f, 0.5f, 0.9f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	}
	// Stone surround + a back panel, with a couple of logs in the firebox.
	BHPropVisuals::ConfigurePart(Hearth, BHPropVisuals::CubeMesh(), BHPropVisuals::MarbleMaterial(), FVector(0.0f, 0.0f, 45.0f), FRotator::ZeroRotator, FVector(1.0f, 0.5f, 0.92f), true);
	BHPropVisuals::ConfigurePart(Logs, BHPropVisuals::CylinderMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, -18.0f, 30.0f), FRotator(0.0f, 0.0f, 90.0f), FVector(0.10f, 0.10f, 0.5f), false);
	BHPropVisuals::TintPart(Hearth, FLinearColor(0.55f, 0.54f, 0.52f, 1.0f));
	BHPropVisuals::TintPart(Logs, FLinearColor(0.20f, 0.11f, 0.05f, 1.0f));

	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(Flames[Index], BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector((Index - 1.5f) * 9.0f, -18.0f, 45.0f), FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.12f), false);
		BHPropVisuals::TintPart(Flames[Index], FireWarm, 1.0f);
	}
}
