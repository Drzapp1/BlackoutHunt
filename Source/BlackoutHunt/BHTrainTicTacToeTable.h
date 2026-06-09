// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainTicTacToeTable.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class ABHPlayerState;

// A playable, server-authoritative tic-tac-toe table for the train lobby. Solo vs a (perfect) AI or human PvP,
// using the same look-and-tap flow as the chess table: look at a cell and TAP to place your mark. X goes
// first. The board is replicated; each player's HUD shows the grid + whose turn (ABHHUD::DrawMinigameStatus),
// and the physical board shows X (warm cube) / O (cool cylinder) markers.
UCLASS()
class BLACKOUTHUNT_API ABHTrainTicTacToeTable : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainTicTacToeTable();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual void EndInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	// Client HUD: multi-line board readout for the given player. Reads only replicated state.
	void GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const;

protected:
	UFUNCTION()
	void OnRep_BoardState();

	enum class ETTTPhase : uint8
	{
		Idle = 0,
		Playing,
		GameOver
	};

	// ---- flow (server) ----
	float GetServerTimeSeconds() const;
	int32 CellUnderView(const ABHCharacter* Character) const;   // -1 if not looking at the board
	void HandleSeating(ABHCharacter* Character, bool bHold);
	void PruneSeats();   // server: free a seat whose player disconnected / walked away, reopening the table
	void HandlePlayClick(ABHCharacter* Character, int32 Cell);
	void StartNewGame(bool bAgainstAI);
	void MakeMove(int32 Cell, int32 Side);
	void DoAIMove();
	void EndGame(const FString& Result, const FLinearColor& Accent);
	int32 SideForPlayer(int32 PlayerId) const;     // +1 X, -1 O, 0 = not seated
	void StatusToPlayer(ABHCharacter* Character, const FString& Message, float Duration = 2.0f) const;

	// ---- rules (pure; 9-cell board: 0 empty, +1 X, -1 O) ----
	static int32 CheckWinner(const TArray<int32>& C);   // +1 X, -1 O, 0 none
	static bool IsBoardFull(const TArray<int32>& C);
	static int32 Minimax(TArray<int32> C, int32 SideToMove);

	// ---- visuals ----
	void ApplyTableVisuals();
	void RefreshBoardVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BoardBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Tiles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Markers;

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

	// ---- replicated state ----
	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	TArray<int32> Cells;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 XPlayerId = -1;   // -1 empty, -2 AI

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 OPlayerId = -1;   // -1 empty, -2 AI

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString XName;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString OName;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	uint8 Turn = 0;   // 0 = X to move, 1 = O to move

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	uint8 Phase = static_cast<uint8>(ETTTPhase::Idle);

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 LastCell = -1;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	bool bVsAI = false;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString StatusText;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FLinearColor StatusAccent = FLinearColor(0.60f, 0.80f, 1.0f, 1.0f);

	// Server-only: per-player press time (we measure tap/hold ourselves, same as the other tables).
	TMap<int32, float> PressTimeByPlayerId;

	// Server-only seat owners (mirror the replicated ids) so a disconnected / departed player frees their seat.
	TWeakObjectPtr<ABHPlayerState> XOwner;
	TWeakObjectPtr<ABHPlayerState> OOwner;
};
