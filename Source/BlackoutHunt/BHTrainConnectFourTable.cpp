// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainConnectFourTable.h"
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
	constexpr float BHC4HoldSeconds = 0.35f;
	constexpr int32 BHC4Cols = 7;
	constexpr int32 BHC4Rows = 6;
	constexpr int32 BHC4Cells = BHC4Cols * BHC4Rows;
	constexpr int32 BHC4AIDepth = 5;
	constexpr int32 BHC4INF = 100000000;
	constexpr float BHC4Cell = 16.0f;
	constexpr float BHC4BottomZ = 66.0f;   // row 0 disc height above the actor origin

	const FLinearColor C4Win(0.30f, 1.0f, 0.52f, 1.0f);
	const FLinearColor C4Neutral(0.55f, 0.78f, 1.0f, 1.0f);

	const int32 C4ColumnOrder[BHC4Cols] = { 3, 2, 4, 1, 5, 0, 6 };

	FVector DiscLocal(int32 Index)
	{
		const int32 Col = Index % BHC4Cols;
		const int32 Row = Index / BHC4Cols;
		return FVector((Col - 3) * BHC4Cell, -2.0f, BHC4BottomZ + Row * BHC4Cell);
	}

	TCHAR C4Glyph(int32 Cell)
	{
		return Cell > 0 ? TEXT('R') : (Cell < 0 ? TEXT('Y') : TEXT('.'));
	}

	void BHConfigureC4Text(UTextRenderComponent* TextComponent, const FVector& Location, float WorldSize, const FColor& Color)
	{
		BHPropVisuals::ConfigureReadableText(TextComponent, Location, FRotator(0.0f, 90.0f, 0.0f), WorldSize, Color);
	}
}

ABHTrainConnectFourTable::ABHTrainConnectFourTable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Connect Four"));

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(SceneRoot);
	BoardBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardBody"));
	BoardBody->SetupAttachment(SceneRoot);

	Discs.Reserve(BHC4Cells);
	for (int32 Index = 0; Index < BHC4Cells; ++Index)
	{
		UStaticMeshComponent* Disc = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Disc_%d"), Index));
		Disc->SetupAttachment(SceneRoot);
		Discs.Add(Disc);
	}

	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(SceneRoot);
	BHConfigureC4Text(TitleText, FVector(0.0f, -6.0f, 178.0f), 11.0f, FColor(255, 214, 120));

	StatusTextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusTextRender->SetupAttachment(SceneRoot);
	BHConfigureC4Text(StatusTextRender, FVector(0.0f, -6.0f, 46.0f), 6.5f, FColor(206, 226, 244));

	TableLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TableLight"));
	TableLight->SetupAttachment(SceneRoot);
	TableLight->SetRelativeLocation(FVector(0.0f, -40.0f, 150.0f));
	TableLight->SetIntensity(105.0f);
	TableLight->SetAttenuationRadius(340.0f);
	TableLight->SetLightColor(FLinearColor(0.85f, 0.86f, 0.95f, 1.0f));
	TableLight->SetCastShadows(false);

	Cells.Init(0, BHC4Cells);
	StatusText = TEXT("Open table - tap to take a seat.");
	ApplyTableVisuals();
	RefreshBoardVisuals();
}

void ABHTrainConnectFourTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshBoardVisuals();
}

void ABHTrainConnectFourTable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainConnectFourTable, Cells);
	DOREPLIFETIME(ABHTrainConnectFourTable, RedPlayerId);
	DOREPLIFETIME(ABHTrainConnectFourTable, YellowPlayerId);
	DOREPLIFETIME(ABHTrainConnectFourTable, RedName);
	DOREPLIFETIME(ABHTrainConnectFourTable, YellowName);
	DOREPLIFETIME(ABHTrainConnectFourTable, Turn);
	DOREPLIFETIME(ABHTrainConnectFourTable, Phase);
	DOREPLIFETIME(ABHTrainConnectFourTable, LastCell);
	DOREPLIFETIME(ABHTrainConnectFourTable, bVsAI);
	DOREPLIFETIME(ABHTrainConnectFourTable, StatusText);
	DOREPLIFETIME(ABHTrainConnectFourTable, StatusAccent);
}

bool ABHTrainConnectFourTable::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainConnectFourTable::BeginInteract_Implementation(ABHCharacter* Character)
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

