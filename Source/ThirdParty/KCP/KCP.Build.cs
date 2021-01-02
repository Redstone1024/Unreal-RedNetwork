using System.IO;
using UnrealBuildTool;

public class KCP : ModuleRules
{
    public KCP(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
                Path.Combine(ModuleDirectory, "Public")
            });


        PrivateIncludePaths.AddRange(
            new string[]
            {
                Path.Combine(ModuleDirectory, "Private")
            });

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            });


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
            });

        PublicDefinitions.Add("_CRT_HAS_CXX17=0");
    }
}
