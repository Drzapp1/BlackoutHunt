using UnrealBuildTool;
using System.IO;

public class BlackoutHunt : ModuleRules
{
	public BlackoutHunt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
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
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"GameplayTags",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineBase"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("bcrypt.lib");
		}

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AssetRegistry",
				"PropertyBindingUtils",
				"PropertyBindingUtilsEditor",
				"StructUtilsEditor",
				"StateTreeEditorModule",
				"UnrealEd"
			});
		}

		string PlayitAgentPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "Playit", "playit.exe");
		if (File.Exists(PlayitAgentPath))
		{
			RuntimeDependencies.Add("$(TargetOutputDir)/playit.exe", PlayitAgentPath);
		}
	}
}
