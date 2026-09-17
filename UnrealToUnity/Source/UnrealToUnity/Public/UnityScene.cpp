// Copyright Ciprian Stanciu 2024

#include "UnityScene.h"
#include "ExportToUnity.h"
#ifdef ENABLE_GODOT_EXPORT
	#include "GodotScene.h"
#endif
#include "UTXModule.h"

#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/DirectionalLightComponent.h"

#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SplineMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Runtime/Engine/Public/Rendering/SkeletalMeshRenderData.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Misc/ScopedSlowTask.h"
#include "Animation/AnimSequence.h"
#include "Misc/MessageDialog.h"
#include "Engine/Blueprint.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	#include "Runtime/Engine/Public/StaticMeshComponentLODInfo.h"
	#include "Animation/BuiltInAttributeTypes.h"
#endif

#if PLATFORM_WINDOWS
#else
//#define snprintf sprintf
#endif
#if WITH_EDITOR

std::string NullGUID = "0000000000000000f000000000000000";

#ifdef ENABLE_UNITY_EXPORT

#include "Editor/UnrealEd/Private/FbxExporter.h"
#include "Editor/UnrealEd/Classes/Exporters/FbxExportOption.h"
#include "Misc/FeedbackContext.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Light.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Runtime/Engine/Public/Rendering/SkeletalMeshModel.h"
#include "Runtime/CoreUObject/Public/UObject/Metadata.h"

using namespace UnFbx;
using namespace fbxsdk;

FbxManager* SdkManager = nullptr;
FbxAnimStack* AnimStack = nullptr;
FbxAnimLayer* AnimLayer = nullptr;
FbxCamera* DefaultCamera = nullptr;
//FbxScene* TheScene = nullptr;
bool bSceneGlobalTimeLineSet = false;
bool bKeepHierarchy = true;

TMap<FString, int32> FbxNodeNameToIndexMap;
TMap<const AActor*, FbxNode*> FbxActors;
TMap<const USkeletalMeshComponent*, FbxNode*> FbxSkeletonRoots;
TMap<const UMaterialInterface*, FbxSurfaceMaterial*> FbxMaterials;
TMap<const UStaticMesh*, FbxMesh*> FbxMeshes;
TMap<const UStaticMesh*, FbxMesh*> FbxCollisionMeshes;
FFbxDataConverter Converter;
void DetermineVertsToWeld( TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshLODResources& RenderMesh )
{
	const int32 VertexCount = RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts 
	VertRemap.Empty( VertexCount );
	VertRemap.AddUninitialized( VertexCount );

	// List of Unreal Verts to keep
	UniqueVerts.Empty( VertexCount );

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector, int32> HashedVerts;
	for( int32 a = 0; a < VertexCount; a++ )
	{
		const FVector& PositionA = (FVector&)RenderMesh.VertexBuffers.PositionVertexBuffer.VertexPosition( a );
		const int32* FoundIndex = HashedVerts.Find( PositionA );
		if( !FoundIndex )
		{
			int32 NewIndex = UniqueVerts.Add( a );
			VertRemap[ a ] = NewIndex;
			HashedVerts.Add( PositionA, NewIndex );
		}
		else
		{
			VertRemap[ a ] = *FoundIndex;
		}
	}
}
void DetermineUVsToWeld( TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshVertexBuffer& VertexBuffer, int32 TexCoordSourceIndex )
{
	const int32 VertexCount = VertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts
	VertRemap.Empty( VertexCount );
	VertRemap.AddUninitialized( VertexCount );

	// List of Unreal Verts to keep
	UniqueVerts.Empty( VertexCount );

	// Combine matching verts using hashed search to maintain good performance
	#if ENGINE_MAJOR_VERSION == 4
		TMap<FVector2D, int32> HashedVerts;
	#else
		TMap<FVector2f, int32> HashedVerts;
	#endif
	for( int32 Vertex = 0; Vertex < VertexCount; Vertex++ )
	{
		#if ENGINE_MAJOR_VERSION == 4
			const FVector2D& PositionA = VertexBuffer.GetVertexUV( Vertex, TexCoordSourceIndex );
		#else
			const FVector2f& PositionA = VertexBuffer.GetVertexUV( Vertex, TexCoordSourceIndex );
		#endif
		const int32* FoundIndex = HashedVerts.Find( PositionA );
		if( !FoundIndex )
		{
			int32 NewIndex = UniqueVerts.Add( Vertex );
			VertRemap[ Vertex ] = NewIndex;
			HashedVerts.Add( PositionA, NewIndex );
		}
		else
		{
			VertRemap[ Vertex ] = *FoundIndex;
		}
	}
}
FbxDouble3 SetMaterialComponent( FColorMaterialInput& MatInput, bool ToLinear )
{
	FColor RGBColor;
	FLinearColor LinearColor = FLinearColor( 1, 1, 1, 1 );
	bool LinearSet = false;

	if( MatInput.Expression )
	{
		if( Cast<UMaterialExpressionConstant>( MatInput.Expression ) )
		{
			UMaterialExpressionConstant* Expr = Cast<UMaterialExpressionConstant>( MatInput.Expression );
			RGBColor = FColor( Expr->R );
		}
		else if( Cast<UMaterialExpressionVectorParameter>( MatInput.Expression ) )
		{
			UMaterialExpressionVectorParameter* Expr = Cast<UMaterialExpressionVectorParameter>( MatInput.Expression );
			LinearColor = Expr->DefaultValue;
			LinearSet = true;
			//Linear to sRGB color space conversion
			RGBColor = Expr->DefaultValue.ToFColor( true );
		}
		else if( Cast<UMaterialExpressionConstant3Vector>( MatInput.Expression ) )
		{
			UMaterialExpressionConstant3Vector* Expr = Cast<UMaterialExpressionConstant3Vector>( MatInput.Expression );
			RGBColor.R = Expr->Constant.R;
			RGBColor.G = Expr->Constant.G;
			RGBColor.B = Expr->Constant.B;
		}
		else if( Cast<UMaterialExpressionConstant4Vector>( MatInput.Expression ) )
		{
			UMaterialExpressionConstant4Vector* Expr = Cast<UMaterialExpressionConstant4Vector>( MatInput.Expression );
			RGBColor.R = Expr->Constant.R;
			RGBColor.G = Expr->Constant.G;
			RGBColor.B = Expr->Constant.B;
			//RGBColor.A = Expr->A;
		}
		else if( Cast<UMaterialExpressionConstant2Vector>( MatInput.Expression ) )
		{
			UMaterialExpressionConstant2Vector* Expr = Cast<UMaterialExpressionConstant2Vector>( MatInput.Expression );
			RGBColor.R = Expr->R;
			RGBColor.G = Expr->G;
			RGBColor.B = 0;
		}
		else
		{
			RGBColor.R = MatInput.Constant.R;
			RGBColor.G = MatInput.Constant.G;
			RGBColor.B = MatInput.Constant.B;
		}
	}
	else
	{
		RGBColor.R = MatInput.Constant.R;
		RGBColor.G = MatInput.Constant.G;
		RGBColor.B = MatInput.Constant.B;
	}

	if( ToLinear )
	{
		if( !LinearSet )
		{
			//sRGB to linear color space conversion
			LinearColor = FLinearColor( RGBColor );
		}
		return FbxDouble3( LinearColor.R, LinearColor.G, LinearColor.B );
	}
	return FbxDouble3( RGBColor.R, RGBColor.G, RGBColor.B );
}
bool FillFbxTextureProperty( const char* PropertyName, const FExpressionInput& MaterialInput, FbxSurfaceMaterial* FbxMaterial, FbxScene* FBXScene )
{
	if( FBXScene == NULL )
	{
		return false;
	}

	FbxProperty FbxColorProperty = FbxMaterial->FindProperty( PropertyName );
	if( FbxColorProperty.IsValid() )
	{
		if( MaterialInput.IsConnected() && MaterialInput.Expression )
		{
			if( MaterialInput.Expression->IsA( UMaterialExpressionTextureSample::StaticClass() ) )
			{
				UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>( MaterialInput.Expression );
				if( TextureSample && TextureSample->Texture && TextureSample->Texture->AssetImportData )
				{
					FString TextureSourceFullPath = TextureSample->Texture->AssetImportData->GetFirstFilename();
					//Create a fbx property
					FbxFileTexture* lTexture = FbxFileTexture::Create( FBXScene, "EnvSamplerTex" );
					lTexture->SetFileName( TCHAR_TO_UTF8( *TextureSourceFullPath ) );
					lTexture->SetTextureUse( FbxTexture::eStandard );
					lTexture->SetMappingType( FbxTexture::eUV );
					lTexture->ConnectDstProperty( FbxColorProperty );
					return true;
				}
			}
		}
	}
	return false;
}
FbxSurfaceMaterial* CreateDefaultMaterial(FbxScene* FBXScene )
{
	// TODO(sbc): the below cast is needed to avoid clang warning.  The upstream
	// signature in FBX should really use 'const char *'.
	FbxSurfaceMaterial* FbxMaterial = FBXScene->GetMaterial( const_cast<char*>( "Fbx Default Material" ) );

	if( !FbxMaterial )
	{
		FbxMaterial = FbxSurfaceLambert::Create( FBXScene, "Fbx Default Material" );
		( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( FbxDouble3( 0.72, 0.72, 0.72 ) );
	}

	return FbxMaterial;
}
bool SetFBXTextureProperty( FbxSurfaceMaterial* FbxMaterial, FString TextureName, FString TextureSourceFullPath, const char* FbxPropertyName, FbxScene* InScene)
{
	FbxProperty FbxColorProperty = FbxMaterial->FindProperty( FbxPropertyName );
	if( FbxColorProperty.IsValid() )
	{
		if( !TextureSourceFullPath.IsEmpty() )
		{
			//Create a fbx property
			FbxFileTexture* lTexture = FbxFileTexture::Create( InScene, TCHAR_TO_UTF8( *TextureName ) );
			lTexture->SetFileName( TCHAR_TO_UTF8( *TextureSourceFullPath ) );
			lTexture->SetTextureUse( FbxTexture::eStandard );
			lTexture->SetMappingType( FbxTexture::eUV );
			lTexture->ConnectDstProperty( FbxColorProperty );
			return true;
		}
	}
	return false;
};
FbxSurfaceMaterial* ExportMaterial_UTU( UMaterialInterface* MaterialInterface, MaterialBinding* Binding, FbxScene* FBXScene )
{
	if( FBXScene == nullptr || MaterialInterface == nullptr || MaterialInterface->GetMaterial() == nullptr ) return nullptr;

	// Verify that this material has not already been exported:
	if( FbxMaterials.Find( MaterialInterface ) )
	{
		return *FbxMaterials.Find( MaterialInterface );
	}

	// Create the Fbx material
	FbxSurfaceMaterial* FbxMaterial = nullptr;

	UMaterial* Material = MaterialInterface->GetMaterial();
	if( !Material )
	{
		//Nothing to export
		return nullptr;
	}

	// Set the shading model
	if( HasOnlyShadingModel( Material, MSM_DefaultLit ) )
	{
		FbxMaterial = FbxSurfacePhong::Create( FBXScene, TCHAR_TO_UTF8( *MaterialInterface->GetName() ) );
		//((FbxSurfacePhong*)FbxMaterial)->Specular.Set(Material->Specular));
		//((FbxSurfacePhong*)FbxMaterial)->Shininess.Set(Material->SpecularPower.Constant);
	}
	else // if (Material->ShadingModel == MSM_Unlit)
	{
		FbxMaterial = FbxSurfaceLambert::Create( FBXScene, TCHAR_TO_UTF8( *MaterialInterface->GetName() ) );
	}

	for( int i = 0; i < Binding->UnityMat->Textures.size(); i++ )
	{
		UnityTexture* Tex = Binding->UnityMat->Textures[ i ];
		if (!Tex)
			continue;

		switch( Tex->Semantic )
		{
			case TS_MainTex			:
			case TS_BaseMap			:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, FBXScene ); break;
			case TS_BumpMap			:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sNormalMap, FBXScene ); break;
			//case TS_DetailAlbedoMap	:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_DetailMask		:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_DetailNormalMap	:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			case TS_EmissionMap		:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sEmissive, FBXScene ); break;
			//case TS_MetallicGlossMap:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_OcclusionMap	:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_ParallaxMap		:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_SpecGlossMap	:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
		}
	}

	FbxMaterials.Add( MaterialInterface, FbxMaterial );

	return FbxMaterial;
}
FbxSurfaceMaterial* ExportMaterial( UMaterialInterface* MaterialInterface, FbxScene* FBXScene)
{
	if( FBXScene == nullptr || MaterialInterface == nullptr || MaterialInterface->GetMaterial() == nullptr ) return nullptr;

	// Verify that this material has not already been exported:
	if( FbxMaterials.Find( MaterialInterface ) )
	{
		return *FbxMaterials.Find( MaterialInterface );
	}

	// Create the Fbx material
	FbxSurfaceMaterial* FbxMaterial = nullptr;

	UMaterial* Material = MaterialInterface->GetMaterial();
	if( !Material )
	{
		//Nothing to export
		return nullptr;
	}

	// Set the shading model
	if( HasOnlyShadingModel( Material, MSM_DefaultLit ) )
	{
		FbxMaterial = FbxSurfacePhong::Create( FBXScene, TCHAR_TO_UTF8( *MaterialInterface->GetName() ) );
		//((FbxSurfacePhong*)FbxMaterial)->Specular.Set(Material->Specular));
		//((FbxSurfacePhong*)FbxMaterial)->Shininess.Set(Material->SpecularPower.Constant);
	}
	else // if (Material->ShadingModel == MSM_Unlit)
	{
		FbxMaterial = FbxSurfaceLambert::Create( FBXScene, TCHAR_TO_UTF8( *MaterialInterface->GetName() ) );
	}


	//Get the base material connected expression parameter name for all the supported fbx material exported properties
	//We only support material input where the connected expression is a parameter of type (constant, scalar, vector, texture, TODO virtual texture)

	FName BaseColorParamName = ( !GetBaseColor(Material)->UseConstant && GetBaseColor( Material )->Expression ) ? GetBaseColor( Material )->Expression->GetParameterName() : NAME_None;
	bool BaseColorParamSet = false;
	FName EmissiveParamName = ( !GetEmissiveColor(Material)->UseConstant && GetEmissiveColor( Material )->Expression ) ? GetEmissiveColor( Material )->Expression->GetParameterName() : NAME_None;
	bool EmissiveParamSet = false;
	FName NormalParamName = GetNormal( Material )->Expression ? GetNormal( Material )->Expression->GetParameterName() : NAME_None;
	bool NormalParamSet = false;
	FName OpacityParamName = ( !GetOpacity(Material )->UseConstant && GetOpacity( Material )->Expression ) ? GetOpacity( Material )->Expression->GetParameterName() : NAME_None;
	bool OpacityParamSet = false;
	FName OpacityMaskParamName = ( !GetOpacityMask( Material )->UseConstant && GetOpacityMask( Material )->Expression ) ? GetOpacityMask( Material )->Expression->GetParameterName() : NAME_None;
	bool OpacityMaskParamSet = false;

	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( MaterialInterface );
	EBlendMode BlendMode = MaterialInstance != nullptr && MaterialInstance->BasePropertyOverrides.bOverride_BlendMode ? MaterialInstance->BasePropertyOverrides.BlendMode : Material->BlendMode;

	// Loop through all types of parameters for this base material and add the supported one to the fbx material.
	//The order is important we search Texture, Vector, Scalar.

	TArray<FMaterialParameterInfo> ParameterInfos;
	TArray<FGuid> Guids;
	//Query all base material texture parameters.
	Material->GetAllTextureParameterInfo( ParameterInfos, Guids );
	for( int32 ParameterIdx = 0; ParameterIdx < ParameterInfos.Num(); ParameterIdx++ )
	{
		const FMaterialParameterInfo& ParameterInfo = ParameterInfos[ ParameterIdx ];
		FName ParameterName = ParameterInfo.Name;
		UTexture* Value;
		//Query the material instance parameter overridden value
		if( MaterialInterface->GetTextureParameterValue( ParameterInfo, Value ) && Value && Value->AssetImportData )
		{
			//This lambda extract the source file path and create the fbx file texture node and connect it to the fbx material
			auto SetTextureProperty = [&FbxMaterial, &Value]( const char* FbxPropertyName, FbxScene* InScene )->bool
			{
				FbxProperty FbxColorProperty = FbxMaterial->FindProperty( FbxPropertyName );
				if( FbxColorProperty.IsValid() )
				{
					const FString TextureName = Value->GetName();
					const FString TextureSourceFullPath = Value->AssetImportData->GetFirstFilename();
					if( !TextureSourceFullPath.IsEmpty() )
					{
						//Create a fbx property
						FbxFileTexture* lTexture = FbxFileTexture::Create( InScene, TCHAR_TO_UTF8( *TextureName ) );
						lTexture->SetFileName( TCHAR_TO_UTF8( *TextureSourceFullPath ) );
						lTexture->SetTextureUse( FbxTexture::eStandard );
						lTexture->SetMappingType( FbxTexture::eUV );
						lTexture->ConnectDstProperty( FbxColorProperty );
						return true;
					}
				}
				return false;
			};

			if( BaseColorParamName != NAME_None && ParameterName == BaseColorParamName )
			{
				BaseColorParamSet = SetTextureProperty( FbxSurfaceMaterial::sDiffuse, FBXScene );
			}
			if( EmissiveParamName != NAME_None && ParameterName == EmissiveParamName )
			{
				EmissiveParamSet = SetTextureProperty( FbxSurfaceMaterial::sEmissive, FBXScene );
			}

			if( BlendMode == BLEND_Translucent )
			{
				if( OpacityParamName != NAME_None && ParameterName == OpacityParamName )
				{
					OpacityParamSet = SetTextureProperty( FbxSurfaceMaterial::sTransparentColor, FBXScene );
				}
				if( OpacityMaskParamName != NAME_None && ParameterName == OpacityMaskParamName )
				{
					OpacityMaskParamSet = SetTextureProperty( FbxSurfaceMaterial::sTransparencyFactor, FBXScene );
				}
			}
			else
			{
				//There is no normal input in Blend translucent mode
				if( NormalParamName != NAME_None && ParameterName == NormalParamName )
				{
					NormalParamSet = SetTextureProperty( FbxSurfaceMaterial::sNormalMap, FBXScene );
				}
			}
		}
	}

	Material->GetAllVectorParameterInfo( ParameterInfos, Guids );
	//Query all base material vector parameters.
	for( int32 ParameterIdx = 0; ParameterIdx < ParameterInfos.Num(); ParameterIdx++ )
	{
		const FMaterialParameterInfo& ParameterInfo = ParameterInfos[ ParameterIdx ];
		FName ParameterName = ParameterInfo.Name;
		FLinearColor LinearColor;
		//Query the material instance parameter overridden value
		if( MaterialInterface->GetVectorParameterValue( ParameterInfo, LinearColor ) )
		{
			FbxDouble3 LinearFbxValue( LinearColor.R, LinearColor.G, LinearColor.B );
			if( !BaseColorParamSet && BaseColorParamName != NAME_None && ParameterName == BaseColorParamName )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( LinearFbxValue );
				BaseColorParamSet = true;
			}
			if( !EmissiveParamSet && EmissiveParamName != NAME_None && ParameterName == EmissiveParamName )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->Emissive.Set( LinearFbxValue );
				EmissiveParamSet = true;
			}
		}
	}

	//Query all base material scalar parameters.
	Material->GetAllScalarParameterInfo( ParameterInfos, Guids );
	for( int32 ParameterIdx = 0; ParameterIdx < ParameterInfos.Num(); ParameterIdx++ )
	{
		const FMaterialParameterInfo& ParameterInfo = ParameterInfos[ ParameterIdx ];
		FName ParameterName = ParameterInfo.Name;
		float Value;
		//Query the material instance parameter overridden value
		if( MaterialInterface->GetScalarParameterValue( ParameterInfo, Value ) )
		{
			FbxDouble FbxValue = (FbxDouble)Value;
			FbxDouble3 FbxVector( FbxValue, FbxValue, FbxValue );
			if( !BaseColorParamSet && BaseColorParamName != NAME_None && ParameterName == BaseColorParamName )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( FbxVector );
				BaseColorParamSet = true;
			}
			if( !EmissiveParamSet && EmissiveParamName != NAME_None && ParameterName == EmissiveParamName )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->Emissive.Set( FbxVector );
				EmissiveParamSet = true;
			}
			if( BlendMode == BLEND_Translucent )
			{
				if( !OpacityParamSet && OpacityParamName != NAME_None && ParameterName == OpacityParamName )
				{
					( (FbxSurfaceLambert*)FbxMaterial )->TransparentColor.Set( FbxVector );
					OpacityParamSet = true;
				}

				if( !OpacityMaskParamSet && OpacityMaskParamName != NAME_None && ParameterName == OpacityMaskParamName )
				{
					( (FbxSurfaceLambert*)FbxMaterial )->TransparencyFactor.Set( FbxValue );
					OpacityMaskParamSet = true;
				}
			}
		}
	}

	//TODO: export virtual texture to fbx
	//Query all base material runtime virtual texture parameters.
// 	Material->GetAllRuntimeVirtualTextureParameterInfo(ParameterInfos, Guids);
// 	for (int32 ParameterIdx = 0; ParameterIdx < ParameterInfos.Num(); ParameterIdx++)
// 	{
// 		const FMaterialParameterInfo& ParameterInfo = ParameterInfos[ParameterIdx];
// 		FName ParameterName = ParameterInfo.Name;
// 		URuntimeVirtualTexture* Value;
//		//Query the material instance parameter overridden value
// 		if (MaterialInterface->GetRuntimeVirtualTextureParameterValue(ParameterInfo, Value, bOveriddenOnly))
// 		{
// 		}
// 	}

	//
	//In case there is no material instance override, we extract the value from the base material
	//

	//The OpacityMaskParam set the transparency factor, so set it only if it was not set
	if( !OpacityMaskParamSet )
	{
		( (FbxSurfaceLambert*)FbxMaterial )->TransparencyFactor.Set( GetOpacity(Material)->Constant );
	}

	// Fill in the profile_COMMON effect with the material information.
	//Fill the texture or constant
	if( !BaseColorParamSet )
	{
		if( !FillFbxTextureProperty( FbxSurfaceMaterial::sDiffuse, *GetBaseColor(Material), FbxMaterial, FBXScene ) )
		{
			( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( SetMaterialComponent( *GetBaseColor(Material), true ) );
		}
	}

	if( !EmissiveParamSet )
	{
		if( !FillFbxTextureProperty( FbxSurfaceMaterial::sEmissive, *GetEmissiveColor(Material), FbxMaterial, FBXScene ) )
		{
			( (FbxSurfaceLambert*)FbxMaterial )->Emissive.Set( SetMaterialComponent( *GetEmissiveColor( Material ), true ) );
		}
	}

	//Always set the ambient to zero since we dont have ambient in unreal we want to avoid default value in DCCs
	( (FbxSurfaceLambert*)FbxMaterial )->Ambient.Set( FbxDouble3( 0.0, 0.0, 0.0 ) );

	if( BlendMode == BLEND_Translucent )
	{
		if( !OpacityParamSet )
		{
			if( !FillFbxTextureProperty( FbxSurfaceMaterial::sTransparentColor, *GetOpacity(Material), FbxMaterial, FBXScene ) )
			{
				FbxDouble3 OpacityValue( (FbxDouble)( GetOpacity(Material)->Constant ), (FbxDouble)( GetOpacity(Material)->Constant ), (FbxDouble)( GetOpacity(Material)->Constant ) );
				( (FbxSurfaceLambert*)FbxMaterial )->TransparentColor.Set( OpacityValue );
			}
		}

		if( !OpacityMaskParamSet )
		{
			if( !FillFbxTextureProperty( FbxSurfaceMaterial::sTransparencyFactor, *GetOpacityMask(Material), FbxMaterial, FBXScene ) )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->TransparencyFactor.Set( GetOpacityMask(Material)->Constant );
			}
		}
	}
	else
	{
		//There is no normal input in Blend translucent mode
		if( !NormalParamSet )
		{
			//Set the Normal map only if there is a texture sampler
			FillFbxTextureProperty( FbxSurfaceMaterial::sNormalMap, *GetNormal( Material ), FbxMaterial, FBXScene );
		}
	}

	FbxMaterials.Add( MaterialInterface, FbxMaterial );

	return FbxMaterial;
}

//#if PLATFORM_WINDOWS
	#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
		#define WITH_CHAOS 1
		#define WITH_PHYSX 0
		#define PHYSICS_INTERFACE_PHYSX 0
	#endif
//#endif

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	#define PHYSICS_INTERFACE_PHYSX 1
	#define WITH_CHAOS 0
#endif

#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	#include "geometry/PxConvexMesh.h"
	using namespace physx;
#else
	#include "PxConvexMesh.h"
#endif
#include "PxVec3.h"
FVector P3ToVector( const PxVec3& PVec )
{
	return FVector( PVec.x, PVec.y, PVec.z );
}
#endif

#if WITH_CHAOS
	#include "Chaos/Convex.h"
#endif
#if (WITH_PHYSX && PHYSICS_INTERFACE_PHYSX) || WITH_CHAOS
class FCollisionFbxExporter
{
public:
	FCollisionFbxExporter( const UStaticMesh* StaticMeshToExport, FbxMesh* ExportMesh, int32 ActualMatIndexToExport )
	{
		BoxPositions[ 0 ] = FVector( -1, -1, +1 );
		BoxPositions[ 1 ] = FVector( -1, +1, +1 );
		BoxPositions[ 2 ] = FVector( +1, +1, +1 );
		BoxPositions[ 3 ] = FVector( +1, -1, +1 );

		BoxFaceRotations[ 0 ] = FRotator( 0, 0, 0 );
		BoxFaceRotations[ 1 ] = FRotator( 90.f, 0, 0 );
		BoxFaceRotations[ 2 ] = FRotator( -90.f, 0, 0 );
		BoxFaceRotations[ 3 ] = FRotator( 0, 0, 90.f );
		BoxFaceRotations[ 4 ] = FRotator( 0, 0, -90.f );
		BoxFaceRotations[ 5 ] = FRotator( 180.f, 0, 0 );

		DrawCollisionSides = 16;

		SpherNumSides = DrawCollisionSides;
		SphereNumRings = DrawCollisionSides / 2;
		SphereNumVerts = ( SpherNumSides + 1 ) * ( SphereNumRings + 1 );

		CapsuleNumSides = DrawCollisionSides;
		CapsuleNumRings = ( DrawCollisionSides / 2 ) + 1;
		CapsuleNumVerts = ( CapsuleNumSides + 1 ) * ( CapsuleNumRings + 1 );

		CurrentVertexOffset = 0;

		StaticMesh = StaticMeshToExport;
		Mesh = ExportMesh;
		ActualMatIndex = ActualMatIndexToExport;
	}

	void ExportCollisions()
	{
		const FKAggregateGeom& AggGeo = GetBodySetup( StaticMesh )->AggGeom;

		int32 VerticeNumber = 0;
		for( const FKConvexElem& ConvexElem : AggGeo.ConvexElems )
		{
			VerticeNumber += GetConvexVerticeNumber( ConvexElem );
		}
		//for( const FKBoxElem& BoxElem : AggGeo.BoxElems )
		//{
		//	VerticeNumber += GetBoxVerticeNumber();
		//}
		//for( const FKSphereElem& SphereElem : AggGeo.SphereElems )
		//{
		//	VerticeNumber += GetSphereVerticeNumber();
		//}
		//for( const FKSphylElem& CapsuleElem : AggGeo.SphylElems )
		//{
		//	VerticeNumber += GetCapsuleVerticeNumber();
		//}

		Mesh->InitControlPoints( VerticeNumber );
		ControlPoints = Mesh->GetControlPoints();
		CurrentVertexOffset = 0;
		//////////////////////////////////////////////////////////////////////////
		// Set all vertex
		for( const FKConvexElem& ConvexElem : AggGeo.ConvexElems )
		{
			AddConvexVertex( ConvexElem );
		}

		//for( const FKBoxElem& BoxElem : AggGeo.BoxElems )
		//{
		//	AddBoxVertex( BoxElem );
		//}
		//
		//for( const FKSphereElem& SphereElem : AggGeo.SphereElems )
		//{
		//	AddSphereVertex( SphereElem );
		//}
		//
		//for( const FKSphylElem& CapsuleElem : AggGeo.SphylElems )
		//{
		//	AddCapsuleVertex( CapsuleElem );
		//}

		// Set the normals on Layer 0.
		FbxLayer* Layer = Mesh->GetLayer( 0 );
		if( Layer == nullptr )
		{
			Mesh->CreateLayer();
			Layer = Mesh->GetLayer( 0 );
		}
		// Create and fill in the per-face-vertex normal data source.
		LayerElementNormal = FbxLayerElementNormal::Create( Mesh, "" );
		LayerElementTangent = FbxLayerElementTangent::Create( Mesh, "" );
		LayerElementBinormal = FbxLayerElementBinormal::Create( Mesh, "" );
		// Set the normals per polygon instead of storing normals on positional control points
		LayerElementNormal->SetMappingMode( FbxLayerElement::eByPolygonVertex );
		LayerElementTangent->SetMappingMode( FbxLayerElement::eByPolygonVertex );
		LayerElementBinormal->SetMappingMode( FbxLayerElement::eByPolygonVertex );
		// Set the normal values for every polygon vertex.
		LayerElementNormal->SetReferenceMode( FbxLayerElement::eDirect );
		LayerElementTangent->SetReferenceMode( FbxLayerElement::eDirect );
		LayerElementBinormal->SetReferenceMode( FbxLayerElement::eDirect );

		//////////////////////////////////////////////////////////////////////////
		//Set the Normals
		for( const FKConvexElem& ConvexElem : AggGeo.ConvexElems )
		{
			AddConvexNormals( ConvexElem );
		}
		//for( const FKBoxElem& BoxElem : AggGeo.BoxElems )
		//{
		//	AddBoxNormal( BoxElem );
		//}

		//int32 SphereIndex = 0;
		//for( const FKSphereElem& SphereElem : AggGeo.SphereElems )
		//{
		//	AddSphereNormals( SphereElem, SphereIndex );
		//	SphereIndex++;
		//}
		//
		//int32 CapsuleIndex = 0;
		//for( const FKSphylElem& CapsuleElem : AggGeo.SphylElems )
		//{
		//	AddCapsuleNormals( CapsuleElem, CapsuleIndex );
		//	CapsuleIndex++;
		//}

		Layer->SetNormals( LayerElementNormal );
		Layer->SetTangents( LayerElementTangent );
		Layer->SetBinormals( LayerElementBinormal );

		//////////////////////////////////////////////////////////////////////////
		// Set polygons
		// Build list of polygon re-used multiple times to lookup Normals, UVs, other per face vertex information
		CurrentVertexOffset = 0; //Reset the current VertexCount
		for( const FKConvexElem& ConvexElem : AggGeo.ConvexElems )
		{
			AddConvexPolygon( ConvexElem );
		}

		//for( const FKBoxElem& BoxElem : AggGeo.BoxElems )
		//{
		//	AddBoxPolygons();
		//}
		//
		//for( const FKSphereElem& SphereElem : AggGeo.SphereElems )
		//{
		//	AddSpherePolygons();
		//}
		//
		//for( const FKSphylElem& CapsuleElem : AggGeo.SphylElems )
		//{
		//	AddCapsulePolygons();
		//}

		//////////////////////////////////////////////////////////////////////////
		//Free the sphere resources
		for( FDynamicMeshVertex* DynamicMeshVertex : SpheresVerts )
		{
			FMemory::Free( DynamicMeshVertex );
		}
		SpheresVerts.Empty();

		//////////////////////////////////////////////////////////////////////////
		//Free the capsule resources
		for( FDynamicMeshVertex* DynamicMeshVertex : CapsuleVerts )
		{
			FMemory::Free( DynamicMeshVertex );
		}
		CapsuleVerts.Empty();
	}

private:
	uint32 GetConvexVerticeNumber( const FKConvexElem& ConvexElem )
	{
	#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
		return ConvexElem.GetConvexMesh() != nullptr ? ConvexElem.GetConvexMesh()->getNbVertices() : 0;
	#elif WITH_CHAOS
		return ConvexElem.VertexData.Num();
	#endif
	}

