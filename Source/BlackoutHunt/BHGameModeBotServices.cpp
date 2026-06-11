// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHGameMode.h"

#include "BHAtmosphereDirector.h"
#include "BHBotController.h"
#include "BHBotPolicySubsystem.h"
#include "BHBreaker.h"
#include "BHCharacter.h"
#include "BHExitGate.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHLocker.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace
{
FLinearColor BotAvatarColorForIndex(int32 Index)
{
	static const FLinearColor Palette[] = {
		FLinearColor(0.22f, 0.58f, 0.74f, 1.0f),
		FLinearColor(0.78f, 0.46f, 0.18f, 1.0f),
		FLinearColor(0.46f, 0.72f, 0.28f, 1.0f),
		FLinearColor(0.70f, 0.30f, 0.42f, 1.0f),
		FLinearColor(0.52f, 0.44f, 0.86f, 1.0f),
		FLinearColor(0.84f, 0.75f, 0.24f, 1.0f),
		FLinearColor(0.28f, 0.68f, 0.62f, 1.0f),
		FLinearColor(0.76f, 0.76f, 0.80f, 1.0f)
	};

	return Palette[FMath::Abs(Index) % UE_ARRAY_COUNT(Palette)];
}

FString NormalizeBotServiceLevelName(FString LevelName)
{
	LevelName.TrimStartAndEndInline();
	if (LevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || LevelName.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
	{
		return TEXT("Foggrounds");
	}
	return LevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? TEXT("Substation") : TEXT("Facility");
}

FString FogPresetToBotServiceString(EBHFogPreset Preset)
{
	switch (Preset)
	{
	case EBHFogPreset::Light:
		return TEXT("Light");
	case EBHFogPreset::Extreme:
		return TEXT("Extreme");
	case EBHFogPreset::Heavy:
	default:
		return TEXT("Heavy");
	}
}

FString BotDifficultyToStatusString(EBHBotDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EBHBotDifficulty::Easy:
		return TEXT("Easy");
	case EBHBotDifficulty::Hard:
		return TEXT("Hard");
	case EBHBotDifficulty::Normal:
	default:
		return TEXT("Normal");
	}
}

FString BotStimulusToStatusString(EBHBotStimulusType Type)
{
	const UEnum* Enum = StaticEnum<EBHBotStimulusType>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Type)) : TEXT("Unknown");
}
}

bool ABHGameMode::IsBotMode() const
{
	return bBotMode;
}

EBHBotDifficulty ABHGameMode::GetBotDifficulty() const
{
	return BotDifficulty;
}

bool ABHGameMode::IsRuntimeNavigationReady() const
{
	return bRuntimeNavigationReady;
}

bool ABHGameMode::GetLatestBotNoiseLocation(float MaxAgeSeconds, FVector& OutLocation) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (LastBotNoiseTime < -1000.0f || Now - LastBotNoiseTime > MaxAgeSeconds)
	{
		return false;
	}

	OutLocation = LastBotNoiseLocation;
	return true;
}

FVector ABHGameMode::GetRandomBotPatrolPoint() const
{
	if (!ScarePoints.IsEmpty())
	{
		return ScarePoints[FMath::RandRange(0, ScarePoints.Num() - 1)];
	}
	if (!SurvivorSpawns.IsEmpty())
	{
		return SurvivorSpawns[FMath::RandRange(0, SurvivorSpawns.Num() - 1)];
	}
	return FVector::ZeroVector;
}

void ABHGameMode::ReportBotStimulus(EBHBotStimulusType Type, const FVector& Location, AActor* SourceActor, AActor* TargetActor, const FString& Reason, float Strength)
{
	if (!GetWorld())
	{
		return;
	}

	SweepExpiredBotTacticalState();

	FBHBotStimulus Stimulus;
	Stimulus.Type = Type;
	Stimulus.SourceActor = SourceActor;
	Stimulus.TargetActor = TargetActor;
	Stimulus.Location = Location;
	Stimulus.TimeSeconds = GetWorld()->GetTimeSeconds();
	Stimulus.Strength = FMath::Max(0.0f, Strength);
	Stimulus.Reason = Reason;
	BotWorldStimuli.Add(Stimulus);
	if (BotWorldStimuli.Num() > 80)
	{
		BotWorldStimuli.RemoveAt(0, BotWorldStimuli.Num() - 80, EAllowShrinking::No);
	}
}

