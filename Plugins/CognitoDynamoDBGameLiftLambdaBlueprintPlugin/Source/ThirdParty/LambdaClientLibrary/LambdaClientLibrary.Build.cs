/* Copyright (C) Siqi.Wu - All Rights Reserved
 * Written by Siqi.Wu <lion547016@gmail.com>, December 2018
 */

using System.IO;
using UnrealBuildTool;

public class LambdaClientLibrary : ModuleRules
{
    public LambdaClientLibrary(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        PublicIncludePaths.Add(ModuleDirectory);

        string ThirdPartyPath = System.IO.Path.Combine(ModuleDirectory, Target.Platform.ToString());

        if (Target.Type != TargetRules.TargetType.Server)
        {
            if (Target.Platform == UnrealTargetPlatform.Linux)
            {
                ThirdPartyPath = System.IO.Path.Combine(ThirdPartyPath, "x86_64-unknown-linux-gnu");
                PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "libaws-cpp-sdk-lambda.so"));
                RuntimeDependencies.Add(System.IO.Path.Combine(ThirdPartyPath, "libaws-cpp-sdk-lambda.so"));
            }
            else if (Target.Platform == UnrealTargetPlatform.Android)
            {
                AdditionalPropertiesForReceipt.Add("AndroidPlugin", System.IO.Path.Combine(ModuleDirectory, "LambdaClientSDK_UPL.xml"));

                {
                    PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "armeabi-v7a", "libaws-cpp-sdk-lambda.so"));
                    RuntimeDependencies.Add(System.IO.Path.Combine(ThirdPartyPath, "armeabi-v7a", "libaws-cpp-sdk-lambda.so"));
                }

                {
                    PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "arm64-v8a", "libaws-cpp-sdk-lambda.so"));
                    RuntimeDependencies.Add(System.IO.Path.Combine(ThirdPartyPath, "arm64-v8a", "libaws-cpp-sdk-lambda.so"));
                }

                {
                    PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "x86_64", "libaws-cpp-sdk-lambda.so"));
                    RuntimeDependencies.Add(System.IO.Path.Combine(ThirdPartyPath, "x86_64", "libaws-cpp-sdk-lambda.so"));
                }

            }
            else if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "aws-cpp-sdk-lambda.lib"));
                PublicDelayLoadDLLs.Add("aws-cpp-sdk-lambda.dll");
                RuntimeDependencies.Add(System.IO.Path.Combine(ThirdPartyPath, "aws-cpp-sdk-lambda.dll"));
            }
            else if (Target.Platform == UnrealTargetPlatform.Mac)
            {
                PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "libaws-cpp-sdk-lambda.dylib"));
                RuntimeDependencies.Add(System.IO.Path.Combine(ThirdPartyPath, "libaws-cpp-sdk-lambda.dylib"));
            }
            else if (Target.Platform == UnrealTargetPlatform.IOS)
            {
                PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyPath, "libaws-cpp-sdk-lambda.a"));
            }
        }
    }
}