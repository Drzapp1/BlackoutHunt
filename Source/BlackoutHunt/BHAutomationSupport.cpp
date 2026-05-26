#include "BHAutomationSupport.h"

#include "Misc/Parse.h"

namespace
{
	bool IsTruthyValue(FString Value)
	{
		Value.TrimStartAndEndInline();
		return Value.Equals(TEXT("1"))
			|| Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("on"), ESearchCase::IgnoreCase);
	}

	bool IsFalsyValue(FString Value)
	{
		Value.TrimStartAndEndInline();
		return Value.Equals(TEXT("0"))
			|| Value.Equals(TEXT("false"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("no"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("off"), ESearchCase::IgnoreCase);
	}

	bool ParseBoolFlag(const TCHAR* CommandLine, const TCHAR* FlagName)
	{
		FString FlagValue;
		const FString ValuePrefix = FString::Printf(TEXT("%s="), FlagName);
		if (FParse::Value(CommandLine, *ValuePrefix, FlagValue))
		{
			return IsTruthyValue(FlagValue);
		}

		return FParse::Param(CommandLine, FlagName);
	}

	FString ParseStringValue(const TCHAR* CommandLine, const TCHAR* KeyName)
	{
		FString Value;
		const FString ValuePrefix = FString::Printf(TEXT("%s="), KeyName);
		FParse::Value(CommandLine, *ValuePrefix, Value);
		Value.TrimStartAndEndInline();
		return Value;
	}
}

FBHAutomationConfig FBHAutomationSupport::ParseCommandLine(const TCHAR* CommandLine)
{
	FBHAutomationConfig Config;
	const TCHAR* SafeCommandLine = CommandLine ? CommandLine : TEXT("");

	FString AutomationValue;
	if (FParse::Value(SafeCommandLine, TEXT("BHAutomation="), AutomationValue))
	{
		Config.bEnabled = IsTruthyValue(AutomationValue);
	}
	else
	{
		Config.bEnabled = FParse::Param(SafeCommandLine, TEXT("BHAutomation"));
	}

	Config.bVirtualBoxSafeRequested = ParseBoolFlag(SafeCommandLine, TEXT("BHVirtualBoxSafe"));

	if (!Config.bEnabled)
	{
		return Config;
	}

	FString AutoReadyValue;
	if (FParse::Value(SafeCommandLine, TEXT("BHAutoReady="), AutoReadyValue))
	{
		Config.bAutoReady = IsTruthyValue(AutoReadyValue) && !IsFalsyValue(AutoReadyValue);
	}
	else
	{
		Config.bAutoReady = FParse::Param(SafeCommandLine, TEXT("BHAutoReady"));
	}

	Config.AutoHost = NormalizeHostMode(ParseStringValue(SafeCommandLine, TEXT("BHAutoHost")));
	Config.AutoJoin = ParseStringValue(SafeCommandLine, TEXT("BHAutoJoin"));
	Config.Tag = ParseStringValue(SafeCommandLine, TEXT("BHAutomationTag"));

	int32 ParsedMinPlayers = 0;
	if (FParse::Value(SafeCommandLine, TEXT("BHAutoMinPlayers="), ParsedMinPlayers))
	{
		Config.AutoMinPlayers = FMath::Clamp(ParsedMinPlayers, 0, 32);
	}

	float ParsedQuitSeconds = 0.0f;
	if (FParse::Value(SafeCommandLine, TEXT("BHAutoQuitSeconds="), ParsedQuitSeconds))
	{
		Config.AutoQuitSeconds = FMath::Clamp(ParsedQuitSeconds, 0.0f, 24.0f * 60.0f * 60.0f);
	}

	return Config;
}

FString FBHAutomationSupport::NormalizeHostMode(FString HostMode)
{
	HostMode.TrimStartAndEndInline();
	if (HostMode.IsEmpty())
	{
		return FString();
	}

	if (HostMode.Equals(TEXT("LiveClassroom"), ESearchCase::IgnoreCase)
		|| HostMode.Equals(TEXT("Classroom"), ESearchCase::IgnoreCase)
		|| HostMode.Equals(TEXT("Live"), ESearchCase::IgnoreCase))
	{
		return TEXT("LiveClassroom");
	}

	if (HostMode.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
	{
		return TEXT("Substation");
	}

	if (HostMode.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase)
		|| HostMode.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
	{
		return TEXT("Foggrounds");
	}

	if (HostMode.Equals(TEXT("Facility"), ESearchCase::IgnoreCase))
	{
		return TEXT("Facility");
	}

	return HostMode;
}

bool FBHAutomationSupport::IsKnownHostMode(const FString& HostMode)
{
	return HostMode.Equals(TEXT("LiveClassroom"), ESearchCase::CaseSensitive)
		|| HostMode.Equals(TEXT("Facility"), ESearchCase::CaseSensitive)
		|| HostMode.Equals(TEXT("Substation"), ESearchCase::CaseSensitive)
		|| HostMode.Equals(TEXT("Foggrounds"), ESearchCase::CaseSensitive);
}

FString FBHAutomationSupport::MakeMarkerLine(const FBHAutomationConfig& Config, const FString& Marker)
{
	if (Config.Tag.IsEmpty())
	{
		return Marker;
	}

	return FString::Printf(TEXT("%s Tag=%s"), *Marker, *Config.Tag);
}
