// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHGameMode.h"

#include "BHCharacter.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

namespace
{
FString NormalizeHostControlLevelName(FString LevelName)
{
	LevelName.TrimStartAndEndInline();
	if (LevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || LevelName.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
	{
		return TEXT("Foggrounds");
	}
	return LevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? TEXT("Substation") : TEXT("Facility");
}

FString HostControlFogPresetToString(EBHFogPreset Preset)
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

FString HostControlSpectatorRolePreferenceToString(EBHPlayerRole Role)
{
	switch (ABHGameMode::SanitizeSpectatorRolePreference(Role))
	{
	case EBHPlayerRole::Hunter:
		return TEXT("Teacher");
	case EBHPlayerRole::Survivor:
		return TEXT("Survivor");
	case EBHPlayerRole::FakeHunter:
		return TEXT("Hall Monitor");
	case EBHPlayerRole::Unassigned:
	default:
		return TEXT("No preference");
	}
}

const TCHAR* SelectHostControlPromptLine(const TCHAR* const* Lines, int32 LineCount, int32 Salt)
{
	if (!Lines || LineCount <= 0)
	{
		return TEXT("");
	}

	// Unsigned modulo: FMath::Abs(INT32_MIN) is UB and stays negative, which would index out of bounds.
	return Lines[static_cast<int32>(static_cast<uint32>(Salt) % static_cast<uint32>(LineCount))];
}
}

bool ABHGameMode::IsHostAdminController(const ABHPlayerController* RequestingController) const
{
	const UWorld* World = GetWorld();
	if (!RequestingController || !World)
	{
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	if (RequestingController->IsLocalController()
		&& (NetMode == NM_ListenServer || NetMode == NM_Standalone))
	{
		return true;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings && !Settings->bClassroomMode && Settings->bAllowStudentTeacherAdminControls)
	{
		const ABHPlayerState* BHPS = RequestingController->GetPlayerState<ABHPlayerState>();
		return BHPS && BHPS->IsAliveHunter();
	}

	return false;
}

bool ABHGameMode::RequireHostAdmin(ABHPlayerController* RequestingController, const TCHAR* ActionDescription) const
{
	if (IsHostAdminController(RequestingController))
	{
		return true;
	}

	const FString Action = ActionDescription && FCString::Strlen(ActionDescription) > 0
		? FString(ActionDescription)
		: FString(TEXT("use this classroom control"));
	UE_LOG(LogTemp, Warning, TEXT("Denied host-admin action '%s' from %s."),
		*Action,
		*GetNameSafe(RequestingController));

	// Throttle the reliable denial reply per controller. Without this a misbehaving client can spam
	// host-only Server RPCs and force us to flood it (and the reliable channel) with status messages.
	// The deny itself is always logged/enforced above; only the client-facing reply is rate-limited.
	if (RequestingController)
	{
		constexpr float DenialReplyCooldownSeconds = 2.5f;
		const UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.0f;
		const TObjectKey<APlayerController> ControllerKey(RequestingController);
		const float* LastReplyTime = HostAdminDenialReplyTimes.Find(ControllerKey);
		if (!LastReplyTime || (Now - *LastReplyTime) >= DenialReplyCooldownSeconds)
		{
			RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Only the host machine can %s."), *Action), 3.0f);
			HostAdminDenialReplyTimes.Add(ControllerKey, Now);
		}
	}

	return false;
}

void ABHGameMode::ForceStartRound(ABHPlayerController* RequestingController)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS)
	{
		return;
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Prep)
	{
		if (!RequireHostAdmin(RequestingController, TEXT("end the role warmup")))
		{
			return;
		}

		StartHuntPhase();
		return;
	}