void ABHTrainConnectFourTable::EndInteract_Implementation(ABHCharacter* Character)
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
	const bool bHold = HeldFor >= BHC4HoldSeconds;

	const EC4Phase CurrentPhase = static_cast<EC4Phase>(Phase);
	if (CurrentPhase == EC4Phase::Idle)
	{
		HandleSeating(Character, bHold);
		return;
	}
	if (CurrentPhase == EC4Phase::GameOver)
	{
		if (SideForPlayer(MyId) != 0)
		{
			StartNewGame(bVsAI);
			StatusToPlayer(Character, TEXT("Rematch! Red to move."), 2.0f);
		}
		else
		{
			StatusToPlayer(Character, TEXT("Game over. A seated player can rematch."), 2.0f);
		}
		return;
	}

	HandlePlayClick(Character, ColumnUnderView(Character));
}

FText ABHTrainConnectFourTable::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	const EC4Phase CurrentPhase = static_cast<EC4Phase>(Phase);
	switch (CurrentPhase)
	{
	case EC4Phase::Idle:
		return FText::FromString(RedPlayerId == -1 ? TEXT("Play Connect Four (sit as Red)") : TEXT("Join as Yellow  /  Hold: vs AI"));
	case EC4Phase::GameOver:
		return FText::FromString(TEXT("Connect Four - Tap to rematch"));
	default:
		return FText::FromString(TEXT("Connect Four - Tap a column to drop"));
	}
}

FBHInteractionPromptInfo ABHTrainConnectFourTable::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("CONNECT FOUR"));
	}
	return Info;
}

void ABHTrainConnectFourTable::OnRep_BoardState()
{
	RefreshBoardVisuals();
}

float ABHTrainConnectFourTable::GetServerTimeSeconds() const
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ABHTrainConnectFourTable::StatusToPlayer(ABHCharacter* Character, const FString& Message, float Duration) const
{
	if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
	{
		PC->ClientShowStatusMessage(Message, Duration);
	}
}

int32 ABHTrainConnectFourTable::SideForPlayer(int32 PlayerId) const
{
	if (PlayerId == RedPlayerId)
	{
		return 1;
	}
	if (PlayerId == YellowPlayerId)
	{
		return -1;
	}
	return 0;
}

int32 ABHTrainConnectFourTable::ColumnUnderView(const ABHCharacter* Character) const
{
	if (!Character)
	{
		return -1;
	}
	FVector EyeLoc;
	FRotator EyeRot;
	Character->GetActorEyesViewPoint(EyeLoc, EyeRot);
	const FVector Dir = EyeRot.Vector();

	// Connect Four stands upright: the playable face is the local Y=0 plane (board faces +/-Y).
	const FTransform& Xform = GetActorTransform();
	const FVector PlaneP = Xform.TransformPosition(FVector(0.0f, 0.0f, BHC4BottomZ));
	const FVector PlaneN = Xform.GetUnitAxis(EAxis::Y);
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

	const float HalfW = (BHC4Cols - 1) * 0.5f * BHC4Cell + BHC4Cell * 0.6f;
	if (FMath::Abs(Local.X) > HalfW)
	{
		return -1;
	}
	if (Local.Z < BHC4BottomZ - BHC4Cell || Local.Z > BHC4BottomZ + BHC4Rows * BHC4Cell)
	{
		return -1;
	}
	return FMath::Clamp(FMath::RoundToInt(Local.X / BHC4Cell + 3.0f), 0, BHC4Cols - 1);
}

void ABHTrainConnectFourTable::HandleSeating(ABHCharacter* Character, bool bHold)
{
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}
	const int32 MyId = BHPS->GetPlayerId();
	const FString MyName = BHPS->GetPlayerName();

	if (RedPlayerId == -1)
	{
		RedPlayerId = MyId;
		RedName = MyName;
		bVsAI = false;
		BHPS->SetActiveMinigameTable(this);
		StatusText = TEXT("Red seated - waiting for an opponent.");
		StatusAccent = C4Neutral;
		ForceNetUpdate();
		RefreshBoardVisuals();
		StatusToPlayer(Character, TEXT("You are Red. Tap with a friend as Yellow, or HOLD to play the AI."), 3.5f);
		return;
	}

	if (MyId == RedPlayerId)
	{
		if (bHold)
		{
			YellowPlayerId = -2;
			YellowName = TEXT("Computer");
			StartNewGame(true);
			StatusToPlayer(Character, TEXT("Game on - you are Red vs the AI."), 3.0f);
		}
		else
		{
			StatusToPlayer(Character, TEXT("Waiting for an opponent. HOLD to play the AI."), 2.5f);
		}
		return;
	}

	if (YellowPlayerId == -1)
	{
		YellowPlayerId = MyId;
		YellowName = MyName;
		bVsAI = false;
		BHPS->SetActiveMinigameTable(this);
		StartNewGame(false);
		StatusToPlayer(Character, TEXT("You are Yellow. Red moves first."), 3.0f);
		return;
	}

	StatusToPlayer(Character, TEXT("Both seats are taken - enjoy the match."), 2.0f);
}