	uint32 GetBoxVerticeNumber() { return 24; }

	uint32 GetSphereVerticeNumber() { return SphereNumVerts; }

	uint32 GetCapsuleVerticeNumber() { return CapsuleNumVerts; }

	void AddConvexVertex( const FKConvexElem& ConvexElem )
	{
	#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if( ConvexMesh == nullptr )
		{
			return;
		}
		FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );//match mesh rotations
		const PxVec3* VertexArray = ConvexMesh->getVertices();
		for( uint32 PosIndex = 0; PosIndex < ConvexMesh->getNbVertices(); ++PosIndex )
		{
			FVector Position = P3ToVector( VertexArray[ PosIndex ] );
			Position = Rotator.RotateVector( Position );
			ControlPoints[ CurrentVertexOffset + PosIndex ] = FbxVector4( -Position.X, Position.Y, Position.Z );
		}
		CurrentVertexOffset += ConvexMesh->getNbVertices();
	#elif WITH_CHAOS
		const TArray<FVector>& VertexArray = ConvexElem.VertexData;
		FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );//match mesh rotations
		for( int32 PosIndex = 0; PosIndex < VertexArray.Num(); ++PosIndex )
		{
			FVector Position = VertexArray[ PosIndex ];
			Position = Rotator.RotateVector( Position );
			ControlPoints[ CurrentVertexOffset + PosIndex ] = FbxVector4( -Position.X, Position.Y, Position.Z );
		}
		CurrentVertexOffset += VertexArray.Num();
	#endif
	}

	void AddConvexNormals( const FKConvexElem& ConvexElem )
	{
	#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if( ConvexMesh == nullptr )
		{
			return;
		}
		const PxU8* PIndexBuffer = ConvexMesh->getIndexBuffer();
		int32 PolygonNumber = ConvexMesh->getNbPolygons();
		for( int32 PolyIndex = 0; PolyIndex < PolygonNumber; ++PolyIndex )
		{
			PxHullPolygon PolyData;
			if( !ConvexMesh->getPolygonData( PolyIndex, PolyData ) )
			{
				continue;
			}
			const PxVec3 PPlaneNormal( PolyData.mPlane[ 0 ], PolyData.mPlane[ 1 ], PolyData.mPlane[ 2 ] );
			FVector Normal = P3ToVector( PPlaneNormal.getNormalized() );
			FbxVector4 FbxNormal = FbxVector4( Normal.X, -Normal.Y, Normal.Z );
			// add vertices 
			for( PxU32 j = 0; j < PolyData.mNbVerts; ++j )
			{
				LayerElementNormal->GetDirectArray().Add( FbxNormal );
			}
		}
	#elif WITH_CHAOS
		
		const auto& ConvexMesh = ConvexElem.GetChaosConvexMesh();
		if (!ConvexMesh.IsValid())
		{
			return;
		}
		const TArray<Chaos::FConvex::FPlaneType>& Faces = ConvexMesh->GetFaces();
		for (int32 PolyIndex = 0; PolyIndex < Faces.Num(); ++PolyIndex)
		{
			FVector Normal = (FVector)Faces[PolyIndex].Normal().GetSafeNormal();
			FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
			// add vertices 
			for (int32 j = 0; j < 3; ++j)
			{
				LayerElementNormal->GetDirectArray().Add(FbxNormal);
				LayerElementTangent->GetDirectArray().Add( FbxNormal );
				LayerElementBinormal->GetDirectArray().Add( FbxNormal );
			}
		}
	#endif
	}

	void AddConvexPolygon( const FKConvexElem& ConvexElem )
	{
	#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if( ConvexMesh == nullptr )
		{
			return;
		}
		const PxU8* PIndexBuffer = ConvexMesh->getIndexBuffer();
		int32 PolygonNumber = ConvexMesh->getNbPolygons();
		for( int32 PolyIndex = 0; PolyIndex < PolygonNumber; ++PolyIndex )
		{
			PxHullPolygon PolyData;
			if( !ConvexMesh->getPolygonData( PolyIndex, PolyData ) )
			{
				continue;
			}
			Mesh->BeginPolygon( ActualMatIndex );
			const PxU8* PolyIndices = PIndexBuffer + PolyData.mIndexBase;
			// add vertices 
			//for( PxU32 j = 0; j < PolyData.mNbVerts; ++j )
			for( int j = PolyData.mNbVerts - 1; j >= 0; j-- )
			{
				const uint32 VertIndex = CurrentVertexOffset + PolyIndices[ j ];
				Mesh->AddPolygon( VertIndex );
			}
			Mesh->EndPolygon();
		}
		CurrentVertexOffset += ConvexMesh->getNbVertices();
	#elif WITH_CHAOS
		//const auto& ConvexMesh = ConvexElem.GetChaosConvexMesh();
		//if( !ConvexMesh.IsValid() )
		//{
		//	return;
		//}
		TArray<int32> IndexData( ConvexElem.IndexData );
		if( IndexData.Num() == 0 )
		{
			IndexData = ConvexElem.GetChaosConvexIndices();
		}
		check( IndexData.Num() % 3 == 0 );
		for( int32 VertexIndex = 0; VertexIndex < IndexData.Num(); VertexIndex += 3 )
		{
			Mesh->BeginPolygon( ActualMatIndex );
			bool ReverseOrder = false;
			if( ReverseOrder )
			{
				Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex ] );
				Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex + 1 ] );
				Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex + 2 ] );
			}
			else
			{
				Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex + 2] );
				Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex + 1 ] );
				Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex ] );
			}
			Mesh->EndPolygon();
		}

		CurrentVertexOffset += ConvexElem.VertexData.Num();
	#endif
	}

	//////////////////////////////////////////////////////////////////////////
	//Box data
	FVector BoxPositions[ 4 ];
	FRotator BoxFaceRotations[ 6 ];


	int32 DrawCollisionSides;
	//////////////////////////////////////////////////////////////////////////
	//Sphere data
	int32 SpherNumSides;
	int32 SphereNumRings;
	int32 SphereNumVerts;
	TArray<FDynamicMeshVertex*> SpheresVerts;

	//////////////////////////////////////////////////////////////////////////
	//Capsule data
	int32 CapsuleNumSides;
	int32 CapsuleNumRings;
	int32 CapsuleNumVerts;
	TArray<FDynamicMeshVertex*> CapsuleVerts;

	//////////////////////////////////////////////////////////////////////////
	//Mesh Data
	uint32 CurrentVertexOffset;

	const UStaticMesh* StaticMesh;
	FbxMesh* Mesh;
	int32 ActualMatIndex;
	FbxVector4* ControlPoints;
	FbxLayerElementNormal* LayerElementNormal;
	FbxLayerElementTangent* LayerElementTangent;
	FbxLayerElementBinormal* LayerElementBinormal;
};
FbxNode* ExportCollisionMesh( FFbxExporter* Exporter, const UStaticMesh* StaticMesh, const TCHAR* MeshName, FbxNode* ParentActor, FbxScene* FBXScene )
{
	const FKAggregateGeom& AggGeo = GetBodySetup( StaticMesh )->AggGeom;
	if( AggGeo.GetElementCount() <= 0 || !ExporterInterface->Settings->ExportPhysics )
	{
		return nullptr;
	}
	//No idea but FbxMeshes doesn't add vertex painted meshes, but returning here misses ConvexHulls for vertex painted objects
	//FbxMesh* Mesh = FbxMeshes.FindRef( StaticMesh );
	//if( !Mesh )
	//{
	//	//We export collision only if the mesh is already exported
	//	return nullptr;
	//}

	//Name the node with the actor name
	FString MeshCollisionName = MeshName;// TEXT( "UCX_" );
	MeshCollisionName += TEXT( "_ConvexHulls" ); //UTF8_TO_TCHAR( ParentActor->GetName() ); //-V595
	FbxNode* FbxActor = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( *MeshCollisionName ) );

	if( ParentActor != nullptr )
	{
		// Collision meshes are added directly to the scene root, so we need to use the global transform instead of the relative one.
		FbxAMatrix& GlobalTransform = ParentActor->EvaluateGlobalTransform();

		const FStaticMeshRenderData* RenderData = GetRenderData( StaticMesh );
		if( RenderData->LODResources.Num() == 1 )
		{
			//This is apparently needed to have 0,0,0 rotation for the collider but only when no LODs
			FbxActor->LclTranslation.Set( GlobalTransform.GetT() );
			FbxActor->LclRotation.Set( GlobalTransform.GetR() );
			FbxActor->LclScaling.Set( GlobalTransform.GetS() );
		}

		if ( ParentActor->GetParent())
			ParentActor->GetParent()->AddChild( FbxActor );
		else
			FBXScene->GetRootNode()->AddChild( FbxActor );
	}

	FbxMesh* CollisionMesh = FbxCollisionMeshes.FindRef( StaticMesh );
	if( !CollisionMesh )
	{
		//Name the mesh attribute with the mesh name
		//MeshCollisionName = TEXT( "UCX_" );
		MeshCollisionName = MeshName;
		MeshCollisionName += TEXT( "_ConvexHulls" );
		CollisionMesh = FbxMesh::Create( FBXScene, TCHAR_TO_UTF8( *MeshCollisionName ) );

		//Export all collision elements in one mesh
		FbxSurfaceMaterial* FbxMaterial = nullptr;
		for( auto iter = FbxMaterials.CreateIterator(); iter; ++iter )
		{
			FbxMaterial = iter->Value;
		}		
		
		int32 ActualMatIndex = FbxActor->AddMaterial( FbxMaterial );
		FCollisionFbxExporter CollisionFbxExporter( StaticMesh, CollisionMesh, ActualMatIndex );
		CollisionFbxExporter.ExportCollisions();
		FbxCollisionMeshes.Add( StaticMesh, CollisionMesh );
	}

	if( CollisionMesh->GetPolygonCount() > 0 )
	{
		//Set the original meshes in case it was already existing
		FbxActor->SetNodeAttribute( CollisionMesh );
	}
	return FbxActor;
}
#endif

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
class INodeNameAdapter_UTU
{
public:
	virtual ~INodeNameAdapter_UTU() {}
	virtual FString GetActorNodeName( const AActor* InActor ) { return InActor->GetName(); }
	virtual void AddFbxNode( UObject* InObject, fbxsdk::FbxNode* InFbxNode ) {}
	virtual fbxsdk::FbxNode* GetFbxNode( UObject* InObject ) { return nullptr; }
};
#define INodeNameAdapter INodeNameAdapter_UTU
#endif

UFbxExportOption* GetExportOptions( FFbxExporter* Exporter )
{
	#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
		uint8* Pointer = (uint8*)Exporter;
		static int Offset = 24;
		UFbxExportOption* Data = *(UFbxExportOption**)( Pointer + Offset );
		return Data;
	#else
		return Exporter->GetExportOptions();
	#endif
}
FbxNode* ExportStaticMeshToFbx( FFbxExporter* Exporter, const UStaticMesh* StaticMesh, int32 ExportLOD, const TCHAR* MeshName, FbxNode* FbxActor, int32 LightmapUVChannel /*= -1*/, const FColorVertexBuffer* ColorBuffer /*= NULL*/, const TArray<FStaticMaterial>* MaterialOrderOverride /*= NULL*/, const TArray<UMaterialInterface*>* OverrideMaterials /*= NULL*/ ,
								FbxScene* FBXScene )
{
	FbxMesh* Mesh = nullptr;
	if( ( ExportLOD == 0 || ExportLOD == -1 ) && LightmapUVChannel == -1 && ColorBuffer == nullptr && MaterialOrderOverride == nullptr )
	{
		Mesh = FbxMeshes.FindRef( StaticMesh );
	}

	if( !Mesh )
	{
		Mesh = FbxMesh::Create( FBXScene, TCHAR_TO_UTF8( MeshName ) );

		const FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport( ExportLOD );

		// Verify the integrity of the static mesh.
		if( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0 )
		{
			return nullptr;
		}

		if( RenderMesh.Sections.Num() == 0 )
		{
			return nullptr;
		}

		// Remaps an Unreal vert to final reduced vertex list
		TArray<int32> VertRemap;
		TArray<int32> UniqueVerts;

		// Weld verts
		DetermineVertsToWeld( VertRemap, UniqueVerts, RenderMesh );

		// Create and fill in the vertex position data source.
		// The position vertices are duplicated, for some reason, retrieve only the first half vertices.
		const int32 VertexCount = VertRemap.Num();
		const int32 PolygonsCount = RenderMesh.Sections.Num();

		Mesh->InitControlPoints( UniqueVerts.Num() );

		FbxVector4* ControlPoints = Mesh->GetControlPoints();
		
		FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );//preview FBX the same as UE

		for( int32 PosIndex = 0; PosIndex < UniqueVerts.Num(); ++PosIndex )
		{
			int32 UnrealPosIndex = UniqueVerts[ PosIndex ];
			FVector Position = (FVector)RenderMesh.VertexBuffers.PositionVertexBuffer.VertexPosition( UnrealPosIndex );
			ControlPoints[ PosIndex ] = FbxVector4( Position.X, -Position.Y, Position.Z );
			if( 1 )
			{
				Position = Rotator.RotateVector( Position );
				ControlPoints[ PosIndex ] = FbxVector4( -Position.X, Position.Y, Position.Z );
			}
		}

		// Set the normals on Layer 0.
		FbxLayer* Layer = Mesh->GetLayer( 0 );
		if( Layer == nullptr )
		{
			Mesh->CreateLayer();
			Layer = Mesh->GetLayer( 0 );
		}

		// Build list of Indices re-used multiple times to lookup Normals, UVs, other per face vertex information
		TArray<uint32> Indices;
		for( int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex )
		{
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			const FStaticMeshSection& Polygons = RenderMesh.Sections[ PolygonsIndex ];
			const uint32 TriangleCount = Polygons.NumTriangles;
			for( uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex )
			{
				//for( int PointIndex = 2; PointIndex >=0; PointIndex-- )
				for( int PointIndex = 0; PointIndex < 3; PointIndex++ )
				{
					uint32 UnrealVertIndex = RawIndices[ Polygons.FirstIndex + ( ( TriangleIndex * 3 ) + PointIndex ) ];
					Indices.Add( UnrealVertIndex );
				}
			}
		}

		// Create and fill in the per-face-vertex normal data source.
		// We extract the Z-tangent and the X/Y-tangents which are also stored in the render mesh.
		FbxLayerElementNormal* LayerElementNormal = FbxLayerElementNormal::Create( Mesh, "" );
		FbxLayerElementTangent* LayerElementTangent = FbxLayerElementTangent::Create( Mesh, "" );
		FbxLayerElementBinormal* LayerElementBinormal = FbxLayerElementBinormal::Create( Mesh, "" );

		// Set 3 NTBs per triangle instead of storing on positional control points
		LayerElementNormal->SetMappingMode( FbxLayerElement::eByPolygonVertex );
		LayerElementTangent->SetMappingMode( FbxLayerElement::eByPolygonVertex );
		LayerElementBinormal->SetMappingMode( FbxLayerElement::eByPolygonVertex );

		// Set the NTBs values for every polygon vertex.
		LayerElementNormal->SetReferenceMode( FbxLayerElement::eDirect );
		LayerElementTangent->SetReferenceMode( FbxLayerElement::eDirect );
		LayerElementBinormal->SetReferenceMode( FbxLayerElement::eDirect );

		TArray<FbxVector4> FbxNormals;
		TArray<FbxVector4> FbxTangents;
		TArray<FbxVector4> FbxBinormals;

		FbxNormals.AddUninitialized( VertexCount );
		FbxTangents.AddUninitialized( VertexCount );
		FbxBinormals.AddUninitialized( VertexCount );

		for( int32 NTBIndex = 0; NTBIndex < VertexCount; ++NTBIndex )
		{
			FVector Normal = (FVector)( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( NTBIndex ) );
			if( 1 )
			{
				Normal.Y *= -1;
				Normal = Rotator.RotateVector( Normal );
			}
			FbxVector4& FbxNormal = FbxNormals[ NTBIndex ];
			FbxNormal = FbxVector4( Normal.X, Normal.Y, Normal.Z );
			FbxNormal.Normalize();

			FVector Tangent = (FVector)( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX( NTBIndex ) );
			if( 1 )
			{
				Tangent.Y *= -1;
				Tangent = Rotator.RotateVector( Tangent );
			}
			FbxVector4& FbxTangent = FbxTangents[ NTBIndex ];
			FbxTangent = FbxVector4( Tangent.X, Tangent.Y, Tangent.Z );
			FbxTangent.Normalize();

			FVector Binormal = -(FVector)( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY( NTBIndex ) );
			if( 1 )
			{
				Binormal.Y *= -1;
				Binormal = Rotator.RotateVector( Binormal );
			}
			FbxVector4& FbxBinormal = FbxBinormals[ NTBIndex ];
			FbxBinormal = FbxVector4( Binormal.X, Binormal.Y, Binormal.Z );
			FbxBinormal.Normalize();
		}

		LayerElementNormal->GetDirectArray().Resize( Indices.Num() );
		LayerElementTangent->GetDirectArray().Resize( Indices.Num() );
		LayerElementBinormal->GetDirectArray().Resize( Indices.Num() );

		// Add one normal per each face index (3 per triangle)
		for( int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++ )
		{
			uint32 UnrealVertIndex = Indices[ FbxVertIndex ];
			FbxVector4 Normal = FbxNormals[ UnrealVertIndex ];
			FbxVector4 Tangent = FbxTangents[ UnrealVertIndex ];
			FbxVector4 Binormal = FbxBinormals[ UnrealVertIndex ];

			LayerElementNormal->GetDirectArray().SetAt( FbxVertIndex, Normal );
			LayerElementTangent->GetDirectArray().SetAt( FbxVertIndex, Tangent );
			LayerElementBinormal->GetDirectArray().SetAt( FbxVertIndex, Binormal );
		}

		Layer->SetNormals( LayerElementNormal );
		Layer->SetTangents( LayerElementTangent );
		Layer->SetBinormals( LayerElementBinormal );

		FbxNormals.Empty();
		FbxTangents.Empty();
		FbxBinormals.Empty();

		// Create and fill in the per-face-vertex texture coordinate data source(s).
		// Create UV for Diffuse channel.
		int32 TexCoordSourceCount = ( LightmapUVChannel == -1 ) ? RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() : LightmapUVChannel + 1;
		int32 TexCoordSourceIndex = ( LightmapUVChannel == -1 ) ? 0 : LightmapUVChannel;
		for( ; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex )
		{
			FbxLayer* UVsLayer = ( LightmapUVChannel == -1 ) ? Mesh->GetLayer( TexCoordSourceIndex ) : Mesh->GetLayer( 0 );
			if( UVsLayer == NULL )
			{
				Mesh->CreateLayer();
				UVsLayer = ( LightmapUVChannel == -1 ) ? Mesh->GetLayer( TexCoordSourceIndex ) : Mesh->GetLayer( 0 );
			}
			check( UVsLayer );

			FString UVChannelNameBuilder = TEXT( "UVmap_" ) + FString::FromInt( TexCoordSourceIndex );
			//const char* UVChannelName = TCHAR_TO_UTF8( *UVChannelNameBuilder ); // actually UTF8 as required by Fbx, but can't use UE4's UTF8CHAR type because that's a uint8 aka *unsigned* char
			//if( ( LightmapUVChannel >= 0 ) || ( ( LightmapUVChannel == -1 ) && ( TexCoordSourceIndex == StaticMesh->GetLightMapCoordinateIndex() ) ) )
			//{
			//	UVChannelName = "LightMapUV";
			//}

			FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create( Mesh, TCHAR_TO_UTF8( *UVChannelNameBuilder ) );

			// Note: when eINDEX_TO_DIRECT is used, IndexArray must be 3xTriangle count, DirectArray can be smaller
			UVDiffuseLayer->SetMappingMode( FbxLayerElement::eByPolygonVertex );
			UVDiffuseLayer->SetReferenceMode( FbxLayerElement::eIndexToDirect );

			TArray<int32> UvsRemap;
			TArray<int32> UniqueUVs;
			// Weld UVs
			DetermineUVsToWeld( UvsRemap, UniqueUVs, RenderMesh.VertexBuffers.StaticMeshVertexBuffer, TexCoordSourceIndex );

			// Create the texture coordinate data source.
			for( int32 FbxVertIndex = 0; FbxVertIndex < UniqueUVs.Num(); FbxVertIndex++ )
			{
				int32 UnrealVertIndex = UniqueUVs[ FbxVertIndex ];
				#if ENGINE_MAJOR_VERSION == 4
					const FVector2D& TexCoord = RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( UnrealVertIndex, TexCoordSourceIndex );
				#else
					const FVector2f& TexCoord = RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( UnrealVertIndex, TexCoordSourceIndex );
				#endif
				//UVDiffuseLayer->GetDirectArray().Add( FbxVector2( TexCoord.X, TexCoord.Y ) );
				UVDiffuseLayer->GetDirectArray().Add( FbxVector2( TexCoord.X, -TexCoord.Y + 1.0 ) );//Why this Epic ?
			}

			// For each face index, point to a texture uv
			UVDiffuseLayer->GetIndexArray().SetCount( Indices.Num() );
			for( int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++ )
			{
				uint32 UnrealVertIndex = Indices[ FbxVertIndex ];
				int32 NewVertIndex = UvsRemap[ UnrealVertIndex ];
				UVDiffuseLayer->GetIndexArray().SetAt( FbxVertIndex, NewVertIndex );
			}

			UVsLayer->SetUVs( UVDiffuseLayer, FbxLayerElement::eTextureDiffuse );
		}

		FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create( Mesh, "" );
		MatLayer->SetMappingMode( FbxLayerElement::eByPolygon );
		MatLayer->SetReferenceMode( FbxLayerElement::eIndexToDirect );
		Layer->SetMaterials( MatLayer );

		// Keep track of the number of tri's we export
		uint32 AccountedTriangles = 0;
		for( int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex )
		{
			const FStaticMeshSection& Polygons = RenderMesh.Sections[ PolygonsIndex ];
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			UMaterialInterface* Material = nullptr;

			if( OverrideMaterials && OverrideMaterials->IsValidIndex( Polygons.MaterialIndex ) )
			{
				Material = ( *OverrideMaterials )[ Polygons.MaterialIndex ];
			}
			else
			{
				Material = StaticMesh->GetMaterial( Polygons.MaterialIndex );
			}

			MaterialBinding* Binding = GetMaterialBinding( Material );
			FbxSurfaceMaterial* FbxMaterial = nullptr;
			if( Binding )
				FbxMaterial = ExportMaterial_UTU( Material, Binding, FBXScene );
			else
				FbxMaterial = ExportMaterial( Material, FBXScene );
			//FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial( Material ) : NULL;
			if( !FbxMaterial )
			{
				FbxMaterial = CreateDefaultMaterial( FBXScene );
			}
			int ExistentMaterialIndex = -1;
			if( FbxMaterial )
			{
				ExistentMaterialIndex = FbxActor->GetMaterialIndex( FbxMaterial->GetName() );
			}
			if ( ExistentMaterialIndex == -1 )
				ExistentMaterialIndex = FbxActor->AddMaterial( FbxMaterial );

			// Determine the actual material index
			int32 ActualMatIndex = ExistentMaterialIndex;

			if( MaterialOrderOverride )
			{
				ActualMatIndex = MaterialOrderOverride->Find( Material );
			}
			// Static meshes contain one triangle list per element.
			// [GLAFORTE] Could it occasionally contain triangle strips? How do I know?
			uint32 TriangleCount = Polygons.NumTriangles;

			// Copy over the index buffer into the FBX polygons set.
			for( uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex )
			{
				Mesh->BeginPolygon( ActualMatIndex );
				//for( int PointIndex = 2; PointIndex >=0; PointIndex-- )
				for( int PointIndex = 0; PointIndex < 3; PointIndex++ )
				{
					uint32 OriginalUnrealVertIndex = RawIndices[ Polygons.FirstIndex + ( ( TriangleIndex * 3 ) + PointIndex ) ];
					int32 RemappedVertIndex = VertRemap[ OriginalUnrealVertIndex ];
					Mesh->AddPolygon( RemappedVertIndex );
				}
				Mesh->EndPolygon();
			}

			AccountedTriangles += TriangleCount;
		}

	#ifdef TODO_FBX
		// Throw a warning if this is a lightmap export and the exported poly count does not match the raw triangle data count
		if( LightmapUVChannel != -1 && AccountedTriangles != RenderMesh.RawTriangles.GetElementCount() )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT( "UnrealEd", "StaticMeshEditor_LightmapExportFewerTriangles", "Fewer polygons have been exported than the raw triangle count.  This Lightmapped UV mesh may contain fewer triangles than the destination mesh on import." ) );
		}

		// Create and fill in the smoothing data source.
		FbxLayerElementSmoothing* SmoothingInfo = FbxLayerElementSmoothing::Create( Mesh, "" );
		SmoothingInfo->SetMappingMode( FbxLayerElement::eByPolygon );
		SmoothingInfo->SetReferenceMode( FbxLayerElement::eDirect );
		FbxLayerElementArrayTemplate<int>& SmoothingArray = SmoothingInfo->GetDirectArray();
		Layer->SetSmoothing( SmoothingInfo );

		// This is broken. We are exporting the render mesh but providing smoothing
		// information from the source mesh. The render triangles are not in the
		// same order. Therefore we should export the raw mesh or not export
		// smoothing group information!
		int32 TriangleCount = RenderMesh.RawTriangles.GetElementCount();
		FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)RenderMesh.RawTriangles.Lock( LOCK_READ_ONLY );
		for( int32 TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++ )
		{
			FStaticMeshTriangle* Triangle = ( RawTriangleData++ );

			SmoothingArray.Add( Triangle->SmoothingMask );
		}
		RenderMesh.RawTriangles.Unlock();
	#endif // #if TODO_FBX

		// Create and fill in the vertex color data source.
		const FColorVertexBuffer* ColorBufferToUse = ColorBuffer ? ColorBuffer : &RenderMesh.VertexBuffers.ColorVertexBuffer;
		uint32 ColorVertexCount = ColorBufferToUse->GetNumVertices();

		// Only export vertex colors if they exist
		UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
		if( UnitySettings && UnitySettings->ExportVertexColors && ColorVertexCount > 0 )
		{
			FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create( Mesh, "" );
			VertexColor->SetMappingMode( FbxLayerElement::eByPolygonVertex );
			VertexColor->SetReferenceMode( FbxLayerElement::eIndexToDirect );
			FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
			Layer->SetVertexColors( VertexColor );

			for( int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++ )
			{
				FLinearColor VertColor( 1.0f, 1.0f, 1.0f );
				uint32 UnrealVertIndex = Indices[ FbxVertIndex ];
				if( UnrealVertIndex < ColorVertexCount )
				{
					VertColor = ColorBufferToUse->VertexColor( UnrealVertIndex ).ReinterpretAsLinear();
				}

				VertexColorArray.Add( FbxColor( VertColor.R, VertColor.G, VertColor.B, VertColor.A ) );
			}

			VertexColor->GetIndexArray().SetCount( Indices.Num() );
			for( int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++ )
			{
				VertexColor->GetIndexArray().SetAt( FbxVertIndex, FbxVertIndex );
			}
		}

		if( ( ExportLOD == 0 || ExportLOD == -1 ) && LightmapUVChannel == -1 && ColorBuffer == nullptr && MaterialOrderOverride == nullptr )
		{
			FbxMeshes.Add( StaticMesh, Mesh );
		}
	}
	else
	{
		//Materials in fbx are store in the node and not in the mesh, so even if the mesh was already export
		//we have to find and assign the mesh material.
		const FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport( ExportLOD );
		const int32 PolygonsCount = RenderMesh.Sections.Num();
		uint32 AccountedTriangles = 0;
		for( int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex )
		{
			const FStaticMeshSection& Polygons = RenderMesh.Sections[ PolygonsIndex ];
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			UMaterialInterface* Material = nullptr;

			if( OverrideMaterials && OverrideMaterials->IsValidIndex( Polygons.MaterialIndex ) )
			{
				Material = ( *OverrideMaterials )[ Polygons.MaterialIndex ];
			}
			else
			{
				Material = StaticMesh->GetMaterial( Polygons.MaterialIndex );
			}

			FbxSurfaceMaterial* FbxMaterial =  Material ? ExportMaterial( Material, FBXScene ) : NULL;
			if( !FbxMaterial )
			{
				FbxMaterial = CreateDefaultMaterial( FBXScene );
			}
			FbxActor->AddMaterial( FbxMaterial );
		}
	}

#if (WITH_PHYSX && PHYSICS_INTERFACE_PHYSX) || WITH_CHAOS
	if( ( ExportLOD == 0 || ExportLOD == -1 ) && GetExportOptions( Exporter )->Collision )
	{
		ExportCollisionMesh( Exporter, StaticMesh, MeshName, FbxActor, FBXScene );
	}
