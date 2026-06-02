// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHStudentScareSwitch.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

ABHStudentScareSwitch::ABHStudentScareSwitch()
{
	Severity = 1;
	ScareTitle = TEXT("Shadow Flicker");
	CooldownEndServerTime = 0.0f;
	CooldownSeconds = 60.0f;
	InteractionLabel = FText::FromString(TEXT("Shadow Flicker 1/4"));
	SetActorScale3D(FVector::OneVector);

	SwitchPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchPlate"));
	SwitchPlate->SetupAttachment(RootComponent);
	SwitchLever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchLever"));
	SwitchLever->SetupAttachment(RootComponent);
	SeverityLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SeverityLight"));
	SeverityLight->SetupAttachment(RootComponent);
	CooldownLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CooldownLight"));
	CooldownLight->SetupAttachment(RootComponent);
	TitleBadge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TitleBadge"));
	TitleBadge->SetupAttachment(RootComponent);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.56f, 0.16f, 0.74f), true);
	BHPropVisuals::ConfigurePart(SwitchPlate, BHPropVisuals::CubeMesh(), BHPropVisuals::DiamondPlateMaterial(), FVector(30.0f, 0.0f, -3.0f), FRotator::ZeroRotator, FVector(0.024f, 0.120f, 0.48f));
	BHPropVisuals::ConfigurePart(SwitchLever, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(34.0f, 0.0f, -7.0f), FRotator(0.0f, 0.0f, -22.0f), FVector(0.035f, 0.045f, 0.31f));
	BHPropVisuals::ConfigurePart(SeverityLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(35.5f, -11.0f, 26.0f), FRotator::ZeroRotator, FVector(0.052f));
	BHPropVisuals::ConfigurePart(CooldownLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(35.5f, 11.0f, 26.0f), FRotator::ZeroRotator, FVector(0.044f));
	BHPropVisuals::ConfigurePart(TitleBadge, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(35.0f, 0.0f, -32.0f), FRotator::ZeroRotator, FVector(0.018f, 0.105f, 0.052f));

	BHPropVisuals::TintPart(SwitchLever, FLinearColor(0.05f, 0.052f, 0.056f, 1.0f));
	BHPropVisuals::TintPart(TitleBadge, FLinearColor(0.16f, 0.20f, 0.22f, 1.0f));
	ApplyScareVisuals();
}

void ABHStudentScareSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHStudentScareSwitch, Severity);
	DOREPLIFETIME(ABHStudentScareSwitch, ScareTitle);
	DOREPLIFETIME(ABHStudentScareSwitch, CooldownEndServerTime);
	DOREPLIFETIME(ABHStudentScareSwitch, CooldownSeconds);
}

void ABHStudentScareSwitch::Configure(int32 NewSeverity, const FString& NewScareTitle, float NewCooldownSeconds)
{
	Severity = FMath::Clamp(NewSeverity, 1, 4);
	ScareTitle = NewScareTitle.IsEmpty() ? TEXT("Scare") : NewScareTitle.Left(24);
	CooldownSeconds = FMath::Clamp(NewCooldownSeconds, 45.0f, 90.0f);
	InteractionLabel = GetInteractionLabel_Implementation(nullptr);
	ApplyScareVisuals();
}

bool ABHStudentScareSwitch::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	return BHPS
		&& BHPS->LifeState == EBHPlayerLifeState::Alive
		&& (BHPS->PlayerRole == EBHPlayerRole::Survivor
			|| BHPS->PlayerRole == EBHPlayerRole::FakeHunter
			|| BHPS->PlayerRole == EBHPlayerRole::Tester);
}

void ABHStudentScareSwitch::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character || !GetWorld())
	{
		return;
	}

	ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController());
	const float RemainingCooldown = GetRemainingCooldownSeconds();
	if (RemainingCooldown > 0.0f)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("%s cooling down: %ds."), *ScareTitle, FMath::CeilToInt(RemainingCooldown)), 1.8f);
		}
		return;
	}

	ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>();
	if (!BHGM)
	{
		return;
	}

	if (BHGM->TriggerStudentScareSwitch(Character, GetActorLocation(), ScareTitle, Severity))
	{
		const AGameStateBase* GameStateBase = GetWorld()->GetGameState();
		const float Now = GameStateBase ? GameStateBase->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		CooldownEndServerTime = Now + CooldownSeconds;
		ApplyScareVisuals();
		ForceNetUpdate();
	}
}

FText ABHStudentScareSwitch::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const float RemainingCooldown = GetRemainingCooldownSeconds();
	if (RemainingCooldown > 0.0f)
	{
		return FText::FromString(FString::Printf(TEXT("%s %d/4 (%ds)"), *ScareTitle, FMath::Clamp(Severity, 1, 4), FMath::CeilToInt(RemainingCooldown)));
	}

	return FText::FromString(FString::Printf(TEXT("%s %d/4"), *ScareTitle, FMath::Clamp(Severity, 1, 4)));
}

void ABHStudentScareSwitch::OnRep_ScareState()
{
	InteractionLabel = GetInteractionLabel_Implementation(nullptr);
	ApplyScareVisuals();
}

void ABHStudentScareSwitch::ApplyScareVisuals()
{
	FLinearColor SeverityTint = FLinearColor(0.20f, 0.88f, 0.56f, 1.0f);
	if (Severity == 2)
	{
		SeverityTint = FLinearColor(0.92f, 0.76f, 0.18f, 1.0f);
	}
	else if (Severity == 3)
	{
		SeverityTint = FLinearColor(0.95f, 0.36f, 0.13f, 1.0f);
	}
	else if (Severity >= 4)
	{
		SeverityTint = FLinearColor(1.0f, 0.06f, 0.035f, 1.0f);
	}

	const bool bCoolingDown = GetRemainingCooldownSeconds() > 0.0f;
	BHPropVisuals::TintPart(SeverityLight, SeverityTint, 1.6f + static_cast<float>(FMath::Clamp(Severity, 1, 4)) * 0.35f);
	BHPropVisuals::TintPart(CooldownLight, bCoolingDown ? FLinearColor(0.86f, 0.28f, 0.08f, 1.0f) : FLinearColor(0.16f, 0.85f, 0.64f, 1.0f), bCoolingDown ? 2.0f : 1.4f);
}

float ABHStudentScareSwitch::GetRemainingCooldownSeconds() const
{
	if (!GetWorld() || CooldownEndServerTime <= 0.0f)
	{
		return 0.0f;
	}

	const AGameStateBase* GameStateBase = GetWorld()->GetGameState();
	const float Now = GameStateBase ? GameStateBase->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, CooldownEndServerTime - Now);
}
