// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeUnrealToUnity_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_UnrealToUnity;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_UnrealToUnity()
	{
		if (!Z_Registration_Info_UPackage__Script_UnrealToUnity.OuterSingleton)
		{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/UnrealToUnity",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000040,
			0xC00AFCB2,
			0xE1A0D9F9,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_UnrealToUnity.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_UnrealToUnity.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_UnrealToUnity(Z_Construct_UPackage__Script_UnrealToUnity, TEXT("/Script/UnrealToUnity"), Z_Registration_Info_UPackage__Script_UnrealToUnity, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0xC00AFCB2, 0xE1A0D9F9));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