#endif


	//Set the original meshes in case it was already existing
	FbxActor->SetNodeAttribute( Mesh );

	//ExportObjectMetadata( StaticMesh, FbxActor );

	return FbxActor;
}
FbxNode* FindActor( AActor* Actor, INodeNameAdapter* NodeNameAdapter )
{
	if( NodeNameAdapter )
	{
		FbxNode* ActorNode = NodeNameAdapter->GetFbxNode( Actor );

		if( ActorNode )
		{
			return ActorNode;
		}
	}

	if( FbxActors.Find( Actor ) )
	{
		return *FbxActors.Find( Actor );
	}
	else
	{
		return NULL;
	}
}
FString GetFbxObjectName( const FString& FbxObjectNode, INodeNameAdapter& NodeNameAdapter )
{
	FString FbxTestName = FbxObjectNode;
	int32* NodeIndex = FbxNodeNameToIndexMap.Find( FbxTestName );
	if( NodeIndex )
	{
		FbxTestName = FString::Printf( TEXT( "%s%d" ), *FbxTestName, *NodeIndex );
		++( *NodeIndex );
	}
	else
	{
		FbxNodeNameToIndexMap.Add( FbxTestName, 1 );
	}
	return FbxTestName;
}
FbxNode* CreateSkeleton( const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes, FbxScene* FBXScene )
{
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 27) || ENGINE_MAJOR_VERSION == 5
		const FReferenceSkeleton& RefSkeleton = SkelMesh->GetRefSkeleton();
	#else
		const FReferenceSkeleton& RefSkeleton = SkelMesh->RefSkeleton;
	#endif
	

	if( RefSkeleton.GetRawBoneNum() == 0 )
	{
		return NULL;
	}

	// Create a list of the nodes we create for each bone, so that children can 
	// later look up their parent
	BoneNodes.Reserve( RefSkeleton.GetRawBoneNum() );

	for( int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetRawBoneNum(); ++BoneIndex )
	{
		const FMeshBoneInfo& CurrentBone = RefSkeleton.GetRefBoneInfo()[ BoneIndex ];
		FTransform& BoneTransform = ( FTransform&)RefSkeleton.GetRefBonePose()[ BoneIndex ];

		FbxString BoneName = Converter.ConvertToFbxString( CurrentBone.ExportName );


		// Create the node's attributes
		FbxSkeleton* SkeletonAttribute = FbxSkeleton::Create( FBXScene, BoneName.Buffer() );
		if( BoneIndex )
		{
			SkeletonAttribute->SetSkeletonType( FbxSkeleton::eLimbNode );
			//SkeletonAttribute->Size.Set(1.0);
		}
		else
		{
			SkeletonAttribute->SetSkeletonType( FbxSkeleton::eRoot );
			//SkeletonAttribute->Size.Set(1.0);
		}


		// Create the node
		FbxNode* BoneNode = FbxNode::Create( FBXScene, BoneName.Buffer() );
		BoneNode->SetNodeAttribute( SkeletonAttribute );

		// Set the bone node's local orientation
		FVector UnrealRotation = BoneTransform.GetRotation().Euler();
		
		FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 0, 0 ) );

		//UTU Flip for Y up
		//FTransform YUpTrans;
		//YUpTrans.SetRotation( Rotator.Quaternion() );
		//BoneTransform = BoneTransform * YUpTrans;

		FbxVector4 LocalPos = Converter.ConvertToFbxPos( BoneTransform.GetTranslation() );
		FbxVector4 LocalRot = Converter.ConvertToFbxRot( UnrealRotation );
		FbxVector4 LocalScale = Converter.ConvertToFbxScale( BoneTransform.GetScale3D() );

		BoneNode->LclTranslation.Set( LocalPos );
		BoneNode->LclRotation.Set( LocalRot );
		BoneNode->LclScaling.Set( LocalScale );


		// If this is not the root bone, attach it to its parent
		if( BoneIndex )
		{
			BoneNodes[ CurrentBone.ParentIndex ]->AddChild( BoneNode );
		}


		// Add the node to the list of nodes, in bone order
		BoneNodes.Push( BoneNode );
	}

	return BoneNodes[ 0 ];
}

