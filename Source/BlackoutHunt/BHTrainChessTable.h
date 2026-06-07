// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainChessTable.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

// A playable, server-authoritative "fast chess" table for the train lobby. It is a 6x6 minichess (Rook,
// Knight, Bishop, Queen, King + 6 pawns a side) so a game finishes in a couple of minutes, which suits the
// short rides. Win by CAPTURING THE KING (no check/checkmate/castling/en-passant) -- the simplification that
// keeps it quick and robust. Each piece type has a distinct marker shape/height, and the king wears a gold
// crown so it is unmistakable.
//
// Modes: a lone player claims White and can play vs a built-in AI (negamax), or a second human claims Black
// for PvP. Selection reuses the lobby's tap/hold interact: you look at a square (server ray-casts the board
// plane), TAP to pick up your piece, TAP a destination to move, HOLD to deselect. The authoritative board is
// replicated; the physical tiles/markers show the layout + highlights, and each player's HUD draws the full
// lettered board (oriented for their colour) + whose move it is (see ABHHUD::DrawMinigameStatus).
UCLASS()
class BLACKOUTHUNT_API ABHTrainChessTable : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainChessTable();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	// Client HUD: multi-line board readout for the given player (board oriented for their colour). Returns the
	// lines + an accent colour. Safe to call on any client; reads only replicated state.
	void GetHudBoardLines(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const;

protected:
	UFUNCTION()
	void OnRep_BoardState();

	enum class EChessPhase : uint8
	{
		Idle = 0,    // open table; tap to take a seat
		Playing,     // a game is in progress
		GameOver     // finished; tap to rematch
	};

	// ---- interaction / flow (server) ----
	float GetServerTimeSeconds() const;
	int32 SquareUnderView(const ABHCharacter* Character) const;   // -1 if not looking at the board
	void HandleSeating(ABHCharacter* Character, bool bHold);
	void HandlePlayClick(ABHCharacter* Character, int32 Square, bool bHold);
	void StartNewGame(bool bAgainstAI);
	void MakeMoveAuthoritative(int32 From, int32 To);
	void DoAIMove();
	void EndGame(const FString& Result, const FLinearColor& Accent);
	int32 SideForPlayer(int32 PlayerId) const;     // +1 white, -1 black, 0 = not seated
	void StatusToPlayer(ABHCharacter* Character, const FString& Message, float Duration = 2.0f) const;

	// ---- chess rules (pure; operate on a 25-int board: 0 empty, +piece white, -piece black) ----
	static void InitBoard(TArray<int32>& B);
	static void PseudoLegalMoves(const TArray<int32>& B, int32 Side, TArray<int32>& OutFromTo);  // packed From*25+To
	static bool IsLegalMove(const TArray<int32>& B, int32 Side, int32 From, int32 To);
	static int32 ApplyMove(TArray<int32>& B, int32 From, int32 To);  // returns the captured piece (0 if none)
	static bool HasKing(const TArray<int32>& B, int32 Side);
	static int32 EvaluateForWhite(const TArray<int32>& B);
	static int32 Negamax(TArray<int32> B, int32 Side, int32 Depth, int32 Alpha, int32 Beta);
	static int32 PieceValue(int32 Piece);
	static TCHAR PieceGlyph(int32 Piece);

	// ---- visuals ----
	void ApplyTableVisuals();
	void RefreshBoardVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BoardBody;

	// One checker tile per square (also the highlight surface) and one piece marker per square (shown where
	// occupied; its mesh/height encode the piece type and the colour tints by side).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Tiles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Markers;

	// Gold crowns that float over each side's king so the king reads at a glance.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WhiteKingCrown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BlackKingCrown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TitleText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> StatusTextRender;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> TableLight;

	// ---- replicated state (one shared OnRep drives all client visuals) ----
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	TArray<int32> Board;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 WhitePlayerId = -1;   // -1 empty, -2 AI

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 BlackPlayerId = -1;   // -1 empty, -2 AI

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString WhiteName;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString BlackName;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	uint8 Turn = 0;   // 0 = white to move, 1 = black to move

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	uint8 Phase = static_cast<uint8>(EChessPhase::Idle);

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 SelectedSquare = -1;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	bool bVsAI = false;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString StatusText;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FLinearColor StatusAccent = FLinearColor(0.60f, 0.80f, 1.0f, 1.0f);

	// Server-only: per-player press time for the tap-vs-hold measurement (same trick as the blackjack table).
	TMap<int32, float> PressTimeByPlayerId;
};
