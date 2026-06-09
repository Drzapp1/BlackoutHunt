// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainOthelloTable.h"
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
	constexpr float BHOthHoldSeconds = 0.35f;
	constexpr int32 BHOthN = 8;
	constexpr int32 BHOthCells = BHOthN * BHOthN;
	constexpr int32 BHOthAIDepth = 4;
	constexpr int32 BHOthINF = 1000000000;
	constexpr float BHOthBoardTopZ = 86.0f;
	constexpr float BHOthSquareSize = 13.0f;

	const FLinearColor OthWin(0.30f, 1.0f, 0.52f, 1.0f);
	const FLinearColor OthNeutral(0.55f, 0.78f, 1.0f, 1.0f);

	const int32 OthDirR[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	const int32 OthDirC[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

	const int32 OthWeight[BHOthCells] = {
		120, -20,  20,   5,   5,  20, -20, 120,
		-20, -40,  -5,  -5,  -5,  -5, -40, -20,
		 20,  -5,  15,   3,   3,  15,  -5,  20,
		  5,  -5,   3,   3,   3,   3,  -5,   5,
		  5,  -5,   3,   3,   3,   3,  -5,   5,
		 20,  -5,  15,   3,   3,  15,  -5,  20,
		-20, -40,  -5,  -5,  -5,  -5, -40, -20,
		120, -20,  20,   5,   5,  20, -20, 120
	};

	FVector OthSquareLocal(int32 Index, float ZOffset)
	{
		const int32 Col = Index % BHOthN;
		const int32 Row = Index / BHOthN;
		const float Half = (BHOthN - 1) * 0.5f;
		return FVector((Col - Half) * BHOthSquareSize, (Row - Half) * BHOthSquareSize, ZOffset);
	}

	void BHConfigureOthText(UTextRenderComponent* TextComponent, const FVector& Location, float WorldSize, const FColor& Color)
	{
		BHPropVisuals::ConfigureReadableText(TextComponent, Location, FRotator(0.0f, 90.0f, 0.0f), WorldSize, Color);
	}
}

ABHTrainOthelloTable::ABHTrainOthelloTable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Othello"));

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(SceneRoot);
	BoardBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardBody"));
	BoardBody->SetupAttachment(SceneRoot);

	Tiles.Reserve(BHOthCells);
	Discs.Reserve(BHOthCells);
	for (int32 Index = 0; Index < BHOthCells; ++Index)
	{
		UStaticMeshComponent* Tile = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Tile_%d"), Index));
		Tile->SetupAttachment(SceneRoot);
		Tiles.Add(Tile);
		UStaticMeshComponent* Disc = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Disc_%d"), Index));
		Disc->SetupAttachment(SceneRoot);
		Discs.Add(Disc);
	}

	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(SceneRoot);
	BHConfigureOthText(TitleText, FVector(0.0f, 60.0f, 150.0f), 12.0f, FColor(236, 232, 210));

	StatusTextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusTextRender->SetupAttachment(SceneRoot);
	BHConfigureOthText(StatusTextRender, FVector(0.0f, 60.0f, 128.0f), 7.0f, FColor(206, 226, 244));

	// Mirrored copies facing the opposite (-Y) seat so both players read the text the right way round.
	TitleTextBack = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleTextBack"));
	TitleTextBack->SetupAttachment(SceneRoot);
	BHPropVisuals::ConfigureReadableText(TitleTextBack, FVector(0.0f, -60.0f, 150.0f), FRotator(0.0f, -90.0f, 0.0f), 12.0f, FColor(236, 232, 210));

	StatusTextBack = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusTextBack"));
	StatusTextBack->SetupAttachment(SceneRoot);
	BHPropVisuals::ConfigureReadableText(StatusTextBack, FVector(0.0f, -60.0f, 128.0f), FRotator(0.0f, -90.0f, 0.0f), 7.0f, FColor(206, 226, 244));

	TableLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TableLight"));
	TableLight->SetupAttachment(SceneRoot);
	TableLight->SetRelativeLocation(FVector(0.0f, 0.0f, 165.0f));
	TableLight->SetIntensity(110.0f);
	TableLight->SetAttenuationRadius(360.0f);
	TableLight->SetLightColor(FLinearColor(0.82f, 0.88f, 0.82f, 1.0f));
	TableLight->SetCastShadows(false);

	InitBoard(Board);
	StatusText = TEXT("Open table - tap to take a seat.");
	ApplyTableVisuals();
	RefreshBoardVisuals();
}

void ABHTrainOthelloTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		PruneSeats();
	}
	RefreshBoardVisuals();
}

void ABHTrainOthelloTable::PruneSeats()
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

	if (!SeatGone(BlackPlayerId, BlackOwner) && !SeatGone(WhitePlayerId, WhiteOwner))
	{
		return;
	}

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
	ReleaseOwner(BlackOwner);
	ReleaseOwner(WhiteOwner);
	PressTimeByPlayerId.Empty();
	BlackPlayerId = -1;
	WhitePlayerId = -1;
	BlackName.Reset();
	WhiteName.Reset();
	InitBoard(Board);
	Turn = 0;
	bVsAI = false;
	Phase = static_cast<uint8>(EOthPhase::Idle);
	StatusText = TEXT("Open table - tap to take a seat.");
	StatusAccent = OthNeutral;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

void ABHTrainOthelloTable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainOthelloTable, Board);
	DOREPLIFETIME(ABHTrainOthelloTable, BlackPlayerId);
	DOREPLIFETIME(ABHTrainOthelloTable, WhitePlayerId);
	DOREPLIFETIME(ABHTrainOthelloTable, BlackName);
	DOREPLIFETIME(ABHTrainOthelloTable, WhiteName);
	DOREPLIFETIME(ABHTrainOthelloTable, Turn);
	DOREPLIFETIME(ABHTrainOthelloTable, Phase);
	DOREPLIFETIME(ABHTrainOthelloTable, bVsAI);
	DOREPLIFETIME(ABHTrainOthelloTable, StatusText);
	DOREPLIFETIME(ABHTrainOthelloTable, StatusAccent);
}

bool ABHTrainOthelloTable::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainOthelloTable::BeginInteract_Implementation(ABHCharacter* Character)
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

void ABHTrainOthelloTable::EndInteract_Implementation(ABHCharacter* Character)
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
	const bool bHold = HeldFor >= BHOthHoldSeconds;

	const EOthPhase CurrentPhase = static_cast<EOthPhase>(Phase);
	if (CurrentPhase == EOthPhase::Idle)
	{
		HandleSeating(Character, bHold);
		return;
	}
	if (CurrentPhase == EOthPhase::GameOver)
	{
		if (SideForPlayer(MyId) != 0)
		{
			StartNewGame(bVsAI);
			StatusToPlayer(Character, TEXT("Rematch! Black to move."), 2.0f);
		}
		else
		{
			StatusToPlayer(Character, TEXT("Game over. A seated player can rematch."), 2.0f);
		}
		return;
	}

	HandlePlayClick(Character, CellUnderView(Character));
}

FText ABHTrainOthelloTable::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	const EOthPhase CurrentPhase = static_cast<EOthPhase>(Phase);
	switch (CurrentPhase)
	{
	case EOthPhase::Idle:
		return FText::FromString(BlackPlayerId == -1 ? TEXT("Play Othello (sit as Black)") : TEXT("Join as White  /  Hold: vs AI"));
	case EOthPhase::GameOver:
		return FText::FromString(TEXT("Othello - Tap to rematch"));
	default:
		return FText::FromString(TEXT("Othello - Tap a highlighted cell"));
	}
}

FBHInteractionPromptInfo ABHTrainOthelloTable::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("OTHELLO"));
	}
	return Info;
}

void ABHTrainOthelloTable::OnRep_BoardState()
{
	RefreshBoardVisuals();
}

float ABHTrainOthelloTable::GetServerTimeSeconds() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ABHTrainOthelloTable::StatusToPlayer(ABHCharacter* Character, const FString& Message, float Duration) const
{
	if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
	{
		PC->ClientShowStatusMessage(Message, Duration);
	}
}

int32 ABHTrainOthelloTable::SideForPlayer(int32 PlayerId) const
{
	if (PlayerId == BlackPlayerId)
	{
		return 1;
	}
	if (PlayerId == WhitePlayerId)
	{
		return -1;
	}
	return 0;
}

