#include "BHPlayerState.h"
#include "BHCosmeticUnlocks.h"
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
	DOREPLIFETIME(ABHPlayerState, MapVote);
	DOREPLIFETIME(ABHPlayerState, bHasFogPresetVote);
	DOREPLIFETIME(ABHPlayerState, FogPresetVote);
	DOREPLIFETIME(ABHPlayerState, bFakeHunterEligible);
	DOREPLIFETIME(ABHPlayerState, SpectatorRolePreference);
	DOREPLIFETIME(ABHPlayerState, SpectatorEncouragementCount);
	DOREPLIFETIME(ABHPlayerState, RevisionStats);
	DOREPLIFETIME(ABHPlayerState, RevisionReviewQueue);
	DOREPLIFETIME(ABHPlayerState, QuestionPoints);
	DOREPLIFETIME(ABHPlayerState, LifetimeQuestionPoints);
	DOREPLIFETIME(ABHPlayerState, HunterPoints);
	DOREPLIFETIME(ABHPlayerState, LifetimeHunterPoints);
	DOREPLIFETIME(ABHPlayerState, Powerups);
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
