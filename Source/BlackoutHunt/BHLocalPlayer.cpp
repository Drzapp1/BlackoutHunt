// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHLocalPlayer.h"

#include "BHGameInstance.h"

FString UBHLocalPlayer::GetGameLoginOptions() const
{
	if (const UBHGameInstance* BHGI = Cast<UBHGameInstance>(GetGameInstance()))
	{
		const FString& Token = BHGI->GetClientReconnectToken();
		if (!Token.IsEmpty())
		{
			// The server reads this in ABHGameMode::InitNewPlayer (?BHReconnect=) and keys travel-state
			// restore on it, so the player's role/score/inventory survive every level transition.
			return FString::Printf(TEXT("?BHReconnect=%s"), *Token);
		}
	}
	return FString();
}
