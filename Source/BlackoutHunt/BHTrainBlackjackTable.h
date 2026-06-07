// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainBlackjackTable.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class ABHPlayerState;

// One player's seat at the blackjack table. Replicated as an array so every client knows the table layout,
// but each player reads only their OWN seat for their on-screen HUD readout (see GetHudLinesForPlayer).
USTRUCT()
struct FBHBlackjackSeat
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PlayerId = -1;

	UPROPERTY()
	FString PlayerName;

	UPROPERTY()
	TArray<int32> Cards;

	UPROPERTY()
	int32 Chips = 0;

	// 0 = Active (may hit/stand), 1 = Stand, 2 = Bust. Reset to Active each deal.
	UPROPERTY()
	uint8 State = 0;

	UPROPERTY()
	FString Result;

	UPROPERTY()
	FLinearColor ResultAccent = FLinearColor(0.46f, 0.78f, 1.0f, 1.0f);
};

// A fully playable, self-contained, MULTI-PLAYER blackjack table for the train lobby. Server-authoritative:
// several players can each take a seat and play their own hand against one shared dealer. Interaction is a
// single verb on the table, resolved per seat:
//   * TAP interact  -> take a seat (when idle), DEAL a new round (seated player, between rounds), or HIT.
//   * HOLD interact  -> STAND.
// When every seated player has finished (stand/bust), the dealer reveals the hole card and draws to 17, then
// all hands resolve. Dealer stands on 17+. Blackjack pays 3:2. Each player's hand/chips are shown on THEIR
// screen (HUD), not on the felt -- see ABHHUD::DrawMinigameStatus + GetHudLinesForPlayer.
UCLASS()
class BLACKOUTHUNT_API ABHTrainBlackjackTable : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainBlackjackTable();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	// Client HUD: the local player's blackjack readout (their hand + value, dealer up-card, chips, result,
	// and the current prompt). Returns the lines + an accent colour. Reads only replicated state.
	void GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const;

protected:
	UFUNCTION()
	void OnRep_TableState();

	// Phases of a round (shared by all seated players).
	enum class EBJPhase : uint8
	{
		Idle = 0,     // no round in progress; tap to seat / deal
		PlayerTurn,   // seated players may hit (tap) or stand (hold) on their own hand
		Resolved      // round finished; tap to deal the next one
	};

	enum class EBJSeatState : uint8
	{
		Active = 0,
		Stand,
		Bust
	};

	float GetServerTimeSeconds() const;

	// Seat management (server).
	int32 FindSeatIndex(int32 PlayerId) const;
	int32 AddSeatForPlayer(ABHCharacter* Character);   // -1 if the table is full
	void PruneSeats();                                  // drop seats whose player left / walked away
	bool AllSeatsDone() const;

	// Round flow (server).
	void StartRound(ABHCharacter* Character);
	void PlayerHit(int32 SeatIndex);
	void PlayerStand(int32 SeatIndex);
	void ResolveRound();

	int32 DrawCard();
	void BuildAndShuffleDeck();
	static int32 CardRank(int32 Card);   // 1..13 (Ace..King)
	static int32 CardSuit(int32 Card);   // 0..3
	static FString CardLabel(int32 Card);
	static int32 HandValue(const TArray<int32>& Cards, bool& bOutSoft);
	static bool IsBlackjack(const TArray<int32>& Cards);
	static FString HandLine(const TArray<int32>& Cards, bool bHideHole);

	void ApplyTableVisuals();
	void RefreshTableText();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Felt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Rail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChipStack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Shoe;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TitleText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> FeltText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> TableLight;

	// ---- replicated table state ----
	UPROPERTY(ReplicatedUsing = OnRep_TableState)
	TArray<FBHBlackjackSeat> Seats;

	UPROPERTY(ReplicatedUsing = OnRep_TableState)
	TArray<int32> DealerCards;

	UPROPERTY(ReplicatedUsing = OnRep_TableState)
	bool bDealerHoleHidden = true;

	UPROPERTY(ReplicatedUsing = OnRep_TableState)
	uint8 Phase = static_cast<uint8>(EBJPhase::Idle);

	UPROPERTY(ReplicatedUsing = OnRep_TableState)
	int32 SeatedCount = 0;

	// Server-only: shuffled shoe, tap/hold timing, weak refs to seat owners (for prune), and the round clock.
	TArray<int32> Deck;
	TMap<int32, float> PressTimeByPlayerId;
	TArray<TWeakObjectPtr<ABHPlayerState>> SeatOwners;
	float PlayerTurnStartTime = 0.0f;
};
