// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "ExportActor.h"

#ifdef UNREALTOUNITY_ExportActor_generated_h
#error "ExportActor.generated.h already included, missing '#pragma once' in ExportActor.h"
#endif
#define UNREALTOUNITY_ExportActor_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class AExportActor *************************************************************
#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h_16_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execEventExport);


struct Z_Construct_UClass_AExportActor_Statics;
UNREALTOUNITY_API UClass* Z_Construct_UClass_AExportActor_NoRegister();

#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h_16_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAExportActor(); \
	friend struct ::Z_Construct_UClass_AExportActor_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend UNREALTOUNITY_API UClass* ::Z_Construct_UClass_AExportActor_NoRegister(); \
public: \
	DECLARE_CLASS2(AExportActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UnrealToUnity"), Z_Construct_UClass_AExportActor_NoRegister) \
	DECLARE_SERIALIZER(AExportActor)


#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h_16_ENHANCED_CONSTRUCTORS \
	/** Deleted move- and copy-constructors, should never be used */ \
	AExportActor(AExportActor&&) = delete; \
	AExportActor(const AExportActor&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AExportActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AExportActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AExportActor) \
	NO_API virtual ~AExportActor();


#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h_13_PROLOG
#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h_16_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h_16_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h_16_INCLASS_NO_PURE_DECLS \
	FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h_16_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class AExportActor;

// ********** End Class AExportActor ***************************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_ExportActor_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
