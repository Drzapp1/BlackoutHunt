// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainSlotMachine.h"
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
	constexpr int32 BHSlotStartingCredits = 200;
	constexpr int32 BHSlotBet = 10;
	constexpr int32 BHSlotSymbols = 6;   // 0 Cherry, 1 Lemon, 2 Bell, 3 Star, 4 Diamond, 5 Seven

	const FLinearColor SlotWin(0.30f, 1.0f, 0.52f, 1.0f);
	const FLinearColor SlotLose(1.0f, 0.45f, 0.30f, 1.0f);
	const FLinearColor SlotIdle(0.95f, 0.85f, 0.40f, 1.0f);

	void BHConfigureSlotText(UTextRenderComponent* TextComponent, const FVector& Location, float WorldSize, const FColor& Color)
	{
		BHPropVisuals::ConfigureReadableText(TextComponent, Location, FRotator(0.0f, 90.0f, 0.0f), WorldSize, Color);
	}
}

ABHTrainSlotMachine::ABHTrainSlotMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Slots"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(SceneRoot);
	Lever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lever"));
	Lever->SetupAttachment(SceneRoot);

	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(SceneRoot);
	BHConfigureSlotText(TitleText, FVector(0.0f, -32.0f, 168.0f), 12.0f, FColor(255, 224, 120));

	ReelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ReelText"));
	ReelText->SetupAttachment(SceneRoot);
	BHConfigureSlotText(ReelText, FVector(0.0f, -32.0f, 120.0f), 18.0f, FColor(255, 255, 255));

	MachineLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MachineLight"));
	MachineLight->SetupAttachment(SceneRoot);
	MachineLight->SetRelativeLocation(FVector(0.0f, -40.0f, 150.0f));
	MachineLight->SetIntensity(90.0f);
	MachineLight->SetAttenuationRadius(300.0f);
	MachineLight->SetLightColor(FLinearColor(1.0f, 0.78f, 0.35f, 1.0f));
	MachineLight->SetCastShadows(false);

	Reels.Init(0, 3);
	LastResultText = TEXT("Tap to spin!");
	ApplyVisuals();
	RefreshVisuals();
}

void ABHTrainSlotMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshVisuals();
}

void ABHTrainSlotMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainSlotMachine, Reels);
	DOREPLIFETIME(ABHTrainSlotMachine, ActiveSpinnerId);
	DOREPLIFETIME(ABHTrainSlotMachine, ActiveCredits);
	DOREPLIFETIME(ABHTrainSlotMachine, LastResultText);
	DOREPLIFETIME(ABHTrainSlotMachine, LastResultAccent);
}

bool ABHTrainSlotMachine::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainSlotMachine::BeginInteract_Implementation(ABHCharacter* Character)
{
	// Single verb: any press spins on release. Nothing to record.
	(void)Character;
}

void ABHTrainSlotMachine::EndInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	Spin(Character);
}

FText ABHTrainSlotMachine::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(TEXT("Pull the lever (Spin)"));
}

FBHInteractionPromptInfo ABHTrainSlotMachine::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("SPIN TO WIN"));
	}
	return Info;
}

void ABHTrainSlotMachine::OnRep_State()
{
	RefreshVisuals();
}

float ABHTrainSlotMachine::GetServerTimeSeconds() const
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

FString ABHTrainSlotMachine::SymbolGlyph(int32 Symbol)
{
	static const TCHAR* Glyphs[BHSlotSymbols] = { TEXT("C"), TEXT("L"), TEXT("B"), TEXT("*"), TEXT("D"), TEXT("7") };
	const int32 Index = FMath::Clamp(Symbol, 0, BHSlotSymbols - 1);
	return Glyphs[Index];
}

int32 ABHTrainSlotMachine::EvaluatePayout(int32 A, int32 B, int32 C, int32 Bet)
{
	if (A == 5 && B == 5 && C == 5)
	{
		return Bet * 50;   // jackpot: three 7s
	}
	if (A == 4 && B == 4 && C == 4)
	{
		return Bet * 25;   // three diamonds
	}
	if (A == B && B == C)
	{
		return Bet * 10;   // any three of a kind
	}
	if (A == B || B == C || A == C)
	{
		return Bet * 2;    // any pair
	}
	if (A == 5 || B == 5 || C == 5)
	{
		return Bet;        // a lone seven returns the bet
	}
	return 0;
}

