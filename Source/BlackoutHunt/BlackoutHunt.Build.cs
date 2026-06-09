// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

using UnrealBuildTool;
using System.IO;

public class BlackoutHunt : ModuleRules
{
	public BlackoutHunt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Developer-only "unlock everything" (Ctrl+Alt+U). OFF by default, so it is compiled OUT of Shipping exactly
		// as before and production packages never contain it. To compile it into YOUR personal build (including a
		// Shipping .exe), set the build-time environment variable BH_DEV_UNLOCK=1 before compiling/packaging; the
		// unlock then still requires the runtime Saved/.bhdev marker (or BH_DEV=1) to fire. Leave the variable unset
		// for production. Always defined as 0/1 so the #if guards never reference an undefined identifier.
		bool bDevUnlock = System.Environment.GetEnvironmentVariable("BH_DEV_UNLOCK") == "1";
		PrivateDefinitions.Add("BH_DEV_UNLOCK=" + (bDevUnlock ? "1" : "0"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RenderCore",
			"InputCore",
			"ApplicationCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"NetCore",
			"AudioMixer",
			"Sockets",
			"HTTP",
			"Json",
			"Niagara",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"GameplayTags",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineBase",
			"AssetRegistry"
		});

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			DynamicallyLoadedModuleNames.AddRange(new string[]
			{
				"OnlineSubsystemEOS",
				"SocketSubsystemEOS"
			});

			PublicSystemLibraries.Add("bcrypt.lib");
		}

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"PropertyBindingUtils",
				"PropertyBindingUtilsEditor",
				"StructUtilsEditor",
				"StateTreeEditorModule",
				"UnrealEd",
				"MaterialEditor"
			});
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string PlayitAgentPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "Playit", "playit.exe");
			if (File.Exists(PlayitAgentPath))
			{
				RuntimeDependencies.Add("$(TargetOutputDir)/playit.exe", PlayitAgentPath);
			}
		}
	}
}
