// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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
		string SteamAppId = System.Environment.GetEnvironmentVariable("BH_STEAM_APP_ID");
		if (!string.IsNullOrWhiteSpace(SteamAppId))
		{
			GlobalDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID=" + SteamAppId);
		}
		ExtraModuleNames.Add("BlackoutHunt");
	}
}
