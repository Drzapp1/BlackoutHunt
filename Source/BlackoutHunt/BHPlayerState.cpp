// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHPlayerState.h"
#include "BHCosmeticUnlocks.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Add two non-negative point totals, saturating at INT32_MAX instead of wrapping to a negative
	// value (which the surrounding FMath::Max(0, ...) would then clamp to 0, silently resetting the
	// lifetime counter on a very long-running server).
	int32 BHSaturatingAddPoints(int32 Current, int32 ToAdd)
	{
		const int64 Sum = static_cast<int64>(Current) + static_cast<int64>(ToAdd);
		return static_cast<int32>(FMath::Clamp<int64>(Sum, 0, MAX_int32));
	}
}

ABHPlayerState::ABHPlayerState()
{
	bReady = false;
	PlayerRole = EBHPlayerRole::Unassigned;
	DesiredRole = EBHPlayerRole::Unassigned;
	LifeState = EBHPlayerLifeState::Alive;
	bHiddenInLocker = false;
	AvatarIndex = 0;
	AvatarColor = FLinearColor(0.22f, 0.58f, 0.74f, 1.0f);
	AvatarHeadwearIndex = 0;
	AvatarGearIndex = 0;
	MapVote = TEXT("");
	bHasFogPresetVote = false;
	FogPresetVote = EBHFogPreset::Heavy;
	bFakeHunterEligible = false;
	SpectatorRolePreference = EBHPlayerRole::Unassigned;
	SpectatorEncouragementCount = 0;
	RevisionStats = FBHPlayerRevisionStats();
	RevisionReviewQueue.Reset();
	WarmupChecklistMask = 0;
	bWarmupComplete = false;
	QuestionPoints = 0;
	LifetimeQuestionPoints = 0;
	HunterPoints = 0;
	LifetimeHunterPoints = 0;
}

void ABHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABHPlayerState, bReady);
	DOREPLIFETIME(ABHPlayerState, PlayerRole);
	DOREPLIFETIME(ABHPlayerState, DesiredRole);
	DOREPLIFETIME(ABHPlayerState, LifeState);
	DOREPLIFETIME(ABHPlayerState, bHiddenInLocker);
	DOREPLIFETIME(ABHPlayerState, AvatarIndex);
	DOREPLIFETIME(ABHPlayerState, AvatarColor);
	DOREPLIFETIME(ABHPlayerState, AvatarHeadwearIndex);
	DOREPLIFETIME(ABHPlayerState, AvatarGearIndex);
	DOREPLIFETIME(ABHPlayerState, AvatarSlotColors);
	DOREPLIFETIME(ABHPlayerState, SelectedTitleIndex);
	DOREPLIFETIME(ABHPlayerState, SelectedEmblemIndex);
	DOREPLIFETIME(ABHPlayerState, MapVote);
	DOREPLIFETIME(ABHPlayerState, bHasFogPresetVote);
	DOREPLIFETIME(ABHPlayerState, FogPresetVote);
	DOREPLIFETIME(ABHPlayerState, bFakeHunterEligible);
	DOREPLIFETIME(ABHPlayerState, SpectatorRolePreference);
	DOREPLIFETIME(ABHPlayerState, SpectatorEncouragementCount);
	// Owner-only: a student's academic record (mastery %, attempt/correct/hint counts, and the exact
	// IDs of questions they got wrong) is private. The owning client's HUD is the only client-side
	// reader; the host reads it server-side for the classroom board, so it never needs to reach other
	// students' clients. Replicating it to everyone let a modded/curious student read classmates'
	// performance straight off the wire. Mirror the warmup fields below.
	DOREPLIFETIME_CONDITION(ABHPlayerState, RevisionStats, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABHPlayerState, RevisionReviewQueue, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABHPlayerState, WarmupChecklistMask, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABHPlayerState, bWarmupComplete, COND_OwnerOnly);
	DOREPLIFETIME(ABHPlayerState, QuestionPoints);
	DOREPLIFETIME(ABHPlayerState, LifetimeQuestionPoints);
	DOREPLIFETIME(ABHPlayerState, HunterPoints);
	DOREPLIFETIME(ABHPlayerState, LifetimeHunterPoints);
	DOREPLIFETIME(ABHPlayerState, Powerups);
	// Owner-only: only the player's own HUD needs to know which minigame table to draw status for.
	DOREPLIFETIME_CONDITION(ABHPlayerState, ActiveMinigameTable, COND_OwnerOnly);
}

