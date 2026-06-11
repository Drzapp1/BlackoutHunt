// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainChessTable.h"
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
	constexpr float BHChessHoldSeconds = 0.35f;   // press longer than this == deselect
	constexpr int32 BHChessAIDepth = 3;           // negamax search depth (tiny board, this is instant)
	constexpr int32 BHChessN = 6;                 // board is 6x6
	constexpr int32 BHChessCells = BHChessN * BHChessN;
	constexpr float BHBoardTopZ = 86.0f;          // tile surface height above the actor origin
	constexpr float BHSquareSize = 16.0f;         // cm per square
	constexpr int32 BHChessINF = 1000000000;

	// Piece encoding: 0 empty; +ve white, -ve black; magnitude 1=Pawn 2=Knight 3=Bishop 4=Rook 5=Queen 6=King.
	const FLinearColor BHChessWin(0.30f, 1.0f, 0.52f, 1.0f);
	const FLinearColor BHChessLose(1.0f, 0.40f, 0.26f, 1.0f);
	const FLinearColor BHChessNeutral(0.55f, 0.78f, 1.0f, 1.0f);

	FVector SquareLocal(int32 Index, float ZOffset)
	{
		const int32 Col = Index % BHChessN;
		const int32 Row = Index / BHChessN;
		const float Half = (BHChessN - 1) * 0.5f;
		return FVector((Col - Half) * BHSquareSize, (Row - Half) * BHSquareSize, ZOffset);
	}

	FString SquareName(int32 Index)
	{
		if (Index < 0 || Index >= BHChessCells)
		{
			return TEXT("-");
		}
		const TCHAR File = static_cast<TCHAR>('a' + (Index % BHChessN));
		const int32 Rank = (Index / BHChessN) + 1;
		return FString::Printf(TEXT("%c%d"), File, Rank);
	}

	void BHConfigureChessText(UTextRenderComponent* TextComponent, const FVector& Location, float WorldSize, const FColor& Color)
	{
		BHPropVisuals::ConfigureReadableText(TextComponent, Location, FRotator(0.0f, 90.0f, 0.0f), WorldSize, Color);
	}

	// Each piece type gets a distinct silhouette so the board reads at a glance (the king is the tallest and
	// also wears a crown). Only engine basic shapes are available, so type is conveyed by mesh + height.
	void BHChessMarkerStyle(int32 Piece, UStaticMesh*& OutMesh, FVector& OutScale, float& OutYaw)
	{
		switch (FMath::Abs(Piece))
		{
		case 1: OutMesh = BHPropVisuals::SphereMesh();   OutScale = FVector(0.052f, 0.052f, 0.052f); OutYaw = 0.0f;  break;   // pawn: small ball
		case 2: OutMesh = BHPropVisuals::CubeMesh();     OutScale = FVector(0.060f, 0.052f, 0.085f); OutYaw = 45.0f; break;   // knight: angled block
		case 3: OutMesh = BHPropVisuals::CylinderMesh(); OutScale = FVector(0.045f, 0.045f, 0.120f); OutYaw = 0.0f;  break;   // bishop: narrow + tall
		case 4: OutMesh = BHPropVisuals::CubeMesh();     OutScale = FVector(0.080f, 0.080f, 0.090f); OutYaw = 0.0f;  break;   // rook: blocky tower
		case 5: OutMesh = BHPropVisuals::CylinderMesh(); OutScale = FVector(0.064f, 0.064f, 0.150f); OutYaw = 0.0f;  break;   // queen: tall column
		case 6: OutMesh = BHPropVisuals::CylinderMesh(); OutScale = FVector(0.074f, 0.074f, 0.190f); OutYaw = 0.0f;  break;   // king: tallest (+ crown)
		default: OutMesh = BHPropVisuals::CylinderMesh(); OutScale = FVector(0.05f, 0.05f, 0.05f);   OutYaw = 0.0f;  break;
		}
	}
}

