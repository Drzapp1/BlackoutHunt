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
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineBase"
		});

		string PlayitAgentPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "Playit", "playit.exe");
		if (File.Exists(PlayitAgentPath))
		{
			RuntimeDependencies.Add("$(TargetOutputDir)/playit.exe", PlayitAgentPath);
		}
	}
}