bool ABHGameMode::GetBotWorldMemorySnapshot(FBHBotMemory& OutMemory, float MaxAgeSeconds) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	OutMemory = FBHBotMemory();
	for (int32 Index = BotWorldStimuli.Num() - 1; Index >= 0; --Index)
	{
		const FBHBotStimulus& Stimulus = BotWorldStimuli[Index];
		if (Now - Stimulus.TimeSeconds > MaxAgeSeconds)
		{
			continue;
		}

		OutMemory.RecentStimuli.Add(Stimulus);
		if (Stimulus.Type == EBHBotStimulusType::Noise && OutMemory.LastHeardTime < Stimulus.TimeSeconds)
		{
			OutMemory.LastHeardLocation = Stimulus.Location;
			OutMemory.LastHeardTime = Stimulus.TimeSeconds;
		}
		if (Stimulus.Type == EBHBotStimulusType::Sight)
		{
			ABHCharacter* TargetCharacter = Cast<ABHCharacter>(Stimulus.TargetActor.Get());
			const ABHPlayerState* TargetPS = TargetCharacter ? TargetCharacter->GetPlayerState<ABHPlayerState>() : nullptr;
			if (TargetPS && TargetPS->IsAliveSurvivor() && OutMemory.LastSeenSurvivorTime < Stimulus.TimeSeconds)
			{
				OutMemory.LastSeenSurvivor = TargetCharacter;
				OutMemory.LastSeenSurvivorLocation = Stimulus.Location;
				OutMemory.LastSeenSurvivorTime = Stimulus.TimeSeconds;
			}
			else if (TargetPS && TargetPS->IsAliveHunter() && OutMemory.LastSeenHunterTime < Stimulus.TimeSeconds)
			{
				OutMemory.LastSeenHunter = TargetCharacter;
				OutMemory.LastSeenHunterLocation = Stimulus.Location;
				OutMemory.LastSeenHunterTime = Stimulus.TimeSeconds;
			}
		}
	}

	OutMemory.ThreatPressure = FMath::Clamp(static_cast<float>(OutMemory.RecentStimuli.Num()) / 10.0f, 0.0f, 1.0f);
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (BHGS)
	{
		const int32 RemainingWork = FMath::Max(0, BHGS->BreakersRequired - BHGS->BreakersCompleted)
			+ FMath::Max(0, BHGS->SideObjectivesRequired - BHGS->SideObjectivesCompleted);
		OutMemory.ObjectivePressure = BHGS->bExitUnlocked ? 1.0f : FMath::Clamp(1.0f - static_cast<float>(RemainingWork) / 8.0f, 0.0f, 1.0f);
	}
	return !OutMemory.RecentStimuli.IsEmpty();
}

void ABHGameMode::SetBotCount(ABHPlayerController* RequestingController, int32 NewBotCount)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change bot count")))
	{
		return;
	}

	TargetBotCount = FMath::Clamp(NewBotCount, 0, FMath::Max(0, MaxPlayers - 1));
	const bool bShouldEnableBotMode = TargetBotCount > 0;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		const bool bCanRefreshRosterNow = BHGS->RoundPhase == EBHRoundPhase::Lobby || bPracticeMode;
		if (bCanRefreshRosterNow && !bShouldEnableBotMode)
		{
			while (RemoveOneBot())
			{
			}
		}
		// Only flip bBotMode when the roster can actually be refreshed now (Lobby/practice). Mid-round the
		// change is queued (TargetBotCount is already updated above), so flipping bBotMode here would desync it
		// from the still-live bots: RefreshBotRoster early-returns on !bBotMode, so a later human join/leave
		// could no longer re-clamp the live-bot count. Keep the current value until the next round applies it.
		if (bCanRefreshRosterNow)
		{
			bBotMode = bShouldEnableBotMode;
		}
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
		if (bCanRefreshRosterNow)
		{
			if (bBotMode)
			{
				RefreshBotRoster(RequestingController);
			}
			else if (RequestingController)
			{
				RequestingController->ClientShowStatusMessage(TEXT("Bot roster disabled."), 3.0f);
			}
		}
		else if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Bot count queued for next round: %d."), TargetBotCount), 3.0f);
		}
	}
}