ABHTrainChessTable::ABHTrainChessTable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Chess"));

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(SceneRoot);
	BoardBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardBody"));
	BoardBody->SetupAttachment(SceneRoot);

	Tiles.Reserve(BHChessCells);
	Markers.Reserve(BHChessCells);
	for (int32 Index = 0; Index < BHChessCells; ++Index)
	{
		UStaticMeshComponent* Tile = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Tile_%d"), Index));
		Tile->SetupAttachment(SceneRoot);
		Tiles.Add(Tile);

		UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Marker_%d"), Index));
		Marker->SetupAttachment(SceneRoot);
		Markers.Add(Marker);
	}

	WhiteKingCrown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WhiteKingCrown"));
	WhiteKingCrown->SetupAttachment(SceneRoot);
	BlackKingCrown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlackKingCrown"));
	BlackKingCrown->SetupAttachment(SceneRoot);

	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(SceneRoot);
	BHConfigureChessText(TitleText, FVector(0.0f, 60.0f, 150.0f), 12.0f, FColor(236, 226, 200));

	StatusTextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusTextRender->SetupAttachment(SceneRoot);
	BHConfigureChessText(StatusTextRender, FVector(0.0f, 60.0f, 128.0f), 7.0f, FColor(206, 226, 244));

	// Mirrored copies facing the opposite (-Y) seat so both players read the text the right way round.
	TitleTextBack = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleTextBack"));
	TitleTextBack->SetupAttachment(SceneRoot);
	BHPropVisuals::ConfigureReadableText(TitleTextBack, FVector(0.0f, -60.0f, 150.0f), FRotator(0.0f, -90.0f, 0.0f), 12.0f, FColor(236, 226, 200));

	StatusTextBack = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusTextBack"));
	StatusTextBack->SetupAttachment(SceneRoot);
	BHPropVisuals::ConfigureReadableText(StatusTextBack, FVector(0.0f, -60.0f, 128.0f), FRotator(0.0f, -90.0f, 0.0f), 7.0f, FColor(206, 226, 244));

	TableLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TableLight"));
	TableLight->SetupAttachment(SceneRoot);
	TableLight->SetRelativeLocation(FVector(0.0f, 0.0f, 165.0f));
	TableLight->SetIntensity(110.0f);
	TableLight->SetAttenuationRadius(360.0f);
	TableLight->SetLightColor(FLinearColor(0.78f, 0.82f, 0.95f, 1.0f));
	TableLight->SetCastShadows(false);

	InitBoard(Board);
	StatusText = TEXT("Open table - tap to take a seat.");
	ApplyTableVisuals();
	RefreshBoardVisuals();
}

void ABHTrainChessTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		PruneSeats();
	}
	RefreshBoardVisuals();
}

void ABHTrainChessTable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainChessTable, Board);
	DOREPLIFETIME(ABHTrainChessTable, WhitePlayerId);
	DOREPLIFETIME(ABHTrainChessTable, BlackPlayerId);
	DOREPLIFETIME(ABHTrainChessTable, WhiteName);
	DOREPLIFETIME(ABHTrainChessTable, BlackName);
	DOREPLIFETIME(ABHTrainChessTable, Turn);
	DOREPLIFETIME(ABHTrainChessTable, Phase);
	DOREPLIFETIME(ABHTrainChessTable, SelectedSquare);
	DOREPLIFETIME(ABHTrainChessTable, bVsAI);
	DOREPLIFETIME(ABHTrainChessTable, StatusText);
	DOREPLIFETIME(ABHTrainChessTable, StatusAccent);
}

// ---------------------------------------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------------------------------------

bool ABHTrainChessTable::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainChessTable::BeginInteract_Implementation(ABHCharacter* Character)
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

void ABHTrainChessTable::EndInteract_Implementation(ABHCharacter* Character)
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
	const bool bHold = HeldFor >= BHChessHoldSeconds;

	const EChessPhase CurrentPhase = static_cast<EChessPhase>(Phase);
	if (CurrentPhase == EChessPhase::Idle)
	{
		HandleSeating(Character, bHold);
		return;
	}
	if (CurrentPhase == EChessPhase::GameOver)
	{
		if (SideForPlayer(MyId) != 0)
		{
			StartNewGame(bVsAI);
			StatusToPlayer(Character, TEXT("Rematch! White to move."), 2.0f);
		}
		else
		{
			StatusToPlayer(Character, TEXT("Game over. A seated player can rematch."), 2.0f);
		}
		return;
	}

	const int32 Square = bHold ? -1 : SquareUnderView(Character);
	HandlePlayClick(Character, Square, bHold);
}

FText ABHTrainChessTable::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	const EChessPhase CurrentPhase = static_cast<EChessPhase>(Phase);
	switch (CurrentPhase)
	{
	case EChessPhase::Idle:
		return FText::FromString(WhitePlayerId == -1 ? TEXT("Play Chess (sit as White)") : TEXT("Join Chess as Black  /  Hold: vs AI"));
	case EChessPhase::GameOver:
		return FText::FromString(TEXT("Chess - Tap to rematch"));
	default:
		return FText::FromString(TEXT("Chess - Tap: select/move  Hold: deselect"));
	}
}

FBHInteractionPromptInfo ABHTrainChessTable::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;   // we measure hold ourselves so a tap stays instant
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("FAST CHESS"));
	}
	return Info;
}

void ABHTrainChessTable::OnRep_BoardState()
{
	RefreshBoardVisuals();
}

