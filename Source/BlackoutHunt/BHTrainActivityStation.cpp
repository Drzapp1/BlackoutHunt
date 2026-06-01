#include "BHTrainActivityStation.h"
#include "BHCharacter.h"
#include "BHGameState.h"
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
	constexpr int32 BHTrainReflexSlotCount = 10;

	FLinearColor BHActivityAccent(EBHTrainActivityType Type)
	{
		switch (Type)
		{
		case EBHTrainActivityType::SnackCart:
			return FLinearColor(1.0f, 0.70f, 0.22f, 1.0f);
		case EBHTrainActivityType::DrinkCooler:
			return FLinearColor(0.25f, 0.78f, 1.0f, 1.0f);
		case EBHTrainActivityType::ReflexArcade:
			return FLinearColor(0.36f, 1.0f, 0.56f, 1.0f);
		case EBHTrainActivityType::MemoryTable:
			return FLinearColor(0.88f, 0.58f, 1.0f, 1.0f);
		default:
			return FLinearColor(0.50f, 0.90f, 0.80f, 1.0f);
		}
	}

	FString BHActivityClock(float Seconds)
	{
		const int32 ClampedSeconds = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), ClampedSeconds / 60, ClampedSeconds % 60);
	}

	FString BHActivityCardName(int32 Slot)
	{
		static const TCHAR* CardNames[] = {
			TEXT("KEY"),
			TEXT("BELL"),
			TEXT("MOON"),
			TEXT("STAR")
		};
		const int32 Index = FMath::Clamp(Slot, 0, UE_ARRAY_COUNT(CardNames) - 1);
		return CardNames[Index];
	}

	FString BHBuildSlotLine(int32 HighlightSlot, const TCHAR* HighlightToken)
	{
		TArray<FString> Slots;
		for (int32 Slot = 0; Slot < BHTrainReflexSlotCount; ++Slot)
		{
			Slots.Add(Slot == HighlightSlot ? FString(HighlightToken) : FString(TEXT(".")));
		}
		return FString::Join(Slots, TEXT(" "));
	}

	void BHConfigureActivityText(UTextRenderComponent* TextComponent, const FVector& Location, float WorldSize, const FColor& Color)
	{
		BHPropVisuals::ConfigureReadableText(TextComponent, Location, FRotator(0.0f, 90.0f, 0.0f), WorldSize, Color);
	}
}

ABHTrainActivityStation::ABHTrainActivityStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.12f;
	InteractionLabel = FText::FromString(TEXT("Train Activity"));
	ActivityType = EBHTrainActivityType::SnackCart;
	ActivitySeed = 0;
	ActivityUseCounter = 0;
	LastResultText = TEXT("");
	LastResultAccent = BHActivityAccent(ActivityType);
	SetActorScale3D(FVector(0.92f, 0.38f, 1.08f));

	ScreenPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenPanel"));
	ScreenPanel->SetupAttachment(SceneRoot);
	CounterTop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CounterTop"));
	CounterTop->SetupAttachment(SceneRoot);
	AccentStrip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccentStrip"));
	AccentStrip->SetupAttachment(SceneRoot);
	PropA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropA"));
	PropA->SetupAttachment(SceneRoot);
	PropB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropB"));
	PropB->SetupAttachment(SceneRoot);
	PropC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropC"));
	PropC->SetupAttachment(SceneRoot);
	ButtonA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonA"));
	ButtonA->SetupAttachment(SceneRoot);
	ButtonB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonB"));
	ButtonB->SetupAttachment(SceneRoot);
	ButtonC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonC"));
	ButtonC->SetupAttachment(SceneRoot);

	// Text on the +Y FRONT face (reader's side) so the body/props never occlude it; same +Y facing as before so
	// it reads correctly (not mirrored). Title above, detail spaced below it.
	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(SceneRoot);
	BHConfigureActivityText(TitleText, FVector(-58.0f, 58.0f, 52.0f), 14.5f, FColor(255, 210, 92));

	DetailText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DetailText"));
	DetailText->SetupAttachment(SceneRoot);
	BHConfigureActivityText(DetailText, FVector(-58.0f, 58.0f, 30.0f), 8.8f, FColor(222, 244, 232));

	AccentLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AccentLight"));
	AccentLight->SetupAttachment(SceneRoot);
	AccentLight->SetRelativeLocation(FVector(40.0f, -82.0f, 82.0f));
	AccentLight->SetIntensity(85.0f);
	AccentLight->SetAttenuationRadius(210.0f);
	AccentLight->SetCastShadows(false);

	ApplyActivityVisuals();
	RefreshActivityText();
}

void ABHTrainActivityStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshActivityText();
}

void ABHTrainActivityStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainActivityStation, ActivityType);
	DOREPLIFETIME(ABHTrainActivityStation, ActivitySeed);
	DOREPLIFETIME(ABHTrainActivityStation, ActivityUseCounter);
	DOREPLIFETIME(ABHTrainActivityStation, LastResultText);
	DOREPLIFETIME(ABHTrainActivityStation, LastResultAccent);
}

bool ABHTrainActivityStation::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return IsPassengerEligibleForActivity(BHPS)
		&& BHGS
		&& BHGS->RoundPhase == EBHRoundPhase::Intermission
		&& BHGS->TrainPhase != EBHTrainPhase::Inactive;
}

void ABHTrainActivityStation::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character || !CanInteract_Implementation(Character))
	{
		return;
	}

	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}

	const float Now = GetServerTimeSeconds();
	const float CooldownRemaining = GetCooldownRemaining(Character);
	if (CooldownRemaining > 0.0f)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("%s ready in %s."),
				*GetActivityTitle(ActivityType),
				*BHActivityClock(CooldownRemaining)), 2.25f);
		}
		return;
	}

	LastUseTimeByPlayerId.Add(BHPS->GetPlayerId(), Now);

	FString ResultText;
	FLinearColor ResultAccent = BHActivityAccent(ActivityType);
	switch (ActivityType)
	{
	case EBHTrainActivityType::SnackCart:
		Character->RecoverStamina(12.0f);
		Character->AddFear(-3.0f);
		ResultText = TEXT("Snack break: stamina recovered and fear lowered.");
		break;
	case EBHTrainActivityType::DrinkCooler:
		Character->RefillFlashlight(14.0f);
		Character->RecoverStamina(6.0f);
		Character->AddFear(-1.0f);
		ResultText = TEXT("Drink break: flashlight topped up, stamina recovered, nerves steadied.");
		break;
	case EBHTrainActivityType::ReflexArcade:
	{
		const int32 CursorSlot = GetReflexCursorSlot(Now);
		const int32 TargetSlot = GetReflexTargetSlot();
		const int32 DirectDistance = FMath::Abs(CursorSlot - TargetSlot);
		const int32 WrappedDistance = FMath::Min(DirectDistance, BHTrainReflexSlotCount - DirectDistance);
		if (WrappedDistance == 0)
		{
			const FString CurrencyText = AwardActivityPoints(BHPS, 2);
			Character->RecoverStamina(3.0f);
			Character->AddFear(-2.0f);
			ResultText = FString::Printf(TEXT("Perfect reflex hit. +2 %s and a quick morale boost."), *CurrencyText);
		}
		else if (WrappedDistance == 1)
		{
			Character->AddFear(-1.0f);
			ResultText = TEXT("Close reflex hit. Nerves steadied, but no tickets paid out.");
		}
		else
		{
			ResultAccent = FLinearColor(1.0f, 0.44f, 0.22f, 1.0f);
			ResultText = TEXT("Missed the green slot. Time E when the cursor reaches GREEN.");
		}
		break;
	}
	case EBHTrainActivityType::MemoryTable:
	{
		int32 LeftSlot = 0;
		int32 RightSlot = 0;
		GetMemoryCardSlots(Now, LeftSlot, RightSlot);
		if (LeftSlot == RightSlot)
		{
			const FString CurrencyText = AwardActivityPoints(BHPS, 2);
			Character->RecoverStamina(2.0f);
			Character->AddFear(-2.0f);
			ResultText = FString::Printf(TEXT("Memory pair matched: %s. +2 %s."), *BHActivityCardName(LeftSlot), *CurrencyText);
		}
		else
		{
			ResultAccent = FLinearColor(1.0f, 0.44f, 0.22f, 1.0f);
			ResultText = FString::Printf(TEXT("No pair: %s and %s. Wait until both cards match."),
				*BHActivityCardName(LeftSlot),
				*BHActivityCardName(RightSlot));
		}
		break;
	}
	default:
		ResultText = TEXT("Train activity complete.");
		break;
	}

	ActivityUseCounter = FMath::Max(0, ActivityUseCounter + 1);
	ShowResult(Character, ResultText, ResultAccent);
}