void ABHTrainSlotMachine::Spin(ABHCharacter* Character)
{
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return;
	}
	const int32 MyId = BHPS->GetPlayerId();

	int32& Credits = CreditsByPlayer.FindOrAdd(MyId);
	if (Credits <= 0)
	{
		Credits = BHSlotStartingCredits;   // free re-buy
	}
	Credits -= BHSlotBet;

	const int32 A = FMath::RandRange(0, BHSlotSymbols - 1);
	const int32 B = FMath::RandRange(0, BHSlotSymbols - 1);
	const int32 C = FMath::RandRange(0, BHSlotSymbols - 1);
	Reels.Reset();
	Reels.Add(A);
	Reels.Add(B);
	Reels.Add(C);

	const int32 Payout = EvaluatePayout(A, B, C, BHSlotBet);
	Credits += Payout;

	FString Result;
	if (Payout >= BHSlotBet * 25)
	{
		Result = FString::Printf(TEXT("JACKPOT! +%d"), Payout);
		LastResultAccent = SlotWin;
	}
	else if (Payout > BHSlotBet)
	{
		Result = FString::Printf(TEXT("WIN +%d"), Payout);
		LastResultAccent = SlotWin;
	}
	else if (Payout == BHSlotBet)
	{
		Result = TEXT("Push - bet returned");
		LastResultAccent = SlotIdle;
	}
	else
	{
		Result = FString::Printf(TEXT("No win  -%d"), BHSlotBet);
		LastResultAccent = SlotLose;
	}

	ActiveSpinnerId = MyId;
	ActiveCredits = Credits;
	LastResultText = Result;
	BHPS->SetActiveMinigameTable(this);

	ForceNetUpdate();
	RefreshVisuals();

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(FString::Printf(TEXT("Slots: %s %s %s  -  %s  (credits: %d)"),
			*SymbolGlyph(A), *SymbolGlyph(B), *SymbolGlyph(C), *Result, Credits), 3.0f);
	}
}

void ABHTrainSlotMachine::GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const
{
	OutLines.Reset();
	OutColor = LastResultAccent;
	OutLines.Add(TEXT("LUCKY SEVENS - SLOTS"));
	if (Reels.Num() == 3)
	{
		OutLines.Add(FString::Printf(TEXT("[ %s ] [ %s ] [ %s ]"), *SymbolGlyph(Reels[0]), *SymbolGlyph(Reels[1]), *SymbolGlyph(Reels[2])));
	}
	if (PlayerId == ActiveSpinnerId)
	{
		OutLines.Add(LastResultText);
		OutLines.Add(FString::Printf(TEXT("CREDITS: %d     (bet %d)"), ActiveCredits, BHSlotBet));
	}
	else
	{
		OutLines.Add(FString::Printf(TEXT("Tap to spin!  (%d credits to start)"), BHSlotStartingCredits));
	}
	OutLines.Add(TEXT("7 7 7 = JACKPOT x50   D D D = x25   3-kind x10"));
}

void ABHTrainSlotMachine::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Cabinet body + a glowing screen face + a side lever. Body carries collision (the look/interact target).
	BHPropVisuals::ConfigurePart(Body, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 80.0f), FRotator::ZeroRotator, FVector(0.55f, 0.45f, 1.6f), true);
	BHPropVisuals::ConfigurePart(Screen, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -23.0f, 120.0f), FRotator::ZeroRotator, FVector(0.42f, 0.04f, 0.30f), false);
	BHPropVisuals::ConfigurePart(Lever, BHPropVisuals::CylinderMesh(), BHPropVisuals::RustedMetalMaterial(), FVector(30.0f, 0.0f, 110.0f), FRotator(20.0f, 0.0f, 0.0f), FVector(0.05f, 0.05f, 0.40f), false);

	BHPropVisuals::TintPart(Body, FLinearColor(0.62f, 0.12f, 0.12f, 1.0f), 0.05f);
	BHPropVisuals::TintPart(Screen, FLinearColor(0.02f, 0.03f, 0.05f, 1.0f), 0.0f);
	BHPropVisuals::TintPart(Lever, FLinearColor(0.85f, 0.85f, 0.9f, 1.0f), 0.0f);
}

void ABHTrainSlotMachine::RefreshVisuals()
{
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("LUCKY 7s")));
		TitleText->SetTextRenderColor(FColor(255, 224, 120));
	}
	if (ReelText && Reels.Num() == 3)
	{
		ReelText->SetText(FText::FromString(FString::Printf(TEXT("%s %s %s"),
			*SymbolGlyph(Reels[0]), *SymbolGlyph(Reels[1]), *SymbolGlyph(Reels[2]))));
		ReelText->SetTextRenderColor(LastResultAccent.ToFColor(true));
	}
}
