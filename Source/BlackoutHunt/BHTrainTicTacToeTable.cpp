// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainTicTacToeTable.h"
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
	constexpr float BHTTTHoldSeconds = 0.35f;
	constexpr int32 BHTTTN = 3;
	constexpr int32 BHTTTCells = BHTTTN * BHTTTN;
	constexpr float BHTTTBoardTopZ = 86.0f;
	constexpr float BHTTTSquareSize = 22.0f;

	const FLinearColor TTTWin(0.30f, 1.0f, 0.52f, 1.0f);
	const FLinearColor TTTLose(1.0f, 0.40f, 0.26f, 1.0f);
	const FLinearColor TTTNeutral(0.55f, 0.78f, 1.0f, 1.0f);

	FVector TTTCellLocal(int32 Index, float ZOffset)
	{
		const int32 Col = Index % BHTTTN;
		const int32 Row = Index / BHTTTN;
		const float Half = (BHTTTN - 1) * 0.5f;
		return FVector((Col - Half) * BHTTTSquareSize, (Row - Half) * BHTTTSquareSize, ZOffset);
	}

	TCHAR TTTGlyph(int32 Mark)
	{
		return Mark > 0 ? TEXT('X') : (Mark < 0 ? TEXT('O') : TEXT('.'));
	}

	void BHConfigureTTTText(UTextRenderComponent* TextComponent, const FVector& Location, float WorldSize, const FColor& Color)
	{
		BHPropVisuals::ConfigureReadableText(TextComponent, Location, FRotator(0.0f, 90.0f, 0.0f), WorldSize, Color);
	}
}

ABHTrainTicTacToeTable::ABHTrainTicTacToeTable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Tic-Tac-Toe"));

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(SceneRoot);
	BoardBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardBody"));
	BoardBody->SetupAttachment(SceneRoot);

	Tiles.Reserve(BHTTTCells);
	Markers.Reserve(BHTTTCells);
	for (int32 Index = 0; Index < BHTTTCells; ++Index)
	{
		UStaticMeshComponent* Tile = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Tile_%d"), Index));
		Tile->SetupAttachment(SceneRoot);
		Tiles.Add(Tile);

		UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Marker_%d"), Index));
		Marker->SetupAttachment(SceneRoot);
		Markers.Add(Marker);
	}

	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(SceneRoot);
	BHConfigureTTTText(TitleText, FVector(0.0f, 48.0f, 138.0f), 11.0f, FColor(236, 226, 200));

	StatusTextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusTextRender->SetupAttachment(SceneRoot);
	BHConfigureTTTText(StatusTextRender, FVector(0.0f, 48.0f, 118.0f), 6.5f, FColor(206, 226, 244));

	TableLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TableLight"));
	TableLight->SetupAttachment(SceneRoot);
	TableLight->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	TableLight->SetIntensity(105.0f);
	TableLight->SetAttenuationRadius(340.0f);
	TableLight->SetLightColor(FLinearColor(0.85f, 0.86f, 0.95f, 1.0f));
	TableLight->SetCastShadows(false);

	Cells.Init(0, BHTTTCells);
	StatusText = TEXT("Open table - tap to take a seat.");
	ApplyTableVisuals();
	RefreshBoardVisuals();
}

void ABHTrainTicTacToeTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshBoardVisuals();
}

void ABHTrainTicTacToeTable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainTicTacToeTable, Cells);
	DOREPLIFETIME(ABHTrainTicTacToeTable, XPlayerId);
	DOREPLIFETIME(ABHTrainTicTacToeTable, OPlayerId);
	DOREPLIFETIME(ABHTrainTicTacToeTable, XName);
	DOREPLIFETIME(ABHTrainTicTacToeTable, OName);
	DOREPLIFETIME(ABHTrainTicTacToeTable, Turn);
	DOREPLIFETIME(ABHTrainTicTacToeTable, Phase);
	DOREPLIFETIME(ABHTrainTicTacToeTable, LastCell);
	DOREPLIFETIME(ABHTrainTicTacToeTable, bVsAI);
	DOREPLIFETIME(ABHTrainTicTacToeTable, StatusText);
	DOREPLIFETIME(ABHTrainTicTacToeTable, StatusAccent);
}

// ---------------------------------------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------------------------------------

