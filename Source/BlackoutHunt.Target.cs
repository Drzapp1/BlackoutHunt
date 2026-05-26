using UnrealBuildTool;
using System.Collections.Generic;

public class BlackoutHuntTarget : TargetRules
{
	public BlackoutHuntTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bOverrideBuildEnvironment = true;
		bEnableTrace = false;
		ExtraModuleNames.Add("BlackoutHunt");
	}
}
