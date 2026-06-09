// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BHAntiCheatSubsystem.generated.h"

class APawn;
class APlayerState;
class APlayerController;

// Categories of anomaly the monitor can raise. Small + stable for logging/telemetry.
UENUM()
enum class EBHAntiCheatFlag : uint8
{
	Speed,      // sustained horizontal speed beyond the server-allowed max (speedhack)
	Teleport,   // single-sample position jump far beyond what velocity*dt allows
	Fly,        // flying movement mode (players never fly in normal Blackout Hunt play)
	Generic     // reported by another system via ReportViolation()
};

/**
 * UBHAntiCheatSubsystem -- a LIGHTWEIGHT, SERVER-SIDE, OPT-IN cheat-anomaly monitor (a reversible CANDIDATE).
 *
 * Why this shape:
 *  - Self-contained & reversible: a UTickableWorldSubsystem auto-registers with the engine, so it wires into
 *    NOTHING. Adding or removing the feature is literally adding/deleting these two files -- it cannot collide
 *    with other work and is trivially `git`-reversible. (Unlike a normal feature, there are no call sites to
 *    rip out.)
 *  - Inert by default: the master CVar `bh.AntiCheat` defaults to 0 (off). The subsystem ticks but immediately
 *    early-outs, so it has ~zero cost and ZERO behaviour until a host explicitly enables it.
 *  - Classroom-safe: when enabled it defaults to LOG-ONLY (mode 1). Enforcement/kick (mode 2) requires an
 *    explicit opt-in AND a sustained, *decaying* violation budget, so a single lag spike or legitimate teleport
 *    never kicks a student. The listen-server host (== the server) is always trusted and never policed.
 *  - A BACKSTOP, not the primary defence: Blackout Hunt is already strongly server-authoritative (every client
 *    action is re-validated server-side; the CharacterMovementComponent already server-corrects movement). This
 *    layer OBSERVES the *authoritative* state of remote clients to surface a modified client (speed / teleport /
 *    fly) for the teacher, and offers a report API other systems can feed.
 *  - Build-safe: depends on ENGINE-ONLY API (no Blackout Hunt headers), so it can't break from BH header drift.
 *    It reads each pawn's *live* CharacterMovement MaxWalkSpeed as the allowed bound, so it automatically tracks
 *    role tuning, powerups, fear-slow, special moves AND the intentional "flow chain"/bunny-hop tech -- those
 *    raise MaxWalkSpeed, so they are NOT flagged.
 *
 * Enable in a test session (console): `bh.AntiCheat 1` (monitor+log) or `bh.AntiCheat 2` (enforce). Inspect with
 * `bh.AntiCheat.Dump`. All thresholds are CVars (`bh.AntiCheat.*`). Runs only on a listen/dedicated server.
 */
UCLASS()
class BLACKOUTHUNT_API UBHAntiCheatSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem / FTickableGameObject
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Convenience accessor (returns nullptr off-server / before world init).
	static UBHAntiCheatSubsystem* Get(const UObject* WorldContext);

	// Report API other systems MAY call (no-op unless enabled + on a server). Lets game code feed the monitor a
	// violation it detected itself (e.g. a rejected Server RPC, an impossible interaction), weighted into the
	// same decaying budget. Optional -- the candidate does not wire any call sites; this is here for later use.
	void ReportViolation(APlayerState* PlayerState, EBHAntiCheatFlag Flag, float Weight, const FString& Detail);

	// Tell the monitor a player was LEGITIMATELY teleported (locker exit, final-escape, roof reset, auto-board)
	// so the next position sample doesn't read as a teleport hack. Safe to call even when disabled. Optional --
	// without it, legitimate teleports merely produce a harmless one-off LOG line in log-only mode.
	void NotifyTrustedTeleport(APlayerState* PlayerState);

	// Print the current per-player report to the log (backs the `bh.AntiCheat.Dump` console command).
	void DumpReport() const;

private:
	struct FBHPlayerWatch
	{
		TWeakObjectPtr<APlayerState> PlayerState;
		TWeakObjectPtr<APawn> Pawn;
		int32 PlayerId = -1;
		FString PlayerName;
		FVector LastLocation = FVector::ZeroVector;
		bool bHasBaseline = false;
		bool bSuppressNextTeleport = false;
		int32 SpeedStreak = 0;        // consecutive over-speed samples
		float ViolationScore = 0.0f;  // leaky-bucket budget (enforce mode kicks when this exceeds the budget)
		int32 SpeedFlags = 0;
		int32 TeleportFlags = 0;
		int32 FlyFlags = 0;
		int32 GenericFlags = 0;
		double LastLogTime = 0.0;
		bool bKicked = false;
	};

	void ScanOnce(float DeltaSeconds, int32 Mode);
	FBHPlayerWatch& FindOrAddWatch(APlayerState* PS);
	void Accrue(FBHPlayerWatch& Watch, APlayerController* PC, EBHAntiCheatFlag Flag, float Weight, const FString& Detail, int32 Mode);

	TMap<int32, FBHPlayerWatch> Watches;   // keyed by stable PlayerId
	float Accumulator = 0.0f;
	bool bAnnounced = false;
};