void ABHGameMode::FillBotsToCapacity(ABHPlayerController* RequestingController)
{
	// Aim for a full table: one bot per empty slot after the current humans. SetBotCount clamps to
	// MaxPlayers - 1 and RefreshBotRoster re-clamps live bots to (MaxPlayers - humans) every time a
	// human joins, so this is a "fill it up now" request that still leaves room for late arrivals.
	const int32 HumanCount = CountHumanPlayers();
	const int32 DesiredBots = FMath::Clamp(MaxPlayers - HumanCount, 0, FMath::Max(0, MaxPlayers - 1));
	SetBotCount(RequestingController, DesiredBots);
}

void ABHGameMode::SetBotDifficulty(ABHPlayerController* RequestingController, EBHBotDifficulty NewDifficulty)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change bot difficulty")))
	{
		return;
	}

	BotDifficulty = NewDifficulty;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Bot difficulty set to %s."), *BotDifficultyToStatusString(BotDifficulty)), 3.0f);
	}
}

void ABHGameMode::ForceBotHunt(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("force bot hunt")))
	{
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS)
	{
		return;
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Hunt)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Bot hunt is already running."), 2.5f);
		}
		return;
	}

	if (!bBotMode)
	{
		bBotMode = true;
		TargetBotCount = FMath::Max(TargetBotCount, GetDefault<UBHGameSettings>()->DefaultBotCount);
		RefreshBotRoster(RequestingController);
	}

	StartHuntPhaseImmediately();
}

void ABHGameMode::StartBotSoak(ABHPlayerController* RequestingController, const FString& LevelName, int32 DurationSeconds, int32 NewBotCount)
{
	if (!RequireHostAdmin(RequestingController, TEXT("start bot soak")))
	{
		return;
	}

	const FString NormalizedLevel = NormalizeBotServiceLevelName(LevelName);
	if (!RuntimeLevelName.Equals(NormalizedLevel, ESearchCase::IgnoreCase) && GetWorld())
	{
		const FString TravelURL = FString::Printf(TEXT("%s?listen?BHLevel=%s?BHFogPreset=%s?BHBotMode=1?BHBotCount=%d?BHBotDifficulty=%s?BHHuntSeconds=%d?BHForceHunt=1"),
			*BHResolveLevelMapPackage(NormalizedLevel),
			*NormalizedLevel,
			*FogPresetToBotServiceString(NextFogPreset),
			FMath::Clamp(NewBotCount, 0, FMath::Max(0, MaxPlayers - 1)),
			*BotDifficultyToStatusString(BotDifficulty),
			FMath::Clamp(DurationSeconds, 30, 3600));
		RequestServerTravel(TravelURL);
		return;
	}

	HuntSeconds = FMath::Clamp(DurationSeconds, 30, 3600);
	bBotMode = true;
	TargetBotCount = FMath::Clamp(NewBotCount, 0, FMath::Max(0, MaxPlayers - 1));
	RefreshBotRoster(RequestingController);
	ForceBotHunt(RequestingController);
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Bot soak started: %s, %d seconds, %d bots."), *RuntimeLevelName, HuntSeconds, TargetBotCount), 4.0f);
	}
}

