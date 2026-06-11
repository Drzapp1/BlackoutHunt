// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

struct FBHHotspotLaunchResult
{
	bool bSuccess = false;
	FString Ssid;
	FString Passphrase;
	FString Message;
};

struct FBHInternetTunnelResult
{
	bool bSuccess = false;
	bool bTunnelReady = false;
	int32 LocalPort = 7777;
	FString AgentPath;
	FString LogPath;
	FString TunnelAddress;
	FString Message;
};

class FBHNetworkSupport
{
public:
	static FBHHotspotLaunchResult StartGameHotspot(const FString& PreferredSsid = FString(), const FString& PreferredPassphrase = FString());
	static FBHHotspotLaunchResult StopGameHotspot(const FString& KnownSsid);
	static FBHInternetTunnelResult StartInternetTunnel(int32 LocalPort = 7777);
	static FBHInternetTunnelResult StopInternetTunnel();
	static FBHInternetTunnelResult OpenInternetTunnelSetup(int32 LocalPort = 7777);
	static FBHInternetTunnelResult GetInternetTunnelStatus(int32 LocalPort = 7777);
	static FString NormalizeJoinAddress(const FString& Address, int32 DefaultPort = 7777);
	static FString NormalizePreferredJoinEndpoint(const TArray<FString>& Endpoints, int32 DefaultPort = 7777);
	// Tunnel-agent log triage (pure text policy, exposed for tests): true when an agent session marker
	// ("udp session details received" / "agent registered") appears within the LAST TailCharsToScan
	// characters of the log text. The playit agent log only accumulates — a dead session that keeps
	// appending reconnect errors still CONTAINS markers from when it was healthy (and that spam keeps
	// the file mtime fresh, defeating any mtime gate) — while a live agent re-logs its session marker
	// every ~10 seconds. Requiring the marker in the recent tail makes the marker itself prove recency.
	static bool LogTextShowsRecentAgentSession(const FString& LogText, int32 TailCharsToScan = 64 * 1024);
	static FString MakeJoinInviteCode(const FString& Address, int32 DefaultPort = 7777);
	static FString ResolveLocalJoinAddress(int32 LocalPort = 7777);
	static FString MakeDefaultGameSsid();
	static FString MakeDefaultPassphrase();
	static bool IsGameHotspotSsid(const FString& Ssid);
};