bool SetupAnimStack( const UAnimSequence* AnimSeq, FbxScene* FBXScene )
{
	if( GetSequenceLength( AnimSeq ) == 0.f )
	{
		// something is wrong
		return false;
	}
	
	const float FrameRate = FMath::TruncToFloat( ( ( GetNumberOfFrames(AnimSeq) - 1 ) / GetSequenceLength( AnimSeq ) ) + 0.5f );
	
	//Configure the scene time line
	{
		FbxGlobalSettings& SceneGlobalSettings = FBXScene->GetGlobalSettings();
		double CurrentSceneFrameRate = FbxTime::GetFrameRate( SceneGlobalSettings.GetTimeMode() );
		if( !bSceneGlobalTimeLineSet || FrameRate > CurrentSceneFrameRate )
		{
			FbxTime::EMode ComputeTimeMode = FbxTime::ConvertFrameRateToTimeMode( FrameRate );
			FbxTime::SetGlobalTimeMode( ComputeTimeMode, ComputeTimeMode == FbxTime::eCustom ? FrameRate : 0.0 );
			SceneGlobalSettings.SetTimeMode( ComputeTimeMode );
			if( ComputeTimeMode == FbxTime::eCustom )
			{
				SceneGlobalSettings.SetCustomFrameRate( FrameRate );
			}
			bSceneGlobalTimeLineSet = true;
		}
	}

	// set time correctly
	FbxTime ExportedStartTime, ExportedStopTime;
	ExportedStartTime.SetSecondDouble( 0.f );
	ExportedStopTime.SetSecondDouble( GetSequenceLength( AnimSeq ) );

	FbxTimeSpan ExportedTimeSpan;
	ExportedTimeSpan.Set( ExportedStartTime, ExportedStopTime );
	AnimStack->SetLocalTimeSpan( ExportedTimeSpan );

	return true;
}
#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <=1 )
void IterateInsideAnimSequence( const UAnimSequence* AnimSeq, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime, TFunctionRef<void( float, FbxTime, bool )> IterationLambda )
{
	float AnimTime = AnimStartOffset;
	float AnimEndTime = ( GetSequenceLength( AnimSeq ) - AnimEndOffset );
	// Subtracts 1 because NumFrames includes an initial pose for 0.0 second
	double TimePerKey = ( GetSequenceLength( AnimSeq ) / ( GetNumberOfFrames( AnimSeq ) - 1 ) );
	const float AnimTimeIncrement = TimePerKey * AnimPlayRate;
	uint32 AnimFrameIndex = 0;

	FbxTime ExportTime;
	ExportTime.SetSecondDouble( StartTime );

	FbxTime ExportTimeIncrement;
	ExportTimeIncrement.SetSecondDouble( TimePerKey );

	// Step through each frame and add custom curve data
	bool bLastKey = false;
	while( !bLastKey )
	{
		bLastKey = ( AnimTime + KINDA_SMALL_NUMBER ) > AnimEndTime;

		IterationLambda( AnimTime, ExportTime, bLastKey );

		ExportTime += ExportTimeIncrement;
		AnimFrameIndex++;
		AnimTime = AnimStartOffset + ( (float)AnimFrameIndex * AnimTimeIncrement );
	}
}
void ExportCustomAnimCurvesToFbx( const TMap<FName, FbxAnimCurve*>& CustomCurves, const UAnimSequence* AnimSeq,
								  float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime, float ValueScale,
								  FbxScene* FBXScene )
{
	// stack allocator for extracting curve
	FMemMark Mark( FMemStack::Get() );
	const USkeleton* Skeleton = AnimSeq->GetSkeleton();
	const FSmartNameMapping* SmartNameMapping = Skeleton ? Skeleton->GetSmartNameContainer( USkeleton::AnimCurveMappingName ) : nullptr;

	if( !Skeleton || !SmartNameMapping || !SetupAnimStack( AnimSeq, FBXScene ) )
	{
		//Something is wrong.
		return;
	}

	TArray<SmartName::UID_Type> AnimCurveUIDs;
	{
		//We need to recreate the UIDs array manually so that we keep the empty entries otherwise the BlendedCurve won't have the correct mapping.
		TArray<FName> UID_ToNameArray;
		SmartNameMapping->FillUIDToNameArray( UID_ToNameArray );
		AnimCurveUIDs.Reserve( UID_ToNameArray.Num() );
		for( int32 NameIndex = 0; NameIndex < UID_ToNameArray.Num(); ++NameIndex )
		{
			AnimCurveUIDs.Add( NameIndex );
		}
	}

	for( auto CustomCurve : CustomCurves )
	{
		CustomCurve.Value->KeyModifyBegin();
	}

	auto ExportLambda = [&]( float AnimTime, FbxTime ExportTime, bool bLastKey )
	{
		FBlendedCurve BlendedCurve;
		BlendedCurve.InitFrom( &AnimCurveUIDs );
		AnimSeq->EvaluateCurveData( BlendedCurve, AnimTime, true );
		if( BlendedCurve.IsValid() )
		{
			//Loop over the custom curves and add the actual keys
			for( auto CustomCurve : CustomCurves )
			{
				SmartName::UID_Type NameUID = Skeleton->GetUIDByName( USkeleton::AnimCurveMappingName, CustomCurve.Key );
				if( NameUID != SmartName::MaxUID )
				{
					float CurveValueAtTime = BlendedCurve.Get( NameUID ) * ValueScale;
					int32 KeyIndex = CustomCurve.Value->KeyAdd( ExportTime );
					CustomCurve.Value->KeySetValue( KeyIndex, CurveValueAtTime );
				}
			}
		}
	};

	IterateInsideAnimSequence( AnimSeq, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime, ExportLambda );

	for( auto CustomCurve : CustomCurves )
	{
		CustomCurve.Value->KeyModifyEnd();
	}
}
void ExportAnimSequenceToFbx( FFbxExporter* Exporter, const UAnimSequence* AnimSeq,
							  const USkeletalMesh* SkelMesh,
							  TArray<FbxNode*>& BoneNodes,
							  FbxAnimLayer* InAnimLayer,
							  float AnimStartOffset,
							  float AnimEndOffset,
							  float AnimPlayRate,
							  float StartTime,
							  FbxScene* FBXScene )
{
	// stack allocator for extracting curve
	FMemMark Mark( FMemStack::Get() );

	USkeleton* Skeleton = AnimSeq->GetSkeleton();

	if( Skeleton == nullptr || !SetupAnimStack( AnimSeq, FBXScene ) )
	{
		// something is wrong
		return;
	}

	//Prepare root anim curves data to be exported
	TArray<FName> AnimCurveNames;
	TMap<FName, FbxAnimCurve*> CustomCurveMap;
	if( BoneNodes.Num() > 0 )
	{
		const FSmartNameMapping* AnimCurveMapping = Skeleton->GetSmartNameContainer( USkeleton::AnimCurveMappingName );

		if( AnimCurveMapping )
		{
			AnimCurveMapping->FillNameArray( AnimCurveNames );

			const UFbxExportOption* ExportOptions = GetExportOptions( Exporter );
			const bool bExportMorphTargetCurvesInMesh = ExportOptions && ExportOptions->bExportPreviewMesh && ExportOptions->bExportMorphTargets;

			for( auto AnimCurveName : AnimCurveNames )
			{
				const FCurveMetaData* CurveMetaData = AnimCurveMapping->GetCurveMetaData( AnimCurveName );

				//Only export the custom curve if it is not used in a MorphTarget that will be exported latter on.
				if( !( bExportMorphTargetCurvesInMesh && CurveMetaData && CurveMetaData->Type.bMorphtarget ) )
				{
					FbxProperty AnimCurveFbxProp = FbxProperty::Create( BoneNodes[ 0 ], FbxDoubleDT, TCHAR_TO_ANSI( *AnimCurveName.ToString() ) );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
					CustomCurveMap.Add( AnimCurveName, AnimFbxCurve );
				}
			}
		}
	}

	ExportCustomAnimCurvesToFbx( CustomCurveMap, AnimSeq, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime, 1.f, FBXScene );

	TArray<FCustomAttribute> CustomAttributes;

	// Add the animation data to the bone nodes
	for( int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex )
	{
		FbxNode* CurrentBoneNode = BoneNodes[ BoneIndex ];
		int32 BoneTreeIndex = Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex( SkelMesh, BoneIndex );
		int32 BoneTrackIndex = Skeleton->GetRawAnimationTrackIndex( BoneTreeIndex, AnimSeq );
		FName BoneName = Skeleton->GetReferenceSkeleton().GetBoneName( BoneTreeIndex );

		CustomAttributes.Reset();
		//AnimSeq->GetDataModel()->GetAttributesForBone( BoneName, CustomAttributes );

		TArray<TPair<int32, FbxAnimCurve*>> FloatCustomAttributeIndices;
		TArray<TPair<int32, FbxAnimCurve*>> IntCustomAttributeIndices;

		// Setup custom attribute properties and curves
		for( int32 AttributeIndex = 0; AttributeIndex < CustomAttributes.Num(); ++AttributeIndex )
		{
			const FCustomAttribute& Attribute = CustomAttributes[ AttributeIndex ];
			const FName& AttributeName = Attribute.Name;

			const EVariantTypes VariantType = static_cast<EVariantTypes>( Attribute.VariantType );

			if( VariantType == EVariantTypes::Int32 )
			{
				FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxIntDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

				FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
				AnimFbxCurve->KeyModifyBegin();
				IntCustomAttributeIndices.Emplace( AttributeIndex, AnimFbxCurve );
			}
			else if( VariantType == EVariantTypes::Float )
			{
				FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxFloatDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

				FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
				AnimFbxCurve->KeyModifyBegin();
				FloatCustomAttributeIndices.Emplace( AttributeIndex, AnimFbxCurve );
			}
			else if( VariantType == EVariantTypes::String )
			{
				FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxStringDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

				// String attributes can't be keyed, simply set a normal value.
				FString AttributeValue;
			#if ENGINE_MAJOR_VERSION == 4
				FCustomAttributesRuntime::GetAttributeValue( Attribute, 0.f, AttributeValue );
			#endif
				FbxString FbxValueString( TCHAR_TO_UTF8( *AttributeValue ) );
				AnimCurveFbxProp.Set( FbxValueString );
			}
			else
			{
				ensureMsgf( false, TEXT( "Trying to export unsupported custom attribte (float, int32 and FString are currently supported)" ) );
			}
		}

		// Create the transform AnimCurves
		const uint32 NumberOfCurves = 9;
		FbxAnimCurve* Curves[ NumberOfCurves ];

		// Individual curves for translation, rotation and scaling
		Curves[ 0 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 1 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 2 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		Curves[ 3 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 4 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 5 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		Curves[ 6 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 7 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 8 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		if( BoneTrackIndex == INDEX_NONE )
		{
			// If this sequence does not have a track for the current bone, then skip it
			continue;
		}

		for( FbxAnimCurve* Curve : Curves )
		{
			Curve->KeyModifyBegin();
		}

		auto ExportLambda = [&]( float AnimTime, FbxTime ExportTime, bool bLastKey )
		{
			FTransform BoneAtom;
			AnimSeq->GetBoneTransform( BoneAtom, BoneTrackIndex, AnimTime, true );

			FbxAMatrix FbxMatrix = Converter.ConvertMatrix( BoneAtom.ToMatrixWithScale() );

			FbxVector4 Translation = Converter.ConvertToFbxPos( BoneAtom.GetTranslation() );
			FbxVector4 Rotation = Converter.ConvertToFbxRot( BoneAtom.GetRotation().Euler() );
			FbxVector4 Scale = Converter.ConvertToFbxScale( BoneAtom.GetScale3D() );
			FbxVector4 Vectors[ 3 ] = { Translation, Rotation, Scale };

			// Loop over each curve and channel to set correct values
			for( uint32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex )
			{
				for( uint32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex )
				{
					uint32 OffsetCurveIndex = ( CurveIndex * 3 ) + ChannelIndex;

					int32 lKeyIndex = Curves[ OffsetCurveIndex ]->KeyAdd( ExportTime );
					Curves[ OffsetCurveIndex ]->KeySetValue( lKeyIndex, Vectors[ CurveIndex ][ ChannelIndex ] );
					Curves[ OffsetCurveIndex ]->KeySetInterpolation( lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic );

					if( bLastKey )
					{
						Curves[ OffsetCurveIndex ]->KeySetConstantMode( lKeyIndex, FbxAnimCurveDef::eConstantStandard );
					}
				}
			}

		#if ENGINE_MAJOR_VERSION == 4
			for( TPair<int32, FbxAnimCurve*>& CurrentAttributeCurve : FloatCustomAttributeIndices )
			{
				float AttributeValue = 0.f;
				FCustomAttributesRuntime::GetAttributeValue( CustomAttributes[ CurrentAttributeCurve.Key ], AnimTime, AttributeValue );
				int32 KeyIndex = CurrentAttributeCurve.Value->KeyAdd( ExportTime );
				CurrentAttributeCurve.Value->KeySetValue( KeyIndex, AttributeValue );
			}

			for( TPair<int32, FbxAnimCurve*>& CurrentAttributeCurve : IntCustomAttributeIndices )
			{
				int32 AttributeValue = 0;
				FCustomAttributesRuntime::GetAttributeValue( CustomAttributes[ CurrentAttributeCurve.Key ], AnimTime, AttributeValue );
				int32 KeyIndex = CurrentAttributeCurve.Value->KeyAdd( ExportTime );
				CurrentAttributeCurve.Value->KeySetValue( KeyIndex, static_cast<float>( AttributeValue ) );
			}
		#endif
		};

		IterateInsideAnimSequence( AnimSeq, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime, ExportLambda );

		for( FbxAnimCurve* Curve : Curves )
		{
			Curve->KeyModifyEnd();
		}

		auto MarkCurveEnd = []( auto& CurvesArray )
		{
			for( auto& CurvePair : CurvesArray )
			{
				CurvePair.Value->KeyModifyEnd();
			}
		};

		MarkCurveEnd( FloatCustomAttributeIndices );
		MarkCurveEnd( IntCustomAttributeIndices );
	}
}
#else
void IterateInsideAnimSequence( const UAnimSequence* AnimSeq, FFrameTime StartFrameTime, FFrameTime EndFrameTime, float FrameRateScale, float StartTime, TFunctionRef<void( double, FbxTime, bool )> IterationLambda )
{
	const double TimePerKey = AnimSeq->GetDataModel()->GetFrameRate().AsInterval();
	const double AnimTimeIncrement = TimePerKey * FrameRateScale;
	uint32 AnimFrameIndex = 0;

	FbxTime ExportTime;
	ExportTime.SetSecondDouble( StartTime );

	FbxTime ExportTimeIncrement;
	ExportTimeIncrement.SetSecondDouble( TimePerKey );

	// Step through each frame and add custom curve data
	bool bLastKey = false;
	FFrameTime FrameTime = StartFrameTime;
	const FFrameRate& FrameRate = AnimSeq->GetDataModel()->GetFrameRate();
	while( !bLastKey )
	{
		bLastKey = FrameTime > EndFrameTime;
		IterationLambda( FrameRate.AsSeconds( FrameTime ), ExportTime, bLastKey );

		ExportTime += ExportTimeIncrement;
		AnimFrameIndex++;
		FrameTime += FFrameTime::FromDecimal( FrameRateScale );
	}
}
void ExportAnimSequenceToFbx( FFbxExporter* Exporter, const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes, FbxAnimLayer* InAnimLayer, FFrameTime StartFrameTime, FFrameTime EndFrameTime, float FrameRateScale, float StartTime,
							  FbxScene* FBXScene )
{
	// stack allocator for extracting curve
	FMemMark Mark( FMemStack::Get() );

	USkeleton* Skeleton = AnimSeq->GetSkeleton();

	if( Skeleton == nullptr || !SetupAnimStack( AnimSeq, FBXScene ) )
	{
		// something is wrong
		return;
	}

	//Prepare root anim curves data to be exported
	TArray<FName> AnimCurveNames;
	TMap<FName, FbxAnimCurve*> CustomCurveMap;
	if( BoneNodes.Num() > 0 )
	{
		const UFbxExportOption* ExportOptions = Exporter->GetExportOptions();

		#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <=2 )
		const FSmartNameMapping* AnimCurveMapping = Skeleton->GetSmartNameContainer( USkeleton::AnimCurveMappingName );

		if( AnimCurveMapping )
		{
			AnimCurveMapping->FillNameArray( AnimCurveNames );
			
			const bool bExportMorphTargetCurvesInMesh = ExportOptions && ExportOptions->bExportPreviewMesh && ExportOptions->bExportMorphTargets;

			for( auto AnimCurveName : AnimCurveNames )
			{
				const FCurveMetaData* CurveMetaData = AnimCurveMapping->GetCurveMetaData( AnimCurveName );

				//Only export the custom curve if it is not used in a MorphTarget that will be exported latter on.
				if( !( bExportMorphTargetCurvesInMesh && CurveMetaData && CurveMetaData->Type.bMorphtarget ) )
				{
					FbxProperty AnimCurveFbxProp = FbxProperty::Create( BoneNodes[ 0 ], FbxDoubleDT, TCHAR_TO_ANSI( *AnimCurveName.ToString() ) );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
					CustomCurveMap.Add( AnimCurveName, AnimFbxCurve );
				}
			}
		}
		#endif
		#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		
		const bool bExportMorphTargetCurvesInMesh = ExportOptions && ExportOptions->bExportPreviewMesh && ExportOptions->bExportMorphTargets;

		Skeleton->ForEachCurveMetaData( [&CustomCurveMap, &BoneNodes, InAnimLayer, bExportMorphTargetCurvesInMesh]( const FName& InCurveName, const FCurveMetaData& InMetaData )
			{
				//Only export the custom curve if it is not used in a MorphTarget that will be exported latter on.
				if (!(bExportMorphTargetCurvesInMesh && InMetaData.Type.bMorphtarget))
				{
					FbxProperty AnimCurveFbxProp = FbxProperty::Create( BoneNodes[0], FbxDoubleDT, TCHAR_TO_ANSI( *InCurveName.ToString() ) );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
					CustomCurveMap.Add( InCurveName, AnimFbxCurve );
				}
			} );
		#endif
	}

#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <=1 )
	ExportCustomAnimCurvesToFbx( CustomCurveMap, AnimSeq, StartFrameTime, EndFrameTime, FrameRateScale, StartTime, 1.f, FBXScene );
#endif

	TArray<const FAnimatedBoneAttribute*> CustomAttributes;

	// Add the animation data to the bone nodes
	for( int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex )
	{
		FbxNode* CurrentBoneNode = BoneNodes[ BoneIndex ];
		const int32 BoneTreeIndex = Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex( SkelMesh, BoneIndex );
		const FName BoneName = Skeleton->GetReferenceSkeleton().GetBoneName( BoneTreeIndex );
		const IAnimationDataModel* DataModel = AnimSeq->GetDataModel();

		CustomAttributes.Reset();
		DataModel->GetAttributesForBone( BoneName, CustomAttributes );

		TArray<TPair<int32, FbxAnimCurve*>> FloatCustomAttributeIndices;
		TArray<TPair<int32, FbxAnimCurve*>> IntCustomAttributeIndices;

		// Setup custom attribute properties and curves
		for( int32 AttributeIndex = 0; AttributeIndex < CustomAttributes.Num(); ++AttributeIndex )
		{
			if( const FAnimatedBoneAttribute* AttributePtr = CustomAttributes[ AttributeIndex ] )
			{
				const FAnimatedBoneAttribute& Attribute = *AttributePtr;

				const FName& AttributeName = Attribute.Identifier.GetName();
				const UScriptStruct* AttributeType = Attribute.Identifier.GetType();

				if( AttributeType == FIntegerAnimationAttribute::StaticStruct() )
				{
					FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxIntDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

					FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
					AnimFbxCurve->KeyModifyBegin();
					IntCustomAttributeIndices.Emplace( AttributeIndex, AnimFbxCurve );
				}
				else if( AttributeType == FFloatAnimationAttribute::StaticStruct() )
				{
					FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxFloatDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

					FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
					AnimFbxCurve->KeyModifyBegin();
					FloatCustomAttributeIndices.Emplace( AttributeIndex, AnimFbxCurve );
				}
				else if( AttributeType == FStringAnimationAttribute::StaticStruct() )
				{
					FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxStringDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

					// String attributes can't be keyed, simply set a normal value.
					FStringAnimationAttribute EvaluatedAttribute = Attribute.Curve.Evaluate<FStringAnimationAttribute>( 0.f );

					FbxString FbxValueString( TCHAR_TO_UTF8( *EvaluatedAttribute.Value ) );
					AnimCurveFbxProp.Set( FbxValueString );
				}
				else
				{
					ensureMsgf( false, TEXT( "Trying to export unsupported custom attribte (float, int32 and FString are currently supported)" ) );
				}
			}
		}

		// Create the transform AnimCurves
		const uint32 NumberOfCurves = 9;
		FbxAnimCurve* Curves[ NumberOfCurves ];

		// Individual curves for translation, rotation and scaling
		Curves[ 0 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 1 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 2 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		Curves[ 3 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 4 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 5 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		Curves[ 6 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 7 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 8 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		if( !DataModel->IsValidBoneTrackName( BoneName ) )
		{
			// If this sequence does not have a track for the current bone, then skip it
			continue;
		}

		for( FbxAnimCurve* Curve : Curves )
		{
			Curve->KeyModifyBegin();
		}

		auto ExportLambda = [DataModel, BoneName, AnimSeq, &FloatCustomAttributeIndices, &IntCustomAttributeIndices, &Curves, &CustomAttributes]( double AnimTime, FbxTime ExportTime, bool bLastKey )
		{
			const FTransform BoneAtom = DataModel->EvaluateBoneTrackTransform( BoneName, DataModel->GetFrameRate().AsFrameTime( AnimTime ), AnimSeq->Interpolation );
			const FbxAMatrix FbxMatrix = Converter.ConvertMatrix( BoneAtom.ToMatrixWithScale() );

			const FbxVector4 Translation = FbxMatrix.GetT();
			const FbxVector4 Rotation = FbxMatrix.GetR();
			const FbxVector4 Scale = FbxMatrix.GetS();
			const FbxVector4 Vectors[ 3 ] = { Translation, Rotation, Scale };

			// Loop over each curve and channel to set correct values
			for( uint32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex )
			{
				for( uint32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex )
				{
					const uint32 OffsetCurveIndex = ( CurveIndex * 3 ) + ChannelIndex;

					const int32 lKeyIndex = Curves[ OffsetCurveIndex ]->KeyAdd( ExportTime );
					Curves[ OffsetCurveIndex ]->KeySetValue( lKeyIndex, Vectors[ CurveIndex ][ ChannelIndex ] );
					Curves[ OffsetCurveIndex ]->KeySetInterpolation( lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic );

					if( bLastKey )
					{
						Curves[ OffsetCurveIndex ]->KeySetConstantMode( lKeyIndex, FbxAnimCurveDef::eConstantStandard );
					}
				}
			}

			for( const TPair<int32, FbxAnimCurve*>& CurrentAttributeCurve : FloatCustomAttributeIndices )
			{
				if( const FAnimatedBoneAttribute* AttributePtr = CustomAttributes[ CurrentAttributeCurve.Key ] )
				{
					ensure( AttributePtr->Identifier.GetType() == FFloatAnimationAttribute::StaticStruct() );

					const FFloatAnimationAttribute EvaluatedAttribute = AttributePtr->Curve.Evaluate<FFloatAnimationAttribute>( AnimTime );
					const int32 KeyIndex = CurrentAttributeCurve.Value->KeyAdd( ExportTime );
					CurrentAttributeCurve.Value->KeySetValue( KeyIndex, EvaluatedAttribute.Value );

				}
			}

			for( const TPair<int32, FbxAnimCurve*>& CurrentAttributeCurve : IntCustomAttributeIndices )
			{
				if( const FAnimatedBoneAttribute* AttributePtr = CustomAttributes[ CurrentAttributeCurve.Key ] )
				{
					ensure( AttributePtr->Identifier.GetType() == FIntegerAnimationAttribute::StaticStruct() );

					const FIntegerAnimationAttribute EvaluatedAttribute = AttributePtr->Curve.Evaluate<FIntegerAnimationAttribute>( AnimTime );
					const int32 KeyIndex = CurrentAttributeCurve.Value->KeyAdd( ExportTime );
					CurrentAttributeCurve.Value->KeySetValue( KeyIndex, static_cast<float>( EvaluatedAttribute.Value ) );
				}
			}
		};

		IterateInsideAnimSequence( AnimSeq, StartFrameTime, EndFrameTime, FrameRateScale, StartTime, ExportLambda );

		for( FbxAnimCurve* Curve : Curves )
		{
			Curve->KeyModifyEnd();
		}

		auto MarkCurveEnd = []( auto& CurvesArray )
		{
			for( auto& CurvePair : CurvesArray )
			{
				CurvePair.Value->KeyModifyEnd();
			}
		};

		MarkCurveEnd( FloatCustomAttributeIndices );
		MarkCurveEnd( IntCustomAttributeIndices );
	}
}
#endif

void CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* InAnimLayer )
{
	// Add the animation data to the bone nodes
	for( int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex )
	{
		FbxNode* CurrentBoneNode = BoneNodes[ BoneIndex ];

		// Fetch the AnimCurves
		FbxAnimCurve* Curves[ 3 ];
		Curves[ 0 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 1 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 2 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		for( int32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex )
		{
			FbxAnimCurve* CurrentCurve = Curves[ CurveIndex ];
			CurrentCurve->KeyModifyBegin();

			float CurrentAngleOffset = 0.f;
			for( int32 KeyIndex = 1; KeyIndex < CurrentCurve->KeyGetCount(); ++KeyIndex )
			{
				float PreviousOutVal = CurrentCurve->KeyGetValue( KeyIndex - 1 );
				float CurrentOutVal = CurrentCurve->KeyGetValue( KeyIndex );

				float DeltaAngle = ( CurrentOutVal + CurrentAngleOffset ) - PreviousOutVal;

				if( DeltaAngle >= 180 )
				{
					CurrentAngleOffset -= 360;
				}
				else if( DeltaAngle <= -180 )
				{
					CurrentAngleOffset += 360;
				}

				CurrentOutVal += CurrentAngleOffset;

				CurrentCurve->KeySetValue( KeyIndex, CurrentOutVal );
			}

			CurrentCurve->KeyModifyEnd();
		}
	}
}
void BindMeshToSkeleton( const USkeletalMesh* SkelMesh, FbxNode* MeshRootNode, TArray<FbxNode*>& BoneNodes, int32 LODIndex, FbxScene* FBXScene )
{
	const FSkeletalMeshModel* SkelMeshResource = SkelMesh->GetImportedModel();
	if( !SkelMeshResource->LODModels.IsValidIndex( LODIndex ) )
	{
		//We cannot bind the LOD if its not valid
		return;
	}
	const FSkeletalMeshLODModel& SourceModel = SkelMeshResource->LODModels[ LODIndex ];
	const int32 VertexCount = SourceModel.NumVertices;

	FbxAMatrix MeshMatrix;

	FbxScene* lScene = MeshRootNode->GetScene();
	if( lScene )
	{
		MeshMatrix = MeshRootNode->EvaluateGlobalTransform();
	}

	FbxGeometry* MeshAttribute = (FbxGeometry*)MeshRootNode->GetNodeAttribute();
	FbxSkin* Skin = FbxSkin::Create( FBXScene, "" );

	const int32 BoneCount = BoneNodes.Num();
	for( int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex )
	{
		FbxNode* BoneNode = BoneNodes[ BoneIndex ];

		// Create the deforming cluster
		FbxCluster* CurrentCluster = FbxCluster::Create( FBXScene, "" );
		CurrentCluster->SetLink( BoneNode );
		CurrentCluster->SetLinkMode( FbxCluster::eTotalOne );

		// Add all the vertices that are weighted to the current skeletal bone to the cluster
		// NOTE: the bone influence indices contained in the vertex data are based on a per-chunk
		// list of verts.  The convert the chunk bone index to the mesh bone index, the chunk's
		// boneMap is needed
		int32 VertIndex = 0;
		const int32 SectionCount = SourceModel.Sections.Num();
		for( int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex )
		{
			const FSkelMeshSection& Section = SourceModel.Sections[ SectionIndex ];
			if( 0 )//Section.HasClothingData() )
			{
				continue;
			}

			for( int32 SoftIndex = 0; SoftIndex < Section.SoftVertices.Num(); ++SoftIndex )
			{
				const FSoftSkinVertex& Vert = Section.SoftVertices[ SoftIndex ];

				for( int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex )
				{
					int32 InfluenceBone = Section.BoneMap[ Vert.InfluenceBones[ InfluenceIndex ] ];
					float InfluenceWeight = Vert.InfluenceWeights[ InfluenceIndex ] / 255.f;

					if( InfluenceBone == BoneIndex && InfluenceWeight > 0.f )
					{
						CurrentCluster->AddControlPointIndex( VertIndex, InfluenceWeight );
					}
				}

				++VertIndex;
			}
		}

		// Now we have the Patch and the skeleton correctly positioned,
		// set the Transform and TransformLink matrix accordingly.
		CurrentCluster->SetTransformMatrix( MeshMatrix );

		FbxAMatrix LinkMatrix;
		if( lScene )
		{
			LinkMatrix = BoneNode->EvaluateGlobalTransform();
		}

		CurrentCluster->SetTransformLinkMatrix( LinkMatrix );

		// Add the clusters to the mesh by creating a skin and adding those clusters to that skin.
		Skin->AddCluster( CurrentCluster );
	}

	// Add the skin to the mesh after the clusters have been added
	MeshAttribute->AddDeformer( Skin );
}
void AddNodeRecursively( FbxArray<FbxNode*>& pNodeArray, FbxNode* pNode )
{
	if( pNode )
	{
		AddNodeRecursively( pNodeArray, pNode->GetParent() );

		if( pNodeArray.Find( pNode ) == -1 )
		{
			// Node not in the list, add it
			pNodeArray.Add( pNode );
		}
	}
}
void CreateBindPose( FbxNode* MeshRootNode, FbxScene* FBXScene )
{
	if( !MeshRootNode )
	{
		return;
	}

	// In the bind pose, we must store all the link's global matrix at the time of the bind.
	// Plus, we must store all the parent(s) global matrix of a link, even if they are not
	// themselves deforming any model.

	// Create a bind pose with the link list

	FbxArray<FbxNode*> lClusteredFbxNodes;
	int                       i, j;

	if( MeshRootNode->GetNodeAttribute() )
	{
		int lSkinCount = 0;
		int lClusterCount = 0;
		switch( MeshRootNode->GetNodeAttribute()->GetAttributeType() )
		{
			case FbxNodeAttribute::eMesh:
			case FbxNodeAttribute::eNurbs:
			case FbxNodeAttribute::ePatch:

				lSkinCount = ( (FbxGeometry*)MeshRootNode->GetNodeAttribute() )->GetDeformerCount( FbxDeformer::eSkin );
				//Go through all the skins and count them
				//then go through each skin and get their cluster count
				for( i = 0; i < lSkinCount; ++i )
				{
					FbxSkin* lSkin = (FbxSkin*)( (FbxGeometry*)MeshRootNode->GetNodeAttribute() )->GetDeformer( i, FbxDeformer::eSkin );
					lClusterCount += lSkin->GetClusterCount();
				}
				break;
		}
		//if we found some clusters we must add the node
		if( lClusterCount )
		{
			//Again, go through all the skins get each cluster link and add them
			for( i = 0; i < lSkinCount; ++i )
			{
				FbxSkin* lSkin = (FbxSkin*)( (FbxGeometry*)MeshRootNode->GetNodeAttribute() )->GetDeformer( i, FbxDeformer::eSkin );
				lClusterCount = lSkin->GetClusterCount();
				for( j = 0; j < lClusterCount; ++j )
				{
					FbxNode* lClusterNode = lSkin->GetCluster( j )->GetLink();
					AddNodeRecursively( lClusteredFbxNodes, lClusterNode );
				}

			}

			// Add the patch to the pose
			lClusteredFbxNodes.Add( MeshRootNode );
		}
	}

	// Now create a bind pose with the link list
	if( lClusteredFbxNodes.GetCount() )
	{
		// A pose must be named. Arbitrarily use the name of the patch node.
		FbxPose* lPose = FbxPose::Create( FBXScene, MeshRootNode->GetName() );

		// default pose type is rest pose, so we need to set the type as bind pose
		lPose->SetIsBindPose( true );

		for( i = 0; i < lClusteredFbxNodes.GetCount(); i++ )
		{
			FbxNode* lKFbxNode = lClusteredFbxNodes.GetAt( i );
			FbxMatrix lBindMatrix = lKFbxNode->EvaluateGlobalTransform();

			lPose->Add( lKFbxNode, lBindMatrix );
		}

		// Add the pose to the scene
		FBXScene->AddPose( lPose );
	}
}
FbxSurfaceMaterial* ExportMaterial( UMaterialInterface* MaterialInterface, FbxScene* FBXScene );
FbxNode* CreateMesh( FFbxExporter* Exporter, const USkeletalMesh* SkelMesh, const TCHAR* MeshName, int32 LODIndex, const UAnimSequence* AnimSeq /*= nullptr*/, const TArray<UMaterialInterface*>* OverrideMaterials /*= nullptr*/,
					 FbxScene* FBXScene )
{
	const FSkeletalMeshModel* SkelMeshResource = SkelMesh->GetImportedModel();
	if( !SkelMeshResource->LODModels.IsValidIndex( LODIndex ) )
	{
		//Return an empty node
		return FbxNode::Create( FBXScene, TCHAR_TO_UTF8( MeshName ) );
	}

	const FSkeletalMeshLODModel& SourceModel = SkelMeshResource->LODModels[ LODIndex ];

	//Current code works with 5.4 & hoodies with cloth from https://www.unrealengine.com/marketplace/en-US/product/modular-sorcerer-male-humans-fantasy-collection
	//const int32 VertexCount = SourceModel.GetNumNonClothingVertices();
	const int32 VertexCount = SourceModel.NumVertices;

	// Verify the integrity of the mesh.
	if( VertexCount == 0 ) return NULL;

	// Copy all the vertex data from the various chunks to a single buffer.
	// Makes the rest of the code in this function cleaner and easier to maintain.  
	TArray<FSoftSkinVertex> Vertices;
	//SourceModel.GetNonClothVertices( Vertices );
	SourceModel.GetVertices( Vertices );
	//if( Vertices.Num() != VertexCount ) return NULL;

	FbxMesh* Mesh = FbxMesh::Create( FBXScene, TCHAR_TO_UTF8( MeshName ) );

	//UTU - aparently skinning needs different rotation ?
	FRotator Rotator = FRotator::MakeFromEuler( FVector( -90, 0, 0 ) );//preview FBX the same as UE

	// Create and fill in the vertex position data source.
	Mesh->InitControlPoints( VertexCount );
	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	for( int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex )
	{
		FVector Position = (FVector)Vertices[ VertIndex ].Position;
		//Position = Rotator.RotateVector( Position );
		ControlPoints[ VertIndex ] = Converter.ConvertToFbxPos( Position );
	}

	// Create Layer 0 to hold the normals
	FbxLayer* LayerZero = Mesh->GetLayer( 0 );
	if( LayerZero == NULL )
	{
		Mesh->CreateLayer();
		LayerZero = Mesh->GetLayer( 0 );
	}

	// Create and fill in the per-face-vertex normal data source.
	// We extract the Z-tangent and drop the X/Y-tangents which are also stored in the render mesh.
	FbxLayerElementNormal* LayerElementNormal = FbxLayerElementNormal::Create( Mesh, "" );

	LayerElementNormal->SetMappingMode( FbxLayerElement::eByControlPoint );
	// Set the normal values for every control point.
	LayerElementNormal->SetReferenceMode( FbxLayerElement::eDirect );

	for( int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex )
	{
		FVector Normal;
		#if ENGINE_MAJOR_VERSION >= 5
			FVector4f Normal4 = Vertices[ VertIndex ].TangentZ;
			Normal = FVector( Normal4.X, Normal4.Y, Normal4.Z );
		#else
			Normal = Vertices[ VertIndex ].TangentZ;
		#endif

		//Normal.Y *= -1;
		//Normal = Rotator.RotateVector( Normal );

		FbxVector4 FbxNormal = Converter.ConvertToFbxPos( Normal );

		LayerElementNormal->GetDirectArray().Add( FbxNormal );
	}

	LayerZero->SetNormals( LayerElementNormal );


	// Create and fill in the per-face-vertex texture coordinate data source(s).
	// Create UV for Diffuse channel.
	const int32 TexCoordSourceCount = SourceModel.NumTexCoords;
	TCHAR UVChannelName[ 32 ];
	for( int32 TexCoordSourceIndex = 0; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex )
	{
		FbxLayer* Layer = Mesh->GetLayer( TexCoordSourceIndex );
		if( Layer == NULL )
		{
			Mesh->CreateLayer();
			Layer = Mesh->GetLayer( TexCoordSourceIndex );
		}

		if( TexCoordSourceIndex == 1 )
		{
			FCString::Sprintf( UVChannelName, TEXT( "LightMapUV" ) );
		}
		else
		{
			FCString::Sprintf( UVChannelName, TEXT( "DiffuseUV" ) );
		}

		FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create( Mesh, TCHAR_TO_UTF8( UVChannelName ) );
		UVDiffuseLayer->SetMappingMode( FbxLayerElement::eByControlPoint );
		UVDiffuseLayer->SetReferenceMode( FbxLayerElement::eDirect );

		// Create the texture coordinate data source.
		for( int32 TexCoordIndex = 0; TexCoordIndex < VertexCount; ++TexCoordIndex )
		{
			#if ENGINE_MAJOR_VERSION == 4
			const FVector2D& TexCoord = Vertices[ TexCoordIndex ].UVs[ TexCoordSourceIndex ];
			#else
			const FVector2f& TexCoord = Vertices[ TexCoordIndex ].UVs[ TexCoordSourceIndex ];
			#endif
			UVDiffuseLayer->GetDirectArray().Add( FbxVector2( TexCoord.X, -TexCoord.Y + 1.0 ) );
		}

		Layer->SetUVs( UVDiffuseLayer, FbxLayerElement::eTextureDiffuse );
	}

	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create( Mesh, "" );
	MatLayer->SetMappingMode( FbxLayerElement::eByPolygon );
	MatLayer->SetReferenceMode( FbxLayerElement::eIndexToDirect );
	LayerZero->SetMaterials( MatLayer );


	// Create the per-material polygons sets.
	int32 SectionCount = SourceModel.Sections.Num();
	int32 ClothSectionVertexRemoveOffset = 0;
	TArray<TPair<uint32, uint32>> VertexIndexOffsetPairArray{ TPair<uint32, uint32>( 0,0 ) };
	for( int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex )
	{
		const FSkelMeshSection& Section = SourceModel.Sections[ SectionIndex ];
		//if( Section.HasClothingData() )
		//{
		//	ClothSectionVertexRemoveOffset += Section.GetNumVertices();
		//	VertexIndexOffsetPairArray.Emplace( Section.BaseVertexIndex, ClothSectionVertexRemoveOffset );
		//	continue;
		//}

		int32 MatIndex = Section.MaterialIndex;

		// Static meshes contain one triangle list per element.
		int32 TriangleCount = Section.NumTriangles;

		// Copy over the index buffer into the FBX polygons set.
		for( int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex )
		{
			Mesh->BeginPolygon( MatIndex );
			for( int32 PointIndex = 0; PointIndex < 3; PointIndex++ )
			{
				int32 VertexPositionIndex = SourceModel.IndexBuffer[ Section.BaseIndex + ( ( TriangleIndex * 3 ) + PointIndex ) ] - ClothSectionVertexRemoveOffset;
				check( VertexPositionIndex >= 0 );
				Mesh->AddPolygon( VertexPositionIndex );
			}
			Mesh->EndPolygon();
		}
	}

	if( Exporter->GetExportOptions()->VertexColor )
	{
		// Create and fill in the vertex color data source.
		FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create( Mesh, "" );
		VertexColor->SetMappingMode( FbxLayerElement::eByControlPoint );
		VertexColor->SetReferenceMode( FbxLayerElement::eDirect );
		FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
		LayerZero->SetVertexColors( VertexColor );

		for( int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex )
		{
			FLinearColor VertColor = Vertices[ VertIndex ].Color.ReinterpretAsLinear();
			VertexColorArray.Add( FbxColor( VertColor.R, VertColor.G, VertColor.B, VertColor.A ) );
		}
	}


	if( Exporter->GetExportOptions()->bExportMorphTargets && GetSkeleton(SkelMesh) ) //The skeleton can be null if this is a destructible mesh.
	{
		#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <=2 )
		const FSmartNameMapping* SmartNameMapping = GetSkeleton( SkelMesh )->GetSmartNameContainer( USkeleton::AnimCurveMappingName );
		#endif
		TMap<FName, FbxAnimCurve*> BlendShapeCurvesMap;

		if( GetMorphTargets( SkelMesh ).Num() )
		{
			// The original BlendShape Name was not saved during import, so we need to come up with a new one.
			const FString BlendShapeName( SkelMesh->GetName() + TEXT( "_blendShapes" ) );
			FbxBlendShape* BlendShape = FbxBlendShape::Create( Mesh, TCHAR_TO_UTF8( *BlendShapeName ) );
			bool bHasBadMorphTarget = false;

			for( UMorphTarget* MorphTarget : GetMorphTargets(SkelMesh) )
			{
				int32 DeformerIndex = Mesh->AddDeformer( BlendShape );
				FbxBlendShapeChannel* BlendShapeChannel = FbxBlendShapeChannel::Create( BlendShape, TCHAR_TO_UTF8( *MorphTarget->GetName() ) );

				if( BlendShape->AddBlendShapeChannel( BlendShapeChannel ) )
				{
					FbxShape* Shape = FbxShape::Create( BlendShapeChannel, TCHAR_TO_UTF8( *MorphTarget->GetName() ) );
					Shape->InitControlPoints( VertexCount );
					FbxVector4* ShapeControlPoints = Shape->GetControlPoints();

					// Replicate the base mesh in the shape control points to set up the data.
					for( int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex )
					{
						FVector Position = (FVector)Vertices[ VertIndex ].Position;

						//Position = Rotator.RotateVector( Position );

						ShapeControlPoints[ VertIndex ] = Converter.ConvertToFbxPos( Position );
					}

					int32 NumberOfDeltas = 0;
					const FMorphTargetDelta* MorphTargetDeltas = MorphTarget->GetMorphTargetDelta( LODIndex, NumberOfDeltas );
					for( int32 MorphTargetDeltaIndex = 0; MorphTargetDeltaIndex < NumberOfDeltas; ++MorphTargetDeltaIndex )
					{
						// Apply the morph target deltas to the control points.
						const FMorphTargetDelta& CurrentDelta = MorphTargetDeltas[ MorphTargetDeltaIndex ];
						uint32 RemappedSourceIndex = CurrentDelta.SourceIdx;

						if( VertexIndexOffsetPairArray.Num() > 1 )
						{
							//If the skeletal mesh contains clothing we need to remap the morph target index too.
							int32 UpperBoundIndex = Algo::UpperBoundBy( VertexIndexOffsetPairArray, RemappedSourceIndex,
																		[]( const auto& CurrentPair ) { return CurrentPair.Key; } ); //Value functor
							RemappedSourceIndex -= VertexIndexOffsetPairArray[ UpperBoundIndex - 1 ].Value;
						}

						if( RemappedSourceIndex < static_cast<uint32>( VertexCount ) )
						{
							ShapeControlPoints[ RemappedSourceIndex ] = Converter.ConvertToFbxPos( (FVector)Vertices[ RemappedSourceIndex ].Position + (FVector)CurrentDelta.PositionDelta );
						}
						else
						{
							bHasBadMorphTarget = true;
						}
					}

					BlendShapeChannel->AddTargetShape( Shape );
					FName MorphTargetName = MorphTarget->GetFName();
					#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <=2 )
					if( AnimSeq && SmartNameMapping && SmartNameMapping->GetCurveMetaData( MorphTargetName ) && SmartNameMapping->GetCurveMetaData( MorphTargetName )->Type.bMorphtarget )
					{
						FbxAnimCurve* AnimCurve = Mesh->GetShapeChannel( DeformerIndex, BlendShape->GetBlendShapeChannelCount() - 1, AnimLayer, true );
						BlendShapeCurvesMap.Add( MorphTargetName, AnimCurve );
					}
					#endif
				}
			}

			if( bHasBadMorphTarget )
			{
				UE_LOG( LogTemp, Warning, TEXT( "Encountered corrupted morphtarget(s) during export of SkeletalMesh %s, bad vertices were ignored." ), *SkelMesh->GetName() );
			}
		}

		if( AnimSeq && BlendShapeCurvesMap.Num() > 0 )
		{
			#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <=1 )
			ExportCustomAnimCurvesToFbx( BlendShapeCurvesMap, AnimSeq,
										 0.f,		// AnimStartOffset
										 0.f,		// AnimEndOffset
										 1.f,		// AnimPlayRate
										 0.f,		// StartTime
										 100.f,		// ValueScale, for some reason we need to scale BlendShape curves by a factor of 100.
										 FBXScene );
			#endif
		}
	}

	FbxNode* MeshNode = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( MeshName ) );
	MeshNode->SetNodeAttribute( Mesh );



	// Add the materials for the mesh
	const TArray<FSkeletalMaterial>& SkelMeshMaterials = GetMaterials(SkelMesh);
	int32 MaterialCount = SkelMeshMaterials.Num();

	for( int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex )
	{
		UMaterialInterface* MatInterface = nullptr;

		if( OverrideMaterials && OverrideMaterials->IsValidIndex( MaterialIndex ) )
		{
			MatInterface = ( *OverrideMaterials )[ MaterialIndex ];
		}
		else
		{
			MatInterface = GetMaterials( SkelMesh )[ MaterialIndex ].MaterialInterface;
		}

		FbxSurfaceMaterial* FbxMaterial = NULL;
		if( LODIndex == 0 )
		{
			if( MatInterface && !FbxMaterials.Find( MatInterface ) )
			{
				FbxMaterial = ExportMaterial( MatInterface, FBXScene );
			}
		}
		else if( MatInterface )
		{
			if( FbxSurfaceMaterial** FbxMaterialPtr = FbxMaterials.Find( MatInterface ) )
			{
				FbxMaterial = *FbxMaterialPtr;
			}
		}

		if( !FbxMaterial )
		{
			// Note: The vertex data relies on there being a set number of Materials.  
			// If you try to add the same material again it will not be added, so create a 
			// default material with a unique name to ensure the proper number of materials

			TCHAR NewMaterialName[ MAX_SPRINTF ] = TEXT( "" );
			FCString::Sprintf( NewMaterialName, TEXT( "Fbx Default Material %i" ), MaterialIndex );

			FbxMaterial = FbxSurfaceLambert::Create( FBXScene, TCHAR_TO_UTF8( NewMaterialName ) );
			( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( FbxDouble3( 0.72, 0.72, 0.72 ) );
		}

		MeshNode->AddMaterial( FbxMaterial );
	}

	int32 SavedMaterialCount = MeshNode->GetMaterialCount();
	check( SavedMaterialCount == MaterialCount );

	return MeshNode;
}
FbxNode* ExportAnimSequence( FFbxExporter* Exporter, const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, bool bExportSkelMesh, FString MeshName, FbxNode* ActorRootNode, const TArray<UMaterialInterface*>* OverrideMaterials /*= nullptr*/,
							 FbxScene* FBXScene )
{
	if( FBXScene == NULL || AnimSeq == NULL || SkelMesh == NULL )
	{
		return NULL;
	}


	FbxNode* RootNode = ( ActorRootNode ) ? ActorRootNode : FBXScene->GetRootNode();

	//Create a temporary node attach to the scene root.
	//This will allow us to do the binding without the scene transform (non uniform scale is not supported when binding the skeleton)
	//We then detach from the temp node and attach to the parent and remove the temp node
	FString FbxNodeName = FGuid::NewGuid().ToString( EGuidFormats::Digits );
	FbxNode* TmpNodeNoTransform = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( *FbxNodeName ) );
	FBXScene->GetRootNode()->AddChild( TmpNodeNoTransform );


	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton( SkelMesh, BoneNodes, FBXScene );
	TmpNodeNoTransform->AddChild( SkeletonRootNode );


	// Export the anim sequence
	{
		#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <=1 )
			ExportAnimSequenceToFbx( Exporter, AnimSeq,
									 SkelMesh,
									 BoneNodes,
									 AnimLayer,
									 0.f,		// AnimStartOffset
									 0.f,		// AnimEndOffset
									 1.f,		// AnimPlayRate
									 0.f, 		// StartTime
									 FBXScene );
		#else
			ExportAnimSequenceToFbx( Exporter, AnimSeq,
									 SkelMesh,
									 BoneNodes,
									 AnimLayer,
									 // Start frame to export
									 FFrameTime( 0 ),
									 // Final frame to export
									 FFrameTime( AnimSeq->GetDataModel()->GetNumberOfFrames() ),
									 1.f,		// AnimPlayRate
									 0.f, 		// StartTime
									 FBXScene );
		#endif

		CorrectAnimTrackInterpolation( BoneNodes, AnimLayer );
	}


	// Optionally export the mesh
	if( bExportSkelMesh )
	{
		FString MeshNodeName;

		if( MeshName.Len() > 0 )
		{
			MeshNodeName = MeshName;
		}
		else
		{
			SkelMesh->GetName( MeshNodeName );
		}

		FbxNode* MeshRootNode = nullptr;
		if( Exporter->GetExportOptions()->LevelOfDetail && SkelMesh->GetLODNum() > 1 )
		{
			FString LodGroup_MeshName = MeshNodeName + TEXT( "_LodGroup" );
			MeshRootNode = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( *LodGroup_MeshName ) );
			TmpNodeNoTransform->AddChild( MeshRootNode );
			LodGroup_MeshName = MeshNodeName + TEXT( "_LodGroupAttribute" );
			FbxLODGroup* FbxLodGroupAttribute = FbxLODGroup::Create( FBXScene, TCHAR_TO_UTF8( *LodGroup_MeshName ) );
			MeshRootNode->AddNodeAttribute( FbxLodGroupAttribute );

			FbxLodGroupAttribute->ThresholdsUsedAsPercentage = true;
			//Export an Fbx Mesh Node for every LOD and child them to the fbx node (LOD Group)
			for( int CurrentLodIndex = 0; CurrentLodIndex < SkelMesh->GetLODNum(); ++CurrentLodIndex )
			{
				FString FbxLODNodeName = MeshNodeName + TEXT( "_LOD" ) + FString::FromInt( CurrentLodIndex );
				if( CurrentLodIndex + 1 < SkelMesh->GetLODNum() )
				{
					//Convert the screen size to a threshold, it is just to be sure that we set some threshold, there is no way to convert this precisely
					double LodScreenSize = (double)( 10.0f / SkelMesh->GetLODInfo( CurrentLodIndex )->ScreenSize.Default );
					FbxLodGroupAttribute->AddThreshold( LodScreenSize );
				}
				FbxNode* FbxActorLOD = CreateMesh( Exporter, SkelMesh, *FbxLODNodeName, CurrentLodIndex, AnimSeq, OverrideMaterials, FBXScene );
				if( FbxActorLOD )
				{
					MeshRootNode->AddChild( FbxActorLOD );
					if( SkeletonRootNode )
					{
						// Bind the mesh to the skeleton
						BindMeshToSkeleton( SkelMesh, FbxActorLOD, BoneNodes, CurrentLodIndex, FBXScene );
						// Add the bind pose
						CreateBindPose( FbxActorLOD, FBXScene );
					}
				}
			}
		}
		else
		{
			const int32 LodIndex = 0;
			MeshRootNode = CreateMesh( Exporter, SkelMesh, *MeshNodeName, LodIndex, AnimSeq, OverrideMaterials, FBXScene );
			if( MeshRootNode )
			{
				TmpNodeNoTransform->AddChild( MeshRootNode );
				if( SkeletonRootNode )
				{
					// Bind the mesh to the skeleton
					BindMeshToSkeleton( SkelMesh, MeshRootNode, BoneNodes, LodIndex, FBXScene );

					// Add the bind pose
					CreateBindPose( MeshRootNode, FBXScene );
				}
			}
		}

		if( MeshRootNode )
		{
			TmpNodeNoTransform->RemoveChild( MeshRootNode );
			RootNode->AddChild( MeshRootNode );
		}
	}

	if( SkeletonRootNode )
	{
		TmpNodeNoTransform->RemoveChild( SkeletonRootNode );
		RootNode->AddChild( SkeletonRootNode );
	}

	FBXScene->GetRootNode()->RemoveChild( TmpNodeNoTransform );
	FBXScene->RemoveNode( TmpNodeNoTransform );

	return SkeletonRootNode;
}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	#define UMetaData FMetaData
#endif
void ExportObjectMetadata( const UObject* ObjectToExport, FbxNode* Node )
{
	if( ObjectToExport && Node )
	{
		// Retrieve the metadata map without creating it
		const TMap<FName, FString>* MetadataMap = UMetaData::GetMapForObject( ObjectToExport );
		if( MetadataMap )
		{
			static const FString MetadataPrefix( FBX_METADATA_PREFIX );
			for( const auto& MetadataIt : *MetadataMap )
			{
				// Export object metadata tags that are prefixed as FBX custom user-defined properties
				// Remove the prefix since it's for Unreal use only (and '.' is considered an invalid character for user property names in DCC like Maya)
				FString TagAsString = MetadataIt.Key.ToString();
				if( TagAsString.RemoveFromStart( MetadataPrefix ) )
				{
					// Remaining tag follows the format NodeName.PropertyName, so replace '.' with '_'
					TagAsString.ReplaceInline( TEXT( "." ), TEXT( "_" ) );

					if( MetadataIt.Value == TEXT( "true" ) || MetadataIt.Value == TEXT( "false" ) )
					{
						FbxProperty Property = FbxProperty::Create( Node, FbxBoolDT, TCHAR_TO_UTF8( *TagAsString ) );
						FbxBool ValueBool = MetadataIt.Value == TEXT( "true" ) ? true : false;

						Property.Set( ValueBool );
						Property.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					}
					else
					{
						FbxProperty Property = FbxProperty::Create( Node, FbxStringDT, TCHAR_TO_UTF8( *TagAsString ) );
						FbxString ValueString( TCHAR_TO_UTF8( *MetadataIt.Value ) );

						Property.Set( ValueString );
						Property.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					}
				}
			}
		}
	}
}
void ExportObjectMetadataToBones( const UObject* ObjectToExport, const TArray<FbxNode*>& Nodes )
{
	if( !ObjectToExport || Nodes.Num() == 0 )
	{
		return;
	}

	// Retrieve the metadata map without creating it
	const TMap<FName, FString>* MetadataMap = UMetaData::GetMapForObject( ObjectToExport );
	if( MetadataMap )
	{
		// Map the nodes to their names for fast access
		TMap<FString, FbxNode*> NameToNode;
		for( FbxNode* Node : Nodes )
		{
			NameToNode.Add( FString( Node->GetName() ), Node );
		}

		static const FString MetadataPrefix( FBX_METADATA_PREFIX );
		for( const auto& MetadataIt : *MetadataMap )
		{
			// Export object metadata tags that are prefixed as FBX custom user-defined properties
			// Remove the prefix since it's for Unreal use only (and '.' is considered an invalid character for user property names in DCC like Maya)
			FString TagAsString = MetadataIt.Key.ToString();
			if( TagAsString.RemoveFromStart( MetadataPrefix ) )
			{
				// Extract the node name from the metadata tag
				FString NodeName;
				int32 CharPos = INDEX_NONE;
				if( TagAsString.FindChar( TEXT( '.' ), CharPos ) )
				{
					NodeName = TagAsString.Left( CharPos );

					// The remaining part is the actual metadata tag
					TagAsString.RightChopInline( CharPos + 1, false ); // exclude the period
				}

				// Try to attach the metadata to its associated node by name
				FbxNode** Node = NameToNode.Find( NodeName );
				if( Node )
				{
					if( MetadataIt.Value == TEXT( "true" ) || MetadataIt.Value == TEXT( "false" ) )
					{
						FbxProperty Property = FbxProperty::Create( *Node, FbxBoolDT, TCHAR_TO_UTF8( *TagAsString ) );
						FbxBool ValueBool = MetadataIt.Value == TEXT( "true" ) ? true : false;

						Property.Set( ValueBool );
						Property.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					}
					else
					{
						FbxProperty Property = FbxProperty::Create( *Node, FbxStringDT, TCHAR_TO_UTF8( *TagAsString ) );
						FbxString ValueString( TCHAR_TO_UTF8( *MetadataIt.Value ) );

						Property.Set( ValueString );
						Property.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					}
				}
			}
		}
	}
}
void PrepareFBXExporter( FString CurrentFilename, UnFbx::FFbxExporter** Exporter, FbxScene** FBXScene );
void WriteToFile( FFbxExporter* UEExporter, FString Filename, FbxScene* FBXScene );

FbxNode* ExportSkeletalMeshToFbx( FFbxExporter* Exporter, const USkeletalMesh* SkeletalMesh, const UAnimSequence* AnimSeq, const TCHAR* MeshName, FbxNode* ActorRootNode, const TArray<UMaterialInterface*>* OverrideMaterials /*= nullptr*/,
								  FbxScene* FBXScene)
{
	if( AnimSeq )
	{
		//AddAnimationEntry( SkeletalMesh, AnimSeq, MeshName, OverrideMaterials );
	}
	//else
	{
		//Create a temporary node attach to the scene root.
		//This will allow us to do the binding without the scene transform (non uniform scale is not supported when binding the skeleton)
		//We then detach from the temp node and attach to the parent and remove the temp node
		FString FbxNodeName = FGuid::NewGuid().ToString( EGuidFormats::Digits );
		FbxNode* TmpNodeNoTransform = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( *FbxNodeName ) );
		FBXScene->GetRootNode()->AddChild( TmpNodeNoTransform );

		TArray<FbxNode*> BoneNodes;

		// Add the skeleton to the scene
		FbxNode* SkeletonRootNode = CreateSkeleton( SkeletalMesh, BoneNodes, FBXScene );
		if( SkeletonRootNode )
		{
			TmpNodeNoTransform->AddChild( SkeletonRootNode );
		}

		FbxNode* MeshRootNode = nullptr;
		UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
		if( UnitySettings && UnitySettings->ExportLODs && SkeletalMesh->GetLODNum() > 1 )
		{
			FString LodGroup_MeshName = FString( MeshName ) + TEXT( "_LodGroup" );
			MeshRootNode = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( *LodGroup_MeshName ) );
			TmpNodeNoTransform->AddChild( MeshRootNode );
			LodGroup_MeshName = FString( MeshName ) + TEXT( "_LodGroupAttribute" );
			FbxLODGroup* FbxLodGroupAttribute = FbxLODGroup::Create( FBXScene, TCHAR_TO_UTF8( *LodGroup_MeshName ) );
			MeshRootNode->AddNodeAttribute( FbxLodGroupAttribute );

			FbxLodGroupAttribute->ThresholdsUsedAsPercentage = true;
			//Export an Fbx Mesh Node for every LOD and child them to the fbx node (LOD Group)
			for( int CurrentLodIndex = 0; CurrentLodIndex < SkeletalMesh->GetLODNum(); ++CurrentLodIndex )
			{
				FString FbxLODNodeName = FString( MeshName ) + TEXT( "_LOD" ) + FString::FromInt( CurrentLodIndex );
				if( CurrentLodIndex + 1 < SkeletalMesh->GetLODNum() )
				{
					//Convert the screen size to a threshold, it is just to be sure that we set some threshold, there is no way to convert this precisely
					double LodScreenSize = (double)( 10.0f / SkeletalMesh->GetLODInfo( CurrentLodIndex )->ScreenSize.Default );
					FbxLodGroupAttribute->AddThreshold( LodScreenSize );
				}
				const UAnimSequence* NullAnimSeq = nullptr;
				FbxNode* FbxActorLOD = CreateMesh( Exporter, SkeletalMesh, *FbxLODNodeName, CurrentLodIndex, NullAnimSeq, OverrideMaterials, FBXScene );
				if( FbxActorLOD )
				{
					MeshRootNode->AddChild( FbxActorLOD );
					if( SkeletonRootNode )
					{
						// Bind the mesh to the skeleton
						BindMeshToSkeleton( SkeletalMesh, FbxActorLOD, BoneNodes, CurrentLodIndex, FBXScene );
						// Add the bind pose
						CreateBindPose( FbxActorLOD, FBXScene );
					}
				}
			}
		}
		else
		{
			const int32 LODIndex = 0;
			const UAnimSequence* NullAnimSeq = nullptr;
			FString FbxMeshName = FString( MeshName );// +TEXT( "_LOD0" );
			MeshRootNode = CreateMesh( Exporter, SkeletalMesh, *FbxMeshName, LODIndex, NullAnimSeq, OverrideMaterials, FBXScene );
			if( MeshRootNode )
			{
				TmpNodeNoTransform->AddChild( MeshRootNode );
				if( SkeletonRootNode )
				{
					// Bind the mesh to the skeleton
					BindMeshToSkeleton( SkeletalMesh, MeshRootNode, BoneNodes, 0, FBXScene );

					// Add the bind pose
					CreateBindPose( MeshRootNode, FBXScene );
				}
			}
		}

		if( SkeletonRootNode )
		{
			TmpNodeNoTransform->RemoveChild( SkeletonRootNode );
			ActorRootNode->AddChild( SkeletonRootNode );
		}

		ExportObjectMetadataToBones( GetSkeleton(SkeletalMesh), BoneNodes );

		if( MeshRootNode )
		{
			TmpNodeNoTransform->RemoveChild( MeshRootNode );
			ActorRootNode->AddChild( MeshRootNode );
			ExportObjectMetadata( SkeletalMesh, MeshRootNode );
		}

		FBXScene->GetRootNode()->RemoveChild( TmpNodeNoTransform );
		FBXScene->RemoveNode( TmpNodeNoTransform );
		return SkeletonRootNode;
	}

	return NULL;
}


void ExportSkeletalMeshComponent( FFbxExporter* Exporter, USkeletalMeshComponent* SkelMeshComp, const TCHAR* MeshName, FbxNode* ActorRootNode, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq, FbxScene* FBXScene )
{
	if( SkelMeshComp && GetSkeletalMesh( SkelMeshComp ) )
	{
		UAnimSequence* AnimSeq = ( bSaveAnimSeq && SkelMeshComp->GetAnimationMode() == EAnimationMode::AnimationSingleNode ) ? GetAnimationSequence( SkelMeshComp ) : NULL;
		#if ENGINE_MAJOR_VERSION == 4
			FbxNode* SkeletonRootNode = ExportSkeletalMeshToFbx( Exporter, SkelMeshComp->SkeletalMesh, AnimSeq, MeshName, ActorRootNode, &SkelMeshComp->OverrideMaterials, FBXScene );
		#else
			TArray<UMaterialInterface*> Materials = GetArrayOfMaterials( SkelMeshComp->OverrideMaterials );
			FbxNode* SkeletonRootNode = ExportSkeletalMeshToFbx( Exporter, GetSkeletalMesh( SkelMeshComp ), AnimSeq, MeshName, ActorRootNode, &Materials, FBXScene );
		#endif

		if( SkeletonRootNode )
		{
			FbxSkeletonRoots.Add( SkelMeshComp, SkeletonRootNode );
			NodeNameAdapter.AddFbxNode( SkelMeshComp, SkeletonRootNode );
		}
	}
}
FbxNode* ExportActor( FFbxExporter* Exporter, AActor* Actor, bool bExportComponents, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq /*= true*/, FbxScene* FBXScene )
{
	// Verify that this actor isn't already exported, create a structure for it
	// and buffer it.
	FbxNode* ActorNode = FindActor( Actor, &NodeNameAdapter );
	if( ActorNode == NULL )
	{
		FString FbxNodeName = NodeNameAdapter.GetActorNodeName( Actor );
		FbxNodeName = GetFbxObjectName( FbxNodeName, NodeNameAdapter );
		FbxNodeName = FixUnicodeCharacters( FbxNodeName );
		ActorNode = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( *FbxNodeName ) );

		AActor* ParentActor = Actor->GetAttachParentActor();
		// this doesn't work with skeletalmeshcomponent
		FbxNode* ParentNode = FindActor( ParentActor, &NodeNameAdapter );
		FVector ActorLocation, ActorRotation, ActorScale;

		TArray<AActor*> AttachedActors;
		Actor->GetAttachedActors( AttachedActors );
		const bool bHasAttachedActors = AttachedActors.Num() != 0;

		// For cameras and lights: always add a rotation to get the correct coordinate system.
		FTransform RotationDirectionConvert = FTransform::Identity;
		const bool bIsCameraActor = Actor->IsA( ACameraActor::StaticClass() );
		const bool bIsLightActor = Actor->IsA( ALight::StaticClass() );
		if( bIsCameraActor || bIsLightActor )
		{
			if( bIsCameraActor )
			{
				FRotator Rotator = FFbxDataConverter::GetCameraRotation().GetInverse();
				RotationDirectionConvert = FTransform( Rotator );
			}
			else if( bIsLightActor )
			{
				FRotator Rotator = FFbxDataConverter::GetLightRotation().GetInverse();
				RotationDirectionConvert = FTransform( Rotator );
			}
		}

		//If the parent is the root or is not export use the root node as the parent
		if( bKeepHierarchy && ParentNode )
		{
			// Set the default position of the actor on the transforms
			// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
			const FTransform RelativeTransform = RotationDirectionConvert * Actor->GetTransform().GetRelativeTransform( ParentActor->GetTransform() );
			ActorLocation = RelativeTransform.GetTranslation();
			ActorRotation = RelativeTransform.GetRotation().Euler();
			ActorScale = RelativeTransform.GetScale3D();
		}
		else
		{
			ParentNode = FBXScene->GetRootNode();
			// Set the default position of the actor on the transforms
			// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
			if( ParentActor != NULL )
			{
				//In case the parent was not export, get the absolute transform
				const FTransform AbsoluteTransform = RotationDirectionConvert * Actor->GetTransform();
				ActorLocation = AbsoluteTransform.GetTranslation();
				ActorRotation = AbsoluteTransform.GetRotation().Euler();
				ActorScale = AbsoluteTransform.GetScale3D();
			}
			else
			{
				const FTransform ConvertedTransform = RotationDirectionConvert * Actor->GetTransform();
				ActorLocation = ConvertedTransform.GetTranslation();
				ActorRotation = ConvertedTransform.GetRotation().Euler();
				ActorScale = ConvertedTransform.GetScale3D();
			}
		}

		ParentNode->AddChild( ActorNode );
		FbxActors.Add( Actor, ActorNode );
		NodeNameAdapter.AddFbxNode( Actor, ActorNode );

		// Set the default position of the actor on the transforms
		// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		bool SetIdentity = true;
		if( SetIdentity )
		{
			ActorNode->LclRotation.Set( Converter.ConvertToFbxRot( FVector( 90, 0, 0 )));//used to work
			//ActorNode->LclRotation.Set( Converter.ConvertToFbxRot( FVector( 90, 180, 0 ) ) );//rotate for similar preview angle
		}
		else
		{
			ActorNode->LclTranslation.Set( Converter.ConvertToFbxPos( ActorLocation ) );
			ActorNode->LclRotation.Set( Converter.ConvertToFbxRot( ActorRotation ) );
			ActorNode->LclScaling.Set( Converter.ConvertToFbxScale( ActorScale ) );
		}

		if( bExportComponents )
		{
			TInlineComponentArray<USceneComponent*> ComponentsToExport;
			for( UActorComponent* ActorComp : Actor->GetComponents() )
			{
				USceneComponent* Component = Cast<USceneComponent>( ActorComp );

				if( Component == nullptr || Component->bHiddenInGame )
				{
					//Skip hidden component like camera mesh or other editor helper
					continue;
				}

				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( Component );
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>( Component );
				UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>( Component );

				if( StaticMeshComp && StaticMeshComp->GetStaticMesh() )
				{
					ComponentsToExport.Add( StaticMeshComp );
				}
				else if( SkelMeshComp && GetSkeletalMesh( SkelMeshComp ) )
				{
					ComponentsToExport.Add( SkelMeshComp );
				}
				else if( Component->IsA( UCameraComponent::StaticClass() ) )
				{
					ComponentsToExport.Add( Component );
				}
				else if( Component->IsA( ULightComponent::StaticClass() ) )
				{
					ComponentsToExport.Add( Component );
				}
				else if( ChildActorComp && ChildActorComp->GetChildActor() )
				{
					ComponentsToExport.Add( ChildActorComp );
				}
			}

			for( int32 CompIndex = 0; CompIndex < ComponentsToExport.Num(); ++CompIndex )
			{
				USceneComponent* Component = ComponentsToExport[ CompIndex ];

				RotationDirectionConvert = FTransform::Identity;
				// For cameras and lights: always add a rotation to get the correct coordinate system
				// Unless we are parented to an Actor of the same type, since the rotation direction was already added
				if( Component->IsA( UCameraComponent::StaticClass() ) || Component->IsA( ULightComponent::StaticClass() ) )
				{
					if( !bIsCameraActor && Component->IsA( UCameraComponent::StaticClass() ) )
					{
						FRotator Rotator = FFbxDataConverter::GetCameraRotation().GetInverse();
						RotationDirectionConvert = FTransform( Rotator );
					}
					else if( !bIsLightActor && Component->IsA( ULightComponent::StaticClass() ) )
					{
						FRotator Rotator = FFbxDataConverter::GetLightRotation().GetInverse();
						RotationDirectionConvert = FTransform( Rotator );
					}
				}

				FbxNode* ExportNode = ActorNode;
				if( ComponentsToExport.Num() > 1 )
				{
					// This actor has multiple components
					// create a child node under the actor for each component
					FbxNode* CompNode = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( *Component->GetName() ) );

					if( Component != Actor->GetRootComponent() )
					{
						// Transform is relative to the root component
						const FTransform RelativeTransform = RotationDirectionConvert * Component->GetComponentToWorld().GetRelativeTransform( Actor->GetTransform() );
						CompNode->LclTranslation.Set( Converter.ConvertToFbxPos( RelativeTransform.GetTranslation() ) );
						CompNode->LclRotation.Set( Converter.ConvertToFbxRot( RelativeTransform.GetRotation().Euler() ) );
						CompNode->LclScaling.Set( Converter.ConvertToFbxScale( RelativeTransform.GetScale3D() ) );
					}

					ExportNode = CompNode;
					ActorNode->AddChild( CompNode );
				}
				// If this actor has attached actors, don't allow its non root component components to contribute to the transform
				else if( Component != Actor->GetRootComponent() && !bHasAttachedActors )
				{
					// Merge the component relative transform in the ActorNode transform since this is the only component to export and its not the root
					const FTransform RelativeTransform = RotationDirectionConvert * Component->GetComponentToWorld().GetRelativeTransform( Actor->GetTransform() );

					FTransform ActorTransform( FRotator::MakeFromEuler( ActorRotation ).Quaternion(), ActorLocation, ActorScale );
					FTransform TotalTransform = RelativeTransform * ActorTransform;

					ActorNode->LclTranslation.Set( Converter.ConvertToFbxPos( TotalTransform.GetLocation() ) );
					ActorNode->LclRotation.Set( Converter.ConvertToFbxRot( TotalTransform.GetRotation().Euler() ) );
					ActorNode->LclScaling.Set( Converter.ConvertToFbxScale( TotalTransform.GetScale3D() ) );
				}

				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( Component );
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>( Component );
				UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>( Component );

				if( StaticMeshComp && StaticMeshComp->GetStaticMesh() )
				{
					if( USplineMeshComponent* SplineMeshComp = Cast<USplineMeshComponent>( StaticMeshComp ) )
					{
						//ExportSplineMeshToFbx( SplineMeshComp, *SplineMeshComp->GetName(), ExportNode );
					}
					else if( UInstancedStaticMeshComponent* InstancedMeshComp = Cast<UInstancedStaticMeshComponent>( StaticMeshComp ) )
					{
						//ExportInstancedMeshToFbx( InstancedMeshComp, *InstancedMeshComp->GetName(), ExportNode );
					}
					else
					{
						const int32 LODIndex = ( StaticMeshComp->ForcedLodModel > 0 ? StaticMeshComp->ForcedLodModel - 1 : /* auto-select*/ 0 );
						const int32 LightmapUVChannel = -1;
						const TArray<FStaticMaterial>* MaterialOrderOverride = nullptr;
						const TArray<UMaterialInterface*> Materials = StaticMeshComp->GetMaterials();
						const FColorVertexBuffer* ColorBuffer = nullptr;

						//Original name is a blueprint name
						//TODO : temporary fix, LODs are not exported through this !
						FString FbxLODNodeName = StaticMeshComp->GetStaticMesh()->GetName();
						FbxLODNodeName += TEXT( "_LOD0" );
						ExportNode->SetName( TCHAR_TO_UTF8( *FbxLODNodeName ) );
						ExportStaticMeshToFbx( Exporter, StaticMeshComp->GetStaticMesh(), LODIndex, *StaticMeshComp->GetName(), ExportNode, LightmapUVChannel, ColorBuffer, MaterialOrderOverride, &Materials, FBXScene );
					}
				}
				else if( SkelMeshComp && GetSkeletalMesh( SkelMeshComp ) )
				{
					ExportSkeletalMeshComponent( Exporter, SkelMeshComp, *GetSkeletalMesh(SkelMeshComp)->GetName(), ExportNode, NodeNameAdapter, bSaveAnimSeq, FBXScene );
				}
				// If this actor has attached actors, don't allow a camera component to determine the node attributes because that would alter the transform
				else if( Component->IsA( UCameraComponent::StaticClass() ) && !bHasAttachedActors )
				{
					//FbxCamera* Camera = FbxCamera::Create( Scene, TCHAR_TO_UTF8( *Component->GetName() ) );
					//FillFbxCameraAttribute( ActorNode, Camera, Cast<UCameraComponent>( Component ) );
					//ExportNode->SetNodeAttribute( Camera );
				}
				else if( Component->IsA( ULightComponent::StaticClass() ) && !bHasAttachedActors )
				{
					//FbxLight* Light = FbxLight::Create( Scene, TCHAR_TO_UTF8( *Component->GetName() ) );
					//FillFbxLightAttribute( Light, ActorNode, Cast<ULightComponent>( Component ) );
					//ExportNode->SetNodeAttribute( Light );
				}
				else if( ChildActorComp && ChildActorComp->GetChildActor() )
				{
					FbxNode* ChildActorNode = ExportActor( Exporter, ChildActorComp->GetChildActor(), true, NodeNameAdapter, bSaveAnimSeq, FBXScene );
					FbxActors.Add( ChildActorComp->GetChildActor(), ChildActorNode );
					NodeNameAdapter.AddFbxNode( ChildActorComp->GetChildActor(), ChildActorNode );
				}
			}
		}

	}

	return ActorNode;
}
//Figure out if component or base mesh has unique materials since we need the unique materials variant because unity merges meshes
TArray<UMaterialInterface*> GetMaterialsToBeUsedInFBX( UStaticMeshComponent* StaticMeshComp )
{
	TArray<UMaterialInterface*> Materials = StaticMeshComp->GetMaterials();
	TArray<FStaticMaterial> StaticMaterials = GetStaticMaterials( StaticMeshComp->GetStaticMesh() );
	bool ComponentHasDuplicates = false;
	bool BaseMeshHasDuplicates = false;
	for( int i = 0; i < Materials.Num() - 1; i++ )
	{
		for( int u = i + 1; u < Materials.Num(); u++ )
		{
			if( Materials[ i ] == Materials[ u ] )
				ComponentHasDuplicates = true;
		}
	}
	for( int i = 0; i < StaticMaterials.Num() - 1; i++ )
	{
		for( int u = i + 1; u < StaticMaterials.Num(); u++ )
		{
			if( StaticMaterials[ i ].MaterialInterface == StaticMaterials[ u ].MaterialInterface )
				BaseMeshHasDuplicates = true;
		}
	}
	
	TArray<UMaterialInterface*> OutMaterials;
	int NumMaterials = StaticMeshComp->GetNumMaterials();
	
	for( int i = 0; i < NumMaterials; i++ )
	{
		UMaterialInterface* Mat = nullptr;
		if( StaticMeshComp )
		{
			if( !ComponentHasDuplicates )
				Mat = StaticMeshComp->GetMaterial( i );
			else
				Mat = StaticMeshComp->GetStaticMesh()->GetMaterial( i );
		}
		OutMaterials.Add( Mat );
	}

	return OutMaterials;
}
void ExportStaticMesh( FFbxExporter* Exporter, AActor* Actor, UStaticMeshComponent* StaticMeshComponent, INodeNameAdapter& NodeNameAdapter, FbxScene* FBXScene )
{
	//if( Exporter->Scene == NULL || Actor == NULL || StaticMeshComponent == NULL )
	//{
	//	return;
	//}

	// Retrieve the static mesh rendering information at the correct LOD level.
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if( StaticMesh == NULL || !StaticMesh->HasValidRenderData() )
	{
		return;
	}
	FString FbxNodeName = NodeNameAdapter.GetActorNodeName( Actor );
	FString FbxMeshName = StaticMesh->GetName();// .Replace( TEXT( "-" ), TEXT( "_" ) );
	FbxMeshName = FixUnicodeCharacters( FbxMeshName );
	FColorVertexBuffer* ColorBuffer = NULL;

	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings && UnitySettings->ExportLODs && StaticMesh->GetNumLODs() > 1 )
	{
		//Create a fbx LOD Group node
		FbxNode* FbxActor = ExportActor( Exporter, Actor, false, NodeNameAdapter, true, FBXScene );
		FString FbxLODGroupName = NodeNameAdapter.GetActorNodeName( Actor );
		FbxLODGroupName += TEXT( "_LodGroup" );
		FbxLODGroupName = GetFbxObjectName( FbxLODGroupName, NodeNameAdapter );
		FbxLODGroup* FbxLodGroupAttribute = FbxLODGroup::Create( FBXScene, TCHAR_TO_UTF8( *FbxLODGroupName ) );
		FbxActor->AddNodeAttribute( FbxLodGroupAttribute );
		FbxLodGroupAttribute->ThresholdsUsedAsPercentage = true;
		//Export an Fbx Mesh Node for every LOD and child them to the fbx node (LOD Group)
		for( int CurrentLodIndex = 0; CurrentLodIndex < StaticMesh->GetNumLODs(); ++CurrentLodIndex )
		{
			if( CurrentLodIndex < StaticMeshComponent->LODData.Num() )
			{
				ColorBuffer = StaticMeshComponent->LODData[ CurrentLodIndex ].OverrideVertexColors;
			}
			else
			{
				ColorBuffer = nullptr;
			}
			FString FbxLODNodeName = StaticMesh->GetName();// NodeNameAdapter.GetActorNodeName( Actor );
			FbxLODNodeName += TEXT( "_LOD" ) + FString::FromInt( CurrentLodIndex );
			FbxLODNodeName = GetFbxObjectName( FbxLODNodeName, NodeNameAdapter );
			FbxNode* FbxActorLOD = FbxNode::Create( FBXScene, TCHAR_TO_UTF8( *FbxLODNodeName ) );
			FbxActor->AddChild( FbxActorLOD );
			if( CurrentLodIndex + 1 < StaticMesh->GetNumLODs() )
			{
				//Convert the screen size to a threshold, it is just to be sure that we set some threshold, there is no way to convert this precisely
				double LodScreenSize = (double)( 10.0f / GetRenderData( StaticMesh )->ScreenSize[ CurrentLodIndex ].Default );
				FbxLodGroupAttribute->AddThreshold( LodScreenSize );
			}

			const int32 LightmapUVChannel = -1;
			const TArray<FStaticMaterial>* MaterialOrderOverride = nullptr;
			const TArray<UMaterialInterface*> Materials = GetMaterialsToBeUsedInFBX( StaticMeshComponent );			
			ExportStaticMeshToFbx( Exporter, StaticMesh, CurrentLodIndex, *FbxMeshName, FbxActorLOD, LightmapUVChannel, ColorBuffer, MaterialOrderOverride,  &Materials, FBXScene );
		}
	}
	else
	{
		const int32 LODIndex = ( StaticMeshComponent->ForcedLodModel > 0 ? StaticMeshComponent->ForcedLodModel - 1 : /* auto-select*/ 0 );
		if( LODIndex != INDEX_NONE && LODIndex < StaticMeshComponent->LODData.Num() )
		{
			ColorBuffer = StaticMeshComponent->LODData[ LODIndex ].OverrideVertexColors;
		}
		FbxNode* FbxActor = ExportActor( Exporter, Actor, false, NodeNameAdapter, true, FBXScene );

		FString FbxLODNodeName = StaticMesh->GetName();
		FbxLODNodeName = FixUnicodeCharacters( FbxLODNodeName );
		FbxActor->SetName( TCHAR_TO_UTF8( *FbxLODNodeName ) );

		const int32 LightmapUVChannel = -1;
		const TArray<FStaticMaterial>* MaterialOrderOverride = nullptr;
		const TArray<UMaterialInterface*> Materials = GetMaterialsToBeUsedInFBX( StaticMeshComponent );
		ExportStaticMeshToFbx( Exporter, StaticMesh, LODIndex, *FbxMeshName, FbxActor, LightmapUVChannel, ColorBuffer, MaterialOrderOverride, &Materials, FBXScene );
	}
}
void ExportLevelMesh( FFbxExporter* Exporter, ULevel* InLevel, bool bExportLevelGeometry, TArray<AActor*>& ActorToExport, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq, FbxScene* FBXScene )
{
	if( InLevel == NULL )
	{
		return;
	}

	if( bExportLevelGeometry )
	{		
		// Exports the level's scene geometry
		// the vertex number of Model must be more than 2 (at least a triangle panel)
		//if( InLevel->Model != NULL && InLevel->Model->VertexBuffer.Vertices.Num() > 2 && InLevel->Model->MaterialIndexBuffers.Num() > 0 )
		//{
		//	// create a FbxNode
		//	FbxNode* Node = FbxNode::Create( TheScene, "LevelMesh" );
		//
		//	// set the shading mode to view texture
		//	Node->SetShadingMode( FbxNode::eTextureShading );
		//	Node->LclScaling.Set( FbxVector4( 1.0, 1.0, 1.0 ) );
		//
		//	TheScene->GetRootNode()->AddChild( Node );
		//
		//	// Export the mesh for the world
		//	//Exporter->ExportModel( InLevel->Model, Node, "Level Mesh" );
		//}
	}

	//Sort the hierarchy to make sure parent come first
	//Exporter->SortActorsHierarchy( ActorToExport );
	int32 ActorCount = ActorToExport.Num();
	for( int32 ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex )
	{
		AActor* Actor = ActorToExport[ ActorIndex ];
		//We export only valid actor
		if( Actor == nullptr )
			continue;

		bool bIsBlueprintClass = false;
		if( UClass* ActorClass = Actor->GetClass() )
		{
			// Check if we export the actor as a blueprint
			bIsBlueprintClass = UBlueprint::GetBlueprintFromClass( ActorClass ) != nullptr;
		}

		//Blueprint can be any type of actor so it must be done first
		if( bIsBlueprintClass )
		{
			// Export blueprint actors and all their components.
			//ExportActor( Exporter, Actor, true, NodeNameAdapter, bSaveAnimSeq );

			TArray<UActorComponent*> comps;
			Actor->GetComponents( comps );
			
			for( int i = 0; i < comps.Num(); i++ )
			{
				UActorComponent* AC = comps[ i ];
				UClass* Class = AC->GetClass();
				UStaticMeshComponent* StaticMeshComp = dynamic_cast<UStaticMeshComponent*>( AC );
				USkeletalMeshComponent* SkeletalMeshComp = dynamic_cast<USkeletalMeshComponent*>( AC );
				if( StaticMeshComp )
				{
					ExportStaticMesh( Exporter, Actor, StaticMeshComp, NodeNameAdapter, FBXScene );
				}
				if( SkeletalMeshComp )
				{
					ExportActor( Exporter, Actor, true, NodeNameAdapter, bSaveAnimSeq, FBXScene );
				}
			}			
		}
		//else if( Actor->IsA( ALight::StaticClass() ) )
		//{
		//	Exporter->ExportLight( (ALight*)Actor, NodeNameAdapter );
		//}
		else if( Actor->IsA( AStaticMeshActor::StaticClass() ) )
		{
			ExportStaticMesh( Exporter, Actor, CastChecked<AStaticMeshActor>( Actor )->GetStaticMeshComponent(), NodeNameAdapter, FBXScene );
		}
		//else if( Actor->IsA( ALandscapeProxy::StaticClass() ) )
		//{
		//	Exporter->ExportLandscape( CastChecked<ALandscapeProxy>( Actor ), false, NodeNameAdapter );
		//}
		//else if( Actor->IsA( ABrush::StaticClass() ) )
		//{
		//	// All brushes should be included within the world geometry exported above.
		//	Exporter->ExportBrush( (ABrush*)Actor, NULL, 0, NodeNameAdapter );
		//}
		//else if( Actor->IsA( AEmitter::StaticClass() ) )
		//{
		//	Exporter->ExportActor( Actor, false, NodeNameAdapter, bSaveAnimSeq ); // Just export the placement of the particle emitter.
		//}
		//else if( Actor->IsA( ACameraActor::StaticClass() ) )
		//{
		//	Exporter->ExportCamera( CastChecked<ACameraActor>( Actor ), false, NodeNameAdapter );
		//}
		else
		{
			// Export any other type of actors and all their components.
			ExportActor( Exporter, Actor, true, NodeNameAdapter, bSaveAnimSeq, FBXScene );
		}
	}
}
FbxScene* CreateDocument( FFbxExporter* Exporter )
{
	SdkManager = FbxManager::Create();

	// create an IOSettings object
	FbxIOSettings* ios = FbxIOSettings::Create( SdkManager, "IOSROOT");
	SdkManager->SetIOSettings( ios );

	DefaultCamera = NULL;

	FbxScene* RetScene = FbxScene::Create( SdkManager, "" );

	FString PluginName = ExporterInterface->GetPluginName();
	std::string PluginNameStr = ToANSIString( *PluginName );
	std::string PluginVersion = ToANSIString( *ExporterInterface->GetPluginVersion() );
	// create scene info
	FbxDocumentInfo* SceneInfo = FbxDocumentInfo::Create( SdkManager, "SceneInfo" );
	SceneInfo->mTitle = "UnrealToUnity FBX Exporter";
	SceneInfo->mSubject = "UnrealToUnity Export FBX meshes";
	SceneInfo->Original_ApplicationVendor.Set( "Ciprian Stanciu" );
	SceneInfo->Original_ApplicationName.Set( PluginNameStr.c_str() );
	SceneInfo->Original_ApplicationVersion.Set( PluginVersion.c_str() );
	SceneInfo->LastSaved_ApplicationVendor.Set( "Ciprian Stanciu" );
	SceneInfo->LastSaved_ApplicationName.Set( PluginNameStr.c_str() );
	SceneInfo->LastSaved_ApplicationVersion.Set( PluginVersion.c_str() );

	RetScene->SetSceneInfo( SceneInfo );

	//FbxScene->GetGlobalSettings().SetOriginalUpAxis(KFbxAxisSystem::Max);
	FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd;
	if( GetExportOptions( Exporter )->bForceFrontXAxis )
		FrontVector = FbxAxisSystem::eParityEven;

	const FbxAxisSystem UnrealZUp( FbxAxisSystem::eZAxis, FrontVector, FbxAxisSystem::eRightHanded );
	RetScene->GetGlobalSettings().SetAxisSystem( UnrealZUp );
	RetScene->GetGlobalSettings().SetOriginalUpAxis( UnrealZUp );
	// Maya use cm by default
	RetScene->GetGlobalSettings().SetSystemUnit( FbxSystemUnit::cm );
	//FbxScene->GetGlobalSettings().SetOriginalSystemUnit( KFbxSystemUnit::m );

	//bSceneGlobalTimeLineSet = false;
	RetScene->GetGlobalSettings().SetTimeMode( FbxTime::eDefaultMode );

	// setup anim stack
	AnimStack = FbxAnimStack::Create( RetScene, "Unreal Take" );
	//KFbxSet<KTime>(AnimStack->LocalStart, KTIME_ONE_SECOND);
	AnimStack->Description.Set( "Animation Take for Unreal." );

	// this take contains one base layer. In fact having at least one layer is mandatory.
	AnimLayer = FbxAnimLayer::Create( RetScene, "Base Layer" );
	AnimStack->AddMember( AnimLayer );

	return RetScene;
}
void CloseDocument(FbxScene* FBXScene)
{
	FbxActors.Reset();
	FbxSkeletonRoots.Reset();
	FbxMaterials.Reset();
	FbxMeshes.Reset();
	FbxCollisionMeshes.Reset();
	FbxNodeNameToIndexMap.Reset();

	if( FBXScene )
	{
		FBXScene->Destroy();
		FBXScene = NULL;
	}
	//bSceneGlobalTimeLineSet = false;
}
#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(SdkManager->GetIOSettings()))
#endif
void WriteToFile( FFbxExporter* UEExporter, FString Filename, FbxScene* FBXScene )
{
	int32 Major, Minor, Revision;
	bool Status = true;

	int32 FileFormat = -1;
	bool bEmbedMedia = false;
	bool bASCII = GetExportOptions( UEExporter )->bASCII;

	// Create an exporter.
	FbxExporter* Exporter = FbxExporter::Create( SdkManager, "" );

	// set file format
	// Write in fall back format if pEmbedMedia is true
	if( bASCII )
	{
		FileFormat = SdkManager->GetIOPluginRegistry()->FindWriterIDByDescription( "FBX ascii (*.fbx)" );
	}
	else
	{
		FileFormat = SdkManager->GetIOPluginRegistry()->GetNativeWriterFormat();
	}

	// Set the export states. By default, the export states are always set to 
	// true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below 
	// shows how to change these states.

	IOS_REF.SetBoolProp( EXP_FBX_MATERIAL, true );
	IOS_REF.SetBoolProp( EXP_FBX_TEXTURE, true );
	IOS_REF.SetBoolProp( EXP_FBX_EMBEDDED, bEmbedMedia );
	IOS_REF.SetBoolProp( EXP_FBX_SHAPE, true );
	IOS_REF.SetBoolProp( EXP_FBX_GOBO, true );
	IOS_REF.SetBoolProp( EXP_FBX_ANIMATION, true );
	IOS_REF.SetBoolProp( EXP_FBX_GLOBAL_SETTINGS, true );
	IOS_REF.SetBoolProp( EXP_ASCIIFBX, bASCII );

	//Get the compatibility from the editor settings
	const char* CompatibilitySetting = FBX_2013_00_COMPATIBLE;
	const EFbxExportCompatibility FbxExportCompatibility = GetExportOptions( UEExporter )->FbxExportCompatibility;
	switch( FbxExportCompatibility )
	{
		case EFbxExportCompatibility::FBX_2011:
			CompatibilitySetting = FBX_2011_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2012:
			CompatibilitySetting = FBX_2012_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2013:
			CompatibilitySetting = FBX_2013_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2014:
			CompatibilitySetting = FBX_2014_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2016:
			CompatibilitySetting = FBX_2016_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2018:
			CompatibilitySetting = FBX_2018_00_COMPATIBLE;
			break;
		#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26
		case EFbxExportCompatibility::FBX_2019:
			CompatibilitySetting = FBX_2019_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2020:
			CompatibilitySetting = FBX_2020_00_COMPATIBLE;
			break;
		#endif
		default:
			CompatibilitySetting = FBX_2013_00_COMPATIBLE;
			break;
	}

	// We export using FBX 2013 format because many users are still on that version and FBX 2014 files has compatibility issues with
	// normals when importing to an earlier version of the plugin
	if( !Exporter->SetFileExportVersion( CompatibilitySetting, FbxSceneRenamer::eNone ) )
	{
		UE_LOG( LogTemp, Warning, TEXT( "Call to KFbxExporter::SetFileExportVersion(FBX_2013_00_COMPATIBLE) to export 2013 fbx file format failed.\n" ) );
	}

	// Initialize the exporter by providing a filename.
	std::string ANSIStr = ToANSIString( *Filename );
	//const char* UTF8_FileName = TCHAR_TO_UTF8( Filename );
	//int WideLen = wcslen( Filename );
	//int UTF8Len = strlen( UTF8_FileName );
	if( !Exporter->Initialize( ANSIStr.c_str(), FileFormat, SdkManager->GetIOSettings()) )
	{
		UE_LOG( LogTemp, Warning, TEXT( "Call to KFbxExporter::Initialize() failed.\n" ) );
		auto ErrorString = Exporter->GetStatus().GetErrorString();
		FString Str = ErrorString;
		UE_LOG( LogTemp, Warning, TEXT( "Error returned: %s\n\n" ), *Str );
		return;
	}

	FbxManager::GetFileFormatVersion( Major, Minor, Revision );
	UE_LOG( LogTemp, Warning, TEXT( "FBX version number for this version of the FBX SDK is %d.%d.%d\n\n" ), Major, Minor, Revision );

	// Export the scene.
	Status = Exporter->Export( FBXScene );
	if( !Status )
	{
		const char* ErrorString = Exporter->GetStatus().GetErrorString();
		FString Str = ErrorString;
		FString DialogText = FString::Printf( TEXT(
			"Error!\n\n"
			"Could not save Mesh %s !\n"
			"Status = %s"
			"\n"
			"Do you have enough free space on that drive ?\n"
			"Do you have permission to save files there ?\n" ), *Filename, *Str );
		FText Txt = FText::FromString( DialogText );
		FMessageDialog::Open( EAppMsgType::Ok, Txt );
	}

	// Destroy the exporter.
	Exporter->Destroy();

	CloseDocument( FBXScene );

	return;
}