bool ABHTrainTicTacToeTable::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainTicTacToeTable::BeginInteract_Implementation(ABHCharacter* Character)
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

void ABHTrainTicTacToeTable::EndInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return;
	}

	const int32 MyId = BHPS->GetPlayerId();
	const float* PressTime = PressTimeByPlayerId.Find(MyId);
	const float HeldFor = PressTime ? (GetServerTimeSeconds() - *PressTime) : 0.0f;
	PressTimeByPlayerId.Remove(MyId);
	const bool bHold = HeldFor >= BHTTTHoldSeconds;

	const ETTTPhase CurrentPhase = static_cast<ETTTPhase>(Phase);
	if (CurrentPhase == ETTTPhase::Idle)
	{
		HandleSeating(Character, bHold);
		return;
	}
	if (CurrentPhase == ETTTPhase::GameOver)
	{
		if (SideForPlayer(MyId) != 0)
		{
			StartNewGame(bVsAI);
			StatusToPlayer(Character, TEXT("Rematch! X to move."), 2.0f);
		}
		else
		{
			StatusToPlayer(Character, TEXT("Game over. A seated player can rematch."), 2.0f);
		}
		return;
	}

	HandlePlayClick(Character, CellUnderView(Character));
}

FText ABHTrainTicTacToeTable::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	const ETTTPhase CurrentPhase = static_cast<ETTTPhase>(Phase);
	switch (CurrentPhase)
	{
	case ETTTPhase::Idle:
		return FText::FromString(XPlayerId == -1 ? TEXT("Play Tic-Tac-Toe (sit as X)") : TEXT("Join as O  /  Hold: vs AI"));
	case ETTTPhase::GameOver:
		return FText::FromString(TEXT("Tic-Tac-Toe - Tap to rematch"));
	default:
		return FText::FromString(TEXT("Tic-Tac-Toe - Tap a square"));
	}
}

FBHInteractionPromptInfo ABHTrainTicTacToeTable::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("TIC-TAC-TOE"));
	}
	return Info;
}

void ABHTrainTicTacToeTable::OnRep_BoardState()
{
	RefreshBoardVisuals();
}

float ABHTrainTicTacToeTable::GetServerTimeSeconds() const
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ABHTrainTicTacToeTable::StatusToPlayer(ABHCharacter* Character, const FString& Message, float Duration) const
{
	if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
	{
		PC->ClientShowStatusMessage(Message, Duration);
	}
}

int32 ABHTrainTicTacToeTable::SideForPlayer(int32 PlayerId) const
{
	if (PlayerId == XPlayerId)
	{
		return 1;
	}
	if (PlayerId == OPlayerId)
	{
		return -1;
	}
	return 0;
}

int32 ABHTrainTicTacToeTable::CellUnderView(const ABHCharacter* Character) const
{
	if (!Character)
	{
		return -1;
	}
	FVector EyeLoc;
	FRotator EyeRot;
	Character->GetActorEyesViewPoint(EyeLoc, EyeRot);
	const FVector Dir = EyeRot.Vector();

	const FTransform& Xform = GetActorTransform();
	const FVector PlaneP = Xform.TransformPosition(FVector(0.0f, 0.0f, BHTTTBoardTopZ));
	const FVector PlaneN = Xform.GetUnitAxis(EAxis::Z);
	const float Denom = FVector::DotProduct(Dir, PlaneN);
	if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
	{
		return -1;
	}
	const float T = FVector::DotProduct(PlaneP - EyeLoc, PlaneN) / Denom;
	if (T <= 0.0f || T > 1500.0f)
	{
		return -1;
	}
	const FVector Local = Xform.InverseTransformPosition(EyeLoc + Dir * T);

	const float Half = (BHTTTN - 1) * 0.5f;
	const float HalfSpan = Half * BHTTTSquareSize + BHTTTSquareSize * 0.6f;
	if (FMath::Abs(Local.X) > HalfSpan || FMath::Abs(Local.Y) > HalfSpan)
	{
		return -1;
	}
	const int32 Col = FMath::Clamp(FMath::RoundToInt(Local.X / BHTTTSquareSize + Half), 0, BHTTTN - 1);
	const int32 Row = FMath::Clamp(FMath::RoundToInt(Local.Y / BHTTTSquareSize + Half), 0, BHTTTN - 1);
	return Row * BHTTTN + Col;
}