int32 ABHTrainOthelloTable::CellUnderView(const ABHCharacter* Character) const
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
	const FVector PlaneP = Xform.TransformPosition(FVector(0.0f, 0.0f, BHOthBoardTopZ));
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
	const float Half = (BHOthN - 1) * 0.5f;
	const float HalfSpan = Half * BHOthSquareSize + BHOthSquareSize * 0.6f;
	if (FMath::Abs(Local.X) > HalfSpan || FMath::Abs(Local.Y) > HalfSpan)
	{
		return -1;
	}
	const int32 Col = FMath::Clamp(FMath::RoundToInt(Local.X / BHOthSquareSize + Half), 0, BHOthN - 1);
	const int32 Row = FMath::Clamp(FMath::RoundToInt(Local.Y / BHOthSquareSize + Half), 0, BHOthN - 1);
	return Row * BHOthN + Col;
}

void ABHTrainOthelloTable::HandleSeating(ABHCharacter* Character, bool bHold)
{
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}
	const int32 MyId = BHPS->GetPlayerId();
	const FString MyName = BHPS->GetPlayerName();

	if (BlackPlayerId == -1)
	{
		BlackPlayerId = MyId;
		BlackName = MyName;
		BlackOwner = BHPS;
		bVsAI = false;
		BHPS->SetActiveMinigameTable(this);
		StatusText = TEXT("Black seated - waiting for an opponent.");
		StatusAccent = OthNeutral;
		ForceNetUpdate();
		RefreshBoardVisuals();
		StatusToPlayer(Character, TEXT("You are Black. Tap with a friend as White, or HOLD to play the AI."), 3.5f);
		return;
	}
	if (MyId == BlackPlayerId)
	{
		if (bHold)
		{
			WhitePlayerId = -2;
			WhiteName = TEXT("Computer");
			StartNewGame(true);
			StatusToPlayer(Character, TEXT("Game on - you are Black vs the AI."), 3.0f);
		}
		else
		{
			StatusToPlayer(Character, TEXT("Waiting for an opponent. HOLD to play the AI."), 2.5f);
		}
		return;
	}
	if (WhitePlayerId == -1)
	{
		WhitePlayerId = MyId;
		WhiteName = MyName;
		WhiteOwner = BHPS;
		bVsAI = false;
		BHPS->SetActiveMinigameTable(this);
		StartNewGame(false);
		StatusToPlayer(Character, TEXT("You are White. Black moves first."), 3.0f);
		return;
	}
	StatusToPlayer(Character, TEXT("Both seats are taken - enjoy the match."), 2.0f);
}

void ABHTrainOthelloTable::HandlePlayClick(ABHCharacter* Character, int32 Cell)
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
	if (Cell < 0)
	{
		StatusToPlayer(Character, TEXT("Look at a cell on the board, then tap."), 1.5f);
		return;
	}
	if (FlipsForMove(Board, Cell, MySide, nullptr) <= 0)
	{
		StatusToPlayer(Character, TEXT("Illegal - you must flank and flip at least one disc."), 1.5f);
		return;
	}
	PlaceAndAdvance(Cell, MySide);
}