UFbxExportOption* GlobalFBXExportOptions = nullptr;
void PrepareFBXExporter( FString CurrentFilename, UnFbx::FFbxExporter** Exporter, FbxScene** FBXScene )
{
	*Exporter = UnFbx::FFbxExporter::GetInstance();
	(*Exporter)->SetExportOptionsOverride(GlobalFBXExportOptions);

	*FBXScene = CreateDocument( *Exporter );
}
bool ExportCancel = false;

void ExportFBXForActor( AActor *Actor, FString CurrentFilename )
{	
	UWorld* World = Actor->GetWorld();

	UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
	
	if( !GlobalFBXExportOptions )
	{
		ExportCancel = false;
		//Show the fbx export dialog options
		bool ExportAll = false;

		UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
		if ( UnitySettings->ShowFBXExportDialog )
			Exporter->FillExportOptions( false, true, CurrentFilename, ExportCancel, ExportAll );
		GlobalFBXExportOptions = GetExportOptions( Exporter );
	}
	else
		Exporter->SetExportOptionsOverride( GlobalFBXExportOptions );

	if( !ExportCancel )
	{
		auto TheScene = CreateDocument( Exporter );

		GWarn->StatusUpdate( 0, 1, NSLOCTEXT( "UnrealEd", "ExportingToFBX", "Exporting To FBX" ) );
		{
			ULevel* Level = World->PersistentLevel;

			//if( bSelectedOnly )
			//{
			//	Exporter->ExportBSP( World->GetModel(), true );
			//}

			INodeNameAdapter NodeNameAdapter;
			bool bSaveAnimSeq = true;

			TArray<AActor*> ActorToExport;
			ActorToExport.Add( Actor );
			//Exporter->ExportLevelMesh( Level, bSelectedOnly, NodeNameAdapter );
			ExportLevelMesh( Exporter, Level, true, ActorToExport, NodeNameAdapter, bSaveAnimSeq, TheScene );

			// Export streaming levels and actors
			//for( int32 CurLevelIndex = 0; CurLevelIndex < World->GetNumLevels(); ++CurLevelIndex )
			//{
			//	ULevel* CurLevel = World->GetLevel( CurLevelIndex );
			//	if( CurLevel != NULL && CurLevel != Level )
			//	{
			//		Exporter->ExportLevelMesh( CurLevel, bSelectedOnly, NodeNameAdapter );
			//	}
			//}
		}
		WriteToFile( Exporter , CurrentFilename, TheScene );
	}

	Exporter->SetExportOptionsOverride( nullptr );
}

