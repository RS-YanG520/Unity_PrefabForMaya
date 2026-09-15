// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "BaseScene.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeBaseScene() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
UNREALTOUNITY_API UClass* Z_Construct_UClass_UBaseSettingsClass();
UNREALTOUNITY_API UClass* Z_Construct_UClass_UBaseSettingsClass_NoRegister();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_EngineType();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_FoliageExportType();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_RenderPipeline();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_UnityShadersType();
UPackage* Z_Construct_UPackage__Script_UnrealToUnity();
// ********** End Cross Module References **********************************************************

// ********** Begin Enum RenderPipeline ************************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_RenderPipeline;
static UEnum* RenderPipeline_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_RenderPipeline.OuterSingleton)
	{
		Z_Registration_Info_UEnum_RenderPipeline.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UnrealToUnity_RenderPipeline, (UObject*)Z_Construct_UPackage__Script_UnrealToUnity(), TEXT("RenderPipeline"));
	}
	return Z_Registration_Info_UEnum_RenderPipeline.OuterSingleton;
}
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<RenderPipeline>()
{
	return RenderPipeline_StaticEnum();
}
struct Z_Construct_UEnum_UnrealToUnity_RenderPipeline_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/BaseScene.h" },
		{ "RP_BUILTIN.DisplayName", "Built In Render Pipeline" },
		{ "RP_BUILTIN.Name", "RenderPipeline::RP_BUILTIN" },
		{ "RP_HDRP.DisplayName", "HD Render Pipeline" },
		{ "RP_HDRP.Name", "RenderPipeline::RP_HDRP" },
		{ "RP_URP.DisplayName", "Universal Render Pipeline" },
		{ "RP_URP.Name", "RenderPipeline::RP_URP" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "RenderPipeline::RP_BUILTIN", (int64)RenderPipeline::RP_BUILTIN },
		{ "RenderPipeline::RP_URP", (int64)RenderPipeline::RP_URP },
		{ "RenderPipeline::RP_HDRP", (int64)RenderPipeline::RP_HDRP },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_UnrealToUnity_RenderPipeline_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UnrealToUnity_RenderPipeline_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UnrealToUnity,
	nullptr,
	"RenderPipeline",
	"RenderPipeline",
	Z_Construct_UEnum_UnrealToUnity_RenderPipeline_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_RenderPipeline_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_RenderPipeline_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UnrealToUnity_RenderPipeline_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UnrealToUnity_RenderPipeline()
{
	if (!Z_Registration_Info_UEnum_RenderPipeline.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_RenderPipeline.InnerSingleton, Z_Construct_UEnum_UnrealToUnity_RenderPipeline_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_RenderPipeline.InnerSingleton;
}
// ********** End Enum RenderPipeline **************************************************************

// ********** Begin Enum SkinnedMeshOptionsEnum ****************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_SkinnedMeshOptionsEnum;
static UEnum* SkinnedMeshOptionsEnum_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_SkinnedMeshOptionsEnum.OuterSingleton)
	{
		Z_Registration_Info_UEnum_SkinnedMeshOptionsEnum.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum, (UObject*)Z_Construct_UPackage__Script_UnrealToUnity(), TEXT("SkinnedMeshOptionsEnum"));
	}
	return Z_Registration_Info_UEnum_SkinnedMeshOptionsEnum.OuterSingleton;
}
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<SkinnedMeshOptionsEnum>()
{
	return SkinnedMeshOptionsEnum_StaticEnum();
}
struct Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/BaseScene.h" },
		{ "SMO_EXPORT_AS_SKINNED.DisplayName", "Export As Skinned Mesh" },
		{ "SMO_EXPORT_AS_SKINNED.Name", "SkinnedMeshOptionsEnum::SMO_EXPORT_AS_SKINNED" },
		{ "SMO_EXPORT_AS_STATIC.DisplayName", "Export As Static Mesh" },
		{ "SMO_EXPORT_AS_STATIC.Name", "SkinnedMeshOptionsEnum::SMO_EXPORT_AS_STATIC" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "SkinnedMeshOptionsEnum::SMO_EXPORT_AS_STATIC", (int64)SkinnedMeshOptionsEnum::SMO_EXPORT_AS_STATIC },
		{ "SkinnedMeshOptionsEnum::SMO_EXPORT_AS_SKINNED", (int64)SkinnedMeshOptionsEnum::SMO_EXPORT_AS_SKINNED },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UnrealToUnity,
	nullptr,
	"SkinnedMeshOptionsEnum",
	"SkinnedMeshOptionsEnum",
	Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum()
{
	if (!Z_Registration_Info_UEnum_SkinnedMeshOptionsEnum.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_SkinnedMeshOptionsEnum.InnerSingleton, Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_SkinnedMeshOptionsEnum.InnerSingleton;
}
// ********** End Enum SkinnedMeshOptionsEnum ******************************************************

// ********** Begin Enum NaniteOptionsEnum *********************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_NaniteOptionsEnum;
static UEnum* NaniteOptionsEnum_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_NaniteOptionsEnum.OuterSingleton)
	{
		Z_Registration_Info_UEnum_NaniteOptionsEnum.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum, (UObject*)Z_Construct_UPackage__Script_UnrealToUnity(), TEXT("NaniteOptionsEnum"));
	}
	return Z_Registration_Info_UEnum_NaniteOptionsEnum.OuterSingleton;
}
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<NaniteOptionsEnum>()
{
	return NaniteOptionsEnum_StaticEnum();
}
struct Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/BaseScene.h" },
		{ "NANITE_COARSE_MESH.DisplayName", "Export Coarse Mesh" },
		{ "NANITE_COARSE_MESH.Name", "NaniteOptionsEnum::NANITE_COARSE_MESH" },
		{ "NANITE_ORIGINAL_MESH.DisplayName", "Export Original Mesh" },
		{ "NANITE_ORIGINAL_MESH.Name", "NaniteOptionsEnum::NANITE_ORIGINAL_MESH" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "NaniteOptionsEnum::NANITE_COARSE_MESH", (int64)NaniteOptionsEnum::NANITE_COARSE_MESH },
		{ "NaniteOptionsEnum::NANITE_ORIGINAL_MESH", (int64)NaniteOptionsEnum::NANITE_ORIGINAL_MESH },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UnrealToUnity,
	nullptr,
	"NaniteOptionsEnum",
	"NaniteOptionsEnum",
	Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum()
{
	if (!Z_Registration_Info_UEnum_NaniteOptionsEnum.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_NaniteOptionsEnum.InnerSingleton, Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_NaniteOptionsEnum.InnerSingleton;
}
// ********** End Enum NaniteOptionsEnum ***********************************************************

// ********** Begin Enum TangentToWorldLocationType ************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_TangentToWorldLocationType;
static UEnum* TangentToWorldLocationType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_TangentToWorldLocationType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_TangentToWorldLocationType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType, (UObject*)Z_Construct_UPackage__Script_UnrealToUnity(), TEXT("TangentToWorldLocationType"));
	}
	return Z_Registration_Info_UEnum_TangentToWorldLocationType.OuterSingleton;
}
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<TangentToWorldLocationType>()
{
	return TangentToWorldLocationType_StaticEnum();
}
struct Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/BaseScene.h" },
		{ "TTWL_AFTER_NORMAL_CALCULATION.DisplayName", "Flip YZ After Normals" },
		{ "TTWL_AFTER_NORMAL_CALCULATION.Name", "TangentToWorldLocationType::TTWL_AFTER_NORMAL_CALCULATION" },
		{ "TTWL_BEFORE_NORMAL_CALCULATION.DisplayName", "Flip YZ Before Normals" },
		{ "TTWL_BEFORE_NORMAL_CALCULATION.Name", "TangentToWorldLocationType::TTWL_BEFORE_NORMAL_CALCULATION" },
		{ "TTWL_KEEP_UNITY_VECTORS.DisplayName", "Keep Unity Vectors" },
		{ "TTWL_KEEP_UNITY_VECTORS.Name", "TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS" },
		{ "TTWL_KEEP_UNITY_VECTORS_NO_WORLDNORMALCOPY.DisplayName", "Keep Unity Vectors + No WorldNormalCopy" },
		{ "TTWL_KEEP_UNITY_VECTORS_NO_WORLDNORMALCOPY.Name", "TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS_NO_WORLDNORMALCOPY" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS", (int64)TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS },
		{ "TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS_NO_WORLDNORMALCOPY", (int64)TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS_NO_WORLDNORMALCOPY },
		{ "TangentToWorldLocationType::TTWL_BEFORE_NORMAL_CALCULATION", (int64)TangentToWorldLocationType::TTWL_BEFORE_NORMAL_CALCULATION },
		{ "TangentToWorldLocationType::TTWL_AFTER_NORMAL_CALCULATION", (int64)TangentToWorldLocationType::TTWL_AFTER_NORMAL_CALCULATION },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UnrealToUnity,
	nullptr,
	"TangentToWorldLocationType",
	"TangentToWorldLocationType",
	Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType()
{
	if (!Z_Registration_Info_UEnum_TangentToWorldLocationType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_TangentToWorldLocationType.InnerSingleton, Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_TangentToWorldLocationType.InnerSingleton;
}
// ********** End Enum TangentToWorldLocationType **************************************************

// ********** Begin Enum FoliageExportType *********************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_FoliageExportType;
static UEnum* FoliageExportType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_FoliageExportType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_FoliageExportType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UnrealToUnity_FoliageExportType, (UObject*)Z_Construct_UPackage__Script_UnrealToUnity(), TEXT("FoliageExportType"));
	}
	return Z_Registration_Info_UEnum_FoliageExportType.OuterSingleton;
}
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<FoliageExportType>()
{
	return FoliageExportType_StaticEnum();
}
struct Z_Construct_UEnum_UnrealToUnity_FoliageExportType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "FET_CREATE_GAMEOBJECTS.DisplayName", "Create GameObject Per Instance" },
		{ "FET_CREATE_GAMEOBJECTS.Name", "FoliageExportType::FET_CREATE_GAMEOBJECTS" },
		{ "FET_USE_TREEINSTANCES.DisplayName", "Tree Instances(requires extra step in Unity)" },
		{ "FET_USE_TREEINSTANCES.Name", "FoliageExportType::FET_USE_TREEINSTANCES" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "FoliageExportType::FET_CREATE_GAMEOBJECTS", (int64)FoliageExportType::FET_CREATE_GAMEOBJECTS },
		{ "FoliageExportType::FET_USE_TREEINSTANCES", (int64)FoliageExportType::FET_USE_TREEINSTANCES },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_UnrealToUnity_FoliageExportType_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UnrealToUnity_FoliageExportType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UnrealToUnity,
	nullptr,
	"FoliageExportType",
	"FoliageExportType",
	Z_Construct_UEnum_UnrealToUnity_FoliageExportType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_FoliageExportType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_FoliageExportType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UnrealToUnity_FoliageExportType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UnrealToUnity_FoliageExportType()
{
	if (!Z_Registration_Info_UEnum_FoliageExportType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_FoliageExportType.InnerSingleton, Z_Construct_UEnum_UnrealToUnity_FoliageExportType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_FoliageExportType.InnerSingleton;
}
// ********** End Enum FoliageExportType ***********************************************************

// ********** Begin Enum EngineType ****************************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EngineType;
static UEnum* EngineType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EngineType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EngineType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UnrealToUnity_EngineType, (UObject*)Z_Construct_UPackage__Script_UnrealToUnity(), TEXT("EngineType"));
	}
	return Z_Registration_Info_UEnum_EngineType.OuterSingleton;
}
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<EngineType>()
{
	return EngineType_StaticEnum();
}
struct Z_Construct_UEnum_UnrealToUnity_EngineType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "DEFAULT.Hidden", "" },
		{ "DEFAULT.Name", "EngineType::DEFAULT" },
		{ "GODOT.DisplayName", "Godot" },
		{ "GODOT.Name", "EngineType::GODOT" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
		{ "UNITY.DisplayName", "Unity" },
		{ "UNITY.Name", "EngineType::UNITY" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EngineType::DEFAULT", (int64)EngineType::DEFAULT },
		{ "EngineType::UNITY", (int64)EngineType::UNITY },
		{ "EngineType::GODOT", (int64)EngineType::GODOT },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_UnrealToUnity_EngineType_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UnrealToUnity_EngineType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UnrealToUnity,
	nullptr,
	"EngineType",
	"EngineType",
	Z_Construct_UEnum_UnrealToUnity_EngineType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_EngineType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_EngineType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UnrealToUnity_EngineType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UnrealToUnity_EngineType()
{
	if (!Z_Registration_Info_UEnum_EngineType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EngineType.InnerSingleton, Z_Construct_UEnum_UnrealToUnity_EngineType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EngineType.InnerSingleton;
}
// ********** End Enum EngineType ******************************************************************

// ********** Begin Enum UnityShadersType **********************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_UnityShadersType;
static UEnum* UnityShadersType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_UnityShadersType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_UnityShadersType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UnrealToUnity_UnityShadersType, (UObject*)Z_Construct_UPackage__Script_UnrealToUnity(), TEXT("UnityShadersType"));
	}
	return Z_Registration_Info_UEnum_UnityShadersType.OuterSingleton;
}
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<UnityShadersType>()
{
	return UnityShadersType_StaticEnum();
}
struct Z_Construct_UEnum_UnrealToUnity_UnityShadersType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/BaseScene.h" },
		{ "ST_CUSTOM_SHADERS.DisplayName", "CustomShaders(designed for accuracy)" },
		{ "ST_CUSTOM_SHADERS.Name", "UnityShadersType::ST_CUSTOM_SHADERS" },
		{ "ST_STANDARD_SHADERS.DisplayName", "Standard Shaders" },
		{ "ST_STANDARD_SHADERS.Name", "UnityShadersType::ST_STANDARD_SHADERS" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "UnityShadersType::ST_CUSTOM_SHADERS", (int64)UnityShadersType::ST_CUSTOM_SHADERS },
		{ "UnityShadersType::ST_STANDARD_SHADERS", (int64)UnityShadersType::ST_STANDARD_SHADERS },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_UnrealToUnity_UnityShadersType_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UnrealToUnity_UnityShadersType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UnrealToUnity,
	nullptr,
	"UnityShadersType",
	"UnityShadersType",
	Z_Construct_UEnum_UnrealToUnity_UnityShadersType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_UnityShadersType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UnrealToUnity_UnityShadersType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UnrealToUnity_UnityShadersType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UnrealToUnity_UnityShadersType()
{
	if (!Z_Registration_Info_UEnum_UnityShadersType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_UnityShadersType.InnerSingleton, Z_Construct_UEnum_UnrealToUnity_UnityShadersType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_UnityShadersType.InnerSingleton;
}
// ********** End Enum UnityShadersType ************************************************************

// ********** Begin Class UBaseSettingsClass *******************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UBaseSettingsClass;
UClass* UBaseSettingsClass::GetPrivateStaticClass()
{
	using TClass = UBaseSettingsClass;
	if (!Z_Registration_Info_UClass_UBaseSettingsClass.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("BaseSettingsClass"),
			Z_Registration_Info_UClass_UBaseSettingsClass.InnerSingleton,
			StaticRegisterNativesUBaseSettingsClass,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_UBaseSettingsClass.InnerSingleton;
}
UClass* Z_Construct_UClass_UBaseSettingsClass_NoRegister()
{
	return UBaseSettingsClass::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UBaseSettingsClass_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "BaseScene.h" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OutputDirectory_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SkinnedMeshes_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NaniteOptions_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExportVertexShader_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExportBrushes_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExportPhysics_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExportTextures_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExportAllSounds_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/BaseScene.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UBaseSettingsClass constinit property declarations ***********************
	static const UECodeGen_Private::FStrPropertyParams NewProp_OutputDirectory;
	static const UECodeGen_Private::FBytePropertyParams NewProp_SkinnedMeshes_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_SkinnedMeshes;
	static const UECodeGen_Private::FBytePropertyParams NewProp_NaniteOptions_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_NaniteOptions;
	static void NewProp_ExportVertexShader_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExportVertexShader;
	static void NewProp_ExportBrushes_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExportBrushes;
	static void NewProp_ExportPhysics_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExportPhysics;
	static void NewProp_ExportTextures_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExportTextures;
	static void NewProp_ExportAllSounds_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExportAllSounds;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UBaseSettingsClass constinit property declarations *************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UBaseSettingsClass>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UBaseSettingsClass_Statics

// ********** Begin Class UBaseSettingsClass Property Definitions **********************************
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_OutputDirectory = { "OutputDirectory", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UBaseSettingsClass, OutputDirectory), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OutputDirectory_MetaData), NewProp_OutputDirectory_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_SkinnedMeshes_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_SkinnedMeshes = { "SkinnedMeshes", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UBaseSettingsClass, SkinnedMeshes), Z_Construct_UEnum_UnrealToUnity_SkinnedMeshOptionsEnum, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SkinnedMeshes_MetaData), NewProp_SkinnedMeshes_MetaData) }; // 2460002844
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_NaniteOptions_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_NaniteOptions = { "NaniteOptions", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UBaseSettingsClass, NaniteOptions), Z_Construct_UEnum_UnrealToUnity_NaniteOptionsEnum, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NaniteOptions_MetaData), NewProp_NaniteOptions_MetaData) }; // 1776056675
void Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportVertexShader_SetBit(void* Obj)
{
	((UBaseSettingsClass*)Obj)->ExportVertexShader = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportVertexShader = { "ExportVertexShader", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UBaseSettingsClass), &Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportVertexShader_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExportVertexShader_MetaData), NewProp_ExportVertexShader_MetaData) };
void Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportBrushes_SetBit(void* Obj)
{
	((UBaseSettingsClass*)Obj)->ExportBrushes = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportBrushes = { "ExportBrushes", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UBaseSettingsClass), &Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportBrushes_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExportBrushes_MetaData), NewProp_ExportBrushes_MetaData) };
void Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportPhysics_SetBit(void* Obj)
{
	((UBaseSettingsClass*)Obj)->ExportPhysics = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportPhysics = { "ExportPhysics", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UBaseSettingsClass), &Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportPhysics_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExportPhysics_MetaData), NewProp_ExportPhysics_MetaData) };
void Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportTextures_SetBit(void* Obj)
{
	((UBaseSettingsClass*)Obj)->ExportTextures = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportTextures = { "ExportTextures", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UBaseSettingsClass), &Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportTextures_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExportTextures_MetaData), NewProp_ExportTextures_MetaData) };
void Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportAllSounds_SetBit(void* Obj)
{
	((UBaseSettingsClass*)Obj)->ExportAllSounds = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportAllSounds = { "ExportAllSounds", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UBaseSettingsClass), &Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportAllSounds_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExportAllSounds_MetaData), NewProp_ExportAllSounds_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UBaseSettingsClass_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_OutputDirectory,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_SkinnedMeshes_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_SkinnedMeshes,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_NaniteOptions_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_NaniteOptions,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportVertexShader,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportBrushes,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportPhysics,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportTextures,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBaseSettingsClass_Statics::NewProp_ExportAllSounds,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UBaseSettingsClass_Statics::PropPointers) < 2048);