bool ABHGameMode::ClaimBotObjective(AController* Claimant, AActor* Target, EBHBotIntent Intent, float ClaimSeconds)
{
	if (!Claimant || !Target || !GetWorld() || !IsBotTargetStillUseful(Target))
	{
		return false;
	}

	SweepExpiredBotTacticalState();

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bExclusive = IsExclusiveBotClaimIntent(Intent);
	for (FBHBotObjectiveClaim& Claim : BotObjectiveClaims)
	{
		if (Claim.Target.Get() != Target || Claim.ExpireTimeSeconds <= Now)
		{
			continue;
		}
		if (Claim.Claimant.Get() == Claimant)
		{
			Claim.Intent = Intent;
			Claim.ExpireTimeSeconds = Now + FMath::Max(1.0f, ClaimSeconds);
			return true;
		}
		if (bExclusive)
		{
			return false;
		}
	}

	ReleaseBotObjective(Claimant);

	FBHBotObjectiveClaim NewClaim;
	NewClaim.Target = Target;
	NewClaim.Claimant = Claimant;
	NewClaim.Intent = Intent;
	NewClaim.ExpireTimeSeconds = Now + FMath::Max(1.0f, ClaimSeconds);
	BotObjectiveClaims.Add(NewClaim);
	return true;
}

void ABHGameMode::ReleaseBotObjective(AController* Claimant, AActor* Target)
{
	if (!Claimant)
	{
		return;
	}

	BotObjectiveClaims.RemoveAll([Claimant, Target](const FBHBotObjectiveClaim& Claim)
	{
		return Claim.Claimant.Get() == Claimant && (!Target || Claim.Target.Get() == Target);
	});
}

int32 ABHGameMode::CountBotClaimsForTarget(const AActor* Target) const
{
	if (!Target || !GetWorld())
	{
		return 0;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	int32 Count = 0;
	for (const FBHBotObjectiveClaim& Claim : BotObjectiveClaims)
	{
		if (Claim.Target.Get() == Target && Claim.ExpireTimeSeconds > Now)
		{
			++Count;
		}
	}
	return Count;
}

bool ABHGameMode::IsBotTargetOnCooldown(const AController* Claimant, const AActor* Target) const
{
	if (!Claimant || !Target || !GetWorld())
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	for (const FBHBotTargetCooldown& Cooldown : BotTargetCooldowns)
	{
		if (Cooldown.Claimant.Get() == Claimant && Cooldown.Target.Get() == Target && Cooldown.ExpireTimeSeconds > Now)
		{
			return true;
		}
	}
	return false;
}

void ABHGameMode::AddBotTargetCooldown(AController* Claimant, AActor* Target, float CooldownSeconds, const FString& Reason)
{
	if (!Claimant || !Target || !GetWorld())
	{
		return;
	}

	SweepExpiredBotTacticalState();
	ReleaseBotObjective(Claimant, Target);

	BotTargetCooldowns.RemoveAll([Claimant, Target](const FBHBotTargetCooldown& Cooldown)
	{
		return Cooldown.Claimant.Get() == Claimant && Cooldown.Target.Get() == Target;
	});

	FBHBotTargetCooldown Cooldown;
	Cooldown.Claimant = Claimant;
	Cooldown.Target = Target;
	Cooldown.ExpireTimeSeconds = GetWorld()->GetTimeSeconds() + FMath::Max(1.0f, CooldownSeconds);
	BotTargetCooldowns.Add(Cooldown);
	ReportBotStimulus(EBHBotStimulusType::Unreachable, Target->GetActorLocation(), Claimant->GetPawn(), Target, Reason, 0.5f);

	const FString LogKey = FString::Printf(TEXT("%s|%s"), *GetNameSafe(Target), *Reason);
	if (!LoggedBotTacticalWarnings.Contains(LogKey))
	{
		LoggedBotTacticalWarnings.Add(LogKey);
		UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt bot target cooldown: %s avoided for %.1fs after %s first reported %s"),
			*GetNameSafe(Target),
			CooldownSeconds,
			*GetNameSafe(Claimant),
			*Reason);
	}
}