extern TArray< AnimationEntry > AnimationsToExport;
void ExportAllAnimations( FScopedSlowTask& MeshesProgress, MeshBinding* MB )
{
	for (int a = 0; a < AnimationsToExport.Num(); a++)
	{
		AnimationEntry& Entry = AnimationsToExport[a];
		//Animations are now gathered from all components, export just this mesh's compatible animations since meshes are unique
		if (Entry.SkeletalMesh != MB->UnrealSkeletalMesh)
			continue;
		FString Str = FString::Printf( TEXT( "Exporting %d Animations for %s" ), AnimationsToExport.Num(), *MB->UnrealSkeletalMesh->GetName() );
		FText StrTxt = FText::FromString( Str );
		MeshesProgress.EnterProgressFrame( 1.0f / (float)AnimationsToExport.Num(), StrTxt );

		FString AnimFBXName = GenerateAnimationFilePath( Entry.AnimSeq );
		//Skip existing files, Animation export can take a while, people might end task this
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists( *AnimFBXName ))
			continue;

		UnFbx::FFbxExporter* AnimExporter = nullptr;
		FbxScene* AnimFBXScene = nullptr;
		PrepareFBXExporter( AnimFBXName, &AnimExporter, &AnimFBXScene );
		std::string AnimNameStr = ToANSIString( *Entry.AnimSeq->GetName() );
		if (AnimStack)
			AnimStack->SetName( AnimNameStr.c_str() );
		auto AnimNode = ExportAnimSequence( AnimExporter, Entry.AnimSeq, Entry.SkeletalMesh,
			GetExportOptions( AnimExporter )->bExportPreviewMesh, Entry.MeshName,
			AnimFBXScene->GetRootNode(), &Entry.OverrideMaterials, AnimFBXScene );
		AnimFBXScene->GetRootNode()->AddChild( AnimNode );

		WriteToFile( AnimExporter, AnimFBXName, AnimFBXScene );

		//Write Animation FBX Meta		
		std::string MetaData = ExporterInterface->GenerateAnimationFBXMeta( Entry.GUID, Entry.AnimSeq );
		int MetaDataLen = MetaData.length();
		FString AnimationFBXMetaPath = AnimFBXName + GetMetaExtension();
		std::string AnimationFBXMetaPathStr = ToANSIString( *AnimationFBXMetaPath );

		::SaveFile( AnimationFBXMetaPathStr.c_str(), (uint32*)MetaData.c_str(), MetaDataLen );
	}
}
#endif
std::string UnityExporter::TransformComponent_GetSceneData( TransformComponent* THIS )
{
	uint64 ParentID = 0;
	if( THIS->Owner->Parent )
	{
		TransformComponent * TC = ( TransformComponent*)THIS->Owner->Parent->GetComponent( CT_TRANSFORM );
		
		ParentID = TC->ID;
	}
	std::string ChildrenText = THIS->GetChildrenText();
	int Size = 1024 + ChildrenText.length();
	char* Text = new char[ Size ];
	snprintf( Text, Size,
			   "--- !u!4 &%lld\n"
			   "Transform:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_LocalRotation: {x: %f, y: %f, z: %f, w: %f}\n"
			   "  m_LocalPosition: {x: %f, y: %f, z: %f}\n"
			   "  m_LocalScale: {x: %f, y: %f, z: %f}\n"
			   "  m_ConstrainProportionsScale: 0\n"
			   "  m_Children:%s"
			   "  m_Father: {fileID: %lld}\n"
			   "  m_RootOrder: 0\n"
			   "  m_LocalEulerAnglesHint: {x: 0, y: 0, z: 0}\n"
			   ,
			  THIS->ID,
			  THIS->Owner->ID,//Gameobject
			  THIS->LocalRotation[ 0 ], THIS->LocalRotation[ 1 ], THIS->LocalRotation[ 2 ], THIS->LocalRotation[ 3 ],//Rotation
			  THIS->LocalPosition[ 0 ], THIS->LocalPosition[ 1 ], THIS->LocalPosition[ 2 ],//Position
			  THIS->LocalScale[ 0 ], THIS->LocalScale[ 1 ], THIS->LocalScale[ 2 ],//Scale
			  ChildrenText.c_str(),
			  ParentID//father
			   );
	
	std::string RetStr = Text;

	return RetStr;
}


std::string UnityExporter::MeshRenderer_GetSceneData( MeshRenderer* THIS )
{
	std::string MaterialText;
	char Line[ 512 ];

	if( THIS->Materials.size() > 0 )
	{
		//MeshFilter* MF = (MeshFilter*)Owner->GetComponent( ComponentType::CT_MESHFILTER );
		
		for( int i = 0; i < THIS->Materials.size(); i++ )
		{
			int fileIDBase = 2100000;
			UnityMaterial* Mat = THIS->Materials[ i ];
			//Don't reindex materials since I already did that based on LOD level
			std::string MatGUID = "0000000000000000f000000000000000";
			if( Mat )
				MatGUID = Mat->GUID;
			snprintf( Line, sizeof(Line), "  - {fileID: %d, guid: %s, type: 2}\n", fileIDBase, MatGUID.c_str() );
			MaterialText += Line;
		}
	}
	else
	{
		snprintf( Line, sizeof(Line), "  - {fileID: 10303, guid: 0000000000000000f000000000000000, type: 0}\n" );
		MaterialText += Line;
	}

	int TextLen = 2048 + MaterialText.length();
	char* Text = new char[ TextLen ];

	snprintf( Text, TextLen,
	"--- !u!23 &%lld\n"
	"MeshRenderer:\n"
	"  m_ObjectHideFlags: 0\n"
	"  m_CorrespondingSourceObject: {fileID: 0}\n"
	"  m_PrefabInstance: {fileID: 0}\n"
	"  m_PrefabAsset: {fileID: 0}\n"
	"  m_GameObject: {fileID: %lld}\n"
	"  m_Enabled: 1\n"
	"  m_CastShadows: %d\n"
	"  m_ReceiveShadows: 1\n"
	"  m_DynamicOccludee: 1\n"
	"  m_MotionVectors: 1\n"
	"  m_LightProbeUsage: 1\n"
	"  m_ReflectionProbeUsage: 1\n"
	"  m_RayTracingMode: 2\n"
	"  m_RayTraceProcedural: 0\n"
	"  m_RenderingLayerMask: 1\n"
	"  m_RendererPriority: 0\n"
	"  m_Materials:\n"
	"%s"
	"  m_StaticBatchInfo:\n"
	"    firstSubMesh: 0\n"
	"    subMeshCount: 0\n"
	"  m_StaticBatchRoot: {fileID: 0}\n"
	"  m_ProbeAnchor: {fileID: 0}\n"
	"  m_LightProbeVolumeOverride: {fileID: 0}\n"
	"  m_ScaleInLightmap: 1\n"
	"  m_ReceiveGI: 1\n"
	"  m_PreserveUVs: 1\n"
	"  m_IgnoreNormalsForChartDetection: 0\n"
	"  m_ImportantGI: 0\n"
	"  m_StitchLightmapSeams: 1\n"
	"  m_SelectedEditorRenderState: 3\n"
	"  m_MinimumChartSize: 4\n"
	"  m_AutoUVMaxDistance: 0.5\n"
	"  m_AutoUVMaxAngle: 89\n"
	"  m_LightmapParameters: {fileID: 0}\n"
	"  m_SortingLayerID: 0\n"
	"  m_SortingLayer: 0\n"
	"  m_SortingOrder: 0\n"
	"  m_AdditionalVertexStreams: {fileID: 0}\n"
	, THIS->ID, THIS->Owner->ID, THIS->CastShadows, MaterialText.c_str() );

	std::string RetStr = Text;

	delete[] Text;

	return RetStr;
}
std::string UnityExporter::SkinnedMeshRenderer_GetSceneData( SkinnedMeshRenderer* THIS )
{
	std::string MaterialText;
	char Line[ 512 ];

	if( THIS->Materials.size() > 0 )
	{
		MeshFilter* MF = (MeshFilter*)THIS->Owner->GetComponent( ComponentType::CT_MESHFILTER );

		for( int i = 0; i < THIS->Materials.size(); i++ )
		{
			int fileIDBase = 2100000;
			UnityMaterial* Mat = THIS->Materials[ i ];
			//Don't reindex materials since I already did that based on LOD level
			std::string MatGUID = "0000000000000000f000000000000000";
			if( Mat )
				MatGUID = Mat->GUID;
			snprintf( Line, sizeof(Line), "  - {fileID: %d, guid: %s, type: 2}\n", fileIDBase, MatGUID.c_str());
			MaterialText += Line;
		}
	}
	else
	{
		snprintf( Line, sizeof( Line ), "  - {fileID: 10303, guid: 0000000000000000f000000000000000, type: 0}\n" );
		MaterialText += Line;
	}

	THIS->FindBoneGameObjects();

	int MeshFileID = 4300000 + THIS->LOD * 2;
	if ( THIS->TotalLods > 1 )
		MeshFileID = 4300000 + THIS->LOD * 2;
	std::string AllBonesStr = THIS->GenerateBoneDesc();
	uint64 RootTransformID = THIS->GetRootTransformID();
	int TextLen = 2048 + MaterialText.length() + AllBonesStr.length();
	char* Text = new char[ TextLen ];
	snprintf( Text, TextLen,
			   "--- !u!137 &%lld\n"
			   "SkinnedMeshRenderer:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Enabled: 1\n"
			   "  m_CastShadows: %d\n"
			   "  m_ReceiveShadows: 1\n"
			   "  m_DynamicOccludee: 1\n"
			   "  m_StaticShadowCaster: 0\n"
			   "  m_MotionVectors: 1\n"
			   "  m_LightProbeUsage: 1\n"
			   "  m_ReflectionProbeUsage: 1\n"
			   "  m_RayTracingMode: 3\n"
			   "  m_RayTraceProcedural: 0\n"
			   "  m_RenderingLayerMask: 1\n"
			   "  m_RendererPriority: 0\n"
			   "  m_Materials:\n"
			   "%s"
			   "  m_StaticBatchInfo:\n"
			   "    firstSubMesh: 0\n"
			   "    subMeshCount: 0\n"
			   "  m_StaticBatchRoot: {fileID: 0}\n"
			   "  m_ProbeAnchor: {fileID: 0}\n"
			   "  m_LightProbeVolumeOverride: {fileID: 0}\n"
			   "  m_ScaleInLightmap: 1\n"
			   "  m_ReceiveGI: 1\n"
			   "  m_PreserveUVs: 1\n"
			   "  m_IgnoreNormalsForChartDetection: 0\n"
			   "  m_ImportantGI: 0\n"
			   "  m_StitchLightmapSeams: 1\n"
			   "  m_SelectedEditorRenderState: 3\n"
			   "  m_MinimumChartSize: 4\n"
			   "  m_AutoUVMaxDistance: 0.5\n"
			   "  m_AutoUVMaxAngle: 89\n"
			   "  m_LightmapParameters: {fileID: 0}\n"
			   "  m_SortingLayerID: 0\n"
			   "  m_SortingLayer: 0\n"
			   "  m_SortingOrder: 0\n"
			   "  serializedVersion: 2\n"
			   "  m_Quality: 0\n"
			   "  m_UpdateWhenOffscreen: 0\n"
			   "  m_SkinnedMotionVectors: 1\n"
			   "  m_Mesh: {fileID: %d, guid: %s, type: 3}\n"
			   "  m_Bones: %s"
			   "  m_BlendShapeWeights: []\n"
			   "  m_RootBone: {fileID: %lld}\n"
			   "  m_AABB:\n"
			   "    m_Center: {x: %f, y: %f, z: %f}\n"
			   "    m_Extent: {x: %f, y: %f, z: %f}\n"
			   "  m_DirtyAABB: 0\n"
			   , THIS->ID, THIS->Owner->ID, THIS->CastShadows, MaterialText.c_str(), MeshFileID, THIS->Mesh->GUID.c_str(), AllBonesStr.c_str(), RootTransformID,
			  THIS->Center.X, THIS->Center.Y, THIS->Center.Z,
			  THIS->Extent.X, THIS->Extent.Y, THIS->Extent.Z );

	std::string RetStr = Text;

	delete[] Text;

	return RetStr;
}
std::string UnityExporter::AnimationComponent_GetSceneData( AnimationComponent* THIS )
{
	char Text[ 1024];
	uint64 AnimationFileID = 7400000;	
	snprintf( Text, sizeof( Text ),
			  "--- !u!111 &%lld\n"
			  "Animation:\n"
			  "  m_ObjectHideFlags: 0\n"
			  "  m_CorrespondingSourceObject: {fileID: 0}\n"
			  "  m_PrefabInstance: {fileID: 0}\n"
			  "  m_PrefabAsset: {fileID: 0}\n"
			  "  m_GameObject: {fileID: %lld}\n"
			  "  m_Enabled: 1\n"
			  "  serializedVersion: 3\n"
			  "  m_Animation: {fileID: %lld, guid: %s, type: 3}\n"
			  "  m_Animations:\n"
			  "  - {fileID: %lld, guid: %s, type: 3}\n"
			  "  m_WrapMode: 0\n"
			  "  m_PlayAutomatically: 1\n"
			  "  m_AnimatePhysics: 0\n"
			  "  m_CullingType: 0\n",
			  THIS->ID, THIS->Owner->ID, AnimationFileID, THIS->AnimationGUID.c_str(), AnimationFileID, THIS->AnimationGUID.c_str() );

	std::string Ret = Text;
	return Ret;
}
std::string UnityExporter::MeshFilter_GetSceneData( MeshFilter* THIS )
{
	//uint64_t SubMeshID = 4300000 + LOD * 2;

	char Text[ 1024 ];
	snprintf( Text, sizeof( Text ),
		"--- !u!33 &%lld\n"
		"MeshFilter:\n"
		"  m_ObjectHideFlags: 0\n"
		"  m_CorrespondingSourceObject: {fileID: 0}\n"
		"  m_PrefabInstance: {fileID: 0}\n"
		"  m_PrefabAsset: {fileID: 0}\n"
		"  m_GameObject: {fileID: %lld}\n"
		"  m_Mesh: {fileID: %lld, guid: %s, type: 3}\n"
		, THIS->ID, THIS->Owner->ID, THIS->SubMeshID, THIS->Mesh->GUID.c_str() );

	std::string RetStr = Text;

	return RetStr;
}

