// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class TwoDSSG : ModuleRules
{
	public TwoDSSG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Http" }); 

        PublicIncludePaths.AddRange(new string[] {
        "C:/Program Files/MySQL/MySQL Connector C++ 8.2/include"
        });

        PublicSystemLibraryPaths.AddRange(new string[] {
        "C:/Program Files/MySQL/MySQL Connector C++ 8.2/lib64"
        });

        PublicAdditionalLibraries.AddRange(new string[] {
        "C:/Program Files/MySQL/MySQL Connector C++ 8.2/lib64/vs14/mysqlcppconn.lib"
        });   

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
