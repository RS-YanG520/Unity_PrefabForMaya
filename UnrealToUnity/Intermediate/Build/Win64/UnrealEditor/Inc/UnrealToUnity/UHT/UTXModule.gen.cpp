// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "UTXModule.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeUTXModule() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
UNREALTOUNITY_API UClass* Z_Construct_UClass_UBakeTextureSettingsClass();
UNREALTOUNITY_API UClass* Z_Construct_UClass_UBakeTextureSettingsClass_NoRegister();
UNREALTOUNITY_API UClass* Z_Construct_UClass_UBaseSettingsClass();
UNREALTOUNITY_API UClass* Z_Construct_UClass_UUnitySettingsClass();
UNREALTOUNITY_API UClass* Z_Construct_UClass_UUnitySettingsClass_NoRegister();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_FoliageExportType();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_RenderPipeline();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_UnityShadersType();
UPackage* Z_Construct_UPackage__Script_UnrealToUnity();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UUnitySettingsClass ******************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UUnitySettingsClass;
UClass* UUnitySettingsClass::GetPrivateStaticClass()
{
	using TClass = UUnitySettingsClass;
	if (!Z_Registration_Info_UClass_UUnitySettingsClass.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("UnitySettingsClass"),
			Z_Registration_Info_UClass_UUnitySettingsClass.InnerSingleton,
			StaticRegisterNativesUUnitySettingsClass,
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
	return Z_Registration_Info_UClass_UUnitySettingsClass.InnerSingleton;
}
UClass* Z_Construct_UClass_UUnitySettingsClass_NoRegister()
{
	return UUnitySettingsClass::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UUnitySettingsClass_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UTXModule.h" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ScriptableRenderPipeline_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Shaders_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExposeMaterialParameters_MetaData[] = {
		{ "Category", "Parameters" },
		{ "DisplayName", "ExposeParameters(experimental,requires UE5.4+)" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ShowFBXExportDialog_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExportLODs_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExportVertexColors_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_TangentToWorldLocation_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExportAllAnimations_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FoliageExport_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_UsePrefabs_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UUnitySettingsClass constinit property declarations **********************
	static const UECodeGen_Private::FBytePropertyParams NewProp_ScriptableRenderPipeline_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ScriptableRenderPipeline;
	static const UECodeGen_Private::FBytePropertyParams NewProp_Shaders_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Shaders;
	static void NewProp_ExposeMaterialParameters_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExposeMaterialParameters;
	static void NewProp_ShowFBXExportDialog_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ShowFBXExportDialog;
	static void NewProp_ExportLODs_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExportLODs;
	static void NewProp_ExportVertexColors_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExportVertexColors;
	static const UECodeGen_Private::FBytePropertyParams NewProp_TangentToWorldLocation_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_TangentToWorldLocation;
	static void NewProp_ExportAllAnimations_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ExportAllAnimations;
	static const UECodeGen_Private::FBytePropertyParams NewProp_FoliageExport_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_FoliageExport;
	static void NewProp_UsePrefabs_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_UsePrefabs;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UUnitySettingsClass constinit property declarations ************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UUnitySettingsClass>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UUnitySettingsClass_Statics

// ********** Begin Class UUnitySettingsClass Property Definitions *********************************
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ScriptableRenderPipeline_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ScriptableRenderPipeline = { "ScriptableRenderPipeline", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UUnitySettingsClass, ScriptableRenderPipeline), Z_Construct_UEnum_UnrealToUnity_RenderPipeline, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ScriptableRenderPipeline_MetaData), NewProp_ScriptableRenderPipeline_MetaData) }; // 1971503897
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_Shaders_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_Shaders = { "Shaders", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UUnitySettingsClass, Shaders), Z_Construct_UEnum_UnrealToUnity_UnityShadersType, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Shaders_MetaData), NewProp_Shaders_MetaData) }; // 3186265630
void Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExposeMaterialParameters_SetBit(void* Obj)
{
	((UUnitySettingsClass*)Obj)->ExposeMaterialParameters = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExposeMaterialParameters = { "ExposeMaterialParameters", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UUnitySettingsClass), &Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExposeMaterialParameters_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExposeMaterialParameters_MetaData), NewProp_ExposeMaterialParameters_MetaData) };
void Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ShowFBXExportDialog_SetBit(void* Obj)
{
	((UUnitySettingsClass*)Obj)->ShowFBXExportDialog = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ShowFBXExportDialog = { "ShowFBXExportDialog", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UUnitySettingsClass), &Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ShowFBXExportDialog_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ShowFBXExportDialog_MetaData), NewProp_ShowFBXExportDialog_MetaData) };
void Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportLODs_SetBit(void* Obj)
{
	((UUnitySettingsClass*)Obj)->ExportLODs = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportLODs = { "ExportLODs", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UUnitySettingsClass), &Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportLODs_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExportLODs_MetaData), NewProp_ExportLODs_MetaData) };
void Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportVertexColors_SetBit(void* Obj)
{
	((UUnitySettingsClass*)Obj)->ExportVertexColors = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportVertexColors = { "ExportVertexColors", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UUnitySettingsClass), &Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportVertexColors_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExportVertexColors_MetaData), NewProp_ExportVertexColors_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_TangentToWorldLocation_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_TangentToWorldLocation = { "TangentToWorldLocation", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UUnitySettingsClass, TangentToWorldLocation), Z_Construct_UEnum_UnrealToUnity_TangentToWorldLocationType, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_TangentToWorldLocation_MetaData), NewProp_TangentToWorldLocation_MetaData) }; // 2651763092
void Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportAllAnimations_SetBit(void* Obj)
{
	((UUnitySettingsClass*)Obj)->ExportAllAnimations = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportAllAnimations = { "ExportAllAnimations", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UUnitySettingsClass), &Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportAllAnimations_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExportAllAnimations_MetaData), NewProp_ExportAllAnimations_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_FoliageExport_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_FoliageExport = { "FoliageExport", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UUnitySettingsClass, FoliageExport), Z_Construct_UEnum_UnrealToUnity_FoliageExportType, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FoliageExport_MetaData), NewProp_FoliageExport_MetaData) }; // 400690577
void Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_UsePrefabs_SetBit(void* Obj)
{
	((UUnitySettingsClass*)Obj)->UsePrefabs = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_UsePrefabs = { "UsePrefabs", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UUnitySettingsClass), &Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_UsePrefabs_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_UsePrefabs_MetaData), NewProp_UsePrefabs_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UUnitySettingsClass_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ScriptableRenderPipeline_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ScriptableRenderPipeline,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_Shaders_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_Shaders,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExposeMaterialParameters,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ShowFBXExportDialog,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportLODs,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportVertexColors,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_TangentToWorldLocation_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_TangentToWorldLocation,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_ExportAllAnimations,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_FoliageExport_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_FoliageExport,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UUnitySettingsClass_Statics::NewProp_UsePrefabs,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UUnitySettingsClass_Statics::PropPointers) < 2048);
// ********** End Class UUnitySettingsClass Property Definitions ***********************************
UObject* (*const Z_Construct_UClass_UUnitySettingsClass_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UBaseSettingsClass,
	(UObject* (*)())Z_Construct_UPackage__Script_UnrealToUnity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UUnitySettingsClass_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UUnitySettingsClass_Statics::ClassParams = {
	&UUnitySettingsClass::StaticClass,
	"EditorPerProjectUserSettings",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UUnitySettingsClass_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UUnitySettingsClass_Statics::PropPointers),
	0,
	0x000800A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UUnitySettingsClass_Statics::Class_MetaDataParams), Z_Construct_UClass_UUnitySettingsClass_Statics::Class_MetaDataParams)
};
void UUnitySettingsClass::StaticRegisterNativesUUnitySettingsClass()
{
}
UClass* Z_Construct_UClass_UUnitySettingsClass()
{
	if (!Z_Registration_Info_UClass_UUnitySettingsClass.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UUnitySettingsClass.OuterSingleton, Z_Construct_UClass_UUnitySettingsClass_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UUnitySettingsClass.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UUnitySettingsClass);
UUnitySettingsClass::~UUnitySettingsClass() {}
// ********** End Class UUnitySettingsClass ********************************************************

// ********** Begin Class UBakeTextureSettingsClass ************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UBakeTextureSettingsClass;
UClass* UBakeTextureSettingsClass::GetPrivateStaticClass()
{
	using TClass = UBakeTextureSettingsClass;
	if (!Z_Registration_Info_UClass_UBakeTextureSettingsClass.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("BakeTextureSettingsClass"),
			Z_Registration_Info_UClass_UBakeTextureSettingsClass.InnerSingleton,
			StaticRegisterNativesUBakeTextureSettingsClass,
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
	return Z_Registration_Info_UClass_UBakeTextureSettingsClass.InnerSingleton;
}
UClass* Z_Construct_UClass_UBakeTextureSettingsClass_NoRegister()
{
	return UBakeTextureSettingsClass::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UBakeTextureSettingsClass_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UTXModule.h" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ScriptableRenderPipeline_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_UVChannel_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_TargetResolution_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LOD_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_UseMeshData_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OneTextureSetPerMaterial_MetaData[] = {
		{ "Category", "Parameters" },
		{ "ModuleRelativePath", "Public/UTXModule.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UBakeTextureSettingsClass constinit property declarations ****************
	static const UECodeGen_Private::FBytePropertyParams NewProp_ScriptableRenderPipeline_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ScriptableRenderPipeline;
	static const UECodeGen_Private::FIntPropertyParams NewProp_UVChannel;
	static const UECodeGen_Private::FIntPropertyParams NewProp_TargetResolution;
	static const UECodeGen_Private::FIntPropertyParams NewProp_LOD;
	static void NewProp_UseMeshData_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_UseMeshData;
	static void NewProp_OneTextureSetPerMaterial_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_OneTextureSetPerMaterial;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UBakeTextureSettingsClass constinit property declarations ******************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UBakeTextureSettingsClass>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UBakeTextureSettingsClass_Statics

// ********** Begin Class UBakeTextureSettingsClass Property Definitions ***************************
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_ScriptableRenderPipeline_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_ScriptableRenderPipeline = { "ScriptableRenderPipeline", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UBakeTextureSettingsClass, ScriptableRenderPipeline), Z_Construct_UEnum_UnrealToUnity_RenderPipeline, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ScriptableRenderPipeline_MetaData), NewProp_ScriptableRenderPipeline_MetaData) }; // 1971503897
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_UVChannel = { "UVChannel", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UBakeTextureSettingsClass, UVChannel), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_UVChannel_MetaData), NewProp_UVChannel_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_TargetResolution = { "TargetResolution", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UBakeTextureSettingsClass, TargetResolution), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_TargetResolution_MetaData), NewProp_TargetResolution_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_LOD = { "LOD", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UBakeTextureSettingsClass, LOD), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LOD_MetaData), NewProp_LOD_MetaData) };
void Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_UseMeshData_SetBit(void* Obj)
{
	((UBakeTextureSettingsClass*)Obj)->UseMeshData = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_UseMeshData = { "UseMeshData", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UBakeTextureSettingsClass), &Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_UseMeshData_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_UseMeshData_MetaData), NewProp_UseMeshData_MetaData) };
void Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_OneTextureSetPerMaterial_SetBit(void* Obj)
{
	((UBakeTextureSettingsClass*)Obj)->OneTextureSetPerMaterial = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_OneTextureSetPerMaterial = { "OneTextureSetPerMaterial", nullptr, (EPropertyFlags)0x0010000000004005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UBakeTextureSettingsClass), &Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_OneTextureSetPerMaterial_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OneTextureSetPerMaterial_MetaData), NewProp_OneTextureSetPerMaterial_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UBakeTextureSettingsClass_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_ScriptableRenderPipeline_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_ScriptableRenderPipeline,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_UVChannel,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_TargetResolution,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_LOD,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_UseMeshData,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBakeTextureSettingsClass_Statics::NewProp_OneTextureSetPerMaterial,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UBakeTextureSettingsClass_Statics::PropPointers) < 2048);
// ********** End Class UBakeTextureSettingsClass Property Definitions *****************************
UObject* (*const Z_Construct_UClass_UBakeTextureSettingsClass_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_UnrealToUnity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UBakeTextureSettingsClass_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UBakeTextureSettingsClass_Statics::ClassParams = {
	&UBakeTextureSettingsClass::StaticClass,
	"EditorPerProjectUserSettings",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UBakeTextureSettingsClass_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UBakeTextureSettingsClass_Statics::PropPointers),
	0,
	0x000800A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UBakeTextureSettingsClass_Statics::Class_MetaDataParams), Z_Construct_UClass_UBakeTextureSettingsClass_Statics::Class_MetaDataParams)
};
void UBakeTextureSettingsClass::StaticRegisterNativesUBakeTextureSettingsClass()
{
}
UClass* Z_Construct_UClass_UBakeTextureSettingsClass()
{
	if (!Z_Registration_Info_UClass_UBakeTextureSettingsClass.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UBakeTextureSettingsClass.OuterSingleton, Z_Construct_UClass_UBakeTextureSettingsClass_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UBakeTextureSettingsClass.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UBakeTextureSettingsClass);
UBakeTextureSettingsClass::~UBakeTextureSettingsClass() {}
// ********** End Class UBakeTextureSettingsClass **************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_UTXModule_h__Script_UnrealToUnity_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UUnitySettingsClass, UUnitySettingsClass::StaticClass, TEXT("UUnitySettingsClass"), &Z_Registration_Info_UClass_UUnitySettingsClass, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UUnitySettingsClass), 1446622103U) },
		{ Z_Construct_UClass_UBakeTextureSettingsClass, UBakeTextureSettingsClass::StaticClass, TEXT("UBakeTextureSettingsClass"), &Z_Registration_Info_UClass_UBakeTextureSettingsClass, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UBakeTextureSettingsClass), 703760972U) },
	};
}; // Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_UTXModule_h__Script_UnrealToUnity_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_UTXModule_h__Script_UnrealToUnity_985688337{
	TEXT("/Script/UnrealToUnity"),
	Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_UTXModule_h__Script_UnrealToUnity_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_UTXModule_h__Script_UnrealToUnity_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