void ABHTrainConnectFourTable::HandlePlayClick(ABHCharacter* Character, int32 Column)
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
	if (Column < 0)
	{
		StatusToPlayer(Character, TEXT("Look at a column on the board, then tap."), 1.5f);
		return;
	}
	if (LowestEmptyRow(Cells, Column) < 0)
	{
		StatusToPlayer(Character, TEXT("That column is full."), 1.5f);
		return;
	}

	DropDisc(Column, MySide);
}

void ABHTrainConnectFourTable::StartNewGame(bool bAgainstAI)
{
	Cells.Init(0, BHC4Cells);
	Turn = 0;
	LastCell = -1;
	bVsAI = bAgainstAI;
	Phase = static_cast<uint8>(EC4Phase::Playing);
	StatusText = bAgainstAI ? TEXT("Your move (Red) vs the AI.") : TEXT("Red to move.");
	StatusAccent = C4Neutral;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

void ABHTrainConnectFourTable::DropDisc(int32 Column, int32 Side)
{
	const int32 Row = LowestEmptyRow(Cells, Column);
	if (Row < 0)
	{
		return;
	}
	const int32 Index = Row * BHC4Cols + Column;
	Cells[Index] = Side;
	LastCell = Index;

	const int32 Winner = CheckWinner(Cells);
	if (Winner != 0)
	{
		const FString WinnerName = (Winner > 0)
			? (RedName.IsEmpty() ? TEXT("Red") : RedName)
			: (YellowName.IsEmpty() ? TEXT("Yellow") : YellowName);
		EndGame(FString::Printf(TEXT("%s wins - four in a row!"), *WinnerName), C4Win);
		return;
	}
	if (IsBoardFull(Cells))
	{
		EndGame(TEXT("Draw - the board is full."), C4Neutral);
		return;
	}

	Turn = (Turn == 0) ? 1 : 0;
	StatusText = (Turn == 0) ? TEXT("Red to move.") : TEXT("Yellow to move.");
	StatusAccent = C4Neutral;
	ForceNetUpdate();
	RefreshBoardVisuals();

	if (bVsAI && Turn == 1 && static_cast<EC4Phase>(Phase) == EC4Phase::Playing)
	{
		DoAIMove();
	}
}

void ABHTrainConnectFourTable::DoAIMove()
{
	// Yellow is the AI (-1): pick the column maximising its negamax value.
	int32 BestScore = -BHC4INF;
	TArray<int32> BestColumns;
	for (int32 Ci = 0; Ci < BHC4Cols; ++Ci)
	{
		const int32 Column = C4ColumnOrder[Ci];
		const int32 Row = LowestEmptyRow(Cells, Column);
		if (Row < 0)
		{
			continue;
		}
		TArray<int32> Copy = Cells;
		Copy[Row * BHC4Cols + Column] = -1;
		const int32 Score = -Negamax(Copy, 1, BHC4AIDepth - 1, -BHC4INF, BHC4INF);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestColumns.Reset();
			BestColumns.Add(Column);
		}
		else if (Score == BestScore)
		{
			BestColumns.Add(Column);
		}
	}
	if (BestColumns.Num() == 0)
	{
		return;
	}
	DropDisc(BestColumns[FMath::RandRange(0, BestColumns.Num() - 1)], -1);
}

void ABHTrainConnectFourTable::EndGame(const FString& Result, const FLinearColor& Accent)
{
	Phase = static_cast<uint8>(EC4Phase::GameOver);
	StatusText = Result;
	StatusAccent = Accent;
	ForceNetUpdate();
	RefreshBoardVisuals();
}

// ---------------------------------------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------------------------------------

int32 ABHTrainConnectFourTable::LowestEmptyRow(const TArray<int32>& C, int32 Column)
{
	if (Column < 0 || Column >= BHC4Cols || C.Num() != BHC4Cells)
	{
		return -1;
	}
	for (int32 Row = 0; Row < BHC4Rows; ++Row)
	{
		if (C[Row * BHC4Cols + Column] == 0)
		{
			return Row;
		}
	}
	return -1;
}