FText ABHTrainActivityStation::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	switch (ActivityType)
	{
	case EBHTrainActivityType::SnackCart:
		return FText::FromString(TEXT("Eat Snack"));
	case EBHTrainActivityType::DrinkCooler:
		return FText::FromString(TEXT("Take Drink"));
	case EBHTrainActivityType::ReflexArcade:
		return FText::FromString(TEXT("Play Reflex Arcade"));
	case EBHTrainActivityType::MemoryTable:
		return FText::FromString(TEXT("Play Memory Cards"));
	default:
		return FText::FromString(GetActivityTitle(ActivityType));
	}
}

FBHInteractionPromptInfo ABHTrainActivityStation::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;

	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(BuildPromptRiskText(Character));
		return Info;
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Intermission || BHGS->TrainPhase == EBHTrainPhase::Inactive)
	{
		Info.DisabledReason = FText::FromString(TEXT("TRAIN ACTIVITY ONLY"));
	}
	else if (!IsPassengerEligibleForActivity(BHPS))
	{
		Info.DisabledReason = FText::FromString(TEXT("PASSENGERS ONLY"));
	}
	else
	{
		Info.DisabledReason = FText::FromString(TEXT("ACTIVITY LOCKED"));
	}
	return Info;
}

void ABHTrainActivityStation::ConfigureActivity(EBHTrainActivityType NewActivityType, int32 NewActivitySeed)
{
	ActivityType = NewActivityType;
	ActivitySeed = NewActivitySeed;
	ActivityUseCounter = 0;
	LastResultText = TEXT("");
	LastResultAccent = BHActivityAccent(ActivityType);
	LastUseTimeByPlayerId.Reset();
	InteractionLabel = FText::FromString(GetActivityTitle(ActivityType));
	ApplyActivityVisuals();
	RefreshActivityText();
	ForceNetUpdate();
}

FString ABHTrainActivityStation::GetActivityTitle(EBHTrainActivityType Type)
{
	switch (Type)
	{
	case EBHTrainActivityType::SnackCart:
		return TEXT("Snack Cart");
	case EBHTrainActivityType::DrinkCooler:
		return TEXT("Drink Cooler");
	case EBHTrainActivityType::ReflexArcade:
		return TEXT("Reflex Arcade");
	case EBHTrainActivityType::MemoryTable:
		return TEXT("Memory Cards");
	default:
		return TEXT("Train Activity");
	}
}

FString ABHTrainActivityStation::BuildActivitySummary(EBHTrainActivityType Type)
{
	switch (Type)
	{
	case EBHTrainActivityType::SnackCart:
		return TEXT("Grab a snack to recover stamina and lower fear. Short personal cooldown.");
	case EBHTrainActivityType::DrinkCooler:
		return TEXT("Take a drink to refill flashlight battery, recover stamina, and steady nerves.");
	case EBHTrainActivityType::ReflexArcade:
		return TEXT("Press E when the moving cursor reaches GREEN. Perfect timing awards small role points.");
	case EBHTrainActivityType::MemoryTable:
		return TEXT("Press E when both card names match. Matched pairs award small role points.");
	default:
		return TEXT("Optional train activity.");
	}
}

bool ABHTrainActivityStation::IsPassengerEligibleForActivity(const ABHPlayerState* PlayerState)
{
	return PlayerState
		&& PlayerState->LifeState == EBHPlayerLifeState::Alive
		&& PlayerState->PlayerRole != EBHPlayerRole::Unassigned;
}

void ABHTrainActivityStation::OnRep_ActivityState()
{
	InteractionLabel = FText::FromString(GetActivityTitle(ActivityType));
	ApplyActivityVisuals();
	RefreshActivityText();
}

float ABHTrainActivityStation::GetServerTimeSeconds() const
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

float ABHTrainActivityStation::GetCooldownDuration() const
{
	switch (ActivityType)
	{
	case EBHTrainActivityType::SnackCart:
		return 16.0f;
	case EBHTrainActivityType::DrinkCooler:
		return 14.0f;
	case EBHTrainActivityType::ReflexArcade:
		return 9.0f;
	case EBHTrainActivityType::MemoryTable:
		return 9.0f;
	default:
		return 4.0f;
	}
}