	if (BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Force start is only available in the lobby or role warmup."), 3.0f);
		}
		return;
	}

	// In the TRAIN LOBBY the round can't start "in place" -- it departs to the first hunt level. Honour the same
	// host-admin + quorum guard as the normal lobby, then travel (the warmup happens on arrival).
	if (bLobbyLevel)
	{
		if (!RequireHostAdmin(RequestingController, TEXT("depart the lobby for the first stop")))
		{
			return;
		}

		if (!bAllowHostForceStart)
		{
			int32 ReadyCount = 0;
			if (GameState)
			{
				for (APlayerState* RawPS : GameState->PlayerArray)
				{
					const ABHPlayerState* ReadyPS = Cast<ABHPlayerState>(RawPS);
					if (ReadyPS && ReadyPS->bReady)
					{
						++ReadyCount;
					}
				}
			}
			if (ReadyCount < MinPlayers)
			{
				if (RequestingController)
				{
					RequestingController->ClientShowStatusMessage(
						FString::Printf(TEXT("Can't depart yet: need at least %d ready players (currently %d). Ready up more students, or enable force-start in settings."), MinPlayers, ReadyCount),
						3.5f);
				}
				return;
			}
		}

		BroadcastStatus(TEXT("Host departed the lobby. Loading the first stop..."), 3.5f);
		TravelFromLobbyToFirstHunt();
		return;
	}

	if (!bAllowHostForceStart)
	{
		// bAllowHostForceStart gates the UNCONDITIONAL brute force-start (launch even with nobody ready),
		// which ships off by default so a stray click can't start an empty/unprepared lobby. But a single
		// AFK or never-ready student must never be able to block the whole class: AreAllReady() requires
		// EVERY connected player to be ready, so one straggler stalls all 32 with no escape hatch. Even with
		// the brute flag off, allow a host-initiated start once a healthy quorum (>= MinPlayers) has readied
		// up. This goes through the normal StartPrepPhase() warmup (not the brute immediate hunt), and any
		// not-ready stragglers simply come along in their current lobby state.
		if (!RequireHostAdmin(RequestingController, TEXT("start the round with the ready players")))
		{
			return;
		}

		int32 ReadyCount = 0;
		if (GameState)
		{
			for (APlayerState* RawPS : GameState->PlayerArray)
			{
				const ABHPlayerState* ReadyPS = Cast<ABHPlayerState>(RawPS);
				if (ReadyPS && ReadyPS->bReady)
				{
					++ReadyCount;
				}
			}
		}

		if (ReadyCount < MinPlayers)
		{
			if (RequestingController)
			{
				RequestingController->ClientShowStatusMessage(
					FString::Printf(TEXT("Can't start yet: need at least %d ready players (currently %d). Ready up more students, or enable force-start in settings."), MinPlayers, ReadyCount),
					3.5f);
			}
			return;
		}

		BroadcastStatus(FString::Printf(TEXT("Host started the round with %d ready player(s)."), ReadyCount), 3.5f);
		StartPrepPhase();
		return;
	}

	if (!RequireHostAdmin(RequestingController, TEXT("force-start the round")))
	{
		return;
	}

	StartHuntPhaseImmediately();
}

void ABHGameMode::SetDesiredRole(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState, EBHPlayerRole DesiredRole)
{
	if (!RequireHostAdmin(RequestingController, TEXT("assign roles")))
	{
		return;
	}

	if (bPracticeMode && RequestingController && RequestingController->PlayerState == TargetPlayerState)
	{
		SetPracticeRole(RequestingController, DesiredRole);
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Roles can only be assigned in the lobby."), 3.0f);
		}
		return;
	}

	ABHPlayerState* TargetBHPS = Cast<ABHPlayerState>(TargetPlayerState);
	if (!TargetBHPS || !GameState || !GameState->PlayerArray.Contains(TargetBHPS))
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Could not find that player for role assignment."), 3.0f);
		}
		return;
	}

	if (DesiredRole != EBHPlayerRole::Hunter && DesiredRole != EBHPlayerRole::Survivor && DesiredRole != EBHPlayerRole::FakeHunter)
	{
		DesiredRole = EBHPlayerRole::Unassigned;
	}

	TargetBHPS->SetDesiredRole(DesiredRole);
	TargetBHPS->SetSpectatorRolePreference(EBHPlayerRole::Unassigned);

	if (RequestingController)
	{
		const UEnum* RoleEnum = StaticEnum<EBHPlayerRole>();
		FString RoleName = RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(DesiredRole)) : FString(TEXT("Unassigned"));
		if (DesiredRole == EBHPlayerRole::Hunter)
		{
			RoleName = TEXT("Teacher");
		}
		else if (DesiredRole == EBHPlayerRole::FakeHunter)
		{
			RoleName = TEXT("Hall Monitor");
		}
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("%s queued as %s."),
			*TargetBHPS->GetPlayerName(),
			*RoleName), 2.5f);
	}
}