bool ABHGameMode::GetBotApproachPoint(AActor* Target, const FVector& FromLocation, float DesiredDistance, FVector& OutLocation)
{
	if (!Target)
	{
		return false;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		OutLocation = Target->GetActorLocation();
		return true;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const float RadiusA = FMath::Clamp(DesiredDistance, 180.0f, 620.0f);
	const float Radii[] = { RadiusA, 320.0f, 480.0f, 650.0f, 820.0f };
	float BestScore = TNumericLimits<float>::Max();
	FVector BestLocation = FVector::ZeroVector;
	bool bFound = false;

	for (float Radius : Radii)
	{
		for (int32 Step = 0; Step < 16; ++Step)
		{
			const float Angle = static_cast<float>(Step) * (2.0f * PI / 16.0f);
			const FVector Candidate = TargetLocation + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
			FNavLocation Projected;
			if (!NavSystem->ProjectPointToNavigation(Candidate, Projected, FVector(260.0f, 260.0f, 280.0f)))
			{
				continue;
			}

			UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), FromLocation, Projected.Location);
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				continue;
			}

			float CrowdPenalty = 0.0f;
			for (TActorIterator<ABHCharacter> CharacterIt(GetWorld()); CharacterIt; ++CharacterIt)
			{
				const ABHCharacter* Character = *CharacterIt;
				const ABHPlayerState* CharacterPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
				if (!Character || !CharacterPS || CharacterPS->LifeState != EBHPlayerLifeState::Alive)
				{
					continue;
				}

				const FVector CharacterLocation = Character->GetActorLocation();
				if (FVector::DistSquared2D(CharacterLocation, FromLocation) <= FMath::Square(120.0f))
				{
					continue;
				}

				const float Separation = FVector::Dist2D(CharacterLocation, Projected.Location);
				if (Separation < 380.0f)
				{
					CrowdPenalty += FMath::Square(380.0f - Separation) * 80.0f;
				}
			}

			const float Score = FVector::DistSquared2D(FromLocation, Projected.Location)
				+ FMath::Abs(FVector::Dist2D(TargetLocation, Projected.Location) - RadiusA) * 100.0f
				+ CrowdPenalty;
			if (Score < BestScore)
			{
				BestScore = Score;
				BestLocation = Projected.Location;
				bFound = true;
			}
		}
	}

	if (bFound)
	{
		OutLocation = BestLocation;
		return true;
	}

	FNavLocation DirectProjected;
	if (NavSystem->ProjectPointToNavigation(TargetLocation, DirectProjected, FVector(320.0f, 320.0f, 300.0f)))
	{
		OutLocation = DirectProjected.Location;
		return true;
	}
	return false;
}

int32 ABHGameMode::RunBotNavCheck(FString& OutSummary)
{
	SweepExpiredBotTacticalState();

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		OutSummary = TEXT("Bot nav check failed: no navigation system.");
		return 0;
	}

	int32 Total = 0;
	int32 Failed = 0;
	TArray<FString> Failures;
	auto CheckPoint = [&](const FString& Label, const FVector& Location)
	{
		++Total;
		FNavLocation Projected;
		if (!NavSystem->ProjectPointToNavigation(Location, Projected, FVector(320.0f, 320.0f, 300.0f)))
		{
			++Failed;
			if (Failures.Num() < 10)
			{
				Failures.Add(Label);
			}
		}
	};
	auto CheckActor = [&](const FString& Label, AActor* Actor)
	{
		if (!Actor)
		{
			return;
		}
		++Total;
		FVector Approach = FVector::ZeroVector;
		if (!GetBotApproachPoint(Actor, Actor->GetActorLocation() + FVector(800.0f, 0.0f, 0.0f), 420.0f, Approach))
		{
			++Failed;
			if (Failures.Num() < 10)
			{
				Failures.Add(FString::Printf(TEXT("%s:%s"), *Label, *GetNameSafe(Actor)));
			}
		}
	};

	CheckPoint(TEXT("TeacherSpawn"), HunterSpawn);
	for (int32 Index = 0; Index < SurvivorSpawns.Num(); ++Index)
	{
		CheckPoint(FString::Printf(TEXT("SurvivorSpawn%d"), Index), SurvivorSpawns[Index]);
	}
	for (ABHObjectiveStation* Station : ObjectiveStations)
	{
		if (Station && Station->IsDirectorActive() && !Station->IsCompleted())
		{
			CheckActor(TEXT("Station"), Station);
		}
	}
	for (ABHBreaker* Breaker : BreakerActors)
	{
		if (Breaker && Breaker->IsDirectorActive() && !Breaker->IsRepaired())
		{
			CheckActor(TEXT("Breaker"), Breaker);
		}
	}
	for (ABHExitGate* ExitGate : ExitGates)
	{
		if (ExitGate && ExitGate->IsDirectorActive())
		{
			CheckActor(TEXT("Exit"), ExitGate);
		}
	}
	for (TActorIterator<ABHLocker> It(GetWorld()); It; ++It)
	{
		CheckActor(TEXT("Locker"), *It);
	}

	OutSummary = FString::Printf(TEXT("Bot nav check: %d/%d passed, failed=%d%s%s"),
		Total - Failed,
		Total,
		Failed,
		Failures.IsEmpty() ? TEXT("") : TEXT(" failures="),
		Failures.IsEmpty() ? TEXT("") : *FString::Join(Failures, TEXT(",")));
	UE_LOG(LogTemp, Log, TEXT("%s"), *OutSummary);
	return Failed;
}