int32 ABHTrainConnectFourTable::CheckWinner(const TArray<int32>& C)
{
	if (C.Num() != BHC4Cells)
	{
		return 0;
	}
	static const int32 DirR[4] = { 1, 0, 1, 1 };
	static const int32 DirC[4] = { 0, 1, 1, -1 };
	for (int32 Row = 0; Row < BHC4Rows; ++Row)
	{
		for (int32 Col = 0; Col < BHC4Cols; ++Col)
		{
			const int32 First = C[Row * BHC4Cols + Col];
			if (First == 0)
			{
				continue;
			}
			for (int32 D = 0; D < 4; ++D)
			{
				int32 Count = 1;
				int32 R = Row + DirR[D];
				int32 Cc = Col + DirC[D];
				while (R >= 0 && R < BHC4Rows && Cc >= 0 && Cc < BHC4Cols && C[R * BHC4Cols + Cc] == First)
				{
					++Count;
					if (Count >= 4)
					{
						return First > 0 ? 1 : -1;
					}
					R += DirR[D];
					Cc += DirC[D];
				}
			}
		}
	}
	return 0;
}

bool ABHTrainConnectFourTable::IsBoardFull(const TArray<int32>& C)
{
	for (int32 Col = 0; Col < BHC4Cols; ++Col)
	{
		if (LowestEmptyRow(C, Col) >= 0)
		{
			return false;
		}
	}
	return true;
}

int32 ABHTrainConnectFourTable::EvaluateForRed(const TArray<int32>& C)
{
	auto WindowScore = [](int32 Red, int32 Yellow) -> int32
	{
		if (Red > 0 && Yellow > 0)
		{
			return 0;
		}
		static const int32 Weights[5] = { 0, 1, 12, 60, 1000 };
		if (Red > 0)
		{
			return Weights[Red];
		}
		if (Yellow > 0)
		{
			return -Weights[Yellow];
		}
		return 0;
	};

	int32 Score = 0;
	static const int32 DirR[4] = { 1, 0, 1, 1 };
	static const int32 DirC[4] = { 0, 1, 1, -1 };
	for (int32 Row = 0; Row < BHC4Rows; ++Row)
	{
		for (int32 Col = 0; Col < BHC4Cols; ++Col)
		{
			for (int32 D = 0; D < 4; ++D)
			{
				const int32 EndR = Row + 3 * DirR[D];
				const int32 EndC = Col + 3 * DirC[D];
				if (EndR < 0 || EndR >= BHC4Rows || EndC < 0 || EndC >= BHC4Cols)
				{
					continue;
				}
				int32 Red = 0;
				int32 Yellow = 0;
				for (int32 S = 0; S < 4; ++S)
				{
					const int32 V = C[(Row + S * DirR[D]) * BHC4Cols + (Col + S * DirC[D])];
					if (V > 0)
					{
						++Red;
					}
					else if (V < 0)
					{
						++Yellow;
					}
				}
				Score += WindowScore(Red, Yellow);
			}
		}
	}
	// Centre-column control bonus.
	for (int32 Row = 0; Row < BHC4Rows; ++Row)
	{
		Score += 3 * C[Row * BHC4Cols + 3];
	}
	return Score;
}

