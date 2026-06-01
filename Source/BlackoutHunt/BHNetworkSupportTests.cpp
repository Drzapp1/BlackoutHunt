// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHNetworkSupport.h"
#include "BHLocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHNetworkSupportNormalizeJoinAddressTest,
	"BlackoutHunt.Network.NormalizeJoinAddress",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBHNetworkSupportNormalizeJoinAddressTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("IPv4 address gets the default port"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("192.168.1.20")),
		FString(TEXT("192.168.1.20:7777")));

	TestEqual(TEXT("Host and explicit port are preserved"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("classroom-host.local:7788")),
		FString(TEXT("classroom-host.local:7788")));

	TestEqual(TEXT("Owned Playit classroom endpoint is preserved"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("blackouthunt.playit.plus:24761")),
		FString(TEXT("blackouthunt.playit.plus:24761")));

	TArray<FString> ClassroomEndpoints;
	ClassroomEndpoints.Add(TEXT(""));
	ClassroomEndpoints.Add(TEXT("blackouthunt.playit.plus:24761"));
	ClassroomEndpoints.Add(TEXT("192.168.1.20:7777"));
	TestEqual(TEXT("Preferred classroom endpoint uses the first configured Playit endpoint"),
		FBHNetworkSupport::NormalizePreferredJoinEndpoint(ClassroomEndpoints),
		FString(TEXT("blackouthunt.playit.plus:24761")));

	TestEqual(TEXT("Bracketed IPv6 and port are preserved"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("[2001:db8::1]:7777")),
		FString(TEXT("[2001:db8::1]:7777")));

	TestEqual(TEXT("Bare IPv6 gets bracketed with the default port"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("2001:db8::1")),
		FString(TEXT("[2001:db8::1]:7777")));

	TestTrue(TEXT("Unsupported URI schemes are rejected"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("steam://example.com:7777")).IsEmpty());

	TestEqual(TEXT("udp:// scheme prefix is stripped so a pasted tunnel address still joins"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("udp://blackouthunt.playit.plus:24761")),
		FString(TEXT("blackouthunt.playit.plus:24761")));

	TestTrue(TEXT("Path fragments are rejected"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("example.com:7777/path")).IsEmpty());

	TestTrue(TEXT("Query fragments are rejected"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("example.com:7777?x=1")).IsEmpty());

	TestTrue(TEXT("Malformed ports are rejected"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("example.com:notaport")).IsEmpty());

	TestTrue(TEXT("Unsafe host characters are rejected"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("example.com;quit:7777")).IsEmpty());

	const FString InviteCode = FBHNetworkSupport::MakeJoinInviteCode(TEXT("classroom-host.local:7790"));
	TestFalse(TEXT("Invite code is generated"), InviteCode.IsEmpty());
	TestEqual(TEXT("Invite code decodes to the normalized address"),
		FBHNetworkSupport::NormalizeJoinAddress(InviteCode),
		FString(TEXT("classroom-host.local:7790")));

	TestTrue(TEXT("Malformed invite code is rejected"),
		FBHNetworkSupport::NormalizeJoinAddress(TEXT("BH1:not valid")).IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHLocalPlayerClassConfiguredTest,
	"BlackoutHunt.Network.LocalPlayerClassConfigured",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBHLocalPlayerClassConfiguredTest::RunTest(const FString& Parameters)
{
	// The reconnect-token-across-ServerTravel feature relies on UBHLocalPlayer::GetGameLoginOptions being the
	// active local player class, wired via DefaultEngine.ini ([/Script/Engine.Engine] LocalPlayerClassName).
	// If that ini entry or the class were ever dropped (e.g. a bad cook), the engine silently falls back to
	// base ULocalPlayer and travel-state restore degrades to display-name matching. Assert the wiring resolves.
	UClass* ResolvedClass = LoadClass<ULocalPlayer>(nullptr, TEXT("/Script/BlackoutHunt.BHLocalPlayer"));
	TestNotNull(TEXT("Configured LocalPlayer class path resolves"), ResolvedClass);
	TestTrue(TEXT("Resolved LocalPlayer class is UBHLocalPlayer"), ResolvedClass == UBHLocalPlayer::StaticClass());

	if (GEngine)
	{
		TestTrue(TEXT("GEngine LocalPlayerClass is UBHLocalPlayer (ini wiring took effect)"),
			GEngine->LocalPlayerClass == UBHLocalPlayer::StaticClass());
	}

	return true;
}

#endif
