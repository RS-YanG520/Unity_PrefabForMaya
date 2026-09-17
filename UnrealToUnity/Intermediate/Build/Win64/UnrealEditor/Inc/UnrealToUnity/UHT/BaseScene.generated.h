// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "BaseScene.h"

#ifdef UNREALTOUNITY_BaseScene_generated_h
#error "BaseScene.generated.h already included, missing '#pragma once' in BaseScene.h"
#endif
#define UNREALTOUNITY_BaseScene_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class UBaseSettingsClass *******************************************************
struct Z_Construct_UClass_UBaseSettingsClass_Statics;
UNREALTOUNITY_API UClass* Z_Construct_UClass_UBaseSettingsClass_NoRegister();

#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h_602_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUBaseSettingsClass(); \
	friend struct ::Z_Construct_UClass_UBaseSettingsClass_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend UNREALTOUNITY_API UClass* ::Z_Construct_UClass_UBaseSettingsClass_NoRegister(); \
public: \
	DECLARE_CLASS2(UBaseSettingsClass, UObject, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UnrealToUnity"), Z_Construct_UClass_UBaseSettingsClass_NoRegister) \
	DECLARE_SERIALIZER(UBaseSettingsClass) \
	static constexpr const TCHAR* StaticConfigName() {return TEXT("EditorPerProjectUserSettings");} \



#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h_602_ENHANCED_CONSTRUCTORS \
	/** Deleted move- and copy-constructors, should never be used */ \
	UBaseSettingsClass(UBaseSettingsClass&&) = delete; \
	UBaseSettingsClass(const UBaseSettingsClass&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(UNREALTOUNITY_API, UBaseSettingsClass); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UBaseSettingsClass); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(UBaseSettingsClass) \
	UNREALTOUNITY_API virtual ~UBaseSettingsClass();


#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h_599_PROLOG
#define FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h_602_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h_602_INCLASS_NO_PURE_DECLS \
	FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h_602_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UBaseSettingsClass;

// ********** End Class UBaseSettingsClass *********************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_UnrealProjects_Test5_7_Plugins_UnrealToUnity_Source_UnrealToUnity_Public_BaseScene_h

// ********** Begin Enum RenderPipeline ************************************************************
#define FOREACH_ENUM_RENDERPIPELINE(op) \
	op(RenderPipeline::RP_BUILTIN) \
	op(RenderPipeline::RP_URP) \
	op(RenderPipeline::RP_HDRP) 

enum class RenderPipeline : uint8;
template<> struct TIsUEnumClass<RenderPipeline> { enum { Value = true }; };
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<RenderPipeline>();
// ********** End Enum RenderPipeline **************************************************************

// ********** Begin Enum SkinnedMeshOptionsEnum ****************************************************
#define FOREACH_ENUM_SKINNEDMESHOPTIONSENUM(op) \
	op(SkinnedMeshOptionsEnum::SMO_EXPORT_AS_STATIC) \
	op(SkinnedMeshOptionsEnum::SMO_EXPORT_AS_SKINNED) 

enum class SkinnedMeshOptionsEnum : uint8;
template<> struct TIsUEnumClass<SkinnedMeshOptionsEnum> { enum { Value = true }; };
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<SkinnedMeshOptionsEnum>();
// ********** End Enum SkinnedMeshOptionsEnum ******************************************************

// ********** Begin Enum NaniteOptionsEnum *********************************************************
#define FOREACH_ENUM_NANITEOPTIONSENUM(op) \
	op(NaniteOptionsEnum::NANITE_COARSE_MESH) \
	op(NaniteOptionsEnum::NANITE_ORIGINAL_MESH) 

enum class NaniteOptionsEnum : uint8;
template<> struct TIsUEnumClass<NaniteOptionsEnum> { enum { Value = true }; };
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<NaniteOptionsEnum>();
// ********** End Enum NaniteOptionsEnum ***********************************************************

// ********** Begin Enum TangentToWorldLocationType ************************************************
#define FOREACH_ENUM_TANGENTTOWORLDLOCATIONTYPE(op) \
	op(TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS) \
	op(TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS_NO_WORLDNORMALCOPY) \
	op(TangentToWorldLocationType::TTWL_BEFORE_NORMAL_CALCULATION) \
	op(TangentToWorldLocationType::TTWL_AFTER_NORMAL_CALCULATION) 

enum class TangentToWorldLocationType : uint8;
template<> struct TIsUEnumClass<TangentToWorldLocationType> { enum { Value = true }; };
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<TangentToWorldLocationType>();
// ********** End Enum TangentToWorldLocationType **************************************************

// ********** Begin Enum FoliageExportType *********************************************************
#define FOREACH_ENUM_FOLIAGEEXPORTTYPE(op) \
	op(FoliageExportType::FET_CREATE_GAMEOBJECTS) \
	op(FoliageExportType::FET_USE_TREEINSTANCES) 

enum class FoliageExportType : uint8;
template<> struct TIsUEnumClass<FoliageExportType> { enum { Value = true }; };
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<FoliageExportType>();
// ********** End Enum FoliageExportType ***********************************************************

// ********** Begin Enum EngineType ****************************************************************
#define FOREACH_ENUM_ENGINETYPE(op) \
	op(EngineType::DEFAULT) \
	op(EngineType::UNITY) \
	op(EngineType::GODOT) 

enum class EngineType : uint8;
template<> struct TIsUEnumClass<EngineType> { enum { Value = true }; };
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<EngineType>();
// ********** End Enum EngineType ******************************************************************

// ********** Begin Enum UnityShadersType **********************************************************
#define FOREACH_ENUM_UNITYSHADERSTYPE(op) \
	op(UnityShadersType::ST_CUSTOM_SHADERS) \
	op(UnityShadersType::ST_STANDARD_SHADERS) 

enum class UnityShadersType : uint8;
template<> struct TIsUEnumClass<UnityShadersType> { enum { Value = true }; };
template<> UNREALTOUNITY_NON_ATTRIBUTED_API UEnum* StaticEnum<UnityShadersType>();
// ********** End Enum UnityShadersType ************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
