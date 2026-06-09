// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainDartboard.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float BHDartHoldSeconds = 0.35f;
	constexpr float BHDartCenterZ = 145.0f;    // board centre height above the actor origin
	constexpr float BHDartBoardRadius = 46.0f;
	constexpr float BHDartSpread = 5.0f;        // +/- cm random scatter per throw
	constexpr int32 BHDartMaxMarkers = 10;
	constexpr int32 BHDartNumRings = 6;

	const FLinearColor DartHot(1.0f, 0.84f, 0.3f, 1.0f);
	const FLinearColor DartNeutral(0.55f, 0.78f, 1.0f, 1.0f);
	const FLinearColor DartMiss(0.7f, 0.4f, 0.35f, 1.0f);

	// Outer radius (cm) and tint for each ring, largest first.
	const float RingRadius[BHDartNumRings] = { 46.0f, 39.0f, 30.0f, 21.0f, 13.0f, 6.0f };
	const FLinearColor RingTint[BHDartNumRings] = {
		FLinearColor(0.05f, 0.05f, 0.05f, 1.0f),   // 1
		FLinearColor(0.85f, 0.82f, 0.70f, 1.0f),   // 5
		FLinearColor(0.10f, 0.10f, 0.10f, 1.0f),   // 10
		FLinearColor(0.80f, 0.78f, 0.66f, 1.0f),   // 15
		FLinearColor(0.70f, 0.10f, 0.10f, 1.0f),   // 20
		FLinearColor(0.10f, 0.45f, 0.18f, 1.0f),   // 25 (outer bull)
	};

	void BHConfigureDartText(UTextRenderComponent* TextComponent, const FVector& Location, float WorldSize, const FColor& Color)
	{
		BHPropVisuals::ConfigureReadableText(TextComponent, Location, FRotator(0.0f, 90.0f, 0.0f), WorldSize, Color);
	}
}

ABHTrainDartboard::ABHTrainDartboard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Darts"));

	Stand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stand"));
	Stand->SetupAttachment(SceneRoot);

	Rings.Reserve(BHDartNumRings + 1);
	for (int32 Index = 0; Index < BHDartNumRings + 1; ++Index)
	{
		UStaticMeshComponent* Ring = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Ring_%d"), Index));
		Ring->SetupAttachment(SceneRoot);
		Rings.Add(Ring);
	}

	DartMarkers.Reserve(BHDartMaxMarkers);
	for (int32 Index = 0; Index < BHDartMaxMarkers; ++Index)
	{
		UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Dart_%d"), Index));
		Marker->SetupAttachment(SceneRoot);
		DartMarkers.Add(Marker);
	}

	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(SceneRoot);
	BHConfigureDartText(TitleText, FVector(0.0f, -8.0f, 205.0f), 11.0f, FColor(255, 224, 120));

	StatusTextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusTextRender->SetupAttachment(SceneRoot);
	BHConfigureDartText(StatusTextRender, FVector(0.0f, -8.0f, 80.0f), 6.0f, FColor(206, 226, 244));

	BoardLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BoardLight"));
	BoardLight->SetupAttachment(SceneRoot);
	BoardLight->SetRelativeLocation(FVector(0.0f, -50.0f, BHDartCenterZ));
	BoardLight->SetIntensity(85.0f);
	BoardLight->SetAttenuationRadius(300.0f);
	BoardLight->SetLightColor(FLinearColor(0.95f, 0.92f, 0.85f, 1.0f));
	BoardLight->SetCastShadows(false);

	ApplyVisuals();
	RefreshVisuals();
}

void ABHTrainDartboard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshVisuals();
}

void ABHTrainDartboard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainDartboard, RecentDarts);
	DOREPLIFETIME(ABHTrainDartboard, ActiveThrowerId);
	DOREPLIFETIME(ABHTrainDartboard, ActiveTotal);
	DOREPLIFETIME(ABHTrainDartboard, ActiveThrows);
	DOREPLIFETIME(ABHTrainDartboard, LastScore);
	DOREPLIFETIME(ABHTrainDartboard, ActiveThrowerName);
	DOREPLIFETIME(ABHTrainDartboard, LastResultAccent);
}

