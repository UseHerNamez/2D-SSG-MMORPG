using UnrealBuildTool;
using System;
using System.IO;

public class TwoDSSG : ModuleRules
{
    public TwoDSSG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bEnableExceptions = true;
        bUseRTTI = true;
        CppStandard = CppStandardVersion.Cpp17;

        string ModulePath = ModuleDirectory;

        // Public headers for your static libs - server/editor only
        if (Target.Type == TargetType.Server || Target.Type == TargetType.Editor)
        {
            PublicIncludePaths.Add(Path.Combine(ModulePath, "DbStaticLib", "include"));
            PublicIncludePaths.Add(Path.Combine(ModulePath, "UtilsStaticLib", "include"));
        }

        // Your static libs (.lib) - Win64 subfolder
        string DbLibDir = Path.Combine(ModulePath, "DbStaticLib", "lib", "Win64");
        string UtilLibDir = Path.Combine(ModulePath, "UtilsStaticLib", "lib", "Win64");

        // Third-party you vendored
        string MySqlLibDir = Path.Combine(ModulePath, "MySQL", "lib"); // mysqlcppconnx.lib
        string MySqlBinDir = Path.Combine(ModulePath, "MySQL", "bin"); // mysqlcppconnx-2-vs14.dll + deps

        string OsslLibDir = Path.Combine(ModulePath, "OpenSSL", "lib"); // libssl.lib, libcrypto.lib
        string OsslBinDir = Path.Combine(ModulePath, "OpenSSL", "bin"); // libssl-3-x64.dll, libcrypto-3-x64.dll

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Link your libs - server/editor only
            if (Target.Type == TargetType.Server || Target.Type == TargetType.Editor)
            {
                PublicAdditionalLibraries.Add(Path.Combine(DbLibDir, "DbStaticLib.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(UtilLibDir, "UtilsStaticLib.lib"));
            }

            // Link third-party import libs required by your .obj code - server/editor only
            if (Target.Type == TargetType.Server || Target.Type == TargetType.Editor)
            {
                PublicAdditionalLibraries.Add(Path.Combine(MySqlLibDir, "mysqlcppconnx.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(OsslLibDir, "libssl.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(OsslLibDir, "libcrypto.lib"));
            }

            // Windows system libs used by the connector - server/editor only
            if (Target.Type == TargetType.Server || Target.Type == TargetType.Editor)
            {
                PublicSystemLibraries.AddRange(new string[]
                {
                    "ws2_32.lib",
                    "dnsapi.lib",
                    "crypt32.lib",
                    "advapi32.lib",
                    "user32.lib"
                });
            }

            // Stage runtime DLLs
            Action<string, string> Stage = (dir, dll) =>
            {
                string src = Path.Combine(dir, dll);
                if (File.Exists(src))
                {
                    PublicDelayLoadDLLs.Add(dll);
                    RuntimeDependencies.Add("$(TargetOutputDir)/" + dll, src);
                }
            };

            // MySQL connector and its deps
            Stage(MySqlBinDir, "mysqlcppconnx-2-vs14.dll");
            Stage(MySqlBinDir, "zlib1.dll");
            Stage(MySqlBinDir, "lz4.dll");
            Stage(MySqlBinDir, "zstd.dll");
            Stage(MySqlBinDir, "libprotobuf-lite.dll");
            Stage(MySqlBinDir, "abseil_dll.dll");

            // OpenSSL runtime (now from your OpenSSL/bin)
            Stage(OsslBinDir, "libssl-3-x64.dll");
            Stage(OsslBinDir, "libcrypto-3-x64.dll");
        }

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "HTTP", "Json", "JsonUtilities", "Paper2D"
        });
    }
}
