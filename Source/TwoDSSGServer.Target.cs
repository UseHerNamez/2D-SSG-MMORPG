// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;


public class TwoDSSGServerTarget : TargetRules //Change this line according to the name of your project
{
    public TwoDSSGServerTarget(TargetInfo Target) : base(Target) //Change this line according to the name of your project
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("TwoDSSG"); //Change this line according to the name of your project
    }
}