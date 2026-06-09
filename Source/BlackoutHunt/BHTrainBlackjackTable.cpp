// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainBlackjackTable.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float BHBlackjackHoldSeconds = 0.35f;   // press longer than this == STAND
	constexpr int32 BHBlackjackStartingChips = 500;
	constexpr int32 BHBlackjackBet = 25;
	constexpr int32 BHBlackjackMaxSeats = 5;
	constexpr float BHBlackjackSeatRange = 800.0f;     // a seated player further than this is dropped
	constexpr float BHBlackjackTurnTimeout = 45.0f;    // auto-resolve if a hand stalls (e.g. someone wandered off)

	const FLinearColor BJWin(0.30f, 1.0f, 0.52f, 1.0f);
	const FLinearColor BJLose(1.0f, 0.40f, 0.26f, 1.0f);
	const FLinearColor BJNeutral(0.46f, 0.78f, 1.0f, 1.0f);

	void BHConfigureBJText(UTextRenderComponent* TextComponent, const FVector& Location, float WorldSize, const FColor& Color)
	{
		BHPropVisuals::ConfigureReadableText(TextComponent, Location, FRotator(0.0f, 90.0f, 0.0f), WorldSize, Color);
	}
}

ABHTrainBlackjackTable::ABHTrainBlackjackTable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.15f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Blackjack"));

	Felt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Felt"));
	Felt->SetupAttachment(SceneRoot);
	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(SceneRoot);
	Rail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rail"));
	Rail->SetupAttachment(SceneRoot);
	ChipStack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChipStack"));
	ChipStack->SetupAttachment(SceneRoot);
	Shoe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shoe"));
	Shoe->SetupAttachment(SceneRoot);

	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(SceneRoot);
	BHConfigureBJText(TitleText, FVector(-86.0f, 0.0f, 96.0f), 13.0f, FColor(255, 226, 120));

	FeltText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FeltText"));
	FeltText->SetupAttachment(SceneRoot);
	BHConfigureBJText(FeltText, FVector(-86.0f, 0.0f, 74.0f), 7.6f, FColor(226, 246, 234));

	TableLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TableLight"));
	TableLight->SetupAttachment(SceneRoot);
	TableLight->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	TableLight->SetIntensity(120.0f);
	TableLight->SetAttenuationRadius(360.0f);
	TableLight->SetLightColor(FLinearColor(0.30f, 0.90f, 0.52f, 1.0f));
	TableLight->SetCastShadows(false);

	ApplyTableVisuals();
	RefreshTableText();
}

void ABHTrainBlackjackTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Server-side round housekeeping: drop players who left/wandered off so a hand can't stall, and force a
	// resolve once everyone is done or the round times out.
	if (HasAuthority())
	{
		// Prune departed/disconnected players in ANY phase so a stale seat (and its ActiveMinigameTable)
		// never lingers -- not only while a hand is being played.
		PruneSeats();
		if (static_cast<EBJPhase>(Phase) == EBJPhase::PlayerTurn)
		{
			if (Seats.Num() == 0)
			{
				Phase = static_cast<uint8>(EBJPhase::Idle);
				DealerCards.Reset();
				bDealerHoleHidden = true;
				ForceNetUpdate();
			}
			else if (AllSeatsDone())
			{
				ResolveRound();
			}
			else if (GetServerTimeSeconds() - PlayerTurnStartTime > BHBlackjackTurnTimeout)
			{
				for (FBHBlackjackSeat& Seat : Seats)
				{
					if (Seat.State == static_cast<uint8>(EBJSeatState::Active))
					{
						Seat.State = static_cast<uint8>(EBJSeatState::Stand);
					}
				}
				ResolveRound();
			}
		}
	}

	RefreshTableText();
}

void ABHTrainBlackjackTable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainBlackjackTable, Seats);
	DOREPLIFETIME(ABHTrainBlackjackTable, DealerCards);
	DOREPLIFETIME(ABHTrainBlackjackTable, bDealerHoleHidden);
	DOREPLIFETIME(ABHTrainBlackjackTable, Phase);
	DOREPLIFETIME(ABHTrainBlackjackTable, SeatedCount);
}

bool ABHTrainBlackjackTable::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainBlackjackTable::BeginInteract_Implementation(ABHCharacter* Character)
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
	PressTimeByPlayerId.Add(BHPS->GetPlayerId(), GetServerTimeSeconds());
}