void ABHTrainTicTacToeTable::HandleSeating(ABHCharacter* Character, bool bHold)
{
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}
	const int32 MyId = BHPS->GetPlayerId();
	const FString MyName = BHPS->GetPlayerName();

	if (XPlayerId == -1)
	{
		XPlayerId = MyId;
		XName = MyName;
		bVsAI = false;
		BHPS->SetActiveMinigameTable(this);
		StatusText = TEXT("X seated - waiting for an opponent.");
		StatusAccent = TTTNeutral;
		ForceNetUpdate();
		RefreshBoardVisuals();
		StatusToPlayer(Character, TEXT("You are X. Tap with a friend as O, or HOLD to play the AI."), 3.5f);
		return;
	}

	if (MyId == XPlayerId)
	{
		if (bHold)
		{
			OPlayerId = -2;
			OName = TEXT("Computer");
			StartNewGame(true);
			StatusToPlayer(Character, TEXT("Game on - you are X vs the AI."), 3.0f);
		}
		else
		{
			StatusToPlayer(Character, TEXT("Waiting for an opponent. HOLD to play the AI."), 2.5f);
		}
		return;
	}

	if (OPlayerId == -1)
	{
		OPlayerId = MyId;
		OName = MyName;
		bVsAI = false;
		BHPS->SetActiveMinigameTable(this);
		StartNewGame(false);
		StatusToPlayer(Character, TEXT("You are O. X moves first."), 3.0f);
		return;
	}

	StatusToPlayer(Character, TEXT("Both seats are taken - enjoy the match."), 2.0f);
}

void ABHTrainTicTacToeTable::HandlePlayClick(ABHCharacter* Character, int32 Cell)
{
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}
	const int32 MyId = BHPS->GetPlayerId();
	const int32 MySide = SideForPlayer(MyId);
	if (MySide == 0)
	{
		StatusToPlayer(Character, TEXT("Both seats are taken - you're spectating."), 2.0f);
		return;
	}
	const int32 SideToMove = (Turn == 0) ? 1 : -1;
	if (MySide != SideToMove)
	{
		StatusToPlayer(Character, TEXT("Not your turn."), 1.5f);
		return;
	}
	if (Cell < 0)
	{
		StatusToPlayer(Character, TEXT("Look at a square on the board, then tap."), 1.5f);
		return;
	}
	if (!Cells.IsValidIndex(Cell) || Cells[Cell] != 0)
	{
		StatusToPlayer(Character, TEXT("That square is taken."), 1.5f);
		return;
	}

	MakeMove(Cell, MySide);
}