void ABHTrainOthelloTable::StartNewGame(bool bAgainstAI)
{
	InitBoard(Board);
	Turn = 0;
	bVsAI = bAgainstAI;
	Phase = static_cast<uint8>(EOthPhase::Playing);
	StatusText = bAgainstAI ? TEXT("Your move (Black) vs the AI.") : TEXT("Black to move.");
	StatusAccent = OthNeutral;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

void ABHTrainOthelloTable::PlaceAndAdvance(int32 Cell, int32 Side)
{
	ApplyMove(Board, Cell, Side);

	TArray<int32> OppMoves;
	CollectLegalMoves(Board, -Side, OppMoves);
	if (OppMoves.Num() > 0)
	{
		Turn = (-Side > 0) ? 0 : 1;
	}
	else
	{
		TArray<int32> SelfMoves;
		CollectLegalMoves(Board, Side, SelfMoves);
		if (SelfMoves.Num() > 0)
		{
			Turn = (Side > 0) ? 0 : 1;   // opponent passes
		}
		else
		{
			EndGame();
			return;
		}
	}

	const int32 NextSide = (Turn == 0) ? 1 : -1;
	StatusText = (NextSide > 0) ? TEXT("Black to move.") : TEXT("White to move.");
	StatusAccent = OthNeutral;
	ForceNetUpdate();
	RefreshBoardVisuals();

	if (bVsAI && NextSide == -1 && static_cast<EOthPhase>(Phase) == EOthPhase::Playing)
	{
		DoAIMove();
	}
}

void ABHTrainOthelloTable::DoAIMove()
{
	TArray<int32> Moves;
	CollectLegalMoves(Board, -1, Moves);
	if (Moves.Num() == 0)
	{
		return;   // shouldn't happen (PlaceAndAdvance only calls when white has a move)
	}

	int32 BestScore = -BHOthINF;
	TArray<int32> BestCells;
	for (const int32 Cell : Moves)
	{
		TArray<int32> Copy = Board;
		ApplyMove(Copy, Cell, -1);
		const int32 Score = -Negamax(Copy, 1, BHOthAIDepth - 1, -BHOthINF, BHOthINF);
		if (Score > BestScore)
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
	// Slip up sometimes so the AI isn't a flawless wall: with this chance it plays a random legal move (often a weak
	// square) instead of its best, giving the human room to come back.
	if (FMath::FRand() < 0.25f)
	{
		BestCells = Moves;
	}
	const int32 Chosen = BestCells[FMath::RandRange(0, BestCells.Num() - 1)];
	PlaceAndAdvance(Chosen, -1);
}

void ABHTrainOthelloTable::EndGame()
{
	const int32 BlackCount = CountDiscs(Board, 1);
	const int32 WhiteCount = CountDiscs(Board, -1);
	const FString BName = BlackName.IsEmpty() ? TEXT("Black") : BlackName;
	const FString WName = WhiteName.IsEmpty() ? TEXT("White") : WhiteName;
	if (BlackCount > WhiteCount)
	{
		StatusText = FString::Printf(TEXT("%s wins %d-%d!"), *BName, BlackCount, WhiteCount);
		StatusAccent = OthWin;
	}
	else if (WhiteCount > BlackCount)
	{
		StatusText = FString::Printf(TEXT("%s wins %d-%d!"), *WName, WhiteCount, BlackCount);
		StatusAccent = OthWin;
	}
	else
	{
		StatusText = FString::Printf(TEXT("Draw %d-%d."), BlackCount, WhiteCount);
		StatusAccent = OthNeutral;
	}
	Phase = static_cast<uint8>(EOthPhase::GameOver);
	ForceNetUpdate();
	RefreshBoardVisuals();
}

// ---------------------------------------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------------------------------------

void ABHTrainOthelloTable::InitBoard(TArray<int32>& B)
{
	B.Init(0, BHOthCells);
	B[3 * BHOthN + 3] = -1;   // white
	B[3 * BHOthN + 4] = 1;    // black
	B[4 * BHOthN + 3] = 1;    // black
	B[4 * BHOthN + 4] = -1;   // white
}

int32 ABHTrainOthelloTable::FlipsForMove(const TArray<int32>& B, int32 Cell, int32 Side, TArray<int32>* OutFlips)
{
	if (B.Num() != BHOthCells || Cell < 0 || Cell >= BHOthCells || B[Cell] != 0)
	{
		return 0;
	}
	const int32 R = Cell / BHOthN;
	const int32 C = Cell % BHOthN;
	int32 Total = 0;
	for (int32 D = 0; D < 8; ++D)
	{
		int32 Nr = R + OthDirR[D];
		int32 Nc = C + OthDirC[D];
		TArray<int32> Line;
		while (Nr >= 0 && Nr < BHOthN && Nc >= 0 && Nc < BHOthN && B[Nr * BHOthN + Nc] == -Side)
		{
			Line.Add(Nr * BHOthN + Nc);
			Nr += OthDirR[D];
			Nc += OthDirC[D];
		}
		if (Line.Num() > 0 && Nr >= 0 && Nr < BHOthN && Nc >= 0 && Nc < BHOthN && B[Nr * BHOthN + Nc] == Side)
		{
			Total += Line.Num();
			if (OutFlips)
			{
				OutFlips->Append(Line);
			}
		}
	}
	return Total;
}

void ABHTrainOthelloTable::CollectLegalMoves(const TArray<int32>& B, int32 Side, TArray<int32>& OutCells)
{
	OutCells.Reset();
	for (int32 Cell = 0; Cell < BHOthCells; ++Cell)
	{
		if (B[Cell] == 0 && FlipsForMove(B, Cell, Side, nullptr) > 0)
		{
			OutCells.Add(Cell);
		}
	}
}

int32 ABHTrainOthelloTable::ApplyMove(TArray<int32>& B, int32 Cell, int32 Side)
{
	TArray<int32> Flips;
	const int32 N = FlipsForMove(B, Cell, Side, &Flips);
	if (N <= 0)
	{
		return 0;
	}
	B[Cell] = Side;
	for (const int32 F : Flips)
	{
		B[F] = Side;
	}
	return N;
}

int32 ABHTrainOthelloTable::CountDiscs(const TArray<int32>& B, int32 Side)
{
	int32 Count = 0;
	for (const int32 V : B)
	{
		if (V == Side)
		{
			++Count;
		}
	}
	return Count;
}

int32 ABHTrainOthelloTable::EvaluateForBlack(const TArray<int32>& B)
{
	int32 Positional = 0;
	for (int32 i = 0; i < BHOthCells; ++i)
	{
		if (B[i] > 0)
		{
			Positional += OthWeight[i];
		}
		else if (B[i] < 0)
		{
			Positional -= OthWeight[i];
		}
	}
	TArray<int32> BlackMoves;
	TArray<int32> WhiteMoves;
	CollectLegalMoves(B, 1, BlackMoves);
	CollectLegalMoves(B, -1, WhiteMoves);
	const int32 Mobility = 5 * (BlackMoves.Num() - WhiteMoves.Num());
	return Positional + Mobility;
}

int32 ABHTrainOthelloTable::Negamax(TArray<int32> B, int32 Side, int32 Depth, int32 Alpha, int32 Beta)
{
	if (Depth <= 0)
	{
		return EvaluateForBlack(B) * Side;
	}
	TArray<int32> Moves;
	CollectLegalMoves(B, Side, Moves);
	if (Moves.Num() == 0)
	{
		TArray<int32> OppMoves;
		CollectLegalMoves(B, -Side, OppMoves);
		if (OppMoves.Num() == 0)
		{
			return EvaluateForBlack(B) * Side;   // terminal
		}
		return -Negamax(B, -Side, Depth - 1, -Beta, -Alpha);   // pass
	}

	int32 Best = -BHOthINF;
	for (const int32 Cell : Moves)
	{
		TArray<int32> Copy = B;
		ApplyMove(Copy, Cell, Side);
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
// HUD
// ---------------------------------------------------------------------------------------------------------

void ABHTrainOthelloTable::GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const
{
	OutLines.Reset();
	OutColor = StatusAccent;
	const int32 ViewerSide = SideForPlayer(PlayerId);
	const FString You = (ViewerSide > 0) ? TEXT("Black (X)") : (ViewerSide < 0 ? TEXT("White (O)") : TEXT("Spectating"));
	const FString Opp = (ViewerSide > 0)
		? (bVsAI ? TEXT("Computer") : (WhiteName.IsEmpty() ? TEXT("(open)") : WhiteName))
		: (BlackName.IsEmpty() ? TEXT("Black") : BlackName);
	OutLines.Add(FString::Printf(TEXT("OTHELLO   You: %s   vs %s"), *You, *Opp));

	if (Board.Num() == BHOthCells)
	{
		const bool bWhiteView = (ViewerSide < 0);
		for (int32 Display = 0; Display < BHOthN; ++Display)
		{
			const int32 Row = bWhiteView ? (BHOthN - 1 - Display) : Display;
			FString Line;
			for (int32 ColStep = 0; ColStep < BHOthN; ++ColStep)
			{
				const int32 Col = bWhiteView ? (BHOthN - 1 - ColStep) : ColStep;
				const int32 V = Board[Row * BHOthN + Col];
				Line += (V > 0) ? TEXT("X ") : (V < 0 ? TEXT("O ") : TEXT(". "));
			}
			OutLines.Add(Line);
		}
		OutLines.Add(FString::Printf(TEXT("Black(X): %d   White(O): %d"), CountDiscs(Board, 1), CountDiscs(Board, -1)));
	}

	const EOthPhase CurrentPhase = static_cast<EOthPhase>(Phase);
	if (CurrentPhase == EOthPhase::Playing && ViewerSide != 0)
	{
		const int32 SideToMove = (Turn == 0) ? 1 : -1;
		OutLines.Add(ViewerSide == SideToMove ? TEXT(">> YOUR MOVE (tap a highlighted cell) <<") : TEXT("Waiting for opponent..."));
	}
	else
	{
		OutLines.Add(StatusText);
	}
}

// ---------------------------------------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------------------------------------

void ABHTrainOthelloTable::ApplyTableVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	const float BoardSpan = BHOthN * BHOthSquareSize;
	BHPropVisuals::ConfigurePart(Base, BHPropVisuals::CylinderMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 41.0f), FRotator::ZeroRotator, FVector(0.34f, 0.34f, 0.82f), true);
	BHPropVisuals::ConfigurePart(BoardBody, BHPropVisuals::CubeMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 82.0f), FRotator::ZeroRotator, FVector(BoardSpan / 100.0f + 0.06f, BoardSpan / 100.0f + 0.06f, 0.06f), true);
	BHPropVisuals::TintPart(Base, FLinearColor(0.20f, 0.13f, 0.08f, 1.0f));

	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(Tiles[Index], BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), OthSquareLocal(Index, BHOthBoardTopZ), FRotator::ZeroRotator, FVector(BHOthSquareSize / 100.0f * 0.92f, BHOthSquareSize / 100.0f * 0.92f, 0.02f), false);
	}
	for (int32 Index = 0; Index < Discs.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(Discs[Index], BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), OthSquareLocal(Index, BHOthBoardTopZ + 4.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.03f), false);
	}
}

