// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainVendingMachine.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr int32 BHVendCount = 8;
	const TCHAR* BHVendNames[BHVendCount] = {
		TEXT("Cola"), TEXT("Lemon-Lime"), TEXT("Grape Soda"), TEXT("Energy Drink"),
		TEXT("Iced Coffee"), TEXT("Spring Water"), TEXT("Chips"), TEXT("Candy Bar") };
	const FLinearColor BHVendColors[BHVendCount] = {
		FLinearColor(0.7f, 0.1f, 0.1f, 1.0f), FLinearColor(0.6f, 0.85f, 0.2f, 1.0f),
		FLinearColor(0.5f, 0.2f, 0.7f, 1.0f), FLinearColor(0.1f, 0.8f, 0.7f, 1.0f),
		FLinearColor(0.35f, 0.22f, 0.12f, 1.0f), FLinearColor(0.4f, 0.7f, 0.95f, 1.0f),
		FLinearColor(0.9f, 0.7f, 0.2f, 1.0f), FLinearColor(0.6f, 0.3f, 0.15f, 1.0f) };
}

ABHTrainVendingMachine::ABHTrainVendingMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Vending Machine"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Glass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glass"));
	Glass->SetupAttachment(SceneRoot);

	Products.Reserve(BHVendCount);
	for (int32 Index = 0; Index < BHVendCount; ++Index)
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Product_%d"), Index));
		P->SetupAttachment(SceneRoot);
		Products.Add(P);
	}

	TrayItem = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrayItem"));
	TrayItem->SetupAttachment(SceneRoot);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(SceneRoot);
	Glow->SetRelativeLocation(FVector(0.0f, -30.0f, 130.0f));
	Glow->SetIntensity(45.0f);
	Glow->SetAttenuationRadius(280.0f);
	Glow->SetLightColor(FLinearColor(0.7f, 0.85f, 1.0f, 1.0f));
	Glow->SetCastShadows(false);

	ApplyVisuals();
}

void ABHTrainVendingMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainVendingMachine, LastItemIndex);
	DOREPLIFETIME(ABHTrainVendingMachine, DispenseServerTime);
}

float ABHTrainVendingMachine::GetServerTimeSeconds() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ABHTrainVendingMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!TrayItem)
	{
		return;
	}
	const bool bRecent = (LastItemIndex >= 0) && (GetServerTimeSeconds() - DispenseServerTime < 6.0f);
	BHPropVisuals::SetPartVisible(TrayItem, bRecent);
	if (bRecent)
	{
		// Tint only when the dispensed item changes (the colour is static for the whole 6s window).
		if (CachedTintIndex != LastItemIndex)
		{
			CachedTintIndex = LastItemIndex;
			BHPropVisuals::TintPart(TrayItem, BHVendColors[FMath::Clamp(LastItemIndex, 0, BHVendCount - 1)], 0.3f);
		}
	}
	else
	{
		CachedTintIndex = -1;
	}
}

bool ABHTrainVendingMachine::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainVendingMachine::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	LastItemIndex = FMath::RandRange(0, BHVendCount - 1);
	DispenseServerTime = GetServerTimeSeconds();
	ForceNetUpdate();
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(FString::Printf(TEXT("*clunk* You grabbed a %s."), BHVendNames[LastItemIndex]), 2.5f);
	}
}

FText ABHTrainVendingMachine::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Grab a snack"));
}

FBHInteractionPromptInfo ABHTrainVendingMachine::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("FREE SNACKS"));
	}
	return Info;
}

void ABHTrainVendingMachine::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetRelativeScale3D(FVector(0.62f, 0.5f, 1.7f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f));
	}
	BHPropVisuals::ConfigurePart(Body, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 88.0f), FRotator::ZeroRotator, FVector(0.62f, 0.5f, 1.7f), false);
	BHPropVisuals::ConfigurePart(Glass, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -26.0f, 110.0f), FRotator::ZeroRotator, FVector(0.5f, 0.03f, 1.05f), false);
	BHPropVisuals::TintPart(Body, FLinearColor(0.15f, 0.30f, 0.55f, 1.0f), 0.05f);
	BHPropVisuals::TintPart(Glass, FLinearColor(0.02f, 0.03f, 0.05f, 1.0f), 0.0f);

	// Product rows behind the glass (2 columns x 4 rows).
	for (int32 Index = 0; Index < Products.Num(); ++Index)
	{
		const int32 Col = Index % 2;
		const int32 Row = Index / 2;
		const FVector Loc(-12.0f + Col * 24.0f, -22.0f, 70.0f + Row * 26.0f);
		BHPropVisuals::ConfigurePart(Products[Index], BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), Loc, FRotator::ZeroRotator, FVector(0.08f, 0.06f, 0.12f), false);
		BHPropVisuals::TintPart(Products[Index], BHVendColors[Index % BHVendCount], 0.25f);
	}

	// Dispense tray can (hidden until you grab something).
	BHPropVisuals::ConfigurePart(TrayItem, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -22.0f, 26.0f), FRotator::ZeroRotator, FVector(0.06f, 0.06f, 0.10f), false);
	BHPropVisuals::SetPartVisible(TrayItem, false);
}