FString ABHGameMode::GetBotStatusReport() const
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UBHBotPolicySubsystem* Policy = GameInstance ? GameInstance->GetSubsystem<UBHBotPolicySubsystem>() : nullptr;
	const FString PolicyStatus = Policy ? Policy->GetPolicyStatus() : TEXT("policy=unavailable");
	int32 BotHunterCount = 0;
	int32 BotSurvivorCount = 0;
	int32 BotFakeHunterCount = 0;
	TMap<FString, int32> ArchetypeCounts;
	for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
	{
		const ABHPlayerState* BotPS = Bot ? Bot->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!BotPS)
		{
			continue;
		}
		ArchetypeCounts.FindOrAdd(Bot->GetBotArchetypeLabel())++;
		if (BotPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			++BotHunterCount;
		}
		else if (BotPS->PlayerRole == EBHPlayerRole::Survivor)
		{
			++BotSurvivorCount;
		}
		else if (BotPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			++BotFakeHunterCount;
		}
	}
	ArchetypeCounts.KeySort(TLess<FString>());
	TArray<FString> ArchetypeParts;
	for (const TPair<FString, int32>& Pair : ArchetypeCounts)
	{
		ArchetypeParts.Add(FString::Printf(TEXT("%s:%d"), *Pair.Key, Pair.Value));
	}
	const FString ArchetypeSummary = ArchetypeParts.IsEmpty() ? FString(TEXT("none")) : FString::Join(ArchetypeParts, TEXT(","));

	return FString::Printf(TEXT("BotStatus mode=%s target=%d active=%d botHunters=%d botSurvivors=%d botMonitors=%d archetypes=%s difficulty=%s nav=%s claims=%d cooldowns=%d stimuli=%d map=%s %s"),
		bBotMode ? TEXT("on") : TEXT("off"),
		TargetBotCount,
		BotControllers.Num(),
		BotHunterCount,
		BotSurvivorCount,
		BotFakeHunterCount,
		*ArchetypeSummary,
		*BotDifficultyToStatusString(BotDifficulty),
		bRuntimeNavigationReady ? TEXT("ready") : TEXT("building"),
		BotObjectiveClaims.Num(),
		BotTargetCooldowns.Num(),
		BotWorldStimuli.Num(),
		*RuntimeLevelName,
		*PolicyStatus);
}

