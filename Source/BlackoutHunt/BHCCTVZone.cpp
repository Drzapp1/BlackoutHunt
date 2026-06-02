// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHCCTVZone.h"

#include "BHCharacter.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "BHSecurityCamera.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ABHCCTVZone::ABHCCTVZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.35f;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(3.0f);
	SetMinNetUpdateFrequency(0.5f);

	ZoneTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneTrigger"));
	SetRootComponent(ZoneTrigger);
	ZoneTrigger->InitBoxExtent(FVector(1200.0f, 650.0f, 120.0f));
	ZoneTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	ZoneTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ZoneTrigger->SetGenerateOverlapEvents(true);
	ZoneTrigger->SetCanEverAffectNavigation(false);

	ZonePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZonePlate"));
	ZonePlate->SetupAttachment(ZoneTrigger);
	WarningStripeA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripeA"));
	WarningStripeA->SetupAttachment(ZoneTrigger);
	WarningStripeB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripeB"));
	WarningStripeB->SetupAttachment(ZoneTrigger);
	WarningStripeC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripeC"));
	WarningStripeC->SetupAttachment(ZoneTrigger);
	WarningStripeD = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripeD"));
	WarningStripeD->SetupAttachment(ZoneTrigger);

	LinkedCamera = nullptr;
	bZoneEnabled = true;
	bZoneVisible = true;
	bRoundOffline = false;
	CircuitId = 0;
	AlertLabel = TEXT("CCTV zone");

	RefreshMarkerScale(ZoneTrigger->GetUnscaledBoxExtent());
	ApplyZoneVisuals();
}

void ABHCCTVZone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && ZoneTrigger)
	{
		ZoneTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABHCCTVZone::OnZoneBeginOverlap);
	}

	SetActorTickEnabled(HasAuthority());
	ApplyZoneVisuals();
}

void ABHCCTVZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ZoneTrigger)
	{
		ZoneTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &ABHCCTVZone::OnZoneBeginOverlap);
	}

	Super::EndPlay(EndPlayReason);
}

void ABHCCTVZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bZoneEnabled || !ZoneTrigger)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	ZoneTrigger->GetOverlappingActors(OverlappingActors, ABHCharacter::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryAlertForActor(OverlappingActor);
	}
}

void ABHCCTVZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABHCCTVZone, LinkedCamera);
	DOREPLIFETIME(ABHCCTVZone, bZoneEnabled);
	DOREPLIFETIME(ABHCCTVZone, bZoneVisible);
	DOREPLIFETIME(ABHCCTVZone, CircuitId);
	DOREPLIFETIME(ABHCCTVZone, AlertLabel);
}

void ABHCCTVZone::ConfigureZone(ABHSecurityCamera* NewCamera, int32 NewCircuitId, const FString& NewAlertLabel, const FVector& NewBoxExtent, bool bNewVisible)
{
	LinkedCamera = NewCamera;
	CircuitId = NewCircuitId;
	AlertLabel = NewAlertLabel.IsEmpty() ? TEXT("CCTV zone") : NewAlertLabel;
	bZoneVisible = bNewVisible;

	const FVector SafeExtent(
		FMath::Max(240.0f, NewBoxExtent.X),
		FMath::Max(180.0f, NewBoxExtent.Y),
		FMath::Max(80.0f, NewBoxExtent.Z));
	if (ZoneTrigger)
	{
		ZoneTrigger->SetBoxExtent(SafeExtent);
	}
	RefreshMarkerScale(SafeExtent);
	ApplyZoneVisuals();
	ForceNetUpdate();
}

void ABHCCTVZone::SetZoneEnabled(bool bNewEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bZoneEnabled = bNewEnabled && !bRoundOffline;
	if (ZoneTrigger)
	{
		ZoneTrigger->SetGenerateOverlapEvents(bZoneEnabled);
		ZoneTrigger->SetCollisionEnabled(bZoneEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	ApplyZoneVisuals();
	ForceNetUpdate();
}

void ABHCCTVZone::SetRoundOffline(bool bNewRoundOffline)
{
	if (!HasAuthority())
	{
		return;
	}

	bRoundOffline = bNewRoundOffline;
	SetZoneEnabled(!bRoundOffline);
}

void ABHCCTVZone::SetZoneVisible(bool bNewVisible)
{
	if (!HasAuthority())
	{
		return;
	}

	bZoneVisible = bNewVisible;
	ApplyZoneVisuals();
	ForceNetUpdate();
}

int32 ABHCCTVZone::GetCircuitId() const
{
	return CircuitId;
}

bool ABHCCTVZone::IsZoneEnabled() const
{
	return bZoneEnabled;
}

bool ABHCCTVZone::IsRoundOffline() const
{
	return bRoundOffline;
}

ABHSecurityCamera* ABHCCTVZone::GetLinkedCamera() const
{
	return LinkedCamera;
}

void ABHCCTVZone::OnRep_ZoneState()
{
	ApplyZoneVisuals();
}

void ABHCCTVZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryAlertForActor(OtherActor);
}

void ABHCCTVZone::TryAlertForActor(AActor* OtherActor)
{
	if (!HasAuthority() || !bZoneEnabled || !LinkedCamera)
	{
		return;
	}

	ABHCharacter* Survivor = Cast<ABHCharacter>(OtherActor);
	const ABHPlayerState* SurvivorPS = Survivor ? Survivor->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!Survivor || !SurvivorPS || !SurvivorPS->IsAliveSurvivor() || Survivor->IsHiddenInLocker())
	{
		return;
	}

	LinkedCamera->TryTriggerZoneAlert(Survivor, this, AlertLabel);
}

