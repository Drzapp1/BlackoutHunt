// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHAutomationSupport.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAutomationCommandLineTest,
	"BlackoutHunt.Automation.CommandLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAutomationCommandLineTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FBHAutomationConfig DisabledConfig = FBHAutomationSupport::ParseCommandLine(
		TEXT("-BHAutoHost=Facility -BHAutoJoin=192.168.56.1:7777 -BHAutoReady=1 -BHAutoQuitSeconds=5 -BHAutomationTag=ignored"));
	TestFalse(TEXT("Automation remains inert without -BHAutomation=1."), DisabledConfig.bEnabled);
	TestFalse(TEXT("Auto host remains inert when automation is disabled."), DisabledConfig.HasAutoHost());
	TestFalse(TEXT("Auto join remains inert when automation is disabled."), DisabledConfig.HasAutoJoin());
	TestFalse(TEXT("Auto ready remains inert when automation is disabled."), DisabledConfig.ShouldAutoReady());

	const FBHAutomationConfig EnabledConfig = FBHAutomationSupport::ParseCommandLine(
		TEXT("-BHAutomation=1 -BHAutoHost=Live -BHAutoJoin=192.168.56.1:7777 -BHAutoReady=1 -BHAutoQuitSeconds=12.5 -BHAutomationTag=student-01 -BHVirtualBoxSafe"));
	TestTrue(TEXT("Automation is enabled by explicit release-safe flag."), EnabledConfig.bEnabled);
	TestEqual(TEXT("Live alias normalizes to LiveClassroom."), EnabledConfig.AutoHost, FString(TEXT("LiveClassroom")));
	TestEqual(TEXT("Auto join address is captured."), EnabledConfig.AutoJoin, FString(TEXT("192.168.56.1:7777")));
	TestTrue(TEXT("Auto ready is enabled."), EnabledConfig.ShouldAutoReady());
	TestEqual(TEXT("Auto quit seconds are parsed."), EnabledConfig.AutoQuitSeconds, 12.5f);
	TestEqual(TEXT("Automation tag is captured."), EnabledConfig.Tag, FString(TEXT("student-01")));
	TestTrue(TEXT("VirtualBox-safe mode can be forced independently."), EnabledConfig.ShouldUseVirtualBoxSafeMode());

	const FBHAutomationConfig AtmosphereConfig = FBHAutomationSupport::ParseCommandLine(
		TEXT("-BHAutomation=1 -BHAutoAtmosphereTests=ambient,charge,targetclient -BHAutoMinPlayers=2"));
	TestTrue(TEXT("Atmosphere proof commands are parsed for command-line smoke runs."), AtmosphereConfig.HasAutoAtmosphereTests());
	TestEqual(TEXT("Atmosphere proof command list is captured."), AtmosphereConfig.AutoAtmosphereTests, FString(TEXT("ambient,charge,targetclient")));

	const FBHAutomationConfig MinPlayersConfig = FBHAutomationSupport::ParseCommandLine(
		TEXT("-BHAutomation=1 -BHAutoReady=1 -BHAutoMinPlayers=3"));
	TestEqual(TEXT("Auto min players can delay host ready for multi-client validation."), MinPlayersConfig.GetAutoMinPlayers(), 3);

	const FBHAutomationConfig VirtualBoxOnlyConfig = FBHAutomationSupport::ParseCommandLine(TEXT("-BHVirtualBoxSafe"));
	TestFalse(TEXT("VirtualBox safe mode does not enable automation."), VirtualBoxOnlyConfig.bEnabled);
	TestTrue(TEXT("VirtualBox safe mode can be requested without automation."), VirtualBoxOnlyConfig.ShouldUseVirtualBoxSafeMode());

	TestTrue(TEXT("Known host mode accepts Foggrounds."), FBHAutomationSupport::IsKnownHostMode(FBHAutomationSupport::NormalizeHostMode(TEXT("fog"))));
	TestEqual(TEXT("Live Facility alias keeps Live Classroom map selection."), FBHAutomationSupport::NormalizeHostMode(TEXT("LiveFacility")), FString(TEXT("LiveFacility")));
	TestEqual(TEXT("Live Substation alias keeps Live Classroom map selection."), FBHAutomationSupport::NormalizeHostMode(TEXT("LiveSubstation")), FString(TEXT("LiveSubstation")));
	TestEqual(TEXT("Live Fog alias keeps Live Classroom map selection."), FBHAutomationSupport::NormalizeHostMode(TEXT("LiveFog")), FString(TEXT("LiveFoggrounds")));
	TestTrue(TEXT("Known host mode accepts Live Foggrounds."), FBHAutomationSupport::IsKnownHostMode(FBHAutomationSupport::NormalizeHostMode(TEXT("LiveFoggrounds"))));
	TestFalse(TEXT("Unknown host mode remains invalid."), FBHAutomationSupport::IsKnownHostMode(FBHAutomationSupport::NormalizeHostMode(TEXT("Basement"))));

	return true;
}

#endif