void ABHTrainTicTacToeTable::StartNewGame(bool bAgainstAI)
{
	Cells.Init(0, BHTTTCells);
	Turn = 0;
	LastCell = -1;
	bVsAI = bAgainstAI;
	Phase = static_cast<uint8>(ETTTPhase::Playing);
	StatusText = bAgainstAI ? TEXT("Your move (X) vs the AI.") : TEXT("X to move.");
	StatusAccent = TTTNeutral;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

void ABHTrainTicTacToeTable::MakeMove(int32 Cell, int32 Side)
{
	if (!Cells.IsValidIndex(Cell) || Cells[Cell] != 0)
	{
		return;
	}
	Cells[Cell] = Side;
	LastCell = Cell;

	const int32 Winner = CheckWinner(Cells);
	if (Winner != 0)
	{
		const FString WinnerName = (Winner > 0)
			? (XName.IsEmpty() ? TEXT("X") : XName)
			: (OName.IsEmpty() ? TEXT("O") : OName);
		EndGame(FString::Printf(TEXT("%s wins!"), *WinnerName), TTTWin);
		return;
	}
	if (IsBoardFull(Cells))
	{
		EndGame(TEXT("Draw - cat's game."), TTTNeutral);
		return;
	}

	Turn = (Turn == 0) ? 1 : 0;
	StatusText = (Turn == 0) ? TEXT("X to move.") : TEXT("O to move.");
	StatusAccent = TTTNeutral;
	ForceNetUpdate();
	RefreshBoardVisuals();

	if (bVsAI && Turn == 1 && static_cast<ETTTPhase>(Phase) == ETTTPhase::Playing)
	{
		DoAIMove();
	}
}

void ABHTrainTicTacToeTable::DoAIMove()
{
	// O is the AI (side -1): minimise the X-perspective score with a small random tie-break for variety.
	int32 BestScore = 1000;
	TArray<int32> BestCells;
	for (int32 Cell = 0; Cell < Cells.Num(); ++Cell)
	{
		if (Cells[Cell] != 0)
		{
			continue;
		}
		TArray<int32> Copy = Cells;
		Copy[Cell] = -1;
		const int32 Score = Minimax(Copy, 1);
		if (Score < BestScore)
		{
			BestScore = Score;
			BestCells.Reset();
			BestCells.Add(Cell);
		}
		else if (Score == BestScore)
		{
			BestCells.Add(Cell);
		}
	}
	if (BestCells.Num() == 0)
	{
		return;
	}
	MakeMove(BestCells[FMath::RandRange(0, BestCells.Num() - 1)], -1);
}

void ABHTrainTicTacToeTable::EndGame(const FString& Result, const FLinearColor& Accent)
{
	Phase = static_cast<uint8>(ETTTPhase::GameOver);
	StatusText = Result;
	StatusAccent = Accent;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

// ---------------------------------------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------------------------------------

int32 ABHTrainTicTacToeTable::CheckWinner(const TArray<int32>& C)
{
	if (C.Num() != BHTTTCells)
	{
		return 0;
	}
	static const int32 Lines[8][3] = {
		{ 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 },   // rows
		{ 0, 3, 6 }, { 1, 4, 7 }, { 2, 5, 8 },   // cols
		{ 0, 4, 8 }, { 2, 4, 6 }                 // diagonals
	};
	for (const int32(&Line)[3] : Lines)
	{
		const int32 Sum = C[Line[0]] + C[Line[1]] + C[Line[2]];
		if (Sum == 3)
		{
			return 1;
		}
		if (Sum == -3)
		{
			return -1;
		}
	}
	return 0;
}

bool ABHTrainTicTacToeTable::IsBoardFull(const TArray<int32>& C)
{
	for (const int32 Mark : C)
	{
		if (Mark == 0)
		{
			return false;
		}
	}
	return true;
}

int32 ABHTrainTicTacToeTable::Minimax(TArray<int32> C, int32 SideToMove)
{
	const int32 Winner = CheckWinner(C);
	if (Winner != 0)
	{
		return Winner * 10;   // +10 X win, -10 O win
	}
	if (IsBoardFull(C))
	{
		return 0;
	}

	int32 Best = (SideToMove > 0) ? -1000 : 1000;
	for (int32 Cell = 0; Cell < C.Num(); ++Cell)
	{
		if (C[Cell] != 0)
		{
			continue;
		}
		C[Cell] = SideToMove;
		const int32 Score = Minimax(C, -SideToMove);
		C[Cell] = 0;
		Best = (SideToMove > 0) ? FMath::Max(Best, Score) : FMath::Min(Best, Score);
	}
	return Best;
}

// ---------------------------------------------------------------------------------------------------------
// HUD readout
// ---------------------------------------------------------------------------------------------------------

void ABHTrainTicTacToeTable::GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const
{
	OutLines.Reset();
	OutColor = StatusAccent;

	const int32 ViewerSide = SideForPlayer(PlayerId);
	const FString You = (ViewerSide > 0) ? TEXT("X") : (ViewerSide < 0 ? TEXT("O") : TEXT("Spectating"));
	const FString Opponent = (ViewerSide > 0)
		? (bVsAI ? TEXT("Computer") : (OName.IsEmpty() ? TEXT("(open)") : OName))
		: (XName.IsEmpty() ? TEXT("X") : XName);
	OutLines.Add(FString::Printf(TEXT("TIC-TAC-TOE   You: %s   vs %s"), *You, *Opponent));

	if (Cells.Num() == BHTTTCells)
	{
		for (int32 Row = 0; Row < BHTTTN; ++Row)
		{
			OutLines.Add(FString::Printf(TEXT(" %c | %c | %c "),
				TTTGlyph(Cells[Row * BHTTTN + 0]), TTTGlyph(Cells[Row * BHTTTN + 1]), TTTGlyph(Cells[Row * BHTTTN + 2])));
			if (Row < BHTTTN - 1)
			{
				OutLines.Add(TEXT("---+---+---"));
			}
		}
	}

	const ETTTPhase CurrentPhase = static_cast<ETTTPhase>(Phase);
	if (CurrentPhase == ETTTPhase::Playing && ViewerSide != 0)
	{
		const int32 SideToMove = (Turn == 0) ? 1 : -1;
		OutLines.Add(ViewerSide == SideToMove ? TEXT(">> YOUR MOVE <<") : TEXT("Waiting for opponent..."));
	}
	else
	{
		OutLines.Add(StatusText);
	}
}

// ---------------------------------------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------------------------------------

void ABHTrainTicTacToeTable::ApplyTableVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const FLinearColor Wood(0.20f, 0.13f, 0.08f, 1.0f);
	const float BoardSpan = BHTTTN * BHTTTSquareSize;
	BHPropVisuals::ConfigurePart(Base, BHPropVisuals::CylinderMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 41.0f), FRotator::ZeroRotator, FVector(0.30f, 0.30f, 0.82f), true);
	BHPropVisuals::ConfigurePart(BoardBody, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, 82.0f), FRotator::ZeroRotator, FVector(BoardSpan / 100.0f + 0.06f, BoardSpan / 100.0f + 0.06f, 0.06f), true);
	BHPropVisuals::TintPart(Base, Wood);
	BHPropVisuals::TintPart(BoardBody, Wood * 1.3f);

	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(Tiles[Index], BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), TTTCellLocal(Index, BHTTTBoardTopZ), FRotator::ZeroRotator, FVector(BHTTTSquareSize / 100.0f * 0.92f, BHTTTSquareSize / 100.0f * 0.92f, 0.02f), false);
	}
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(Markers[Index], BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), TTTCellLocal(Index, BHTTTBoardTopZ + 6.0f), FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.07f), false);
	}
}

