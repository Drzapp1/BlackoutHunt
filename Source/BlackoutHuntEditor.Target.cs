using UnrealBuildTool;
using System.Collections.Generic;

public class BlackoutHuntEditorTarget : TargetRules
{
	public BlackoutHuntEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BlackoutHunt");
	}
}
