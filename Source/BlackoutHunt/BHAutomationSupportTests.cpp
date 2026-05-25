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

	const FBHAutomationConfig VirtualBoxOnlyConfig = FBHAutomationSupport::ParseCommandLine(TEXT("-BHVirtualBoxSafe"));
	TestFalse(TEXT("VirtualBox safe mode does not enable automation."), VirtualBoxOnlyConfig.bEnabled);
	TestTrue(TEXT("VirtualBox safe mode can be requested without automation."), VirtualBoxOnlyConfig.ShouldUseVirtualBoxSafeMode());

	TestTrue(TEXT("Known host mode accepts Foggrounds."), FBHAutomationSupport::IsKnownHostMode(FBHAutomationSupport::NormalizeHostMode(TEXT("fog"))));
	TestFalse(TEXT("Unknown host mode remains invalid."), FBHAutomationSupport::IsKnownHostMode(FBHAutomationSupport::NormalizeHostMode(TEXT("Basement"))));

	return true;
}

#endif
