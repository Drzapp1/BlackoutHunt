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
	static FString MakeJoinInviteCode(const FString& Address, int32 DefaultPort = 7777);
	static FString ResolveLocalJoinAddress(int32 LocalPort = 7777);
	static FString MakeDefaultGameSsid();
	static FString MakeDefaultPassphrase();
	static bool IsGameHotspotSsid(const FString& Ssid);
};
