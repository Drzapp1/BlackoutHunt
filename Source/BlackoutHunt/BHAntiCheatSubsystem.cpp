// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHAntiCheatSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Stats/Stats.h"

DEFINE_LOG_CATEGORY_STATIC(LogBHAntiCheat, Log, All);

// ---- Tunables (all live CVars so a host can adjust mid-session; nothing is hard-coded into a build) ----------
namespace
{
	TAutoConsoleVariable<int32> CVarBHAntiCheat(
		TEXT("bh.AntiCheat"), 0,
		TEXT("Blackout Hunt anti-cheat monitor (server-only, opt-in).\n")
		TEXT("  0 = off (default, fully inert)\n")
		TEXT("  1 = monitor + log only (classroom-safe)\n")
		TEXT("  2 = enforce (kick on a sustained, decaying violation budget)"),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarBHACInterval(
		TEXT("bh.AntiCheat.Interval"), 0.5f,
		TEXT("Seconds between server scans."), ECVF_Default);

	TAutoConsoleVariable<float> CVarBHACSpeedTol(
		TEXT("bh.AntiCheat.SpeedTolerance"), 1.6f,
		TEXT("Allowed speed = live MaxWalkSpeed * this before a SPEED flag (generous to avoid false positives)."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarBHACSpeedSamples(
		TEXT("bh.AntiCheat.SpeedSustainedSamples"), 4,
		TEXT("Consecutive over-speed scans required before a SPEED flag (sustained, not a single spike)."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarBHACTeleTol(
		TEXT("bh.AntiCheat.TeleportTolerance"), 4.0f,
		TEXT("Allowed move = MaxSpeed*dt*this + slack before a TELEPORT flag."), ECVF_Default);

	TAutoConsoleVariable<float> CVarBHACTeleSlack(
		TEXT("bh.AntiCheat.TeleportSlackCm"), 300.0f,
		TEXT("Absolute cm of teleport slack for lag / short legitimate teleports."), ECVF_Default);

	TAutoConsoleVariable<float> CVarBHACDecay(
		TEXT("bh.AntiCheat.DecayPerSecond"), 1.0f,
		TEXT("Violation-score leak per second (so isolated false positives bleed off)."), ECVF_Default);

	TAutoConsoleVariable<float> CVarBHACKickBudget(
		TEXT("bh.AntiCheat.KickViolationBudget"), 15.0f,
		TEXT("Enforce mode only: kick once a player's violation score exceeds this."), ECVF_Default);

	TAutoConsoleVariable<float> CVarBHACLogThrottle(
		TEXT("bh.AntiCheat.LogThrottleSeconds"), 5.0f,
		TEXT("Minimum seconds between per-player log lines (0 = log every flag)."), ECVF_Default);

	TAutoConsoleVariable<int32> CVarBHACVerbose(
		TEXT("bh.AntiCheat.Verbose"), 0,
		TEXT("1 = log every flag, ignoring the throttle."), ECVF_Default);

	// Per-flag weights into the leaky-bucket budget.
	constexpr float BHACWeightSpeed = 5.0f;
	constexpr float BHACWeightTeleport = 2.0f;
	constexpr float BHACWeightFly = 6.0f;

	const TCHAR* BHACFlagName(EBHAntiCheatFlag Flag)
	{
		switch (Flag)
		{
		case EBHAntiCheatFlag::Speed:    return TEXT("SPEED");
		case EBHAntiCheatFlag::Teleport: return TEXT("TELEPORT");
		case EBHAntiCheatFlag::Fly:      return TEXT("FLY");
		default:                         return TEXT("GENERIC");
		}
	}

	// Console: dump the per-player report.
	void BHAntiCheatDumpCommand(UWorld* World)
	{
		if (World)
		{
			if (UBHAntiCheatSubsystem* AC = World->GetSubsystem<UBHAntiCheatSubsystem>())
			{
				AC->DumpReport();
			}
		}
	}

	FAutoConsoleCommandWithWorld GBHAntiCheatDumpCmd(
		TEXT("bh.AntiCheat.Dump"),
		TEXT("Print the Blackout Hunt anti-cheat monitor's per-player report."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&BHAntiCheatDumpCommand));
}

// ---------------------------------------------------------------------------------------------------------------

void UBHAntiCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UBHAntiCheatSubsystem::Deinitialize()
{
	Watches.Empty();
	Super::Deinitialize();
}

bool UBHAntiCheatSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Only real game / PIE worlds; never editor preview/inactive worlds.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UBHAntiCheatSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBHAntiCheatSubsystem, STATGROUP_Tickables);
}

UBHAntiCheatSubsystem* UBHAntiCheatSubsystem::Get(const UObject* WorldContext)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UBHAntiCheatSubsystem>();
	}
	return nullptr;
}

void UBHAntiCheatSubsystem::Tick(float DeltaTime)
{
	const int32 Mode = CVarBHAntiCheat.GetValueOnGameThread();
	if (Mode <= 0)
	{
		Accumulator = 0.0f;
		return;   // off -> fully inert
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Only police a server that has remote clients. A standalone game or a client has nothing to police here
	// (a client never has authority over other players; the host IS the server).
	const ENetMode NetMode = World->GetNetMode();
	if (NetMode != NM_ListenServer && NetMode != NM_DedicatedServer)
	{
		return;
	}

	if (!bAnnounced)
	{
		bAnnounced = true;
		UE_LOG(LogBHAntiCheat, Display,
			TEXT("Anti-cheat monitor ACTIVE (mode=%d, %s). Server-only backstop; checks: speed/teleport/fly. ")
			TEXT("Inspect with 'bh.AntiCheat.Dump'."),
			Mode, Mode >= 2 ? TEXT("ENFORCE/kick") : TEXT("log-only"));
	}

	const float Interval = FMath::Max(0.1f, CVarBHACInterval.GetValueOnGameThread());
	Accumulator += DeltaTime;
	if (Accumulator < Interval)
	{
		return;
	}
	const float Elapsed = Accumulator;
	Accumulator = 0.0f;

	ScanOnce(Elapsed, Mode);
}

void UBHAntiCheatSubsystem::ScanOnce(float DeltaSeconds, int32 Mode)
{
	UWorld* World = GetWorld();
	if (!World || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float SpeedTol = FMath::Max(1.0f, CVarBHACSpeedTol.GetValueOnGameThread());
	const int32 SpeedSamples = FMath::Max(1, CVarBHACSpeedSamples.GetValueOnGameThread());
	const float TeleTol = FMath::Max(1.0f, CVarBHACTeleTol.GetValueOnGameThread());
	const float TeleSlack = FMath::Max(0.0f, CVarBHACTeleSlack.GetValueOnGameThread());
	const float Decay = FMath::Max(0.0f, CVarBHACDecay.GetValueOnGameThread());

	TSet<int32> LiveIds;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || PC->IsLocalController())
		{
			continue;   // null, or the listen-server host (== the server) -> trusted, not policed
		}
		APlayerState* PS = PC->PlayerState;
		if (!PS)
		{
			continue;
		}
		ACharacter* Char = Cast<ACharacter>(PC->GetPawn());
		if (!Char)
		{
			continue;   // no character pawn (lobby/spectator/possession gap) -> nothing to sample
		}
		UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
		if (!CMC)
		{
			continue;
		}

		const int32 Id = PS->GetPlayerId();
		LiveIds.Add(Id);
		FBHPlayerWatch& Watch = FindOrAddWatch(PS);
		Watch.PlayerName = PS->GetPlayerName();

		// Leak the violation budget so isolated false positives bleed away over time.
		Watch.ViolationScore = FMath::Max(0.0f, Watch.ViolationScore - Decay * DeltaSeconds);

		// Re-baseline on a possession change (don't read a fresh pawn's first delta as a teleport).
		if (Watch.Pawn.Get() != Char)
		{
			Watch.Pawn = Char;
			Watch.bHasBaseline = false;
			Watch.SpeedStreak = 0;
			Watch.bSuppressNextTeleport = true;
		}

		const FVector Loc = Char->GetActorLocation();
		// The live, server-set bound. Includes role tuning, powerups, fear-slow, special moves, AND the
		// intentional flow-chain / bunny-hop tech (they raise MaxWalkSpeed) -> those are not flagged.
		const float AllowedMax = FMath::Max3(CMC->MaxWalkSpeed, CMC->GetMaxSpeed(), 50.0f);
		const float Speed2D = CMC->Velocity.Size2D();

		// ---- Fly: players never use MOVE_Flying in normal play (high confidence). ----
		if (CMC->MovementMode == MOVE_Flying)
		{
			Accrue(Watch, PC, EBHAntiCheatFlag::Fly, BHACWeightFly,
				FString::Printf(TEXT("MOVE_Flying spd=%.0f"), Speed2D), Mode);
		}

		// ---- Speed: sustained authoritative horizontal velocity over the allowed bound. ----
		if (Speed2D > AllowedMax * SpeedTol)
		{
			if (++Watch.SpeedStreak >= SpeedSamples)
			{
				Accrue(Watch, PC, EBHAntiCheatFlag::Speed, BHACWeightSpeed,
					FString::Printf(TEXT("spd=%.0f > max=%.0f x%.2f"), Speed2D, AllowedMax, SpeedTol), Mode);
				Watch.SpeedStreak = 0;   // re-arm; a continuing hack re-trips each sustained window
			}
		}
		else
		{
			Watch.SpeedStreak = 0;
		}

		// ---- Teleport: a single-sample position jump far beyond what velocity*dt can explain. ----
		if (Watch.bHasBaseline && !Watch.bSuppressNextTeleport)
		{
			const float Moved = FVector::Dist(Loc, Watch.LastLocation);
			const float AllowedMove = AllowedMax * DeltaSeconds * TeleTol + TeleSlack;
			if (Moved > AllowedMove)
			{
				Accrue(Watch, PC, EBHAntiCheatFlag::Teleport, BHACWeightTeleport,
					FString::Printf(TEXT("jump=%.0fcm > %.0fcm"), Moved, AllowedMove), Mode);
			}
		}
		Watch.bSuppressNextTeleport = false;
		Watch.LastLocation = Loc;
		Watch.bHasBaseline = true;
	}

	// Drop watches for players who left so the map can't grow over a long session.
	for (TMap<int32, FBHPlayerWatch>::TIterator It(Watches); It; ++It)
	{
		if (!LiveIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

UBHAntiCheatSubsystem::FBHPlayerWatch& UBHAntiCheatSubsystem::FindOrAddWatch(APlayerState* PS)
{
	const int32 Id = PS ? PS->GetPlayerId() : -1;
	FBHPlayerWatch& Watch = Watches.FindOrAdd(Id);
	Watch.PlayerId = Id;
	if (!Watch.PlayerState.IsValid())
	{
		Watch.PlayerState = PS;
	}
	return Watch;
}

void UBHAntiCheatSubsystem::Accrue(FBHPlayerWatch& Watch, APlayerController* PC, EBHAntiCheatFlag Flag, float Weight, const FString& Detail, int32 Mode)
{
	Watch.ViolationScore += Weight;
	switch (Flag)
	{
	case EBHAntiCheatFlag::Speed:    ++Watch.SpeedFlags; break;
	case EBHAntiCheatFlag::Teleport: ++Watch.TeleportFlags; break;
	case EBHAntiCheatFlag::Fly:      ++Watch.FlyFlags; break;
	default:                         ++Watch.GenericFlags; break;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float Throttle = CVarBHACLogThrottle.GetValueOnGameThread();
	const bool bVerbose = CVarBHACVerbose.GetValueOnGameThread() != 0;
	if (bVerbose || Throttle <= 0.0f || (Now - Watch.LastLogTime) >= Throttle)
	{
		Watch.LastLogTime = Now;
		UE_LOG(LogBHAntiCheat, Warning, TEXT("[%s id=%d] %s (%s) score=%.1f"),
			*Watch.PlayerName, Watch.PlayerId, BHACFlagName(Flag), *Detail, Watch.ViolationScore);
	}

	// Enforcement is opt-in (mode >= 2) and only on a sustained, decaying budget -- never on a single blip.
	if (Mode >= 2 && !Watch.bKicked && Watch.ViolationScore >= CVarBHACKickBudget.GetValueOnGameThread())
	{
		Watch.bKicked = true;
		UE_LOG(LogBHAntiCheat, Error, TEXT("[%s id=%d] ENFORCE: removing (score=%.1f >= budget)"),
			*Watch.PlayerName, Watch.PlayerId, Watch.ViolationScore);
		if (PC)
		{
			if (UWorld* W = GetWorld())
			{
				if (AGameModeBase* GM = W->GetAuthGameMode())
				{
					if (GM->GameSession)
					{
						GM->GameSession->KickPlayer(PC, FText::FromString(
							TEXT("Removed by anti-cheat (anomalous movement). Contact the host to rejoin.")));
					}
				}
			}
		}
	}
}

void UBHAntiCheatSubsystem::ReportViolation(APlayerState* PlayerState, EBHAntiCheatFlag Flag, float Weight, const FString& Detail)
{
	if (!PlayerState || CVarBHAntiCheat.GetValueOnGameThread() <= 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const ENetMode NetMode = World->GetNetMode();
	if (NetMode != NM_ListenServer && NetMode != NM_DedicatedServer)
	{
		return;
	}

	FBHPlayerWatch& Watch = FindOrAddWatch(PlayerState);
	Watch.PlayerName = PlayerState->GetPlayerName();
	APlayerController* PC = Cast<APlayerController>(PlayerState->GetOwningController());
	Accrue(Watch, PC, Flag, Weight, Detail, CVarBHAntiCheat.GetValueOnGameThread());
}

void UBHAntiCheatSubsystem::NotifyTrustedTeleport(APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}
	if (FBHPlayerWatch* Watch = Watches.Find(PlayerState->GetPlayerId()))
	{
		Watch->bSuppressNextTeleport = true;
	}
}

void UBHAntiCheatSubsystem::DumpReport() const
{
	UE_LOG(LogBHAntiCheat, Display, TEXT("==== Anti-cheat report: %d tracked, mode=%d ===="),
		Watches.Num(), CVarBHAntiCheat.GetValueOnGameThread());
	for (const TPair<int32, FBHPlayerWatch>& Pair : Watches)
	{
		const FBHPlayerWatch& Watch = Pair.Value;
		UE_LOG(LogBHAntiCheat, Display,
			TEXT("  id=%d %-16s score=%5.1f  speed=%d tele=%d fly=%d generic=%d%s"),
			Pair.Key, *Watch.PlayerName, Watch.ViolationScore,
			Watch.SpeedFlags, Watch.TeleportFlags, Watch.FlyFlags, Watch.GenericFlags,
			Watch.bKicked ? TEXT("  [KICKED]") : TEXT(""));
	}
}