void ABHTrainTicTacToeTable::RefreshBoardVisuals()
{
	const FLinearColor GridLight(0.78f, 0.74f, 0.60f, 1.0f);
	const FLinearColor GridDark(0.24f, 0.28f, 0.34f, 1.0f);
	const FLinearColor LastMoveTint(1.0f, 0.84f, 0.30f, 1.0f);
	const FLinearColor XColor(1.0f, 0.45f, 0.28f, 1.0f);   // warm
	const FLinearColor OColor(0.34f, 0.72f, 1.0f, 1.0f);   // cool

	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		if (!Tiles[Index])
		{
			continue;
		}
		const int32 Row = Index / BHTTTN;
		const int32 Col = Index % BHTTTN;
		FLinearColor TileColor = ((Row + Col) % 2 == 0) ? GridLight : GridDark;
		float Emissive = 0.0f;
		if (Index == LastCell)
		{
			TileColor = LastMoveTint;
			Emissive = 0.4f;
		}
		BHPropVisuals::TintPart(Tiles[Index], TileColor, Emissive);
	}

	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		if (!Markers[Index])
		{
			continue;
		}
		const int32 Mark = Cells.IsValidIndex(Index) ? Cells[Index] : 0;
		if (Mark == 0)
		{
			BHPropVisuals::SetPartVisible(Markers[Index], false);
			continue;
		}
		// X = a warm angular cube; O = a cool round cylinder. Distinct shape AND colour.
		UStaticMesh* MarkMesh = (Mark > 0) ? BHPropVisuals::CubeMesh() : BHPropVisuals::CylinderMesh();
		const FVector MarkScale = (Mark > 0) ? FVector(0.11f, 0.11f, 0.05f) : FVector(0.12f, 0.12f, 0.05f);
		const float Yaw = (Mark > 0) ? 45.0f : 0.0f;
		Markers[Index]->SetStaticMesh(MarkMesh);
		Markers[Index]->SetRelativeScale3D(MarkScale);
		Markers[Index]->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
		Markers[Index]->SetRelativeLocation(TTTCellLocal(Index, BHTTTBoardTopZ + 4.0f));
		BHPropVisuals::SetPartVisible(Markers[Index], true);
		BHPropVisuals::TintPart(Markers[Index], Mark > 0 ? XColor : OColor, 0.45f);
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("TIC-TAC-TOE")));
	}
	if (StatusTextRender)
	{
		StatusTextRender->SetText(FText::FromString(StatusText));
		StatusTextRender->SetTextRenderColor(StatusAccent.ToFColor(true));
	}
}