bool ABHPlayerState::IsAliveSurvivor() const
{
	return (PlayerRole == EBHPlayerRole::Survivor || PlayerRole == EBHPlayerRole::Tester) && LifeState == EBHPlayerLifeState::Alive;
}

bool ABHPlayerState::IsAliveHunter() const
{
	return (PlayerRole == EBHPlayerRole::Hunter || PlayerRole == EBHPlayerRole::Tester) && LifeState == EBHPlayerLifeState::Alive;
}

void ABHPlayerState::SetReady(bool bNewReady)
{
	bReady = bNewReady;
}

void ABHPlayerState::SetRole(EBHPlayerRole NewRole)
{
	PlayerRole = NewRole;
}

void ABHPlayerState::SetDesiredRole(EBHPlayerRole NewRole)
{
	DesiredRole = NewRole;
}

void ABHPlayerState::SetLifeState(EBHPlayerLifeState NewLifeState)
{
	LifeState = NewLifeState;
}

void ABHPlayerState::SetHiddenInLocker(bool bNewHidden)
{
	bHiddenInLocker = bNewHidden;
}

void ABHPlayerState::SetAvatarIndex(int32 NewAvatarIndex)
{
	AvatarIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Outfit, NewAvatarIndex);
}

void ABHPlayerState::SetAvatarColor(const FLinearColor& NewAvatarColor)
{
	AvatarColor = NewAvatarColor;
}

void ABHPlayerState::SetAvatarHeadwearIndex(int32 NewHeadwearIndex)
{
	AvatarHeadwearIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Headwear, NewHeadwearIndex);
}

void ABHPlayerState::SetAvatarGearIndex(int32 NewGearIndex)
{
	(void)NewGearIndex;
	AvatarGearIndex = 0;
}

void ABHPlayerState::SetAvatarSlotColors(const TArray<uint8>& NewSlotColors)
{
	// Normalise to the fixed registry size and to valid values (0 = authored default; 1..N = palette colour
	// index+1, where N = ShirtColor count, matching the entries of BHAvatarPaletteColor). Else collapses to authored.
	const int32 Count = BHColorableMaterialCount();
	const uint8 MaxStored = static_cast<uint8>(BHCosmeticMaxIndex(EBHCosmeticCategory::ShirtColor) + 1);
	AvatarSlotColors.SetNumZeroed(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const uint8 Value = NewSlotColors.IsValidIndex(Index) ? NewSlotColors[Index] : 0;
		AvatarSlotColors[Index] = (Value >= 1 && Value <= MaxStored) ? Value : 0;
	}
}

void ABHPlayerState::SetSelectedTitleIndex(int32 NewTitleIndex)
{
	SelectedTitleIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Title, NewTitleIndex);
}

void ABHPlayerState::SetSelectedEmblemIndex(int32 NewEmblemIndex)
{
	SelectedEmblemIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Emblem, NewEmblemIndex);
}

void ABHPlayerState::SetMapVote(const FString& NewMapVote)
{
	MapVote = NewMapVote;
}

void ABHPlayerState::SetFogPresetVote(EBHFogPreset NewFogPresetVote)
{
	FogPresetVote = NewFogPresetVote;
	bHasFogPresetVote = true;
}

void ABHPlayerState::ClearFogPresetVote()
{
	bHasFogPresetVote = false;
	FogPresetVote = EBHFogPreset::Heavy;
}

void ABHPlayerState::SetFakeHunterEligible(bool bNewEligible)
{
	bFakeHunterEligible = bNewEligible;
}