float ABHTrainActivityStation::GetCooldownRemaining(const ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS)
	{
		return 0.0f;
	}

	const float* LastUseTime = LastUseTimeByPlayerId.Find(BHPS->GetPlayerId());
	if (!LastUseTime)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, *LastUseTime + GetCooldownDuration() - GetServerTimeSeconds());
}

int32 ABHTrainActivityStation::GetReflexCursorSlot(float ServerTime) const
{
	const float RawSlot = FMath::Fmod(ServerTime * 5.25f + static_cast<float>(ActivitySeed % 17) * 0.37f, static_cast<float>(BHTrainReflexSlotCount));
	return FMath::Clamp(FMath::FloorToInt(RawSlot), 0, BHTrainReflexSlotCount - 1);
}

int32 ABHTrainActivityStation::GetDisplayedActivityRound() const
{
	// The reflex/memory TARGET must be the same value the client renders as the hint and the value the
	// server grades against. Previously it was folded from ActivityUseCounter, which the server bumped on
	// every use: between that bump and the counter replicating, a client still displayed the OLD target
	// while the server already graded against the NEW one, so input timed to the visible hint scored a miss.
	// Deriving the round from the synchronized server clock (GetServerWorldTimeSeconds, identical on server
	// and clients at any instant) makes the displayed and graded target agree by construction, with no
	// replication-window race. ActivityUseCounter remains for use accounting/cooldowns but no longer steers
	// the graded slot. The 6s cadence is slower than any cooldown so the target is stable across a single use.
	return FMath::FloorToInt(GetServerTimeSeconds() / 6.0f);
}

int32 ABHTrainActivityStation::GetReflexTargetSlot() const
{
	const int32 RawTarget = ActivitySeed + GetDisplayedActivityRound() * 3;
	return FMath::Abs(RawTarget) % BHTrainReflexSlotCount;
}

void ABHTrainActivityStation::GetMemoryCardSlots(float ServerTime, int32& OutLeftSlot, int32& OutRightSlot) const
{
	const int32 Round = GetDisplayedActivityRound();
	const int32 SeedA = FMath::Abs(ActivitySeed + Round * 5);
	const int32 SeedB = FMath::Abs(ActivitySeed * 3 + Round * 7 + 11);
	OutLeftSlot = (SeedA + FMath::FloorToInt(ServerTime * 1.55f)) % 4;
	OutRightSlot = (SeedB + FMath::FloorToInt(ServerTime * 1.95f)) % 4;
}

FString ABHTrainActivityStation::BuildLiveActivityBody() const
{
	const float ServerTime = GetServerTimeSeconds();
	switch (ActivityType)
	{
	case EBHTrainActivityType::ReflexArcade:
	{
		const int32 CursorSlot = GetReflexCursorSlot(ServerTime);
		const int32 TargetSlot = GetReflexTargetSlot();
		return FString::Printf(TEXT("TIME E ON GREEN\nNOW    %s\nGREEN  %s\nPerfect +2 pts / close steadies nerves."),
			*BHBuildSlotLine(CursorSlot, TEXT("X")),
			*BHBuildSlotLine(TargetSlot, TEXT("G")));
	}
	case EBHTrainActivityType::MemoryTable:
	{
		int32 LeftSlot = 0;
		int32 RightSlot = 0;
		GetMemoryCardSlots(ServerTime, LeftSlot, RightSlot);
		return FString::Printf(TEXT("MATCH THE PAIR\nLEFT  %s\nRIGHT %s\nPress E when both names match."),
			*BHActivityCardName(LeftSlot),
			*BHActivityCardName(RightSlot));
	}
	default:
		return BuildActivitySummary(ActivityType);
	}
}

FString ABHTrainActivityStation::BuildPromptRiskText(const ABHCharacter* Character) const
{
	const float CooldownRemaining = GetCooldownRemaining(Character);
	if (CooldownRemaining > 0.0f)
	{
		return FString::Printf(TEXT("READY IN %s"), *BHActivityClock(CooldownRemaining));
	}

	switch (ActivityType)
	{
	case EBHTrainActivityType::SnackCart:
		return TEXT("+STAMINA / -FEAR");
	case EBHTrainActivityType::DrinkCooler:
		return TEXT("+FLASHLIGHT / +STAMINA");
	case EBHTrainActivityType::ReflexArcade:
		return TEXT("TIME E ON GREEN / +2 PTS");
	case EBHTrainActivityType::MemoryTable:
		return TEXT("MATCH PAIR / +2 PTS");
	default:
		return TEXT("OPTIONAL ACTIVITY");
	}
}

