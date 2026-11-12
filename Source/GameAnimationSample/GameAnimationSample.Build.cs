// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class GameAnimationSample : ModuleRules
{
	public GameAnimationSample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"InputCore",
			"EnhancedInput",

			// AI
			"AIModule",
			"NavigationSystem",

			// Behavior Tree
			"GameplayTasks",

			// Niagara
			"Niagara",

			// Mover
			"Mover",
			
			//Motion Warping
			"MotionWarping",

			// Online
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
            "CoreOnline",
        });
		
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// UMG for widgets
			"UMG",
			"Slate",
			"SlateCore",

			// Audio
			"AudioMixer", "MultiplayerSessions", 
			//Camera
			"GameplayCameras",
		});
		
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