float ABHTrainChessTable::GetServerTimeSeconds() const
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ABHTrainChessTable::StatusToPlayer(ABHCharacter* Character, const FString& Message, float Duration) const
{
	if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
	{
		PC->ClientShowStatusMessage(Message, Duration);
	}
}

int32 ABHTrainChessTable::SideForPlayer(int32 PlayerId) const
{
	if (PlayerId == WhitePlayerId)
	{
		return 1;
	}
	if (PlayerId == BlackPlayerId)
	{
		return -1;
	}
	return 0;
}

int32 ABHTrainChessTable::SquareUnderView(const ABHCharacter* Character) const
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
	const FVector PlaneP = Xform.TransformPosition(FVector(0.0f, 0.0f, BHBoardTopZ));
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

	const float Half = (BHChessN - 1) * 0.5f;
	const float HalfSpan = Half * BHSquareSize + BHSquareSize * 0.6f;
	if (FMath::Abs(Local.X) > HalfSpan || FMath::Abs(Local.Y) > HalfSpan)
	{
		return -1;
	}
	const int32 Col = FMath::Clamp(FMath::RoundToInt(Local.X / BHSquareSize + Half), 0, BHChessN - 1);
	const int32 Row = FMath::Clamp(FMath::RoundToInt(Local.Y / BHSquareSize + Half), 0, BHChessN - 1);
	return Row * BHChessN + Col;
}

void ABHTrainChessTable::HandleSeating(ABHCharacter* Character, bool bHold)
{
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}
	const int32 MyId = BHPS->GetPlayerId();
	const FString MyName = BHPS->GetPlayerName();

	if (WhitePlayerId == -1)
	{
		WhitePlayerId = MyId;
		WhiteName = MyName;
		WhiteOwner = BHPS;
		bVsAI = false;
		BHPS->SetActiveMinigameTable(this);
		StatusText = TEXT("White seated - waiting for an opponent.");
		StatusAccent = BHChessNeutral;
		ForceNetUpdate();
		RefreshBoardVisuals();
		StatusToPlayer(Character, TEXT("You are White. Tap again with a friend as Black, or HOLD to play the AI."), 3.5f);
		return;
	}

	if (MyId == WhitePlayerId)
	{
		if (bHold)
		{
			BlackPlayerId = -2;
			BlackName = TEXT("Computer");
			StartNewGame(true);
			StatusToPlayer(Character, TEXT("Game on - you are White vs the AI."), 3.0f);
		}
		else
		{
			StatusToPlayer(Character, TEXT("Waiting for an opponent. HOLD to play the AI."), 2.5f);
		}
		return;
	}

	if (BlackPlayerId == -1)
	{
		BlackPlayerId = MyId;
		BlackName = MyName;
		BlackOwner = BHPS;
		bVsAI = false;
		BHPS->SetActiveMinigameTable(this);
		StartNewGame(false);
		StatusToPlayer(Character, TEXT("You are Black. White moves first."), 3.0f);
		return;
	}

	StatusToPlayer(Character, TEXT("Both seats are taken - enjoy the match."), 2.0f);
}

void ABHTrainChessTable::HandlePlayClick(ABHCharacter* Character, int32 Square, bool bHold)
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
		StatusToPlayer(Character, TEXT("Not your move."), 1.5f);
		return;
	}

	if (bHold)
	{
		SelectedSquare = -1;
		ForceNetUpdate();
		RefreshBoardVisuals();
		return;
	}
	if (Square < 0)
	{
		StatusToPlayer(Character, TEXT("Look at a square on the board, then tap."), 1.5f);
		return;
	}

	const int32 PieceHere = Board.IsValidIndex(Square) ? Board[Square] : 0;

	if (SelectedSquare == -1)
	{
		if (PieceHere != 0 && FMath::Sign(PieceHere) == MySide)
		{
			SelectedSquare = Square;
			StatusText = FString::Printf(TEXT("%s selected %s."), (MySide > 0 ? TEXT("White") : TEXT("Black")), *SquareName(Square));
			ForceNetUpdate();
			RefreshBoardVisuals();
		}
		else
		{
			StatusToPlayer(Character, TEXT("Select one of your own pieces."), 1.5f);
		}
		return;
	}

	if (Square == SelectedSquare)
	{
		SelectedSquare = -1;
		ForceNetUpdate();
		RefreshBoardVisuals();
		return;
	}
	if (PieceHere != 0 && FMath::Sign(PieceHere) == MySide)
	{
		SelectedSquare = Square;
		ForceNetUpdate();
		RefreshBoardVisuals();
		return;
	}

	if (IsLegalMove(Board, MySide, SelectedSquare, Square))
	{
		MakeMoveAuthoritative(SelectedSquare, Square);
	}
	else
	{
		StatusToPlayer(Character, TEXT("That piece can't move there."), 1.5f);
	}
}