FString ABHTrainActivityStation::AwardActivityPoints(ABHPlayerState* PlayerState, int32 Points) const
{
	const int32 ClampedPoints = FMath::Max(0, Points);
	if (!PlayerState || ClampedPoints <= 0)
	{
		return TEXT("points");
	}

	if (PlayerState->PlayerRole == EBHPlayerRole::Hunter)
	{
		PlayerState->AddHunterPoints(ClampedPoints);
		return TEXT("capture pts");
	}

	PlayerState->AddQuestionPoints(ClampedPoints);
	return TEXT("question pts");
}

void ABHTrainActivityStation::ApplyActivityVisuals()
{
	const FLinearColor Accent = BHActivityAccent(ActivityType);
	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, -4.0f), FRotator::ZeroRotator, FVector(0.86f, 0.34f, 1.04f), true);
	BHPropVisuals::ConfigurePart(ScreenPanel, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(44.0f, 0.0f, 28.0f), FRotator::ZeroRotator, FVector(0.020f, 0.285f, 0.34f));
	BHPropVisuals::ConfigurePart(CounterTop, BHPropVisuals::CubeMesh(), BHPropVisuals::DiamondPlateMaterial(), FVector(3.0f, 0.0f, 58.0f), FRotator::ZeroRotator, FVector(0.74f, 0.40f, 0.08f));
	BHPropVisuals::ConfigurePart(AccentStrip, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(46.5f, 0.0f, 72.0f), FRotator::ZeroRotator, FVector(0.018f, 0.35f, 0.055f));
	BHPropVisuals::ConfigurePart(PropA, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(16.0f, -22.0f, 80.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.22f));
	BHPropVisuals::ConfigurePart(PropB, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(16.0f, 0.0f, 80.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.22f));
	BHPropVisuals::ConfigurePart(PropC, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(16.0f, 22.0f, 80.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.22f));
	BHPropVisuals::ConfigurePart(ButtonA, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(48.0f, -18.0f, -16.0f), FRotator::ZeroRotator, FVector(0.060f));
	BHPropVisuals::ConfigurePart(ButtonB, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(48.0f, 0.0f, -16.0f), FRotator::ZeroRotator, FVector(0.060f));
	BHPropVisuals::ConfigurePart(ButtonC, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(48.0f, 18.0f, -16.0f), FRotator::ZeroRotator, FVector(0.060f));

	switch (ActivityType)
	{
	case EBHTrainActivityType::SnackCart:
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::RustedMetalMaterial(), FVector(0.0f, 0.0f, -12.0f), FRotator::ZeroRotator, FVector(0.98f, 0.46f, 0.78f), true);
		BHPropVisuals::ConfigurePart(PropA, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(12.0f, -24.0f, 78.0f), FRotator(0.0f, 0.0f, 8.0f), FVector(0.18f, 0.08f, 0.06f));
		BHPropVisuals::ConfigurePart(PropB, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(12.0f, 0.0f, 80.0f), FRotator(0.0f, 0.0f, -5.0f), FVector(0.20f, 0.09f, 0.06f));
		BHPropVisuals::ConfigurePart(PropC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(12.0f, 24.0f, 78.0f), FRotator(0.0f, 0.0f, 5.0f), FVector(0.17f, 0.08f, 0.06f));
		break;
	case EBHTrainActivityType::DrinkCooler:
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, -8.0f), FRotator::ZeroRotator, FVector(0.82f, 0.42f, 1.00f), true);
		BHPropVisuals::ConfigurePart(CounterTop, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(38.0f, 0.0f, 22.0f), FRotator::ZeroRotator, FVector(0.025f, 0.31f, 0.42f));
		BHPropVisuals::ConfigurePart(PropA, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(28.0f, -20.0f, 64.0f), FRotator::ZeroRotator, FVector(0.075f, 0.075f, 0.24f));
		BHPropVisuals::ConfigurePart(PropB, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(28.0f, 0.0f, 64.0f), FRotator::ZeroRotator, FVector(0.075f, 0.075f, 0.24f));
		BHPropVisuals::ConfigurePart(PropC, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(28.0f, 20.0f, 64.0f), FRotator::ZeroRotator, FVector(0.075f, 0.075f, 0.24f));
		break;
	case EBHTrainActivityType::ReflexArcade:
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, -18.0f), FRotator::ZeroRotator, FVector(0.78f, 0.40f, 1.20f), true);
		BHPropVisuals::ConfigurePart(CounterTop, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(34.0f, 0.0f, -30.0f), FRotator::ZeroRotator, FVector(0.18f, 0.36f, 0.07f));
		BHPropVisuals::ConfigurePart(PropA, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(49.0f, -20.0f, -14.0f), FRotator::ZeroRotator, FVector(0.060f));
		BHPropVisuals::ConfigurePart(PropB, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(49.0f, 0.0f, -14.0f), FRotator::ZeroRotator, FVector(0.060f));
		BHPropVisuals::ConfigurePart(PropC, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(49.0f, 20.0f, -14.0f), FRotator::ZeroRotator, FVector(0.060f));
		break;
	case EBHTrainActivityType::MemoryTable:
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::RustedMetalMaterial(), FVector(0.0f, 0.0f, -34.0f), FRotator::ZeroRotator, FVector(0.92f, 0.52f, 0.50f), true);
		BHPropVisuals::ConfigurePart(ScreenPanel, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(16.0f, 0.0f, 45.0f), FRotator::ZeroRotator, FVector(0.36f, 0.46f, 0.030f));
		BHPropVisuals::ConfigurePart(PropA, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(14.0f, -24.0f, 64.0f), FRotator(0.0f, 0.0f, -8.0f), FVector(0.18f, 0.12f, 0.018f));
		BHPropVisuals::ConfigurePart(PropB, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(14.0f, 0.0f, 64.0f), FRotator(0.0f, 0.0f, 4.0f), FVector(0.18f, 0.12f, 0.018f));
		BHPropVisuals::ConfigurePart(PropC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(14.0f, 24.0f, 64.0f), FRotator(0.0f, 0.0f, 8.0f), FVector(0.18f, 0.12f, 0.018f));
		break;
	default:
		break;
	}

	BHPropVisuals::TintPart(ScreenPanel, FLinearColor(0.010f, 0.018f, 0.020f, 1.0f), 0.0f);
	BHPropVisuals::TintPart(CounterTop, ActivityType == EBHTrainActivityType::DrinkCooler ? FLinearColor(0.08f, 0.18f, 0.22f, 1.0f) : FLinearColor(0.075f, 0.076f, 0.070f, 1.0f));
	BHPropVisuals::TintPart(AccentStrip, Accent, 2.1f);
	BHPropVisuals::TintPart(PropA, Accent, 0.9f);
	BHPropVisuals::TintPart(PropB, Accent * 0.78f + FLinearColor(0.12f, 0.12f, 0.10f, 0.0f), 0.5f);
	BHPropVisuals::TintPart(PropC, Accent * 0.55f + FLinearColor(0.18f, 0.12f, 0.08f, 0.0f), 0.35f);
	BHPropVisuals::TintPart(ButtonA, FLinearColor(0.18f, 0.96f, 0.42f, 1.0f), 1.3f);
	BHPropVisuals::TintPart(ButtonB, FLinearColor(0.96f, 0.78f, 0.16f, 1.0f), 1.0f);
	BHPropVisuals::TintPart(ButtonC, FLinearColor(0.96f, 0.24f, 0.16f, 1.0f), 1.0f);
	if (AccentLight)
	{
		AccentLight->SetLightColor(Accent);
	}
}

void ABHTrainActivityStation::RefreshActivityText()
{
	const FLinearColor Accent = LastResultText.IsEmpty() ? BHActivityAccent(ActivityType) : LastResultAccent;
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(GetActivityTitle(ActivityType).ToUpper()));
		TitleText->SetTextRenderColor(Accent.ToFColor(true));
	}
	if (DetailText)
	{
		TArray<FString> Lines;
		Lines.Add(BuildLiveActivityBody());
		if (!LastResultText.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("\nLAST: %s"), *LastResultText.Left(116)));
		}
		DetailText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
		DetailText->SetTextRenderColor(FColor(222, 244, 232));
	}
}

void ABHTrainActivityStation::ShowResult(ABHCharacter* Character, const FString& ResultText, const FLinearColor& ResultAccent)
{
	LastResultText = ResultText;
	LastResultAccent = ResultAccent;
	RefreshActivityText();
	ForceNetUpdate();

	if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
	{
		PC->ClientShowStatusMessage(ResultText, 3.25f);
	}
}