FString ABHGameMode::GetBotMemoryReport() const
{
	TArray<FString> Lines;
	Lines.Add(GetBotStatusReport());
	Lines.Add(GetAtmosphereDebugStatus());
	for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
	{
		if (Bot && Lines.Num() < 8)
		{
			Lines.Add(Bot->GetBotDebugLine());
		}
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	for (int32 Index = BotWorldStimuli.Num() - 1; Index >= 0 && Lines.Num() < 13; --Index)
	{
		const FBHBotStimulus& Stimulus = BotWorldStimuli[Index];
		Lines.Add(FString::Printf(TEXT("%s age=%.1f loc=(%.0f,%.0f,%.0f) src=%s target=%s reason=%s"),
			*BotStimulusToStatusString(Stimulus.Type),
			Now - Stimulus.TimeSeconds,
			Stimulus.Location.X,
			Stimulus.Location.Y,
			Stimulus.Location.Z,
			*GetNameSafe(Stimulus.SourceActor.Get()),
			*GetNameSafe(Stimulus.TargetActor.Get()),
			*Stimulus.Reason));
	}
	return FString::Join(Lines, TEXT(" | "));
}

FString ABHGameMode::GetAtmosphereDebugStatus() const
{
	return AtmosphereDirector ? AtmosphereDirector->GetDebugStatus() : TEXT("Atmosphere unavailable");
}

void ABHGameMode::RefreshBotRoster(ABHPlayerController* RequestingController)
{
	if (!HasAuthority() || !bBotMode || !GetWorld())
	{
		return;
	}

	BotControllers.RemoveAll([](const TObjectPtr<ABHBotController>& Bot)
	{
		return !IsValid(Bot);
	});

	const int32 HumanCount = CountHumanPlayers();
	const int32 DesiredLiveBots = FMath::Clamp(TargetBotCount, 0, FMath::Max(0, MaxPlayers - HumanCount));
	while (BotControllers.Num() > DesiredLiveBots)
	{
		if (!RemoveOneBot())
		{
			break;
		}
	}

	while (BotControllers.Num() < DesiredLiveBots)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABHBotController* Bot = GetWorld()->SpawnActor<ABHBotController>(ABHBotController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (!Bot)
		{
			break;
		}

		if (!Bot->PlayerState)
		{
			Bot->InitPlayerState();
		}

		ABHPlayerState* BotPS = Bot->GetPlayerState<ABHPlayerState>();
		if (BotPS)
		{
			if (GameState && !GameState->PlayerArray.Contains(BotPS))
			{
				GameState->AddPlayerState(BotPS);
			}
			const int32 BotIndex = BotControllers.Num() + 1;
			BotPS->SetIsABot(true);
			BotPS->SetPlayerName(FString::Printf(TEXT("Bot %02d"), BotIndex));
			BotPS->SetReady(true);
			BotPS->SetDesiredRole(EBHPlayerRole::Unassigned);
			BotPS->SetRole(EBHPlayerRole::Unassigned);
			BotPS->SetLifeState(EBHPlayerLifeState::Alive);
			BotPS->SetHiddenInLocker(false);
			BotPS->SetAvatarIndex(BotIndex % 8);
			BotPS->SetAvatarColor(BotAvatarColorForIndex(BotIndex));
			BotPS->SetMapVote(TEXT(""));
			BotPS->ClearFogPresetVote();
			BotPS->SetFakeHunterEligible(false);
		}

		BotControllers.Add(Bot);
		RestartPlayer(Bot);
	}

	TrimBotRosterToCapacity();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
	}

	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot roster: %d active, target %d, humans %d"),
		BotControllers.Num(),
		TargetBotCount,
		HumanCount);

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Bot roster: %d active, target %d."), BotControllers.Num(), TargetBotCount), 3.0f);
	}
}

