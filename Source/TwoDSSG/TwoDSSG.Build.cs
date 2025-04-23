using UnrealBuildTool;
using System.IO;

public class TwoDSSG : ModuleRules
{
    public TwoDSSG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp17;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "Http", "Json", "JsonUtilities"
        });

    }
}