std::string UnityExporter::LodGroup_GetSceneData( LodGroup* THIS )
{
	std::string LodsString = THIS->GetLODsString();

	int TextSize = 1024 + LodsString.length();
	char* Text = new char[ TextSize ];
	snprintf( Text, TextSize,
			   "--- !u!205 &%lld\n"
			   "LODGroup:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  serializedVersion: 2\n"
			   "  m_LocalReferencePoint: {x: %f, y: %f, z: %f}\n"
			   "  m_Size: %f\n"
			   "  m_FadeMode: 0\n"
			   "  m_AnimateCrossFading: 0\n"
			   "  m_LastLODIsBillboard: 0\n"
			   "  m_LODs:\n"
			   "%s"
			   "  m_Enabled: 1\n"
			   , THIS->ID, THIS->Owner->ID, THIS->LocalReferencePoint.X, THIS->LocalReferencePoint.Y, THIS->LocalReferencePoint.Z, THIS->Size, LodsString.c_str() );

	std::string RetStr = Text;

	delete[] Text;

	return RetStr;
}
std::string UnityExporter::MeshCollider_GetSceneData( MeshCollider* THIS )
{
	std::string MeshGUID = "";
	if( THIS->Mesh )
		MeshGUID = THIS->Mesh->GUID;
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!64 &%lld\n"
			   "MeshCollider:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Material: {fileID: 0}\n"
			   "  m_IsTrigger: 0\n"
			   "    m_Enabled: 1\n"
			   "    serializedVersion: 3\n"
			   "    m_Convex: %d\n"
			   "    m_CookingOptions: 14\n"
			   "    m_Mesh : {fileID: %lld, guid : %s, type : 3}\n",
			  THIS->ID, THIS->Owner->ID, THIS->IsConvex ? 1 : 0, THIS->SubMeshID, MeshGUID.c_str() );

	return Text;
}
std::string UnityExporter::BoxCollider_GetSceneData( BoxCollider* THIS )
{
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!65 &%lld\n"
			   "BoxCollider:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Material: {fileID: 0}\n"
			   "  m_IsTrigger: 0\n"
			   "  m_Enabled: 1\n"
			   "  serializedVersion: 2\n"
			   "  m_Size : {x: %f, y : %f, z : %f}\n"
			   "  m_Center : {x: %f, y : %f, z : %f}\n",
			  THIS->ID, THIS->Owner->ID, THIS->Size.X, THIS->Size.Y, THIS->Size.Z, THIS->Center.X, THIS->Center.Y, THIS->Center.Z );

	return Text;
}
std::string UnityExporter::SphereCollider_GetSceneData( SphereCollider* THIS )
{
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!135 &%lld\n"
			   "SphereCollider:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Material: {fileID: 0}\n"
			   "  m_IsTrigger: 0\n"
			   "  m_Enabled: 1\n"
			   "  serializedVersion: 2\n"
			   "  m_Radius : %f\n"
			   "  m_Center : {x: %f, y : %f, z : %f}\n",
			  THIS->ID, THIS->Owner->ID, THIS->Radius, THIS->Center.X, THIS->Center.Y, THIS->Center.Z );

	return Text;
}
std::string UnityExporter::CapsuleCollider_GetSceneData( CapsuleCollider* THIS )
{
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!136 &%lld\n"
			   "CapsuleCollider:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Material: {fileID: 0}\n"
			   "  m_IsTrigger: 0\n"
			   "  m_Enabled: 1\n"
			   "  m_Radius : %f\n"
			   "  m_Height : %f\n"
			   "  m_Direction : %d\n"
			   "  m_Center : {x: %f, y : %f, z : %f}\n",
			  THIS->ID, THIS->Owner->ID, THIS->Radius, THIS->Height, THIS->Direction, THIS->Center.X, THIS->Center.Y, THIS->Center.Z );

	return Text;
}
std::string UnityExporter::LightComponent_GetSceneData( LightComponent* THIS )
{
	char Text[ 2048 ];
	int EnabledInt = ( THIS->Enabled == true  ? 1: 0);
	int serializedVersion = 10;//previously 8 but the range wasn't correctly read in AreaLights !
	int ShadowsType = ( THIS->Shadows == true ? 2 : 0 );
	snprintf( Text, sizeof( Text ),
			   "--- !u!108 &%lld\n"
			   "Light:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Enabled: %d\n"
			   "  serializedVersion: %d\n"
			   "  m_Type: %d\n"
			   "  m_Color: {r: %f, g: %f, b: %f, a: %f}\n"
			   "  m_Intensity: %f\n"
			   "  m_Range: %f\n"
			   "  m_SpotAngle: %f\n"
			   "  m_CookieSize: 10\n"
			   "  m_Shadows:\n"
			   "    m_Type: %d\n"
			   "    m_Resolution: -1\n"
			   "    m_CustomResolution: -1\n"
			   "    m_Strength: 1\n"
			   "    m_Bias: 0.05\n"
			   "    m_NormalBias: 0.4\n"
			   "    m_NearPlane: 0.2\n"
			   "  m_Cookie: {fileID: 0}\n"
			   "  m_DrawHalo: 0\n"
			   "  m_Flare: {fileID: 0}\n"
			   "  m_RenderMode: 0\n"
			   "  m_CullingMask:\n"
			   "    serializedVersion: 2\n"
			   "    m_Bits: 4294967295\n"
			   "  m_Lightmapping: 4\n"
			   "  m_LightShadowCasterMode: 0\n"
			   "  m_AreaSize: {x: %f, y: %f}\n"
			   "  m_BounceIntensity: 1\n"
			   "  m_ColorTemperature: 6570\n"
			   "  m_UseColorTemperature: 0\n"
			   "  m_ShadowRadius: 0\n"
			   "  m_ShadowAngle: 0\n",
			  THIS->ID, THIS->Owner->ID, EnabledInt, serializedVersion, (int)THIS->Type,
			   ToFloat( THIS->Color.R), ToFloat( THIS->Color.G), ToFloat( THIS->Color.B ), ToFloat( THIS->Color.A ),
			  THIS->Intensity, THIS->Range, THIS->SpotAngle, ShadowsType,
			  THIS->Width, THIS->Height );

	std::string RetStr = Text;

	return RetStr;
}
std::string UnityExporter::ReflectionProbeComponent_GetSceneData( ReflectionProbeComponent* THIS )
{
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!215 &%lld\n"
			   "ReflectionProbe:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Enabled: 1\n"
			   "  serializedVersion: 2\n"
			   "  m_Type: 0\n"
			   "  m_Mode: 1\n"
			   "  m_RefreshMode: 0\n"
			   "  m_TimeSlicingMode: 0\n"
			   "  m_Resolution: 512\n"
			   "  m_UpdateFrequency: 0\n"
			   "  m_BoxSize: {x: %f, y: %f, z: %f}\n"
			   "  m_BoxOffset: {x: 0, y: 0, z: 0}\n"
			   "  m_NearClip: 0.3\n"
			   "  m_FarClip: 1000\n"
			   "  m_ShadowDistance: 100\n"
			   "  m_ClearFlags: 1\n"
			   "  m_BackGroundColor: {r: 0.19215687, g: 0.3019608, b: 0.4745098, a: 0}\n"
			   "  m_CullingMask:\n"
			   "    serializedVersion: 2\n"
			   "    m_Bits: 4294967295\n"
			   "  m_IntensityMultiplier: %f\n"
			   "  m_BlendDistance: 1\n"
			   "  m_HDR: 1\n"
			   "  m_BoxProjection: 0\n"
			   "  m_RenderDynamicObjects: 0\n"
			   "  m_UseOcclusionCulling: 1\n"
			   "  m_Importance: 1\n"
			   "  m_CustomBakedTexture: {fileID: 0}\n",
			  THIS->ID, THIS->Owner->ID, THIS->Size.X, THIS->Size.Y, THIS->Size.Z, THIS->Intensity );

	return Text;
}
std::string UnityExporter::DecalComponent_GetSceneData( DecalComponent* THIS )
{
	std::string MatGUID = "0000000000000000f000000000000000";
	if( THIS->Material )
		MatGUID = THIS->Material->GUID;

	char Text[ 2048 ];
	const char* ScriptGUID = "0777d029ed3dffa4692f417d4aba19ca";//URP
	if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
	{
		ScriptGUID = "f19d9143a39eb3b46bc4563e9889cfbd";
	}

	snprintf( Text, sizeof( Text ),
			   "--- !u!114 &%lld\n"
			   "MonoBehaviour:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Enabled: 1\n"
			   "  m_EditorHideFlags: 0\n"
			   "  m_Script: {fileID: 11500000, guid: %s, type: 3}\n"
			   "  m_Name: \n"
			   "  m_EditorClassIdentifier: \n"
			   "  m_Material: {fileID: 2100000, guid: %s, type: 2}\n"
			   "  m_DrawDistance: 1000\n"
			   "  m_FadeScale: 0.9\n"
			   "  m_StartAngleFade: 180\n"
			   "  m_EndAngleFade: 180\n"
			   "  m_UVScale: {x: 1, y: 1}\n"
			   "  m_UVBias: {x: 0, y: 0}\n"
			   "  m_ScaleMode: 1\n"
			   "  m_Offset: {x: 0, y: 0, z: 0.0}\n"
			   "  m_Size: {x: %f, y: %f, z: %f}\n"
			   "  m_FadeFactor: 1\n",
			  THIS->ID, THIS->Owner->ID, ScriptGUID, MatGUID.c_str(), THIS->Size.X, THIS->Size.Y, THIS->Size.Z );

	return Text;
}
std::string UnityExporter::AdditionalCameraData_GetSceneData( AdditionalCameraData* THIS )
{
	//URP only for now
	const char* ScriptGUID = "a79441f348de89743a2939f4d699eac1";
	
	char Text[ 4096 ];
	if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
	{
		snprintf( Text, sizeof( Text ),
				  "--- !u!114 &%lld\n"
				  "MonoBehaviour:\n"
				  "  m_ObjectHideFlags: 0\n"
				  "  m_CorrespondingSourceObject: {fileID: 0}\n"
				  "  m_PrefabInstance: {fileID: 0}\n"
				  "  m_PrefabAsset: {fileID: 0}\n"
				  "  m_GameObject: {fileID: %lld}\n"
				  "  m_Enabled: 1\n"
				  "  m_EditorHideFlags: 0\n"
				  "  m_Script: {fileID: 11500000, guid: %s, type: 3}\n"
				  "  m_Name: \n"
				  "  m_EditorClassIdentifier: \n"
				  "  m_RenderShadows: 1\n"
				  "  m_RequiresDepthTextureOption: 2\n"
				  "  m_RequiresOpaqueTextureOption: 2\n"
				  "  m_CameraType: 0\n"
				  "  m_Cameras: []\n"
				  "  m_RendererIndex: -1\n"
				  "  m_VolumeLayerMask:\n"
				  "    serializedVersion: 2\n"
				  "    m_Bits: 1\n"
				  "  m_VolumeTrigger: {fileID: 0}\n"
				  "  m_VolumeFrameworkUpdateModeOption: 2\n"
				  "  m_RenderPostProcessing: 1\n"
				  "  m_Antialiasing: 0\n"
				  "  m_AntialiasingQuality: 2\n"
				  "  m_StopNaN: 0\n"
				  "  m_Dithering: 0\n"
				  "  m_ClearDepth: 1\n"
				  "  m_AllowXRRendering: 1\n"
				  "  m_RequiresDepthTexture: 0\n"
				  "  m_RequiresColorTexture: 0\n"
				  "  m_Version: 2\n"
				  ,
				  THIS->ID, THIS->Owner->ID, ScriptGUID );
	}

	return Text;
}
std::string UnityExporter::AdditionalLightData_GetSceneData( AdditionalLightData* THIS )
{
	if( !THIS->Light )
		return "";

	const char* ScriptGUID = "7a68c43fe1f2a47cfa234b5eeaa98012";
	char Text[ 4096 ];
	
	if (UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP)	
	{
		ScriptGUID = "474bcb49853aa07438625e644c072ee6";
		snprintf( Text, sizeof( Text ),
			"--- !u!114 &%lld\n"
			"MonoBehaviour:\n"
			"  m_ObjectHideFlags: 0\n"
			"  m_CorrespondingSourceObject: {fileID: 0}\n"
			"  m_PrefabInstance: {fileID: 0}\n"
			"  m_PrefabAsset: {fileID: 0}\n"
			"  m_GameObject: {fileID: %lld}\n"
			"  m_Enabled: 1\n"
			"  m_EditorHideFlags: 0\n"
			"  m_Script: {fileID: 11500000, guid: %s, type: 3}\n"
			"  m_Name: \n"
			"  m_EditorClassIdentifier: \n"
			"  m_Version: 1\n"
			"  m_UsePipelineSettings: 1\n"
			"  m_AdditionalLightsShadowResolutionTier: 2\n"
			"  m_LightLayerMask: 1\n"
			"  m_CustomShadowLayers: 0\n"
			"  m_ShadowLayerMask: 1\n"
			"  m_LightCookieSize: {x: 1, y: 1}\n"
			"  m_LightCookieOffset: {x: 0, y: 0}\n",
			THIS->ID, THIS->Owner->ID, ScriptGUID
		);
	}
	if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
	{
		snprintf( Text, sizeof( Text ),
			  "--- !u!114 &%lld\n"
			  "MonoBehaviour:\n"
			  "  m_ObjectHideFlags: 0\n"
			  "  m_CorrespondingSourceObject: {fileID: 0}\n"
			  "  m_PrefabInstance: {fileID: 0}\n"
			  "  m_PrefabAsset: {fileID: 0}\n"
			  "  m_GameObject: {fileID: %lld}\n"
			  "  m_Enabled: 1\n"
			  "  m_EditorHideFlags: 0\n"
			  "  m_Script: {fileID: 11500000, guid: %s, type: 3}\n"
			  "  m_Name: \n"
			  "  m_EditorClassIdentifier: \n"
			  "  m_Intensity: %f\n"
			  "  m_EnableSpotReflector: 1\n"
			  "  m_LuxAtDistance : 1\n"
			  "  m_InnerSpotPercent: 0\n"
			  "  m_SpotIESCutoffPercent: 100\n"
			  "  m_LightDimmer: 1\n"
			  "  m_VolumetricDimmer: 1\n"
			  "  m_LightUnit: %d\n"
			  "  m_FadeDistance: 10000\n"
			  "  m_VolumetricFadeDistance: 10000\n"
			  "  m_AffectDiffuse: 1\n"
			  "  m_AffectSpecular: 1\n"
			  "  m_NonLightmappedOnly: 0\n"
			  "  m_ShapeWidth: 0.5\n"
			  "  m_ShapeHeight: 0.5\n"
			  "  m_AspectRatio: 1\n"
			  "  m_ShapeRadius: 0.025\n"
			  "  m_SoftnessScale: 1\n"
			  "  m_UseCustomSpotLightShadowCone: 0\n"
			  "  m_CustomSpotLightShadowCone: 30\n"
			  "  m_MaxSmoothness: 0.99\n"
			  "  m_ApplyRangeAttenuation: 1\n"
			  "  m_DisplayAreaLightEmissiveMesh: 0\n"
			  "  m_AreaLightCookie: {fileID: 0}\n"
			  "  m_IESPoint: {fileID: 0}\n"
			  "  m_IESSpot: {fileID: 0}\n"
			  "  m_IncludeForRayTracing: 1\n"
			  "  m_AreaLightShadowCone: 120\n"
			  "  m_UseScreenSpaceShadows: 0\n"
			  "  m_InteractsWithSky: 1\n"
			  "  m_AngularDiameter: 0.5\n"
			  "  m_FlareSize: 2\n"
			  "  m_FlareTint: {r: 1, g: 1, b: 1, a: 1}\n"
			  "  m_FlareFalloff: 4\n"
			  "  m_SurfaceTexture: {fileID: 0}\n"
			  "  m_SurfaceTint: {r: 1, g: 1, b: 1, a: 1}\n"
			  "  m_Distance: 1.5e+11\n"
			  "  m_UseRayTracedShadows: 0\n"
			  "  m_NumRayTracingSamples: 4\n"
			  "  m_FilterTracedShadow: 1\n"
			  "  m_FilterSizeTraced: 16\n"
			  "  m_SunLightConeAngle: 0.5\n"
			  "  m_LightShadowRadius: 0.5\n"
			  "  m_SemiTransparentShadow: 0\n"
			  "  m_ColorShadow: 1\n"
			  "  m_DistanceBasedFiltering: 0\n"
			  "  m_EvsmExponent: 15\n"
			  "  m_EvsmLightLeakBias: 0\n"
			  "  m_EvsmVarianceBias: 0.00001\n"
			  "  m_EvsmBlurPasses: 0\n"
			  "  m_LightlayersMask: 1\n"
			  "  m_LinkShadowLayers: 1\n"
			  "  m_ShadowNearPlane: 0.1\n"
			  "  m_BlockerSampleCount: 24\n"
			  "  m_FilterSampleCount: 16\n"
			  "  m_MinFilterSize: 0.1\n"
			  "  m_KernelSize: 5\n"
			  "  m_LightAngle: 1\n"
			  "  m_MaxDepthBias: 0.001\n"
			  "  m_ShadowResolution:\n"
			  "     m_Override: 512\n"
			  "    m_UseOverride: 1\n"
			  "    m_Level: 0\n"
			  "  m_ShadowDimmer: 1\n"
			  "  m_VolumetricShadowDimmer: 1\n"
			  "  m_ShadowFadeDistance: 10000\n"
			  "  m_UseContactShadow:\n"
			  "    m_Override : 0\n"
			  "    m_UseOverride: 1\n"
			  "    m_Level: 0\n"
			  "  m_RayTracedContactShadow: 0\n"
			  "  m_ShadowTint: {r: 0, g: 0, b: 0, a: 1}\n"
			  "  m_PenumbraTint: 0\n"
			  "  m_NormalBias: 0.75\n"
			  "  m_SlopeBias: 0.5\n"
			  "  m_ShadowUpdateMode: 0\n"
			  "  m_AlwaysDrawDynamicShadows: 0\n"
			  "  m_UpdateShadowOnLightMovement: 0\n"
			  "  m_CachedShadowTranslationThreshold: 0.01\n"
			  "  m_CachedShadowAngularThreshold: 0.5\n"
			  "  m_BarnDoorAngle: 90\n"
			  "  m_BarnDoorLength: 0.05\n"
			  "  m_preserveCachedShadow: 0\n"
			  "  m_OnDemandShadowRenderOnPlacement: 1\n"
			  "  m_ShadowCascadeRatios:\n"
			  "  - 0.05\n"
			  "  - 0.2\n"
			  "  - 0.3\n"
				"  m_ShadowCascadeBorders:\n"
				"  - 0.2\n"
				"  - 0.2\n"
				"  - 0.2\n"
				"  - 0.2\n"
				"  m_ShadowAlgorithm: 0\n"
				"  m_ShadowVariant: 0\n"
				"  m_ShadowPrecision: 0\n"
				"  useOldInspector: 0\n"
				"  useVolumetric: 1\n"
				"  featuresFoldout: 1\n"
				"  m_AreaLightEmissiveMeshShadowCastingMode: 0\n"
				"  m_AreaLightEmissiveMeshMotionVectorGenerationMode: 0\n"
				"  m_AreaLightEmissiveMeshLayer: -1\n"
				"  m_Version: 11\n"
				"  m_ObsoleteShadowResolutionTier: 1\n"
				"  m_ObsoleteUseShadowQualitySettings: 0\n"
				"  m_ObsoleteCustomShadowResolution: 512\n"
				"  m_ObsoleteContactShadows: 0\n"
				"  m_PointlightHDType: 0\n"
				"  m_SpotLightShape: 0\n"
				"  m_AreaLightShape: 0\n"
			  ,
			THIS->ID, THIS->Owner->ID, ScriptGUID, THIS->Light->Intensity, THIS->Unit );
	}
	return Text;
}
std::string UnityExporter::TreeInstancesComponent_GetSceneData( TreeInstancesComponent* THIS )
{
	char Text[ 4096 ];
	snprintf( Text, sizeof( Text ),
			  "--- !u!114 &%lld\n"
			  "MonoBehaviour:\n"
			  "  m_ObjectHideFlags: 0\n"
			  "  m_CorrespondingSourceObject: {fileID: 0}\n"
			  "  m_PrefabInstance: {fileID: 0}\n"
			  "  m_PrefabAsset: {fileID: 0}\n"
			  "  m_GameObject: {fileID: %lld}\n"
			  "  m_Enabled: 1\n"
			  "  m_EditorHideFlags: 0\n"
			  "  m_Script: {fileID: 11500000, guid:7f919289a76fc594f87d8f933c257fc5, type: 3}\n"
			  "  m_Name: \n"
			  "  m_EditorClassIdentifier: \n",
			  THIS->ID, THIS->Owner->ID );

	std::string InstanceText;
	InstanceText += "  position:";
	if( THIS->TreeInstances.size() > 0 )
	{
		InstanceText += "\n";
		for( int i = 0; i < THIS->TreeInstances.size(); i++ )
		{
			TreeInstancesComponent::TreeInstance* Instance = THIS->TreeInstances[ i ];
			char Local[ 256 ] = "";
			snprintf( Local, sizeof( Local ), "  - {x: %f, y: %f, z: %f}\n", Instance->Position.X, Instance->Position.Y, Instance->Position.Z );
			InstanceText += Local;
		}
	}
	else
	{
		InstanceText += " []\n";
	}

	InstanceText += "  widthScale:";
	if( THIS->TreeInstances.size() > 0 )
	{
		InstanceText += "\n";
		for( int i = 0; i < THIS->TreeInstances.size(); i++ )
		{
			TreeInstancesComponent::TreeInstance* Instance = THIS->TreeInstances[ i ];
			char Local[ 256 ] = "";
			snprintf( Local, sizeof( Local ), "  - %f\n", Instance->widthScale );
			InstanceText += Local;
		}
	}
	else
	{
		InstanceText += " []\n";
	}

	InstanceText += "  heightScale:";
	if( THIS->TreeInstances.size() > 0 )
	{
		InstanceText += "\n";
		for( int i = 0; i < THIS->TreeInstances.size(); i++ )
		{
			TreeInstancesComponent::TreeInstance* Instance = THIS->TreeInstances[ i ];
			char Local[ 256 ] = "";
			snprintf( Local, sizeof( Local ), "  - %f\n", Instance->heightScale );
			InstanceText += Local;
		}
	}
	else
	{
		InstanceText += " []\n";
	}
	
	InstanceText += "  rotation:";
	if( THIS->TreeInstances.size() > 0 )
	{
		InstanceText += "\n";
		for( int i = 0; i < THIS->TreeInstances.size(); i++ )
		{
			TreeInstancesComponent::TreeInstance* Instance = THIS->TreeInstances[ i ];
			char Local[ 256 ] = "";
			snprintf( Local, sizeof( Local ), "  - %f\n", Instance->rotation );
			InstanceText += Local;
		}
	}
	else
	{
		InstanceText += " []\n";
	}

	InstanceText += "  TreePrefabs:";
	if( THIS->TreeInstances.size() > 0 )
	{
		InstanceText += "\n";
		for( int i = 0; i < THIS->TreeInstances.size(); i++ )
		{
			TreeInstancesComponent::TreeInstance* Instance = THIS->TreeInstances[ i ];
			char Local[ 256 ] = "";
			std::string PrefabGUID = NullGUID;
			uint64 FileID = 0;
			if( Instance->Prefab )
			{
				FileID = Instance->Prefab->ID;
				PrefabGUID = Instance->Prefab->PrefabGUID;
			}
			snprintf( Local, sizeof( Local ), "  - {fileID: %lld, guid: %s, type: 3}\n", FileID,
					  PrefabGUID.c_str() );
			InstanceText += Local;
		}
	}
	else
	{
		InstanceText += " []\n";
	}

	InstanceText += "  GrassTextures:";
	if( THIS->GrassMeshes.size() > 0 )
	{
		InstanceText += "\n";
		for( int i = 0; i < THIS->GrassMeshes.size(); i++ )
		{
			TreeInstancesComponent::GrassMesh* Entry = THIS->GrassMeshes[ i ];
			char Local[ 256 ] = "";
			snprintf( Local, sizeof( Local ), "  - %s\n", Entry->GrassTexture.c_str() );
			InstanceText += Local;
		}
	}
	else
	{
		InstanceText += " []\n";
	}

	InstanceText += "  GrassPrefabs:";
	if( THIS->GrassMeshes.size() > 0 )
	{
		InstanceText += "\n";
		for( int i = 0; i < THIS->GrassMeshes.size(); i++ )
		{
			TreeInstancesComponent::GrassMesh* Entry = THIS->GrassMeshes[ i ];
			char Local[ 256 ] = "";
			std::string PrefabGUID = NullGUID;
			uint64 FileID = 0;
			if( Entry->Prefab )
			{
				FileID = Entry->Prefab->ID;
				PrefabGUID = Entry->Prefab->PrefabGUID;
			}
			snprintf( Local, sizeof( Local ), "  - {fileID: %lld, guid: %s, type: 3}\n", FileID, PrefabGUID.c_str() );
			InstanceText += Local;
		}
	}
	else
	{
		InstanceText += " []\n";
	}

	std::string Ret = Text;

	snprintf( Text, sizeof( Text ), "  HeightMapTexture: %s\n", THIS->HeightMapTexture.c_str() );
	InstanceText += Text;
	snprintf( Text, sizeof( Text ), "  TerrainHeight: %f\n", THIS->TerrainHeight );
	InstanceText += Text;

	Ret += InstanceText;
	return Ret;
}
UnityLightIntensityUnity GetUnityLightIntensity( ELightUnits Unit )
{
	switch( Unit )
	{
		default: return LIU_LUX;
		case ELightUnits::Candelas: return LIU_CANDELAS;
		case ELightUnits::Lumens: return LIU_LUMENS;
	}
}
void UnityExporter::TranslateLightData( ULightComponent* LightComp, LightComponent* NewLightComponent, AdditionalLightData* LightData )
{
	ELightComponentType LightType = LightComp->GetLightType();
	if( LightType == LightType_Directional )
	{
		UDirectionalLightComponent* DirectionalLightComp = Cast< UDirectionalLightComponent>( LightComp );

		float Brightness = DirectionalLightComp->ComputeLightBrightness();
		if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
		{
			//HDRP uses Lux for directional light as well
			NewLightComponent->Intensity = DirectionalLightComp->Intensity;
		}
		else
			NewLightComponent->Intensity = DirectionalLightComp->Intensity;// Brightness / PI;
	}
	else if( LightType == LightType_Point )
	{
		UPointLightComponent* PointLightComp = Cast< UPointLightComponent>( LightComp );
		NewLightComponent->Range = PointLightComp->AttenuationRadius / 100.0f;

		if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP && PointLightComp->IntensityUnits != ELightUnits::Unitless )
		{
			LightData->Unit = GetUnityLightIntensity( PointLightComp->IntensityUnits );
			NewLightComponent->Intensity = PointLightComp->Intensity;
		}
		else
		{
			if( PointLightComp->IntensityUnits != ELightUnits::Candelas )
			{
				float LastLightBrigtness = PointLightComp->ComputeLightBrightness();
				PointLightComp->IntensityUnits = ELightUnits::Candelas;
			#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
				PointLightComp->SetLightBrightness( LastLightBrigtness );
			#endif
			}

			if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
				NewLightComponent->Intensity = PointLightComp->Intensity;
			else if ( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
				NewLightComponent->Intensity = PointLightComp->Intensity * 3.0f;
			else
				NewLightComponent->Intensity = PointLightComp->Intensity / 3.0f;
		}
	}
	else if( LightType == LightType_Spot )
	{
		USpotLightComponent* SpotLightComp = Cast< USpotLightComponent>( LightComp );
		NewLightComponent->Range = SpotLightComp->AttenuationRadius / 100.0f;
		NewLightComponent->SpotAngle = ConvertSpotAngle( SpotLightComp->OuterConeAngle );

		if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP && SpotLightComp->IntensityUnits != ELightUnits::Unitless )
		{
			LightData->Unit = GetUnityLightIntensity( SpotLightComp->IntensityUnits );
			NewLightComponent->Intensity = SpotLightComp->Intensity;
		}
		else
		{
			if( SpotLightComp->IntensityUnits != ELightUnits::Candelas )
			{
				float LastLightBrigtness = SpotLightComp->ComputeLightBrightness();
				SpotLightComp->IntensityUnits = ELightUnits::Candelas;
			#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
				SpotLightComp->SetLightBrightness( LastLightBrigtness );
			#endif
			}

			if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
				NewLightComponent->Intensity = SpotLightComp->Intensity;
			else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
				NewLightComponent->Intensity = SpotLightComp->Intensity * 3.0f;
			else
				NewLightComponent->Intensity = SpotLightComp->Intensity / 3.0f;
		}
	}
	else if( LightType == LightType_Rect )
	{
		URectLightComponent* RectLightComponent = Cast< URectLightComponent>( LightComp );

		NewLightComponent->Width = RectLightComponent->SourceHeight / 100.0f;
		NewLightComponent->Height = RectLightComponent->SourceWidth / 100.0f;
		NewLightComponent->Range = RectLightComponent->AttenuationRadius / 100.0f;

		if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP && RectLightComponent->IntensityUnits != ELightUnits::Unitless )
		{
			LightData->Unit = GetUnityLightIntensity( RectLightComponent->IntensityUnits );
			NewLightComponent->Intensity = RectLightComponent->Intensity;
		}
		else
		{
			if( RectLightComponent->IntensityUnits != ELightUnits::Candelas )
			{
				float LastLightBrigtness = RectLightComponent->ComputeLightBrightness();
				RectLightComponent->IntensityUnits = ELightUnits::Candelas;
			#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
				RectLightComponent->SetLightBrightness( LastLightBrigtness );
			#endif
			}

			if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
				NewLightComponent->Intensity = RectLightComponent->Intensity;
			else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
				NewLightComponent->Intensity = RectLightComponent->Intensity * 3.0f;
			else
				NewLightComponent->Intensity = RectLightComponent->Intensity / 3.0f;
		}
	}
}
void UnityExporter::CopyGlobalFiles()
{
	FString AssetsFolder = ExporterInterface->GetAssetsFolder();

	FString ShadersFolder = AssetsFolder + TEXT( "Shaders/" );
	FString ScriptsFolder = AssetsFolder + TEXT( "Scripts/" );
	FString ProjectSettingsFolder = AssetsFolder + TEXT( "../ProjectSettings/" );
	FString SettingsFolder = AssetsFolder + TEXT( "Settings/" );
	FString PackagesFolder = AssetsFolder + TEXT( "../Packages/" );
	FString HDRPDefaultResourcesFolder = AssetsFolder + TEXT( "../Assets/HDRPDefaultResources/" );

	VerifyOrCreateDirectory( ShadersFolder );
	VerifyOrCreateDirectory( ProjectSettingsFolder );
	VerifyOrCreateDirectory( ScriptsFolder );

	if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
	{
		CopyToOutput( TEXT( "URP/ProjectSettings.asset" ), TEXT( "ProjectSettings/ProjectSettings.asset" ), false );
		CopyToOutput( TEXT( "URP/GraphicsSettings.asset" ), TEXT( "ProjectSettings/GraphicsSettings.asset" ), false );
		CopyToOutput( TEXT( "URP/ProjectVersion.txt" ), TEXT( "ProjectSettings/ProjectVersion.txt" ), false );
		CopyToOutput( TEXT( "URP/QualitySettings.asset" ), TEXT( "ProjectSettings/QualitySettings.asset" ), false );
	}
	else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
	{
		CopyToOutput( TEXT( "HDRP/ProjectSettings.asset" ), TEXT( "ProjectSettings/ProjectSettings.asset" ), false );
		CopyToOutput( TEXT( "HDRP/GraphicsSettings.asset" ), TEXT( "ProjectSettings/GraphicsSettings.asset" ), false );
		CopyToOutput( TEXT( "HDRP/ProjectVersion.txt" ), TEXT( "ProjectSettings/ProjectVersion.txt" ), false );
	}
	else
	{
		CopyToOutput( TEXT( "ProjectSettings.asset" ), TEXT( "ProjectSettings/ProjectSettings.asset" ), false );
		CopyToOutput( TEXT( "ProjectVersion.txt" ), TEXT( "ProjectSettings/ProjectVersion.txt" ), false );
	}

	CopyToOutput( TEXT( "Utilities.cs" ), TEXT( "Assets/Scripts/Utilities.cs" ), false );
	CopyToOutput( TEXT( "TreeInstanceComponent.cs" ), TEXT( "Assets/Scripts/TreeInstanceComponent.cs" ), false );
	CopyToOutput( TEXT( "TreeInstanceComponent.cs.meta" ), TEXT( "Assets/Scripts/TreeInstanceComponent.cs.meta" ), false );
	CopyToOutput( TEXT( "InstancedMesh.cs" ), TEXT( "Assets/Scripts/InstancedMesh.cs" ), false );

	VerifyOrCreateDirectory( PackagesFolder );

	if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_BUILTIN )
	{
		CopyToOutput( TEXT( "manifest.json" ), TEXT( "/Packages/manifest.json" ), false );
		CopyToOutput( TEXT( "packages-lock.json" ), TEXT( "/Packages/packages-lock.json" ), false );
	}
	else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
	{
		VerifyOrCreateDirectory( SettingsFolder );

		CopyToOutput( TEXT( "URP/ForwardRenderer.asset" ), TEXT( "/Assets/Settings/ForwardRenderer.asset" ), false );
		CopyToOutput( TEXT( "URP/ForwardRenderer.asset.meta" ), TEXT( "/Assets/Settings/ForwardRenderer.asset.meta" ), false );
		CopyToOutput( TEXT( "URP/UniversalRP-HighQuality.asset" ), TEXT( "/Assets/Settings/UniversalRP-HighQuality.asset" ), false );
		CopyToOutput( TEXT( "URP/UniversalRP-HighQuality.asset.meta" ), TEXT( "/Assets/Settings/UniversalRP-HighQuality.asset.meta" ), false );
		CopyToOutput( TEXT( "URP/manifest.json" ), TEXT( "/Packages/manifest.json" ), false );
		CopyToOutput( TEXT( "URP/packages-lock.json" ), TEXT( "/Packages/packages-lock.json" ), false );
		CopyToOutput( TEXT( "URP/Global Volume Profile.asset" ), TEXT( "/Assets/Settings/Global Volume Profile.asset" ), false );
		CopyToOutput( TEXT( "URP/Global Volume Profile.asset.meta" ), TEXT( "/Assets/Settings/Global Volume Profile.asset.meta" ), false );
		//CopyToOutput( TEXT("URP/URPCommonPass.hlsl", TEXT("/Assets/Shaders/URPCommonPass.hlsl" );
	}
	else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
	{
		VerifyOrCreateDirectory( HDRPDefaultResourcesFolder );

		CopyToOutput( TEXT( "HDRP/DefaultHDRPAsset.asset" ), TEXT( "/Assets/HDRPDefaultResources/DefaultHDRPAsset.asset" ), false );
		CopyToOutput( TEXT( "HDRP/DefaultHDRPAsset.asset.meta" ), TEXT( "/Assets/HDRPDefaultResources/DefaultHDRPAsset.asset.meta" ), false );
		CopyToOutput( TEXT( "HDRP/DefaultSettingsVolumeProfile.asset" ), TEXT( "/Assets/HDRPDefaultResources/DefaultSettingsVolumeProfile.asset" ), true );
		CopyToOutput( TEXT( "HDRP/DefaultSettingsVolumeProfile.asset.meta" ), TEXT( "/Assets/HDRPDefaultResources/DefaultSettingsVolumeProfile.asset.meta" ), true );
		CopyToOutput( TEXT( "HDRP/HDRenderPipelineGlobalSettings.asset" ), TEXT( "/Assets/HDRPDefaultResources/HDRenderPipelineGlobalSettings.asset" ), true );
		CopyToOutput( TEXT( "HDRP/HDRenderPipelineGlobalSettings.asset.meta" ), TEXT( "/Assets/HDRPDefaultResources/HDRenderPipelineGlobalSettings.asset.meta" ), true );
		CopyToOutput( TEXT( "HDRP/manifest.json" ), TEXT( "/Packages/manifest.json" ), false );
		CopyToOutput( TEXT( "HDRP/packages-lock.json" ), TEXT( "/Packages/packages-lock.json" ), false );
		//CopyToOutput( TEXT("URP/URPCommonPass.hlsl", TEXT("/Assets/Shaders/URPCommonPass.hlsl" );
	}

	if( UnitySettings->Shaders == UnityShadersType::ST_CUSTOM_SHADERS )
	{
		CopyToOutput( TEXT( "UnrealCommon.cginc" ), TEXT( "Assets/Shaders/UnrealCommon.cginc" ), false );
	#if ENGINE_MAJOR_VERSION >= 5
		CopyToOutput( TEXT( "LargeWorldCoordinates.hlsl" ), TEXT( "Assets/Shaders/LargeWorldCoordinates.hlsl" ), false );
		CopyToOutput( TEXT( "LWCOperations.hlsl" ), TEXT( "Assets/Shaders/LWCOperations.hlsl" ), false );
	#endif

		if( UnitySettings && ( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP ))
		{
			ShowWarningIfFileIsMissing( TEXT( "URP/URPGraphShader.shader" ) );
			ShowWarningIfFileIsMissing( TEXT( "URP/URPDecalGraphShader.shader" ) );
			ShowWarningIfFileIsMissing( TEXT( "URP/URPFullscreenShader.shader" ) );
		}
		else  if( UnitySettings && ( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP ))
		{
			ShowWarningIfFileIsMissing( TEXT( "HDRP/HDRPGraphShader.shader" ) );
			ShowWarningIfFileIsMissing( TEXT( "HDRP/HDRPDecalGraphShader.shader" ) );
		}
	}
}
FString UnityExporter::GetLandscapeWeightString()
{
	return TEXT( "%s.r + " );
}
std::string UnityExporter::GenerateTextureMeta( UnityTexture* Tex, bool IsNormalMap, int sRGB, UnityTextureShape Shape, int TileX, int TileY )
{
	char Text[ 2048 ] = "";
	
	int texturetype = 0;

	if( IsNormalMap )
	{		
		texturetype = 1;
	}

	int textureShape = 1;
	if( Shape == UTS_Cube )
		textureShape = 2;
	else if( Shape == UTS_2DArray )
		textureShape = 4;
	else if( Shape == UTS_3D )
		textureShape = 8;

	int MaxTextureSize = 16384;
	if (Tex->MaxTextureSize > 0)
	{
		MaxTextureSize = Tex->MaxTextureSize;

		MaxTextureSize = FMath::RoundUpToPowerOfTwo( MaxTextureSize );
	}

	int serializedVersion = 4;
	if( serializedVersion == 4 )
	{
		snprintf( Text, sizeof( Text ),
				  "fileFormatVersion: 2\n"
				  "guid: %s\n"
				  "TextureImporter:\n"
				  "  fileIDToRecycleName: {}\n"
				  "  serializedVersion: 4\n"
				  "  mipmaps:\n"
				  "    mipMapMode: 0\n"
				  "    enableMipMap: 1\n"
				  "    sRGBTexture: %d\n"
				  "    linearTexture: 0\n"
				  "    fadeOut: 0\n"
				  "    borderMipMap: 0\n"
				  "    mipMapsPreserveCoverage: 0\n"
				  "    alphaTestReferenceValue: 0.5\n"
				  "    mipMapFadeDistanceStart: 1\n"
				  "    mipMapFadeDistanceEnd: 3\n"
				  "  bumpmap:\n"
				  "    convertToNormalMap: 0\n"
				  "    externalNormalMap: 0\n"
				  "    heightScale: 0.25\n"
				  "    normalMapFilter: 0\n"
				  "  isReadable: %d\n"
				  "  grayScaleToAlpha: 0\n"
				  "  generateCubemap: 6\n"
				  "  cubemapConvolution: 0\n"
				  "  seamlessCubemap: 0\n"
				  "  textureFormat: 1\n"
				  "  maxTextureSize: %d\n"
				  "  textureSettings:\n"
				  "    serializedVersion: 2\n"
				  "    filterMode: -1\n"
				  "    aniso: -1\n"
				  "    mipBias: -1\n"
				  "    wrapU: -1\n"
				  "    wrapV: -1\n"
				  "    wrapW: -1\n"
				  "  nPOTScale: 0\n"
				  "  lightmap: 0\n"
				  "  compressionQuality: 50\n"
				  "  spriteMode: 0\n"
				  "  spriteExtrude: 1\n"
				  "  spriteMeshType: 1\n"
				  "  alignment: 0\n"
				  "  spritePivot: {x: 0.5, y: 0.5}\n"
				  "  spriteBorder: {x: 0, y: 0, z: 0, w: 0}\n"
				  "  spritePixelsToUnits: 100\n"
				  "  alphaUsage: 1\n"
				  "  alphaIsTransparency: 0\n"
				  "  spriteTessellationDetail: -1\n"
				  "  textureType: %d\n"
				  "  textureShape: %d\n"
				  "  singleChannelComponent: 0\n"
				  "  flipbookRows : %d\n"
				  "  flipbookColumns : %d\n"
				  "  maxTextureSizeSet: 0\n"
				  "  compressionQualitySet: 0\n"
				  "  textureFormatSet: 0\n"
				  "  platformSettings:\n"
				  "  - buildTarget: DefaultTexturePlatform\n"
				  "    maxTextureSize: %d\n"
				  "    textureFormat: -1\n"
				  "    textureCompression: %d\n"
				  "    compressionQuality: 50\n"
				  "    crunchedCompression: 0\n"
				  "    allowsAlphaSplitting: 0\n"
				  "    overridden: 0\n"
				  "  spriteSheet:\n"
				  "    serializedVersion: 2\n"
				  "    sprites: []\n"
				  "    outline: []\n"
				  "    physicsShape: []\n"
				  "  spritePackingTag: \n"
				  "  userData: \n"
				  "  assetBundleName: \n"
				  "  assetBundleVariant: \n",
				  Tex->GUID.c_str(), sRGB, (Tex->IsReadable ? 1 : 0 ), MaxTextureSize, texturetype, textureShape, TileY, TileX, MaxTextureSize, Tex->Compression );
	}
	return Text;
}
void UnityExporter::WriteScene( ExporterScene* Scene, const char* File, const char* ProxyScene )
{
	FILE* f = nullptr;
	f = fopen( File, "w" );
	if( !f )
	{
		return;
	}

	std::string SceneProxy;
	std::string Str = SceneProxy;

	for( int i = 0; i < Scene->GameObjects.size(); i++ )
	{
		GameObject* GO = Scene->GameObjects[ i ];
		if( !GO )
			continue;

		if( !GO->Parent )
		{
			if( Scene->UsePrefabs //&& GO->GetComponentInChildren( CT_MESHRENDERER )
				&& GO->PrefabTemplate )
			{
				std::string TextData = GO->GeneratePrefabInstance();
				Str += TextData;
			}
			else
			{
				std::string TextData = GO->ToSceneData();
				Str += TextData;
			}
		}
	}

	unsigned char* ProxySceneData = nullptr;
	int Size = LoadFile( ProxyScene, &ProxySceneData );
	if( !ProxySceneData || Size <= 0 )
	{
		FString Str2 = ProxyScene;
		UE_LOG( LogTemp, Error, TEXT( "LoadFile( %s ) failed" ), *Str2 );
		return;
	}

	std::string FinalSceneData = (char*)ProxySceneData;

	FinalSceneData += Str;

	fprintf( f, "%s", FinalSceneData.c_str() );
	fclose( f );
}
std::string UnityExporter::GenerateMaterialMeta( std::string GUID )
{

	char Text[ 2048 ] = "";
	snprintf( Text, sizeof( Text ),
			  "fileFormatVersion: 2\n"
			  "guid: %s\n"
			  "NativeFormatImporter:\n"
			  "  mainObjectFileID: 2100000\n"
			  "  userData: \n"
			  "  assetBundleName: \n"
			  "  assetBundleVariant: \n",
			  GUID.c_str() );

	return Text;
}