bool ABHGameMode::RemoveOneBot()
{
	BotControllers.RemoveAll([](const TObjectPtr<ABHBotController>& Bot)
	{
		return !IsValid(Bot);
	});
	if (BotControllers.IsEmpty())
	{
		return false;
	}

	// Role/phase-aware trim. The old pop-the-newest behaviour could delete the bot Teacher mid-round
	// (a human joining clamps the roster via PostLogin), and the next win-condition tick then resolved
	// an unearned SurvivorsWin. Prefer benched/captured bots, then alive non-hunters; an alive hunter
	// only goes when another alive hunter remains. Practice mode is exempt from the live-round guard:
	// its disable-bots path ("set bot count to 0") must always be able to clear the whole roster.
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	const EBHRoundPhase Phase = BHGS ? BHGS->RoundPhase : EBHRoundPhase::Lobby;
	const bool bRoundLive = !bPracticeMode
		&& (Phase == EBHRoundPhase::Prep || Phase == EBHRoundPhase::Hunt || Phase == EBHRoundPhase::FinalEscape);

	int32 AliveHunterCount = 0;
	if (GameState)
	{
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			const ABHPlayerState* PS = Cast<ABHPlayerState>(RawPS);
			if (PS && PS->IsAliveHunter())
			{
				++AliveHunterCount;
			}
		}
	}

	TArray<BHBotBehavior::FBHBotRemovalCandidate> RemovalCandidates;
	RemovalCandidates.Reserve(BotControllers.Num());
	for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
	{
		const ABHPlayerState* BotPS = Bot ? Bot->GetPlayerState<ABHPlayerState>() : nullptr;
		BHBotBehavior::FBHBotRemovalCandidate Candidate;
		Candidate.bAlive = BotPS && BotPS->LifeState == EBHPlayerLifeState::Alive;
		Candidate.bHunter = BotPS && BotPS->PlayerRole == EBHPlayerRole::Hunter;
		RemovalCandidates.Add(Candidate);
	}

	const int32 RemoveIndex = BHBotBehavior::SelectBotRemovalIndex(RemovalCandidates, bRoundLive, AliveHunterCount);
	if (RemoveIndex == INDEX_NONE)
	{
		// Every removable bot is the round's lone alive hunter; deleting it would instantly resolve
		// SurvivorsWin. Defer the trim — the callers' while-loops stop on false and RefreshBotRoster
		// re-runs the clamp on the next roster change (human join/leave, round transition).
		return false;
	}

	ABHBotController* Bot = BotControllers[RemoveIndex];
	BotControllers.RemoveAt(RemoveIndex);
	if (APawn* Pawn = Bot->GetPawn())
	{
		Pawn->Destroy();
	}
	if (GameState && Bot->PlayerState)
	{
		GameState->RemovePlayerState(Bot->PlayerState);
	}
	Bot->Destroy();
	return true;
}

void ABHGameMode::TrimBotRosterToCapacity()
{
	if (!GameState)
	{
		return;
	}

	while (GameState->PlayerArray.Num() > MaxPlayers)
	{
		if (!RemoveOneBot())
		{
			break;
		}
	}
}

int32 ABHGameMode::CountHumanPlayers() const
{
	int32 Count = 0;
	if (!GameState)
	{
		return Count;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		if (RawPS && !RawPS->IsABot())
		{
			++Count;
		}
	}

	return Count;
}

void ABHGameMode::SweepExpiredBotTacticalState()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	BotWorldStimuli.RemoveAll([Now](const FBHBotStimulus& Stimulus)
	{
		return Now - Stimulus.TimeSeconds > 45.0f;
	});
	BotObjectiveClaims.RemoveAll([Now, this](const FBHBotObjectiveClaim& Claim)
	{
		return !Claim.Claimant.IsValid() || !Claim.Target.IsValid() || Claim.ExpireTimeSeconds <= Now || !IsBotTargetStillUseful(Claim.Target.Get());
	});
	BotTargetCooldowns.RemoveAll([Now](const FBHBotTargetCooldown& Cooldown)
	{
		return !Cooldown.Claimant.IsValid() || !Cooldown.Target.IsValid() || Cooldown.ExpireTimeSeconds <= Now;
	});
}

bool ABHGameMode::IsExclusiveBotClaimIntent(EBHBotIntent Intent) const
{
	return Intent == EBHBotIntent::AnswerStation
		|| Intent == EBHBotIntent::WorkStation
		|| Intent == EBHBotIntent::RepairBreaker
		|| Intent == EBHBotIntent::Hide
		|| Intent == EBHBotIntent::SearchLocker
		|| Intent == EBHBotIntent::DropTrap;
}

bool ABHGameMode::IsBotTargetStillUseful(const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	if (const ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target))
	{
		return Station->IsDirectorActive() && !Station->IsCompleted();
	}
	if (const ABHBreaker* Breaker = Cast<ABHBreaker>(Target))
	{
		return Breaker->IsDirectorActive() && !Breaker->IsRepaired();
	}
	if (const ABHExitGate* Exit = Cast<ABHExitGate>(Target))
	{
		const ABHGameState* BHGS = GetGameState<ABHGameState>();
		return Exit->IsDirectorActive() && BHGS && BHGS->bExitUnlocked;
	}
	if (Cast<ABHLocker>(Target))
	{
		return true;
	}
	if (Cast<ABHCharacter>(Target))
	{
		return true;
	}
	return !Target->IsActorBeingDestroyed();
}
