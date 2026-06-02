// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

struct FBHAutomationConfig
{
	bool bEnabled = false;
	bool bAutoReady = false;
	bool bVirtualBoxSafeRequested = false;
	bool bVirtualBoxSafeDetected = false;
	int32 AutoMinPlayers = 0;
	float AutoQuitSeconds = 0.0f;
	FString AutoHost;
	FString AutoJoin;
	FString AutoAtmosphereTests;
	FString Tag;

	bool HasAutoHost() const
	{
		return bEnabled && !AutoHost.IsEmpty();
	}

	bool HasAutoJoin() const
	{
		return bEnabled && !AutoJoin.IsEmpty();
	}

	bool ShouldAutoReady() const
	{
		return bEnabled && bAutoReady;
	}

	int32 GetAutoMinPlayers() const
	{
		return bEnabled ? AutoMinPlayers : 0;
	}

	bool ShouldAutoQuit() const
	{
		return bEnabled && AutoQuitSeconds > 0.0f;
	}

	bool HasAutoAtmosphereTests() const
	{
		return bEnabled && !AutoAtmosphereTests.IsEmpty();
	}

	bool ShouldUseVirtualBoxSafeMode() const
	{
		return bVirtualBoxSafeRequested || bVirtualBoxSafeDetected;
	}
};

class FBHAutomationSupport
{
public:
	static FBHAutomationConfig ParseCommandLine(const TCHAR* CommandLine);
	static FString NormalizeHostMode(FString HostMode);
	static bool IsKnownHostMode(const FString& HostMode);
	static FString MakeMarkerLine(const FBHAutomationConfig& Config, const FString& Marker);
};