void ABHPlayerState::SetSpectatorRolePreference(EBHPlayerRole NewRole)
{
	if (NewRole != EBHPlayerRole::Hunter && NewRole != EBHPlayerRole::Survivor && NewRole != EBHPlayerRole::FakeHunter)
	{
		NewRole = EBHPlayerRole::Unassigned;
	}

	SpectatorRolePreference = NewRole;
}

void ABHPlayerState::ClearSpectatorSupportState(bool bClearRolePreference)
{
	if (bClearRolePreference)
	{
		SpectatorRolePreference = EBHPlayerRole::Unassigned;
	}
	SpectatorEncouragementCount = 0;
}

void ABHPlayerState::AddSpectatorEncouragement()
{
	SpectatorEncouragementCount = FMath::Max(0, SpectatorEncouragementCount + 1);
}

void ABHPlayerState::ResetRevisionStats()
{
	RevisionStats = FBHPlayerRevisionStats();
	RevisionReviewQueue.Reset();
}

void ABHPlayerState::ResetRevisionRoundContribution()
{
	// Per-round gate only: zero the contribution tally but keep every mastery field (and the review queue
	// untouched), so a student's durable mastery accumulates across the session's stages.
	RevisionStats.ContributionCount = 0;
}

void ABHPlayerState::MarkWarmupStep(EBHWarmupStep Step)
{
	// Server-authoritative, and only meaningful during the live role warmup (Prep, not the
	// practice/test sandboxes). Mirrors BHIsRoleWarmupPhase() without depending on that
	// translation-unit-local helper. A no-op everywhere else, so callers can fire it freely.
	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!HasAuthority() || !BHGS
		|| BHGS->RoundPhase != EBHRoundPhase::Prep
		|| BHGS->bPracticeMode
		|| BHGS->bTestMode)
	{
		return;
	}

	const uint8 Bit = static_cast<uint8>(Step);
	if ((WarmupChecklistMask & Bit) != 0)
	{
		return; // already tried this action
	}
	WarmupChecklistMask |= Bit;

	const uint8 Expected = BHWarmupExpectedMask(PlayerRole);
	if (!bWarmupComplete && Expected != 0 && (WarmupChecklistMask & Expected) == Expected)
	{
		bWarmupComplete = true;
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(GetOwningController()))
		{
			PC->ClientShowStatusMessage(TEXT("Warmup complete - you're ready! Wait for the class to start."), 3.5f);
		}
	}
}

void ABHPlayerState::ResetWarmupChecklist()
{
	WarmupChecklistMask = 0;
	bWarmupComplete = false;
}

namespace
{
	// Cap the review backlog so a struggling player gets re-tested on recent
	// misses without the queue growing without bound across a long session.
	constexpr int32 BHMaxRevisionReviewQueue = 8;
}

void ABHPlayerState::EnqueueRevisionReview(const FString& QuestionId)
{
	if (QuestionId.IsEmpty())
	{
		return;
	}

	// Dedup: if it is already queued, move it to the back (most recently missed).
	RevisionReviewQueue.RemoveAll([&QuestionId](const FString& Existing)
	{
		return Existing.Equals(QuestionId, ESearchCase::IgnoreCase);
	});
	RevisionReviewQueue.Add(QuestionId);

	while (RevisionReviewQueue.Num() > BHMaxRevisionReviewQueue)
	{
		RevisionReviewQueue.RemoveAt(0);
	}
}

bool ABHPlayerState::DequeueRevisionReview(const FString& QuestionId)
{
	if (QuestionId.IsEmpty())
	{
		return false;
	}

	const int32 Removed = RevisionReviewQueue.RemoveAll([&QuestionId](const FString& Existing)
	{
		return Existing.Equals(QuestionId, ESearchCase::IgnoreCase);
	});
	return Removed > 0;
}

FString ABHPlayerState::PeekRevisionReview() const
{
	return RevisionReviewQueue.Num() > 0 ? RevisionReviewQueue[0] : FString();
}

void ABHPlayerState::AddQuestionPoints(int32 Points)
{
	const int32 ClampedPoints = FMath::Max(0, Points);
	QuestionPoints = BHSaturatingAddPoints(QuestionPoints, ClampedPoints);
	LifetimeQuestionPoints = BHSaturatingAddPoints(LifetimeQuestionPoints, ClampedPoints);
}