void ABHGameMode::QueueSpectatorRolePreference(ABHPlayerController* RequestingController, EBHPlayerRole DesiredRole)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!RequestingController || !BHGS || !BHPS)
	{
		return;
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Lobby)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Role requests are for late spectators during active rounds. Use the lobby roster now."), 3.0f);
		return;
	}

	if (BHPS->PlayerRole != EBHPlayerRole::Spectator)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Only late-join spectators can request a next-round role."), 3.0f);
		return;
	}

	const EBHPlayerRole SanitizedRole = SanitizeSpectatorRolePreference(DesiredRole);
	BHPS->SetSpectatorRolePreference(SanitizedRole);

	const FString RoleText = HostControlSpectatorRolePreferenceToString(SanitizedRole);
	if (SanitizedRole == EBHPlayerRole::Unassigned)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Next-round role preference cleared."), 3.0f);
		return;
	}

	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Next-round role preference: %s. The host must approve it in the lobby roster."), *RoleText), 4.0f);

	if (UWorld* World = GetWorld())
	{
		const FString PlayerName = BHPS->GetPlayerName().IsEmpty() ? FString(TEXT("Spectator")) : BHPS->GetPlayerName();
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ABHPlayerController* HostPC = Cast<ABHPlayerController>(It->Get());
			if (HostPC && HostPC != RequestingController && IsHostAdminController(HostPC))
			{
				HostPC->ClientShowStatusMessage(FString::Printf(TEXT("%s requested %s next round. Approve from the lobby roster."), *PlayerName, *RoleText), 5.0f);
			}
		}
	}
}

void ABHGameMode::RequestSpectatorEncouragement(ABHPlayerController* RequestingController)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	UWorld* World = GetWorld();
	if (!RequestingController || !BHGS || !BHPS || !World)
	{
		return;
	}

	if (BHGS->RoundPhase != EBHRoundPhase::Hunt && BHGS->RoundPhase != EBHRoundPhase::FinalEscape)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Spectator support opens during the Hunt and final escape."), 3.0f);
		return;
	}

	if (BHPS->PlayerRole != EBHPlayerRole::Spectator)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Only late-join spectators can send spectator support."), 3.0f);
		return;
	}

	const float Now = World->GetTimeSeconds();
	constexpr float PerSpectatorCooldownSeconds = 45.0f;
	constexpr float GlobalCooldownSeconds = 12.0f;
	const TObjectKey<APlayerState> PlayerKey(BHPS);
	if (const float* LastPersonalTime = SpectatorEncouragementTimes.Find(PlayerKey))
	{
		const float Remaining = PerSpectatorCooldownSeconds - (Now - *LastPersonalTime);
		if (Remaining > 0.0f)
		{
			RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Spectator support cooling down: %.0fs."), Remaining), 2.5f);
			return;
		}
	}

	const float GlobalRemaining = GlobalCooldownSeconds - (Now - LastSpectatorEncouragementTime);
	if (GlobalRemaining > 0.0f)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Class support channel cooling down: %.0fs."), GlobalRemaining), 2.5f);
		return;
	}

	static const TCHAR* HuntEncouragementLines[] = {
		TEXT("Spectator support: stay calm, finish the current task, and keep moving."),
		TEXT("Spectator support: check your objective, then regroup only when it is safe."),
		TEXT("Spectator support: use cover, manage battery, and finish the classroom work."),
		TEXT("Spectator support: one task at a time. The exit opens when the team keeps contributing.")
	};
	static const TCHAR* FinalEscapeEncouragementLines[] = {
		TEXT("Spectator support: keep moving for the evacuation train."),
		TEXT("Spectator support: use cover and commit to the final route."),
		TEXT("Spectator support: stay together when safe and board before departure.")
	};

	const int32 Salt = BHPS->GetPlayerId() + BHPS->SpectatorEncouragementCount + RoundSeed;
	const FString Message = BHGS->RoundPhase == EBHRoundPhase::FinalEscape
		? SelectHostControlPromptLine(FinalEscapeEncouragementLines, UE_ARRAY_COUNT(FinalEscapeEncouragementLines), Salt)
		: SelectHostControlPromptLine(HuntEncouragementLines, UE_ARRAY_COUNT(HuntEncouragementLines), Salt);
	SpectatorEncouragementTimes.Add(PlayerKey, Now);
	LastSpectatorEncouragementTime = Now;
	BHPS->AddSpectatorEncouragement();
	BroadcastStatus(Message, 3.5f);
	RequestingController->ClientShowStatusMessage(TEXT("Team encouragement sent. No locations or answers were shared."), 3.0f);
}