void ABHTrainChessTable::StartNewGame(bool bAgainstAI)
{
	InitBoard(Board);
	Turn = 0;
	SelectedSquare = -1;
	bVsAI = bAgainstAI;
	Phase = static_cast<uint8>(EChessPhase::Playing);
	StatusText = bAgainstAI ? TEXT("Your move (White) vs the AI.") : TEXT("White to move.");
	StatusAccent = BHChessNeutral;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

void ABHTrainChessTable::MakeMoveAuthoritative(int32 From, int32 To)
{
	if (!Board.IsValidIndex(From) || !Board.IsValidIndex(To))
	{
		return;
	}
	const int32 Mover = FMath::Sign(Board[From]);
	const int32 Captured = ApplyMove(Board, From, To);
	SelectedSquare = -1;

	if (FMath::Abs(Captured) == 6)
	{
		const FString Winner = (Mover > 0)
			? (WhiteName.IsEmpty() ? TEXT("White") : WhiteName)
			: (BlackName.IsEmpty() ? TEXT("Black") : BlackName);
		EndGame(FString::Printf(TEXT("%s wins - king captured!"), *Winner), BHChessWin);
		return;
	}

	Turn = (Turn == 0) ? 1 : 0;

	// In this simplified ruleset (win by king capture, no check) a side with zero moves loses outright —
	// the AI path already applies that rule in DoAIMove, but a PvP game reaching the same position would
	// otherwise soft-stall forever: every click answers "can't move there" and the table stays claimed
	// until both players walk away. Apply the rule for whoever is to move, regardless of opponent type.
	const int32 SideToMove = (Turn == 0) ? 1 : -1;
	TArray<int32> AvailableMoves;
	PseudoLegalMoves(Board, SideToMove, AvailableMoves);
	if (AvailableMoves.Num() == 0)
	{
		const FString Winner = (Mover > 0)
			? (WhiteName.IsEmpty() ? TEXT("White") : WhiteName)
			: (BlackName.IsEmpty() ? TEXT("Black") : BlackName);
		EndGame(FString::Printf(TEXT("%s has no moves - %s wins."),
			SideToMove > 0 ? TEXT("White") : TEXT("Black"),
			*Winner), BHChessWin);
		return;
	}

	StatusText = (Turn == 0) ? TEXT("White to move.") : TEXT("Black to move.");
	StatusAccent = BHChessNeutral;
	ForceNetUpdate();
	RefreshBoardVisuals();

	if (bVsAI && Turn == 1 && static_cast<EChessPhase>(Phase) == EChessPhase::Playing)
	{
		DoAIMove();
	}
}

void ABHTrainChessTable::DoAIMove()
{
	TArray<int32> Moves;
	PseudoLegalMoves(Board, -1, Moves);
	if (Moves.Num() == 0)
	{
		EndGame(TEXT("Black has no moves - White wins."), BHChessWin);
		return;
	}

	int32 BestScore = -BHChessINF;
	TArray<int32> BestMoves;
	for (const int32 Move : Moves)
	{
		TArray<int32> Copy = Board;
		ApplyMove(Copy, Move / BHChessCells, Move % BHChessCells);
		const int32 Score = -Negamax(Copy, 1, BHChessAIDepth - 1, -BHChessINF, BHChessINF);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestMoves.Reset();
			BestMoves.Add(Move);
		}
		else if (Score == BestScore)
		{
			BestMoves.Add(Move);
		}
	}

	// Slip up sometimes so a human can actually win instead of facing flawless play: with this chance the AI makes
	// a deliberate blunder (any legal move) rather than its best one.
	const int32 Chosen = (FMath::FRand() < 0.22f)
		? Moves[FMath::RandRange(0, Moves.Num() - 1)]
		: BestMoves[FMath::RandRange(0, BestMoves.Num() - 1)];
	MakeMoveAuthoritative(Chosen / BHChessCells, Chosen % BHChessCells);
}