bool ABHPlayerState::SpendQuestionPoints(int32 Points)
{
	const int32 ClampedPoints = FMath::Max(0, Points);
	if (QuestionPoints < ClampedPoints)
	{
		return false;
	}

	QuestionPoints -= ClampedPoints;
	return true;
}

int32 ABHPlayerState::ApplyCaughtQuestionPointPenalty(float PenaltyFraction)
{
	const float ClampedFraction = FMath::Clamp(PenaltyFraction, 0.0f, 1.0f);
	const int32 Penalty = FMath::Min(QuestionPoints, FMath::CeilToInt(static_cast<float>(QuestionPoints) * ClampedFraction));
	QuestionPoints = FMath::Max(0, QuestionPoints - Penalty);
	return Penalty;
}

void ABHPlayerState::AddHunterPoints(int32 Points)
{
	const int32 ClampedPoints = FMath::Max(0, Points);
	HunterPoints = BHSaturatingAddPoints(HunterPoints, ClampedPoints);
	LifetimeHunterPoints = BHSaturatingAddPoints(LifetimeHunterPoints, ClampedPoints);
}

bool ABHPlayerState::SpendHunterPoints(int32 Points)
{
	const int32 ClampedPoints = FMath::Max(0, Points);
	if (HunterPoints < ClampedPoints)
	{
		return false;
	}

	HunterPoints -= ClampedPoints;
	return true;
}

int32 ABHPlayerState::GetPowerupCharges(EBHPowerupType Type) const
{
	for (const FBHPowerupInventoryEntry& Entry : Powerups)
	{
		if (Entry.Type == Type)
		{
			return Entry.Charges;
		}
	}
	return 0;
}

bool ABHPlayerState::AddPowerupCharge(EBHPowerupType Type, int32 MaxCharges)
{
	const int32 ClampedMaxCharges = FMath::Max(1, MaxCharges);
	for (FBHPowerupInventoryEntry& Entry : Powerups)
	{
		if (Entry.Type == Type)
		{
			if (Entry.Charges >= ClampedMaxCharges)
			{
				return false;
			}
			Entry.Charges = FMath::Clamp(Entry.Charges + 1, 0, ClampedMaxCharges);
			return true;
		}
	}

	FBHPowerupInventoryEntry NewEntry;
	NewEntry.Type = Type;
	NewEntry.Charges = 1;
	Powerups.Add(NewEntry);
	return true;
}

bool ABHPlayerState::ConsumePowerupCharge(EBHPowerupType Type)
{
	for (FBHPowerupInventoryEntry& Entry : Powerups)
	{
		if (Entry.Type == Type)
		{
			if (Entry.Charges <= 0)
			{
				return false;
			}
			--Entry.Charges;
			return true;
		}
	}
	return false;
}

void ABHPlayerState::SetPowerupCooldown(EBHPowerupType Type, float CooldownEndServerTime)
{
	for (FBHPowerupInventoryEntry& Entry : Powerups)
	{
		if (Entry.Type == Type)
		{
			Entry.CooldownEndServerTime = FMath::Max(0.0f, CooldownEndServerTime);
			return;
		}
	}

	FBHPowerupInventoryEntry NewEntry;
	NewEntry.Type = Type;
	NewEntry.Charges = 0;
	NewEntry.CooldownEndServerTime = FMath::Max(0.0f, CooldownEndServerTime);
	Powerups.Add(NewEntry);
}

void ABHPlayerState::SetActiveMinigameTable(AActor* Table)
{
	// Server-authoritative; replicates to the owning client whose HUD draws the minigame status overlay.
	if (HasAuthority())
	{
		ActiveMinigameTable = Table;
	}
}

float ABHPlayerState::GetPowerupCooldownEnd(EBHPowerupType Type) const
{
	for (const FBHPowerupInventoryEntry& Entry : Powerups)
	{
		if (Entry.Type == Type)
		{
			return Entry.CooldownEndServerTime;
		}
	}
	return 0.0f;
}