bool ABHTrainDartboard::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainDartboard::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}
	// Reuse the actor's transient map only for tap/hold timing via a dynamic property would be overkill; we
	// instead record the press time on a per-actor scratch via the recent-darts path. Store nothing here.
}

void ABHTrainDartboard::EndInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	// Hold not needed for darts; a tap throws. (A long hold resets your tally, handled in ThrowDart.)
	ThrowDart(Character, false);
}

FText ABHTrainDartboard::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Throw a dart (aim + tap)"));
}

FBHInteractionPromptInfo ABHTrainDartboard::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("BULLSEYE = 50"));
	}
	return Info;
}

void ABHTrainDartboard::OnRep_State()
{
	RefreshVisuals();
}

float ABHTrainDartboard::GetServerTimeSeconds() const
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

int32 ABHTrainDartboard::ScoreForRadius(float RadiusCm)
{
	if (RadiusCm < 2.5f) { return 50; }   // inner bull
	if (RadiusCm < 6.0f) { return 25; }   // outer bull
	if (RadiusCm < 13.0f) { return 20; }
	if (RadiusCm < 21.0f) { return 15; }
	if (RadiusCm < 30.0f) { return 10; }
	if (RadiusCm < 39.0f) { return 5; }
	if (RadiusCm < 46.0f) { return 1; }
	return 0;
}

void ABHTrainDartboard::ThrowDart(ABHCharacter* Character, bool bHold)
{
	(void)bHold;
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return;
	}
	const int32 MyId = BHPS->GetPlayerId();

	// Aim ray vs the board face plane (local Y = 0).
	FVector EyeLoc;
	FRotator EyeRot;
	Character->GetActorEyesViewPoint(EyeLoc, EyeRot);
	const FVector Dir = EyeRot.Vector();
	const FTransform& Xform = GetActorTransform();
	const FVector PlaneP = Xform.TransformPosition(FVector(0.0f, 0.0f, BHDartCenterZ));
	const FVector PlaneN = Xform.GetUnitAxis(EAxis::Y);
	const float Denom = FVector::DotProduct(Dir, PlaneN);
	if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float T = FVector::DotProduct(PlaneP - EyeLoc, PlaneN) / Denom;
	if (T <= 0.0f || T > 2000.0f)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(TEXT("Face the dartboard, then tap to throw."), 1.5f);
		}
		return;
	}
	const FVector Local = Xform.InverseTransformPosition(EyeLoc + Dir * T);

	// Impact relative to board centre, plus a little scatter.
	float HitX = Local.X + FMath::FRandRange(-BHDartSpread, BHDartSpread);
	float HitZ = (Local.Z - BHDartCenterZ) + FMath::FRandRange(-BHDartSpread, BHDartSpread);
	const float Radius = FMath::Sqrt(HitX * HitX + HitZ * HitZ);

	const int32 Score = (Radius <= BHDartBoardRadius) ? ScoreForRadius(Radius) : 0;

	// Clamp the visual marker onto the board so a wild miss still shows at the rim.
	if (Radius > BHDartBoardRadius && Radius > KINDA_SMALL_NUMBER)
	{
		const float Scale = BHDartBoardRadius / Radius;
		HitX *= Scale;
		HitZ *= Scale;
	}

	RecentDarts.Add(FVector(HitX, 0.0f, BHDartCenterZ + HitZ));
	while (RecentDarts.Num() > BHDartMaxMarkers)
	{
		RecentDarts.RemoveAt(0);
	}

	int32& Total = TotalByPlayer.FindOrAdd(MyId);
	int32& Throws = ThrowsByPlayer.FindOrAdd(MyId);
	Total += Score;
	Throws += 1;

	ActiveThrowerId = MyId;
	ActiveThrowerName = BHPS->GetPlayerName();
	ActiveTotal = Total;
	ActiveThrows = Throws;
	LastScore = Score;
	LastResultAccent = (Score >= 25) ? DartHot : (Score == 0 ? DartMiss : DartNeutral);
	BHPS->SetActiveMinigameTable(this);

	ForceNetUpdate();
	RefreshVisuals();

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		const FString Flair = (Score == 50) ? TEXT("BULLSEYE!") : (Score == 0 ? TEXT("Missed the board") : FString::Printf(TEXT("scored %d"), Score));
		PC->ClientShowStatusMessage(FString::Printf(TEXT("Darts: %s  (total %d over %d)"), *Flair, Total, Throws), 2.5f);
	}
}

