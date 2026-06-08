// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainConnectFourTable.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

// Playable Connect-4 (7x6 upright board) for the train lobby. Solo vs AI (alpha-beta minimax) or human PvP.
// Look at a COLUMN on the standing board and TAP to drop your disc; first to four-in-a-row wins. Same
// seating/HUD conventions as the chess/tic-tac-toe tables.
UCLASS()
class BLACKOUTHUNT_API ABHTrainConnectFourTable : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainConnectFourTable();

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

	enum class EC4Phase : uint8 { Idle = 0, Playing, GameOver };

	float GetServerTimeSeconds() const;
	int32 ColumnUnderView(const ABHCharacter* Character) const;   // -1 if not looking at the board
	void HandleSeating(ABHCharacter* Character, bool bHold);
	void HandlePlayClick(ABHCharacter* Character, int32 Column);
	void StartNewGame(bool bAgainstAI);
	void DropDisc(int32 Column, int32 Side);
	void DoAIMove();
	void EndGame(const FString& Result, const FLinearColor& Accent);
	int32 SideForPlayer(int32 PlayerId) const;     // +1 red, -1 yellow, 0 = not seated
	void StatusToPlayer(ABHCharacter* Character, const FString& Message, float Duration = 2.0f) const;

	// rules (pure; 42-cell board, row*7+col, row 0 = bottom; 0 empty, +1 red, -1 yellow)
	static int32 LowestEmptyRow(const TArray<int32>& C, int32 Column);   // -1 if column full
	static int32 CheckWinner(const TArray<int32>& C);                    // +1 / -1 / 0
	static bool IsBoardFull(const TArray<int32>& C);
	static int32 EvaluateForRed(const TArray<int32>& C);
	static int32 Negamax(TArray<int32> C, int32 Side, int32 Depth, int32 Alpha, int32 Beta);

	void ApplyTableVisuals();
	void RefreshBoardVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BoardBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Discs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> TitleText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> StatusTextRender;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> TableLight;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	TArray<int32> Cells;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 RedPlayerId = -1;   // -1 empty, -2 AI

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 YellowPlayerId = -1;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString RedName;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString YellowName;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	uint8 Turn = 0;   // 0 = red, 1 = yellow

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	uint8 Phase = static_cast<uint8>(EC4Phase::Idle);

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	int32 LastCell = -1;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	bool bVsAI = false;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FString StatusText;

	UPROPERTY(ReplicatedUsing = OnRep_BoardState)
	FLinearColor StatusAccent = FLinearColor(0.60f, 0.80f, 1.0f, 1.0f);

	TMap<int32, float> PressTimeByPlayerId;
};