void ABHTrainChessTable::EndGame(const FString& Result, const FLinearColor& Accent)
{
	Phase = static_cast<uint8>(EChessPhase::GameOver);
	SelectedSquare = -1;
	StatusText = Result;
	StatusAccent = Accent;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

void ABHTrainChessTable::PruneSeats()
{
	// A human seat is "gone" if its owning PlayerState was destroyed (disconnect) or its pawn walked away. AI
	// (-2) and empty (-1) seats are never pruned.
	constexpr float SeatRange = 900.0f;
	auto SeatGone = [this](int32 SeatId, const TWeakObjectPtr<ABHPlayerState>& SeatOwnerPtr) -> bool
	{
		if (SeatId < 0)
		{
			return false;
		}
		const ABHPlayerState* PS = SeatOwnerPtr.Get();
		if (!PS)
		{
			return true;
		}
		const APawn* Pawn = PS->GetPawn();
		return !Pawn || FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > SeatRange;
	};

	if (!SeatGone(WhitePlayerId, WhiteOwner) && !SeatGone(BlackPlayerId, BlackOwner))
	{
		return;
	}

	// A seated human left: release both seats and reopen the table so anyone can play.
	auto ReleaseOwner = [this](TWeakObjectPtr<ABHPlayerState>& SeatOwnerPtr)
	{
		if (ABHPlayerState* PS = SeatOwnerPtr.Get())
		{
			if (PS->GetActiveMinigameTable() == this)
			{
				PS->SetActiveMinigameTable(nullptr);
			}
		}
		SeatOwnerPtr = nullptr;
	};
	ReleaseOwner(WhiteOwner);
	ReleaseOwner(BlackOwner);
	PressTimeByPlayerId.Empty();
	WhitePlayerId = -1;
	BlackPlayerId = -1;
	WhiteName.Reset();
	BlackName.Reset();
	InitBoard(Board);
	Turn = 0;
	SelectedSquare = -1;
	bVsAI = false;
	Phase = static_cast<uint8>(EChessPhase::Idle);
	StatusText = TEXT("Open table - tap to take a seat.");
	StatusAccent = BHChessNeutral;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

// ---------------------------------------------------------------------------------------------------------
// Chess rules (pure)
// ---------------------------------------------------------------------------------------------------------

void ABHTrainChessTable::InitBoard(TArray<int32>& B)
{
	B.Init(0, BHChessCells);
	const int32 WhiteBack[BHChessN] = { 4, 2, 3, 5, 6, 4 };   // R N B Q K R
	for (int32 Col = 0; Col < BHChessN; ++Col)
	{
		B[0 * BHChessN + Col] = WhiteBack[Col];                  // white back rank (row 0)
		B[1 * BHChessN + Col] = 1;                               // white pawns (row 1)
		B[(BHChessN - 2) * BHChessN + Col] = -1;                 // black pawns (row N-2)
		B[(BHChessN - 1) * BHChessN + Col] = -WhiteBack[Col];    // black back rank (row N-1)
	}
}

int32 ABHTrainChessTable::PieceValue(int32 Piece)
{
	switch (FMath::Abs(Piece))
	{
	case 1: return 100;
	case 2: return 300;
	case 3: return 320;
	case 4: return 500;
	case 5: return 900;
	case 6: return 20000;
	default: return 0;
	}
}

TCHAR ABHTrainChessTable::PieceGlyph(int32 Piece)
{
	static const TCHAR Letters[7] = { '.', 'P', 'N', 'B', 'R', 'Q', 'K' };
	const int32 Abs = FMath::Clamp(FMath::Abs(Piece), 0, 6);
	TCHAR Glyph = Letters[Abs];
	if (Piece < 0 && Abs > 0)
	{
		Glyph = FChar::ToLower(Glyph);
	}
	return Glyph;
}

bool ABHTrainChessTable::HasKing(const TArray<int32>& B, int32 Side)
{
	const int32 King = Side * 6;
	return B.Contains(King);
}

int32 ABHTrainChessTable::EvaluateForWhite(const TArray<int32>& B)
{
	int32 Sum = 0;
	for (const int32 Piece : B)
	{
		if (Piece > 0)
		{
			Sum += PieceValue(Piece);
		}
		else if (Piece < 0)
		{
			Sum -= PieceValue(Piece);
		}
	}
	return Sum;
}

void ABHTrainChessTable::PseudoLegalMoves(const TArray<int32>& B, int32 Side, TArray<int32>& OutFromTo)
{
	OutFromTo.Reset();
	if (B.Num() != BHChessCells)
	{
		return;
	}

	auto InBounds = [](int32 R, int32 C) { return R >= 0 && R < BHChessN && C >= 0 && C < BHChessN; };
	auto Add = [&OutFromTo](int32 From, int32 To) { OutFromTo.Add(From * BHChessCells + To); };

	static const int32 KnightR[8] = { 1, 2, -1, -2, 1, 2, -1, -2 };
	static const int32 KnightC[8] = { 2, 1, 2, 1, -2, -1, -2, -1 };
	static const int32 BishopR[4] = { 1, 1, -1, -1 };
	static const int32 BishopC[4] = { 1, -1, 1, -1 };
	static const int32 RookR[4] = { 1, -1, 0, 0 };
	static const int32 RookC[4] = { 0, 0, 1, -1 };

	for (int32 Sq = 0; Sq < BHChessCells; ++Sq)
	{
		const int32 Piece = B[Sq];
		if (Piece == 0 || FMath::Sign(Piece) != Side)
		{
			continue;
		}
		const int32 R = Sq / BHChessN;
		const int32 C = Sq % BHChessN;
		const int32 Abs = FMath::Abs(Piece);

		if (Abs == 1)   // pawn: forward one, diagonal captures, promotion handled in ApplyMove
		{
			const int32 Dr = Side;
			const int32 Nr = R + Dr;
			if (InBounds(Nr, C) && B[Nr * BHChessN + C] == 0)
			{
				Add(Sq, Nr * BHChessN + C);
			}
			for (int32 Dc = -1; Dc <= 1; Dc += 2)
			{
				if (InBounds(Nr, C + Dc))
				{
					const int32 T = Nr * BHChessN + (C + Dc);
					if (B[T] != 0 && FMath::Sign(B[T]) != Side)
					{
						Add(Sq, T);
					}
				}
			}
		}
		else if (Abs == 2)   // knight
		{
			for (int32 K = 0; K < 8; ++K)
			{
				const int32 Nr = R + KnightR[K];
				const int32 Nc = C + KnightC[K];
				if (InBounds(Nr, Nc))
				{
					const int32 T = Nr * BHChessN + Nc;
					if (B[T] == 0 || FMath::Sign(B[T]) != Side)
					{
						Add(Sq, T);
					}
				}
			}
		}
		else if (Abs == 6)   // king
		{
			for (int32 Dr = -1; Dr <= 1; ++Dr)
			{
				for (int32 Dc = -1; Dc <= 1; ++Dc)
				{
					if (Dr == 0 && Dc == 0)
					{
						continue;
					}
					const int32 Nr = R + Dr;
					const int32 Nc = C + Dc;
					if (InBounds(Nr, Nc))
					{
						const int32 T = Nr * BHChessN + Nc;
						if (B[T] == 0 || FMath::Sign(B[T]) != Side)
						{
							Add(Sq, T);
						}
					}
				}
			}
		}
		else   // sliding: bishop (3) diagonal, rook (4) orthogonal, queen (5) both
		{
			const bool bDiagonal = (Abs == 3 || Abs == 5);
			const bool bOrthogonal = (Abs == 4 || Abs == 5);
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				const bool bUseThisPass = (Pass == 0) ? bDiagonal : bOrthogonal;
				if (!bUseThisPass)
				{
					continue;
				}
				const int32* DR = (Pass == 0) ? BishopR : RookR;
				const int32* DC = (Pass == 0) ? BishopC : RookC;
				for (int32 D = 0; D < 4; ++D)
				{
					int32 Nr = R + DR[D];
					int32 Nc = C + DC[D];
					while (InBounds(Nr, Nc))
					{
						const int32 T = Nr * BHChessN + Nc;
						if (B[T] == 0)
						{
							Add(Sq, T);
						}
						else
						{
							if (FMath::Sign(B[T]) != Side)
							{
								Add(Sq, T);
							}
							break;
						}
						Nr += DR[D];
						Nc += DC[D];
					}
				}
			}
		}
	}
}

bool ABHTrainChessTable::IsLegalMove(const TArray<int32>& B, int32 Side, int32 From, int32 To)
{
	TArray<int32> Moves;
	PseudoLegalMoves(B, Side, Moves);
	return Moves.Contains(From * BHChessCells + To);
}

int32 ABHTrainChessTable::ApplyMove(TArray<int32>& B, int32 From, int32 To)
{
	if (!B.IsValidIndex(From) || !B.IsValidIndex(To))
	{
		return 0;
	}
	const int32 Piece = B[From];
	const int32 Captured = B[To];
	B[To] = Piece;
	B[From] = 0;

	if (FMath::Abs(Piece) == 1)
	{
		const int32 ToRow = To / BHChessN;
		if ((Piece > 0 && ToRow == BHChessN - 1) || (Piece < 0 && ToRow == 0))
		{
			B[To] = (Piece > 0) ? 5 : -5;   // promote to queen
		}
	}
	return Captured;
}

int32 ABHTrainChessTable::Negamax(TArray<int32> B, int32 Side, int32 Depth, int32 Alpha, int32 Beta)
{
	if (Depth <= 0 || !HasKing(B, 1) || !HasKing(B, -1))
	{
		return EvaluateForWhite(B) * Side;
	}

	TArray<int32> Moves;
	PseudoLegalMoves(B, Side, Moves);
	if (Moves.Num() == 0)
	{
		return EvaluateForWhite(B) * Side;
	}

	int32 Best = -BHChessINF;
	for (const int32 Move : Moves)
	{
		TArray<int32> Copy = B;
		ApplyMove(Copy, Move / BHChessCells, Move % BHChessCells);
		const int32 Score = -Negamax(Copy, -Side, Depth - 1, -Beta, -Alpha);
		Best = FMath::Max(Best, Score);
		Alpha = FMath::Max(Alpha, Score);
		if (Alpha >= Beta)
		{
			break;
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------------------------------------
// HUD board readout
// ---------------------------------------------------------------------------------------------------------

void ABHTrainChessTable::GetHudBoardLines(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const
{
	OutLines.Reset();
	OutColor = StatusAccent;

	const int32 ViewerSide = SideForPlayer(PlayerId);
	const bool bBlackView = (ViewerSide < 0);

	const FString Opponent = bBlackView
		? (WhiteName.IsEmpty() ? TEXT("White") : WhiteName)
		: (bVsAI ? TEXT("Computer") : (BlackName.IsEmpty() ? TEXT("(open)") : BlackName));
	const FString You = (ViewerSide > 0) ? TEXT("White") : (ViewerSide < 0 ? TEXT("Black") : TEXT("Spectating"));
	OutLines.Add(FString::Printf(TEXT("FAST CHESS   You: %s   vs %s"), *You, *Opponent));

	if (Board.Num() == BHChessCells)
	{
		for (int32 Display = 0; Display < BHChessN; ++Display)
		{
			const int32 Row = bBlackView ? Display : (BHChessN - 1 - Display);
			FString Line = FString::Printf(TEXT("%d  "), Row + 1);
			for (int32 ColStep = 0; ColStep < BHChessN; ++ColStep)
			{
				const int32 Col = bBlackView ? (BHChessN - 1 - ColStep) : ColStep;
				const int32 Sq = Row * BHChessN + Col;
				const TCHAR Glyph = PieceGlyph(Board[Sq]);
				if (Sq == SelectedSquare)
				{
					Line += FString::Printf(TEXT("[%c]"), Glyph);
				}
				else
				{
					Line += FString::Printf(TEXT(" %c "), Glyph);
				}
			}
			OutLines.Add(Line);
		}
		FString Files = TEXT("   ");
		for (int32 ColStep = 0; ColStep < BHChessN; ++ColStep)
		{
			const int32 Col = bBlackView ? (BHChessN - 1 - ColStep) : ColStep;
			Files += FString::Printf(TEXT(" %c "), static_cast<TCHAR>('a' + Col));
		}
		OutLines.Add(Files);
	}

	const EChessPhase CurrentPhase = static_cast<EChessPhase>(Phase);
	if (CurrentPhase == EChessPhase::Playing && ViewerSide != 0)
	{
		const int32 SideToMove = (Turn == 0) ? 1 : -1;
		OutLines.Add(ViewerSide == SideToMove ? TEXT(">> YOUR MOVE <<") : TEXT("Waiting for opponent..."));
		OutLines.Add(TEXT("Look at a square: TAP select/move, HOLD deselect."));
	}
	else
	{
		OutLines.Add(StatusText);
	}
}

// ---------------------------------------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------------------------------------

void ABHTrainChessTable::ApplyTableVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const FLinearColor Wood(0.20f, 0.13f, 0.08f, 1.0f);
	const float BoardSpan = BHChessN * BHSquareSize;   // ~96cm
	BHPropVisuals::ConfigurePart(Base, BHPropVisuals::CylinderMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 41.0f), FRotator::ZeroRotator, FVector(0.34f, 0.34f, 0.82f), true);
	BHPropVisuals::ConfigurePart(BoardBody, BHPropVisuals::CubeMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 82.0f), FRotator::ZeroRotator, FVector(BoardSpan / 100.0f + 0.06f, BoardSpan / 100.0f + 0.06f, 0.06f), true);
	BHPropVisuals::TintPart(Base, Wood);
	BHPropVisuals::TintPart(BoardBody, Wood * 1.3f);

	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(Tiles[Index], BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), SquareLocal(Index, BHBoardTopZ), FRotator::ZeroRotator, FVector(BHSquareSize / 100.0f * 0.92f, BHSquareSize / 100.0f * 0.92f, 0.02f), false);
	}
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		// Mesh/scale are set per piece each refresh; just establish material + (no) collision here.
		BHPropVisuals::ConfigurePart(Markers[Index], BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), SquareLocal(Index, BHBoardTopZ + 6.0f), FRotator::ZeroRotator, FVector(0.05f, 0.05f, 0.05f), false);
	}

	BHPropVisuals::ConfigurePart(WhiteKingCrown, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, BHBoardTopZ), FRotator(0.0f, 45.0f, 0.0f), FVector(0.05f, 0.05f, 0.05f), false);
	BHPropVisuals::ConfigurePart(BlackKingCrown, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, BHBoardTopZ), FRotator(0.0f, 45.0f, 0.0f), FVector(0.05f, 0.05f, 0.05f), false);
}

