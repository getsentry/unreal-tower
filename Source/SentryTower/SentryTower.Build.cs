// Copyright (c) 2024 Sentry. All Rights Reserved.

using UnrealBuildTool;

public class SentryTower : ModuleRules
{
	public SentryTower(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "EnhancedInput", "UMG", "Sentry", "HTTP", "Json", "JsonUtilities" });

		string PlatformName = Target.Platform.ToString();
		if (PlatformName == "Win64" || PlatformName == "XSX" || PlatformName == "XB1")
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "SentryShaders" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
