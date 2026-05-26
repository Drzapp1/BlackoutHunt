#if WITH_DEV_AUTOMATION_TESTS

#include "BHNetworkSupport.h"
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

#endif