void ABHGameMode::KickPlayer(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState)
{
	if (!RequireHostAdmin(RequestingController, TEXT("kick players")))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_ListenServer)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Player kick is only available to the listen-server host."), 3.0f);
		}
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Players can only be kicked from the lobby."), 3.0f);
		}
		return;
	}

	ABHPlayerState* TargetBHPS = Cast<ABHPlayerState>(TargetPlayerState);
	if (!TargetBHPS || !GameState || !GameState->PlayerArray.Contains(TargetBHPS) || TargetBHPS->IsABot())
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Could not find a connected student to kick."), 3.0f);
		}
		return;
	}

	if (RequestingController && RequestingController->PlayerState == TargetBHPS)
	{
		RequestingController->ClientShowStatusMessage(TEXT("The host cannot kick themselves."), 3.0f);
		return;
	}

	ABHPlayerController* TargetController = nullptr;
	if (World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ABHPlayerController* CandidateController = Cast<ABHPlayerController>(It->Get());
			if (CandidateController && CandidateController->PlayerState == TargetBHPS)
			{
				TargetController = CandidateController;
				break;
			}
		}
	}

	if (!TargetController || TargetController->IsLocalController())
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("The host/local player cannot be kicked."), 3.0f);
		}
		return;
	}

	const FString TargetName = TargetBHPS->GetPlayerName().IsEmpty() ? FString(TEXT("Player")) : TargetBHPS->GetPlayerName();
	TargetBHPS->SetReady(false);
	TargetController->ClientShowStatusMessage(TEXT("You were removed from this classroom lobby by the host."), 6.0f);
	TargetController->ClientTravel(TEXT("/Engine/Maps/Entry?BHRemovedByHost=1"), TRAVEL_Absolute);

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Removed %s from the lobby."), *TargetName), 3.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("Classroom soft kick: host %s removed %s from lobby."),
		*GetNameSafe(RequestingController),
		*TargetName);
}

void ABHGameMode::SetNextLevel(ABHPlayerController* RequestingController, const FString& LevelName)
{
	if (!RequireHostAdmin(RequestingController, TEXT("set the next level")))
	{
		return;
	}

	NextRuntimeLevelName = NormalizeHostControlLevelName(LevelName);

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	UpdateDirectorGameState(BHGS ? BHGS->ObjectiveText : FString(TEXT("Next level updated.")));

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Next level set to %s."), *NextRuntimeLevelName), 3.0f);
	}
}

