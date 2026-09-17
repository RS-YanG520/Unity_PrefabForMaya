// Copyright Ciprian Stanciu 2024

using System;
using System.IO;
using UnrealBuildTool;

public class UnrealToUnity : ModuleRules
{
	public UnrealToUnity( ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange( new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay",
         //Extra
           "RenderCore",
           "MaterialEditor",
           "UnrealEd",
            "RHI",
            "Slate",
            "InputCore",
            "SlateCore",
            "EditorStyle",
            "Foliage",
            "ProceduralMeshComponent",
            "Landscape",
            "MeshDescription",
            "RawMesh",
			"Projects",
            "MaterialBaking",
            "MeshMergeUtilities",
            "MeshConversion",
            "TextureCompressor",
            "ImageCore"
        } );

        PrivateDependencyModuleNames.AddRange( new string[] {"Core", "CoreUObject", "Engine", "InputCore", 
            "RenderCore",
            "MaterialEditor",
            "UnrealEd",
            "RHI",
            "Slate",
            "InputCore",
            "SlateCore",
            "EditorStyle",
            "Foliage",
            "ProceduralMeshComponent",
            "Landscape",
            "MeshDescription",
            "RawMesh",
			"Projects",
            "MaterialBaking",
            "MeshMergeUtilities",
            "MeshConversion",
            "TextureCompressor",
            "ImageCore"
        } );

        if( Target.Version.MajorVersion == 4 && Target.Version.MinorVersion < 26 )
        {
            PrivateIncludePaths.AddRange(
                new string[] {
                "ThirdParty\\FBX\\2018.1.1\\include",//4.20 compatibility
                "Editor/UnrealEd/Private",  //4.20 compatibility
                }
            );

            PublicIncludePaths.AddRange(
               new string[] {
                "ThirdParty\\FBX\\2018.1.1\\include",//4.20 compatibility
                "Editor/UnrealEd/Private",  //4.20 compatibility
               }
           );
        }

        //4.20 compatibility
        //if( Target.Version.MajorVersion == 4 && Target.Version.MinorVersion < 26 )
            //SetupModulePhysXAPEXSupport(Target);
        //else
        {
            AddEngineThirdPartyPrivateStaticDependencies( Target,
            "FBX"
            );
        }
        if( (Target.Version.MajorVersion == 4 && Target.Version.MinorVersion > 20 ) || Target.Version.MajorVersion >= 5 )
        {
            PublicDependencyModuleNames.Add( "StaticMeshDescription" );
            PublicDependencyModuleNames.Add( "GeometryCollectionEngine" );
            PublicDependencyModuleNames.Add( "Chaos" );
            //
            PrivateDependencyModuleNames.Add( "StaticMeshDescription" );
            PrivateDependencyModuleNames.Add( "GeometryCollectionEngine" );
            PrivateDependencyModuleNames.Add( "Chaos" );
        }
        if( Target.Version.MajorVersion >= 5 && Target.Version.MinorVersion >= 0 )
        {
            PublicDependencyModuleNames.Add( "GeometryFramework" );
            //            
            PrivateDependencyModuleNames.Add( "GeometryFramework" );
        }
        //System.Console.WriteLine( "PluginDirectory " + PluginDirectory );

        //if( Target.Version.MajorVersion >= 5 && Target.Version.MinorVersion >= 5 )
        //{
        //    //for MaterialUniformExpressions.h
        //    PrivateIncludePaths.AddRange( new string[] {
        //    System.IO.Path.Combine(GetModuleDirectory("Engine"), "Private"),
        //    } );
        //}
    }
}
