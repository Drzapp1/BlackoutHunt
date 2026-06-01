// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "BHLocalPlayer.generated.h"

// Custom local player that injects the server-issued reconnect token into the login URL via
// GetGameLoginOptions. The engine calls that hook both when a remote client reconnects after a
// non-seamless ServerTravel (UPendingNetGame) and when the listen-server host re-spawns its local
// player across travel (ULocalPlayer::SpawnPlayActor). Without it, a level hop drops the token and
// the server can only fall back to display-name matching for travel-state restore, which scrambles
// roles/scores when two students share a name. Wired via [/Script/Engine.Engine] LocalPlayerClassName.
UCLASS()
class BLACKOUTHUNT_API UBHLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	virtual FString GetGameLoginOptions() const override;
};
