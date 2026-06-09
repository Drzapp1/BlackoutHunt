// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainOthelloTable.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class ABHPlayerState;

// Playable Othello / Reversi (8x8) for the train lobby. Solo vs AI (minimax) or human PvP. Look at a legal
// cell and TAP to place a disc, flanking and flipping the opponent's discs; most discs at the end wins. Same
// seating/HUD conventions as the chess table. Legal cells are highlighted for the player to move.
UCLASS()
class BLACKOUTHUNT_API ABHTrainOthelloTable : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainOthelloTable();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const;

protected:
	UFUNCTION()
	void OnRep_BoardState();

	enum class EOthPhase : uint8 { Idle = 0, Playing, GameOver };

	float GetServerTimeSeconds() const;
	int32 CellUnderView(const ABHCharacter* Character) const;
	void HandleSeating(ABHCharacter* Character, bool bHold);
	void PruneSeats();   // server: free a seat whose player disconnected / walked away, reopening the table
	void HandlePlayClick(ABHCharacter* Character, int32 Cell);
	void StartNewGame(bool bAgainstAI);
	void PlaceAndAdvance(int32 Cell, int32 Side);
	void DoAIMove();
	void EndGame();
	int32 SideForPlayer(int32 PlayerId) const;   // +1 black, -1 white, 0 = not seated
	void StatusToPlayer(ABHCharacter* Character, const FString& Message, float Duration = 2.0f) const;

	// rules (pure; 64-cell board, row*8+col; 0 empty, +1 black, -1 white)
	static void InitBoard(TArray<int32>& B);
	static int32 FlipsForMove(const TArray<int32>& B, int32 Cell, int32 Side, TArray<int32>* OutFlips);
	static void CollectLegalMoves(const TArray<int32>& B, int32 Side, TArray<int32>& OutCells);
	static int32 ApplyMove(TArray<int32>& B, int32 Cell, int32 Side);   // returns discs flipped
	static int32 EvaluateForBlack(const TArray<int32>& B);
	static int32 Negamax(TArray<int32> B, int32 Side, int32 Depth, int32 Alpha, int32 Beta);
	static int32 CountDiscs(const TArray<int32>& B, int32 Side);

	void ApplyTableVisuals();
	void RefreshBoardVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Base;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BoardBody;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Tiles;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Discs;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TitleText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> StatusTextRender;

	// Back-facing duplicates so the player on the far (-Y) side reads the title/status the right way round.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TitleTextBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> StatusTextBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> TableLight;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	TArray<int32> Board;
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 BlackPlayerId = -1;   // -1 empty, -2 AI
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 WhitePlayerId = -1;
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString BlackName;
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString WhiteName;
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	uint8 Turn = 0;   // 0 = black, 1 = white
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	uint8 Phase = static_cast<uint8>(EOthPhase::Idle);
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	bool bVsAI = false;
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString StatusText;
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FLinearColor StatusAccent = FLinearColor(0.60f, 0.80f, 1.0f, 1.0f);

	TMap<int32, float> PressTimeByPlayerId;

	// Server-only seat owners (mirror the replicated ids) so a disconnected / departed player frees their seat.
	TWeakObjectPtr<ABHPlayerState> BlackOwner;
	TWeakObjectPtr<ABHPlayerState> WhiteOwner;
};