void ABHTrainOthelloTable::RefreshBoardVisuals()
{
	// Legal-move highlights for the side to move.
	TArray<int32> Legal;
	const int32 SideToMove = (Turn == 0) ? 1 : -1;
	if (static_cast<EOthPhase>(Phase) == EOthPhase::Playing)
	{
		CollectLegalMoves(Board, SideToMove, Legal);
	}

	const FLinearColor FeltGreen(0.06f, 0.34f, 0.16f, 1.0f);
	const FLinearColor FeltDark(0.05f, 0.26f, 0.13f, 1.0f);
	const FLinearColor MoveHi(0.85f, 0.80f, 0.30f, 1.0f);
	const FLinearColor BlackDisc(0.05f, 0.05f, 0.06f, 1.0f);
	const FLinearColor WhiteDisc(0.95f, 0.95f, 0.92f, 1.0f);

	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		if (!Tiles[Index])
		{
			continue;
		}
		const int32 Row = Index / BHOthN;
		const int32 Col = Index % BHOthN;
		FLinearColor TileColor = ((Row + Col) % 2 == 0) ? FeltGreen : FeltDark;
		float Emissive = 0.0f;
		if (Legal.Contains(Index))
		{
			TileColor = MoveHi;
			Emissive = 0.35f;
		}
		BHPropVisuals::TintPart(Tiles[Index], TileColor, Emissive);
	}

	for (int32 Index = 0; Index < Discs.Num(); ++Index)
	{
		if (!Discs[Index])
		{
			continue;
		}
		const int32 V = Board.IsValidIndex(Index) ? Board[Index] : 0;
		if (V == 0)
		{
			BHPropVisuals::SetPartVisible(Discs[Index], false);
			continue;
		}
		BHPropVisuals::SetPartVisible(Discs[Index], true);
		BHPropVisuals::TintPart(Discs[Index], V > 0 ? BlackDisc : WhiteDisc, V > 0 ? 0.0f : 0.18f);
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("OTHELLO")));
	}
	if (TitleTextBack)
	{
		TitleTextBack->SetText(FText::FromString(TEXT("OTHELLO")));
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