std::string UnityExporter::GenerateShaderMeta( std::string GUID )
{
	char Text[ 2048 ] = "";
	snprintf( Text, sizeof( Text ),
			  "fileFormatVersion: 2\n"
			  "guid: %s\n"
			  "ShaderImporter:\n"
			  "  defaultTextures: []\n"
			  "  userData: \n"
			  "  assetBundleName: \n"
			  "  assetBundleVariant: \n",
			  GUID.c_str() );

	return Text;
}
std::string UnityExporter::GenerateAnimationFBXMeta( std::string GUID, UAnimSequence* Anim )
{
	std::string AnimationName = ToANSIString( *Anim->GetName() );
	
	int TextLength = 4096;
	char* Text = new char[ TextLength ];

	std::string EventsText = " []\n";
	int NumEvents = 0;
	if( Anim->AnimNotifyTracks.Num() > 0 )
	{
		for( int t = 0; t < Anim->AnimNotifyTracks.Num(); t++ )
		{
			const FAnimNotifyTrack& Track = Anim->AnimNotifyTracks[ t ];

			for( int n = 0; n < Track.Notifies.Num(); n++ )
			{
				if( NumEvents == 0 )
				{
					EventsText = "\n";
				}
				NumEvents++;
				FAnimNotifyEvent* Event = Track.Notifies[ n ];
				float Time = Event->GetTriggerTime();

				std::string FunctionName = "NewEvent";
				FunctionName = ToANSIString( *Event->NotifyName.ToString() );

				snprintf( Text, TextLength,
						  "      - time: %f\n"
						  "        functionName: %s\n"
						  "        data: \n"
						  "        objectReferenceParameter: {instanceID: 0}\n"
						  "        floatParameter: 0\n"
						  "        intParameter: %d\n"
						  "        messageOptions: 0\n"
						  , Time, FunctionName.c_str(), n );

				EventsText += Text;
			}			
		}
	}

	int avatarSetup = 1;//0=None,1=CreateFromThisModel
	int AnimationFrames = (int)( Anim->GetPlayLength() * 30.0f );
	snprintf( Text, TextLength,
			  "fileFormatVersion: 2\n"
			  "guid: %s\n"
			  "ModelImporter:\n"
			  "  serializedVersion: 23\n"
			  "  fileIDToRecycleName:\n"
		      "    100000: //RootNode\n"
		      "    7400000: %s\n"
			  "  animations:\n"
			  "    animationWrapMode: 2\n"
			  "    clipAnimations:\n"
			  "    - serializedVersion: 16\n"
			  "      name: %s\n"
			  "      takeName: %s\n"
			  "      internalID: 7400000\n"
			  "      firstFrame: 0\n"
			  "      lastFrame: %d\n"
			  "      wrapMode: 0\n"
			  "      orientationOffsetY: 0\n"
			  "      level: 0\n"
			  "      cycleOffset: 0\n"
			  "      loop: 1\n"
			  "      hasAdditiveReferencePose: 0\n"
			  "      loopTime: 0\n"
			  "      loopBlend: 0\n"
			  "      loopBlendOrientation: 0\n"
			  "      loopBlendPositionY: 0\n"
			  "      loopBlendPositionXZ: 0\n"
			  "      keepOriginalOrientation: 0\n"
			  "      keepOriginalPositionY: 1\n"
			  "      keepOriginalPositionXZ: 0\n"
			  "      heightFromFeet: 0\n"
			  "      mirror: 0\n"
			  "      bodyMask: 01000000010000000100000001000000010000000100000001000000010000000100000001000000010000000100000001000000\n"
			  "      curves: []\n"
			  "      events:%s"
			  "  animationType: 1\n"
			  "  avatarSetup: %d\n"
			  , GUID.c_str(), AnimationName.c_str(), AnimationName.c_str(), AnimationName.c_str(), AnimationFrames, EventsText.c_str(), avatarSetup );

	std::string Ret;
	Ret = Text;
	delete[] Text;
	return Ret;
}
enum FileIDType
{
	FIT_MATERIALS,
	FIT_MESHES,
};
class FileID
{
public:
	FileID( int pBaseID, const char* str, int pIndex = 0 )
	{
		BaseID = pBaseID;
		//Type = pType;
		Identifier = str;
		Index = pIndex;
	}
	int BaseID;
	//FileIDType Type;
	int Index;
	std::string Identifier;

	static int GetBase( FileIDType Type )
	{
		switch (Type)
		{
		case FIT_MATERIALS: return 2100000;
		case FIT_MESHES: return 4300000;
		default:return Type;
		}

		return Type;
	}
	std::string GetMetaLine()
	{
		//int Base = GetBase( Type );
		int ID = BaseID + Index * 2;
		char Line[1024];
		snprintf( Line, sizeof( Line ), "    %d: %s\n", ID, Identifier.c_str() );

		return Line;
	}
};
void AddLodObjects( int ID, int NumLods, const char* MeshName, std::string& str )
{
	char Text[1024] = "";
	for (int i = 0; i < NumLods; i++)
	{
		snprintf( Text, sizeof( Text ), "    %d: %s_LOD%i\n", ID, MeshName, i );
		ID += 2;
		str += Text;
	}
}
std::string GenerateMaterialIDs( UnityMesh* M )
{
	char Text[1024] = "";
	std::string MaterialsText;
	for (int i = 0; i < M->Materials.size(); i++)
	{
		int ID = 2100000 + i * 2;
		snprintf( Text, sizeof( Text ), "    %d: %s\n", ID, M->Materials[i]->Name.c_str() );
		MaterialsText += Text;
	}

	return MaterialsText;
}
std::string GenerateFileIDsForBones( UnityMesh* M, int BaseID )
{
	std::string Ret;
	char Text[4096] = "";
	for (int i = 0; i < M->BoneNames.size(); i++)
	{
		int ID = BaseID + i * 2;
		snprintf( Text, sizeof( Text ), "    %d: %s\n", ID, M->BoneNames[i].c_str() );
		Ret += Text;
	}

	return Ret;
}
std::string GenerateMeshMetaFileIDs( UnityMesh* M, bool IsFBX, int TotalLods )
{
	const char* MeshName = M->Name.c_str();
	if (IsFBX)
	{
		std::string Ret;
		int TextSize = 4096;
		char* Text = new char[TextSize];
		std::string MaterialsText = GenerateMaterialIDs( M );
		if (TotalLods > 1)
		{
			std::string BoneNames1 = "";
			std::string BoneNames2 = "";
			if (M->BoneNames.size() > 0)
			{
				int BaseID1 = 100000 + (TotalLods + 1) * 2;
				int BaseID2 = 400000 + (TotalLods + 1) * 2;
				BoneNames1 = GenerateFileIDsForBones( M, BaseID1 );
				BoneNames2 = GenerateFileIDsForBones( M, BaseID2 );
			}
			snprintf( Text, TextSize, "    100000: //RootNode\n" );
			Ret += Text;
			AddLodObjects( 100002, TotalLods, MeshName, Ret );
			Ret += BoneNames1;
			snprintf( Text, TextSize, "    400000: //RootNode\n" );
			Ret += Text;
			AddLodObjects( 400002, TotalLods, MeshName, Ret );
			Ret += BoneNames2;
			Ret += MaterialsText;
			AddLodObjects( 2300000, TotalLods, MeshName, Ret );
			AddLodObjects( 3300000, TotalLods, MeshName, Ret );
			AddLodObjects( 4300000, TotalLods, MeshName, Ret );
			if (M->HasConvexHulls)
			{
				snprintf( Text, TextSize, "    %d: %s_ConvexHulls\n", M->ConvexHullMeshID, MeshName );
				Ret += Text;
			}
			snprintf( Text, TextSize, "    20500000: //RootNode\n" );
			Ret += Text;
		}
		else
		{
			std::string BoneNames1 = "";
			std::string BoneNames2 = "";
			if (M->BoneNames.size() > 0)
			{
				TextSize = 256 * M->BoneNames.size() * 2;
				delete[]Text;
				Text = new char[TextSize];
				BoneNames1 = GenerateFileIDsForBones( M, 100002 );
				BoneNames2 = GenerateFileIDsForBones( M, 400002 );
			}
			snprintf( Text, TextSize,
				"    100000: //RootNode\n"
				"%s"
				"    400000: //RootNode\n"
				"%s"
				"%s"
				"    2300000: //RootNode\n"
				"    3300000: //RootNode\n"
				"    4300000: %s\n"
				"    4300002: %s\n"
				, BoneNames1.c_str(), BoneNames2.c_str(), MaterialsText.c_str(), MeshName, MeshName
			);
			Ret += Text;
			if (M->HasConvexHulls)
			{
				snprintf( Text, TextSize, "    %d: %s_ConvexHulls\n", M->ConvexHullMeshID, MeshName );
				Ret += Text;
			}
		}
		delete[] Text;
		return Ret;
	}
	else
	{
		int SubMeshes = M->Sections.size();

		std::string Ret;
		if (SubMeshes <= 1)
		{
			char Text[4096] = "";
			snprintf( Text, sizeof( Text ),
				"    100000: %s\n"
				"    100002: //RootNode\n"
				"    400000: %s\n"
				"    400002: //RootNode\n"
				"    2300000: %s\n"
				"    3300000: %s\n"
				"    4300000: %s\n",
				MeshName, MeshName, MeshName, MeshName, MeshName );
			return  Text;
		}
		int TextLength = 1024 + SubMeshes * 128;
		char* Text = new char[TextLength];

		snprintf( Text, TextLength,
			"    100000: %s\n"
			"    100002: //RootNode\n",
			MeshName );
		Ret += Text;

		std::vector<FileID> FileIDArray;

		//100000: //RootNode
		//400000 : //RootNode
		//2100000 : MI_BS_Screens
		//2100002 : MI_BS_TV_Screen_01
		//2300000 : //RootNode
		//3300000 : //RootNode
		//4300000 : SM_BS_Screen_01_2

		FileIDArray.push_back( FileID( 100000, "//RootNode", 0 ) );
		FileIDArray.push_back( FileID( 400000, "//RootNode", 0 ) );

		for (int i = 0; i < M->Materials.size(); i++)
		{
			if (M->Materials[i])
				FileIDArray.push_back( FileID( 2100000, M->Materials[i]->Name.c_str(), i ) );
		}

		FileIDArray.push_back( FileID( 2300000, "//RootNode", 0 ) );
		FileIDArray.push_back( FileID( 3300000, "//RootNode", 0 ) );

		FileIDArray.push_back( FileID( 4300000, M->Name.c_str(), 0 ) );

		for (int i = 0; i < FileIDArray.size(); i++)
		{
			std::string Line = FileIDArray[i].GetMetaLine();
			Ret += Line;
		}

		return Ret;
	}
}
std::string UnityExporter::GenerateMeshMeta( UnityMesh* Mesh, bool IsFBX, int TotalLods )
{
	const char* MeshName = Mesh->Name.c_str();
	int SubMeshes = Mesh->Sections.size();

	Mesh->GenerateGUID();

	std::string MetaFileIDs = GenerateMeshMetaFileIDs( Mesh, IsFBX, TotalLods );

	int AnimationType = 0;
	if (Mesh->BoneNames.size() > 0)
		AnimationType = 2;

	int TextLength = 4096 + MetaFileIDs.length();
	char* Text = new char[TextLength];
	snprintf( Text, TextLength,
		"fileFormatVersion: 2\n"
		"guid: %s\n"
		"ModelImporter:\n"
		"  serializedVersion: 23\n"
		"  fileIDToRecycleName:\n"
		"%s"//MetaFileIDs
		"  externalObjects: {}\n"
		"  materials:\n"
		"    importMaterials: 1\n"
		"    materialName: 1\n"
		"    materialSearch: 1\n"
		"    materialLocation: 0\n"
		"  animations:\n"
		"    legacyGenerateAnimations: 4\n"
		"    bakeSimulation: 0\n"
		"    resampleCurves: 1\n"
		"    optimizeGameObjects: 0\n"
		"    motionNodeName: \n"
		"    rigImportErrors: \n"
		"    rigImportWarnings: \n"
		"    animationImportErrors: \n"
		"    animationImportWarnings: \n"
		"    animationRetargetingWarnings: \n"
		"    animationDoRetargetingWarnings: 0\n"
		"    animationCompression: 1\n"
		"    animationRotationError: 0.5\n"
		"    animationPositionError: 0.5\n"
		"    animationScaleError: 0.5\n"
		"    animationWrapMode: 0\n"
		"    extraExposedTransformPaths: []\n"
		"    clipAnimations: []\n"
		"    isReadable: 1\n"
		"  meshes:\n"
		"    lODScreenPercentages: []\n"
		"    globalScale: 1\n"
		"    meshCompression: 0\n"
		"    addColliders: 0\n"
		"    importBlendShapes: 1\n"
		"    swapUVChannels: 0\n"
		"    generateSecondaryUV: 0\n"
		"    useFileUnits: 1\n"
		"    optimizeMeshForGPU: 0\n"
		"    keepQuads: 0\n"
		"    weldVertices: 0\n"
		"    preserveHierarchy: 0\n"
		"    indexFormat : 2\n"
		"    secondaryUVAngleDistortion: 8\n"
		"    secondaryUVAreaDistortion: 15.000001\n"
		"    secondaryUVHardAngle: 88\n"
		"    secondaryUVPackMargin: 4\n"
		"    useFileScale: 1\n"
		"  tangentSpace:\n"
		"    normalSmoothAngle: 60\n"
		"    normalImportMode: 0\n"
		"    tangentImportMode: 0\n"
		"  importAnimation: 1\n"
		"  copyAvatar: 0\n"
		"  humanDescription:\n"
		"    serializedVersion: 2\n"
		"    human: []\n"
		"    skeleton: []\n"
		"    armTwist: 0.5\n"
		"    foreArmTwist: 0.5\n"
		"    upperLegTwist: 0.5\n"
		"    legTwist: 0.5\n"
		"    armStretch: 0.05\n"
		"    legStretch: 0.05\n"
		"    feetSpacing: 0\n"
		"    rootMotionBoneName: \n"
		"    rootMotionBoneRotation: {x: 0, y: 0, z: 0, w: 1}\n"
		"    hasTranslationDoF: 0\n"
		"    hasExtraRoot: 0\n"
		"    skeletonHasParents: 1\n"
		"  lastHumanDescriptionAvatarSource: {instanceID: 0}\n"
		"  animationType: %d\n"
		"  humanoidOversampling: 1\n"
		"  additionalBone: 0\n"
		"  userData: \n"
		"  assetBundleName: \n"
		"  assetBundleVariant:\n"

		, Mesh->GUID.c_str(), MetaFileIDs.c_str(), AnimationType );

	std::string Ret;
	Ret = Text;
	delete[] Text;
	return Ret;
}
std::string  GeneratePrefabMeta( std::string GUID )
{
	char Text[ 2048 ] = "";
	snprintf( Text, sizeof( Text ),
			  "fileFormatVersion: 2\n"
			  "guid: %s\n"
			  "PrefabImporter:\n"
			  "  externalObjects: {}\n"
			  "  userData: \n"
			  "  assetBundleName: \n"
			  "  assetBundleVariant: \n",
			  GUID.c_str()
	);
	return Text;
}
void UnityExporter::WritePrefabs( ExporterScene* Scene, const char* PrefabFolder )
{
	for( int i = 0; i < Scene->Prefabs.size(); i++ )
	{
		GameObject* GO = Scene->Prefabs[ i ];

		const char PrefabHeader[] = {
		"%YAML 1.1\n"
		"%TAG !u! tag:unity3d.com,2011:\n"
		};
		std::string TextData = PrefabHeader;
		TextData += GO->ToSceneData( true );

		std::string File = PrefabFolder;
		std::string NameWide = GO->PrefabName;
		File += NameWide;
		File += ".prefab";

		FILE* f = nullptr;
		f = fopen( File.c_str(), "w" );
		if( f )
		{
			fprintf( f, "%s", TextData.c_str() );
			fclose( f );
		}
		GO->PrefabGUID = GenerateGUIDFromString( File );
		std::string PrefabMetaContent = GeneratePrefabMeta( GO->PrefabGUID );

		std::string MetaFile = PrefabFolder;
		MetaFile += NameWide;
		MetaFile += ".prefab.meta";
		f = fopen( MetaFile.c_str(), "w" );
		if( f )
		{
			fprintf( f, "%s", PrefabMetaContent.c_str() );
			fclose( f );
		}
	}
}
void GetTextureTypeAndIDs( UnityTexture* Tex, int* NumCubes, int* Num2DArrays, int* Num3D, int* NumVirtual, const char** Prefix,
	int* fileID, int* TexIndex )
{
	const char* Tex2D = "Material_Texture2D_";
	int Tex2DFileID = 2800000;
	const char* TexCube = "Material_TextureCube_";
	int TexCubeFileID = 8900000;
	const char* Tex2DArray = "Material_Texture2DArray_";
	int Tex2DArrayFileID = 18700000;
	const char* Tex3D = "Material_VolumeTexture_";
	int Tex3DFileID = 11700000;

	*Prefix = Tex2D;
	*fileID = Tex2DFileID;

	if (Tex && Tex->Shape == UTS_Cube)
	{
		*Prefix = TexCube;
		*fileID = TexCubeFileID;
		*TexIndex = *NumCubes;
		(*NumCubes)++;
	}
	if (Tex && Tex->Shape == UTS_2DArray)
	{
		*Prefix = Tex2DArray;
		*fileID = Tex2DArrayFileID;
		*TexIndex =*Num2DArrays;
		(*Num2DArrays)++;
	}
	if (Tex && Tex->Shape == UTS_3D)
	{
		*Prefix = Tex3D;
		*fileID = Tex3DFileID;
		*TexIndex = *Num3D;
		(*Num3D)++;
	}
	if (Tex && Tex->IsVirtual)
	{
		*Prefix = "Material_VirtualTexturePhysical_";
		*TexIndex = *NumVirtual;
		(*NumVirtual)++;
	}
}
std::string  UnityExporter::GenerateMaterial( UnityMaterial* Material )
{
	std::string Ret;
	if( UnitySettings->Shaders == UnityShadersType::ST_STANDARD_SHADERS )
	{
		char Text[ 2048 ] = "";
		const char* StandardShaderGUID = "0000000000000000f000000000000000";
		int StandardShaderFileID = 46;
		const char* URPLitShaderGUID	= "933532a4fcc9baf4fa0491de14d08ed7";
		int URPFileID = 4800000;
		const char* URPDecalShaderGUID = "9b4e681081e2b4c469111bb649e2f7ee";
		int64 URPDecalFileID = -6465566751694194690;
		const char* HDRPLitShaderGUID	= "6e4ae4064600d784cac1e41a9e6f2e59";
		int HDRPFileID = 4800000;
		const char* UsedShaderGUID = StandardShaderGUID;
		int64 UsedFileID = StandardShaderFileID;
		int type = 0;
		if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
		{
			if( Material->IsDecalMaterial )
			{
				UsedShaderGUID = URPDecalShaderGUID;
				UsedFileID = URPDecalFileID;
			}
			else
			{
				UsedShaderGUID = URPLitShaderGUID;
				UsedFileID = URPFileID;
			}
			type = 3;
		}
		if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
		{
			UsedShaderGUID = HDRPLitShaderGUID;
			UsedFileID = HDRPFileID;
			type = 3;
		}
		char ExtraKeywords[2048] = "  - _EMISSION\n";
		if(!Material->EnableEmission )
		{
			ExtraKeywords[0] = 0;
		}
		int SurfaceType = 0;
		if( Material->BaseColorMultiplier.A < 1.0f )
		{
			strcat( ExtraKeywords, "  - _SURFACE_TYPE_TRANSPARENT\n" );
			SurfaceType = 1;
		}
		snprintf( Text, sizeof( Text ),
				   "%%YAML 1.1\n"
				   "%%TAG !u! tag:unity3d.com,2011:\n"
				   "--- !u!114 &-6605459782340829755\n"
				   "MonoBehaviour:\n"
				   "  m_ObjectHideFlags: 11\n"
				   "  m_CorrespondingSourceObject: {fileID: 0}\n"
				   "  m_PrefabInstance: {fileID: 0}\n"
				   "  m_PrefabAsset: {fileID: 0}\n"
				   "  m_GameObject: {fileID: 0}\n"
				   "  m_Enabled: 1\n"
				   "  m_EditorHideFlags: 0\n"
				   "  m_Script: {fileID: 11500000, guid: d0353a89b1f911e48b9e16bdc9f2e058, type: 3}\n"
				   "  m_Name: \n"
				   "  m_EditorClassIdentifier: \n"
				   "  version: 5\n"
				   "--- !u!21 &2100000\n"
				   "Material:\n"
				   "  serializedVersion: 8\n"
				   "  m_ObjectHideFlags: 0\n"
				   "  m_CorrespondingSourceObject: {fileID: 0}\n"
				   "  m_PrefabInstance: {fileID: 0}\n"
				   "  m_PrefabAsset: {fileID: 0}\n"
				   "  m_Name: %s\n"
				   "  m_Shader: {fileID: %lld, guid: %s, type: %d}\n"
				   "  m_ValidKeywords:\n"
				   "%s"
				   "  - _NORMALMAP\n"
				   "  m_InvalidKeywords: []\n"
				   "  m_LightmapFlags: 4\n"
				   "  m_EnableInstancingVariants: 1\n"
				   "  m_DoubleSidedGI: 0\n"
				   "  m_CustomRenderQueue: -1\n"
				   "  stringTagMap:\n"
				   "    RenderType: Opaque\n"
				   "  disabledShaderPasses: []\n"
				   "  m_SavedProperties:\n"
				   "    serializedVersion: 3\n", Material->Name.c_str(), UsedFileID, UsedShaderGUID, type,
				  ExtraKeywords );
		Ret += Text;

		if( Material->Textures.size() > 0 )
		{
			Ret += "    m_TexEnvs:\n";
			const int MaxSemantics = 11;
			const char* SemanticNames[ MaxSemantics ] = {
				"_BaseMap",
				"_BumpMap",
				"_DetailAlbedoMap",
				"_DetailMask",
				"_DetailNormalMap",
				"_EmissionMap",
				"_MainTex",
				"_MetallicGlossMap",
				"_OcclusionMap",
				"_ParallaxMap",
				"_SpecGlossMap"
			};
			if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP && Material->IsDecalMaterial )
			{
				SemanticNames[ 0 ] = "Base_Map";
				SemanticNames[ 1 ] = "Normal_Map";
			}
			if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_BUILTIN )
			{
				SemanticNames[ 0 ] = "_MainTex";
				SemanticNames[ 6 ] = nullptr;
			}
			if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
			{
				SemanticNames[ 0 ] = "_BaseColorMap";
				SemanticNames[ 1 ] = "_NormalMap";
				SemanticNames[ 2 ] = "_DetailMap";
				SemanticNames[ 5 ] = "_EmissiveColorMap";
				SemanticNames[ 7 ] = "_MaskMap";
			}
			
			for( int i = 0; i < MaxSemantics; i++ )
			{
				if( !SemanticNames[ i ] )
					continue;
				TextureSemantic Semantic = (TextureSemantic)( i + 1 );
				UnityTexture* Tex = Material->GetTexture( Semantic );
				std::string TexGUID = NullGUID;
				if( Tex )
				{
					TexGUID = Tex->GUID;
					if( Semantic == TS_MetallicGlossMap )
					{
						//Makes smoothness take texture value without modifying it
						Material->SmoothnessMultiplier = 1.0f;
					}
				}

				snprintf( Text, sizeof( Text ), "    - %s:\n", SemanticNames [i] );
				Ret += Text;

				if( Tex )
				{
					snprintf( Text, sizeof( Text ), "        m_Texture: {fileID: 2800000, guid: %s, type: 3}\n",
							   TexGUID.c_str() );
				}
				else
					snprintf( Text, sizeof( Text ), "        m_Texture: {fileID: 0}\n" );

				Ret += Text;
				snprintf( Text, sizeof( Text ),
						   "        m_Scale: {x: 1, y: 1}\n"
						   "        m_Offset: {x: 0, y: 0}\n" );

				Ret += Text;
			}
		}
		
		snprintf( Text, sizeof( Text ),
				   "    m_Ints: []\n"
				   "    m_Floats:\n"
				   "    - _DstBlend: 0\n"
				   "    - _SrcBlend: 1\n"
				   "    - _UVSec: 0\n"
				   "    - _ZWrite: 1\n"
				   "    - _Smoothness: %f\n"
				   "    - _AlphaClip: %d\n"
				   "    - _Cutoff: 0.333\n"
				   "    - _Cull: %1f\n"
				   "    - _Surface: %d\n"
				   "    m_Colors:\n"
				   "    - _BaseColor: {r: %f, g: %f, b: %f, a: %f}\n"
				   "    - _Color: {r: %f, g: %f, b: %f, a: %f}\n"
				   "    - _EmissionColor: {r: %f, g: %f, b: %f, a: %f}\n"
				   "    - _SpecColor: {r: 0.5, g : 0.5, b : 0.5, a : 0.5}\n"
				   "  m_BuildTextureStacks: []\n",
				  Material->SmoothnessMultiplier, Material->HasOpacityMask, Material->CullMode, SurfaceType,
				  Material->BaseColorMultiplier.R, Material->BaseColorMultiplier.G, Material->BaseColorMultiplier.B, Material->BaseColorMultiplier.A,
				  Material->BaseColorMultiplier.R, Material->BaseColorMultiplier.G, Material->BaseColorMultiplier.B, Material->BaseColorMultiplier.A,
				  Material->EmissionColor.R, Material->EmissionColor.G, Material->EmissionColor.B, Material->EmissionColor.A
		);

		Ret += Text;
	}
	else
	{
		int serializedVersion = 3;
		char Text[ 2048 ] = "";
		int m_CustomRenderQueue = -1;
		if( Material->IsDecalMaterial )
		{
			m_CustomRenderQueue = 2000 + Material->DecalDrawOrder;
		}
		snprintf( Text, sizeof( Text ),
				   "%%YAML 1.1\n"
				   "%%TAG !u! tag:unity3d.com,2011:\n"
				   "--- !u!21 &2100000\n"
				   "Material:\n"
				   "  serializedVersion: 6\n"
				   "  m_ObjectHideFlags: 0\n"
				   "  m_CorrespondingSourceObject: {fileID: 0}\n"
				   "  m_PrefabInstance: {fileID: 0}\n"
				   "  m_PrefabAsset: {fileID: 0}\n"
				   "  m_Name: %s\n"
				   "  m_Shader: {fileID: 4800000, guid: %s, type: 3}\n"
				   "  m_ShaderKeywords: \n"
				   "  m_LightmapFlags: 4\n"
				   "  m_EnableInstancingVariants: 1\n"
				   "  m_DoubleSidedGI: 0\n"
				   "  m_CustomRenderQueue: %d\n"
				   "  stringTagMap: {}\n"
				   "  disabledShaderPasses: []\n"
				   "  m_SavedProperties:\n"
				   "    serializedVersion: %d\n", Material->Name.c_str(), Material->Shader->GUID.c_str(), m_CustomRenderQueue, serializedVersion );
		Ret += Text;

		if( Material->Textures.size() > 0 )
		{
			Ret += "    m_TexEnvs:\n";
		}
		int NumCubes = 0;
		int Num2DArrays = 0;
		int Num3D = 0;
		int NumVirtual = 0;
		for( int i = 0; i < Material->Textures.size(); i++ )
		{
			UnityTexture* Tex = Material->Textures[ i ];
			std::string TexGUID = NullGUID;
			if( Tex )
				TexGUID = Tex->GUID;

			if( serializedVersion == 3 )//Unity 2017.1
			{
				const char* Prefix = nullptr;
				int fileID = -1;
				int TexIndex = i;
				
				GetTextureTypeAndIDs( Tex, &NumCubes, &Num2DArrays, &Num3D, &NumVirtual, &Prefix, &fileID, &TexIndex );

				snprintf( Text, sizeof( Text ),
						   "    - %s%d:\n"
						   "        m_Texture: {fileID: %d, guid: %s, type: 3}\n"
						   "        m_Scale: {x: 1, y: 1}\n"
						   "        m_Offset: {x: 0, y: 0}\n",
						   Prefix,
						   TexIndex,
						   fileID,
						   TexGUID.c_str() );
			}

			Ret += Text;
		}

		snprintf( Text, sizeof( Text ),
				  "    m_Ints: []\n" );
		Ret += Text;

		snprintf( Text, sizeof( Text ),
				  "    m_Floats:\n" );
		Ret += Text;
		for( int i = 0; i < Material->Parameters.size(); i++ )
		{
			ShaderParameter& Param = Material->Parameters[ i ];
			if( Param.Type == SPT_FLOAT )
			{
				snprintf( Text, sizeof( Text ),
						  "    - %s: %f\n", Param.Name.c_str(), Param.DefaultFloat );
				Ret += Text;
			}
		}
		if( Material->IsDecalMaterial )
		{
			snprintf( Text, sizeof( Text ),
					  "    - _DecalMeshBiasType: 0\n"
					  "    - _DecalMeshDepthBias: 0\n"
					  "    - _DecalMeshViewBias: 0\n"
					  "    - _DrawOrder: %d\n", Material->DecalDrawOrder );
			Ret += Text;
		}

		snprintf( Text, sizeof( Text ),
				  "    - _DstBlend: 0\n"
				  "    - _SrcBlend: 1\n"
				  "    - _UVSec: 0\n"
				  "    - _ZWrite: 1\n" );
		Ret += Text;

		snprintf( Text, sizeof( Text ),
				   "    m_Colors:\n"
				   "    - _Color: {r: 0.8, g: 0.8, b: 0.8, a: 1}\n"
				   "    - _EmissionColor: {r: 0, g: 0, b: 0, a: 1}\n" );
		Ret += Text;
		for( int i = 0; i < Material->Parameters.size(); i++ )
		{
			ShaderParameter& Param = Material->Parameters[ i ];
			if( Param.Type == SPT_VECTOR )
			{
				snprintf( Text, sizeof( Text ),
						  "    - %s: {r: %f, g: %f, b: %f, a: %f}\n", Param.Name.c_str(), Param.DefaultVector.X, Param.DefaultVector.Y, Param.DefaultVector.Z, Param.DefaultVector.W );
				Ret += Text;
			}
		}
	}
	
	return Ret;
}
std::string UnityExporter::GetGUIDFromSizeT( size_t Input )
{
	char Base[] = "abc00000000000000000000000000000";
	char Num[ 32 ];
	#if PLATFORM_WINDOWS
		uint64 Input64 = Input;
	#else
		unsigned long Input64 = Input;
	#endif
	snprintf( Num, sizeof( Num ), "%zu", Input64 );
	int NumLen = strlen( Num );
	std::string GUID = Base;
	for( int i = 32 - NumLen,u = 0; i < 32; i++, u++ )
	{
		GUID[ i ] = Num[ u ];
	}

	return GUID;
}
UnityExporter::UnityExporter()
{
	Type = EngineType::UNITY;
}
UBaseSettingsClass* UnityExporter::NewSettings()
{
	UnitySettings = NewObject<UUnitySettingsClass>();
	Settings = UnitySettings;

	return Settings;
}
FString UnityExporter::GetDefaultBaseOutputFolder()
{
	return TEXT( "UnrealToUnity" );
}
FString UnityExporter::GetPluginName()
{
	return TEXT( "UnrealToUnity" );
}
void UnityExporter::OnExportBegin()
{
	UnitySettings->SaveConfig();
};

#endif