void ABHTrainBlackjackTable::EndInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}
	const int32 MyId = BHPS->GetPlayerId();
	const float* PressTime = PressTimeByPlayerId.Find(MyId);
	const float HeldFor = PressTime ? (GetServerTimeSeconds() - *PressTime) : 0.0f;
	PressTimeByPlayerId.Remove(MyId);   // always clear, even if the player died mid-hold
	if (BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return;
	}
	const bool bHold = HeldFor >= BHBlackjackHoldSeconds;

	const EBJPhase CurrentPhase = static_cast<EBJPhase>(Phase);

	if (CurrentPhase == EBJPhase::PlayerTurn)
	{
		int32 SeatIndex = FindSeatIndex(MyId);
		if (SeatIndex == -1)
		{
			// Joining mid-round: take a seat but sit this hand out (dealt in next round).
			SeatIndex = AddSeatForPlayer(Character);
			if (SeatIndex != -1)
			{
				Seats[SeatIndex].State = static_cast<uint8>(EBJSeatState::Stand);
				Seats[SeatIndex].Result = TEXT("Joined - dealt next round.");
				ForceNetUpdate();
				if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
				{
					PC->ClientShowStatusMessage(TEXT("Seated - you'll be dealt next round."), 2.5f);
				}
			}
			else if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
			{
				PC->ClientShowStatusMessage(TEXT("Blackjack table is full."), 2.0f);
			}
			return;
		}
		if (Seats[SeatIndex].State != static_cast<uint8>(EBJSeatState::Active))
		{
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
			{
				PC->ClientShowStatusMessage(TEXT("You've finished this hand - waiting for the dealer."), 2.0f);
			}
			return;
		}
		if (bHold)
		{
			PlayerStand(SeatIndex);
		}
		else
		{
			PlayerHit(SeatIndex);
		}
		return;
	}

	// Idle or Resolved: an unseated player takes a seat; a seated player deals the next round for everyone.
	const int32 SeatIndex = FindSeatIndex(MyId);
	if (SeatIndex == -1)
	{
		const int32 NewSeat = AddSeatForPlayer(Character);
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(NewSeat != -1 ? TEXT("Seated. Tap again to deal.") : TEXT("Blackjack table is full."), 2.5f);
		}
		ForceNetUpdate();
		RefreshTableText();
	}
	else
	{
		StartRound(Character);
	}
}

FText ABHTrainBlackjackTable::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	const EBJPhase CurrentPhase = static_cast<EBJPhase>(Phase);
	return FText::FromString(CurrentPhase == EBJPhase::PlayerTurn ? TEXT("Tap: Hit  /  Hold: Stand") : TEXT("Play Blackjack"));
}

FBHInteractionPromptInfo ABHTrainBlackjackTable::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;   // we measure hold ourselves so tap stays instant
	if (Info.bCanInteract)
	{
		const EBJPhase CurrentPhase = static_cast<EBJPhase>(Phase);
		Info.RiskText = FText::FromString(CurrentPhase == EBJPhase::PlayerTurn ? TEXT("TAP HIT / HOLD STAND") : TEXT("TAKE A SEAT"));
	}
	return Info;
}

void ABHTrainBlackjackTable::OnRep_TableState()
{
	ApplyTableVisuals();
	RefreshTableText();
}

float ABHTrainBlackjackTable::GetServerTimeSeconds() const
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

// ---------------------------------------------------------------------------------------------------------
// Seat management
// ---------------------------------------------------------------------------------------------------------

int32 ABHTrainBlackjackTable::FindSeatIndex(int32 PlayerId) const
{
	for (int32 Index = 0; Index < Seats.Num(); ++Index)
	{
		if (Seats[Index].PlayerId == PlayerId)
		{
			return Index;
		}
	}
	return -1;
}

int32 ABHTrainBlackjackTable::AddSeatForPlayer(ABHCharacter* Character)
{
	ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS)
	{
		return -1;
	}
	const int32 Existing = FindSeatIndex(BHPS->GetPlayerId());
	if (Existing != -1)
	{
		return Existing;
	}
	if (Seats.Num() >= BHBlackjackMaxSeats)
	{
		return -1;
	}

	FBHBlackjackSeat Seat;
	Seat.PlayerId = BHPS->GetPlayerId();
	Seat.PlayerName = BHPS->GetPlayerName();
	Seat.Chips = BHBlackjackStartingChips;
	Seat.State = static_cast<uint8>(EBJSeatState::Stand);   // not active until a round is dealt
	Seat.Result = TEXT("Seated.");
	Seats.Add(Seat);
	SeatOwners.Add(BHPS);
	SeatedCount = Seats.Num();

	BHPS->SetActiveMinigameTable(this);
	return Seats.Num() - 1;
}

