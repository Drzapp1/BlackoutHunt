#include "BHPlayerState.h"
#include "Net/UnrealNetwork.h"

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
	RevisionStats = FBHPlayerRevisionStats();
	QuestionPoints = 0;
	LifetimeQuestionPoints = 0;
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
	DOREPLIFETIME(ABHPlayerState, RevisionStats);
	DOREPLIFETIME(ABHPlayerState, QuestionPoints);
	DOREPLIFETIME(ABHPlayerState, LifetimeQuestionPoints);
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
	AvatarIndex = FMath::Max(0, NewAvatarIndex);
}

void ABHPlayerState::SetAvatarColor(const FLinearColor& NewAvatarColor)
{
	AvatarColor = NewAvatarColor;
}

void ABHPlayerState::SetAvatarHeadwearIndex(int32 NewHeadwearIndex)
{
	AvatarHeadwearIndex = 0;
}

void ABHPlayerState::SetAvatarGearIndex(int32 NewGearIndex)
{
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

void ABHPlayerState::ResetRevisionStats()
{
	RevisionStats = FBHPlayerRevisionStats();
}

void ABHPlayerState::AddQuestionPoints(int32 Points)
{
	const int32 ClampedPoints = FMath::Max(0, Points);
	QuestionPoints = FMath::Max(0, QuestionPoints + ClampedPoints);
	LifetimeQuestionPoints = FMath::Max(0, LifetimeQuestionPoints + ClampedPoints);
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
