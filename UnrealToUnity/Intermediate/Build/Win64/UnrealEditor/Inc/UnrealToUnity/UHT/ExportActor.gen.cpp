// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ExportActor.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeExportActor() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_AActor();
UNREALTOUNITY_API UClass* Z_Construct_UClass_AExportActor();
UNREALTOUNITY_API UClass* Z_Construct_UClass_AExportActor_NoRegister();
UNREALTOUNITY_API UEnum* Z_Construct_UEnum_UnrealToUnity_RenderPipeline();
UPackage* Z_Construct_UPackage__Script_UnrealToUnity();
// ********** End Cross Module References **********************************************************

// ********** Begin Class AExportActor Function EventExport ****************************************
struct Z_Construct_UFunction_AExportActor_EventExport_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/ExportActor.h" },
	};
#endif // WITH_METADATA

// ********** Begin Function EventExport constinit property declarations ***************************
// ********** End Function EventExport constinit property declarations *****************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_AExportActor_EventExport_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_AExportActor, nullptr, "EventExport", 	nullptr, 
	0, 
0,
RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_AExportActor_EventExport_Statics::Function_MetaDataParams), Z_Construct_UFunction_AExportActor_EventExport_Statics::Function_MetaDataParams)},  };
UFunction* Z_Construct_UFunction_AExportActor_EventExport()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_AExportActor_EventExport_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(AExportActor::execEventExport)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->EventExport();
	P_NATIVE_END;
}
// ********** End Class AExportActor Function EventExport ******************************************

// ********** Begin Class AExportActor *************************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_AExportActor;
UClass* AExportActor::GetPrivateStaticClass()
{
	using TClass = AExportActor;
	if (!Z_Registration_Info_UClass_AExportActor.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("ExportActor"),
			Z_Registration_Info_UClass_AExportActor.InnerSingleton,
			StaticRegisterNativesAExportActor,
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
	return Z_Registration_Info_UClass_AExportActor.InnerSingleton;
}
UClass* Z_Construct_UClass_AExportActor_NoRegister()
{
	return AExportActor::GetPrivateStaticClass();
}
struct Z_Construct_UClass_AExportActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "ExportActor.h" },
		{ "ModuleRelativePath", "Public/ExportActor.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Pipeline_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/ExportActor.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OutputFolder_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/ExportActor.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class AExportActor constinit property declarations *****************************
	static const UECodeGen_Private::FBytePropertyParams NewProp_Pipeline_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Pipeline;
	static const UECodeGen_Private::FStrPropertyParams NewProp_OutputFolder;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class AExportActor constinit property declarations *******************************
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("EventExport"), .Pointer = &AExportActor::execEventExport },
	};
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_AExportActor_EventExport, "EventExport" }, // 1339181074
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AExportActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_AExportActor_Statics

// ********** Begin Class AExportActor Property Definitions ****************************************
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_AExportActor_Statics::NewProp_Pipeline_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_AExportActor_Statics::NewProp_Pipeline = { "Pipeline", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AExportActor, Pipeline), Z_Construct_UEnum_UnrealToUnity_RenderPipeline, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Pipeline_MetaData), NewProp_Pipeline_MetaData) }; // 1971503897
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_AExportActor_Statics::NewProp_OutputFolder = { "OutputFolder", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AExportActor, OutputFolder), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OutputFolder_MetaData), NewProp_OutputFolder_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AExportActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AExportActor_Statics::NewProp_Pipeline_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AExportActor_Statics::NewProp_Pipeline,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AExportActor_Statics::NewProp_OutputFolder,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AExportActor_Statics::PropPointers) < 2048);
// ********** End Class AExportActor Property Definitions ******************************************
UObject* (*const Z_Construct_UClass_AExportActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UnrealToUnity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AExportActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AExportActor_Statics::ClassParams = {
	&AExportActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	Z_Construct_UClass_AExportActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(Z_Construct_UClass_AExportActor_Statics::PropPointers),
	0,
	0x008000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AExportActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AExportActor_Statics::Class_MetaDataParams)
};
void AExportActor::StaticRegisterNativesAExportActor()
{
	UClass* Class = AExportActor::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, MakeConstArrayView(Z_Construct_UClass_AExportActor_Statics::Funcs));
}
UClass* Z_Construct_UClass_AExportActor()
{
	if (!Z_Registration_Info_UClass_AExportActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AExportActor.OuterSingleton, Z_Construct_UClass_AExportActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AExportActor.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AExportActor);
AExportActor::~AExportActor() {}
// ********** End Class AExportActor ***************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h__Script_UnrealToUnity_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AExportActor, AExportActor::StaticClass, TEXT("AExportActor"), &Z_Registration_Info_UClass_AExportActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AExportActor), 178872650U) },
	};
}; // Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h__Script_UnrealToUnity_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h__Script_UnrealToUnity_1225590783{
	TEXT("/Script/UnrealToUnity"),
	Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h__Script_UnrealToUnity_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h__Script_UnrealToUnity_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