void ABHCCTVZone::ApplyZoneVisuals()
{
	if (ZoneTrigger)
	{
		ZoneTrigger->SetGenerateOverlapEvents(bZoneEnabled);
		ZoneTrigger->SetCollisionEnabled(bZoneEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	const bool bShowOutline = bZoneVisible;
	const FLinearColor PlateColor(0.012f, 0.018f, 0.018f, 0.0f);
	// Enabled = alarming teal that glows; disabled = a cool slate that is dim but still clearly
	// readable, so "offline" reads as a deliberate state from a distance instead of the stripes
	// nearly vanishing (which players misread as the zone never having been there).
	const FLinearColor StripeColor = bZoneEnabled
		? FLinearColor(0.26f, 0.58f, 0.54f, 0.72f)
		: FLinearColor(0.20f, 0.24f, 0.30f, 0.66f);
	const float StripeGlow = bZoneEnabled ? 0.32f : 0.06f;

	BHPropVisuals::SetPartVisible(ZonePlate, false);
	BHPropVisuals::SetPartVisible(WarningStripeA, bShowOutline);
	BHPropVisuals::SetPartVisible(WarningStripeB, bShowOutline);
	BHPropVisuals::SetPartVisible(WarningStripeC, bShowOutline);
	BHPropVisuals::SetPartVisible(WarningStripeD, bShowOutline);

	BHPropVisuals::TintPart(ZonePlate, PlateColor, 0.0f);
	BHPropVisuals::TintPart(WarningStripeA, StripeColor, StripeGlow);
	BHPropVisuals::TintPart(WarningStripeB, StripeColor, StripeGlow);
	BHPropVisuals::TintPart(WarningStripeC, StripeColor, StripeGlow);
	BHPropVisuals::TintPart(WarningStripeD, StripeColor, StripeGlow);
}

void ABHCCTVZone::RefreshMarkerScale(const FVector& BoxExtent)
{
	const float MarkerZ = -BoxExtent.Z + 3.0f;
	const float PlateScaleX = FMath::Max(0.08f, BoxExtent.X / 50.0f);
	const float PlateScaleY = FMath::Max(0.08f, BoxExtent.Y / 50.0f);
	const float StripeScaleX = FMath::Max(0.08f, BoxExtent.X / 50.0f);
	const float StripeScaleY = FMath::Max(0.08f, BoxExtent.Y / 50.0f);
	const float StripeWidth = 0.035f;
	const float StripeInset = 18.0f;

	BHPropVisuals::ConfigurePart(ZonePlate, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, MarkerZ), FRotator::ZeroRotator, FVector(PlateScaleX, PlateScaleY, 0.018f), false);
	BHPropVisuals::ConfigurePart(WarningStripeA, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, BoxExtent.Y - StripeInset, MarkerZ + 2.0f), FRotator::ZeroRotator, FVector(StripeScaleX, StripeWidth, 0.022f), false);
	BHPropVisuals::ConfigurePart(WarningStripeB, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -BoxExtent.Y + StripeInset, MarkerZ + 2.0f), FRotator::ZeroRotator, FVector(StripeScaleX, StripeWidth, 0.022f), false);
	BHPropVisuals::ConfigurePart(WarningStripeC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(BoxExtent.X - StripeInset, 0.0f, MarkerZ + 2.0f), FRotator::ZeroRotator, FVector(StripeWidth, StripeScaleY, 0.022f), false);
	BHPropVisuals::ConfigurePart(WarningStripeD, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(-BoxExtent.X + StripeInset, 0.0f, MarkerZ + 2.0f), FRotator::ZeroRotator, FVector(StripeWidth, StripeScaleY, 0.022f), false);
}