void ABHTrainChessTable::RefreshBoardVisuals()
{
	TArray<int32> Highlights;
	if (SelectedSquare >= 0 && Board.IsValidIndex(SelectedSquare) && Board[SelectedSquare] != 0)
	{
		const int32 Side = FMath::Sign(Board[SelectedSquare]);
		TArray<int32> Moves;
		PseudoLegalMoves(Board, Side, Moves);
		for (const int32 Move : Moves)
		{
			if (Move / BHChessCells == SelectedSquare)
			{
				Highlights.Add(Move % BHChessCells);
			}
		}
	}

	const FLinearColor LightSquare(0.74f, 0.70f, 0.58f, 1.0f);
	const FLinearColor DarkSquare(0.22f, 0.26f, 0.32f, 1.0f);
	const FLinearColor SelectColor(1.0f, 0.84f, 0.30f, 1.0f);
	const FLinearColor MoveColor(0.30f, 0.85f, 0.45f, 1.0f);
	const FLinearColor WhitePiece(0.94f, 0.94f, 0.90f, 1.0f);
	const FLinearColor BlackPiece(0.08f, 0.09f, 0.12f, 1.0f);

	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		if (!Tiles[Index])
		{
			continue;
		}
		const int32 Row = Index / BHChessN;
		const int32 Col = Index % BHChessN;
		FLinearColor TileColor = ((Row + Col) % 2 == 0) ? LightSquare : DarkSquare;
		float Emissive = 0.0f;
		if (Index == SelectedSquare)
		{
			TileColor = SelectColor;
			Emissive = 0.5f;
		}
		else if (Highlights.Contains(Index))
		{
			TileColor = MoveColor;
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
		const int32 Piece = Board.IsValidIndex(Index) ? Board[Index] : 0;
		if (Piece == 0)
		{
			BHPropVisuals::SetPartVisible(Markers[Index], false);
			continue;
		}
		UStaticMesh* PieceMesh = nullptr;
		FVector PieceScale = FVector(0.05f);
		float Yaw = 0.0f;
		BHChessMarkerStyle(Piece, PieceMesh, PieceScale, Yaw);
		const float HeightCm = PieceScale.Z * 100.0f;
		Markers[Index]->SetStaticMesh(PieceMesh);
		Markers[Index]->SetRelativeScale3D(PieceScale);
		Markers[Index]->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
		Markers[Index]->SetRelativeLocation(SquareLocal(Index, BHBoardTopZ + HeightCm * 0.5f));
		BHPropVisuals::SetPartVisible(Markers[Index], true);
		BHPropVisuals::TintPart(Markers[Index], Piece > 0 ? WhitePiece : BlackPiece, Piece > 0 ? 0.18f : 0.0f);
	}

	// Crowns: float a gold diamond over each surviving king.
	const FLinearColor CrownGold(1.0f, 0.82f, 0.28f, 1.0f);
	int32 WhiteKingSq = INDEX_NONE;
	int32 BlackKingSq = INDEX_NONE;
	for (int32 Index = 0; Index < Board.Num(); ++Index)
	{
		if (Board[Index] == 6)
		{
			WhiteKingSq = Index;
		}
		else if (Board[Index] == -6)
		{
			BlackKingSq = Index;
		}
	}
	UStaticMeshComponent* Crowns[2] = { WhiteKingCrown, BlackKingCrown };
	const int32 KingSquares[2] = { WhiteKingSq, BlackKingSq };
	const float KingHeightCm = 0.190f * 100.0f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (!Crowns[Side])
		{
			continue;
		}
		if (KingSquares[Side] != INDEX_NONE)
		{
			Crowns[Side]->SetRelativeLocation(SquareLocal(KingSquares[Side], BHBoardTopZ + KingHeightCm + 3.0f));
			BHPropVisuals::SetPartVisible(Crowns[Side], true);
			BHPropVisuals::TintPart(Crowns[Side], CrownGold, 0.7f);
		}
		else
		{
			BHPropVisuals::SetPartVisible(Crowns[Side], false);
		}
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("CHESS")));
	}
	if (TitleTextBack)
	{
		TitleTextBack->SetText(FText::FromString(TEXT("CHESS")));
	}
	if (StatusTextRender)
	{
		StatusTextRender->SetText(FText::FromString(StatusText));
		StatusTextRender->SetTextRenderColor(StatusAccent.ToFColor(true));
	}
	if (StatusTextBack)
	{
		StatusTextBack->SetText(FText::FromString(StatusText));
		StatusTextBack->SetTextRenderColor(StatusAccent.ToFColor(true));
	}
}
