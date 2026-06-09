// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainSecretSwitch.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float BHPartySeconds = 30.0f;
	constexpr float BHPartyCooldown = 60.0f;

	FLinearColor PartyHSV(float HueDeg)
	{
		return FLinearColor(FMath::Fmod(HueDeg < 0.0f ? HueDeg + 360.0f : HueDeg, 360.0f), 0.95f, 1.0f).HSVToLinearRGB();
	}
}

ABHTrainSecretSwitch::ABHTrainSecretSwitch()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.06f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("?"));

	Lever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lever"));
	Lever->SetupAttachment(SceneRoot);
	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(SceneRoot);

	BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLight"));
	BeaconLight->SetupAttachment(SceneRoot);
	BeaconLight->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	BeaconLight->SetIntensity(0.0f);
	BeaconLight->SetAttenuationRadius(900.0f);
	BeaconLight->SetCastShadows(false);

	ApplyVisuals();
}

void ABHTrainSecretSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainSecretSwitch, PartyEndServerTime);
}

float ABHTrainSecretSwitch::GetServerTimeSeconds() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ABHTrainSecretSwitch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetServerTimeSeconds();
	const bool bParty = Now < PartyEndServerTime;
	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const FLinearColor Color = bParty ? PartyHSV(T * 160.0f) : FLinearColor(0.25f, 0.25f, 0.28f, 1.0f);
	if (Beacon)
	{
		BHPropVisuals::TintPart(Beacon, Color, bParty ? 1.0f : 0.1f);
	}
	if (BeaconLight)
	{
		BeaconLight->SetLightColor(Color);
		BeaconLight->SetIntensity(bParty ? (90.0f + 40.0f * FMath::Sin(T * 9.0f)) : 0.0f);
	}
}

bool ABHTrainSecretSwitch::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainSecretSwitch::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	const float Now = GetServerTimeSeconds();
	if (Now < NextAllowedTime)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(TEXT("The switch is warm... it won't flip again just yet."), 2.0f);
		}
		return;
	}

	PartyEndServerTime = Now + BHPartySeconds;
	NextAllowedTime = Now + BHPartyCooldown;
	ForceNetUpdate();

	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	const FString Who = BHPS ? BHPS->GetPlayerName() : FString(TEXT("Someone"));
	const FString Toast = FString::Printf(TEXT("%s found the secret switch -- PARTY ON THE TRAIN!"), *Who);
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
			{
				PC->ClientShowStatusMessage(Toast, 4.0f);
			}
		}
	}
}

FText ABHTrainSecretSwitch::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Flip the mysterious switch"));
}

void ABHTrainSecretSwitch::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.30f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 24.0f));
	}
	// A small wall plate + lever, and a little beacon bulb on top.
	BHPropVisuals::ConfigurePart(Lever, BHPropVisuals::CubeMesh(), BHPropVisuals::RustedMetalMaterial(), FVector(0.0f, 0.0f, 22.0f), FRotator(20.0f, 0.0f, 0.0f), FVector(0.06f, 0.06f, 0.26f), false);
	BHPropVisuals::ConfigurePart(Beacon, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 52.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.10f), false);
	BHPropVisuals::TintPart(Lever, FLinearColor(0.6f, 0.1f, 0.1f, 1.0f));
	BHPropVisuals::TintPart(Beacon, FLinearColor(0.25f, 0.25f, 0.28f, 1.0f), 0.1f);
}