// ********** End Class UBaseSettingsClass Property Definitions ************************************
UObject* (*const Z_Construct_UClass_UBaseSettingsClass_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_UnrealToUnity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UBaseSettingsClass_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UBaseSettingsClass_Statics::ClassParams = {
	&UBaseSettingsClass::StaticClass,
	"EditorPerProjectUserSettings",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UBaseSettingsClass_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UBaseSettingsClass_Statics::PropPointers),
	0,
	0x000800A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UBaseSettingsClass_Statics::Class_MetaDataParams), Z_Construct_UClass_UBaseSettingsClass_Statics::Class_MetaDataParams)
};
void UBaseSettingsClass::StaticRegisterNativesUBaseSettingsClass()
{
}
UClass* Z_Construct_UClass_UBaseSettingsClass()
{
	if (!Z_Registration_Info_UClass_UBaseSettingsClass.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UBaseSettingsClass.OuterSingleton, Z_Construct_UClass_UBaseSettingsClass_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UBaseSettingsClass.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UBaseSettingsClass);
UBaseSettingsClass::~UBaseSettingsClass() {}
// ********** End Class UBaseSettingsClass *********************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h__Script_UnrealToUnity_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ RenderPipeline_StaticEnum, TEXT("RenderPipeline"), &Z_Registration_Info_UEnum_RenderPipeline, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1971503897U) },
		{ SkinnedMeshOptionsEnum_StaticEnum, TEXT("SkinnedMeshOptionsEnum"), &Z_Registration_Info_UEnum_SkinnedMeshOptionsEnum, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2460002844U) },
		{ NaniteOptionsEnum_StaticEnum, TEXT("NaniteOptionsEnum"), &Z_Registration_Info_UEnum_NaniteOptionsEnum, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1776056675U) },
		{ TangentToWorldLocationType_StaticEnum, TEXT("TangentToWorldLocationType"), &Z_Registration_Info_UEnum_TangentToWorldLocationType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2651763092U) },
		{ FoliageExportType_StaticEnum, TEXT("FoliageExportType"), &Z_Registration_Info_UEnum_FoliageExportType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 400690577U) },
		{ EngineType_StaticEnum, TEXT("EngineType"), &Z_Registration_Info_UEnum_EngineType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2764193560U) },
		{ UnityShadersType_StaticEnum, TEXT("UnityShadersType"), &Z_Registration_Info_UEnum_UnityShadersType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3186265630U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UBaseSettingsClass, UBaseSettingsClass::StaticClass, TEXT("UBaseSettingsClass"), &Z_Registration_Info_UClass_UBaseSettingsClass, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UBaseSettingsClass), 3828979154U) },
	};
}; // Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h__Script_UnrealToUnity_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h__Script_UnrealToUnity_661946159{
	TEXT("/Script/UnrealToUnity"),
	Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h__Script_UnrealToUnity_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h__Script_UnrealToUnity_Statics::ClassInfo),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h__Script_UnrealToUnity_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h__Script_UnrealToUnity_Statics::EnumInfo),
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