void ABHGameMode::SetMapVote(ABHPlayerController* RequestingController, const FString& LevelName)
{
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	const FString Vote = NormalizeHostControlLevelName(LevelName);
	BHPS->SetMapVote(Vote);
	RefreshNextLevelFromVotes();
	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Map vote set to %s."), *Vote), 2.75f);
}

void ABHGameMode::SetFogPresetOverride(ABHPlayerController* RequestingController, EBHFogPreset FogPreset)
{
	if (!RequireHostAdmin(RequestingController, TEXT("override the next fog preset")))
	{
		return;
	}

	NextFogPreset = FogPreset;
	bFogPresetOverride = true;
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	UpdateDirectorGameState(BHGS ? BHGS->ObjectiveText : FString(TEXT("Fog preset updated.")));

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Next fog preset overridden to %s."), *HostControlFogPresetToString(NextFogPreset)), 3.0f);
	}
}

void ABHGameMode::ClearFogPresetOverride(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("return fog preset control to votes")))
	{
		return;
	}

	bFogPresetOverride = false;
	NextFogPreset = RuntimeFogPreset;
	RefreshNextLevelFromVotes();

	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Fog preset override cleared; lobby votes now choose next fog."), 3.0f);
	}
}

void ABHGameMode::SetFogPresetVote(ABHPlayerController* RequestingController, EBHFogPreset FogPreset)
{
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	BHPS->SetFogPresetVote(FogPreset);
	RefreshNextLevelFromVotes();
	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Fog vote set to %s."), *HostControlFogPresetToString(FogPreset)), 2.75f);
}

void ABHGameMode::SetPlayerAvatar(ABHPlayerController* RequestingController, int32 AvatarIndex)
{
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	const int32 NormalizedIndex = FMath::Clamp(AvatarIndex, 0, 7);
	BHPS->SetAvatarIndex(NormalizedIndex);

	if (ABHCharacter* Character = Cast<ABHCharacter>(RequestingController->GetPawn()))
	{
		Character->ApplyAvatarStyle();
	}

	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Avatar set to %d."), NormalizedIndex + 1), 2.5f);
}

void ABHGameMode::SetTargetHunterCount(ABHPlayerController* RequestingController, int32 NewHunterCount)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change Teacher count")))
	{
		return;
	}

	TargetHunterCount = FMath::Clamp(NewHunterCount, 1, 3);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, BHGS->RoundModifier);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Teacher count set to %d."), TargetHunterCount), 3.0f);
	}
}

void ABHGameMode::SetObjectiveIntensity(ABHPlayerController* RequestingController, int32 NewObjectiveIntensity)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change objective intensity")))
	{
		return;
	}

	ObjectiveIntensity = FMath::Clamp(NewObjectiveIntensity, 0, 3);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, BHGS->RoundModifier);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Objective intensity set to %d."), ObjectiveIntensity), 3.0f);
	}

	if (bTestMode)
	{
		RefreshTestDirector(TEXT("Test objectives refreshed."));
	}
	else if (bPracticeMode)
	{
		RefreshPracticeDirector(TEXT("Practice objectives refreshed."));
	}
}

void ABHGameMode::ToggleInfectionMode(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("toggle infection mode")))
	{
		return;
	}

	bInfectionMode = !bInfectionMode;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, BHGS->RoundModifier);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(bInfectionMode ? TEXT("Infection mode enabled.") : TEXT("Infection mode disabled."), 3.0f);
	}
}

void ABHGameMode::TogglePaceMode(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change round pacing")))
	{
		return;
	}

	bPartyPace = !bPartyPace;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, BHGS->RoundModifier);
	}
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(bPartyPace ? TEXT("Pace set to party chaos.") : TEXT("Pace set to slow horror."), 3.0f);
	}

	if (bTestMode)
	{
		RefreshTestDirector(TEXT("Test pacing refreshed."));
	}
	else if (bPracticeMode)
	{
		RefreshPracticeDirector(TEXT("Practice pacing refreshed."));
	}
}