void ABHTrainDartboard::GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const
{
	OutLines.Reset();
	OutColor = LastResultAccent;
	OutLines.Add(TEXT("DARTS"));
	if (PlayerId == ActiveThrowerId && ActiveThrows > 0)
	{
		OutLines.Add(FString::Printf(TEXT("Last throw: %d"), LastScore));
		const float Avg = ActiveThrows > 0 ? (static_cast<float>(ActiveTotal) / ActiveThrows) : 0.0f;
		OutLines.Add(FString::Printf(TEXT("Total: %d   Throws: %d   Avg: %.1f"), ActiveTotal, ActiveThrows, Avg));
	}
	else
	{
		OutLines.Add(TEXT("Aim at the board and TAP to throw."));
	}
	OutLines.Add(TEXT("Bull 50 / 25, then 20 15 10 5 1 by ring."));
}

void ABHTrainDartboard::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// A stand/post behind the board carries collision so the aim trace hits the actor; rings are thin discs on
	// the local Y=0 face (a flattened cylinder is round in X/Z and thin in Y).
	BHPropVisuals::ConfigurePart(Stand, BHPropVisuals::CubeMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 6.0f, BHDartCenterZ), FRotator::ZeroRotator, FVector(1.05f, 0.10f, 1.05f), true);
	BHPropVisuals::TintPart(Stand, FLinearColor(0.18f, 0.12f, 0.08f, 1.0f));

	for (int32 Index = 0; Index < Rings.Num(); ++Index)
	{
		const bool bBull = (Index == BHDartNumRings);   // last entry = inner bull dot
		const float Radius = bBull ? 2.5f : RingRadius[Index];
		const float Diameter = (Radius * 2.0f) / 100.0f;   // cylinder default is 100cm dia
		const FLinearColor Tint = bBull ? FLinearColor(0.85f, 0.10f, 0.10f, 1.0f) : RingTint[Index];
		const float YOffset = -1.0f - (bBull ? 1.4f : Index * 0.2f);   // stack outward->inward toward the player
		BHPropVisuals::ConfigurePart(Rings[Index], BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, YOffset, BHDartCenterZ), FRotator(90.0f, 0.0f, 0.0f), FVector(Diameter, Diameter, 0.01f), false);
		BHPropVisuals::TintPart(Rings[Index], Tint, 0.05f);
	}

	for (int32 Index = 0; Index < DartMarkers.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(DartMarkers[Index], BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -6.0f, BHDartCenterZ), FRotator(90.0f, 0.0f, 0.0f), FVector(0.02f, 0.02f, 0.08f), false);
		BHPropVisuals::SetPartVisible(DartMarkers[Index], false);
	}
}

void ABHTrainDartboard::RefreshVisuals()
{
	for (int32 Index = 0; Index < DartMarkers.Num(); ++Index)
	{
		if (!DartMarkers[Index])
		{
			continue;
		}
		if (RecentDarts.IsValidIndex(Index))
		{
			const FVector P = RecentDarts[Index];
			DartMarkers[Index]->SetRelativeLocation(FVector(P.X, -7.0f, P.Z));
			BHPropVisuals::SetPartVisible(DartMarkers[Index], true);
			const bool bNewest = (Index == RecentDarts.Num() - 1);
			BHPropVisuals::TintPart(DartMarkers[Index], bNewest ? FLinearColor(1.0f, 0.9f, 0.3f, 1.0f) : FLinearColor(0.85f, 0.85f, 0.9f, 1.0f), bNewest ? 0.6f : 0.0f);
		}
		else
		{
			BHPropVisuals::SetPartVisible(DartMarkers[Index], false);
		}
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("DARTS")));
	}
	if (StatusTextRender)
	{
		const FString Status = (ActiveThrows > 0)
			? FString::Printf(TEXT("%s: last %d  total %d"), *ActiveThrowerName, LastScore, ActiveTotal)
			: FString(TEXT("Aim + tap to throw"));
		StatusTextRender->SetText(FText::FromString(Status));
		StatusTextRender->SetTextRenderColor(LastResultAccent.ToFColor(true));
	}
}