void ABHTrainBlackjackTable::PruneSeats()
{
	bool bChanged = false;
	for (int32 Index = Seats.Num() - 1; Index >= 0; --Index)
	{
		ABHPlayerState* SeatOwner = SeatOwners.IsValidIndex(Index) ? SeatOwners[Index].Get() : nullptr;
		bool bDrop = (SeatOwner == nullptr);
		if (!bDrop)
		{
			const APawn* Pawn = SeatOwner->GetPawn();
			if (!Pawn || FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > BHBlackjackSeatRange)
			{
				bDrop = true;
			}
		}
		if (bDrop)
		{
			if (SeatOwner && SeatOwner->GetActiveMinigameTable() == this)
			{
				SeatOwner->SetActiveMinigameTable(nullptr);
			}
			Seats.RemoveAt(Index);
			if (SeatOwners.IsValidIndex(Index))
			{
				SeatOwners.RemoveAt(Index);
			}
			bChanged = true;
		}
	}
	if (bChanged)
	{
		SeatedCount = Seats.Num();
		ForceNetUpdate();
	}
}

bool ABHTrainBlackjackTable::AllSeatsDone() const
{
	for (const FBHBlackjackSeat& Seat : Seats)
	{
		if (Seat.State == static_cast<uint8>(EBJSeatState::Active))
		{
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------
// Round flow
// ---------------------------------------------------------------------------------------------------------

void ABHTrainBlackjackTable::StartRound(ABHCharacter* Character)
{
	(void)Character;
	PruneSeats();
	if (Seats.Num() == 0)
	{
		return;
	}
	if (Deck.Num() < (Seats.Num() + 1) * 6)
	{
		BuildAndShuffleDeck();
	}

	DealerCards.Reset();
	for (FBHBlackjackSeat& Seat : Seats)
	{
		Seat.Cards.Reset();
		Seat.State = static_cast<uint8>(EBJSeatState::Active);
		Seat.Result.Empty();
		Seat.ResultAccent = BJNeutral;
		if (Seat.Chips <= 0)
		{
			Seat.Chips = BHBlackjackStartingChips;   // free re-buy so nobody gets stuck broke
		}
		Seat.Chips = FMath::Max(0, Seat.Chips - BHBlackjackBet);   // ante
	}

	// Deal two cards to each seat and the dealer.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (FBHBlackjackSeat& Seat : Seats)
		{
			Seat.Cards.Add(DrawCard());
		}
		DealerCards.Add(DrawCard());
	}
	bDealerHoleHidden = true;
	Phase = static_cast<uint8>(EBJPhase::PlayerTurn);
	PlayerTurnStartTime = GetServerTimeSeconds();

	// Naturals stand immediately (paid 3:2 at resolve).
	for (FBHBlackjackSeat& Seat : Seats)
	{
		if (IsBlackjack(Seat.Cards))
		{
			Seat.State = static_cast<uint8>(EBJSeatState::Stand);
		}
	}

	if (AllSeatsDone())
	{
		ResolveRound();
	}
	else
	{
		ForceNetUpdate();
		RefreshTableText();
	}
}

void ABHTrainBlackjackTable::PlayerHit(int32 SeatIndex)
{
	if (static_cast<EBJPhase>(Phase) != EBJPhase::PlayerTurn || !Seats.IsValidIndex(SeatIndex))
	{
		return;
	}
	FBHBlackjackSeat& Seat = Seats[SeatIndex];
	if (Seat.State != static_cast<uint8>(EBJSeatState::Active))
	{
		return;
	}
	Seat.Cards.Add(DrawCard());
	bool bSoft = false;
	const int32 Value = HandValue(Seat.Cards, bSoft);
	if (Value > 21)
	{
		Seat.State = static_cast<uint8>(EBJSeatState::Bust);
		Seat.Result = TEXT("BUST");
		Seat.ResultAccent = BJLose;
	}
	else if (Value == 21)
	{
		Seat.State = static_cast<uint8>(EBJSeatState::Stand);
	}

	if (AllSeatsDone())
	{
		ResolveRound();
	}
	else
	{
		ForceNetUpdate();
		RefreshTableText();
	}
}

void ABHTrainBlackjackTable::PlayerStand(int32 SeatIndex)
{
	if (static_cast<EBJPhase>(Phase) != EBJPhase::PlayerTurn || !Seats.IsValidIndex(SeatIndex))
	{
		return;
	}
	Seats[SeatIndex].State = static_cast<uint8>(EBJSeatState::Stand);

	if (AllSeatsDone())
	{
		ResolveRound();
	}
	else
	{
		ForceNetUpdate();
		RefreshTableText();
	}
}

void ABHTrainBlackjackTable::ResolveRound()
{
	bDealerHoleHidden = false;
	bool bSoft = false;
	// The dealer normally hits below 17 (house rules). Slip up sometimes -- a distracted dealer that stands early or
	// greedily hits a made hand -- so players aren't grinding against perfect house play and can win more rounds.
	int32 DealerStandValue = 17;
	const float DealerSlip = FMath::FRand();
	if (DealerSlip < 0.18f)
	{
		DealerStandValue = 15;   // stands early, often leaving a beatable dealer hand
	}
	else if (DealerSlip < 0.30f)
	{
		DealerStandValue = 19;   // hits a made 17/18 and busts far more often
	}
	while (HandValue(DealerCards, bSoft) < DealerStandValue)
	{
		DealerCards.Add(DrawCard());
	}
	const int32 DealerValue = HandValue(DealerCards, bSoft);
	const bool bDealerBlackjack = IsBlackjack(DealerCards);
	const bool bDealerBust = DealerValue > 21;

	for (int32 Index = 0; Index < Seats.Num(); ++Index)
	{
		FBHBlackjackSeat& Seat = Seats[Index];
		if (Seat.Cards.Num() == 0)
		{
			continue;   // a seat that joined late and sat the round out
		}

		FString Result;
		FLinearColor Accent = BJNeutral;
		int32 Payout = 0;   // net vs the already-deducted ante

		bool bPlayerSoft = false;
		const int32 PlayerValue = HandValue(Seat.Cards, bPlayerSoft);
		const bool bPlayerBlackjack = IsBlackjack(Seat.Cards);

		if (Seat.State == static_cast<uint8>(EBJSeatState::Bust) || PlayerValue > 21)
		{
			Result = FString::Printf(TEXT("BUST at %d - dealer wins."), PlayerValue);
			Accent = BJLose;
		}
		else if (bPlayerBlackjack && !bDealerBlackjack)
		{
			Result = TEXT("BLACKJACK! Pays 3:2.");
			Accent = BJWin;
			// Round the half-chip in the player's favour (e.g. bet 25 -> 63, not a truncated 62).
			Payout = BHBlackjackBet + (BHBlackjackBet * 3 + 1) / 2;
		}
		else if (bDealerBlackjack && !bPlayerBlackjack)
		{
			Result = TEXT("Dealer blackjack - you lose.");
			Accent = BJLose;
		}
		else if (bDealerBust)
		{
			Result = FString::Printf(TEXT("Dealer busts at %d - YOU WIN!"), DealerValue);
			Accent = BJWin;
			Payout = BHBlackjackBet * 2;
		}
		else if (PlayerValue > DealerValue)
		{
			Result = FString::Printf(TEXT("You win %d to %d!"), PlayerValue, DealerValue);
			Accent = BJWin;
			Payout = BHBlackjackBet * 2;
		}
		else if (PlayerValue < DealerValue)
		{
			Result = FString::Printf(TEXT("Dealer wins %d to %d."), DealerValue, PlayerValue);
			Accent = BJLose;
		}
		else
		{
			Result = FString::Printf(TEXT("Push at %d - bet returned."), PlayerValue);
			Accent = BJNeutral;
			Payout = BHBlackjackBet;
		}

		Seat.Chips += Payout;
		Seat.Result = Result;
		Seat.ResultAccent = Accent;

		if (ABHPlayerState* SeatOwner = SeatOwners.IsValidIndex(Index) ? SeatOwners[Index].Get() : nullptr)
		{
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(SeatOwner->GetOwningController()))
			{
				PC->ClientShowStatusMessage(FString::Printf(TEXT("Blackjack: %s  (chips: %d)"), *Result, Seat.Chips), 3.5f);
			}
		}
	}

	Phase = static_cast<uint8>(EBJPhase::Resolved);
	ForceNetUpdate();
	RefreshTableText();
}

// ---------------------------------------------------------------------------------------------------------
// Cards
// ---------------------------------------------------------------------------------------------------------

void ABHTrainBlackjackTable::BuildAndShuffleDeck()
{
	Deck.Reset();
	for (int32 D = 0; D < 4; ++D)   // four-deck shoe so a multi-seat session doesn't run dry
	{
		for (int32 Card = 0; Card < 52; ++Card)
		{
			Deck.Add(Card);
		}
	}
	for (int32 Index = Deck.Num() - 1; Index > 0; --Index)
	{
		const int32 Swap = FMath::RandRange(0, Index);
		Deck.Swap(Index, Swap);
	}
}

int32 ABHTrainBlackjackTable::DrawCard()
{
	if (Deck.Num() == 0)
	{
		BuildAndShuffleDeck();
	}
	if (Deck.Num() == 0)
	{
		return 0;
	}
	const int32 Card = Deck.Last();
	Deck.Pop();
	return Card;
}

int32 ABHTrainBlackjackTable::CardRank(int32 Card)
{
	return (Card % 13) + 1;
}

int32 ABHTrainBlackjackTable::CardSuit(int32 Card)
{
	return (Card / 13) % 4;
}

FString ABHTrainBlackjackTable::CardLabel(int32 Card)
{
	static const TCHAR* RankNames[] = { TEXT("A"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("10"), TEXT("J"), TEXT("Q"), TEXT("K") };
	static const TCHAR* SuitNames[] = { TEXT("S"), TEXT("H"), TEXT("D"), TEXT("C") };
	const int32 Rank = FMath::Clamp(CardRank(Card) - 1, 0, 12);
	const int32 Suit = FMath::Clamp(CardSuit(Card), 0, 3);
	return FString::Printf(TEXT("%s%s"), RankNames[Rank], SuitNames[Suit]);
}

int32 ABHTrainBlackjackTable::HandValue(const TArray<int32>& Cards, bool& bOutSoft)
{
	int32 Total = 0;
	int32 Aces = 0;
	for (int32 Card : Cards)
	{
		const int32 Rank = CardRank(Card);
		if (Rank == 1)
		{
			++Aces;
			Total += 11;
		}
		else
		{
			Total += FMath::Min(Rank, 10);
		}
	}
	int32 SoftAces = Aces;
	while (Total > 21 && SoftAces > 0)
	{
		Total -= 10;
		--SoftAces;
	}
	bOutSoft = SoftAces > 0;
	return Total;
}

bool ABHTrainBlackjackTable::IsBlackjack(const TArray<int32>& Cards)
{
	bool bSoft = false;
	return Cards.Num() == 2 && HandValue(Cards, bSoft) == 21;
}

FString ABHTrainBlackjackTable::HandLine(const TArray<int32>& Cards, bool bHideHole)
{
	TArray<FString> Parts;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		if (bHideHole && Index == 1)
		{
			Parts.Add(TEXT("[##]"));
		}
		else
		{
			Parts.Add(FString::Printf(TEXT("[%s]"), *CardLabel(Cards[Index])));
		}
	}
	return FString::Join(Parts, TEXT(" "));
}

// ---------------------------------------------------------------------------------------------------------
// HUD readout
// ---------------------------------------------------------------------------------------------------------

void ABHTrainBlackjackTable::GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const
{
	OutLines.Reset();
	OutColor = BJNeutral;
	OutLines.Add(TEXT("BLACKJACK  21"));

	const int32 SeatIndex = FindSeatIndex(PlayerId);
	if (SeatIndex == -1)
	{
		OutLines.Add(TEXT("Walk up and TAP (E) to take a seat."));
		OutLines.Add(TEXT("Dealer stands on 17. Blackjack pays 3:2."));
		if (SeatedCount > 0)
		{
			OutLines.Add(FString::Printf(TEXT("%d player(s) at the table."), SeatedCount));
		}
		return;
	}

	const FBHBlackjackSeat& Seat = Seats[SeatIndex];
	const EBJPhase CurrentPhase = static_cast<EBJPhase>(Phase);

	if (DealerCards.Num() > 0)
	{
		FString DealerLine = FString::Printf(TEXT("DEALER  %s"), *HandLine(DealerCards, bDealerHoleHidden));
		if (!bDealerHoleHidden)
		{
			bool bDealerSoft = false;
			DealerLine += FString::Printf(TEXT("  = %d"), HandValue(DealerCards, bDealerSoft));
		}
		OutLines.Add(DealerLine);
	}

	if (Seat.Cards.Num() > 0)
	{
		bool bSoft = false;
		const int32 Value = HandValue(Seat.Cards, bSoft);
		OutLines.Add(FString::Printf(TEXT("YOU     %s  = %d"), *HandLine(Seat.Cards, false), Value));
	}

	if (CurrentPhase == EBJPhase::PlayerTurn)
	{
		if (Seat.State == static_cast<uint8>(EBJSeatState::Active))
		{
			OutLines.Add(TEXT("TAP = Hit      HOLD = Stand"));
		}
		else
		{
			OutLines.Add(Seat.State == static_cast<uint8>(EBJSeatState::Bust)
				? TEXT("BUST - waiting for the dealer...")
				: TEXT("Standing - waiting for the dealer..."));
		}
	}
	else if (CurrentPhase == EBJPhase::Resolved)
	{
		if (!Seat.Result.IsEmpty())
		{
			OutLines.Add(Seat.Result);
			OutColor = Seat.ResultAccent;
		}
		OutLines.Add(TEXT("Tap to deal the next hand."));
	}
	else
	{
		OutLines.Add(TEXT("Tap to deal."));
	}

	OutLines.Add(FString::Printf(TEXT("CHIPS: %d     (bet %d)"), Seat.Chips, BHBlackjackBet));
}

// ---------------------------------------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------------------------------------

void ABHTrainBlackjackTable::ApplyTableVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const FLinearColor FeltGreen(0.05f, 0.32f, 0.16f, 1.0f);
	const FLinearColor Wood(0.24f, 0.15f, 0.09f, 1.0f);
	BHPropVisuals::ConfigurePart(Base, BHPropVisuals::CylinderMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 28.0f), FRotator::ZeroRotator, FVector(0.34f, 0.34f, 0.56f), true);
	BHPropVisuals::ConfigurePart(Felt, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 58.0f), FRotator::ZeroRotator, FVector(1.55f, 1.10f, 0.06f), true);
	BHPropVisuals::ConfigurePart(Rail, BHPropVisuals::CylinderMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 62.0f), FRotator::ZeroRotator, FVector(1.66f, 1.20f, 0.05f));
	BHPropVisuals::ConfigurePart(ChipStack, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(36.0f, -44.0f, 66.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.10f));
	BHPropVisuals::ConfigurePart(Shoe, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(-44.0f, 40.0f, 66.0f), FRotator(0.0f, -24.0f, 0.0f), FVector(0.16f, 0.10f, 0.07f));

	BHPropVisuals::TintPart(Base, Wood);
	BHPropVisuals::TintPart(Felt, FeltGreen, 0.25f);
	BHPropVisuals::TintPart(Rail, Wood * 1.4f, 0.0f);
	BHPropVisuals::TintPart(ChipStack, FLinearColor(0.90f, 0.18f, 0.18f, 1.0f), 0.6f);
	BHPropVisuals::TintPart(Shoe, FLinearColor(0.10f, 0.12f, 0.14f, 1.0f), 0.0f);
}

void ABHTrainBlackjackTable::RefreshTableText()
{
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("BLACKJACK  21")));
		TitleText->SetTextRenderColor(FColor(255, 226, 120));
	}
	if (!FeltText)
	{
		return;
	}

	// Per-player hands live on each player's HUD now; the felt just advertises the table + the shared dealer.
	const EBJPhase CurrentPhase = static_cast<EBJPhase>(Phase);
	TArray<FString> Lines;
	if (CurrentPhase == EBJPhase::Idle || Seats.Num() == 0)
	{
		Lines.Add(TEXT("Multiplayer table - walk up and tap E to sit."));
		Lines.Add(TEXT("Your hand shows on YOUR screen."));
		Lines.Add(TEXT("Dealer stands on 17. Blackjack pays 3:2."));
	}
	else
	{
		Lines.Add(FString::Printf(TEXT("%d seated   -   tap E to play"), SeatedCount));
		Lines.Add(FString::Printf(TEXT("DEALER  %s"), *HandLine(DealerCards, bDealerHoleHidden)));
		Lines.Add(TEXT("Your hand + chips are on your screen."));
	}
	FeltText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
	FeltText->SetTextRenderColor(FColor(226, 246, 234));
}
