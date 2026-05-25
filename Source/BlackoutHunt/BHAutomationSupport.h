#pragma once

#include "CoreMinimal.h"

struct FBHAutomationConfig
{
	bool bEnabled = false;
	bool bAutoReady = false;
	bool bVirtualBoxSafeRequested = false;
	bool bVirtualBoxSafeDetected = false;
	float AutoQuitSeconds = 0.0f;
	FString AutoHost;
	FString AutoJoin;
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

	bool ShouldAutoQuit() const
	{
		return bEnabled && AutoQuitSeconds > 0.0f;
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
