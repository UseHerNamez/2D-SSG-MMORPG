using UnrealBuildTool;
using System.Collections.Generic;

public class TwoDSSGTarget : TargetRules
{
    public TwoDSSGTarget(TargetInfo Target) : base(Target)
    {
        bCompileChaos = false;
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V2;

        ExtraModuleNames.AddRange(new string[] { "TwoDSSG" });

        bOverrideBuildEnvironment = true;
        CppStandard = CppStandardVersion.Cpp17;
    }
}