int32 ABHTrainConnectFourTable::Negamax(TArray<int32> C, int32 Side, int32 Depth, int32 Alpha, int32 Beta)
{
	const int32 Winner = CheckWinner(C);
	if (Winner != 0)
	{
		return (Winner > 0 ? 1000000 : -1000000) * Side - Depth * Side * Winner;
	}
	if (Depth <= 0 || IsBoardFull(C))
	{
		return EvaluateForRed(C) * Side;
	}

	int32 Best = -BHC4INF;
	for (int32 Ci = 0; Ci < BHC4Cols; ++Ci)
	{
		const int32 Column = C4ColumnOrder[Ci];
		const int32 Row = LowestEmptyRow(C, Column);
		if (Row < 0)
		{
			continue;
		}
		C[Row * BHC4Cols + Column] = Side;
		const int32 Score = -Negamax(C, -Side, Depth - 1, -Beta, -Alpha);
		C[Row * BHC4Cols + Column] = 0;
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
// HUD readout
// ---------------------------------------------------------------------------------------------------------

void ABHTrainConnectFourTable::GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const
{
	OutLines.Reset();
	OutColor = StatusAccent;

	const int32 ViewerSide = SideForPlayer(PlayerId);
	const FString You = (ViewerSide > 0) ? TEXT("Red") : (ViewerSide < 0 ? TEXT("Yellow") : TEXT("Spectating"));
	const FString Opponent = (ViewerSide > 0)
		? (bVsAI ? TEXT("Computer") : (YellowName.IsEmpty() ? TEXT("(open)") : YellowName))
		: (RedName.IsEmpty() ? TEXT("Red") : RedName);
	OutLines.Add(FString::Printf(TEXT("CONNECT FOUR   You: %s   vs %s"), *You, *Opponent));

	if (Cells.Num() == BHC4Cells)
	{
		for (int32 Row = BHC4Rows - 1; Row >= 0; --Row)
		{
			FString Line = TEXT(" ");
			for (int32 Col = 0; Col < BHC4Cols; ++Col)
			{
				Line += FString::Printf(TEXT("%c "), C4Glyph(Cells[Row * BHC4Cols + Col]));
			}
			OutLines.Add(Line);
		}
		OutLines.Add(TEXT(" 1 2 3 4 5 6 7"));
	}

	const EC4Phase CurrentPhase = static_cast<EC4Phase>(Phase);
	if (CurrentPhase == EC4Phase::Playing && ViewerSide != 0)
	{
		const int32 SideToMove = (Turn == 0) ? 1 : -1;
		OutLines.Add(ViewerSide == SideToMove ? TEXT(">> YOUR MOVE - tap a column <<") : TEXT("Waiting for opponent..."));
	}
	else
	{
		OutLines.Add(StatusText);
	}
}

// ---------------------------------------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------------------------------------

void ABHTrainConnectFourTable::ApplyTableVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const FLinearColor FrameBlue(0.10f, 0.18f, 0.55f, 1.0f);
	const float BoardW = BHC4Cols * BHC4Cell;
	const float BoardH = BHC4Rows * BHC4Cell;
	const float BoardCenterZ = BHC4BottomZ + (BHC4Rows - 1) * 0.5f * BHC4Cell;

	// Pedestal + an upright board slab (slab carries collision so the look-trace hits the actor).
	BHPropVisuals::ConfigurePart(Base, BHPropVisuals::CylinderMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 6.0f, 30.0f), FRotator::ZeroRotator, FVector(0.30f, 0.30f, 0.60f), true);
	BHPropVisuals::ConfigurePart(BoardBody, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 6.0f, BoardCenterZ), FRotator::ZeroRotator, FVector(BoardW / 100.0f + 0.10f, 0.08f, BoardH / 100.0f + 0.10f), true);
	BHPropVisuals::TintPart(Base, FLinearColor(0.24f, 0.16f, 0.10f, 1.0f));
	BHPropVisuals::TintPart(BoardBody, FrameBlue, 0.1f);

	for (int32 Index = 0; Index < Discs.Num(); ++Index)
	{
		BHPropVisuals::ConfigurePart(Discs[Index], BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), DiscLocal(Index), FRotator::ZeroRotator, FVector(0.13f, 0.06f, 0.13f), false);
	}
}

void ABHTrainConnectFourTable::RefreshBoardVisuals()
{
	const FLinearColor RedDisc(0.95f, 0.20f, 0.20f, 1.0f);
	const FLinearColor YellowDisc(1.0f, 0.84f, 0.20f, 1.0f);
	const FLinearColor EmptyHole(0.04f, 0.07f, 0.20f, 1.0f);

	for (int32 Index = 0; Index < Discs.Num(); ++Index)
	{
		if (!Discs[Index])
		{
			continue;
		}
		const int32 Cell = Cells.IsValidIndex(Index) ? Cells[Index] : 0;
		BHPropVisuals::SetPartVisible(Discs[Index], true);
		if (Cell > 0)
		{
			BHPropVisuals::TintPart(Discs[Index], RedDisc, Index == LastCell ? 0.6f : 0.25f);
		}
		else if (Cell < 0)
		{
			BHPropVisuals::TintPart(Discs[Index], YellowDisc, Index == LastCell ? 0.6f : 0.25f);
		}
		else
		{
			BHPropVisuals::TintPart(Discs[Index], EmptyHole, 0.0f);
		}
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("CONNECT FOUR")));
	}
	if (StatusTextRender)
	{
		StatusTextRender->SetText(FText::FromString(StatusText));
		StatusTextRender->SetTextRenderColor(StatusAccent.ToFColor(true));
	}
}
