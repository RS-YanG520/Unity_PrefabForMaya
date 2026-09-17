// Copyright Ciprian Stanciu 2024

#include "ExportToUnity.h"

#include "BaseScene.h"
#ifdef ENABLE_GODOT_EXPORT
	#include "GodotScene.h"
#endif
#ifdef ENABLE_UNITY_EXPORT
	#include "UnityScene.h"
	#include "Editor/UnrealEd/Private/FbxExporter.h"
#endif

#include "UTXModule.h"

#include "Runtime/Core/Public/GenericPlatform/GenericPlatformFile.h"
#include "Runtime/Core/Public/HAL/PlatformFileManager.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "ImageUtils.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"
#include "Kismet/KismetStringLibrary.h"

#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Components/BoxReflectionCaptureComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Runtime/Engine/Private/Materials/MaterialUniformExpressions.h"
#include "Materials/MaterialInstance.h"
#include "Engine/TextureCube.h"
#include "Exporters/Exporter.h"
#include "Editor/UnrealEd/Classes/Exporters/TextureCubeExporterHDR.h"
#include "Editor/UnrealEd/Classes/Exporters/TextureExporterHDR.h"
#include "Editor/UnrealEd/Classes/Exporters/SoundExporterOGG.h"
#include "Runtime/Engine/Public/AssetExportTask.h"
#include "UObject/GCObjectScopeGuard.h"
#include "ObjectTools.h"
#include "Runtime/Projects/Public/Interfaces/IPluginManager.h"

#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"

#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/Paths.h"

#include "Materials/MaterialInstanceDynamic.h"

#include "Runtime/Engine/Public/Rendering/SkeletalMeshRenderData.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Misc/MessageDialog.h"
#include "Engine/Blueprint.h"

#include "Engine/LODActor.h"
#include "Editor/UnrealEd/Classes/Editor/GroupActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/VolumeTexture.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Async/Async.h"
#include "Sound/SoundWave.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/RendererSettings.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/PostProcessVolume.h"
#include "Runtime/Landscape/Classes/Landscape.h"
#include "ImageCore.h"
#include "TextureCompressorModule.h"
#include "LandscapeGrassType.h"

#define LOCTEXT_NAMESPACE "FUnrealToXModule"

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
	#include "Engine/Texture2DArray.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
	#include "WorldPartition/HLOD/HLODActor.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	#include "Runtime/Engine/Public/StaticMeshComponentLODInfo.h"

	#include "MaterialDomain.h"
	#include "Materials/MaterialRenderProxy.h"
	#include "Engine/SkinnedAssetCommon.h"
	#include "Animation/BuiltInAttributeTypes.h"
#endif

ERHIFeatureLevel::Type GlobalFeatureLevel = ERHIFeatureLevel::SM5;
EMaterialQualityLevel::Type GlobalQualityLevel = EMaterialQualityLevel::High;
EShaderPlatform GlobalShaderPlatform = EShaderPlatform::SP_PCD3D_SM5;

FString GetMetaExtension()
{
	if( ExporterInterface->Type == EngineType::GODOT )
		return TEXT( ".import" );
	else
		return TEXT( ".meta" );
}
FString GetMeshExtension()
{
	if( ExporterInterface->Type == EngineType::GODOT )
		return TEXT( ".glb" );
	else
		return TEXT( ".fbx" );
}
FString GetMaterialExtension()
{
	if( ExporterInterface->Type == EngineType::GODOT )
		return TEXT( ".tres" );
	else
		return TEXT( ".mat" );
}
FString GetSceneExtension()
{
	if( ExporterInterface->Type == EngineType::GODOT )
		return TEXT( ".tscn" );
	else
		return TEXT( ".unity" );
}
FString GetShaderExtension()
{
	if( ExporterInterface->Type == EngineType::GODOT )
		return TEXT( ".tres" );
	else
		return TEXT( ".shader" );
}

TAutoConsoleVariable<int> CVarUseOriginalPaths(
	TEXT( "utu.UseOriginalPaths" ),
#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
	1,
#else
	0,
#endif
	TEXT( "Use Original Unreal paths just in case there's 2 files with the same name, default is 1" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarUsePostImportTool(
	TEXT( "utu.UsePostImportTool" ),
	1,
	TEXT( "Uses the PostImportTool to fix UV sets and submeshes" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarExportTangents(
	TEXT( "utu.ExportTangents" ),
	1,
	TEXT( "By Tangents are exported. Disable if you have meshes with black parts in unity" ),
	ECVF_Cheat );
bool ReusePrefabs = false;
TAutoConsoleVariable<int> CVarFBXPerActor(
	TEXT( "utu.FBXPerActor" ),
	1,
	TEXT( "Do FBXs per actor instead of OBJs" ),
	ECVF_Cheat );

#if WITH_EDITOR


#include "Editor/MaterialEditor/Public/MaterialEditorUtilities.h"

#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

void ConvertTransform( FTransform Trans, TransformComponent* Transform, bool IsRelative, UDecalComponent* ActorDecal = nullptr );
void ConvertTransformForBones( FTransform Trans, TransformComponent* Transform, bool IsRoot );

#if PLATFORM_WINDOWS

//maybe TCHAR_TO_UTF8 ?

std::wstring ToWideString( std::string str )
{
	std::wstring w;
	for( int i = 0; i < str.length(); i++ )
	{
		w += str[ i ];
	}
	return w;
}
//Should be upgraded to TCHAR_TO_ANSI I guess
std::string ToANSIString( std::wstring w )
{
	std::string s;
	for( int i = 0; i < w.length(); i++ )
	{
		s += w[ i ];
	}
	return s;
}
#else
std::u16string ToWideString( std::string str )
{
	std::u16string w;
	for( int i = 0; i < str.length(); i++ )
	{
		w += str[ i ];
	}
	return w;
}
std::string ToANSIString( std::u16string w )
{
	std::string s;
	for( int i = 0; i < w.length(); i++ )
	{
		s += w[ i ];
	}
	return s;
}

//#define _snscanf_s scanf
#define _ftelli64 ftell
#endif


int TotalMeshes = 0;
int TotalTriangles = 0;

bool ExportMeshes = true;// false;
bool DoAsyncTextureExport = true;
bool ForceShaderRecompilation = false;


std::string FixUnicodeCharacters( std::string & In )
{
	for( int i = 0; i < In.length(); i++ )
	{
		if( In[ i ] == '?' )
		{
			In[ i ] = '_';
		}
	}
	return In;
}
FString FixUnicodeCharacters( FString& In )
{
	for( int i = 0; i < In.Len(); i++ )
	{
		if( In[ i ] == '?' || In[ i ] > 255 )
		{
			In[ i ] = '_';
		}
	}
	return In;
}
int CreateAllDirectories( FString FileName )
{
	int created = 0;
	//detects directories, hierarchly and creates them	
	int len = FileName.Len();
	for( int y = 0; y < len; y++ )
	{
		if( FileName[ y ] == '\\' || FileName[ y ] == '/' )
		{
			FString DirName = UKismetStringLibrary::GetSubstring( FileName, 0, y );

			created += VerifyOrCreateDirectory( DirName );
		}
	}

	return created;
}

ExporterScene* GExporterScene = nullptr;

std::vector< TextureBinding* > AllTextures;

FString GetAssetPathFolder( const UObject* Obj )
{
	FString Path;
	if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
	{
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
		FString AssetPath = Obj->GetPackage()->GetPathName();
		AssetPath = AssetPath.Replace( TEXT( "/Game/" ), TEXT( "" ) );
		//AssetPath = AssetPath.Replace( TEXT( "/" ), TEXT( "\\" ) );
		AssetPath.RemoveFromEnd( Obj->GetName() );
		Path += *AssetPath;
	#endif
	}
	return Path;
}
FString GenerateTexturePath( UTexture *Tex, bool IsResourcePath = false )
{
	UTextureCube* CubeTex = Cast<UTextureCube>( Tex );
	FString TextureFileName;
	if (!IsResourcePath )
		TextureFileName = ExporterInterface->GetAssetsFolder();
	ETextureSourceFormat Format = Tex->Source.GetFormat();

	if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
	{
		TextureFileName += GetAssetPathFolder( Tex );
		if( !IsResourcePath )
			CreateAllDirectories( TextureFileName );
	}
	else
	{
		TextureFileName += TEXT("Textures/" );
	}

	TextureFileName += *Tex->GetName();
	if ( CubeTex )//|| Format == TSF_RGBA16F )
		TextureFileName += TEXT(".hdr" );
	else
		TextureFileName += TEXT(".png" );

	return TextureFileName;
}
FString GenerateAssetPath( UObject* Asset, FString Extension )
{
	FString FileName = ExporterInterface->GetAssetsFolder();

	FileName += GetAssetPathFolder( Asset );
	FileName += *Asset->GetName();
	FileName += Extension;

	return FileName;
}

TArray<UExporter*> Exporters;
UAssetExportTask* ExportTask = nullptr;
bool UseUEExporter( UExporter* ExporterToUse, UObject* ObjectToExport, FString Filename, bool ShowOptions, UObject* OverrideOptions )
{
	bool Status = false;
	
	ExporterToUse->SetBatchMode( false );
	ExporterToUse->SetCancelBatch( false );
	ExporterToUse->SetShowExportOption( ShowOptions );
	ExporterToUse->AddToRoot();
		

	if( !ExportTask )
	{
		ExportTask = NewObject<UAssetExportTask>();
	}
	FGCObjectScopeGuard ExportTaskGuard( ExportTask );
	ExportTask->Object = ObjectToExport;
	ExportTask->Exporter = ExporterToUse;
	ExportTask->Filename = Filename;
	ExportTask->bSelected = false;
	ExportTask->bReplaceIdentical = true;
	ExportTask->bPrompt = false;
	ExportTask->bUseFileArchive = ObjectToExport->IsA( UPackage::StaticClass() );
	ExportTask->bWriteEmptyFiles = false;
	ExportTask->Options = OverrideOptions;

	Status = UExporter::RunAssetExportTask( ExportTask );

	ExporterToUse->SetBatchMode( false );
	ExporterToUse->SetCancelBatch( false );
	ExporterToUse->SetShowExportOption( true );
	ExporterToUse->RemoveFromRoot();

	return Status;
}
UExporter* GetExporterForAsset( UObject* Asset, FString* RequiredName )
{
	if( Exporters.Num() == 0 )
	{
		//Apparently new instances get allocated here
		ObjectTools::AssembleListOfExporters( Exporters );
		for( int i = 0; i < Exporters.Num(); i++ )
		{
			UExporter* Exporter = Exporters[ i ];

			Exporter->AddToRoot();
		}
	}

	for( int i = 0; i < Exporters.Num(); i++ )
	{
		UExporter* Exporter = Exporters[ i ];

		bool HasRequiredName = true;
		if( RequiredName )
		{
			HasRequiredName = Exporter->GetName().Contains( *RequiredName );
		}
		if( Exporter && Exporter->SupportedClass == Asset->GetClass() && HasRequiredName )
		{
			return Exporter;
		}
	}

	return nullptr;
}
void ExportSounds()
{
	TArray<USoundWave*> SoundArray;
	TArray<FString> Messages;
	auto lambdaTextures = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		USoundWave* Sound = Cast<USoundWave>( Asset.GetAsset() );
		if( Sound )
		{
			Sound->AddToRoot();
			SoundArray.Add( Sound );
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "SoundWave" ), lambdaTextures );

	FScopedSlowTask ProgressDialog( SoundArray.Num() );
	ProgressDialog.MakeDialog();

	for(int i=0; i< SoundArray.Num(); i++ )
	{
		USoundWave* Sound = SoundArray[i];
		UExporter* Exporter = GetExporterForAsset( Sound );
		FString Extension = TEXT( ".wav" );
		if( Exporter->GetClass()->GetName().Contains( "OGG" ) )
			Extension = TEXT( ".ogg" );
		FString Filename = GenerateAssetPath( Sound, Extension);
		if( !FPlatformFileManager::Get().GetPlatformFile().FileExists( *Filename ) )
		{
			CreateAllDirectories( Filename );
			UseUEExporter( Exporter, Sound, Filename );
		}
		Sound->RemoveFromRoot();

		ProgressDialog.EnterProgressFrame( 1, LOCTEXT( "ExportingSounds", "Exporting Sounds" ) );
	}
}

bool ExportCubeMap( UTextureCube* Cubemap, FString Filename )
{
	if( Exporters.Num() == 0 )
	{
		//Apparently new instances get allocated here
		ObjectTools::AssembleListOfExporters( Exporters );
	}
	for( int i = 0; i < Exporters.Num(); i++ )
	{
		UExporter* Exporter = Exporters[ i ];
		UTextureCubeExporterHDR* CubeExporter = Cast<UTextureCubeExporterHDR>( Exporter );
		//if (Exporter->GetClass()->GetName().Contains(TEXT("TextureExporterDDS")))		
		if( CubeExporter )
		{
			if( DoAsyncTextureExport )
			{
				AsyncTask( ENamedThreads::GameThread, [=]()
				{
					UseUEExporter( Exporter, Cubemap, Filename );
				} );
				return true;
			}
			else
				return UseUEExporter( Exporter, Cubemap, Filename );
		}
	}

	return false;
}
void ExportTextureHDR( UTexture2D* Tex, FString Filename )
{
	if( Exporters.Num() == 0 )
	{
		//Apparently new instances get allocated here
		ObjectTools::AssembleListOfExporters( Exporters );
	}
	for( int i = 0; i < Exporters.Num(); i++ )
	{
		if ( Exporters[ i ]->GetName().Contains(TEXT("TextureExporterHDR")))
		{
			if( DoAsyncTextureExport )
			{
				AsyncTask( ENamedThreads::GameThread, [=]()
				{
					UseUEExporter( Exporters[ i ], Tex, Filename );
				} );
			}
			else
			{
				UseUEExporter( Exporters[ i ], Tex, Filename );
			}
			return;
		}
	}
}

void UETextureExporter::ConvertToColors( TArray64<uint8>& SourceData, ETextureSourceFormat Format, int32 Width, int32 Height, int32 Depth, TArray<FColor>& InputImageData, FString TextureFileName )
{
	if( SourceData.Num() == 0 )
	{
		FString DialogText = FString::Printf( TEXT(
			"Error for %s !\n"
			"\n"
			"ConvertToColors -> SourceData.Num() == 0\n" ), *TextureFileName );
		FText Txt = FText::FromString( DialogText );
		FMessageDialog::Open( EAppMsgType::Ok, Txt );
		return;
	}

	if( Format == TSF_G8 )
	{
		for( int i = 0; i < SourceData.Num(); i++ )
		{
			FColor Color( SourceData[ i ], SourceData[ i ], SourceData[ i ], SourceData[ i ] );
			InputImageData.Add( Color );
		}
	}
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
	else if( Format == TSF_G16 )
	{
		const uint16* Pixels = (const uint16*)SourceData.GetData();
		int NumPixels = Width * Height;
		for( int i = 0; i < NumPixels; i++ )
		{
			const float Val = float( Pixels[ i ] ) / 65535.0f;
			//uint16 R16 = OutMipData[ i + 1 ] * 256 + OutMipData[ i + 0 ];
			//uint8 Val = (uint8)( float( R16 ) / 256.0f );
			FLinearColor Color( Val, Val, Val, 1.0 );
			FColor Color2 = Color.ToFColor( false );
			//FColor Color( Val, Val, Val, Val );
			InputImageData.Add( Color2 );
		}
	}//These textures must not be compressed inside unity because we can get artifacts
	#endif
	else if( Format == TSF_RGBA16F )
	{
		//Maybe Bring back this function ?
		//ExportTextureHDR( Tex2D, TextureFileName );

		FFloat16Color* Pixels = (FFloat16Color*)SourceData.GetData();
		int NumPixels = Width * Height;
		for( int i = 0; i < NumPixels; i++ )
		{
			FLinearColor Color( Pixels[ i ] );
			FColor C = Color.ToFColor( true );
			InputImageData.Add( C );
		}
	}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1	
	else if (Format == TSF_RGBA32F)
	{
		FLinearColor* Pixels = (FLinearColor*)SourceData.GetData();
		int NumPixels = Width * Height;
		for (int i = 0; i < NumPixels; i++)
		{
			FLinearColor Color = Pixels[i];
			FColor C = Color.ToFColor( true );
			InputImageData.Add( C );
		}
	}
#endif
	else if( Format == TSF_RGBA16 )
	{
		for( int i = 0; i < SourceData.Num(); i += 8 )
		{
			FColor Color;
			uint16 R16 = SourceData[ i + 0 ] * 255 + SourceData[ i + 1 ];
			uint16 G16 = SourceData[ i + 2 ] * 255 + SourceData[ i + 3 ];
			uint16 B16 = SourceData[ i + 4 ] * 255 + SourceData[ i + 5 ];
			uint16 A16 = SourceData[ i + 6 ] * 255 + SourceData[ i + 7 ];
			
			Color.B = SourceData[ i + 5 ];
			Color.G = SourceData[ i + 3 ];
			Color.R = SourceData[ i + 1 ];
			Color.A = SourceData[ i + 7 ];
			InputImageData.Add( Color );
		}
	}
	else
	{
		auto AddPixel = [&]( int x, int y, int z )
		{
			int i = ( z * Width * Height + y * Width + x ) * 4;
			if( i + 3 < SourceData.Num() )
			{
				FColor Color( SourceData[ i + 2 ], SourceData[ i + 1 ], SourceData[ i + 0 ], SourceData[ i + 3 ] );
				InputImageData.Add( Color );
			}
		};
		bool UpsideDown = false;// true;
		if( UpsideDown )
		{
			for( int y = Height - 1; y >= 0; y-- )
			{
				for( int x = 0; x < Width; x++ )
				{
					AddPixel( x, y, 0 );
				}
			}
		}
		else
		{
			for( int z = 0; z < Depth; z++ )
			{
				for( int y = 0; y < Height; y++ )
				{
					for( int x = 0; x < Width; x++ )
					{
						AddPixel( x, y, z );
					}
				}
			}
		}
	}
}
#if ENGINE_MAJOR_VERSION == 4
void PNGCompressImageArray( int32 ImageWidth, int32 ImageHeight, const TArray<FColor>& SrcData, TArray<uint8>& DstData )
#else
void PNGCompressImageArray( int32 ImageWidth, int32 ImageHeight, const TArray<FColor>& SrcData, TArray64<uint8>& DstData )
#endif
{
	#if ENGINE_MAJOR_VERSION == 4
		FImageUtils::CompressImageArray(
			ImageWidth,
			ImageHeight,
			SrcData,
			DstData
		);
	#else
		FImageUtils::PNGCompressImageArray(
			ImageWidth,
			ImageHeight,
			SrcData,
			DstData
		);
	#endif
}
bool UETextureExporter::CompressAndSaveTexture( FString TextureFileName, TArray<FColor>& InputImageData, int Width, int Height )
{
	#if ENGINE_MAJOR_VERSION == 4
		TArray<uint8> PNG_Compressed_ImageData;
	#else
		TArray64<uint8> PNG_Compressed_ImageData;
	#endif
	//~~~~~~~~~~~~~~~~
	// Compress to PNG
	//~~~~~~~~~~~~~~~~
	if( InputImageData.Num() == 0 )
	{
		FString DialogText = FString::Printf( TEXT(
			"Error!\n\n"
			"Could not save texture %s !\n"
			"\n"
			"InputImageData.Num() == 0 Tex->Source.GetMipData probably failed ?\n"), *TextureFileName );
		FText Txt = FText::FromString( DialogText );
		FMessageDialog::Open( EAppMsgType::Ok, Txt );
		return false;
	}

	PNGCompressImageArray( Width, Height, InputImageData, PNG_Compressed_ImageData );

	bool Status = FFileHelper::SaveArrayToFile(
		PNG_Compressed_ImageData,
		*TextureFileName
		);

	if( !Status )
	{
		FString DialogText = FString::Printf( TEXT(
			"Error!\n\n"
			"Could not save texture %s !\n"
			"\n"
			"Do you have enough free space on that drive ?\n"
			"Do you have permission to save files there ?\n" ), *TextureFileName );
		if( TextureFileName.Len() > 255 )
		{
			DialogText += TEXT( "FileName.Len() > 255 !!" );
		}
		FText Txt = FText::FromString( DialogText );
		FMessageDialog::Open( EAppMsgType::Ok, Txt );
	}
	return Status;
}
bool NeedAdjustImageColors( const FTextureBuildSettings& InBuildSettings )
{
	const FColorAdjustmentParameters& InParams = InBuildSettings.ColorAdjustment;

	return
		!FMath::IsNearlyEqual( InParams.AdjustBrightness, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustBrightnessCurve, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustSaturation, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustVibrance, 0.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustRGBCurve, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustHue, 0.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustMinAlpha, 0.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustMaxAlpha, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		InBuildSettings.bChromaKeyTexture;
}
bool UETextureExporter::DoExport( TextureBinding* Binding )
{
	UTexture* Tex = Binding->UnrealTexture;
	UTextureCube* CubeTex = Cast<UTextureCube>( Tex );
	UVolumeTexture* VolTex = Cast< UVolumeTexture>( Tex );
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
		UTexture2DArray* Tex2DArray = Cast< UTexture2DArray>( Tex );
	#endif
	UTextureRenderTarget2D* RT2D = Cast<UTextureRenderTarget2D>( Tex );
	FString TextureFileName = GenerateTexturePath( Tex );
		
	if( !ExporterInterface->Settings->ExportTextures )
		return false;
	
	FString FileNameOnDisk = FPlatformFileManager::Get().GetPlatformFile().GetFilenameOnDisk( *TextureFileName );
	//Don't save duplicates !
	if ( !FPlatformFileManager::Get().GetPlatformFile().FileExists( *TextureFileName ) )
	{
		if( CubeTex )
		{
			return ExportCubeMap( CubeTex, TextureFileName );
		}
		if( VolTex )
		{
			#if ENGINE_MAJOR_VERSION == 4
				Tex = VolTex->Source2DTexture;
			#else
				Tex = VolTex->Source2DTexture.Get();
			#endif

			if( !Tex )
			{
				FString DialogText = FString::Printf( TEXT( "UETextureExporter::DoExport(%s) VolumeTexture Source is null !" ), *VolTex->GetName() );
				FText Txt = FText::FromString( DialogText );
				FMessageDialog::Open( EAppMsgType::Ok, Txt );

				return false;
			}
		}
		#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
		if( Tex2DArray )
		{
			TArray<FColor> FinalImageColors;
			//TArray<TArray<FColor>*> SliceImageDataArray;
			for( int i = 0; i < Tex2DArray->SourceTextures.Num(); i++ )
			{
				UTexture2D* SourceTex = Tex2DArray->SourceTextures[ i ];
				if( !SourceTex )
					continue;
				int32 Width = SourceTex->Source.GetSizeX();
				int32 Height = SourceTex->Source.GetSizeY();
				int32 Depth = SourceTex->Source.GetNumSlices();
				ETextureSourceFormat Format = SourceTex->Source.GetFormat();

				TArray64<uint8> SliceData;
				SourceTex->Source.GetMipData( SliceData, 0 );

				//TArray<FColor>* SliceColors = new TArray<FColor>;
				TArray<FColor> SliceColors;
				ConvertToColors( SliceData, Format, Width, Height, Depth, SliceColors, TextureFileName );
					
				for( int c = 0; c < SliceColors.Num(); c++ )
				{
					FinalImageColors.Add( SliceColors[ c ] );
				}
			}

			int32 Width = Tex->Source.GetSizeX();
			int32 Height = Tex->Source.GetSizeY() * Tex2DArray->SourceTextures.Num();
			return CompressAndSaveTexture( TextureFileName, FinalImageColors, Width, Height );
		}
		#endif
		if( RT2D )
		{
			int32 Width = RT2D->SizeX;
			int32 Height = RT2D->SizeY;
			return CompressAndSaveTexture( TextureFileName, Binding->RenderTargetData, Width, Height );
		}
		ETextureSourceFormat Format = Tex->Source.GetFormat();

		#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			#define TSF_RGBA8 TSF_RGBA8_DEPRECATED
		#endif

		if ( Format != TSF_BGRA8 && Format != TSF_RGBA8 && Format != TSF_RGBA16 && Format != TSF_G8 &&
				Format != TSF_BGRE8 &&
				Format != TSF_RGBA16F
				#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
					&& Format != TSF_G16 
				#endif
				#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
					&& Format != TSF_RGBA32F
				#endif
			 )
		{
			//UE_LOG( LogClass, Log, TEXT("UETextureExporter::DoExport Format %d is not standard!"), Format );
			FString DialogText = FString::Printf( TEXT( "UETextureExporter::DoExport(%s) Format %d is not supported!" ), *Tex->GetName(), Format );
			FText Txt = FText::FromString( DialogText );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );

			return false;
		}

		int32 Width = Tex->Source.GetSizeX();
		int32 Height = Tex->Source.GetSizeY();
		int32 Depth = Tex->Source.GetNumSlices();

		TArray64<uint8> OutMipData;
		TArray<FColor> InputImageData;
		int NumBlocks = Tex->Source.GetNumBlocks();
		if( NumBlocks > 1 )//UDIM textures, usually virtual as well
		{
			TArray<TArray<FColor>*> BlocksColors;
			for( int b = 0; b < NumBlocks; b++ )
			{
				TArray64<uint8> BlockMipData;
				Tex->Source.GetMipData( BlockMipData, b, 0, 0 );
				TArray<FColor>* ColorArray = new TArray<FColor>;
				ConvertToColors( BlockMipData, Format, Width, Height, Depth, *ColorArray, TextureFileName );
				BlocksColors.Add( ColorArray );
			}
			
			for( int y = 0; y < Height; y++ )
			{
				for( int b = 0; b < NumBlocks; b++ )
				{
					for( int x = 0; x < Width; x++ )
					{
						TArray<FColor>* BlockImage = BlocksColors[ b ];
						InputImageData.Add( (*BlockImage)[y * Width + x ] );
					}
				}
			}

			Width = Width * NumBlocks;
		}
		else
			Tex->Source.GetMipData( OutMipData, 0 );

	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FTextureBuildSettings BuildSettings;
		FColorAdjustmentParameters& ColorAdjustment = BuildSettings.ColorAdjustment;
		ColorAdjustment.AdjustBrightness = Tex->AdjustBrightness;
		ColorAdjustment.AdjustBrightnessCurve = Tex->AdjustBrightnessCurve;
		ColorAdjustment.AdjustSaturation = Tex->AdjustSaturation;
		ColorAdjustment.AdjustVibrance = Tex->AdjustVibrance;
		ColorAdjustment.AdjustRGBCurve = Tex->AdjustRGBCurve;
		ColorAdjustment.AdjustHue	   = Tex->AdjustHue;
		ColorAdjustment.AdjustMinAlpha = Tex->AdjustMinAlpha;
		ColorAdjustment.AdjustMaxAlpha = Tex->AdjustMaxAlpha;

		FImage ImageBGRA8;
		if( NeedAdjustImageColors( BuildSettings ) )
		{
			FImage OutImage;
			bool ImageFetched = Tex->Source.GetMipImage( OutImage, 0 );
			FImage ImageF32;
			OutImage.CopyTo( ImageF32, ERawImageFormat::RGBA32F, EGammaSpace::Linear );

			ITextureCompressorModule::AdjustImageColors( ImageF32, BuildSettings );

			ImageF32.CopyTo( ImageBGRA8, ERawImageFormat::BGRA8, OutImage.GammaSpace );
			InputImageData = ImageBGRA8.AsBGRA8();
		}
		else if ( NumBlocks == 1 )
	#endif
			ConvertToColors( OutMipData, Format, Width, Height, Depth, InputImageData, TextureFileName );

		if( Tex->bFlipGreenChannel)
		{
			for( int i = 0; i < InputImageData.Num(); i++ )
			{
				FColor& Color = InputImageData[ i ];
				Color.G = 255 - Color.G;
			}
		}

		return CompressAndSaveTexture( TextureFileName, InputImageData, Width, Height );
	}
	else//File exists, so already exported ?
		return true;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//~~~ Empty PNG Buffer ~~~
	//PNG_Compressed_ImageData.Empty();
	//~~~~~~~~~~~~~~~~~~~~~~~~

	//~~~ Empty Color Buffer ~~~
	//PNGScreenShot_ColorBuffer.Empty();
	//~~~~~~~~~~~~~~~~~~~~~~~~
}
TextureBinding* UETextureExporter::Find( UTexture *Tex )
{
	for( int i = 0; i < AllTextures.size(); i++ )
	{
		auto Binding = AllTextures[ i ];
		if( Binding->UnrealTexture == Tex )
			return Binding;
	}

	return nullptr;
}
TextureBinding* UETextureExporter::Export( UTexture* Tex )
{
	auto Existent = Find( Tex );
	if( Existent )
		return Existent;

	TextureBinding *NewTextureBinding = new TextureBinding;
	NewTextureBinding->UnrealTexture = Tex;
	AllTextures.push_back( NewTextureBinding );

	return NewTextureBinding;
}

FExportTextureTask::FExportTextureTask( TextureBinding* Binding )
{
	this->Binding = Binding;
}
const TCHAR* FExportTextureTask::GetTaskName()
{
	return TEXT( "FExportTextureTask" );
}
FORCEINLINE TStatId FExportTextureTask::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT( FExportTextureTask, STATGROUP_TaskGraphTasks );
}

ENamedThreads::Type FExportTextureTask::GetDesiredThread()
{
	return ENamedThreads::AnyBackgroundThreadNormalTask;
}

ESubsequentsMode::Type FExportTextureTask::GetSubsequentsMode()
{
	return ESubsequentsMode::TrackSubsequents;
}
void FExportTextureTask::DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent )
{
	bool Status = UETextureExporter::DoExport( Binding );
}

std::vector< MaterialBinding* > AllMaterials;
//std::vector< UnityShader* > AllShaders;

MaterialBinding *GetMaterialBinding( UMaterialInterface *M )
{
	for ( int i = 0; i < AllMaterials.size(); i++ )
	{
		MaterialBinding* Binding = AllMaterials[ i ];
		if ( Binding->MaterialInterface == M )
			return Binding;
	}

	return nullptr;
}
MaterialBinding *ProcessMaterialReference( UMaterialInterface *M )
{
	MaterialBinding *Binding = GetMaterialBinding( M );

	if ( !Binding )
	{
		Binding = new MaterialBinding();
		Binding->MaterialInterface = M;
		Binding->BaseMaterial = M->GetMaterial();
		Binding->ID = AllMaterials.size();
		//Prevent crashes when exporting multiple scenes
		if( ReusePrefabs )
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>( M );
			if( MID )
			{
				FName UniqueName = MakeUniqueObjectName( GetTransientPackage(), MID->GetClass(), *MID->GetName() );
				MID->Rename( *UniqueName.ToString(), GetTransientPackage() );
			}
			M->AddToRoot();
		}
		AllMaterials.push_back( Binding );
	}
	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings && UnitySettings->WithMaterialParameters() )
	{
		//Make sure BaseMaterial is in the material array
		UMaterial* BaseMaterial = M->GetBaseMaterial();
		MaterialBinding* BaseMaterialBinding = GetMaterialBinding( BaseMaterial );
		if( !BaseMaterialBinding )
			ProcessMaterialReference( BaseMaterial );
	}
	
	return Binding;
}
FString GetProjectName()
{
	FString ProjectFilePath = FPaths::GetProjectFilePath();
	int PointStart = ProjectFilePath.Find( TEXT( "." ), ESearchCase::IgnoreCase, ESearchDir::FromEnd );
	int NameStart = ProjectFilePath.Find( TEXT( "/" ), ESearchCase::IgnoreCase, ESearchDir::FromEnd );
	FString ProjectName = ProjectFilePath.Mid( NameStart + 1, PointStart - NameStart - 1 );
	return ProjectName;
}
FString GetResourceDir()
{
	FString PluginName = ExporterInterface->GetPluginName();
	TSharedPtr<IPlugin> ThisPlugin = IPluginManager::Get().FindPlugin( PluginName );
	
	FString ResourceDir = ThisPlugin->GetBaseDir() / TEXT( "Resources/" );	

	return ResourceDir;
}
void ShowWarningIfFileIsMissing( FString SourceFile )
{
	FString ResourceDir = GetResourceDir();
	FString SourceFilePath = ResourceDir;
	SourceFilePath += SourceFile;

	if( !FPlatformFileManager::Get().GetPlatformFile().FileExists( *SourceFilePath ) )
	{
		FString Text = FString::Printf( TEXT( "SourceFile %s not found. Check your plugin installation !" ), *SourceFilePath );
		FText Txt = FText::FromString( Text );
		FMessageDialog::Open( EAppMsgType::Ok, Txt );
	}
}
void CopyToOutput( FString SourceFile, FString OutFilePath, bool Overwrite )
{
	FString ResourceDir = GetResourceDir();
	FString SourceFilePath = ResourceDir;
	SourceFilePath += SourceFile;
	FString To = ExporterInterface->Settings->OutputDirectory;
	To += OutFilePath;

	if( !Overwrite )
	{
		if( FPlatformFileManager::Get().GetPlatformFile().FileExists( *To ) )
			return;
	}

	bool Result = FPlatformFileManager::Get().GetPlatformFile().CopyFile( *To, *SourceFilePath );
	if( !Result )
	{
		if( !FPlatformFileManager::Get().GetPlatformFile().FileExists( *SourceFilePath ) )
		{
			FString Text = FString::Printf( TEXT( "SourceFile %s not found. Check your plugin installation !" ), *SourceFilePath );
			FText Txt = FText::FromString( Text );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );
		}
		else
		{
			FString Text = FString::Printf( TEXT( "CopyToOutput Error on SourceFile=%s OutFilePath=%s" ), *SourceFilePath, *To );
			FText Txt = FText::FromString( Text );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );
		}
	}
}
bool DoesOutputFileExists( FString OutFilePath )
{
	FString Target = ExporterInterface->Settings->OutputDirectory;
	Target += OutFilePath;
	bool Exists = FPlatformFileManager::Get().GetPlatformFile().FileExists( *Target );
	return Exists;
}
bool WarnAboutRenderPipelineMismatch( RenderPipeline CurrentRenderPipeline)
{
	if( DoesOutputFileExists( TEXT( "/Assets/Settings/ForwardRenderer.asset" ) ) )
	{
		if( CurrentRenderPipeline != RenderPipeline::RP_URP )
		{
			FString Text = FString::Printf( TEXT( "Output folder contains files for a URP project ! Aborting export" ) );
			FText Txt = FText::FromString( Text );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );
			return true;
		}
	}
	else if( DoesOutputFileExists( TEXT( "/Assets/HDRPDefaultResources/DefaultHDRPAsset.asset" ) ) )
	{
		if( CurrentRenderPipeline != RenderPipeline::RP_HDRP )
		{
			FString Text = FString::Printf( TEXT( "Output folder contains files for a HDRP project ! Aborting export" ) );
			FText Txt = FText::FromString( Text );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );
			return true;
		}
	}
	else if( DoesOutputFileExists( TEXT( "ProjectSettings/ProjectSettings.asset" ) ) )
	{
		if( CurrentRenderPipeline != RenderPipeline::RP_BUILTIN )
		{
			FString Text = FString::Printf( TEXT( "Output folder contains files for a BuiltIn project ! Aborting export" ) );
			FText Txt = FText::FromString( Text );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );
			return true;
		}
	}

	return false;
}
void ProcessMaterial_RenderThread( MaterialBinding *Mat, const char *ProxyShaderString, const char* GraphShaderString );
void ProcessAllMaterials( )
{
	FString ResourceDir = GetResourceDir();
	FString ProxyShader;
	FString GraphShader = ResourceDir;

	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );

	if( UnitySettings && ( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP  ||
						   UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP ))
	{
		ProxyShader = TEXT("/ProxyURP_HDRP.hlsl" );
	}
	else
	{
		ProxyShader = TEXT( "/Proxy.shader" );
	}
	
	if( ExporterInterface->Type == EngineType::UNITY )
	{
		ShowWarningIfFileIsMissing( ProxyShader );
	}
	ProxyShader = ResourceDir + ProxyShader;

	const char *ProxyShaderString = nullptr;
	FString DecalGraphShader;
	FString FullscreenGraphShader;
	const char* GraphShaderData = nullptr;
	const char* DecalGraphShaderData = nullptr;
	const char* FullscreenShaderData = nullptr;	

	if( ExporterInterface->Type == EngineType::UNITY )
	{
		int Size = LoadFile( ToANSIString( *ProxyShader ).c_str(), (uint8**)&ProxyShaderString );
		if( Size < 0 )
		{
			FString Text = FString::Printf( TEXT( "ProxyShader %s not found. Check your plugin installation !" ), *ProxyShader );
			FText Txt = FText::FromString( Text );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );
			return;
		}		
		if( UnitySettings && UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
		{
			DecalGraphShader = GraphShader;
			FullscreenGraphShader = GraphShader;
			GraphShader += TEXT( "URP/URPGraphShader.shader" );
			Size = LoadFile( ToANSIString( *GraphShader ).c_str(), (uint8**)&GraphShaderData );

			DecalGraphShader += TEXT( "URP/URPDecalGraphShader.shader" );
			Size = LoadFile( ToANSIString( *DecalGraphShader ).c_str(), (uint8**)&DecalGraphShaderData );
			
			FullscreenGraphShader += TEXT( "URP/URPFullscreenShader.shader" );
			Size = LoadFile( ToANSIString( *FullscreenGraphShader ).c_str(), (uint8**)&FullscreenShaderData );
		}
		else if( UnitySettings && UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
		{
			DecalGraphShader = GraphShader;
			GraphShader += TEXT( "HDRP/HDRPGraphShader.shader" );
			Size = LoadFile( ToANSIString( *GraphShader ).c_str(), (uint8**)&GraphShaderData );

			DecalGraphShader += TEXT( "HDRP/HDRPDecalGraphShader.shader" );
			Size = LoadFile( ToANSIString( *DecalGraphShader ).c_str(), (uint8**)&DecalGraphShaderData );
		}
	}
	
	//double StartTime = FPlatformTime::Seconds();
	//int PrevPrintSeconds = -1;

	FScopedSlowTask MaterialsProgress( AllMaterials.size() );
	MaterialsProgress.MakeDialog();

	for ( int i = 0; i < AllMaterials.size(); i++ )
	{
		MaterialBinding* Mat = AllMaterials[ i ];
		const char* SelectedGraphShaderData = GraphShaderData;
		if( Mat->BaseMaterial->MaterialDomain == MD_DeferredDecal )
		{
			Mat->UnityMat->IsDecalMaterial = true;
			SelectedGraphShaderData = DecalGraphShaderData;
		}
		else if( Mat->BaseMaterial->MaterialDomain == MD_PostProcess &&
				 (UnitySettings && UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP ))//only URP for now
		{
			SelectedGraphShaderData = FullscreenShaderData;
		}
		ProcessMaterial_RenderThread( Mat, ProxyShaderString, SelectedGraphShaderData );

		//double NowTime = FPlatformTime::Seconds();
		//double ElapsedTime = NowTime - StartTime;
		//int ElapsedTimeInSeconds = (int)ElapsedTime;
		////if( ElapsedTimeInSeconds > PrevPrintSeconds )
		////{
		////	PrevPrintSeconds = ElapsedTimeInSeconds;
			FString Text = FString::Printf( TEXT( "Processing Materials ( %d / %d )" ), i + 1, AllMaterials.size() );
			FText StrTxt = FText::FromString( Text );
			MaterialsProgress.EnterProgressFrame( 1.0f, StrTxt );
			UE_LOG( LogTemp, Log, TEXT( "%s" ), *Text );
		//}
	}
	
	if ( GraphShaderData )
		delete[]GraphShaderData;
	if ( DecalGraphShaderData )
		delete[]DecalGraphShaderData;
	if( FullscreenShaderData )
		delete[]FullscreenShaderData;
}
FString GetOutputFile( FString SubFolder, FString Name, FString Ext )
{
	FString OutFolder = ExporterInterface->GetAssetsFolder();
	OutFolder += *SubFolder;
	OutFolder += *Name;
	OutFolder += Ext;

	return OutFolder;
}
class FUniformExpressionSet_Override : public FUniformExpressionSet
{
public:
	
	TMemoryImageArray<FGuid> GetParameterCollections()
	{
		return this->ParameterCollections;
	}

	#if ENGINE_MAJOR_VERSION >= 5
	TMemoryImageArray<FMaterialNumericParameterInfo>& GetUniformNumericParameters()
	{
		return this->UniformNumericParameters;
	}
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& GetUniformPreshaders()
	{
		return this->UniformPreshaders;
	}
	#if ENGINE_MINOR_VERSION >= 1
	TMemoryImageArray<FMaterialUniformPreshaderField>& GetUniformPreshaderFields()
	{
		return this->UniformPreshaderFields;
	}	
	#endif
	UE::Shader::FPreshaderData& GetUniformPreshaderData()
	{
		return UniformPreshaderData;
	}
	#elif ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26
	TMemoryImageArray<FMaterialScalarParameterInfo>& GetUniformScalarParameters()
	{		
		return this->UniformScalarParameters;
	}
	TMemoryImageArray<FMaterialVectorParameterInfo>& GetUniformVectorParameters()
	{		
		return this->UniformVectorParameters;
	}
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& GetVectorPreshaders()
	{
		return this->UniformVectorPreshaders;
	}
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& GetScalarPreshaders()
	{
		return this->UniformScalarPreshaders;
	}
	FMaterialPreshaderData& GetUniformPreshaderData()
	{
		return UniformPreshaderData;
	}
	#elif ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 20
	TArray<TRefCountPtr<FMaterialUniformExpression>>& GetUniformVectorExpressions()
	{
		return UniformVectorExpressions;
	}
	TArray<TRefCountPtr<FMaterialUniformExpression>>& GetUniformScalarExpressions()
	{
		return UniformScalarExpressions;
	}
	TArray<TRefCountPtr<FMaterialUniformExpressionTexture>>& GetUniform2DTextureExpressions()
	{
		return Uniform2DTextureExpressions;
	}
	TArray<TRefCountPtr<FMaterialUniformExpressionTexture>>& GetUniformCubeTextureExpressions()
	{
		return UniformCubeTextureExpressions;
	}
	TArray<TRefCountPtr<FMaterialUniformExpressionTexture>>& GetUniformVolumeTextureExpressions()
	{
		return UniformVolumeTextureExpressions;
	}
	#endif
};

class ShaderProperty
{
public:
	std::string Name;
	int ScalarIndex = -1;
	int VectorIndex = -1;
	//int BufferOffset= -1;
};
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26
using FPreshaderStack = TArray<FLinearColor, TInlineAllocator<64u>>;
void EvaluatePreshader2( const FUniformExpressionSet* UniformExpressionSet, const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data, FLinearColor& OutValue,
						 uint16& LastVectorIndex, uint16& LastScalarIndex, int& NumOpCodes );
void GetPropertyNames( FUniformExpressionSet_Override* Set_Override, FMaterialRenderContext MaterialRenderContext, std::vector< ShaderProperty>& Properties, uint8* TempBuffer, int TempBufferSize )
{
	void* BufferCursor = TempBuffer;
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& VectorPreshaders = Set_Override->GetVectorPreshaders();
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& ScalarPreshaders = Set_Override->GetScalarPreshaders();
	TMemoryImageArray<FMaterialScalarParameterInfo>& UniformScalarParameters = Set_Override->GetUniformScalarParameters();
	TMemoryImageArray<FMaterialVectorParameterInfo>& UniformVectorParameters = Set_Override->GetUniformVectorParameters();

	FPreshaderStack PreshaderStack;
	FPreshaderDataContext PreshaderBaseContext( Set_Override->GetUniformPreshaderData() );
	for( int32 VectorIndex = 0; VectorIndex < VectorPreshaders.Num(); ++VectorIndex )
	{
		FLinearColor VectorValue( 0, 0, 0, 0 );

		const FMaterialUniformPreshaderHeader& Preshader = VectorPreshaders[ VectorIndex ];
		FPreshaderDataContext PreshaderContext( PreshaderBaseContext, Preshader );
		uint16 LastVectorIndex;
		uint16 LastScalarIndex;
		int NumOpCodes;
		EvaluatePreshader2( Set_Override, MaterialRenderContext, PreshaderStack, PreshaderContext, VectorValue, LastVectorIndex, LastScalarIndex, NumOpCodes );
		if( NumOpCodes == 1 && LastVectorIndex != (uint16)-1 )
		{
			ShaderProperty NewShaderProperty;
			const FMaterialVectorParameterInfo& Parameter = Set_Override->GetVectorParameter( LastVectorIndex );
			FString UStr = Parameter.ParameterInfo.Name.ToString();
			NewShaderProperty.Name = ToANSIString( *UStr );
			NewShaderProperty.VectorIndex = VectorIndex;// LastVectorIndex;
			//NewShaderProperty.BufferOffset = VectorIndex;
			Properties.push_back( NewShaderProperty );
		}

		FLinearColor* DestAddress = (FLinearColor*)BufferCursor;
		*DestAddress = VectorValue;
		BufferCursor = DestAddress + 1;
		check( BufferCursor <= TempBuffer + TempBufferSize );
	}

	// Dump scalar expression into the buffer.
	for( int32 ScalarIndex = 0; ScalarIndex < ScalarPreshaders.Num(); ++ScalarIndex )
	{
		FLinearColor VectorValue( 0, 0, 0, 0 );

		const FMaterialUniformPreshaderHeader& Preshader = ScalarPreshaders[ ScalarIndex ];
		FPreshaderDataContext PreshaderContext( PreshaderBaseContext, Preshader );
		uint16 LastVectorIndex;
		uint16 LastScalarIndex;
		int NumOpCodes;
		EvaluatePreshader2( Set_Override, MaterialRenderContext, PreshaderStack, PreshaderContext, VectorValue, LastVectorIndex, LastScalarIndex, NumOpCodes );
		if( NumOpCodes == 1 && LastScalarIndex != (uint16)-1 )
		{
			ShaderProperty NewShaderProperty;
			const FMaterialScalarParameterInfo& Parameter = Set_Override->GetScalarParameter( LastScalarIndex );
			FString UStr = Parameter.ParameterInfo.Name.ToString();
			NewShaderProperty.Name = ToANSIString( *UStr );
			NewShaderProperty.ScalarIndex = ScalarIndex;// LastScalarIndex;
			//NewShaderProperty.BufferOffset = ScalarIndex;
			Properties.push_back( NewShaderProperty );
		}
		float* DestAddress = (float*)BufferCursor;
		*DestAddress = VectorValue.R;
		BufferCursor = DestAddress + 1;
		check( BufferCursor <= TempBuffer + TempBufferSize );
	}
}
#endif
std::string GetMaterialParameterConfig( std::string ShaderSource )
{
	//Should make sure r.MaterialEditor.PreshaderDumpToHLSL is true ?

	std::string Ret;
	int pos = -1;
	int startpos = 0;
	do
	{
		pos = ShaderSource.find( "// PreshaderBuffer[", startpos );
		if( pos == -1 )
			break;

		std::string Line = GetLine( ShaderSource, pos );

		int VectorIndex = -1;
		char SwizzleVector[ 128 ] = "";
		int ParamsRead = sscanf( Line.c_str(), "// PreshaderBuffer[%d].%s =", &VectorIndex, SwizzleVector );
		if( ParamsRead != 2 )
			return Ret;

		//if( strstr( Line.c_str(), "Append" ) )
		//{
		//	int VectorSize = strlen( SwizzleVector );
		//	char FloatVecType[ 128 ];
		//	snprintf( FloatVecType, sizeof( FloatVecType ), "float%d", VectorSize );
		//
		//	StringReplacement( Line, "Append", FloatVecType );
		//}
		if( strstr( Line.c_str(), "TEX[\"" ) )
		{
			Line.insert( 0, "//" );
		}

		Ret += Line;
		Ret += ";\n";
		
		startpos = pos + 1;
	} while( pos != -1 );

	return Ret;
}
FLinearColor RemoveNans( FLinearColor Value )
{
	if( FMath::IsNaN( Value.R ) )
		Value.R = 0.0f;
	if( FMath::IsNaN( Value.G ) )
		Value.G = 0.0f;
	if( FMath::IsNaN( Value.B ) )
		Value.B = 0.0f;
	if( FMath::IsNaN( Value.A ) )
		Value.A = 0.0f;

	return Value;
}
FVector RemoveNans( FVector Value )
{
	if( FMath::IsNaN( Value.X ) )
		Value.X = 0.0f;
	if( FMath::IsNaN( Value.Y ) )
		Value.Y = 0.0f;
	if( FMath::IsNaN( Value.Z ) )
		Value.Z = 0.0f;

	return Value;
}
bool HasNans( FVector Value )
{
	if( FMath::IsNaN( Value.X ) || !FMath::IsFinite( Value.X ) )
		return true;
	if( FMath::IsNaN( Value.Y ) || !FMath::IsFinite( Value.Y ) )
		return true;
	if( FMath::IsNaN( Value.Z ) || !FMath::IsFinite( Value.Z ) )
		return true;

	return false;
}
std::string GetPropertyName( int Vector, int Scalar, const std::vector< ShaderProperty > & Properties )
{
	for( int i = 0; i < Properties.size(); i++ )
	{
		if( Vector != -1 && Vector == Properties[ i ].VectorIndex )
		{
			return Properties[ i ].Name;
		}
		if( Scalar != -1 && Scalar == Properties[ i ].ScalarIndex )
		{
			return Properties[ i ].Name;
		}
	}

	return "(Unknown)";
}
FMaterialResource* GetMaterialResource( UMaterialInterface* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
	return Material->GetMaterialResource( GlobalShaderPlatform );
#else
	return Material->GetMaterialResource( GlobalFeatureLevel, GlobalQualityLevel );
#endif
}
FUniformExpressionSet_Override* GetUniformExpressionSet_Override( FMaterialResource* MaterialResource )
{
	if (!MaterialResource)
	{
		UE_LOG( LogTemp, Error, TEXT( "ERROR! !Mat || !Mat->MaterialResource" ) );
		GLog->Flush();
		return nullptr;
	}

	const FMaterialShaderMap* ShaderMapToUse = MaterialResource->GetRenderingThreadShaderMap();

	if (!ShaderMapToUse)
	{
		UE_LOG( LogTemp, Error, TEXT( "ERROR! !ShaderMapToUse" ) );
		GLog->Flush();
		return nullptr;
	}

	const FUniformExpressionSet* Set = &ShaderMapToUse->GetUniformExpressionSet();
	if (!Set)
	{
		UE_LOG( LogTemp, Error, TEXT( "ERROR! !GetUniformExpressionSet" ) );
		GLog->Flush();
		return nullptr;
	}
	FUniformExpressionSet_Override* Set_Override = (FUniformExpressionSet_Override*)Set;
	return Set_Override;
}
FUniformExpressionSet_Override* GetUniformExpressionSet_Override( MaterialBinding* Mat )
{
	FMaterialResource* MaterialResource = GetMaterialResource( Mat->MaterialInterface );
	return GetUniformExpressionSet_Override( MaterialResource );
}
#if ENGINE_MAJOR_VERSION >= 5
TArray<FMaterialNumericParameterInfo> GetUniqueNumericParameters( TMemoryImageArray<FMaterialNumericParameterInfo>& NumericParameters )
{
	TArray<FMaterialNumericParameterInfo> UniqueParameters;
	for (int i = 0; i < NumericParameters.Num(); i++)
	{
		FMaterialNumericParameterInfo& Param = NumericParameters[i];
		FString ParamName = Param.ParameterInfo.Name.ToString();
		FMaterialNumericParameterInfo* Found = UniqueParameters.FindByPredicate( [&]( const FMaterialNumericParameterInfo& Other ) { return Other.ParameterInfo.Name.ToString().Equals( ParamName ); } );
		if (!Found)
		{
			UniqueParameters.Add( Param );
		}
	}

	return UniqueParameters;
}
#endif
void SanitizeParameterConfig( MaterialBinding* Mat, std::string& ParameterConfig )
{
	if( ParameterConfig.length() == 0 )
		return;

	FUniformExpressionSet_Override* Set_Override = GetUniformExpressionSet_Override( Mat );
	if( Set_Override )
	{
		#if ENGINE_MAJOR_VERSION >= 5
		TMemoryImageArray<FMaterialNumericParameterInfo>& NumericParameters = Set_Override->GetUniformNumericParameters();
		//TArray<FMaterialNumericParameterInfo> UniqueNumericParameters = GetUniqueNumericParameters( NumericParameters );
		for( int i = 0; i < NumericParameters.Num(); i++ )
		{
			FMaterialNumericParameterInfo& Param = NumericParameters[ i ];
			FString ParamName = Param.ParameterInfo.Name.ToString();
			FString SanitizedParamName = ParamName;
			SanitizeParamName( SanitizedParamName );
			if( SanitizedParamName.Compare( ParamName ) != 0 )
			{
				//Some Layerparameters are added twice !
				if (Mat->SanitizedParamNames.Find( ParamName ))
				{
					SanitizedParamName = Mat->SanitizedParamNames[ParamName];
				}
				else
					Mat->SanitizedParamNames.Add( MakeTuple( ParamName, SanitizedParamName ) );
			}

			std::string ParamNameANSI = ToANSIString( *ParamName );
			std::string SanitizedParamNameANSI = ToANSIString( *SanitizedParamName );
			std::string ReplacementWhat = "\"";
			ReplacementWhat += ParamNameANSI;
			ReplacementWhat += "\"";
			std::string ReplacementWith = "\"";
			ReplacementWith += SanitizedParamNameANSI;
			ReplacementWith += "\"";
			StringReplacement( ParameterConfig, ReplacementWhat.c_str(), ReplacementWith.c_str() );
		}
		#endif
	}
}
void FillUniformBuffer( uint8** TempBuffer, uint32* TempBufferSize, MaterialBinding* Mat )
{
#if ENGINE_MAJOR_VERSION >= 5
	FUniformExpressionSet_Override* Set_Override = GetUniformExpressionSet_Override( Mat );
	if( !Set_Override )
		return;

	FMaterialResource* MaterialResource = GetMaterialResource( Mat->MaterialInterface );
	const FMaterialShaderMap* ShaderMapToUse = MaterialResource->GetRenderingThreadShaderMap();
	FMaterialRenderProxy* RenderProxy = Mat->RenderProxy;
	auto& Cache = RenderProxy->UniformExpressionCache[ GlobalFeatureLevel ];
	FMaterialRenderContext DummyContext( RenderProxy, *MaterialResource, nullptr );

	const FRHIUniformBufferLayout* UniformBufferLayout = ShaderMapToUse->GetUniformBufferLayout();
	*TempBufferSize = UniformBufferLayout->ConstantBufferSize;
	//if( UniformBufferLayout->Resources.Num() > 0 )
	//{
	//	int Offset = UniformBufferLayout->Resources[ UniformBufferLayout->Resources.Num() - 1 ].MemberOffset;
	//	FMath::Max( TempBufferSize, Offset + BaseSize );
	//}
	*TempBuffer = new uint8[ *TempBufferSize ];
	//Prevent nans from appearing in ScalarExpressions values
	float* FloatArray = (float*)(*TempBuffer);
	for( uint32 i = 0; i < *TempBufferSize / 4; i++ )
	{
		FloatArray[ i ] = 0.0f;
	}
	Set_Override->FillUniformBuffer( DummyContext, Cache, UniformBufferLayout, *TempBuffer, *TempBufferSize );
#endif
}

std::string GenerateInitializeExpressions( MaterialBinding *Mat, int & NumVectorExpressions, int & NumScalarExpressions, std::string ParameterConfig )
{
	std::string DataString;

	FUniformExpressionSet_Override* Set_Override = GetUniformExpressionSet_Override( Mat );
	if( !Set_Override )
		return "";

	FMaterialResource* MaterialResource = GetMaterialResource( Mat->MaterialInterface );
	const FMaterialShaderMap* ShaderMapToUse = MaterialResource->GetRenderingThreadShaderMap();

	#if ENGINE_MAJOR_VERSION >= 5
		TMemoryImageArray<FMaterialUniformPreshaderHeader>& Preshaders = Set_Override->GetUniformPreshaders();	
		#if ENGINE_MINOR_VERSION >= 1
		TMemoryImageArray<FMaterialUniformPreshaderField>& PreshaderFields = Set_Override->GetUniformPreshaderFields();
		if( PreshaderFields.Num() > 0 )
		{
			FMaterialUniformPreshaderField Field = PreshaderFields[ PreshaderFields.Num() - 1 ];
			int BufferOffset = Field.BufferOffset;
			int NumFloats = ( (int)Field.Type - (int)UE::Shader::EValueType::Float1) + 1;
			int Size = NumFloats * sizeof( float );
			int TotalBufferSize = BufferOffset * sizeof(float) + Size;

			int Div = TotalBufferSize / ( sizeof( float ) * 4 );
			int Mod = TotalBufferSize % ( sizeof( float ) * 4 );
			NumVectorExpressions = Div;
			if( Mod > 0 )
				NumVectorExpressions++;
		}
		#else
			NumVectorExpressions = Preshaders.Num();
		#endif
		NumScalarExpressions = 0;
	#elif ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26
		TMemoryImageArray<FMaterialUniformPreshaderHeader>& VectorPreshaders = Set_Override->GetVectorPreshaders();
		TMemoryImageArray<FMaterialUniformPreshaderHeader>& ScalarPreshaders = Set_Override->GetScalarPreshaders();

		NumVectorExpressions = VectorPreshaders.Num();
		NumScalarExpressions = ScalarPreshaders.Num();
	#endif

	int NumScalarVectors = ( NumScalarExpressions + 3 ) / 4;

	int ExtraSpaceForResources = 300;
	int BaseSize = ( NumVectorExpressions + NumScalarExpressions ) * 16;
	uint32 TempBufferSize = BaseSize + ExtraSpaceForResources;

	FMaterialRenderProxy* RenderProxy = Mat->RenderProxy;
	if( !RenderProxy )
	{
		if( Mat->MaterialInterface )
		{
			FString MessageText = FString::Printf( TEXT(
				"Error in %s"
				"!RenderProxy"
			), *Mat->MaterialInterface->GetName() );
			FText Txt = FText::FromString( MessageText );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );
		}
		return "";
	}
	auto& Cache = RenderProxy->UniformExpressionCache[ GlobalFeatureLevel ];
	static bool AllocatedVTsEarlyOut = false;
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
	if( Cache.AllocatedVTs.Num() > 0 && AllocatedVTsEarlyOut )
	{
		if( Mat->MaterialInterface )
		{
			FString MessageText = FString::Printf( TEXT(
				"Error in %s\n"
				"Cache.AllocatedVTs.Num() > 0 !\n\n"
			), *Mat->MaterialInterface->GetName() );
			FText Txt = FText::FromString( MessageText );
			FMessageDialog::Open( EAppMsgType::Ok, Txt );
		}
		//Fix for crash in FillUniformBuffer
		return "";
	}
	#endif
	FMaterialRenderContext DummyContext( RenderProxy, *MaterialResource, nullptr );

	std::vector< ShaderProperty > Properties;

	#if ENGINE_MAJOR_VERSION >= 5
		uint8* TempBuffer = nullptr;
		FillUniformBuffer( &TempBuffer, &TempBufferSize, Mat );
	#elif (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26)
		uint8* TempBuffer = new uint8[ TempBufferSize ];
		//Prevent nans from appearing in ScalarExpressions values
		float* FloatArray = (float*)TempBuffer;
		for( uint32 i = 0; i < TempBufferSize / 4; i++ )
		{
			FloatArray[ i ] = 0.0f;
		}
		Set_Override->FillUniformBuffer( DummyContext, Cache, TempBuffer, TempBufferSize );
	#else
		uint8* TempBuffer = new uint8[ TempBufferSize ];
	#endif
	#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26
		GetPropertyNames( Set_Override, DummyContext, Properties, TempBuffer, TempBufferSize );
	#endif

	char ValueStr[ 256 ];
	snprintf( ValueStr, sizeof( ValueStr ),
			  "void InitializeExpressions()\r\n"
			  "{\r\n" );// , VectorExpressions.Num(), ScalarExpressions.Num() );
	DataString += ValueStr;

	int SizePerVTStack = 32;
	int SizePerVirtualTextureUsed = sizeof( FUintVector4 );
	int NumVirtualTextures = 0;
	int TotalOffset = 0;
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
		NumVirtualTextures = Set_Override->GetNumTextures( EMaterialTextureParameterType::Virtual );
		TotalOffset = Cache.AllocatedVTs.Num() * SizePerVTStack + NumVirtualTextures * SizePerVirtualTextureUsed;
	#endif
	uint8* BufferCursor = TempBuffer + TotalOffset;
	
	for( int i = 0; i < NumVectorExpressions; i++ )
	{
		FLinearColor OutValue = *(FLinearColor*)BufferCursor;
		if( BufferCursor + sizeof( FLinearColor ) < TempBuffer + TempBufferSize )
		{
			BufferCursor += sizeof( FLinearColor );
		}
		OutValue = RemoveNans( OutValue );
		std::string Name = GetPropertyName( i, -1, Properties );
	#if ENGINE_MAJOR_VERSION >= 5
		snprintf( ValueStr, sizeof( ValueStr), "\tMaterial.PreshaderBuffer[%d] = float4(%f,%f,%f,%f);//%s\r\n", i, OutValue.R, OutValue.G, OutValue.B, OutValue.A, Name.c_str());
	#else
		snprintf( ValueStr, sizeof( ValueStr ), "\tMaterial.VectorExpressions[%d] = float4(%f,%f,%f,%f);//%s\r\n", i, OutValue.R, OutValue.G, OutValue.B, OutValue.A, Name.c_str() );
	#endif
		DataString += ValueStr;
	}

	for( int i = 0; i < NumScalarVectors; i++ )
	{
		FLinearColor ScalarValues = *(FLinearColor*)BufferCursor;
		BufferCursor += sizeof( FLinearColor );
		ScalarValues = RemoveNans( ScalarValues );
		std::string Name1 = GetPropertyName( -1, i * 4 + 0, Properties );
		std::string Name2 = GetPropertyName( -1, i * 4 + 1, Properties );
		std::string Name3 = GetPropertyName( -1, i * 4 + 2, Properties );
		std::string Name4 = GetPropertyName( -1, i * 4 + 3, Properties );
		snprintf( ValueStr, sizeof( ValueStr ), "\tMaterial.ScalarExpressions[%d] = float4(%f,%f,%f,%f);//%s %s %s %s\r\n", i,
					ScalarValues.R, ScalarValues.G, ScalarValues.B, ScalarValues.A, Name1.c_str(), Name2.c_str(), Name3.c_str(), Name4.c_str() );
		DataString += ValueStr;

		ensure( BufferCursor <= TempBuffer + TempBufferSize );
	}

	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings && UnitySettings->WithMaterialParameters() && ParameterConfig.length() > 0 )
	{
		//ParameterConfig		
		StringReplacement( ParameterConfig, "// PreshaderBuffer", "\tMaterial.PreshaderBuffer" );
		StringReplacement( ParameterConfig, "Param[\"", "" );
		StringReplacement( ParameterConfig, "\"]", "");
		StringReplacement( ParameterConfig, "Normalize(", "normalize(" );
		StringReplacement( ParameterConfig, "Max(", "max(" );
		StringReplacement( ParameterConfig, "Rcp(", "rcp(" );
		DataString += "\n";
		DataString += ParameterConfig;
	}

	DataString += "}\r\n";
	delete[] TempBuffer;
	
	return DataString;	
}

std::string AddNumberInString( std::string OriginalString, const char *Prefix, int Number )
{
	int PrefixOffset = OriginalString.find( Prefix );
	int NumberStart = PrefixOffset + strlen( Prefix );

	char NumberStr[ 32 ];
	snprintf( NumberStr, sizeof( NumberStr ), "%d", Number );
	//Erases the existing "1"
	OriginalString.replace( NumberStart, 1, "" );
	OriginalString.insert( NumberStart, NumberStr );
	return OriginalString;
}
int StringReplacement( std::string & OriginalString, const char *What, const char* Replace, int StartPos)
{
	int WhatLen = strlen( What );
	int ReplaceLen = strlen( Replace );

	int pos = StartPos;
	int Replacements = 0;
	pos = OriginalString.find( What, StartPos );
	while( pos != -1 )
	{
		OriginalString.replace( pos, WhatLen, Replace );
		pos = OriginalString.find( What, pos + ReplaceLen );
		Replacements++;
	}

	return Replacements;
}
int DetectMaxVariableSize( std::string GeneratedShader, std::string VariableName )
{
	int Offset = 0;
	int Val = 0;
	int MaxValue = -1;
	int pos = 0;
	while( pos != - 1 )
	{
		pos = GeneratedShader.find( VariableName, Offset );
		if( pos == -1 )
		{
			break;
		}
		else
		{
			int IndexEndPos = GeneratedShader.find( "]", pos );
			int IndexStartPos = pos + VariableName.length() + 1;
			std::string StrIndex = GeneratedShader.substr( IndexStartPos, IndexEndPos - IndexStartPos );
			if( StrIndex.find( "*" ) != -1 )
			{
				int Operands[ 3 ] = { 0 };
				int NumOperands = sscanf( StrIndex.c_str(), "%d*%d+%d", &Operands[ 0 ], &Operands[ 1 ], &Operands[ 2 ] );
				if ( NumOperands >= 2 )
				{
					if( NumOperands == 2 )
						Val = Operands[ 0 ] * Operands[ 1 ];
					else
						Val = Operands[ 0 ] * Operands[ 1 ] + Operands[ 2 ];

					if( MaxValue == -1 )
						MaxValue = Val;
					else
						MaxValue = FMath::Max( MaxValue, Val );
				}
			}
			else if( sscanf( StrIndex.c_str(), "%d", &Val ) == 1 )
			{
				if( MaxValue == -1 )
					MaxValue = Val;
				else
					MaxValue = FMath::Max( MaxValue, Val );
			}
			Offset = pos + 1;
		}
	}
	if( MaxValue == -1 )
		MaxValue = 0;
	return MaxValue;
}
const int MaxTextures = 128;
void DetectNumTexturesUsed( std::string GeneratedShader, int* Actual2DTexturesUsed, int* ActualCubeTexturesUsed,
							int* ActualTextureArraysUsed, int* ActualCubeArraysUsed, int* Actual3DTexturesUsed, int* ActualPhysicalTexturesUsed,
							const char* BeginMarker, const char* EndMarker)
{
	int Offset = 0;
	if ( BeginMarker )
		Offset = GeneratedShader.find( BeginMarker );
	int End = GeneratedShader.length();
	if ( EndMarker )
		End = GeneratedShader.find( EndMarker );

	for( int i = 0; i < MaxTextures; i++ )
	{
		char Tex2DRef[ 256 ];
		snprintf( Tex2DRef, sizeof( Tex2DRef ), "Material_Texture2D_%d", i );
		int pos = GeneratedShader.find( Tex2DRef, Offset );
		if( pos != -1 && pos < End )
		{
			Actual2DTexturesUsed[ i ] = 1;
			continue;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char TexCubeRef[ 256 ];
		snprintf( TexCubeRef, sizeof( TexCubeRef ), "Material_TextureCube_%d", i );
		int pos = GeneratedShader.find( TexCubeRef, Offset );
		if( pos != -1 && pos < End )
		{
			ActualCubeTexturesUsed[ i ] = 1;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char Tex2DRef[ 256 ];
		snprintf( Tex2DRef, sizeof( Tex2DRef ), "Material_Texture2DArray_%d", i );
		int pos = GeneratedShader.find( Tex2DRef, Offset );
		if( pos != -1 && pos < End )
		{
			ActualTextureArraysUsed[ i ] = 1;
			continue;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char TexCubeRef[ 256 ];
		snprintf( TexCubeRef, sizeof( TexCubeRef ), "Material_TextureCubeArray_%d", i );
		int pos = GeneratedShader.find( TexCubeRef, Offset );
		if( pos != -1 && pos < End )
		{
			ActualCubeArraysUsed[ i ] = 1;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char Tex3DRef[ 256 ];
		snprintf( Tex3DRef, sizeof( Tex3DRef ), "Material_VolumeTexture_%d", i );
		int pos = GeneratedShader.find( Tex3DRef, Offset );
		if( pos != -1 && pos < End )
		{
			Actual3DTexturesUsed[ i ] = 1;
			continue;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char PhysicalTexRef[ 256 ];
		snprintf( PhysicalTexRef, sizeof( PhysicalTexRef ), "Material_VirtualTexturePhysical_%d", i );
		int pos = GeneratedShader.find( PhysicalTexRef, Offset );
		if( pos != -1 && pos < End )
		{
			ActualPhysicalTexturesUsed[ i ] = 1;
			continue;
		}
	}
}
void AddTexProperty( int& Cursor, std::string* TargetString, int i, int ParamIndex, MaterialBinding* Mat, const char* Format1, const char* Format2 )
{
	char Line[ 1024 ];
	snprintf( Line, sizeof( Line ), Format1, i );
	FString TexParamName( Line );
	if( ParamIndex < Mat->TextureParamNames.Num() )
		TexParamName = Mat->TextureParamNames[ ParamIndex ];
	std::string TexParamNameCStr = ToANSIString( *TexParamName );

	snprintf( Line, sizeof( Line ), Format2, i, TexParamNameCStr.c_str() );
	int LineLen = strlen( Line );
	TargetString->insert( Cursor, Line );
	Cursor += LineLen;
}
int InvalidCharOccurances = 0;
FString MakePureAnsi( const FString& Input )
{
	FString Output;
	for (TCHAR Char : Input)
	{
		if ( Char <= 0x7F )
			Output.AppendChar( Char );
		else
		{
			Output.AppendChar( '_' );
			Output.AppendInt( InvalidCharOccurances );
			InvalidCharOccurances++;
		}
	}
	return Output;
}
void SanitizeParamName( FString& OriginalName )
{
	ESearchCase::Type SearchCase = ESearchCase::CaseSensitive;
	const char* ShaderKeywords[] = {
		#ifdef ENABLE_GODOT_EXPORT
			"ALBEDO",
			"ALPHA",
			"METALLIC",
			"ROUGHNESS",
			"SPECULAR",
			"EMISSION",
			"AO",
			"AO_LIGHT_AFFECT",
			"NORMAL",
			"NORMAL_MAP",
			"NORMAL_MAP_DEPTH",
			"RIM",
			"RIM_TINT",
			"CLEARCOAT",
			"CLEARCOAT_ROUGHNESS",
			"ANISOTROPY",
			"ANISOTROPY_FLOW",
			"SSS_STRENGTH",
			"BACKLIGHT",
			"ALPHA_SCISSOR_THRESHOLD",
			"ALPHA_HASH_SCALE",
			"ALPHA_ANTIALIASING_EDGE",
			"ALPHA_TEXTURE_COORDINATE",
			"DEPTH",
			"E",//for some reason it says it's a redefinition
			"UV",
			"UV2",
			"TANGENT",
			"BINORMAL",
			"COLOR",
			"VIEW",
			"FRAGCOORD",
			"TIME",
			"EXPOSURE",
			"EYE_OFFSET",
			"CAMERA_POSITION_WORLD",
			"CAMERA_DIRECTION_WORLD",
			"smooth",
			"int",
			"float",
			"double",
		#endif
		#ifdef ENABLE_UNITY_EXPORT
			"uniform",
			"Color",
			"Vector",
			"Diffuse",
			"Specular",			
			"Lighten",
			"offset",
			"Offset",
			"min",
			"max",
			"Fog",
			"Desaturate",
			"Gradient",
			"BlendNormal",
			"Min",
			"Max",
			"ConstantColor",
			"Shininess",
			"Ambient",
			"Multiply",
			"Int",
			"M",
			"Length",
			"DecalColor",
			"colorMask",
			"Add",
			"Light",
			"exp",
			"one",
			"emission"
		#endif
	};
	#ifdef ENABLE_UNITY_EXPORT
		SearchCase = ESearchCase::IgnoreCase;//unity hates Color,color,COLOR and who knows what other combinations
	#endif
	int NumKeywords = sizeof( ShaderKeywords ) / sizeof( char* );
	for( int i = 0; i < NumKeywords; i++ )
	{
		FString Comp = ToWideString( ShaderKeywords[ i ] ).c_str();
		if( OriginalName.Compare( Comp, SearchCase ) == 0 )
		{
			OriginalName += TEXT( "_" );
			OriginalName.AppendInt( InvalidCharOccurances );
			InvalidCharOccurances++;
		}
	}
	
	bool IsPureAnsi = FCString::IsPureAnsi( *OriginalName );
	if (!IsPureAnsi)
	{
		OriginalName = MakePureAnsi( OriginalName );
	}
	std::string Str = ToANSIString( *OriginalName );

	//Param names can't start with a figure
	if( Str[ 0 ] >= '0' && Str[ 0 ] <= '9' )
	{
		OriginalName = TEXT( "UTG_" ) + OriginalName;
		Str = ToANSIString( *OriginalName );
	}

	char InvalidCharacters[] = "~`!@#$%^&*()+-=[]{}|\\;:\'\"<>/?,.\t\n ";
	int InvalidCharsLen = strlen( InvalidCharacters );
	for( int i = 0; i < Str.length(); i++ )
	{
		for( int c = 0; c < InvalidCharsLen; c++ )
		{
			if( Str[ i ] == InvalidCharacters[ c ] )
			{
				Str[ i ] = '_';
				//Add a number so if I have 2 variables !! and ?? they don't both get to become __
				//Str += InvalidCharOccurances;
				//InvalidCharOccurances++;
			}
		}
	}

	OriginalName = ToWideString( Str ).c_str();
}
int GetParameterOffset( std::string ParamName, std::string ParameterConfig )
{
	char SearchStr[ 1024 ] = "";
	snprintf( SearchStr, sizeof(SearchStr), "\"%s\"", ParamName.c_str() );
	int pos = ParameterConfig.find( SearchStr );
	if( pos == -1 )
		return -1;

	std::string Line = GetLine( ParameterConfig, pos );
	if( Line.length() == 0 )
		return -1;

	int VectorIndex = -1;
	char SwizzleVector[ 128 ] = "";
	int ParamsRead = sscanf( Line.c_str(), "// PreshaderBuffer[%d].%s =", &VectorIndex, SwizzleVector );
	if( ParamsRead != 2 )
		return -1;

	int SwizzleOffset = 0;
	if( SwizzleVector[ 0 ] == 'x' )
		SwizzleOffset = 0;
	else if( SwizzleVector[ 0 ] == 'y' )
		SwizzleOffset = sizeof(float);
	else if( SwizzleVector[ 0 ] == 'z' )
		SwizzleOffset = sizeof( float ) * 2;
	else if( SwizzleVector[ 0 ] == 'w' )
		SwizzleOffset = sizeof( float ) * 3;

	int FinalOffset = VectorIndex * ( sizeof( float ) * 4 ) + SwizzleOffset;
	
	return FinalOffset;
}
#if ENGINE_MAJOR_VERSION >= 5
void AddNumericProperty( int& Cursor, std::string* TargetString, FMaterialNumericParameterInfo& ParamInfo, MaterialBinding* Mat )
{
	char Line[ 1024 ] = { 0 };
	UMaterialInterface* MaterialInterface = Mat->MaterialInterface;
	FString ParamName = ParamInfo.ParameterInfo.Name.ToString();
	FString SanitizedParamName = ParamName;
	if( Mat->SanitizedParamNames.Find( ParamName ) )
	{
		SanitizedParamName = Mat->SanitizedParamNames[ ParamName ];
	}

	std::string ParamNameANSI = ToANSIString( *SanitizedParamName );
	
	bool Success = false;
	if( ParamInfo.ParameterType == EMaterialParameterType::Vector )
	{
		FLinearColor Val(0,0,0,0);
		const char* UnityParamType = "Vector";
		if ( ParamName.Contains(TEXT("Color")) || ParamName.Contains( TEXT( "Colour" )))
			UnityParamType = "Color";
		Success = MaterialInterface->GetVectorParameterValue( ParamInfo.ParameterInfo, Val );

		ShaderParameter NewShaderParameter;
		NewShaderParameter.Name = ToANSIString( *SanitizedParamName );
		NewShaderParameter.Type = SPT_VECTOR;
		NewShaderParameter.DefaultVector = FVector4( Val );
		Mat->UnityMat->Parameters.push_back( NewShaderParameter );

		snprintf( Line, sizeof( Line ), "\t\t%s( \"%s\", %s ) = (%f, %f, %f, %f )\n", ParamNameANSI.c_str(), ParamNameANSI.c_str(), UnityParamType, Val.R, Val.G, Val.B, Val.A );
	}
	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	else if( ParamInfo.ParameterType == EMaterialParameterType::DoubleVector )
	{
		FVector4d VectorDouble( 0, 0, 0, 0 );
		const char* UnityParamType = "Vector";
		if( ParamName.Contains( TEXT( "Color" ) ) || ParamName.Contains( TEXT( "Colour" ) ) )
			UnityParamType = "Color";
		Success = MaterialInterface->GetDoubleVectorParameterValue( ParamInfo.ParameterInfo, VectorDouble );

		ShaderParameter NewShaderParameter;
		NewShaderParameter.Name = ToANSIString( *SanitizedParamName );
		NewShaderParameter.Type = SPT_VECTOR;
		NewShaderParameter.DefaultVector = FVector4( VectorDouble.X, VectorDouble.Y, VectorDouble.Z, VectorDouble.W );
		Mat->UnityMat->Parameters.push_back( NewShaderParameter );

		snprintf( Line, sizeof( Line ), "\t\t%s( \"%s\", %s ) = (%f, %f, %f, %f )\n", ParamNameANSI.c_str(), ParamNameANSI.c_str(), UnityParamType, VectorDouble.X, VectorDouble.Y, VectorDouble.Z, VectorDouble.W );
	}
	#endif
	else if( ParamInfo.ParameterType == EMaterialParameterType::Scalar )
	{
		float Val = -1;
		Success = MaterialInterface->GetScalarParameterValue( ParamInfo.ParameterInfo, Val );

		ShaderParameter NewShaderParameter;
		NewShaderParameter.Name = ToANSIString( *SanitizedParamName );
		NewShaderParameter.Type = SPT_FLOAT;
		NewShaderParameter.DefaultFloat = Val;
		Mat->UnityMat->Parameters.push_back( NewShaderParameter );

		snprintf( Line, sizeof( Line ), "\t\t%s( \"%s\", Float ) = %f\n", ParamNameANSI.c_str(), ParamNameANSI.c_str(), Val );
	}
	else if( ParamInfo.ParameterType == EMaterialParameterType::StaticSwitch )
	{
		//Silent ignore
		Success = true;
	}
	
	if( !Success && ParamInfo.ParameterInfo.Name != FName(TEXT("SelectionColor") ) )
	{
		UE_LOG( LogTemp, Error, TEXT( "[UTX] ERROR! GetParameterValue %s failed !" ), *ParamInfo.ParameterInfo.Name.ToString() );
	}
	int LineLen = strlen( Line );
	if( LineLen > 0 )
	{
		TargetString->insert( Cursor, Line );
		Cursor += LineLen;
	}
}
void AddNumericPropertyDecalaration( int& Cursor, std::string* TargetString, FMaterialNumericParameterInfo& ParamInfo, MaterialBinding* Mat )
{
	char Line[ 1024 ] = { 0 };
	FString ParamName = ParamInfo.ParameterInfo.Name.ToString();
	FString SanitizedParamName = ParamName;
	if( Mat->SanitizedParamNames.Find( ParamName ) )
	{
		SanitizedParamName = Mat->SanitizedParamNames[ ParamName ];
	}

	std::string ParamNameANSI = ToANSIString( *SanitizedParamName );

	if( ParamInfo.ParameterType == EMaterialParameterType::Vector ||
		ParamInfo.ParameterType == EMaterialParameterType::DoubleVector )
	{
		snprintf( Line, sizeof( Line ), "uniform float4 %s;\n", ParamNameANSI.c_str() );
	}
	else if( ParamInfo.ParameterType == EMaterialParameterType::Scalar )
	{
		snprintf( Line, sizeof( Line ), "uniform float %s;\n", ParamNameANSI.c_str() );
	}
	else if( ParamInfo.ParameterType == EMaterialParameterType::StaticSwitch )
	{
		//Silent ignore
	}

	int LineLen = strlen( Line );
	if( LineLen > 0 )
	{
		TargetString->insert( Cursor, Line );
		Cursor += LineLen;
	}
}
#endif
void AddTexDecalaration( int& Cursor, std::string* TargetString, int i, std::string& VS, std::string& PS, const char* UESamplerFormat, const char* UnitySamplerFormat,
						 const char* TextureSamplerFormatURP_HDRP, const char* TextureSamplerFormatBuiltIn )
{
	char Line[ 1024 ];
	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings && ( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP ||
						   UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP ))
	{
		char SamplerNameOriginal[ 1024 ] = "";
		snprintf( SamplerNameOriginal, sizeof( SamplerNameOriginal ), UESamplerFormat, i );
		char SamplerNameReplacement[ 1024 ] = "";
		snprintf( SamplerNameReplacement, sizeof( SamplerNameReplacement ), UnitySamplerFormat, i );

		StringReplacement( PS, SamplerNameOriginal, SamplerNameReplacement );
		StringReplacement( VS, SamplerNameOriginal, SamplerNameReplacement );

		snprintf( Line, sizeof( Line ), TextureSamplerFormatURP_HDRP, i, i, i, i );
	}
	else
	{
		snprintf( Line, sizeof( Line ), TextureSamplerFormatBuiltIn, i, i );
	}
	int LineLen = strlen( Line );
	TargetString->insert( Cursor, Line );
	Cursor += LineLen;
}
void GeneratePropertyFields( std::string & GeneratedShader, MaterialBinding* Mat, std::string& GraphShader, std::string& VSStr, std::string& ParameterConfig )
{
	std::string* TargetString = &GeneratedShader;
	
	int Actual2DTexturesUsed[ MaxTextures ] = { 0 };
	int ActualCubeTexturesUsed[ MaxTextures ] = { 0 };
	int ActualTextureArraysUsed[ MaxTextures ] = { 0 };
	int ActualCubeArraysUsed[ MaxTextures ] = { 0 };
	int Actual3DTexturesUsed[ MaxTextures ] = { 0 };
	int ActualPhysicalTexturesUsed[ MaxTextures ] = { 0 };
	DetectNumTexturesUsed( *TargetString, Actual2DTexturesUsed, ActualCubeTexturesUsed, ActualTextureArraysUsed, ActualCubeArraysUsed, Actual3DTexturesUsed,
						   ActualPhysicalTexturesUsed, "CalcPixelMaterialInputs", "PixelMaterialInputs.PixelDepthOffset =" );
	if( VSStr.length() > 0 )
	{
		DetectNumTexturesUsed( VSStr, Actual2DTexturesUsed, ActualCubeTexturesUsed, ActualTextureArraysUsed, ActualCubeArraysUsed, Actual3DTexturesUsed,
							   ActualPhysicalTexturesUsed,
							   nullptr, nullptr );
	}

	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings && ( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP || 
						   UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP ))
		TargetString = &GraphShader;

	int MainTexStart = TargetString->find( "_MainTex" );
	if( MainTexStart == -1 )
	{
		UE_LOG( LogTemp, Error, TEXT( "[UTX] ERROR! GeneratePropertyFields pos == -1 wtf ?" ) );
		return;
	}

	int Cursor = TargetString->find( "\n", MainTexStart );
	Cursor++;

	int ParamIndex = 0;
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( Actual2DTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexProperty( Cursor, TargetString, i, ParamIndex, Mat, "Tex%d", "\t\tMaterial_Texture2D_%d( \"%s\", 2D ) = \"white\" {}\n" );
			ParamIndex++;
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualCubeTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexProperty( Cursor, TargetString, i, ParamIndex, Mat, "TexCube%d", "\t\tMaterial_TextureCube_%d( \"%s\", Cube ) = \"white\" {}\n" );
			ParamIndex++;
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualTextureArraysUsed[ i ] == 1 );
		if( Used )
		{
			AddTexProperty( Cursor, TargetString, i, ParamIndex, Mat, "Tex2DArray%d", "\t\tMaterial_Texture2DArray_%d( \"%s\", 2DArray ) = \"white\" {}\n" );
			ParamIndex++;
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( Actual3DTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexProperty( Cursor, TargetString, i, ParamIndex, Mat, "Tex3D%d", "\t\tMaterial_VolumeTexture_%d( \"%s\", 3D ) = \"white\" {}\n" );
			ParamIndex++;
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualPhysicalTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexProperty( Cursor, TargetString, i, ParamIndex, Mat, "PhysicalTex%d", "\t\tMaterial_VirtualTexturePhysical_%d( \"%s\", 2D ) = \"white\" {}\n" );
			ParamIndex++;
		}
	}

	FUniformExpressionSet_Override* Set_Override = GetUniformExpressionSet_Override( Mat );
	if( UnitySettings && UnitySettings->WithMaterialParameters() && Set_Override && ParameterConfig.length() > 0 )
	{
		#if ENGINE_MAJOR_VERSION >= 5
		TArray<FString> ExportedParamNames;
		TMemoryImageArray<FMaterialNumericParameterInfo>& NumericParameters = Set_Override->GetUniformNumericParameters();
		//TArray<FMaterialNumericParameterInfo> UniqueNumericParameters = GetUniqueNumericParameters( NumericParameters );
		for( int i = 0; i < NumericParameters.Num(); i++ )
		{
			FMaterialNumericParameterInfo& Param = NumericParameters[ i ];
			FString ParamName = Param.ParameterInfo.Name.ToString();
			FString SanitizedParamName = Mat->GetSanitizedParamName( ParamName );
			
			//Sometimes a param name is found multiple times (when material layers are used for example)
			if( ExportedParamNames.Find( SanitizedParamName ) == INDEX_NONE )
			{
				ExportedParamNames.Add( SanitizedParamName );
				AddNumericProperty( Cursor, TargetString, Param, Mat );
			}
		}
		#endif
	}

	TargetString = &GeneratedShader;	

	const char* SamplersStart = "//samplers start";
	Cursor = TargetString->find( SamplersStart, 0 );
	Cursor = TargetString->find( "\n", Cursor );
	Cursor++;

	if( UnitySettings && ( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP ||
						   UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP ) )
	{
		const char* SamplerLines = "SAMPLER( SamplerState_Linear_Repeat );\n"
								   "SAMPLER( SamplerState_Linear_Clamp );\n";
		TargetString->insert( Cursor, SamplerLines );
		Cursor += strlen( SamplerLines );
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = (Actual2DTexturesUsed[ i ] == 1);
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_Texture2D_%dSampler", "samplerMaterial_Texture2D_%d",
								"TEXTURE2D(       Material_Texture2D_%d );\n"
								"SAMPLER(  samplerMaterial_Texture2D_%d );\n"
								"float4 Material_Texture2D_%d_TexelSize;\n"
								"float4 Material_Texture2D_%d_ST;\n",
								"\t\t\tuniform sampler2D    Material_Texture2D_%d;\n"
								"\t\t\tuniform SamplerState Material_Texture2D_%dSampler;\n" );
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualCubeTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_TextureCube_%dSampler", "samplerMaterial_TextureCube_%d",
								"TEXTURECUBE(     Material_TextureCube_%d );\n"
								"SAMPLER(  samplerMaterial_TextureCube_%d );\n",
								//"float4 Material_TextureCube_%d_TexelSize;\n"
								//"float4 Material_TextureCube_%d_ST;\n",
								"\t\t\tuniform samplerCUBE  Material_TextureCube_%d;\n"
								"\t\t\tuniform SamplerState Material_TextureCube_%dSampler;\n" );
		}
	}	
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualTextureArraysUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_Texture2DArray_%dSampler", "samplerMaterial_Texture2DArray_%d",
								"TEXTURE2D_ARRAY( Material_Texture2DArray_%d );\n"
								"SAMPLER(  samplerMaterial_Texture2DArray_%d );\n",
								//"float4 Material_Texture2DArray_%d_TexelSize;\n"
								//"float4 Material_Texture2DArray_%d_ST;\n",
								"\t\t\tuniform sampler2DArray    Material_Texture2DArray_%d;\n"
								"\t\t\tuniform SamplerState Material_Texture2DArray_%dSampler;\n" );
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualCubeArraysUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_TextureCubeArray_%dSampler", "samplerMaterial_TextureCubeArray_%d",
								"TEXTURECUBE_ARRAY( Material_TextureCubeArray_%d );\n"
								"SAMPLER(    samplerMaterial_TextureCubeArray_%d );\n",
								//"float4 Material_TextureCubeArray_%d_TexelSize;\n"
								//"float4 Material_TextureCubeArray_%d_ST;\n",
								"\t\t\tuniform samplerCUBEArray  Material_TextureCubeArray_%d;\n"
								"\t\t\tuniform SamplerState Material_TextureCubeArray_%dSampler;\n" );
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( Actual3DTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_VolumeTexture_%dSampler", "samplerMaterial_VolumeTexture_%d",
								"TEXTURE3D(       Material_VolumeTexture_%d );\n"
								"SAMPLER(  samplerMaterial_VolumeTexture_%d );\n",
								//"float4 Material_VolumeTexture_%d_TexelSize;\n"
								//"float4 Material_VolumeTexture_%d_ST;\n",
								"\t\t\tuniform sampler3D    Material_VolumeTexture_%d;\n"
								"\t\t\tuniform SamplerState Material_VolumeTexture_%dSampler;\n" );
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualPhysicalTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_VirtualTexturePhysical_%dSampler", "samplerMaterial_VirtualTexturePhysical_%d",
								"TEXTURE2D(       Material_VirtualTexturePhysical_%d );\n"
								"SAMPLER(  samplerMaterial_VirtualTexturePhysical_%d );\n",
								//"float4 Material_VirtualTexturePhysical_%d_TexelSize;\n"
								//"float4 Material_VirtualTexturePhysical_%d_ST;\n",
								"\t\t\tuniform sampler2D    Material_VirtualTexturePhysical_%d;\n"
								"\t\t\tuniform SamplerState Material_VirtualTexturePhysical_%dSampler;\n" );
		}
	}

	if( UnitySettings && UnitySettings->WithMaterialParameters() && Set_Override && ParameterConfig.length() > 0)
	{
		#if ENGINE_MAJOR_VERSION >= 5
		TArray<FString> ExportedParamNames;
		auto NumericParameters = Set_Override->GetUniformNumericParameters();
		for( int i = 0; i < NumericParameters.Num(); i++ )
		{
			FMaterialNumericParameterInfo& Param = NumericParameters[ i ];
			FString ParamName = Param.ParameterInfo.Name.ToString();
			FString SanitizedParamName = Mat->GetSanitizedParamName( ParamName );
			//Sometimes a param name is found multiple times (when material layers are used for example)
			if( ExportedParamNames.Find( SanitizedParamName ) == INDEX_NONE )
			{
				ExportedParamNames.Add( SanitizedParamName );
				AddNumericPropertyDecalaration( Cursor, TargetString, Param, Mat );
			}
		}
		#endif
	}
}

MaterialBinding::MaterialBinding()
{
	UnityMat = new UnityMaterial();
	GExporterScene->Materials.push_back( UnityMat );
}
FString MaterialBinding::GetSanitizedParamName( FString ParamName )
{
	FString SanitizedParamName = ParamName;
	if (SanitizedParamNames.Find( ParamName ))
	{
		SanitizedParamName = SanitizedParamNames[ParamName];
	}
	return SanitizedParamName;
}
FString MaterialBinding::GenerateName()
{
	FString MatName = MaterialInterface->GetName();
	
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>( MaterialInterface );
	if( MID )
	{
		MatName = FString::Printf( TEXT( "%s_%d" ), *MatName, MaterialInterface->GetUniqueID() );
		FString ParentName = MID->Parent->GetName();
		FString FinalName = ParentName + TEXT("(" ) + MatName + TEXT(")" );
		UnityMat->Name = ToANSIString( *FinalName );
		return FinalName;
	}
	else
	{
		UnityMat->Name = TCHAR_TO_ANSI( *MatName );
		return MatName;
	}
}
bool IsTextureNormalMap( UMaterial* Mat, UTexture *Tex )
{
	TArray<UMaterialExpression*> OutExpressions = GetExpressions( Mat );
	
	for( int i = 0; i < OutExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = OutExpressions[ i ];
		UMaterialExpressionTextureSample* SampleExpression = Cast< UMaterialExpressionTextureSample>( Exp );
		if( SampleExpression )
		{
			if( SampleExpression->Texture == Tex )
			{
				if( SampleExpression->SamplerType == SAMPLERTYPE_Normal )
					return true;
				else
					return false;
				//if( SampleExpression->SamplerType == SAMPLERTYPE_Color )
			}
		}
	}

	return false;
}
bool IsTextureNormalMap( UMaterialInstance* MaterialInstance, UTexture* Tex )
{
	UMaterial* BaseMaterial = MaterialInstance->GetBaseMaterial();
	bool IsNormalMap = false;
	TArray<FMaterialParameterInfo> OutParameterInfo;
	TArray<FGuid> OutParameterIds;
	MaterialInstance->GetAllTextureParameterInfo( OutParameterInfo, OutParameterIds );
	for( int i = 0; i < OutParameterInfo.Num(); i++ )
	{
		FMaterialParameterInfo& Info = OutParameterInfo[ i ];

		UTexture* OutValue = nullptr;
		if( MaterialInstance->GetTextureParameterValue( Info, OutValue ) && OutValue )
		{
			if( OutValue == Tex )
			{
				UTexture* DefaultTexture = nullptr;
				if( BaseMaterial->GetTextureParameterDefaultValue( Info, DefaultTexture ) && DefaultTexture )
				{
					IsNormalMap = IsTextureNormalMap( BaseMaterial, DefaultTexture );
					return IsNormalMap;
				}
			}
		}
	}

	return false;
}
void GetMaterialTextures( UMaterialInterface* UnrealMat, TArray<UTexture*>& OutTextures, TArray<FString>& TextureParamNames )
{	
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
	const FMaterialResource* MaterialResource = GetMaterialResource( UnrealMat );
	if( MaterialResource )
	{
		const FUniformExpressionSet& UniformExpressions = MaterialResource->GetUniformExpressions();
		int32 Count = (uint32)EMaterialTextureParameterType::Count;
		for( int32 TypeIndex = 0; TypeIndex < Count; TypeIndex++ )
		{
			int NumTextures = UniformExpressions.GetNumTextures( (EMaterialTextureParameterType)TypeIndex );
			for( int32 TextureIndex = 0; TextureIndex < NumTextures; TextureIndex++ )
			{
				// Evaluate the expression in terms of this material instance.
				UTexture* Texture = NULL;
				if ( IsInGameThread() )
					UniformExpressions.GetGameThreadTextureValue( (EMaterialTextureParameterType)TypeIndex, TextureIndex, UnrealMat, *MaterialResource, Texture, true );
				else
				{
					FMaterialRenderContext DummyContext( UnrealMat->GetRenderProxy(), *MaterialResource, nullptr);
					UniformExpressions.GetTextureValue( (EMaterialTextureParameterType)TypeIndex, TextureIndex, DummyContext, *MaterialResource, (const UTexture*&)Texture );
				}

				if( !Texture )
				{
					//Edge case when material instance texture is overriden but is null and UE uses the base texture in this case
					UniformExpressions.GetGameThreadTextureValue( (EMaterialTextureParameterType)TypeIndex, TextureIndex, UnrealMat->GetBaseMaterial(), *MaterialResource, Texture, true);
				}
				OutTextures.Add( Texture );
				FMaterialTextureParameterInfo Param = UniformExpressions.GetTextureParameter( (EMaterialTextureParameterType)TypeIndex, TextureIndex );
				#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
					FString ParamName = Param.ParameterInfo.Name.ToString();
				#else
					FString ParamName = Param.ParameterName;
				#endif
				if( ParamName.Equals( TEXT("None") ) && Texture)
				{
					ParamName = Texture->GetName();
				}
				TextureParamNames.Add( ParamName );
			}
		}
	}
	else
	{
		TArray< TArray<int32> > TextureIndices;
		UnrealMat->GetUsedTexturesAndIndices( OutTextures, TextureIndices, GlobalQualityLevel, GlobalFeatureLevel );
	}
	#else
	const FMaterialResource* MaterialResource = UnrealMat->GetMaterialResource( GlobalFeatureLevel, GlobalQualityLevel );
	if( MaterialResource )
	{
		FMaterialShaderMap* MaterialShaderMap = MaterialResource->GetGameThreadShaderMap();
		const FUniformExpressionSet& UniformExpressions = MaterialShaderMap->GetUniformExpressionSet();
		FUniformExpressionSet_Override* UniformExpressions_Override = (FUniformExpressionSet_Override*)&UniformExpressions;

		auto Uniform2DTextureExpressions = UniformExpressions_Override->GetUniform2DTextureExpressions();
		auto UniformCubeTextureExpressions = UniformExpressions_Override->GetUniformCubeTextureExpressions();
		auto UniformVolumeTextureExpressions = UniformExpressions_Override->GetUniformVolumeTextureExpressions();
		
		FMaterialRenderContext DummyContext( UnrealMat->GetRenderProxy( false ), *MaterialResource, nullptr );

		for( int i = 0; i < Uniform2DTextureExpressions.Num(); i++)
		{
			FMaterialUniformExpressionTexture* TextureExpression = Uniform2DTextureExpressions[ i ].GetReference();
			const UTexture* Texture = NULL;
			ESamplerSourceMode OutSamplerSource;
			TextureExpression->GetTextureValue( DummyContext, *MaterialResource, Texture, OutSamplerSource );
			if( Texture )
			{
				OutTextures.Add( ( UTexture*)Texture );
				//TextureParamNames.Add( TextureExpression-> );
				//MaterialResource->param
				//FMaterial::GetReferencedTextures
			}
		}
		for( int i = 0; i < UniformCubeTextureExpressions.Num(); i++)
		{
		}
		for( int i = 0; i < UniformVolumeTextureExpressions.Num(); i++ )
		{
		}
		//TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > Uniform2DTextureExpressions;
		//TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > UniformCubeTextureExpressions;
		//TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > UniformVolumeTextureExpressions;
	}
	#endif
}
void CountTextures( MaterialBinding* Mat, TArray<UTexture*>& AllUsedTextures )
{
	UMaterialInterface* UnrealMat = Mat->MaterialInterface;
	TArray<UTexture*> UsedTextures;
	TArray< TArray<int32> > TextureIndices;
	GetMaterialTextures( UnrealMat, UsedTextures, Mat->TextureParamNames );

	for( int i = 0; i < UsedTextures.Num(); i++ )
	{
		if( AllUsedTextures.Find( UsedTextures[ i ] ) == INDEX_NONE )
			AllUsedTextures.Add( UsedTextures[ i ] );
	}

	Mat->TextureParamNames.Reset( 0 );
}
TextureSemantic MaterialPropertyToTextureSemantic( EMaterialProperty MP )
{
	switch( MP )
	{
		case MP_EmissiveColor	: return TS_EmissionMap;
		case MP_Opacity			: return TS_OpacityMap;
		case MP_OpacityMask		: return TS_OpacityMap;
		case MP_BaseColor		: return TS_BaseMap;
		case MP_Metallic		: return TS_MetallicGlossMap;
		case MP_Specular		: return TS_MetallicGlossMap;
		case MP_Roughness		: return TS_Unknown;
		case MP_Normal			: return TS_BumpMap;
		case MP_AmbientOcclusion: return TS_OcclusionMap;
		//Unsure about this
		case MP_MaterialAttributes: return TS_BaseMap;

		default: return TS_Unknown;
	}
}
class SemanticArray
{
public:
	TArray<TextureSemantic> Array;
	TextureSemantic Selection = TS_Unknown;
};

void EvaluateSemantics( TArray<SemanticArray>& TextureSemantics, TArray<FString> TextureParamNames )
{

	const char* Keywords[ TS_MaxSemantics ][ 3 ] = {
		{"","",""},
		{"Diffuse","Albedo","Base"},//TS_BaseMap
		{"Normal","Nrm","Bump"},//TS_BumpMap
		{"Detail","",""},//TS_DetailAlbedoMap
		{"Detail","",""},//TS_DetailMask
		{"Detail","",""},//TS_DetailNormalMap
		{"Emission","Emissive",""},//TS_EmissionMap
		{"","",""},//TS_MainTex		
		{"Metal","Gloss",""},//TS_MetallicGlossMap
		{"Occlusion","ORM","AO"},//TS_OcclusionMap
		{"Parallax","",""},//TS_ParallaxMap
		{"","",""},//TS_SpecGlossMap
	};
	for( int i = 1; i < TS_MaxSemantics; i++ )
	{
		TextureSemantic Semantic = (TextureSemantic)i;
		TArray<int> TexturesWithSemantic;
		for( int u = 0; u < TextureSemantics.Num(); u++ )
		{
			for( int t = 0; t < TextureSemantics[ u ].Array.Num(); t++ )
			{
				if( TextureSemantics[ u ].Array[ t ] == Semantic )
				{
					TexturesWithSemantic.Add( u );
				}
			}
		}

		for( int u = 0; u < TexturesWithSemantic.Num(); u++ )
		{
			bool Matches = false;
			for( int k = 0; k < 3; k++ )
			{
				if ( TextureParamNames.Num() > TexturesWithSemantic[ u ] &&
					 TextureParamNames[ TexturesWithSemantic[ u ] ].Contains( Keywords[ Semantic ][ k ] ))
				{
					//keep it
					Matches = true;
					break;
				}
			}
			if( Matches )
				TextureSemantics[ TexturesWithSemantic[ u ] ].Selection = Semantic;
		}
	}

	//The semantics that I know for sure were selected based on their names
	bool DetectedSemantics[ TS_MaxSemantics ] = { false };
	for( int u = 0; u < TextureSemantics.Num(); u++ )
	{
		DetectedSemantics[ TextureSemantics[ u ].Selection ] = true;
	}

	for( int u = 0; u < TextureSemantics.Num(); u++ )
	{
		//If not detected choose the first semantic, but not one that was previously explicitly selected
		SemanticArray& SA = TextureSemantics[ u ];
		if( SA.Selection == TS_Unknown && SA.Array.Num() > 0 )
		{
			for( int s = 0; s < SA.Array.Num(); s++ )
			{
				if( !DetectedSemantics[ SA.Array[ s ] ] )
				{
					SA.Selection = SA.Array[ s ];
					break;
				}
			}
		}
	}
}
//Unreal thinks these are colors so it converted them to SRGB for no reason
float ConvertToOriginalValue( float InVal )
{
	FLinearColor R( InVal, InVal, InVal, InVal );
	FColor Color = R.ToFColor( true );
	float OutVal = (float)Color.R / 255.0f;
	return OutVal;
}
FExpressionInput* GetMaterialAttributesInput( UMaterialExpressionMakeMaterialAttributes* MaterialAttributes, EMaterialProperty Property )
{
	switch( Property )
	{
		case MP_EmissiveColor: return &MaterialAttributes->EmissiveColor;
		case MP_Opacity: return &MaterialAttributes->Opacity;
		case MP_OpacityMask: return &MaterialAttributes->OpacityMask;
		default:
		case MP_BaseColor: return &MaterialAttributes->BaseColor;
		case MP_Metallic: return &MaterialAttributes->Metallic;
		case MP_Specular: return &MaterialAttributes->Specular;
		case MP_Roughness: return &MaterialAttributes->Roughness;
		case MP_Normal: return &MaterialAttributes->Normal;
		case MP_AmbientOcclusion: return &MaterialAttributes->AmbientOcclusion;
	}
}

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
namespace ERHIShadingPath
{
	enum Type : int
	{
		Deferred,
		Forward,
		Mobile,
		Num
	};
}
#endif
bool GetExpressionsInPropertyChain( UMaterial* Material, EMaterialProperty InProperty,
											   TArray<UMaterialExpression*>& OutExpressions, struct FStaticParameterSet* InStaticParameterSet,
											   ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type InQuality, ERHIShadingPath::Type InShadingPath, bool SearchFunctions )
{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	bool RetrievedExpressions = Material->GetExpressionsInPropertyChain( InProperty, OutExpressions, InStaticParameterSet, InFeatureLevel, InQuality );
#elif ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 1 )
	bool RetrievedExpressions = Material->GetExpressionsInPropertyChain( InProperty, OutExpressions, InStaticParameterSet, InFeatureLevel, InQuality, InShadingPath );
#else
	bool RetrievedExpressions = Material->GetExpressionsInPropertyChain( InProperty, OutExpressions, InStaticParameterSet, InFeatureLevel, InQuality, InShadingPath, SearchFunctions );
#endif
	return RetrievedExpressions;
}
class UMaterial2 : public UMaterial
{
public:
	bool RecursiveGetExpressionChainPublic( UMaterialExpression* InExpression, TArray<FExpressionInput*>& InOutProcessedInputs,
													TArray<UMaterialExpression*>& OutExpressions, struct FStaticParameterSet* InStaticParameterSet,
													ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num,
													EMaterialQualityLevel::Type InQuality = EMaterialQualityLevel::Num,
													ERHIShadingPath::Type InShadingPath = ERHIShadingPath::Num,
													EShaderFrequency InShaderFrequency = SF_NumFrequencies,
													EMaterialProperty InProperty = MP_MAX,
													const bool bInRecurseIntoMaterialFunctions = false )
	{
	#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
		return RecursiveGetExpressionChain(
			InExpression,
			InOutProcessedInputs,
			OutExpressions, InStaticParameterSet,
			InFeatureLevel,
			InQuality );
	#elif ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 1 )
		return RecursiveGetExpressionChain(
			InExpression,
			InOutProcessedInputs,
			OutExpressions, InStaticParameterSet,
			InFeatureLevel,
			InQuality,
			InShadingPath,
			InShaderFrequency );
	#else
		return RecursiveGetExpressionChain(
			InExpression,
			InOutProcessedInputs,
			OutExpressions, InStaticParameterSet,
			InFeatureLevel,
			InQuality,
			InShadingPath,
			InShaderFrequency,
			InProperty,
			bInRecurseIntoMaterialFunctions );
	#endif
	}
};
UnityTexture* InitializeUnityTexture( UTexture* Tex, int* NumCubes, int* Num2DArrays, int* Num3D, int* NumVirtual )
{
	UnityTexture* OutTex = new UnityTexture( Tex );
	FString TexturePath = GenerateTexturePath( Tex );
	FString ResourcePath = GenerateTexturePath( Tex, true );
	OutTex->File = ToANSIString( *TexturePath );
	OutTex->ResourcePath = ToANSIString( *ResourcePath );
	OutTex->GenerateGUID();
	UTextureCube* IsCube = Cast<UTextureCube>( Tex );
	if( IsCube )
	{
		OutTex->Shape = UTS_Cube;
		//OutTex->SpecificIndex = *NumCubes;
		(*NumCubes)++;
	}
#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
	UTexture2DArray* Tex2DArray = Cast< UTexture2DArray>( Tex );
	if( Tex2DArray )
	{
		OutTex->Shape = UTS_2DArray;
		//OutTex->SpecificIndex = *Num2DArrays;
		(*Num2DArrays)++;
	}
#endif
	UVolumeTexture* IsVolume = Cast<UVolumeTexture>( Tex );
	if( IsVolume )
	{
		OutTex->Shape = UTS_3D;
		//OutTex->SpecificIndex = *Num3D;
		(*Num3D)++;
	}
#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
	if( Tex->VirtualTextureStreaming == 1 && GetDefault<URendererSettings>()->bVirtualTextures )
	{
		OutTex->IsVirtual = true;
		//OutTex->SpecificIndex = *NumVirtual;
		(*NumVirtual)++;
	}
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	if( Tex->Availability == ETextureAvailability::CPU )
	{
		OutTex->IsReadable = true;
	}
#endif

	if( Tex->CompressionSettings == TC_Default ||
		Tex->CompressionSettings == TC_Normalmap ||
		Tex->CompressionSettings == TC_HDR_Compressed ||
		Tex->CompressionSettings == TC_BC7 )
		OutTex->Compression = 1;
	else
		OutTex->Compression = 0;

	if (Tex->MaxTextureSize > 0 )
		OutTex->MaxTextureSize = Tex->MaxTextureSize;

	return OutTex;
}
void ExportMaterialTextures( MaterialBinding *Mat, TArray<UTexture*>& AllUsedTextures, int InitialTotalTextures )
{
	UMaterialInterface *UnrealMat = Mat->MaterialInterface;

	TArray<UTexture*> UsedTextures;
	TArray<SemanticArray> TextureSemantics;
	GetMaterialTextures( UnrealMat, UsedTextures, Mat->TextureParamNames );

	for( int i = 0; i < UsedTextures.Num(); i++ )
	{
		SemanticArray EmptyArray;
		TextureSemantics.Add( EmptyArray );
	}

	EMaterialProperty PropertiesToTrace[10] =
	{
		MP_EmissiveColor ,
		MP_Opacity ,
		MP_OpacityMask ,
		MP_BaseColor ,
		MP_Metallic ,
		MP_Specular ,
		MP_Roughness ,
		MP_Normal ,
		MP_AmbientOcclusion,		
	};
	auto BaseMaterial = UnrealMat->GetBaseMaterial();
	UMaterialInstance* MatInst = Cast<UMaterialInstance>( UnrealMat );
	
	{
		int NumProps = sizeof( PropertiesToTrace ) / sizeof(PropertiesToTrace[ 0 ]);
		for( int32 MPIdx = 0; MPIdx < NumProps; MPIdx++ )
		{
			EMaterialProperty MaterialProp = PropertiesToTrace[ MPIdx ];
			TArray<UMaterialExpression*> MPRefdExpressions;
			FStaticParameterSet* InStaticParameterSet = nullptr;
			FStaticParameterSet ParameterSet;
			if( MatInst )
			{
				ParameterSet = MatInst->GetStaticParameters();
				InStaticParameterSet = &ParameterSet;
			}
			ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num;
			EMaterialQualityLevel::Type InQuality = EMaterialQualityLevel::Num;
			ERHIShadingPath::Type InShadingPath = ERHIShadingPath::Num;

			bool RetrievedExpressions = false;
			if( BaseMaterial->bUseMaterialAttributes )
			{
				FExpressionInput* Input = BaseMaterial->GetExpressionInputForProperty( MP_MaterialAttributes );
				if( !Input )
					continue;
				UMaterialExpressionMakeMaterialAttributes* MaterialAttributes = Cast< UMaterialExpressionMakeMaterialAttributes>( Input->Expression );
				if( MaterialAttributes )
				{
					FExpressionInput* MAInput = GetMaterialAttributesInput( MaterialAttributes, MaterialProp );
					if( MAInput->Expression )
					{
						TArray<FExpressionInput*> ProcessedInputs;
						ProcessedInputs.Add( MAInput );
						EShaderFrequency ShaderFrequency = SF_NumFrequencies;
						UMaterial2* BaseMaterial2 = (UMaterial2*)BaseMaterial;
						RetrievedExpressions = BaseMaterial2->RecursiveGetExpressionChainPublic( MAInput->Expression, ProcessedInputs, MPRefdExpressions, InStaticParameterSet, InFeatureLevel, InQuality, InShadingPath, ShaderFrequency, MaterialProp, true );
					}
				}
			}
			else
				RetrievedExpressions = GetExpressionsInPropertyChain( BaseMaterial, MaterialProp, MPRefdExpressions, InStaticParameterSet, InFeatureLevel, InQuality, InShadingPath, true );

			if ( RetrievedExpressions )
			{
				for( int i = 0; i < MPRefdExpressions.Num(); i++ )
				{
					UMaterialExpression* Exp = MPRefdExpressions[ i ];
					UMaterialExpressionTextureBase* TexBase = Cast< UMaterialExpressionTextureBase>( Exp );
					if( TexBase )
					{
						UTextureCube* IsCubeTex = Cast< UTextureCube>( TexBase->Texture );
						UVolumeTexture* IsVolumeTex = Cast<UVolumeTexture>( TexBase->Texture );
						//Ignore Cube/3D Textures since it will error out the FBX/standard materials
						if( IsCubeTex || IsVolumeTex )
							continue;

						UMaterialExpressionTextureSampleParameter2D* SampleParam2D = Cast< UMaterialExpressionTextureSampleParameter2D>( Exp );
						UMaterialExpressionTextureObjectParameter* TexObjectParam = Cast< UMaterialExpressionTextureObjectParameter>( Exp );
						int ReferencedIndex = -1;
						if( SampleParam2D )
						{
							for( int t = 0; t < UsedTextures.Num(); t++ )
							{
								if( SampleParam2D->ParameterName.ToString().Compare( Mat->TextureParamNames[ t ] ) == 0 )
								{
									ReferencedIndex = t;
								}
							}
						}
						else if ( TexObjectParam )
						{
							for (int t = 0; t < UsedTextures.Num(); t++)
							{
								if ( TexObjectParam->ParameterName.ToString().Compare( Mat->TextureParamNames[t] ) == 0 )
								{
									ReferencedIndex = t;
								}
							}
						}
						else
						{
							for( int t = 0; t < UsedTextures.Num(); t++ )
							{
								if( TexBase->Texture == UsedTextures[ t ] )
								{
									ReferencedIndex = t;
								}
							}
						}
						if( ReferencedIndex != -1 )
						{
							TextureSemantic Semantic = MaterialPropertyToTextureSemantic( MaterialProp );
							TextureSemantics[ ReferencedIndex ].Array.Add( Semantic );
						}
					}
				}
			}
		}
	}

	EvaluateSemantics( TextureSemantics, Mat->TextureParamNames );
	
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( UnrealMat );

	UTexture* OutTex = nullptr;
	float OutVal = 0.0f;
	bool OutBoolVal = false;
	FGuid OutParameterId;
	FLinearColor Color;
	if( MaterialInstance )
	{
		if( MaterialInstance->GetScalarParameterValue( TEXT( "OpacityConst" ), OutVal, true ) )
		{
			Mat->UnityMat->BaseColorMultiplier.A = OutVal;
		}
		if( MaterialInstance->GetTextureParameterValue( TEXT( "OpacityMaskTexture" ), OutTex, true ))// ||
			//MaterialInstance->GetStaticSwitchParameterValue( TEXT( "UseOpacityMask" ), OutBoolVal, OutParameterId, true )
		{
			//Make it take the texture alpha, not the one from constant color
			Mat->UnityMat->BaseColorMultiplier.A = 1.0f;
			Mat->UnityMat->HasOpacityMask = 1;
		}
		if( MaterialInstance->GetScalarParameterValue( TEXT( "RoughnessConst" ), OutVal, true ) )
		{
			float OriginalRoughness = ConvertToOriginalValue( OutVal );
			Mat->UnityMat->SmoothnessMultiplier = 1.0f - OriginalRoughness;
		}
		if( MaterialInstance->GetScalarParameterValue( TEXT( "MetallicConst" ), OutVal, true ) )
		{
			Mat->UnityMat->MetallicValue = ConvertToOriginalValue( OutVal );
		}
		if( MaterialInstance->GetVectorParameterValue( TEXT( "BaseColorConst" ), Color, true ) )
		{
			Mat->UnityMat->BaseColorMultiplier.R = Color.R;
			Mat->UnityMat->BaseColorMultiplier.G = Color.G;
			Mat->UnityMat->BaseColorMultiplier.B = Color.B;
		}
		if( MaterialInstance->GetVectorParameterValue( TEXT( "EmissiveColorConst" ), Color, true ) )
		{
			Mat->UnityMat->EmissionColor = Color;
			Mat->UnityMat->EnableEmission = true;
		}
		else if( MaterialInstance->GetTextureParameterValue( TEXT( "EmissiveColorTexture" ), OutTex, true ) )
		{
			Mat->UnityMat->EmissionColor = FLinearColor( 1, 1, 1, 1 );
			Mat->UnityMat->EnableEmission = true;
		}
		if( MaterialInstance->IsTwoSided() )
		{
			Mat->UnityMat->CullMode = 0;
		}
		else
			Mat->UnityMat->CullMode = 2;
	}

	int NumCubes = 0;
	int Num2DArrays = 0;
	int Num3D = 0;
	int NumVirtual = 0;
	for( int i = 0; i < UsedTextures.Num(); i++ )
	{
		UTexture* Tex = UsedTextures[ i ];

		if( Tex )
		{
			bool IsNormalMap = false;
			if( MaterialInstance )
			{
				IsNormalMap = IsTextureNormalMap( MaterialInstance, Tex );
			}
			else
				IsNormalMap = IsTextureNormalMap( BaseMaterial, Tex );

			AllUsedTextures.Remove( Tex );
			int CurrentCount = InitialTotalTextures - AllUsedTextures.Num();
			//UE_LOG( LogTemp, Log, TEXT( "Exporting Texture %s (%d/%d)" ), *Tex->GetName(), CurrentCount, InitialTotalTextures );
			TextureBinding* Binding = UETextureExporter::Export( Tex );

			Binding->IsNormalMap = IsNormalMap;
			if( !Binding->UnityTex )
			{
				Binding->UnityTex = InitializeUnityTexture( Tex, &NumCubes, &Num2DArrays, &Num3D, &NumVirtual );
				Binding->UnityTex->Semantic = TextureSemantics[ i ].Selection;
			}
			Mat->UnityMat->Textures.push_back( Binding->UnityTex );
		}
		else
			Mat->UnityMat->Textures.push_back( nullptr );
	}	
}

UMaterialInstanceDynamic* ShaderErrorMID = nullptr;
UMaterialInterface* MaterialParentForShaderError = nullptr;
UTexture2D* ShaderErrorTexture2D = nullptr;
FMaterialRenderProxy* GetRenderProxy( UMaterialInterface* Material)
{
	#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
		return Material->GetRenderProxy( false );
	#else
		return Material->GetRenderProxy();
	#endif
}
bool GetMaterialVariables( MaterialBinding* Binding, bool IsOnGamethread )
{
	UMaterialInterface* M = Binding->MaterialInterface;
	bool WantsGameThread = false;
	UMaterialInstance* MatInstance = dynamic_cast<UMaterialInstance*>( M );
	if( MatInstance )
	{
		if( !MatInstance->Parent )
		{
			UObject* LoadedObject = StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, TEXT( "/Engine/EngineMaterials/DefaultMaterial" ) );
			UMaterialInterface* DefaultMaterial = Cast<UMaterialInterface>( LoadedObject );
			if( DefaultMaterial )
				MatInstance->Parent = DefaultMaterial;
		}
		WantsGameThread = true;
	}

	bool ExtractShaderSource = ( WantsGameThread == IsOnGamethread );
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
	ExtractShaderSource = IsOnGamethread;
#endif
	if( ExtractShaderSource )
	{
		FMaterialResource* MaterialResource = GetMaterialResource( M );
		if( !MaterialResource )
		{
			UE_LOG( LogTemp, Error, TEXT( "[UTX] Warning! Material %s has Parent=null is probably ?" ), *M->GetName() );
			return false;
		}

		Binding->RenderProxy = GetRenderProxy( M );
		return true;
	}

	return false;
}
void GetMaterialShaderSource( MaterialBinding *Binding, bool IsOnGamethread )
{
	if( GetMaterialVariables( Binding, IsOnGamethread ) )
	{
		UMaterialInterface* M = Binding->MaterialInterface;
		FMaterialResource* MaterialResource = GetMaterialResource( Binding->MaterialInterface );
		MaterialResource->GetMaterialExpressionSource( Binding->UnityMat->Shader->UnrealShaderSource );

		//Set our special material if shader couldn't compile so we know which Material/MI is at fault in Unreal
		if( Binding->UnityMat->Shader->UnrealShaderSource.Len() == 0 )
		{
			//Create a new one so if there's multiple uncompilable materials, don't overwrite them !
			auto NewMID = UMaterialInstanceDynamic::Create( MaterialParentForShaderError, nullptr );
			bool IsOn5_2_OrNewer = false;

			#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
				NewMID->SetTextureParameterValue( TEXT( "Texture" ), ShaderErrorTexture2D );			
				IsOn5_2_OrNewer = true;
			#endif
			NewMID->AddToRoot();
			static int NumDidntCompile = 0;
			FString NewName = FString::Printf( TEXT( "%s_Didnt_Compile_%d" ), *M->GetName(), NumDidntCompile++ );

			if ( IsOn5_2_OrNewer )
				NewMID->Rename( *NewName );

			Binding->MaterialInterface = NewMID;

			MaterialResource = GetMaterialResource( Binding->MaterialInterface );
			Binding->RenderProxy = GetRenderProxy( Binding->MaterialInterface );
			MaterialResource->GetMaterialExpressionSource( Binding->UnityMat->Shader->UnrealShaderSource );

			TArray<UTexture*> UsedTextures;
			GetMaterialTextures( Binding->MaterialInterface, UsedTextures, Binding->TextureParamNames );
			if (!IsOn5_2_OrNewer )
				UsedTextures.Add( ShaderErrorTexture2D );
			for( int i = 0; i < UsedTextures.Num(); i++ )
			{
				TextureBinding* TexBinding = UETextureExporter::Export( UsedTextures[ i ] );
				if( !TexBinding->UnityTex )
				{
					UnityTexture* UnityTex = new UnityTexture( UsedTextures[ i ] );
					FString TexturePath = GenerateTexturePath( UsedTextures[ i ] );
					UnityTex->File = ToANSIString( *TexturePath );
					UnityTex->GenerateGUID();
					TexBinding->UnityTex = UnityTex;
				}
				Binding->UnityMat->Textures.push_back( TexBinding->UnityTex );
			}			
		}

		auto ReferencedTextures = MaterialResource->GetReferencedTextures();
	}
}

std::string GetLine( std::string String, int position )
{
	int LineStart = String.rfind( '\n', position );
	int LineEnd = String.find( '\n', position );
	if( LineStart == LineEnd )
		return "";
	if( LineStart != -1 && LineEnd != -1 )
	{
		return String.substr( LineStart + 1, LineEnd - LineStart - 1 );
	}
	else if( LineStart == -1 && LineEnd != -1 )
	{
		return String.substr( 0, LineEnd );
	}
	else
		return "";
}
std::string NormalizeLineEndings( const std::string& input )
{
	std::string output;
	output.reserve( input.size() );

	for( size_t i = 0; i < input.size(); ++i )
	{
		if( input[ i ] == '\r' )
		{
			// If CR is followed by LF, skip the CR (Windows-style)
			if( i + 1 < input.size() && input[ i + 1 ] == '\n' )
			{
				// Skip '\r', let the next loop iteration handle '\n'
				continue;
			}
			else
			{
				// CR-only (classic Mac-style), replace with LF
				output.push_back( '\n' );
			}
		}
		else
		{
			output.push_back( input[ i ] );
		}
	}

	return output;
}
int GetNumTexcoords( std::string UnrealHLSL )
{
	int pos = UnrealHLSL.find( "#define NUM_TEX_COORD_INTERPOLATORS" );
	if( pos != -1 )
	{
		std::string Line = GetLine( UnrealHLSL, pos );
		if( Line.length() > 0 )
		{
			int NumTexcoords = 0;
			if( sscanf( Line.c_str(), "#define NUM_TEX_COORD_INTERPOLATORS %d", &NumTexcoords ) == 1 )
			{
				return NumTexcoords;
			}
		}
	}

	return 0;
}
int GetNumTexcoordsVertex( std::string UnrealHLSL )
{
	int pos = UnrealHLSL.find( "#define NUM_MATERIAL_TEXCOORDS_VERTEX" );
	if( pos != -1 )
	{
		std::string Line = GetLine( UnrealHLSL, pos );
		if( Line.length() > 0 )
		{
			int NumTexcoords = 0;
			if( sscanf( Line.c_str(), "#define NUM_MATERIAL_TEXCOORDS_VERTEX %d", &NumTexcoords ) == 1 )
			{
				return NumTexcoords;
			}
		}
	}

	return 0;
}

int GetEndBraceOffset( std::string AllCode, int StartPos )
{
	int ExpStart = AllCode.rfind( '\n', StartPos );
	int FirstBrace = AllCode.find( '{', ExpStart );

	int Depth = 0;
	const char* str = AllCode.c_str();
	for( int i = FirstBrace + 1; i < AllCode.length(); i++ )
	{
		if( AllCode[ i ] == '{' )
		{
			Depth++;
		}
		if( AllCode[ i ] == '}' )
		{
			Depth--;
		}
		if( Depth < 0 )
		{			
			return i;
		}
	}

	UE_LOG( LogTemp, Error, TEXT( "[UTX] ERROR! GetEndBraceOffset didn't found the end of the function, wtf ?" ) );

	return ExpStart;
}
//Convert HLSL Samples to CG
void ReplaceHLSLSampleCall( std::string & Exp, const char* strToFind, const char* ReplacementFunc )
{
	int FindLen = strlen( strToFind );

	int pos = -1;
	do
	{
		pos = Exp.find( strToFind );
		if( pos != -1 )
		{
			std::string TexName;
			int TexBegin = -1;
			for( int i = pos - 1; i > 0; i-- )
			{
				char c = Exp[ i ];
				if( c == ' ' || c == '\t' || c == '\n' || c == ',' || c == '(' || c == ')' || c == '{' || c == '}' )
				{
					TexName = Exp.substr( i + 1, pos - i - 1 );
					TexBegin = i + 1;
					break;
				}
			}
			if( TexBegin == -1 )
			{
				UE_LOG( LogTemp, Error, TEXT( "[UTX] ERROR! ProcessCustomExpression couldn't detect TexBegin !" ) );
				return;
			}

			std::string Replacement = ReplacementFunc;
			Replacement += TexName;
			Replacement += ", ";
			Exp.replace( TexBegin, TexName.length() + FindLen, Replacement.c_str() );
		}
	} while( pos != -1 );
}
void ProcessCustomExpression( std::string& Exp, bool IsVS )
{
	ReplaceHLSLSampleCall( Exp, ".SampleGrad(", "Texture2DSampleGrad( ");
	ReplaceHLSLSampleCall( Exp, ".Sample(", "Texture2DSample( ");
	ReplaceHLSLSampleCall( Exp, ".SampleLevel(", "Texture2DSampleLevel( ");
	ReplaceHLSLSampleCall( Exp, ".SampleBias(", "Texture2DSampleBias( " );

	if( IsVS )
		StringReplacement( Exp, "Texture2DSample(", "Texture2DSampleForVS(" );

	StringReplacement( Exp, "Material.", "Material_" );
	StringReplacement( Exp, "const", " " );

	StringReplacement( Exp, "View.BufferSizeAndInvSize", "View_BufferSizeAndInvSize" );
	StringReplacement( Exp, "View.MaterialTextureBilinearWrapedSampler",  "View_MaterialTextureBilinearWrapedSampler" );
	StringReplacement( Exp, "View.MaterialTextureBilinearClampedSampler", "View_MaterialTextureBilinearClampedSampler" );
	StringReplacement( Exp, "View.SharedBilinearClampedSampler",		  "View_MaterialTextureBilinearClampedSampler" );

	StringReplacement( Exp, "while", "LOOP\nwhile" );
}
bool ReplaceVertexParametersWithPixelParameters( std::string& Code )
{
	const char* FindStr = "FMaterialVertexParameters";
	const char* ReplaceStr = "FMaterialPixelParameters";
	int ParamPos = Code.find( FindStr );
	if( ParamPos != -1 )
	{
		Code.replace( ParamPos, strlen( FindStr ), ReplaceStr );
		return true;
	}
	else
		return false;
}
void AddCustomExpressions( std::string & MaterialCode, std::string AllCode, bool IsVS = false )
{
	int Index = 0;
	char Text[ 64 ];
	size_t pos = 0;
	do
	{
		snprintf( Text, sizeof( Text ), "CustomExpression%d", Index );
		pos = AllCode.find( Text );
		if ( pos != -1 )
		{
			int ExpStart = AllCode.rfind( '\n', pos );
			int EndBrace = GetEndBraceOffset( AllCode, pos );

			std::string ExpFunction = AllCode.substr( ExpStart, EndBrace + 1 - ExpStart );

			//ReplaceVertexParametersWithPixelParameters( ExpFunction );
			ProcessCustomExpression( ExpFunction, IsVS );

			std::string MaterialCodeWithExpressions = ExpFunction;
			MaterialCodeWithExpressions += '\n';
			MaterialCodeWithExpressions += MaterialCode;
			MaterialCode = MaterialCodeWithExpressions;
		}
		Index++;
	}
	while ( pos != -1 );
}
void ExecuteStringReplacements( std::string& ShaderCode )
{
	StringReplacement( ShaderCode, "Material.Texture", "Material_Texture" );
	StringReplacement( ShaderCode, "Material.VolumeTexture", "Material_VolumeTexture" );
	StringReplacement( ShaderCode, "Material.VirtualTexturePhysical", "Material_VirtualTexturePhysical" );
	StringReplacement( ShaderCode, "Material.Wrap_WorldGroupSettings", "Material_Wrap_WorldGroupSettings" );
	StringReplacement( ShaderCode, "Material.Clamp_WorldGroupSettings", "Material_Clamp_WorldGroupSettings" );
	
	StringReplacement( ShaderCode, "Material_VectorExpressions", "Material.VectorExpressions" );
	StringReplacement( ShaderCode, "Material_ScalarExpressions", "Material.ScalarExpressions" );

	StringReplacement( ShaderCode, "VIRTUALTEXTURE_PAGETABLE_UNIFORM_", "/*VT_Replaced*/" );
	StringReplacement( ShaderCode, "VIRTUALTEXTURE_PAGETABLE_", "/*VIRTUALTEXTURE_PAGETABLE_*/" );
	StringReplacement( ShaderCode, "Scene.MeshPaint.PhysicalTexture", "Material_VirtualTexturePhysical_0" );
	StringReplacement( ShaderCode, "Scene.MeshPaint.PackedUniform", "Material.VTPackedUniform[0]" );
	
	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings )
	{
		if (UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP ||
			UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP)
		{
			StringReplacement( ShaderCode, "View.MaterialTextureBilinearWrapedSampler",  "View_MaterialTextureBilinearWrapedSampler" );
			StringReplacement( ShaderCode, "View.MaterialTextureBilinearClampedSampler", "View_MaterialTextureBilinearClampedSampler" );
			StringReplacement( ShaderCode, "View.SharedBilinearClampedSampler", "View_MaterialTextureBilinearClampedSampler" );
		}
		else//RP_BUILTIN
		{
			StringReplacement( ShaderCode, "View.SharedBilinearClampedSampler", "View.MaterialTextureBilinearClampedSampler" );
		}
	}

	//CameraDirection
	StringReplacement( ShaderCode, "mul(MaterialFloat3(0.00000000,0.00000000,1.00000000).rgb, (MaterialFloat3x3)(ResolvedView.ViewToTranslatedWorld));",
								   "(mul(MaterialFloat3(0.00000000,0.00000000,1.00000000).rgb, (MaterialFloat3x3)(ResolvedView.ViewToTranslatedWorld))).xzy;//AutoReplaced" );
}
std::string GetMaterialCollectionString( const UMaterialParameterCollection* Collection, int CollectionIndex)
{
	int NumVectorsForScalars = ( Collection->ScalarParameters.Num() + 3 ) / 4;
	int TotalVectors = NumVectorsForScalars + Collection->VectorParameters.Num();

	FLinearColor* ScalarVectors = new FLinearColor[ NumVectorsForScalars ];
	std::string ValuesStr;
	std::string* ParamNames = new std::string[ NumVectorsForScalars * 4 ];
	for( int i = 0; i < Collection->ScalarParameters.Num(); i++ )
	{
		FCollectionScalarParameter Param = Collection->ScalarParameters[ i ];
		float* FloatArray = (float*)ScalarVectors;
		FloatArray[ i ] = Param.DefaultValue;
		ParamNames[ i ] = ToANSIString( *Param.ParameterName.ToString());
	}
	for( int i = 0; i < NumVectorsForScalars; i++ )
	{
		char Line[ 1024 ] = "";
		ScalarVectors[ i ] = RemoveNans( ScalarVectors[ i ] );
		snprintf( Line, sizeof( Line ), "\tMaterialCollection%d.Vectors[%d] = float4(%f,%f,%f,%f);//%s,%s,%s,%s\n", CollectionIndex, i, ScalarVectors[i].R, ScalarVectors[ i ].G, ScalarVectors[ i ].B, ScalarVectors[ i ].A,
				   ParamNames[ i * 4 + 0].c_str(), ParamNames[ i * 4 + 1 ].c_str(), ParamNames[ i * 4 + 2 ].c_str(), ParamNames[ i * 4 + 3 ].c_str() );
		ValuesStr += Line;
	}
	for( int i = 0; i < Collection->VectorParameters.Num(); i++ )
	{
		FCollectionVectorParameter Param = Collection->VectorParameters[ i ];
		FLinearColor V = Param.DefaultValue;
		char Line[ 1024 ] = "";
		std::string str = ToANSIString( *Param.ParameterName.ToString() );
		snprintf( Line, sizeof( Line ), "\tMaterialCollection%d.Vectors[%d] = float4(%f,%f,%f,%f);//%s\n", CollectionIndex, NumVectorsForScalars + i, V.R, V.G, V.B, V.A, str.c_str() );
		ValuesStr += Line;
	}

	FString CollectionName = Collection->GetName();
	auto CollectionNameStr = ToANSIString( *CollectionName );
	char Text[ 1024 ] = "";
	snprintf( Text, sizeof( Text ), "struct MaterialCollection%dType\n"
		"{\n"
		"\tfloat4 Vectors[%d];\n"
		"};\n"
		"//%s\n"
		"MaterialCollection%dType MaterialCollection%d;\n"
		"void Initialize_MaterialCollection%d()\n"
		"{\n", CollectionIndex, TotalVectors, CollectionNameStr.c_str(), CollectionIndex, CollectionIndex, CollectionIndex);
	std::string Ret = Text;
	Ret += ValuesStr;

	Ret += "}\n";

	delete[] ScalarVectors;
	delete[] ParamNames;
	return Ret;
}
void UncommentLine( std::string & ShaderString, const char *Marker )
{
	while( 1 )
	{
		size_t Pos = ShaderString.find( Marker );
		if( Pos != ShaderString.npos )
			ShaderString.replace( Pos, strlen( Marker ), "" );//uncomment it
		else
			break;
	}
}
void RemoveComment( std::string& ShaderString, const char* Marker )
{
	while( 1 )
	{
		size_t Pos = ShaderString.find( Marker );
		if( Pos != ShaderString.npos )
			ShaderString.replace( Pos, 2, "" );//remove just "//"
		else
			break;
	}
}
int CommentLine( std::string& ShaderString, const char* Marker )
{
	int Offset = 0;
	int ReturnPos = -1;
	while( 1 )
	{
		size_t Pos = ShaderString.find( Marker, Offset );
		if( Pos != ShaderString.npos )
		{
			ShaderString.insert( Pos, "//" );//add comment
			ReturnPos = Pos;
			Offset = Pos + 3;
		}
		else
			break;
	}

	return ReturnPos;
}
std::vector<std::string> GetLines( std::string String, int position, int NumLines )
{
	std::vector<std::string> Lines;

	int FirstLineStart = String.rfind( '\n', position );
	int Cursor = FirstLineStart + 1;
	for( int i = 0; i < NumLines; i++ )
	{
		std::string Line = GetLine( String, Cursor );
		Cursor += Line.length() + 2;
		Lines.push_back( Line );
	}

	return Lines;
}
void FindAndAddInterpolator( const char* Identifier, std::string HLSL, std::string& GeneratedShader, int InterpStart, int& NextLineMarker )
{
	int DefineCursor = HLSL.find( Identifier, InterpStart );
	if( DefineCursor != -1 )
	{
		std::string Line = GetLine( HLSL, DefineCursor );
		Line += "\n";
		GeneratedShader.insert( NextLineMarker, Line );
		NextLineMarker += Line.length();
	}
}
void ProcessVertexInterpolators( std::string HLSL, std::string & GeneratedShader )
{
	int NumInterpolators = -1;
	int InterpStart = HLSL.find( "NUM_CUSTOM_VERTEX_INTERPOLATORS" );
	if( InterpStart > 0 )
	{
		std::string Line = GetLine( HLSL, InterpStart );
		if( Line.length() > 0 )
		{			
			sscanf( Line.c_str(), "#define NUM_CUSTOM_VERTEX_INTERPOLATORS %d", &NumInterpolators);
		}
	}

	if( NumInterpolators > 0 )
	{
		const char* NUM_CUSTOM_VERTEX_INTERPOLATORS = "NUM_CUSTOM_VERTEX_INTERPOLATORS";
		int NumInterpolatorsCursor = GeneratedShader.find( NUM_CUSTOM_VERTEX_INTERPOLATORS );
		if( NumInterpolatorsCursor == -1 )
			return;

		char Text[ 256 ];
		snprintf( Text, sizeof( Text ), "%d", NumInterpolators );
		GeneratedShader.replace( NumInterpolatorsCursor + strlen( NUM_CUSTOM_VERTEX_INTERPOLATORS ) + 1, strlen( Text ), Text );
		int NextLineMarker = GeneratedShader.find( '\n', NumInterpolatorsCursor );
		NextLineMarker++;

		for( int i = 0; i < NumInterpolators; i++ )
		{
			snprintf( Text, sizeof( Text ), "#define VERTEX_INTERPOLATOR_%d_TEXCOORDS_X", i );
			FindAndAddInterpolator( Text, HLSL, GeneratedShader, InterpStart, NextLineMarker );
			snprintf( Text, sizeof( Text ), "#define VERTEX_INTERPOLATOR_%d_TEXCOORDS_Y", i );
			FindAndAddInterpolator( Text, HLSL, GeneratedShader, InterpStart, NextLineMarker );
			snprintf( Text, sizeof( Text ), "#define VERTEX_INTERPOLATOR_%d_TEXCOORDS_Z", i );
			FindAndAddInterpolator( Text, HLSL, GeneratedShader, InterpStart, NextLineMarker );
			snprintf( Text, sizeof( Text ), "#define VERTEX_INTERPOLATOR_%d_TEXCOORDS_W", i );
			FindAndAddInterpolator( Text, HLSL, GeneratedShader, InterpStart, NextLineMarker );
		}		
	}
}
void ReplaceShaderName( std::string & GeneratedShader, const char * InsertMarker, std::string MatNameStr )
{
	int Pos = GeneratedShader.find( InsertMarker );
	GeneratedShader.replace( Pos, strlen( InsertMarker ), MatNameStr.c_str() );
}
TArray<UMaterialExpression*> GetExpressions( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	TArray<UMaterialExpression*> Expressions;
	if( !Material )
		return Expressions;
	TConstArrayView<TObjectPtr<UMaterialExpression>> ConstExpressions = Material->GetExpressions();
	Expressions.Reserve( ConstExpressions.Num() );
	for( int i = 0; i < ConstExpressions.Num(); i++ )
	{
		Expressions.Add( ConstExpressions[ i ] );
	}
	return Expressions;
#else
	return Material->Expressions;
#endif
}
const TArray<UMaterialExpression*> GetFunctionExpressions( UMaterialFunctionInterface* MaterialFunction)
{
	#if ENGINE_MAJOR_VERSION >= 5
		TArray<UMaterialExpression*> Ret;

		#if ENGINE_MINOR_VERSION >= 1
			TConstArrayView<TObjectPtr<UMaterialExpression>> FunctionExpressions = MaterialFunction->GetExpressions();

			for( int i = 0; i < FunctionExpressions.Num(); i++ )
			{
				Ret.Add( FunctionExpressions[ i ] );
			}
		#else
			const TArray<TObjectPtr<UMaterialExpression>>* FunctionExpressions = MaterialFunction->GetFunctionExpressions();
		
			for( int i = 0; i < FunctionExpressions->Num(); i++ )
			{
				Ret.Add( ( *FunctionExpressions )[ i ] );
			}
		#endif
		return Ret;
	#else
		return *MaterialFunction->GetFunctionExpressions();
	#endif
}

void GatherExpressionsFromMaterialLayers( UMaterialInstance* MaterialInstance, TArray<UMaterialExpression*>& OutExpressions )
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
	FMaterialLayersFunctions OutLayers;
	MaterialInstance->GetMaterialLayers( OutLayers );
	for( int i = 0; i < OutLayers.Layers.Num(); i++ )
	{
		UMaterialFunctionInterface* MaterialFunctionInterface = OutLayers.Layers[ i ];
		if( MaterialFunctionInterface )
		{
			TArray<UMaterialExpression*> FunctionExpressions = GetFunctionExpressions( MaterialFunctionInterface );

			for( int u = 0; u < FunctionExpressions.Num(); u++ )
			{
				auto Exp = FunctionExpressions[ u ];
				OutExpressions.Add( Exp );
			}
		}
	}
	for( int i = 0; i < OutLayers.Blends.Num(); i++ )
	{
		UMaterialFunctionInterface* MaterialFunctionInterface = OutLayers.Blends[ i ];
		if( MaterialFunctionInterface )
		{
			TArray<UMaterialExpression*> FunctionExpressions = GetFunctionExpressions( MaterialFunctionInterface );

			for( int u = 0; u < FunctionExpressions.Num(); u++ )
			{
				auto Exp = FunctionExpressions[ u ];
				OutExpressions.Add( Exp );
			}
		}
	}
#endif
}
TArray<UMaterialExpression*> GetMaterialExpressionsIncludingFunctions( UMaterialInterface* Mat )
{
	TArray<UMaterialExpression*> OutExpressions = GetExpressions( Mat->GetBaseMaterial() );
	for( int i = 0; i < OutExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = OutExpressions[ i ];

		UMaterialExpressionMaterialFunctionCall* ExpMFC = dynamic_cast<UMaterialExpressionMaterialFunctionCall*>( Exp );
		if( ExpMFC && ExpMFC->MaterialFunction )
		{
			TArray<UMaterialExpression*> FunctionExpressions = GetFunctionExpressions( ExpMFC->MaterialFunction );

			for( int u = 0; u < FunctionExpressions.Num(); u++ )
			{
				Exp = FunctionExpressions[ u ];
				OutExpressions.Add( Exp );
			}
			continue;
		}
	}
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( Mat );
	if( MaterialInstance )
	{
		GatherExpressionsFromMaterialLayers( MaterialInstance, OutExpressions );
	}

	return OutExpressions;
}
//bool ExportVS = true;
std::string ExtractFunction( const char* CSource, const char *Marker )
{
	const char* ptr = strstr( CSource, Marker );
	if( ptr )
	{
		int CropStart = (int64_t)ptr - (int64_t)CSource;
		int EndBrace = GetEndBraceOffset( CSource, CropStart );
		int CropEnd = EndBrace;

		int CropSize = CropEnd - CropStart + 1;
		if( CropSize < 0 )
		{
			return "";
		}
		char* StringContents = new char[ CropSize ];
		StringContents[ CropSize ] = 0;
		memcpy( StringContents, ptr, CropSize );
		std::string RetStr = StringContents;
		RetStr += "\n";

		return RetStr;
	}
	else
		return "";
}
const char* PixelToVertexParameters =
"\n"
"	FMaterialVertexParameters Parameters = (FMaterialVertexParameters)0;\n"
"	Parameters.TangentToWorld = PixelParameters.TangentToWorld;\n"
"	Parameters.WorldPosition = PixelParameters.AbsoluteWorldPosition;\n"
"	Parameters.VertexColor = PixelParameters.VertexColor;\n"
"#if NUM_MATERIAL_TEXCOORDS_VERTEX > 0\n"
"	int m = min( NUM_MATERIAL_TEXCOORDS_VERTEX, NUM_TEX_COORD_INTERPOLATORS );\n"
"	for( int i = 0; i < m; i++ )\n"
"	{\n"
"		Parameters.TexCoords[i] = PixelParameters.TexCoords[i];\n"
"	}\n"
"#endif\n"
"	Parameters.PrimitiveId = PixelParameters.PrimitiveId;\n";
void SetupCustomizedUVs( std::string& CustomizedUVs )
{
	if( CustomizedUVs.length() == 0 )
		return;

	CustomizedUVs = std::string("\n") + CustomizedUVs;

	const char* Marker = "FMaterialVertexParameters Parameters";
	int MarkerPos = CustomizedUVs.find( Marker );
	
	const char* PixelParameters = "FMaterialPixelParameters PixelParameters";
	CustomizedUVs.replace( MarkerPos, strlen( Marker ), PixelParameters );
	
	int FirstBraceStart = CustomizedUVs.find( "{" );
	CustomizedUVs.insert( FirstBraceStart + 1, PixelToVertexParameters );

	ExecuteStringReplacements( CustomizedUVs );
}
void SetupTexcoordInterpolators( std::string& ShaderSourceANSI, std::string& GeneratedShader )
{
	int NumTexcoords = GetNumTexcoords( ShaderSourceANSI );
	const char* NumTexcoordsMarker = "NUM_TEX_COORD_INTERPOLATORS";
	char NumTexcoordsStr[ 16 ];
	snprintf( NumTexcoordsStr, sizeof( NumTexcoordsStr ), "%d", NumTexcoords );
	int Pos = GeneratedShader.find( NumTexcoordsMarker );
	if( Pos != -1 )
		GeneratedShader.replace( Pos + strlen( NumTexcoordsMarker ) + 1, strlen( NumTexcoordsStr ), NumTexcoordsStr );
}
void SetupVertexTexcoordInterpolators( std::string& ShaderSourceANSI, std::string& GeneratedShader )
{
	int NumTexcoords = GetNumTexcoordsVertex( ShaderSourceANSI );
	const char* NumTexcoordsMarker = "NUM_MATERIAL_TEXCOORDS_VERTEX";
	char NumTexcoordsStr[ 16 ];
	snprintf( NumTexcoordsStr, sizeof( NumTexcoordsStr ), "%d", NumTexcoords );
	int Pos = GeneratedShader.find( NumTexcoordsMarker );
	if ( Pos != -1 )
		GeneratedShader.replace( Pos + strlen( NumTexcoordsMarker ) + 1, strlen( NumTexcoordsStr ), NumTexcoordsStr );	
}
void AddExpressionsFromMaterialFunctions( TArray<UMaterialExpression*>& OutExpressions )
{
	for( int i = 0; i < OutExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = OutExpressions[ i ];

		UMaterialExpressionMaterialFunctionCall* ExpMFC = dynamic_cast<UMaterialExpressionMaterialFunctionCall*>( Exp );
		if( ExpMFC && ExpMFC->MaterialFunction )
		{
			TArray<UMaterialExpression*> FunctionExpressions = GetFunctionExpressions( ExpMFC->MaterialFunction );

			int Added = 0;
			for( int u = 0; u < FunctionExpressions.Num(); u++ )
			{
				Exp = FunctionExpressions[ u ];
				//Add them in order exactly where FunctionCall was
				//Avoid adding function calls recursively
				if( OutExpressions.Find( Exp ) == INDEX_NONE )
				{
					OutExpressions.Insert( Exp, i + Added );
					Added++;
				}
			}
			continue;
		}
	}
}
void GetMaterialCollectionIndices( UMaterialInterface* UnrealMat, TArray< UMaterialParameterCollection* >& Collections )
{	
	EMaterialProperty Properties[] =
	{
		MP_Normal,
		MP_EmissiveColor,
		MP_DiffuseColor,
		MP_SpecularColor,
		MP_BaseColor,
		MP_Metallic,
		MP_Specular,
		MP_Roughness,
		#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
		MP_Anisotropy,
		#endif
		MP_Opacity,
		MP_OpacityMask,
		#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
		MP_Tangent,
		#endif
		MP_WorldPositionOffset,
		#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
		MP_ShadingModel,
		#endif
		#if ENGINE_MAJOR_VERSION >= 5
			MP_FrontMaterial,
		#endif
		MP_SubsurfaceColor,
		MP_CustomData0,
		MP_CustomData1,
		MP_AmbientOcclusion,
		MP_Refraction,

		MP_PixelDepthOffset
	};

	auto BaseMaterial = UnrealMat->GetBaseMaterial();
	UMaterialInstance* MatInst = Cast<UMaterialInstance>( UnrealMat );

	int ArraySize = sizeof( Properties ) / sizeof( Properties[ 0 ] );
	for( int32 MPIdx = 0; MPIdx < ArraySize; MPIdx++ )
	{
		EMaterialProperty MaterialProp = Properties[ MPIdx ];
		TArray<UMaterialExpression*> MPRefdExpressions;
		FStaticParameterSet* InStaticParameterSet = nullptr;
		FStaticParameterSet ParameterSet;
		ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num;
		EMaterialQualityLevel::Type InQuality = EMaterialQualityLevel::Num;
		ERHIShadingPath::Type InShadingPath = ERHIShadingPath::Num;

		if( MatInst )
		{
			ParameterSet = MatInst->GetStaticParameters();
			InStaticParameterSet = &ParameterSet;
		}
		#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
		if( BaseMaterial->GetExpressionsInPropertyChain( MaterialProp, MPRefdExpressions, InStaticParameterSet, InFeatureLevel, InQuality ) == true )
		#else
		if( BaseMaterial->GetExpressionsInPropertyChain( MaterialProp, MPRefdExpressions, InStaticParameterSet, InFeatureLevel, InQuality, InShadingPath ) == true )
		#endif
		{
			AddExpressionsFromMaterialFunctions( MPRefdExpressions );
			for( int i = 0; i < MPRefdExpressions.Num(); i++ )
			{
				UMaterialExpression* Exp = MPRefdExpressions[ i ];
				UMaterialExpressionCollectionParameter* ExpCollectionParameter = Cast<UMaterialExpressionCollectionParameter>( Exp );

				if( ExpCollectionParameter )
				{
					if( Collections.Find( ExpCollectionParameter->Collection ) == INDEX_NONE )
					{
						Collections.Add( ExpCollectionParameter->Collection );
					}
				}
			}
		}
	}	
}
const char* TangentToWorldAxisFlip =
	"\n"
	"\t//WorldAligned texturing & others use normals & stuff that think Z is up\n"
	"\tParameters.TangentToWorld[0] = Parameters.TangentToWorld[0].xzy;\n"
	"\tParameters.TangentToWorld[1] = Parameters.TangentToWorld[1].xzy;\n"
	"\tParameters.TangentToWorld[2] = Parameters.TangentToWorld[2].xzy;\n";

FExpressionInput* GetMaterialAttributes( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->MaterialAttributes;
#else
	return &Material->MaterialAttributes;
#endif
}
FScalarMaterialInput* GetOpacity( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->Opacity;
#else
	return &Material->Opacity;
#endif
}
FScalarMaterialInput* GetOpacityMask( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->OpacityMask;
#else
	return &Material->OpacityMask;
#endif
}
FColorMaterialInput* GetBaseColor( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->BaseColor;
#else
	return &Material->BaseColor;
#endif
}
FColorMaterialInput* GetEmissiveColor( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->EmissiveColor;
#else
	return &Material->EmissiveColor;
#endif
}
FExpressionInput* GetNormal( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->Normal;
#else
	return &Material->Normal;
#endif
}
FExpressionInput* GetMetallic( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->Metallic;
#else
	return &Material->Metallic;
#endif
}
FExpressionInput* GetRoughness( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->Roughness;
#else
	return &Material->Roughness;
#endif
}
FExpressionInput* GetSpecular( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->Specular;
#else
	return &Material->Specular;
#endif
}
FExpressionInput* GetAnisotropy( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->Anisotropy;
#elif (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
	return &Material->Anisotropy;
#else
	return nullptr;
#endif
}
FExpressionInput* GetTangent( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->Tangent;
#elif (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
	return &Material->Tangent;
#else
	return nullptr;
#endif
}
FExpressionInput* GetWorldPositionOffset( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->WorldPositionOffset;
#else
	return &Material->WorldPositionOffset;
#endif
}
FExpressionInput* GetSubsurfaceColor( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->SubsurfaceColor;
#else
	return &Material->SubsurfaceColor;
#endif
}
FExpressionInput* GetClearCoat( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->ClearCoat;
#else
	return &Material->ClearCoat;
#endif
}
FExpressionInput* GetClearCoatRoughness( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->ClearCoatRoughness;
#else
	return &Material->ClearCoatRoughness;
#endif
}
FExpressionInput* GetAmbientOcclusion( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->AmbientOcclusion;
#else
	return &Material->AmbientOcclusion;
#endif
}
FExpressionInput* GetRefraction( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->Refraction;
#else
	return &Material->Refraction;
#endif
}
FExpressionInput* GetPixelDepthOffset( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->PixelDepthOffset;
#else
	return &Material->PixelDepthOffset;
#endif
}
FExpressionInput* GetShadingModelFromMaterialExpression( UMaterial* Material )
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
	return &Material->GetEditorOnlyData()->ShadingModelFromMaterialExpression;
#else
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
		return &Material->ShadingModelFromMaterialExpression;
	#else
		return nullptr;
	#endif
#endif
}
FExpressionInput* GetCustomizedUVs( UMaterial* Material, int i )
{
	if( i >= 8 )
		return nullptr;
	else
	{
	#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >=1
		return &Material->GetEditorOnlyData()->CustomizedUVs[ i ];
	#else
		return &Material->CustomizedUVs[ i ];
	#endif
	}
}
bool FixStrataStuff( std::string& GeneratedShader )
{
	//Strata Stuff
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#else
	return false;
#endif
	const char* StrataDataType = "FStrataData";
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
	StrataDataType = "FSubstrateData";
#endif
	std::string FindMarker = StrataDataType;
	FindMarker+= " ";
	char VarNameFormat[ 256 ];
	sprintf( VarNameFormat, "\t%s %%s =", StrataDataType );

	int StrataTypePos = GeneratedShader.find( FindMarker );
	if( StrataTypePos == -1 )
		return false;
	
	std::string Line = GetLine( GeneratedShader, StrataTypePos );
	char StrataVarName[ 256 ] = "";
	if( sscanf( Line.c_str(), VarNameFormat, StrataVarName ) != 1 )
		return false;

	CommentLine( GeneratedShader, "PixelMaterialInputs.FrontMaterial =" );
	//int NextLinePos = GeneratedShader.find( "\n", FrontMaterialPos );
	char StrataToMaterialInputs[ 256 ] = "";
	sprintf( StrataToMaterialInputs, "\n\tStrataToMaterialInputs( PixelMaterialInputs, %s );\n", StrataVarName );

	int MaterialEndPos = GeneratedShader.find( "//PixelMaterialInputs.FrontMaterial =" );
	if( MaterialEndPos == -1 )
		return false;

	GeneratedShader.insert( MaterialEndPos, StrataToMaterialInputs );

	return true;
}
void SetupShadingModel( std::string& GraphShader, EMaterialShadingModel ShadingModel )
{
	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );

	if( ShadingModel == MSM_Unlit )
	{
		if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
		{
			StringReplacement( GraphShader, "#define SHADERPASS SHADERPASS_FORWARD", "#define SHADERPASS SHADERPASS_UNLIT" );
			StringReplacement( GraphShader, "PBRForwardPass.hlsl", "UnlitPass.hlsl" );
			StringReplacement( GraphShader, "UniversalMaterialType\" = \"Lit", "UniversalMaterialType\" = \"Unlit" );
			
			//In Shadergraph/unlit mode basecolor is the only output, and in UE in unlit only emissive is written
			StringReplacement( GraphShader, "surface.BaseColor = BaseColor", "surface.BaseColor = Out.Emission" );
		}
		else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
		{
			StringReplacement( GraphShader, "HDLitShader", "HDUnlitShader" );
			StringReplacement( GraphShader, "HDLitSubTarget", "HDUnlitSubTarget" );
			StringReplacement( GraphShader, "Lit/Lit.hlsl", "Unlit/Unlit.hlsl" );
			StringReplacement( GraphShader, "Lit/LitRaytracing.hlsl", "Unlit/UnlitRaytracing.hlsl" );
			StringReplacement( GraphShader, "ShaderPassForward.hlsl", "ShaderPassForwardUnlit.hlsl" );
			StringReplacement( GraphShader, "surface.BaseColor = BaseColor", "surface.BaseColor = Out.Emission" );
			StringReplacement( GraphShader, "#define SHADERPASS SHADERPASS_FORWARD", "#define SHADERPASS SHADERPASS_FORWARD_UNLIT" );
			StringReplacement( GraphShader, "#define HAS_LIGHTLOOP 1",
				"#if defined(_ENABLE_SHADOW_MATTE) && (SHADERPASS == SHADERPASS_FORWARD_UNLIT || SHADERPASS == SHADERPASS_PATH_TRACING)\n"
				"#define LIGHTLOOP_DISABLE_TILE_AND_CLUSTER\n"
				"#define HAS_LIGHTLOOP\n"
				"#endif\n" );
			StringReplacement( GraphShader, "void BuildSurfaceData(FragInputs fragInputs, inout SurfaceDescription surfaceDescription, float3 V, PositionInputs posInput, out SurfaceData surfaceData, out float3 bentNormalWS)",
							   "void BuildSurfaceData(FragInputs fragInputs, inout SurfaceDescription surfaceDescription, float3 V, PositionInputs posInput, out SurfaceData surfaceData)" );
			StringReplacement( GraphShader, "surfaceData.baseColor =", "surfaceData.color =" );
			
			StringReplacement( GraphShader, "Name \"Forward\"", "Name \"ForwardOnly\"" );
			StringReplacement( GraphShader, "\"LightMode\" = \"Forward\"", "\"LightMode\" = \"ForwardOnly\"" );
			
			RemoveComment( GraphShader, "//#define SHADER_UNLIT" );
			CommentLine( GraphShader, "#include \"Packages/com.unity.render-pipelines.high-definition/Runtime/Lighting/Lighting.hlsl\"" );
			CommentLine( GraphShader, "#include \"Packages/com.unity.render-pipelines.high-definition/Runtime/Lighting/LightLoop/LightLoopDef.hlsl\"" );
			CommentLine( GraphShader, "#include \"Packages/com.unity.render-pipelines.high-definition/Runtime/Lighting/LightLoop/LightLoop.hlsl\"" );
			CommentLine( GraphShader, "#include \"Packages/com.unity.render-pipelines.high-definition/Runtime/Material/Decal/DecalUtilities.hlsl\"" );
			//CommentLine( GraphShader, "#include \"Packages/com.unity.render-pipelines.high-definition/Runtime/Material/StandardLit/StandardLit.hlsl\"" );
			CommentLine( GraphShader, "#include \"Packages/com.unity.render-pipelines.high-definition/Runtime/Material/Lit/LitDecalData.hlsl\"" );
			CommentLine( GraphShader, "#include \"Packages/com.unity.render-pipelines.high-definition/Runtime/Material/Lit/LitPathTracing.hlsl\"" );
			CommentLine( GraphShader, "#include \"Packages/com.unity.render-pipelines.high-definition/Runtime/RenderPipeline/Raytracing/Shaders/RaytracingLightLoop.hlsl\"" );
		
			CommentLine( GraphShader, "surfaceData.specularOcclusion =" );
			CommentLine( GraphShader, "surfaceData.ior =" );
			CommentLine( GraphShader, "surfaceData.transmittanceColor =" );
			CommentLine( GraphShader, "surfaceData.atDistance =" );
			CommentLine( GraphShader, "surfaceData.transmittanceMask =" );
			CommentLine( GraphShader, "surfaceData.materialFeatures =" );
			CommentLine( GraphShader, "surfaceData.geomNormalWS =" );
			CommentLine( GraphShader, "surfaceData.tangentWS =" );
			CommentLine( GraphShader, "surfaceData.perceptualSmoothness =" );
			CommentLine( GraphShader, "surfaceData.ambientOcclusion =" );
			CommentLine( GraphShader, "surfaceData.metallic =" );
			CommentLine( GraphShader, "bentNormalWS =" );
			CommentLine( GraphShader, "#pragma multi_compile_fragment DECALS_OFF DECALS_3RT DECALS_4RT" );
			CommentLine( GraphShader, "#pragma multi_compile _ WRITE_DECAL_BUFFER" );
			CommentLine( GraphShader, "#define HAVE_RECURSIVE_RENDERING" );
			CommentLine( GraphShader, "#define _SPECULAR_OCCLUSION_FROM_AO 1" );
			CommentLine( GraphShader, "#define _ENERGY_CONSERVING_SPECULAR 1" );
			CommentLine( GraphShader, "#define SHADER_LIT" );
		
		}
	}
}
TArray<FString> MaterialNames;
FString EnsureUniqueMatName( FString MatName )
{
	int Instances = 0;
	for( int i = 0; i < MaterialNames.Num(); i++ )
	{
		if( MaterialNames[ i ].Compare( MatName, ESearchCase::IgnoreCase ) == 0 )
		{
			Instances++;
		}
	}
	
	MaterialNames.Add( MatName );

	if( Instances == 0 )
	{
		return MatName;
	}
	else
	{
		FString Ret = FString::Printf( TEXT( "%s_%d" ), *MatName, Instances );
		return Ret;
	}
}
bool HasOnlyShadingModel( UMaterial* Mat, EMaterialShadingModel ShadingModel )
{
#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26)
	return Mat->GetShadingModel() == ShadingModel;
#else
	return Mat->GetShadingModels().HasOnlyShadingModel( ShadingModel );
#endif
}
bool HasStaticParametersInHierarchy( UMaterialInstance* MaterialInstance )
{
	FStaticParameterSet Set = MaterialInstance->GetStaticParameters();
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
	if (Set.StaticSwitchParameters.Num() > 0 ||
		Set.MaterialLayers.Layers.Num() > 0 ||
		Set.MaterialLayers.Blends.Num() > 0)
		return true;
#else
	return false;
#endif
	
	UMaterialInstance* ParentInstance = Cast<UMaterialInstance>( MaterialInstance->Parent );
	if (ParentInstance)
		return HasStaticParametersInHierarchy( ParentInstance );
	else
		return false;
}
bool CanHaveShader( MaterialBinding* Binding )
{
	if ( !Binding->MaterialInstance )
		return true;

	if ( HasStaticParametersInHierarchy( Binding->MaterialInstance ) )
		return true;
	auto BaseBlendMode = Binding->MaterialInstance->GetBaseMaterial()->GetBlendMode();
	auto BlendMode = Binding->MaterialInstance->GetBlendMode();
	if( BlendMode != BaseBlendMode )
		return true;
	return false;
}
void WriteShaderAndMeta( MaterialBinding* Binding, FString ShaderFileName, std::string GeneratedShader, std::string GraphShaderStr, std::string MatNameStr )
{
	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );

	if( UnitySettings->WithMaterialParameters())
	{
		if( !CanHaveShader( Binding ) )
			return;
	}
	std::string ShaderFileNameANSI = ToANSIString( *ShaderFileName );

	GraphShaderStr = NormalizeLineEndings( GraphShaderStr );
	GeneratedShader = NormalizeLineEndings( GeneratedShader );

	if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP || UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
	{
		SaveFile( ShaderFileNameANSI.c_str(), (void*)GraphShaderStr.c_str(), GraphShaderStr.length() );
		std::string MatNameHLSL = MatNameStr;
		FString MatNameHLSLWide = ToWideString( MatNameHLSL ).c_str();
		FString MatHLSLWide = GetOutputFile( TEXT( "Shaders/" ), MatNameHLSLWide, TEXT( ".hlsl" ) );

		std::string MatHLSLWideStr = ToANSIString( *MatHLSLWide );
		SaveFile( MatHLSLWideStr.c_str(), (void*)GeneratedShader.c_str(), GeneratedShader.length() );
	}
	else
		SaveFile( ShaderFileNameANSI.c_str(), (void*)GeneratedShader.c_str(), GeneratedShader.length() );
	Binding->UnityMat->Shader->FileName = ShaderFileNameANSI;

	Binding->UnityMat->Shader->GenerateGUID();
	std::string ShaderMetaContents = ExporterInterface->GenerateShaderMeta( Binding->UnityMat->Shader->GUID );
	std::string ShaderMetaFile = ShaderFileNameANSI;
	ShaderMetaFile += ".meta";
	SaveFile( ShaderMetaFile.c_str(), (void*)ShaderMetaContents.c_str(), ShaderMetaContents.length() );

	Binding->UnityMat->Shader->Contents = GeneratedShader;
}
void AddMaterialFunctionExpressions( UMaterialFunctionInterface* MF, TArray<UMaterialExpression*>& OutExpressions )
{
	TArray<UMaterialExpression*> FunctionExpressions = GetFunctionExpressions( MF );

	for (int u = 0; u < FunctionExpressions.Num(); u++)
	{
		auto Exp = FunctionExpressions[u];
		OutExpressions.Add( Exp );
	}
}
void ProcessMaterial_RenderThread( MaterialBinding *Binding, const char *ProxyShaderString, const char* GraphShaderString )
{
	#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
		//Shader source is always extracted on gamethread for 5.2+
	#else
		GetMaterialShaderSource( Binding, false );
	#endif


	bool UsesAlphaTest = false;
	bool IsTransparent = false;

	auto OpacityMaskExp = GetOpacityMask( Binding->BaseMaterial )->Expression;
	
	auto AttributesExp = GetMaterialAttributes( Binding->BaseMaterial)->Expression;
	if ( AttributesExp )
	{
		UMaterialExpressionMakeMaterialAttributes *MakeMaterialAttributes = dynamic_cast<UMaterialExpressionMakeMaterialAttributes*>( AttributesExp );
		if ( MakeMaterialAttributes )
		{
			OpacityMaskExp = MakeMaterialAttributes->OpacityMask.Expression;
		}		
	}
	
	EBlendMode BlendMode = Binding->BaseMaterial->BlendMode;
	if( Binding->MaterialInstance )
		BlendMode = Binding->MaterialInstance->GetBlendMode();

	if ( BlendMode == BLEND_Masked )
		UsesAlphaTest = true;
	if( BlendMode == BLEND_Translucent || BlendMode == BLEND_Additive || BlendMode == BLEND_Modulate )
		IsTransparent = true;

	EMaterialShadingModel ShadingModel = MSM_DefaultLit;
	
	if( HasOnlyShadingModel( Binding->BaseMaterial, MSM_Unlit ))
	{
		ShadingModel = MSM_Unlit;
	}

	std::string MaterialCollectionDefinitions;
	TArray<UMaterialExpression*> OutExpressions = GetExpressions( Binding->BaseMaterial );
	TArray<UMaterialParameterCollection*> Collections;
	TArray<bool> CollectionsInitializations;
	if( Binding->MaterialInstance )
	{
		GatherExpressionsFromMaterialLayers( Binding->MaterialInstance, OutExpressions );
	}
	GetMaterialCollectionIndices( Binding->BaseMaterial, Collections );
	for( int i = 0; i < Collections.Num(); i++ )
		CollectionsInitializations.Add( false );

	for( int i = 0; i < OutExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = OutExpressions[ i ];

		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( Exp );
		if( ExpMFC && ExpMFC->MaterialFunction )
		{
			AddMaterialFunctionExpressions( ExpMFC->MaterialFunction, OutExpressions );
			continue;
		}
		UMaterialExpressionMaterialAttributeLayers* MaterialAttributeLayers = Cast<UMaterialExpressionMaterialAttributeLayers>( Exp );
		if (MaterialAttributeLayers)
		{
			const TArray<UMaterialFunctionInterface*>& Layers = MaterialAttributeLayers->GetLayers();
			for (int l = 0; l < Layers.Num(); l++)
			{
				UMaterialFunctionInterface* MFI = Layers[l];
				if (MFI)
				{
					AddMaterialFunctionExpressions( MFI, OutExpressions );
				}
			}
			const TArray<UMaterialFunctionInterface*>& Blends = MaterialAttributeLayers->GetBlends();
			for (int b = 0; b < Blends.Num(); b++)
			{
				UMaterialFunctionInterface* MFI = Layers[b];
				if (MFI)
				{
					AddMaterialFunctionExpressions( MFI, OutExpressions );
				}
			}
			continue;
		}
		UMaterialExpressionCollectionParameter* ExpCollectionParameter = Cast<UMaterialExpressionCollectionParameter>( Exp );

		if( ExpCollectionParameter && ExpCollectionParameter->Collection )
		{
			int Index = Collections.Find( ExpCollectionParameter->Collection );
			bool WriteDefinition = false;
			if( Index == INDEX_NONE )
			{
				Index = Collections.Num();
				Collections.Add( ExpCollectionParameter->Collection );				
				WriteDefinition = true;
				CollectionsInitializations.Add( true );
			}
			else if( !CollectionsInitializations[ Index ] )
			{
				CollectionsInitializations[ Index ] = true;
				WriteDefinition = true;
			}
			if ( WriteDefinition )
			{
				const UMaterialParameterCollection* Collection = ExpCollectionParameter->Collection;
				int ShaderIndex = Index;
				FMaterialResource* MaterialResource = GetMaterialResource( Binding->BaseMaterial );
				FUniformExpressionSet_Override* Set_Override = GetUniformExpressionSet_Override( MaterialResource );
				if (Set_Override)
				{
					TMemoryImageArray<FGuid> ShaderCollections = Set_Override->GetParameterCollections();
					for (int c = 0; c < ShaderCollections.Num(); c++)
					{
						if (ShaderCollections[c] == Collection->StateId)
						{
							ShaderIndex = c;
							break;
						}
					}
				}
				
				std::string MatCollectionDef = GetMaterialCollectionString( Collection, ShaderIndex );
				MaterialCollectionDefinitions += MatCollectionDef;
			}
		}
	}

	FString MatName = Binding->GenerateName();
	FixUnicodeCharacters( MatName );
	MatName = EnsureUniqueMatName( MatName );

	if ( Binding->UnityMat->Shader->UnrealShaderSource.Len() > 0 )
	{
		UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	#ifdef ENABLE_GODOT_EXPORT
		UGodotSettingsClass* GodotSettings = Cast< UGodotSettingsClass>( ExporterInterface->Settings );
	#endif
		//Don't write shaders if using standard material
		if( UnitySettings && UnitySettings->Shaders == UnityShadersType::ST_STANDARD_SHADERS )
			return;
	#ifdef ENABLE_GODOT_EXPORT
		if( GodotSettings && GodotSettings->Shaders == GodotShadersType::ST_STANDARD_MATERIAL )
			return;
	#endif
		
		int Len = MatName.Len();
		if ( Len > 0 )
		{
			FString ShaderFileName;
			FString ShaderExtension = GetShaderExtension();
			FString ShaderResourcePath = FString::Printf( TEXT( "Shaders/%s%s" ), *MatName, *ShaderExtension );
			ShaderFileName = ExporterInterface->GetAssetsFolder() + ShaderResourcePath;
			
			FString UnrealHLSLFile = GetOutputFile( TEXT("Shaders/"), MatName, TEXT(".hlsl") );

			FString ShaderOutDir = ExporterInterface->GetAssetsFolder();
			ShaderOutDir += TEXT("Shaders/");
			VerifyOrCreateDirectory( ShaderOutDir );

			if( ExporterInterface->Type == EngineType::GODOT )
			{
				Binding->UnityMat->Shader->ResourcePath = ToANSIString( *ShaderResourcePath );
				ExporterInterface->GenerateShader( Binding, ShaderFileName );
				return;
			}

			FString ShaderSourceW = Binding->UnityMat->Shader->UnrealShaderSource;
			std::string ShaderSourceANSI = ToANSIString( *ShaderSourceW );
			const char *CSource = ShaderSourceANSI.c_str();

			//Debug original UE HLSL in case of issues
			bool DebugUnrealHLSL = false;
			if ( DebugUnrealHLSL )
				SaveFile( ToANSIString( *UnrealHLSLFile ).c_str(), (uint32* )CSource, strlen( CSource ) );
			const char *ptr = strstr( CSource, "void CalcPixelMaterialInputs" );
			if ( ptr )
			{
				int CropStart = ( int64_t )ptr - ( int64_t )CSource;
				const char *EndPtr = strstr( ptr, "}" );
				if ( !EndPtr )
					return;
				int CropEnd = ( int64_t )EndPtr - ( int64_t )CSource;

				int CropSize = CropEnd - CropStart + 1;
				if ( CropSize < 0 )
				{
					return;
				}
				char *CalcPixelMaterialInputsStr = new char[ CropSize ];
				CalcPixelMaterialInputsStr[ CropSize ] = 0;
				memcpy( CalcPixelMaterialInputsStr, ptr, CropSize );

				std::string CalcPixelMaterialInputsFinal = CalcPixelMaterialInputsStr;

				//Fix Normal related calculations
				int BracketStart = CalcPixelMaterialInputsFinal.find( "{" );
				CalcPixelMaterialInputsFinal.insert( BracketStart + 1, "\n\tfloat3 WorldNormalCopy = Parameters.WorldNormal;\n" );

				if( UnitySettings->TangentToWorldLocation == TangentToWorldLocationType::TTWL_BEFORE_NORMAL_CALCULATION )
					CalcPixelMaterialInputsFinal.insert( BracketStart + 1, TangentToWorldAxisFlip );

				const char *AfterNormalsMarker = "// Now the rest of the inputs";
				int AfterNormalCodeStart = CalcPixelMaterialInputsFinal.find( AfterNormalsMarker );

				if( AfterNormalCodeStart != -1 )
				{
					if ( UnitySettings->TangentToWorldLocation == TangentToWorldLocationType::TTWL_AFTER_NORMAL_CALCULATION )
						CalcPixelMaterialInputsFinal.insert( AfterNormalCodeStart + strlen(AfterNormalsMarker), TangentToWorldAxisFlip );
					//Attempt to change from a Z up computation to a Y up
					StringReplacement( CalcPixelMaterialInputsFinal, "dot(Parameters.WorldNormal,normalize(MaterialFloat3(0.00000000,0.00000000,1.00000000)))",
									   "dot(Parameters.WorldNormal,normalize(MaterialFloat3(0.00000000,1.00000000,0.00000000)))", AfterNormalCodeStart );
					if ( UnitySettings->TangentToWorldLocation != TangentToWorldLocationType::TTWL_KEEP_UNITY_VECTORS_NO_WORLDNORMALCOPY )
						StringReplacement( CalcPixelMaterialInputsFinal, "Parameters.WorldNormal", "WorldNormalCopy", AfterNormalCodeStart );
					
				}
				else
					UE_LOG( LogTemp, Error, TEXT( "[UTX] ERROR! AfterNormalCodeStart wasn't found !" ));
				//<-

				ExecuteStringReplacements( CalcPixelMaterialInputsFinal );
				
				std::string GeneratedShader = ProxyShaderString;
				std::string GraphShaderStr;
				if ( GraphShaderString )
					GraphShaderStr = GraphShaderString;

				const char *InsertMarker = "M_Chair";
				std::string MatNameStr = ToANSIString( *MatName );

				if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP || UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
				{
					ReplaceShaderName( GraphShaderStr, InsertMarker, MatNameStr );
				}
				else
				{
					ReplaceShaderName( GeneratedShader, InsertMarker, MatNameStr );
				}

				int NumTexcoordInterpolators = GetNumTexcoords( ShaderSourceANSI );
				SetupTexcoordInterpolators( ShaderSourceANSI, GeneratedShader );
				SetupVertexTexcoordInterpolators( ShaderSourceANSI, GeneratedShader );
				int Pos = -1;

				//Need to add alpha here if opacity/opacity mask is used ! + render que transparent
				//#pragma surface surf Standard

				//opacity = blending
				//opacity mask = alpha test
				
				if( Binding->MaterialInterface->IsTwoSided() )
				{
					std::string* TargetCode = &GeneratedShader;
					if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP || UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
						TargetCode = &GraphShaderStr;

					if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
					{
						StringReplacement( *TargetCode, "[HideInInspector]_CullMode(\"Float\", Float) = 2", "[HideInInspector]_CullMode(\"Float\", Float) = 0" );
						StringReplacement( *TargetCode, "[HideInInspector]_CullModeForward(\"Float\", Float) = 2", "[HideInInspector]_CullModeForward(\"Float\", Float) = 0" );
						StringReplacement( *TargetCode, "[HideInInspector][ToggleUI]_DoubleSidedEnable(\"Boolean\", Float) = 0", "[HideInInspector][ToggleUI]_DoubleSidedEnable(\"Boolean\", Float) = 1" );
					}
					else
					{
						const char* CullingMarker = "//Cull Off";

						while( 1 )
						{
							Pos = TargetCode->find( CullingMarker );
							if( Pos != TargetCode->npos )
								TargetCode->replace( Pos, 2, "" );//uncomment it
							else
								break;
						}
					}
				}

				std::string* TransparencyFixTarget = &GeneratedShader;
				if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP || UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
				{
					TransparencyFixTarget = &GraphShaderStr;
				}
				if( IsTransparent )
				{
					if ( BlendMode == BLEND_Additive )
						UncommentLine( *TransparencyFixTarget, "//BLEND_ADDITIVE" );
					else
						UncommentLine( *TransparencyFixTarget, "//BLEND_ON" );

					if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
					{
						StringReplacement( GraphShaderStr, "\"Queue\"=\"Geometry\"", "\"Queue\"=\"Transparent\"" );
					}
					else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
					{
						StringReplacement( GraphShaderStr, "_SurfaceType(\"Float\", Float) = 0", "_SurfaceType(\"Float\", Float ) = 1" );
						StringReplacement( GraphShaderStr, "\"Queue\"=\"Geometry+225\"", "\"Queue\"=\"Transparent+0\"" );

						if( BlendMode == BLEND_Additive )
							StringReplacement( GraphShaderStr, "_BlendMode(\"Float\", Float) = 0", "_BlendMode(\"Float\", Float ) = 1" );
						
					}
				}
				else
				{
					UncommentLine( *TransparencyFixTarget, "//BLEND_OFF" );
				}

				#if ENGINE_MAJOR_VERSION >= 5
					RemoveComment( GeneratedShader, "//#define UE5" );
				#else
					CommentLine( GeneratedShader, "#include \"LargeWorldCoordinates.hlsl\"" );
				#endif

				if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP)
				{
					RemoveComment( GeneratedShader, "//#define URP" );
				}
				else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
				{
					RemoveComment( GeneratedShader, "//#define HDRP" );
				}
				if ( UsesAlphaTest )
				{
					if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
					{
						RemoveComment( GeneratedShader, "//o.Alpha = PixelMaterialInputs.OpacityMask" );
						RemoveComment( GraphShaderStr, "//#pragma shader_feature_local _ _ALPHATEST_ON" );
						RemoveComment( GraphShaderStr, "//[HideInInspector][ToggleUI]_AlphaCutoffEnable(\"Boolean\", Float) = 1" );
					}
					else
						RemoveComment( GeneratedShader, "//if( PixelMaterialInputs.OpacityMask" );
				}

				int NumVectorExpressions = 0;
				int NumScalarExpressions = 0;
				std::string ParameterConfig = GetMaterialParameterConfig( ShaderSourceANSI );
				SanitizeParameterConfig( Binding, ParameterConfig );
				std::string ExpressionsData = GenerateInitializeExpressions( Binding, NumVectorExpressions, NumScalarExpressions, ParameterConfig );
				
				NumVectorExpressions = FMath::Max( NumVectorExpressions, 1 );
				NumScalarExpressions = FMath::Max( NumScalarExpressions, 1 );
				
				int NumScalarVectors = ( NumScalarExpressions + 3 ) / 4;
				
				GeneratedShader = AddNumberInString( GeneratedShader, "VectorExpressions[", NumVectorExpressions );
				GeneratedShader = AddNumberInString( GeneratedShader, "ScalarExpressions[", NumScalarVectors );
				#if ENGINE_MAJOR_VERSION >= 5
					StringReplacement( GeneratedShader, "VectorExpressions[", "PreshaderBuffer[" );
				#endif

				InsertMarker = "MaterialStruct Material;";
				Pos = GeneratedShader.find( InsertMarker );
				Pos += strlen( InsertMarker );

				GeneratedShader.insert( Pos + 1, ExpressionsData );				
				Pos += ExpressionsData.length();

				if( MaterialCollectionDefinitions.length() > 0 )
				{
					GeneratedShader.insert( Pos, MaterialCollectionDefinitions );
					Pos += MaterialCollectionDefinitions.length();

					int Offset = 0;
					while( 1 )//Do this so we initialize the data in VS and PS
					{
						InsertMarker = "InitializeExpressions();";
						int InitPos = GeneratedShader.find( InsertMarker, Offset );
						if( InitPos == -1 )
							break;
						InitPos += strlen( InsertMarker );

						std::string MaterialCollectionInits;
						for( int i = 0; i < Collections.Num(); i++ )
						{
							char Text[ 1024 ] = "";
							snprintf( Text, sizeof( Text ), "\n\tInitialize_MaterialCollection%d();\n", i );
							MaterialCollectionInits += Text;
						}

						GeneratedShader.insert( InitPos, MaterialCollectionInits );
						Offset = InitPos + MaterialCollectionInits.length();
						//Pos = Offset;
					}
				}

				if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
				{
					StringReplacement( GraphShaderStr, "ProxyURP", MatNameStr.c_str() );
				}
				else if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
				{
					StringReplacement( GraphShaderStr, "ProxyHDRP", MatNameStr.c_str() );
				}
				
				#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
					const char* VSMarker = "float3 GetMaterialWorldPositionOffsetRaw(";
				#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
					const char* VSMarker = "WPO_PRECISE(float3) GetMaterialWorldPositionOffset(";
				#else
					const char* VSMarker = "float3 GetMaterialWorldPositionOffset(";
				#endif

				std::string VSStr = ExtractFunction( CSource, VSMarker );

				#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
					StringReplacement( VSStr, "GetMaterialWorldPositionOffsetRaw", "GetMaterialWorldPositionOffset" );
				#endif
				ExecuteStringReplacements( VSStr );
				
				int VTPackedPageTableUniformPS = DetectMaxVariableSize( CalcPixelMaterialInputsFinal, "VTPackedPageTableUniform" );
				int VTPackedUniformPS = DetectMaxVariableSize( CalcPixelMaterialInputsFinal, "VTPackedUniform" );
				int VTPackedPageTableUniformVS = DetectMaxVariableSize( VSStr, "VTPackedPageTableUniform" );				
				int VTPackedUniformVS = DetectMaxVariableSize( VSStr, "VTPackedUniform" );

				int VTPackedPageTableUniformSize = FMath::Max( VTPackedPageTableUniformPS, VTPackedPageTableUniformVS ) + 1;
				int VTPackedUniformSize = FMath::Max( VTPackedUniformPS, VTPackedUniformVS ) + 1;
				int BeforeLength = GeneratedShader.length();
				GeneratedShader = AddNumberInString( GeneratedShader, "VTPackedPageTableUniform[", VTPackedPageTableUniformSize * 2 );
				GeneratedShader = AddNumberInString( GeneratedShader, "VTPackedUniform[", VTPackedUniformSize );

				if( GeneratedShader.length() > BeforeLength )
				{
					//Adjust pixel shader insertion point
					int Diff = GeneratedShader.length() - BeforeLength;
					Pos += Diff;
				}

				std::string CustomizedUVs;
				if( Binding->BaseMaterial->NumCustomizedUVs > 0 && NumTexcoordInterpolators > 0 )
				{
					CustomizedUVs = ExtractFunction( CSource, "void GetMaterialCustomizedUVs" );
					SetupCustomizedUVs( CustomizedUVs );
				}
				
				size_t posPS = CalcPixelMaterialInputsFinal.find( "CustomExpression" );
				size_t posVS = VSStr.find( "CustomExpression" );

				

				if( posPS != -1 || posVS != -1 )
				{
					if( UnitySettings->ExportVertexShader && UnitySettings->ScriptableRenderPipeline != RenderPipeline::RP_BUILTIN )
					{
						AddCustomExpressions( VSStr, CSource, true );
						if( CustomizedUVs.length() > 0 )
						{
							VSStr += CustomizedUVs;
						}
					}
					else
					{
						AddCustomExpressions( CalcPixelMaterialInputsFinal, CSource );
						if( CustomizedUVs.length() > 0 )
						{
							CalcPixelMaterialInputsFinal += CustomizedUVs;
						}
					}
				}
				else
				{
					if ( CustomizedUVs.length() > 0 )
						GeneratedShader.insert( Pos, CustomizedUVs );
				}

				GeneratedShader.insert( Pos, CalcPixelMaterialInputsFinal );

				GeneratePropertyFields( GeneratedShader, Binding, GraphShaderStr, VSStr, ParameterConfig );

				ProcessVertexInterpolators( ShaderSourceANSI, GeneratedShader );

				if( UnitySettings->ExportVertexShader && UnitySettings->ScriptableRenderPipeline != RenderPipeline::RP_BUILTIN )
				{
					int Offset = GeneratedShader.find( "void CalcPixelMaterialInputs" );
					if( Offset != -1 )
					{
						GeneratedShader.insert( Offset, VSStr );
						#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
							GeneratedShader.insert( Offset, "\n#define WPO_PRECISE(T) T\n" );
						#endif
					}

					
					RemoveComment( GraphShaderStr, "//float3 WPO = PrepareAndGetWPO" );
					CommentLine( GraphShaderStr, "float3 WPO = float3( 0, 0, 0 );" );
				}
				else
				{
					CommentLine( GeneratedShader, "Offset = GetMaterialWorldPositionOffset" );
				}
				if( CustomizedUVs.length() > 0 )
				{
					RemoveComment( GeneratedShader, "//#define HAS_CUSTOMIZED_UVS" );
				}

				bool IsStrataMaterial = FixStrataStuff( GeneratedShader );
				if( Binding->BaseMaterial->bTangentSpaceNormal == 0 || IsStrataMaterial )
				{
					StringReplacement( GeneratedShader, "HAS_WORLDSPACE_NORMAL 0", "HAS_WORLDSPACE_NORMAL 1" );
				}

				SetupShadingModel( GraphShaderStr, ShadingModel );

				WriteShaderAndMeta( Binding, ShaderFileName, GeneratedShader, GraphShaderStr, MatNameStr );
			}			
		}		
	}	
}

std::vector< MeshBinding* > MeshList;


MeshBinding* GetMeshBinding( UnityMesh* Mesh )
{
	for( int i = 0; i < MeshList.size(); i++ )
	{
		if( MeshList[ i ]->TheUnityMesh == Mesh )
			return MeshList[ i ];
	}

	return nullptr;
}
int GetMeshVariantIndex( UPrimitiveComponent* Comp )
{
	UStaticMeshComponent* SMC = Cast< UStaticMeshComponent>( Comp );
	if( !SMC || !SMC->GetStaticMesh())
		return -1;
	
	int Index = 0;
	for( int i = 0; i < MeshList.size(); i++ )
	{
		if( MeshList[ i ]->UnrealStaticMesh == SMC->GetStaticMesh() )
			Index++;
	}

	return Index;
}
const FStaticMeshRenderData* GetRenderData( const UStaticMesh* StaticMesh )
{
	if( !StaticMesh )
		return nullptr;
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		const FStaticMeshRenderData* MeshRenderData = StaticMesh->GetRenderData();
	#else
		const FStaticMeshRenderData* MeshRenderData = StaticMesh->RenderData.Get();
	#endif

	return MeshRenderData;
}
UBodySetup* GetBodySetup( const UStaticMesh* StaticMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	#else
		UBodySetup* BodySetup = StaticMesh->BodySetup;
	#endif

	return BodySetup;
}
const USkeleton* GetSkeleton( const USkeletalMesh* SkeletalMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		const USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	#else
		const USkeleton* Skeleton = SkeletalMesh->Skeleton;
	#endif

	return Skeleton;
}
TArray<UMorphTarget*> GetMorphTargets( const USkeletalMesh* SkeletalMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		TArray<UMorphTarget*> MorphTargets = SkeletalMesh->GetMorphTargets();
	#else
		TArray<UMorphTarget*> MorphTargets = SkeletalMesh->MorphTargets;
	#endif

	return MorphTargets;
}
const TArray<FSkeletalMaterial>& GetMaterials( const USkeletalMesh* SkeletalMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
	#else
		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->Materials;
	#endif

	return Materials;
}
bool RequiresNewMeshDueToVertexColors( UPrimitiveComponent* MeshComp, int LOD )
{
	auto StaticMeshComp = Cast< UStaticMeshComponent>( MeshComp );
	if (!StaticMeshComp )
		return false;

	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings && UnitySettings->ExportVertexColors )
	{
		if( StaticMeshComp->LODData.Num() > LOD )
		{
			if( StaticMeshComp->LODData[ LOD ].OverrideVertexColors )
			{
				return true;
			}
		}
	}
	if( ExporterInterface->Type == EngineType::GODOT )
	{
		//Experimental
		//return true;
	}

	return false;
}
UnityMesh * GetUnityMesh( const UStaticMesh *UnrealStaticMesh, USkeletalMesh* SkeletalMesh,
						  const char *Name, int LOD, UPrimitiveComponent* MeshComp )
{
	for( int i = 0; i < MeshList.size(); i++ )
	{
		MeshBinding* M = MeshList[ i ];
		bool HasSameLOD = ( M->LOD == LOD );
		if( CVarFBXPerActor.GetValueOnAnyThread() )
			HasSameLOD = true;
		bool SameComponent = true;
		//If true, it has a unique mesh for vertex colors
		if( MeshComp )
		{
			SameComponent = ( MeshComp == M->MeshComp );
		}
		bool SameMesh = true;
		if( SkeletalMesh )
		{
			SameMesh = ( M->UnrealSkeletalMesh == SkeletalMesh );
		}
		else
		{
			SameMesh = (M->UnrealStaticMesh == UnrealStaticMesh && M->TheUnityMesh->Name.compare( Name ) == 0);
		}
		if( SameMesh && HasSameLOD && SameComponent )
		{
			return M->TheUnityMesh;
		}
	}

	MeshBinding *NewMeshBinding = new MeshBinding;
	NewMeshBinding->TheUnityMesh = new UnityMesh;
	NewMeshBinding->TheUnityMesh->Name = Name;
	NewMeshBinding->LOD = LOD;
	NewMeshBinding->UnrealStaticMesh = ( UStaticMesh*)UnrealStaticMesh;
	NewMeshBinding->UnrealSkeletalMesh = SkeletalMesh;
	
	if( UnrealStaticMesh )
	{
		const FStaticMeshRenderData* MeshRenderData = GetRenderData( (UStaticMesh*)UnrealStaticMesh );
		NewMeshBinding->TotalLods = MeshRenderData->LODResources.Num();
	}
	else if( SkeletalMesh )
	{
		NewMeshBinding->TotalLods = SkeletalMesh->GetResourceForRendering()->LODRenderData.Num();
	}

	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings && !UnitySettings->ExportLODs )
	{
		NewMeshBinding->TotalLods = 1;
	}

	NewMeshBinding->MeshComp = MeshComp;

	//Generate Mesh File output
	FString MeshFile;

	if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
	{
		if ( NewMeshBinding->UnrealStaticMesh )
			MeshFile += GetAssetPathFolder( NewMeshBinding->UnrealStaticMesh );
		else if( NewMeshBinding->UnrealSkeletalMesh )
			MeshFile += GetAssetPathFolder( NewMeshBinding->UnrealSkeletalMesh );
	}
	else
	{
		MeshFile += TEXT("Meshes/");
	}

	FString NameWide = ToWideString( Name ).c_str();
    MeshFile += NameWide;
	if( MeshComp && RequiresNewMeshDueToVertexColors( MeshComp, LOD ) )
	{
		char Text[ 32 ];
		int VariantIndex = GetMeshVariantIndex( MeshComp );
		snprintf( Text, sizeof( Text ), "@VertexColors_%d", VariantIndex );// MeshComp );
		FString TextWide = ToWideString( Text ).c_str();
		MeshFile += TextWide;
	}
	
	MeshFile += GetMeshExtension();

	NewMeshBinding->TheUnityMesh->ResourcePath = ToANSIString( *MeshFile );
	MeshFile = ExporterInterface->GetAssetsFolder() + MeshFile;
	CreateAllDirectories( MeshFile );
	NewMeshBinding->TheUnityMesh->File = ToANSIString( *MeshFile );

	MeshList.push_back( NewMeshBinding );
	return NewMeshBinding->TheUnityMesh;
}
FString GenerateAnimationFilePath( const UAnimSequence* Anim )
{
	FString Path = ExporterInterface->GetAssetsFolder();

	if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
	{		
		Path += GetAssetPathFolder( Anim );
	}
	else
	{
		Path += TEXT( "Meshes/" );
	}

	CreateAllDirectories( Path );
	Path += *Anim->GetName();

	Path += GetMeshExtension();

	return Path;
}
void ProcessSkeletalMesh( USkeletalMeshComponent *SkeletalMeshComponent )
{
	MaterialBinding **MaterialsUsed = nullptr;
	TArray<UMaterialInterface*> Materials = SkeletalMeshComponent->GetMaterials();

	MaterialsUsed = new MaterialBinding*[ Materials.Num()];
	for( int i = 0; i < Materials.Num(); i++ )
		MaterialsUsed[ i ] = nullptr;
	
	UObject* LoadedObject = StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, TEXT( "/Engine/EngineMaterials/DefaultMaterial" ) );
	UMaterialInterface* DefaultMaterial = Cast<UMaterialInterface>( LoadedObject );
	if( Materials.Num() > 0 )
	{
		for( int i = 0; i < Materials.Num(); i++ )
		{
			MaterialsUsed[ i ] = nullptr;
			UMaterialInterface *MaterialInterface = Materials[ i ];
			if( !MaterialInterface )
			{
				Materials[ i ] = DefaultMaterial;
			}

			MaterialsUsed[ i ] = ProcessMaterialReference( MaterialInterface );
		}
	}
}
void MoveComponentsToNewGO( GameObject* GO )
{
	GameObject* NewGameObject = new GameObject;
	
	for( int i = 0; i < GO->Components.size(); i++ )
	{
		UnityComponent* Comp = GO->Components[ i ];
		
		{
			NewGameObject->Components.push_back( Comp );
			Comp->Owner = NewGameObject;

			GO->Components.erase( GO->Components.begin() + i );
			i--;
		}
	}
	
	{
		MeshRenderer* MR = ( MeshRenderer * )NewGameObject->GetComponent( CT_MESHRENDERER );
		if ( MR )
			NewGameObject->Name = MR->Name;

		GO->AddChild( NewGameObject );
		GO->OwnerScene->Add( NewGameObject );
	}	
}
MaterialBinding** GetMaterialsUsed( UStaticMeshComponent* StaticMeshComp, USkeletalMeshComponent* SkeletalMeshComponent,
									bool GetComponentMaterials, int& NumMaterials );
bool BaseMeshHasDuplicateMaterials( MaterialBinding** MeshMaterials, int MaxMeshMaterials )
{
	for( int i = 0; i < MaxMeshMaterials - 1; i++ )
		for( int u = i + 1; u < MaxMeshMaterials; u++ )
		{
			if( MeshMaterials[ i ]->UnityMat == MeshMaterials[ u ]->UnityMat )
			{
				return true;
			}
		}

	return false;
}
void CreateBoneGameObjects( UnityMesh* Mesh, USkeletalMesh* UnrealSkeletalMesh, GameObject* ParentGO )
{
	#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 27) || ENGINE_MAJOR_VERSION == 5
		const FReferenceSkeleton& RefSkeleton = UnrealSkeletalMesh->GetRefSkeleton();
	#else
		const FReferenceSkeleton& RefSkeleton = UnrealSkeletalMesh->RefSkeleton;
	#endif
	

	if( RefSkeleton.GetRawBoneNum() == 0 )
	{
		return;
	}

	int NumBones = RefSkeleton.GetRawBoneNum();
	GameObject** BoneGameObjects = new GameObject*[ NumBones ];
	//Get all names

	for( int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex )
	{
		const FMeshBoneInfo& CurrentBone = RefSkeleton.GetRefBoneInfo()[ BoneIndex ];
		FTransform& BoneTransform = (FTransform&)RefSkeleton.GetRefBonePose()[ BoneIndex ];

		std::string NewBoneName = ToANSIString( *CurrentBone.ExportName );
		Mesh->BoneNames.push_back( NewBoneName );

		BoneGameObjects[ BoneIndex ] = new GameObject();
		BoneGameObjects[ BoneIndex ]->Name = NewBoneName;

		TransformComponent* TC = ( TransformComponent*)BoneGameObjects[ BoneIndex ]->GetComponent( CT_TRANSFORM );
		ConvertTransformForBones( BoneTransform, TC, BoneIndex == 0 );

		if( BoneIndex )
		{
			BoneGameObjects[ CurrentBone.ParentIndex ]->AddChild( BoneGameObjects[ BoneIndex ] );
		}
		else
			ParentGO->AddChild( BoneGameObjects[ BoneIndex ] );
	}
}
bool AreMaterialsUnique( TArray<UMaterialInterface*> Materials )
{
	for( int i = 0; i < Materials.Num() - 1; i++ )
	{
		for( int u = i + 1; u < Materials.Num(); u++ )
		{
			if( Materials[ i ] == Materials[ u ] )
				return false;
		}
	}

	return true;
}
bool HasUniqueMaterials( UStaticMeshComponent* StaticMeshComp )
{
	TArray<UMaterialInterface*> Materials = StaticMeshComp->GetMaterials();
	return AreMaterialsUnique( Materials );
}
TArray<UActorComponent*> AllComponentsInScene;
UStaticMeshComponent* FindComponentWithUniqueMaterials( UStaticMeshComponent* StaticMeshComp )
{
	for( int i = 0; i< AllComponentsInScene.Num(); i++ )
	{
		UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>( AllComponentsInScene[i] );
		if( Comp && StaticMeshComp && Comp->GetStaticMesh() == StaticMeshComp->GetStaticMesh() &&
			Comp->GetOwner() != nullptr && Comp->GetClass() == UStaticMeshComponent::StaticClass() )
		{
			TArray<UMaterialInterface*> Materials = Comp->GetMaterials();
			if( AreMaterialsUnique( Materials ) )
			{
				return Comp;
			}
		}
	}

	return nullptr;
}
USkeletalMesh* GetSkeletalMesh( USkinnedMeshComponent* SMC )
{
	#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 2 )
		return SMC->SkeletalMesh;
	#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>( SMC );
		if( SkeletalMeshComp )
			return SkeletalMeshComp->GetSkeletalMeshAsset();
		else
			return nullptr;
	#endif
}
UAnimSequence* GetAnimationSequence( USkeletalMeshComponent* SkeletalMeshComp )
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>( SkeletalMeshComp->AnimationData.AnimToPlay );
	if( !AnimSequence )
	{
		UAnimationAsset* AnimationAsset = nullptr;
		if ( SkeletalMeshComp->GetSingleNodeInstance() )
			AnimationAsset = SkeletalMeshComp->GetSingleNodeInstance()->GetAnimationAsset();
		AnimSequence = Cast<UAnimSequence>( AnimationAsset );
	}
	return AnimSequence;
}
Renderer* ExportLOD( UStaticMeshComponent* StaticMeshComp, USkeletalMeshComponent* SkeletalMeshComp,
					 GameObject* ComponentGO, AActor* Actor, int LOD, int TotalLODs )
{
	const UStaticMesh* StaticMesh = nullptr;
	const FStaticMeshLODResources* LODResources = nullptr;
	if( StaticMeshComp )
	{
		StaticMesh = StaticMeshComp->GetStaticMesh();
		LODResources = &GetRenderData( StaticMesh )->LODResources[ LOD ];
	}
	USkeletalMesh* SkeletalMesh = nullptr;
	FSkeletalMeshLODRenderData* SkeletalLODRenderData = nullptr;
	if( SkeletalMeshComp )
	{
		SkeletalMesh = GetSkeletalMesh( SkeletalMeshComp );
		SkeletalLODRenderData = &SkeletalMesh->GetResourceForRendering()->LODRenderData[LOD];
	}

	FString OriginalMeshName;
	FString SourceFile;
	if( StaticMeshComp )
	{
		OriginalMeshName = StaticMesh->GetName();
		SourceFile = StaticMesh->GetName();		
	}
	else if( SkeletalMeshComp )
	{
		OriginalMeshName = SkeletalMesh->GetName();
		SourceFile = SkeletalMesh->GetName();
	}

	OriginalMeshName = FixUnicodeCharacters( OriginalMeshName );
	SourceFile = FixUnicodeCharacters( SourceFile );

	if( SourceFile.Len() == 0 )
	{
		SourceFile = Actor->GetName();
	}

	if( SourceFile.Len() > 0 && LOD > 0 && !CVarFBXPerActor.GetValueOnAnyThread() )
	{
		SourceFile = FString::Printf( TEXT( "%s_LOD%d" ), *SourceFile, LOD );
	}

	std::string Name;
	Name = ToANSIString( *SourceFile );

	UStaticMeshComponent* CompareComp = nullptr;

	if( RequiresNewMeshDueToVertexColors( StaticMeshComp, LOD ) )
	{
		//ComponentGO->CanBePrefab = false;
		//if( ComponentGO->Parent )
			//ComponentGO->Parent->CanBePrefab = false;
		CompareComp = StaticMeshComp;
	}

	UnityMesh* NewMesh = GetUnityMesh( StaticMesh, SkeletalMesh, Name.c_str(), LOD, CompareComp);
	MeshBinding* Binding = GetMeshBinding( NewMesh );
	if( Binding )
	{
		if( StaticMeshComp )
		{
			Binding->OwnerActor = StaticMeshComp->GetOwner();
			TArray<UMaterialInterface*> Materials = StaticMeshComp->GetMaterials();
			if( !AreMaterialsUnique( Materials ) && !CompareComp )
			{
				//Choose an actor with different materials so material slots don't get merged in Unity
				auto SomeOtherComponent = FindComponentWithUniqueMaterials( StaticMeshComp );
				if( SomeOtherComponent )
					Binding->OwnerActor = SomeOtherComponent->GetOwner();
			}
		}
		else if( SkeletalMeshComp )
		{
			Binding->OwnerActor = SkeletalMeshComp->GetOwner();
			if( SkeletalMeshComp->GetAnimationMode() == EAnimationMode::AnimationSingleNode )
			{
				Binding->AnimSequences.Add( GetAnimationSequence(SkeletalMeshComp) );
				if( SkeletalMesh )
				{
					auto OverrideMaterials = GetArrayOfMaterials( SkeletalMeshComp->OverrideMaterials );
					AnimationEntry* Entry = AddAnimationEntry( SkeletalMesh, Binding->AnimSequences[0], *SkeletalMesh->GetName(), &OverrideMaterials);
					if ( Binding->AnimSequences[ 0 ] )
					{
						AnimationComponent* AnimComp = (AnimationComponent*)ComponentGO->GetComponent( CT_ANIMATION );
						if (AnimComp)
						{
							AnimComp->AnimationGUID = Entry->GUID;
						}
					}
				}
			}
		}
	}

	int MaxMeshMaterials = 0;
	MaterialBinding** MeshMaterials = GetMaterialsUsed( StaticMeshComp, SkeletalMeshComp, false, MaxMeshMaterials );

	if( NewMesh->Sections.size() == 0 )
	{
		if( LODResources )
		{
			for( int i = 0; i < LODResources->Sections.Num(); i++ )
			{
				FStaticMeshSection Section = LODResources->Sections[ i ];
				MeshSection* NewMeshSection = new MeshSection;
				NewMeshSection->MaterialIndex = Section.MaterialIndex;
				NewMeshSection->IndexOffset = Section.FirstIndex;
				NewMeshSection->NumIndices = Section.NumTriangles * 3;

				NewMesh->Sections.push_back( NewMeshSection );
			}
		}
		else if( SkeletalLODRenderData )
		{
			for( int i = 0; i < SkeletalLODRenderData->RenderSections.Num(); i++ )
			{
				FSkelMeshRenderSection* Section = &SkeletalLODRenderData->RenderSections[ i ];
				MeshSection* NewMeshSection = new MeshSection;
				NewMeshSection->MaterialIndex = Section->MaterialIndex;
				NewMeshSection->IndexOffset = Section->BaseIndex;
				NewMeshSection->NumIndices = Section->NumTriangles * 3;

				NewMesh->Sections.push_back( NewMeshSection );
			}
		}
		
		if( CVarFBXPerActor.GetValueOnAnyThread() == 0 )
		{
			int NumVertices = LODResources->VertexBuffers.PositionVertexBuffer.GetNumVertices();
			int NumTexcoordChannels = LODResources->VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
			int NumIndices = LODResources->IndexBuffer.GetNumIndices();

			uint32* Indices = new uint32[ NumIndices ];
			FVector* Vertices = new FVector[ NumVertices ];
			FVector* Normals = new FVector[ NumVertices ];
			FVector4* Tangents = new FVector4[ NumVertices ];
			FVector2D* Texcoords = new FVector2D[ NumVertices ];
			FVector2D* Texcoords1 = nullptr;
			if( NumTexcoordChannels > 1 )
				Texcoords1 = new FVector2D[ NumVertices ];
			FColor* Colors = nullptr;

			if( NumIndices > 0 )
			{
				FIndexArrayView ArrayView = LODResources->IndexBuffer.GetArrayView();

				for( int i = 0; i < NumIndices; i++ )
				{
					uint32 Index32 = ArrayView[ i ];
					Indices[ i ] = Index32;

					if( LODResources->IndexBuffer.Is32Bit() )
					{

					}
				}

				TotalTriangles += NumIndices / 3;
			}
			else
				TotalTriangles += NumVertices / 3;

			for( int i = 0; i < LODResources->Sections.Num(); i++ )
			{
				FStaticMeshSection Section = LODResources->Sections[ i ];
				int MatIndex = Section.MaterialIndex;
				int VertexStart = Section.MinVertexIndex;
				int VertexEnd = Section.MaxVertexIndex;
				int IndexStart = Section.FirstIndex;
				int NumTriangles = Section.NumTriangles;
			}

			TArray<FColor> ColorArray;

			UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
			if( UnitySettings && UnitySettings->ExportVertexColors )
			{
				if( StaticMeshComp->LODData.Num() > LOD )
				{
					if( StaticMeshComp->LODData[ LOD ].OverrideVertexColors )
					{
						StaticMeshComp->LODData[ LOD ].OverrideVertexColors->GetVertexColors( ColorArray );
					}
				}

				if( ColorArray.Num() == 0 )
				{
					LODResources->VertexBuffers.ColorVertexBuffer.GetVertexColors( ColorArray );
				}
				if( ColorArray.Num() > 0 && ColorArray.Num() == NumVertices )
				{
					Colors = new FColor[ ColorArray.Num() ];
				}
			}
			for( int i = 0; i < NumVertices; i++ )
			{
				FVector Pos = (FVector)LODResources->VertexBuffers.PositionVertexBuffer.VertexPosition( i );
				//Scale down from Unreal's cm units to Unity's m
				Pos /= 100.0f;

				FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 0, 0 ) );

				if( 1 )
				{
					Pos = Rotator.RotateVector( Pos );
				}
				if( CVarUsePostImportTool.GetValueOnAnyThread() == 0 )
				{
					Pos.X *= -1;
				}

				Vertices[ i ] = Pos;
			}

			NewMesh->Vertices = Vertices;
			NewMesh->Normals = Normals;
			NewMesh->Tangents = Tangents;
			NewMesh->Texcoords = Texcoords;
			NewMesh->Texcoords1 = Texcoords1;
			NewMesh->Colors = Colors;
			NewMesh->AllIndices = Indices;
			NewMesh->NumVertices = NumVertices;
			NewMesh->NumIndices = NumIndices;
		}

		for( int i = 0; i < MaxMeshMaterials; i++ )
		{
			if( MeshMaterials[ i ] )
				NewMesh->Materials.push_back( MeshMaterials[ i ]->UnityMat );
			else
				NewMesh->Materials.push_back( nullptr );
		}
	}
		
	FString  LodNameStr = OriginalMeshName;
	if( TotalLODs > 1 )
		LodNameStr = FString::Printf( TEXT( "%s_LOD%d" ), *OriginalMeshName, LOD );

	std::string LodName = ToANSIString( *LodNameStr );

	MeshRenderer* MR = nullptr;
	MeshFilter* MF = nullptr;
	SkinnedMeshRenderer* SMR = nullptr;

	GameObject* LODGO = new GameObject;
	ComponentGO->AddChild( LODGO );
	ComponentGO->OwnerScene->Add( LODGO );
	
	if( StaticMeshComp )
	{
		MR = (MeshRenderer*)LODGO->AddComponent( CT_MESHRENDERER );
		MF = (MeshFilter*)LODGO->AddComponent( CT_MESHFILTER );
	}
	else if( SkeletalMeshComp )
	{
		SMR = (SkinnedMeshRenderer*)LODGO->AddComponent( CT_SKINNEDMESHRENDERER );
		SMR->LOD = LOD;
		SMR->TotalLods = TotalLODs;
		auto Bounds = GetSkeletalMesh( SkeletalMeshComp )->GetBounds();
		SMR->Center = Bounds.Origin / 100.0f;
		SMR->Extent = Bounds.BoxExtent / 100.0f;
		if( SMR->Owner->Parent )
		{
			TransformComponent* ParentTransform = (TransformComponent*)SMR->Owner->Parent->GetComponent( CT_TRANSFORM );
			if( ParentTransform )
			{
				//Undo rotation fix on all actors
				FQuat Quat( ParentTransform->LocalRotation[ 0 ], ParentTransform->LocalRotation[ 1 ], ParentTransform->LocalRotation[ 2 ],
							ParentTransform->LocalRotation[ 3 ] );
				static FVector RotUndoEuler( 0, 90, 0 );
				FQuat QuatUndo = FQuat::MakeFromEuler( RotUndoEuler );
				Quat = QuatUndo * Quat;//An identity rotation should be an identity quat after this point
				ParentTransform->LocalRotation[ 0 ] = Quat.X;
				ParentTransform->LocalRotation[ 1 ] = Quat.Y;
				ParentTransform->LocalRotation[ 2 ] = Quat.Z;
				ParentTransform->LocalRotation[ 3 ] = Quat.W;
			}
		}
		if ( LOD == 0 )
			CreateBoneGameObjects( NewMesh, Binding->UnrealSkeletalMesh, ComponentGO );
	}

	LODGO->Name = LodName;

	if( MF )
		MF->Mesh = NewMesh;
	else if( SMR )
		SMR->Mesh = NewMesh;

	//Only FBX has different FileID for every LOD
	if( CVarFBXPerActor.GetValueOnAnyThread() == 1 )
	{
		if( MF )
		{
			if( TotalLODs > 1 )
				MF->SubMeshID = 4300000 + LOD * 2;
			else
				MF->SubMeshID = 4300000;
		}
		else if( SMR )
		{
			if( TotalLODs > 1 )
				SMR->SubMeshID = 4300000 + LOD * 2;
			else
				SMR->SubMeshID = 4300000;
		}
	}

	if( MR )
	{
		MR->Name = LodName;
		if( StaticMeshComp )
			MR->CastShadows = StaticMeshComp->CastShadow;
	}
	else if( SMR )
	{
		if( SkeletalMeshComp )
			SMR->CastShadows = SkeletalMeshComp->CastShadow;
	}

	int MaxMaterials = 0;
	MaterialBinding** MaterialsUsed = GetMaterialsUsed( StaticMeshComp, SkeletalMeshComp, true, MaxMaterials );
	//bool HasDuplicateMaterials = BaseMeshHasDuplicateMaterials( MeshMaterials, MaxMeshMaterials );
	//Add materials based on that section's MaterialIndex
	if( LODResources )
	{
		for( int i = 0; i < LODResources->Sections.Num(); i++ )
		{
			FStaticMeshSection Section = LODResources->Sections[ i ];
			if( Section.MaterialIndex >= 0 && Section.MaterialIndex < MaxMaterials )
			{
				MaterialBinding* MatBinding = MaterialsUsed[ Section.MaterialIndex ];
				if( MatBinding )
				{
					MR->AddMaterial( MatBinding->UnityMat, false );// HasDuplicateMaterials );
				}
				else
					MR->AddMaterial( nullptr, false );// HasDuplicateMaterials );
			}
		}
	}
	else if ( SkeletalLODRenderData )
	{
		for( int i = 0; i < SkeletalLODRenderData->RenderSections.Num(); i++ )
		{
			FSkelMeshRenderSection* Section = &SkeletalLODRenderData->RenderSections[ i ];
			if( Section->MaterialIndex >= 0 && Section->MaterialIndex < MaxMaterials )
			{
				MaterialBinding* MatBinding = MaterialsUsed[ Section->MaterialIndex ];
				if( MatBinding )
				{
					SMR->AddMaterial( MatBinding->UnityMat, false );// HasDuplicateMaterials );
				}
				else
					SMR->AddMaterial( nullptr, false );// HasDuplicateMaterials );
			}
		}
	}

	if( MR )
		return MR;
	else// if( SMR )
		return SMR;
}
int GetNumRelevantComponents( AActor* A )
{
	TArray<UActorComponent*> comps;
	A->GetComponents( comps );
	int Num = 0;
	for( int i = 0; i < comps.Num(); i++ )
	{
		UActorComponent* AC = comps[ i ];
		ULightComponent* LightComp = dynamic_cast<ULightComponent*>( AC );
		if( LightComp )
		{
			Num++;
		}
		USkeletalMeshComponent* SkelMeshComp = dynamic_cast<USkeletalMeshComponent*>( AC );
		if( SkelMeshComp )
		{
			Num++;
		}
		USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( AC );
		if( SplineMeshComponent )
		{
			
		}
		UStaticMeshComponent* StaticMeshComp = dynamic_cast<UStaticMeshComponent*>( AC );
		if( StaticMeshComp )
		{
			Num++;
		}		
		USphereReflectionCaptureComponent* SphereReflection = Cast<USphereReflectionCaptureComponent>( AC );
		if( SphereReflection )
		{
			Num++;
		}
		UBoxReflectionCaptureComponent* BoxReflection = Cast<UBoxReflectionCaptureComponent>( AC );
		if( BoxReflection )
		{
			Num++;
		}
		UDecalComponent* DecalComponent = Cast<UDecalComponent>( AC );
		if( DecalComponent )
		{			
			Num++;
		}
	}

	return Num;
}
void RotateForYUp( FVector& Vector )
{
	FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 0, 0 ) );	
	Vector = Rotator.RotateVector( Vector );	
}
void SwitchYWithZ( FVector& Vector )
{
	float Temp = Vector.Y;
	Vector.Y = Vector.Z;
	Vector.Z = Temp;
}
void ConvertScaleForUnity( FVector& Vector )
{
	float Temp = Vector.Y;
	Vector.Y = Vector.Z;
	Vector.Z = Temp;

	Temp = Vector.Z;
	Vector.Z = Vector.X;
	Vector.X = Temp;
}
MaterialBinding** GetMaterialsUsed( UStaticMeshComponent* StaticMeshComp, USkeletalMeshComponent* SkeletalMeshComponent,
									bool GetComponentMaterials, int & NumMaterials )
{
	TArray<UMaterialInterface*> OutMaterials;
	
	if( StaticMeshComp )
		NumMaterials = StaticMeshComp->GetNumMaterials();
	if( SkeletalMeshComponent )
		NumMaterials = SkeletalMeshComponent->GetNumMaterials();
	for( int i = 0; i < NumMaterials; i++ )
	{
		UMaterialInterface* Mat = nullptr;
		if( StaticMeshComp )
		{
			if ( GetComponentMaterials )
				Mat = StaticMeshComp->GetMaterial( i );
			else
				Mat = StaticMeshComp->GetStaticMesh()->GetMaterial( i );
		}
		if( SkeletalMeshComponent )
		{
			if ( GetComponentMaterials )
				Mat = SkeletalMeshComponent->GetMaterial( i );
			else
				Mat = GetMaterials( GetSkeletalMesh( SkeletalMeshComponent ) )[i].MaterialInterface;
		}
		OutMaterials.Add( Mat );
	}
	NumMaterials = OutMaterials.Num();
	//Save a mesh with unique materials so they don't get merged in unity
	if( !GetComponentMaterials && !AreMaterialsUnique( OutMaterials ) )
	{
		for( int i = 0; i < AllComponentsInScene.Num(); i++ )
		{
			UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>( AllComponentsInScene[ i ] );
			if( Comp && StaticMeshComp && Comp->GetStaticMesh() == StaticMeshComp->GetStaticMesh() )
			{
				TArray<UMaterialInterface*> Materials = Comp->GetMaterials();
				if( AreMaterialsUnique( Materials ) )
				{
					OutMaterials = Materials;
					break;
				}
			}
		}
	}

	MaterialBinding** MaterialsUsed = nullptr;
	MaterialsUsed = new MaterialBinding* [ NumMaterials ];
	for( int i = 0; i < NumMaterials; i++ )
		MaterialsUsed[ i ] = nullptr;

	UObject* LoadedObject = StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, TEXT( "/Engine/EngineMaterials/DefaultMaterial" ) );
	UMaterialInterface* DefaultMaterial = Cast<UMaterialInterface>( LoadedObject );
	if( OutMaterials.Num() > 0 )
	{
		for( int i = 0; i < OutMaterials.Num(); i++ )
		{
			MaterialsUsed[ i ] = nullptr;
			if( !OutMaterials[ i ] )
			{
				OutMaterials[ i ] = DefaultMaterial;
			}
			MaterialsUsed[ i ] = ProcessMaterialReference( OutMaterials[ i ] );
		}
	}

	return MaterialsUsed;
}
FBoxSphereBounds GetBounds( const USkeletalMesh* SkeletalMesh )
{
	#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
		return ( (USkeletalMesh*)SkeletalMesh)->GetBounds();
	#else
		return SkeletalMesh->GetBounds();
	#endif
}
void ProcessMeshComponent( AActor* Actor, UStaticMeshComponent* StaticMeshComp, USkeletalMeshComponent* SkeletalMeshComponent, GameObject* ActorGO )
{
	const UStaticMesh* StaticMesh = nullptr;
	const USkeletalMesh* SkeletalMesh = nullptr;
	bool IsVisible = false;
	if( StaticMeshComp )
	{
		StaticMesh = StaticMeshComp->GetStaticMesh();
		//ShouldRender is more specific and includes visible layers, export what's actually shown in the editor
		IsVisible = StaticMeshComp->ShouldRender();//IsVisible()
	}
	if( SkeletalMeshComponent )
	{
		SkeletalMesh = GetSkeletalMesh( SkeletalMeshComponent );
		IsVisible = SkeletalMeshComponent->ShouldRender();
	}

	if( (StaticMeshComp && !StaticMesh ) || (SkeletalMeshComponent && !SkeletalMesh) || !IsVisible )
		return;

	if( !ExportMeshes )
		return;

	GameObject* ComponentGO = nullptr;
	int RelevantComponents = GetNumRelevantComponents( Actor );
	if( RelevantComponents <= 1 )
	{
		ComponentGO = ActorGO;
		ActorGO->CanBePrefab = true;
	}
	else
	{
		ComponentGO = new GameObject;
		ActorGO->AddChild( ComponentGO );
		ActorGO->OwnerScene->Add( ComponentGO );
	}
	FString CompNameStr = "";
	if( StaticMeshComp )
		CompNameStr = StaticMeshComp->GetName();
	if( SkeletalMeshComponent )
		CompNameStr = SkeletalMeshComponent->GetName();

	std::string CompName = ToANSIString( *CompNameStr );
	if( RelevantComponents > 1 )
		ComponentGO->Name = CompName;

	TransformComponent* ActorTransformComp = (TransformComponent*)ActorGO->GetComponent( CT_TRANSFORM );
	ActorTransformComp->Reset();

	TransformComponent* ComponentTransform = (TransformComponent*)ComponentGO->GetComponent( CT_TRANSFORM );

	FTransform ComponentTrans;
	if ( StaticMeshComp )
		ComponentTrans = StaticMeshComp->GetComponentTransform();
	if( SkeletalMeshComponent )
		ComponentTrans = SkeletalMeshComponent->GetComponentTransform();
	ConvertTransform( ComponentTrans, ComponentTransform, false );// true );

	const FStaticMeshRenderData* MeshRenderData = nullptr;
	FSkeletalMeshRenderData* SkeletalMeshRenderData = nullptr;
	int NumLods = 0;
	if( StaticMesh )
	{
		MeshRenderData = GetRenderData( (UStaticMesh*)StaticMesh );
		NumLods = MeshRenderData->LODResources.Num();
	}
	if( SkeletalMesh )
	{
		SkeletalMeshRenderData = SkeletalMesh->GetResourceForRendering();
		NumLods = SkeletalMeshRenderData->LODRenderData.Num();
	}

	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings && !UnitySettings->ExportLODs )
	{
		NumLods = 1;
	}

	LodGroup* ComponentLodGroup = nullptr;
	if( NumLods > 1 && ( MeshRenderData  || SkeletalMeshRenderData ) )
	{
		ComponentLodGroup = (LodGroup*)ComponentGO->AddComponent( CT_LODGROUP );
	}
	AnimationComponent* AnimComp = nullptr;
	if ( SkeletalMeshComponent )
	{
		UAnimSequence* Anim = GetAnimationSequence( SkeletalMeshComponent );
		if( Anim )
		{
			AnimComp = (AnimationComponent*)ComponentGO->AddComponent( CT_ANIMATION );
			AnimComp->AnimSequence = Anim;
		}
	}

	UBodySetup* BodySetup = nullptr;
	if ( StaticMesh )
		BodySetup = GetBodySetup( StaticMesh );

	MeshCollider* NewMeshCollider = nullptr;
	if( BodySetup && ExporterInterface->Settings->ExportPhysics )
	{
		int ConvexHulls = BodySetup->AggGeom.ConvexElems.Num();
		int Spheres = BodySetup->AggGeom.SphereElems.Num();
		int Boxes = BodySetup->AggGeom.BoxElems.Num();
		int Capsules = BodySetup->AggGeom.SphylElems.Num();
		int TaperedCapsules = BodySetup->AggGeom.TaperedCapsuleElems.Num();
		int TotalShapes = ConvexHulls + Spheres + Boxes + Capsules + TaperedCapsules;

		if( TotalShapes > 0 )
		{
			FRotator RotationCompensation;
			if( ExporterInterface->Type == EngineType::UNITY)
				RotationCompensation = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );
			else if ( ExporterInterface->Type == EngineType::GODOT )
				RotationCompensation = FRotator::MakeFromEuler( FVector( 90, 0, 0 ) );

			char Text[ 128 ];
			for( int i = 0; i < Boxes; i++ )
			{
				FKBoxElem& Box = BodySetup->AggGeom.BoxElems[ i ];

				GameObject* PrimitiveGO = ComponentGO;
				FVector Euler = Box.Rotation.Euler();
				bool NeedsTransform = false;

				FVector Center = Box.Center / 100.0f;

				if( ExporterInterface->Type == EngineType::GODOT )
				{
					Center = FVector( Center.X, Center.Z, -Center.Y );
				}
				else
					Center = RotationCompensation.RotateVector( Center );

				//Need to apply RotationCompensation all the time !
				//if( !Euler.IsNearlyZero() )
				{
					PrimitiveGO = new GameObject();
					ComponentGO->AddChild( PrimitiveGO );
					snprintf( Text, sizeof( Text ), "Box%d", i );
					PrimitiveGO->Name = Text;
					TransformComponent* TC = (TransformComponent*)PrimitiveGO->GetComponent( CT_TRANSFORM );

					FVector DefaultRotation = RotationCompensation.Euler();
					FVector BoxEuler = Box.Rotation.Euler();
					FRotator BoxRotator = FRotator::MakeFromEuler( FVector( BoxEuler.X, BoxEuler.Y, BoxEuler.Z ) );
					FQuat FinalQuat = RotationCompensation.Quaternion() * BoxRotator.Quaternion();

					TC->Set( Center, FinalQuat, FVector( 1, 1, 1 ) );

					NeedsTransform = true;
				}

				BoxCollider* NewBoxCollider = (BoxCollider*)PrimitiveGO->AddComponent( CT_BOXCOLLIDER );
				NewBoxCollider->Size = FVector( Box.X / 100.0f, Box.Y / 100.0f, Box.Z / 100.0f );
				if( !NeedsTransform )
				{
					NewBoxCollider->Center = Center;
					ConvertScaleForUnity( NewBoxCollider->Size );
				}
				else
				{
					RotateForYUp( NewBoxCollider->Size );
					SwitchYWithZ( NewBoxCollider->Size );
				}
			}
			for( int i = 0; i < Spheres; i++ )
			{
				FKSphereElem& Sphere = BodySetup->AggGeom.SphereElems[ i ];

				SphereCollider* NewSphereCollider = (SphereCollider*)ComponentGO->AddComponent( CT_SPHERECOLLIDER );

				FVector Center = Sphere.Center / 100;
				Center = RotationCompensation.RotateVector( Center );
				NewSphereCollider->Center = Center;
				NewSphereCollider->Radius = Sphere.Radius / 100.0f;
				//RotateForYUp( NewSphereCollider->Center );
			}
			for( int i = 0; i < Capsules; i++ )
			{
				FKSphylElem& Capsule = BodySetup->AggGeom.SphylElems[ i ];

				bool NeedsTransform = false;
				GameObject* PrimitiveGO = ComponentGO;
				FVector Center = Capsule.Center / 100.0f;
				FVector Euler = Capsule.Rotation.Euler();
				Center = RotationCompensation.RotateVector( Center );
				if( !Euler.IsNearlyZero() )
				{
					PrimitiveGO = new GameObject();
					ComponentGO->AddChild( PrimitiveGO );
					snprintf( Text, sizeof( Text ), "Capsule%d", i );
					PrimitiveGO->Name = Text;
					TransformComponent* TC = (TransformComponent*)PrimitiveGO->GetComponent( CT_TRANSFORM );
					
					FVector CapsuleEuler = Capsule.Rotation.Euler();
					//I wonder why the box euler conversion doesn't work here ?
					FVector FinalEuler = FVector( CapsuleEuler.Z, CapsuleEuler.Y, -CapsuleEuler.X );// +FVector( 0, 270, 0 );
					
					if( ExporterInterface->Type == EngineType::GODOT )
					{						
						FinalEuler = FVector( CapsuleEuler.X, -CapsuleEuler.Z, CapsuleEuler.Y );
					}
					auto FinalRotation = FRotator::MakeFromEuler( FinalEuler );

					TC->Set( Center, FinalRotation.Quaternion(), FVector( 1, 1, 1 ) );

					NeedsTransform = true;
				}

				CapsuleCollider* NewCapsuleCollider = (CapsuleCollider*)PrimitiveGO->AddComponent( CT_CAPSULECOLLIDER );

				NewCapsuleCollider->Radius = Capsule.Radius / 100.0f;
				//Unity starts height at 0, Unreal starts with height=radius
				NewCapsuleCollider->Height = Capsule.Length / 100.0f + NewCapsuleCollider->Radius;
				NewCapsuleCollider->Direction = 1;

				if( !NeedsTransform )
				{
					NewCapsuleCollider->Center = Capsule.Center / 100.0f;
					RotateForYUp( NewCapsuleCollider->Center );
				}
			}
		}
		if (ConvexHulls > 0 || BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple)
		{
			NewMeshCollider = (MeshCollider*)ComponentGO->AddComponent( CT_MESHCOLLIDER );
		}
	}

	if( (MeshRenderData || SkeletalMeshRenderData ) && NumLods > 0 )
	{
		for( int i = 0; i < NumLods; i++ )
		{
			Renderer* LODMeshRenderer = ExportLOD( StaticMeshComp, SkeletalMeshComponent, ComponentGO, Actor, i, NumLods );
			if( ComponentLodGroup )
			{
				LOD* NewLOD = new LOD;
				if( i == NumLods - 1 )
				{
					if ( ComponentLodGroup->Lods.size() >= 1 )
					{
						NewLOD->screenRelativeTransitionHeight = ComponentLodGroup->Lods[ ComponentLodGroup->Lods.size() - 1]->screenRelativeTransitionHeight / 2.0f;
						NewLOD->screenRelativeTransitionHeight = FMath::Min( NewLOD->screenRelativeTransitionHeight, 0.05f );
					}
					else
						NewLOD->screenRelativeTransitionHeight = 0.01f;
				}
				else
				{
					float ScreenSize = 1.0;
					if ( MeshRenderData && i + 1 < MAX_STATIC_MESH_LODS )//Fix for a reported crash
						ScreenSize = MeshRenderData->ScreenSize[ i + 1 ].Default;
					NewLOD->screenRelativeTransitionHeight = FMath::Min( ScreenSize, 1.0f );
				}
				NewLOD->renderers.push_back( LODMeshRenderer );
				ComponentLodGroup->Lods.push_back( NewLOD );
				if( StaticMesh )
				{
					ComponentLodGroup->Size = StaticMesh->GetBounds().SphereRadius / 100.0f * 2;
					ComponentLodGroup->LocalReferencePoint = StaticMesh->GetBounds().Origin / 100.0f;
				}
				else if ( SkeletalMesh )
				{
					auto Bounds = GetBounds( SkeletalMesh );
					ComponentLodGroup->Size = Bounds.SphereRadius / 100.0f * 2;
					ComponentLodGroup->LocalReferencePoint = Bounds.Origin / 100.0f;
				}

				FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );//Rotate for Y Up
				ComponentLodGroup->LocalReferencePoint = Rotator.RotateVector( ComponentLodGroup->LocalReferencePoint );
			}
			if( NewMeshCollider && !NewMeshCollider->Mesh )
			{
				auto MF = (MeshFilter*)LODMeshRenderer->Owner->GetComponent( CT_MESHFILTER );
				if( MF && MF->Mesh )
				{
					NewMeshCollider->Mesh = MF->Mesh;
					uint64_t BaseMeshFileID = 4300000;
					//It's always the last mesh					
					if( NumLods > 1 )
						NewMeshCollider->SubMeshID = BaseMeshFileID + NumLods * 2;
					else
						NewMeshCollider->SubMeshID = ( BaseMeshFileID + 2 ) + NumLods * 2;

					//Force LODForCollision be the ConvexHull mesh to be identical to how UE does it
					if (BodySetup && BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple)
					{
						if (StaticMesh->LODForCollision < NumLods)
						{
							if (NumLods > 1)
								NewMeshCollider->SubMeshID = BaseMeshFileID + StaticMesh->LODForCollision * 2;
							else
								NewMeshCollider->SubMeshID = BaseMeshFileID;
						}
					}

					MF->Mesh->HasConvexHulls = true;
					MF->Mesh->ConvexHullMeshID = NewMeshCollider->SubMeshID;
				}
			}
			
			if( UnitySettings && !UnitySettings->ExportLODs )
				break;
		}
		
	}
}
UnityLightType GetUnityLightComponentType( ELightComponentType Type )
{
	if( ExporterInterface->Type == EngineType::UNITY )
	{
		switch( Type )
		{
			case LightType_Directional:return LT_DIRECTIONAL;
			default:
			case LightType_Point:return LT_POINT;
			case LightType_Spot: return LT_SPOT;
			case LightType_Rect: return LT_AREA;
		}
	}
	else //if ( UnitySettings->Engine == EngineType::GODOT )
	{
		switch( Type )
		{
			case LightType_Directional:return LT_DIRECTIONAL;
			default:
			case LightType_Rect :
			case LightType_Point:return LT_POINT;
			case LightType_Spot: return LT_SPOT;
		}
	}
}
float ConvertSpotAngle( float OuterConeAngle )
{
	float MaxUEAngle = 80;
	float MaxUnityAngle = 179;

	float AngleRatio = OuterConeAngle / MaxUEAngle;
	return AngleRatio * MaxUnityAngle;
}
void ConvertTransform( FTransform Trans, TransformComponent* Transform, bool IsRelative, UDecalComponent* ActorDecal)
{
	FVector OriginalTranslation = Trans.GetTranslation();
	OriginalTranslation /= 100.0f;

	FVector Translation = OriginalTranslation;

	FQuat Quat = Trans.GetRotation();
	if( CVarFBXPerActor.GetValueOnAnyThread() == 1 )
	{
		if( ExporterInterface->Type == EngineType::UNITY )
		{
			FRotator Rotator = FRotator::MakeFromEuler( FVector( -90, 0, 90 ) );//fixes FBX preview angle
			Quat = Quat * Rotator.Quaternion();
		}
		else//GODOT
		{
			FRotator Rotator = FRotator::MakeFromEuler( FVector( -90, 0, 0 ) );
			Quat = Quat * Rotator.Quaternion();
		}

		if( ActorDecal )
		{
			if( ActorDecal->GetAttachParent() )
			{
				FTransform CompTransform = ActorDecal->GetComponentTransform();
				FRotator CompTransformRotator = CompTransform.Rotator();
				FRotator DecalRotator = FRotator::MakeFromEuler( FVector( 0, 90, 0 ) );
				Quat = CompTransformRotator.Quaternion() * DecalRotator.Quaternion();
			}
			else
			{
				FRotator DecalRotator = FRotator::MakeFromEuler( FVector( 0, 0, 90 ) );
				Quat = Quat * DecalRotator.Quaternion();
			}
		}
	}
	else if( 1 )
	{
		FRotator Rotator = FRotator::MakeFromEuler( FVector( -90, 0, 0 ) );//works with OBJ
		Quat = Quat * Rotator.Quaternion();
	}
	FVector Scale = Trans.GetScale3D();

	if( CVarFBXPerActor.GetValueOnAnyThread() == 1 )
	{
		float Temp = 0;
		
		Temp = Scale.Y;
		Scale.Y = Scale.Z;
		Scale.Z = Temp;

		if( ExporterInterface->Type == EngineType::UNITY )
		{
			Temp = Scale.Z;
			Scale.Z = Scale.X;
			Scale.X = Temp;
		}

		if( ActorDecal )
		{
			Temp = Scale.X;
			Scale.X = Scale.Y;
			Scale.Y = Temp;
		}
	}
	else if( 1 )
	{
		float Temp = Scale.Y;
		Scale.Y = Scale.Z;
		Scale.Z = Temp;
	}
	

	Transform->LocalPosition[ 0 ] = Translation.X;
	Transform->LocalPosition[ 1 ] = Translation.Y;
	Transform->LocalPosition[ 2 ] = Translation.Z;

	Transform->LocalRotation[ 0 ] = Quat.X;
	Transform->LocalRotation[ 1 ] = Quat.Y;
	Transform->LocalRotation[ 2 ] = Quat.Z;
	Transform->LocalRotation[ 3 ] = Quat.W;

	Transform->LocalScale[ 0 ] = Scale.X;
	Transform->LocalScale[ 1 ] = Scale.Y;
	Transform->LocalScale[ 2 ] = Scale.Z;

	if (ActorDecal && ExporterInterface->Type == EngineType::UNITY )
	{
		//Because I do Decal->DecalSize * ComponentScale down bellow
		for (int i = 0; i < 3; i++)
		{
			Transform->LocalScale[i] = 1.0f;
		}
	}

	if( IsRelative )
	{
		Transform->LocalPosition[ 0 ] *= -1;
	}
}

FVector SplineRotation( 90, 270, 0 );
float ClampTo1( float Val )
{
	if( Val > 1 )
		Val = 1.0f;
	
	return Val;
}
UDecalComponent* GetActorDecal( AActor* A )
{
	TArray<UActorComponent*> comps;
	A->GetComponents( comps );
	for( int i = 0; i < comps.Num(); i++ )
	{
		UActorComponent* AC = comps[ i ];
		UDecalComponent* Decal = Cast<UDecalComponent>( AC );
		if( Decal )
		{
			return Decal;
		}
	}

	return nullptr;
}
extern GameObject* GameObjectForTrees;
void ProcessActor( AActor *A, ExporterScene* S)
{
	if ( !A )
		return;	

	TArray<UActorComponent*> comps;
	A->GetComponents( comps );

	FTransform Trans = A->GetTransform();	

	std::string ActorName = ToANSIString( *A->GetActorLabel() );
	
	GameObject *GO = new GameObject();
	S->Add( GO );

	GO->Name = ActorName;
	
	FQuat Quat = Trans.GetRotation();
	TransformComponent* Transform = (TransformComponent*)GO->GetComponent( CT_TRANSFORM );

	UDecalComponent* ActorDecal = GetActorDecal( A );
	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	ConvertTransform( Trans, Transform, false, ActorDecal );

	APostProcessVolume* PostProcessVolume = Cast<APostProcessVolume>( A );
	if( PostProcessVolume )
	{
		for( int i = 0; i < PostProcessVolume->Settings.WeightedBlendables.Array.Num(); i++ )
		{
			FWeightedBlendable& Blendable = PostProcessVolume->Settings.WeightedBlendables.Array[ i ];
			
			UMaterialInterface* Material = Cast<UMaterialInterface>( Blendable.Object );
			if( Material )
			{
				ProcessMaterialReference( Material );
			}
		}
	}
	for ( int i = 0; i < comps.Num(); i++ )
	{
		UActorComponent *AC = comps[ i ];
		ULightComponent *LightComp = dynamic_cast<ULightComponent*>( AC );
		if ( LightComp )
		{			
			LightComponent* NewLightComponent = new LightComponent;
			
			TransformComponent* LightTransform = Transform;
			if( comps.Num() > 1 )
			{
				Transform->Reset();
				GameObject* ChildGO = new GameObject;
				ChildGO->Name = ToANSIString( *LightComp->GetName() );
				ChildGO->Components.push_back( NewLightComponent );
				NewLightComponent->Owner = ChildGO;
				LightTransform = (TransformComponent*)ChildGO->GetComponent( CT_TRANSFORM );
				
				FTransform ComponentTrans = LightComp->GetComponentTransform();
				ConvertTransform( ComponentTrans, LightTransform, false );

				GO->AddChild( ChildGO );
				GO->OwnerScene->Add( ChildGO );
			}
			else
			{
				NewLightComponent->Owner = GO;
				GO->Components.push_back( NewLightComponent );
			}
			AdditionalLightData* LightData = nullptr;
			if( UnitySettings && (UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP ||
				UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP ))
			{
				LightData = ( AdditionalLightData*)NewLightComponent->Owner->AddComponent( CT_ADDITIONAL_LIGHT_DATA );
				LightData->Light = NewLightComponent;
			}

			FQuat FixQuat( FVector( 0, 1, 0 ), PI / 2 );
			FQuat FinalQuat = Quat * FixQuat;
			LightTransform->LocalRotation[ 0 ] = FinalQuat.X;
			LightTransform->LocalRotation[ 1 ] = FinalQuat.Y;
			LightTransform->LocalRotation[ 2 ] = FinalQuat.Z;
			LightTransform->LocalRotation[ 3 ] = FinalQuat.W;

			ELightComponentType Type = LightComp->GetLightType();
			
			ExporterInterface->TranslateLightData( LightComp, NewLightComponent, LightData );

			NewLightComponent->Color = LightComp->LightColor;
			NewLightComponent->Type = GetUnityLightComponentType( Type );			
			NewLightComponent->Shadows = LightComp->CastShadows;			
			NewLightComponent->Enabled = LightComp->bAffectsWorld;
		}
		USkyLightComponent* SkyLightComponent = Cast< USkyLightComponent >( AC );
		if( SkyLightComponent )
		{
			if( !ExporterInterface->SkylightComponent )
				ExporterInterface->SkylightComponent = SkyLightComponent;
		}
		UCameraComponent* CameraComp = Cast<UCameraComponent>( AC );
		if( CameraComp )
		{
			//if( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP )
			//{
			//	LightData = (AdditionalLightData*)NewLightComponent->Owner->AddComponent( CT_ADDITIONAL_CAMERA_DATA );
			//	//LightData->Camera = NewLightComponent;
			//}
		}
		UInstancedStaticMeshComponent* InstancedMeshComp = Cast<UInstancedStaticMeshComponent>( AC );
		//Continue because I already spawned the instances as individual actors
		if( InstancedMeshComp )
			continue;
		USkeletalMeshComponent *SkelMeshComp = Cast<USkeletalMeshComponent>( AC );
		if ( SkelMeshComp )
		{
			ProcessMeshComponent( A, nullptr, SkelMeshComp, GO );
		}		
		USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( AC );
		if( SplineMeshComponent )
		{
			//auto Pos = SplineMeshComponent->SplineParams.StartPos;
			//FVector SplineExtents = SplineMeshComponent->SplineParams.EndPos - SplineMeshComponent->SplineParams.StartPos;			
			//UStaticMesh* StaticMesh = SplineMeshComponent->GetStaticMesh();
			//if( StaticMesh )
			//{
			//	auto MeshBounds = StaticMesh->GetBounds();
			//	FVector MeshSizes = MeshBounds.BoxExtent * 2;
			//	if( FMath::IsNearlyZero( SplineExtents.X, 1.0f ) )
			//		SplineExtents.X = MeshSizes.X;
			//	if( FMath::IsNearlyZero( SplineExtents.Y, 1.0f ) )
			//		SplineExtents.Y = MeshSizes.Y;
			//	if( FMath::IsNearlyZero( SplineExtents.Z, 1.0f ) )
			//		SplineExtents.Z = MeshSizes.Z;
			//
			//	//Scale based on Z up, needs fixing
			//	FVector Scale = SplineExtents / MeshSizes;
			//	//auto Average = (SplineMeshComponent->SplineParams.StartPos + SplineMeshComponent->SplineParams.EndPos)/2;
			//
			//	FTransform& CompTransform = (FTransform&)SplineMeshComponent->GetComponentToWorld();
			//	CompTransform.SetLocation( Pos );
			//	//rotate from UE to Unity space
			//	FRotator Rotator = FRotator::MakeFromEuler( SplineRotation );
			//	CompTransform.SetRotation( FQuat( Rotator ));
			//	CompTransform.SetScale3D( Scale );
			//}
		}
		UStaticMeshComponent *StaticMeshComp = Cast<UStaticMeshComponent>( AC );
		if ( StaticMeshComp )
		{	
			ProcessMeshComponent( A, StaticMeshComp, nullptr, GO );
		}
		USplineComponent* SplineComponent = Cast<USplineComponent>( AC );
		if( SplineComponent )
		{
			int32 Points = SplineComponent->GetNumberOfSplinePoints();
		}
		USphereReflectionCaptureComponent* SphereReflection = Cast<USphereReflectionCaptureComponent>( AC );
		if( SphereReflection )
		{
			float Influence = SphereReflection->InfluenceRadius / 100.0f;
			ReflectionProbeComponent* NewReflectionProbeComponent = ( ReflectionProbeComponent*)GO->AddComponent( CT_REFLECTIONPROBE );
			NewReflectionProbeComponent->Intensity = 1.0f;// SphereReflection->Brightness;
			NewReflectionProbeComponent->Size = FVector( Influence, Influence, Influence );
		}
		UBoxReflectionCaptureComponent* BoxReflection = Cast<UBoxReflectionCaptureComponent>( AC );
		if( BoxReflection )
		{
			FTransform BoxTransform = BoxReflection->GetComponentTransform();
			ReflectionProbeComponent* NewReflectionProbeComponent = (ReflectionProbeComponent*)GO->AddComponent( CT_REFLECTIONPROBE );
			NewReflectionProbeComponent->Intensity = 1.0f;// BoxReflection->Brightness;
			//Also convert to cm
			NewReflectionProbeComponent->Size = BoxTransform.GetScale3D() / 100.0f;
		}
		UDecalComponent* Decal = Cast<UDecalComponent>( AC );
		bool IsLegalForUnity = UnitySettings && ( UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_URP ||
												  UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP );
		if( Decal && ( IsLegalForUnity || ExporterInterface->Type == EngineType::GODOT ))
		{
			DecalComponent* NewDecalComponent = (DecalComponent*)GO->AddComponent( CT_DECAL );

			UMaterialInterface* DecalMat = Decal->GetDecalMaterial();
			if( DecalMat )
			{
				MaterialBinding* Binding = ProcessMaterialReference( DecalMat );
				NewDecalComponent->Material = Binding->UnityMat;
				NewDecalComponent->Material->DecalDrawOrder = Decal->SortOrder;
				FVector ComponentScale = Decal->GetRelativeScale3D();
				NewDecalComponent->Size = Decal->DecalSize * ComponentScale;
				NewDecalComponent->Size /= 100.0f;
				if( ExporterInterface->Type == EngineType::UNITY )
				{
					if( NewDecalComponent->Size.X < 0 )
						NewDecalComponent->Size.X *= -1;
					if( NewDecalComponent->Size.Y < 0 )
						NewDecalComponent->Size.Y *= -1;
					if (NewDecalComponent->Size.Z < 0 )
						NewDecalComponent->Size.Z *= -1;

					NewDecalComponent->Size *= 2.0f;
				}
				NewDecalComponent->DrawOrder = Decal->SortOrder;
				ConvertScaleForUnity( NewDecalComponent->Size );
				//TODO depending on rotation Size.X needs to be swapped with Size.Y
				
				if( ExporterInterface->Type == EngineType::UNITY )
				{
					float Temp = NewDecalComponent->Size.X;
					NewDecalComponent->Size.X = NewDecalComponent->Size.Y;
					NewDecalComponent->Size.Y = Temp;
				}
				else if( ExporterInterface->Type == EngineType::GODOT )
				{
					float Temp = NewDecalComponent->Size.Y;
					NewDecalComponent->Size.Y = NewDecalComponent->Size.Z;
					NewDecalComponent->Size.Z = Temp;

					Temp = NewDecalComponent->Size.X;
					NewDecalComponent->Size.X = NewDecalComponent->Size.Z;
					NewDecalComponent->Size.Z = Temp;
				}
			}
		}
		ULandscapeComponent* LandscapeComponent = Cast<ULandscapeComponent>( AC );
		if( LandscapeComponent )
		{
			TreeInstancesComponent* TIC = nullptr;
			if( !GameObjectForTrees )
			{
				GameObjectForTrees = GO;
				TIC = (TreeInstancesComponent*)GameObjectForTrees->AddComponent( CT_TREEINSTANCES );
			}
			else
				TIC = (TreeInstancesComponent*)GameObjectForTrees->GetComponent( CT_TREEINSTANCES );

			UTexture2D* HeightMap = GetHeightmap( LandscapeComponent );
			
			TextureBinding* Binding = UETextureExporter::Export( HeightMap );
			Binding->UnityTex = InitializeUnityTexture( HeightMap, nullptr, nullptr, nullptr, nullptr );
			Binding->UnityTex->IsReadable = true;
			Binding->UnityTex->Compression = 0;

			FString HeightmapLocalPath = GenerateTexturePath( HeightMap, true );
			TIC->HeightMapTexture = ToANSIString( *HeightmapLocalPath );
			
			TIC->TerrainHeight = 512;// LandscapeComponent->Bounds.BoxExtent.Z;
		}
	}

	S->EvaluatePrefab( GO );
}

bool VerifyOrCreateDirectory( const FString& TestDir )
{
	//Directory Exists?
	if ( !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *TestDir ) )
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectory( *TestDir );

		if ( !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *TestDir ) )
		{
			return false;
		}
	}
	return true;
}

bool SaveFile( const char *File, void *data, int64_t size )
{
	FILE *f = nullptr;
	if( File && strcmp( File, "" ) != 0 )
		f = fopen( File, "wb" );

	if( !f )
		return false;

	fwrite( data, sizeof( char ), (int)size, f );
	fclose( f );
	return true;
}
int64_t GetFileSize( const char *FileName )
{
	if( !FileName || !FileName[ 0 ] )
		return -1;
	int64_t size = 0;
	FILE* f = nullptr;
	f = fopen( FileName, "rb" );
	if( !f )
		return -1;

	fseek( f, 0, SEEK_END );
	size = ftell( f );

	if( size == -1 )
	{
		//RELog( "The file may be over 2GB, use _ftelli64 !" );
		size = _ftelli64( f );

	}
	fclose( f );
	return size;
}
int64_t LoadFile( const char *File, uint8** data )
{
	if( !File || !data || File[ 0 ] == 0 )
		return -1;

	int64_t size = 0;
	size = GetFileSize( File );

	FILE *f = nullptr;
	f = fopen( File, "rb" );
	if( !f )
	{
		int Ret = -1;	
		return Ret;
	}

	//Add '0' character (if presumed to be acting like a string)
	//might impply allocation errors ?
	*data = new uint8[ (size_t)size + 1 ];
	if( !( *data ) )
	{
		//RELog( "LoadFile Allocation Error" );
		return -1;
	}
	( *data )[ size ] = 0;//last character should be 0 in case it's a string

	size_t BytesRead = fread( *data, sizeof( uint8 ), size, f );
	fclose( f );
	if( BytesRead == 0 )
	{
		BytesRead = 0;
	}
	return BytesRead;
}

#include <atomic>

UWorld *CurrentWorld = nullptr;

std::atomic<int> ProcessingState;
void RenderThreadExport()
{
	ProcessAllMaterials();

	ProcessingState = 1;
}

class RenderThreadTaskInitializer
{
public:
	void QueCommand(  bool UseSync = false )
	{
		if( UseSync )
		{
			RenderThreadExport();
		}
		else
		{
			RenderThreadTaskInitializer* This = this;
			ENQUEUE_RENDER_COMMAND( Command_RenderThreadExport )(
				[This]( FRHICommandListImmediate& RHICmdList )
			{
				RenderThreadExport();
			} );
		}
	}
};
class UICallback
{
public:
	virtual void AddToolbarExtension()
	{

	}
};
RenderThreadTaskInitializer *RTTI = new RenderThreadTaskInitializer;

#endif

ExporterScene* AllocateScene()
{
	return new ExporterScene;
}

int32 GetNumberOfFrames( const UAnimSequence* AnimSeq )
{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	return AnimSeq->GetNumberOfFrames();
#elif ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26
	return AnimSeq->GetRawNumberOfFrames();
#else
	return AnimSeq->GetNumberOfSampledKeys();
#endif
}
float GetSequenceLength( const UAnimSequence* AnimSeq )
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	return AnimSeq->GetPlayLength();
#else
	return AnimSeq->SequenceLength;
#endif	
}

void ConvertTransformForBones( FTransform Trans, TransformComponent* Transform, bool IsRoot )
{
	FVector OriginalTranslation = Trans.GetTranslation();
	OriginalTranslation /= 100.0f;

	static bool ChangeTranslation = true;
	static bool ChangeRotation = true;

	FQuat Quat = Trans.GetRotation();
	FVector Euler = Quat.Euler();
	FVector Translation = OriginalTranslation;

	static FVector Coefs( -1, -1, 1 );
	static FVector RotCoefs( -1, -1, 1 );
	static FVector RootFix( 90, 180, 0 );//90,0,0 previously

	if (ChangeTranslation)
	{
		Translation = FVector( Coefs.X * OriginalTranslation.X,
			Coefs.Y * OriginalTranslation.Y,
			Coefs.Z * OriginalTranslation.Z );
	}
	if ( ChangeRotation )//&& !IsRoot )
	{		
		Euler = FVector( RotCoefs.X * Euler.X,
			RotCoefs.Y * Euler.Y,
			RotCoefs.Z * Euler.Z );
	}
	
	Quat = FQuat::MakeFromEuler( Euler );

	if( IsRoot )
	{
		FQuat QuatFix = FQuat::MakeFromEuler( RootFix );
		Quat = QuatFix * Quat;
	}

	if( 0 )//CVarFBXPerActor.GetValueOnAnyThread() == 1 )
	{
		FRotator Rotator = FRotator::MakeFromEuler( FVector( 0, 0, 0 ) );
		Quat = Quat * Rotator.Quaternion();
	}

	FVector Scale = Trans.GetScale3D();

	if( 0 )//CVarFBXPerActor.GetValueOnAnyThread() == 1 )
	{
		float Temp = Scale.Y;
		Scale.Y = Scale.Z;
		Scale.Z = Temp;

		Temp = Scale.Z;
		Scale.Z = Scale.X;
		Scale.X = Temp;
	}

	Transform->LocalPosition[ 0 ] = Translation.X;
	Transform->LocalPosition[ 1 ] = Translation.Y;
	Transform->LocalPosition[ 2 ] = Translation.Z;

	Transform->LocalRotation[ 0 ] = Quat.X;
	Transform->LocalRotation[ 1 ] = Quat.Y;
	Transform->LocalRotation[ 2 ] = Quat.Z;
	Transform->LocalRotation[ 3 ] = Quat.W;

	Transform->LocalScale[ 0 ] = Scale.X;
	Transform->LocalScale[ 1 ] = Scale.Y;
	Transform->LocalScale[ 2 ] = Scale.Z;

	//if( IsRelative )
	{
		//Transform->LocalPosition[ 0 ] *= -1;
	}
}

TArray< AnimationEntry > AnimationsToExport;

#if ENGINE_MAJOR_VERSION == 4
	TArray<UMaterialInterface*> GetArrayOfMaterials( TArray<UMaterialInterface*>& ArrayType )
#else
	TArray<UMaterialInterface*> GetArrayOfMaterials( TArray<TObjectPtr<class UMaterialInterface>>& NewArrayType )
#endif
{
	#if ENGINE_MAJOR_VERSION == 4
		return ArrayType;
	#else
		TArray<UMaterialInterface*> Array;
		for( int i = 0; i < NewArrayType.Num(); i++ )
		{
			Array.Add( NewArrayType[ i ] );
		}	
		return Array;
	#endif
}

void AnimationEntry::GenerateGUID( )
{
	FString AnimationAssetPath = GetAssetPathFolder( AnimSeq );
	AnimationAssetPath += *AnimSeq->GetName();
	std::string AnimationAssetPathStr = ToANSIString( *AnimationAssetPath );
	std::size_t hash = std::hash<std::string>{}(AnimationAssetPathStr);
	GUID = ExporterInterface->GetGUIDFromSizeT( hash );
}
AnimationEntry* AddAnimationEntry( const USkeletalMesh* SkeletalMesh, UAnimSequence* AnimSeq, const TCHAR* MeshName, const TArray<UMaterialInterface*>* OverrideMaterials )
{
	if( !AnimSeq || !SkeletalMesh )
		return nullptr;

	for( int i = 0; i < AnimationsToExport.Num(); i++ )
	{
		if( AnimationsToExport[ i ].AnimSeq == AnimSeq )
			return &AnimationsToExport[i];
	}

	AnimationEntry NewAnimationEntry;

	NewAnimationEntry.SkeletalMesh = SkeletalMesh;
	NewAnimationEntry.AnimSeq = AnimSeq;
	NewAnimationEntry.MeshName = MeshName;
	for( int i = 0; i < OverrideMaterials->Num(); i++ )
	{
		NewAnimationEntry.OverrideMaterials.Add( ( *OverrideMaterials )[ i ] );
	}

	NewAnimationEntry.GenerateGUID();

	AnimationsToExport.Add( NewAnimationEntry );
	return &AnimationsToExport[ AnimationsToExport.Num() - 1 ];
}

bool ShouldExportActor(AActor* A)
{
	ALODActor* LODActor = Cast< ALODActor >( A );
	if( LODActor )
		return false;
	AGroupActor* GroupActor = Cast< AGroupActor >( A );
	if( GroupActor )
		return false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
	AWorldPartitionHLOD* HLODActor = Cast< AWorldPartitionHLOD >( A );
	if( HLODActor )
		return false;
#endif

	return true;
}
void RemapSanitizedParameters( MaterialBinding* Material, MaterialBinding* BaseMaterial )
{	
	for( auto iter = Material->SanitizedParamNames.CreateIterator(); iter; ++iter )
	{
		FString Key   = iter->Key;
		FString Value = iter->Value;

		FString* SanitizedBase = BaseMaterial->SanitizedParamNames.Find( Key );
		if( SanitizedBase )
		{
			for( int i = 0; i < Material->UnityMat->Parameters.size(); i++ )
			{
				ShaderParameter& Param = Material->UnityMat->Parameters[ i ];
				if( Param.Name.compare( ToANSIString( *Value ) ) == 0 )
				{
					Param.Name = ToANSIString( **SanitizedBase );
				}
			}
		}
	}
}

void DoExportScene( UWorld* World )
{
	ExporterInterface->OnExportBegin();
	
	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
#if WITH_EDITOR

	UObject* ShaderErrorMaterial = StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, TEXT( "/Engine/EngineMaterials/EmissiveTexturedMaterial" ) );
	FString ShaderDidntCompilePath = FString::Printf( TEXT( "/%s/ShaderDidntCompile" ), *ExporterInterface->GetPluginName() );
	UObject* ShaderErrorTexture = StaticLoadObject( UTexture2D::StaticClass(), nullptr, *ShaderDidntCompilePath );
	ShaderErrorTexture2D = Cast<UTexture2D>( ShaderErrorTexture );
	MaterialParentForShaderError = Cast<UMaterialInterface>( ShaderErrorMaterial );
	if( MaterialParentForShaderError && ShaderErrorTexture2D )
	{
		ShaderErrorMID = UMaterialInstanceDynamic::Create( MaterialParentForShaderError, nullptr );
		ShaderErrorMID->SetTextureParameterValue( TEXT( "Texture" ), ShaderErrorTexture2D );
		ShaderErrorMID->AddToRoot();
		MaterialParentForShaderError->AddToRoot();
	}

	GlobalFeatureLevel = GMaxRHIFeatureLevel;
	GlobalQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	GlobalShaderPlatform = GMaxRHIShaderPlatform;

	ProcessingState = 0;
	//Don't clear prefabs when reusing them
	if( !ReusePrefabs )
	{
		AllMaterials.clear();
		MeshList.clear();
		AllTextures.clear();
	}
	MaterialNames.Reset();

	FString AssetsFolder = ExporterInterface->GetAssetsFolder();

	if( ExporterInterface->Type == EngineType::UNITY )
	{		
		bool Mismatch = WarnAboutRenderPipelineMismatch( UnitySettings->ScriptableRenderPipeline );
		if( Mismatch )
			return;
	}

	CurrentWorld = World;

	VerifyOrCreateDirectory( ExporterInterface->Settings->OutputDirectory );
	VerifyOrCreateDirectory( AssetsFolder );

	ExporterInterface->CopyGlobalFiles();

	FString SceneFile = ExporterInterface->GetAssetsFolder();
	SceneFile += *World->GetName();
	SceneFile += GetSceneExtension();

	std::vector< GameObject* > PreviousPrefabs;
	std::vector< UnityMaterial* > PreviousMaterials;
	if( GExporterScene )
	{
		PreviousPrefabs = GExporterScene->Prefabs;
		PreviousMaterials = GExporterScene->Materials;
	}
	GExporterScene = AllocateScene();

	bool UsePrefabs = true;
	if( UnitySettings )
		UsePrefabs = UnitySettings->UsePrefabs;
	GExporterScene->UsePrefabs = UsePrefabs;
	if( GExporterScene->UsePrefabs && ReusePrefabs )
	{
		GExporterScene->Prefabs = PreviousPrefabs;
		GExporterScene->Materials = PreviousMaterials;
	}
	
	AllComponentsInScene.Reset( 0 );
	AnimationsToExport.Reset( 0 );
	Exporters.Reset( 0 );
	ExportTask = nullptr;
	for( TActorIterator<AActor> ActorItr( CurrentWorld ); ActorItr; ++ActorItr )
	{
		AActor* A = *ActorItr;
		//Ignore specific actors
		if( !ShouldExportActor( A ) )
			continue;
		
		TArray<UActorComponent*> comps;
		A->GetComponents( comps );

		for( int i = 0; i < comps.Num(); i++ )
			AllComponentsInScene.Add( comps[ i ] );
	}

	int32 AllActors = CurrentWorld->GetProgressDenominator();

	{
		FScopedSlowTask ActorProgress( AllActors );
		ActorProgress.MakeDialog();

		int ProcessedActors = 0;
		for( TActorIterator<AActor> ActorItr( CurrentWorld ); ActorItr; ++ActorItr )
		{
			auto Current = ActorItr.GetProgressNumerator();
			FString Text = FString::Printf( TEXT( "ProcessingActors %d/%d" ), ProcessedActors, AllActors );
			FText StrTxt = FText::FromString( Text );
			ActorProgress.EnterProgressFrame( 1.0f, StrTxt );

			//Ignore specific actors
			if( !ShouldExportActor( *ActorItr ) )
				continue;

			ProcessActor( *ActorItr, GExporterScene );
			ProcessedActors++;
		}

	}

	TArray<UTexture*> AllUsedTextures;
	int MaterialGUIDCollisions = 0;
	int MaterialShaderGUIDCollisions = 0;
	for( int i = 0; i < AllMaterials.size(); i++ )
	{
		MaterialBinding* Material = AllMaterials[ i ];
		CountTextures( Material, AllUsedTextures );
	}

	{
		int TotalTextureCount = AllUsedTextures.Num();
		FScopedSlowTask TexturesProgress( TotalTextureCount );
		if (!DoAsyncTextureExport )
			TexturesProgress.MakeDialog();

		int PrevCount = 0;
		for( int i = 0; i < AllMaterials.size(); i++ )
		{
			MaterialBinding* Material = AllMaterials[ i ];
			ExportMaterialTextures( Material, AllUsedTextures, TotalTextureCount );
			int CurrentCount = TotalTextureCount - AllUsedTextures.Num();
			if( Material->MaterialInterface )
			{				
				Material->MaterialInstance = Cast<UMaterialInstance>( Material->MaterialInterface );
			}
			if( UnitySettings && UnitySettings->WithMaterialParameters() && !CanHaveShader( Material ) )
			{
				//Assign Shader
				if( Material->MaterialInstance )
				{
					GetMaterialVariables( Material, true );

					for( int u = 0; u < AllMaterials.size(); u++ )
					{
						MaterialBinding* Binding = AllMaterials[u];
						UMaterial* IsMaterial = Cast<UMaterial>( Binding->MaterialInterface );
						UMaterial* BindingBaseMaterial = Binding->MaterialInterface->GetBaseMaterial();
						if (u != i)
						{
							//Binding->MaterialInterface can be ShaderDidntCompile here
							if (Material->BaseMaterial == Binding->BaseMaterial && ( IsMaterial || BindingBaseMaterial == MaterialParentForShaderError ) )
							{
								if (Material->UnityMat->Shader)
									delete Material->UnityMat->Shader;
								Material->UnityMat->Shader = Binding->UnityMat->Shader;

								//Fixes ShaderDidntCompile on MaterialInstances
								if ( Material->UnityMat->Textures.size() == 0 )
								{
									for (int t = 0; t < Binding->UnityMat->Textures.size(); t++)
									{
										Material->UnityMat->Textures.push_back( Binding->UnityMat->Textures[t] );
									}
								}
								//Prevent destroying shared shaders
								break;
							}
						}
					}
				}
				else
					GetMaterialShaderSource( Material, true );
			}
			else
				GetMaterialShaderSource( Material, true );
			if( CurrentCount > PrevCount )
			{
				int Dif = CurrentCount - PrevCount;
				PrevCount = CurrentCount;
				if (!DoAsyncTextureExport )
					TexturesProgress.EnterProgressFrame( Dif, LOCTEXT( "ExportingTextures", "Exporting Textures" ) );
			}
		}
	}

	bool Sync = false;// true;
	RTTI->QueCommand( Sync );

	while( ProcessingState < 1 )
	{
		FPlatformProcess::Sleep( 0.01f );
	}
	
	for( int i = 0; i < AllTextures.size(); i++ )
	{
		auto Binding = AllTextures[ i ];

		UTextureRenderTarget2D* RT2D = Cast<UTextureRenderTarget2D>( Binding->UnrealTexture );
		if( RT2D )
		{
			const EPixelFormat PixelFormat = RT2D->GetFormat();
			ETextureSourceFormat TextureFormat = TSF_Invalid;
			switch( PixelFormat )
			{
				case PF_B8G8R8A8:
					TextureFormat = TSF_BGRA8;
					break;
				case PF_FloatRGBA:
					TextureFormat = TSF_RGBA16F;
					break;
				case PF_G8:
					TextureFormat = TSF_G8;
					break;
				default:
				{
					FText InvalidFormatMessage = NSLOCTEXT( "TextureRenderTarget2D", "UnsupportedFormatRenderTarget2DWarning", "Unsupported format when creating Texture2D from TextureRenderTarget2D. Supported formats are B8G8R8A8, FloatRGBA and G8." );
					//FMessageDialog::Open( EAppMsgType::Ok, InvalidFormatMessage );
				}
			}
			
			if( TextureFormat != TSF_Invalid )
			{
				FRenderTarget* RenderTarget = RT2D->GameThread_GetRenderTargetResource();
				RenderTarget->ReadPixels( Binding->RenderTargetData );
			}
		}
		if( DoAsyncTextureExport )
		{
			Binding->Task = TGraphTask<FExportTextureTask>::CreateTask( NULL, ENamedThreads::GameThread ).ConstructAndHold( Binding );
			Binding->TaskHandle = Binding->Task->GetCompletionEvent();
		}
		
		if( !Binding->UnityTex )
		{
			//UE_LOG( LogEngine, Error, TEXT( "Texture %d has no UnityTex !!" ), i );
			continue;
		}

		std::string TexMetaFile = Binding->UnityTex->File;
		TexMetaFile += ToANSIString( *GetMetaExtension() );

		int sRGB = Binding->UnrealTexture->SRGB;
		UnityTextureShape Shape = UTS_2D;
		int TileX = 0, TileY = 0;
		if( Cast<UTextureCube>( Binding->UnrealTexture ) )
			Shape = UTS_Cube;
		#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 20) || ENGINE_MAJOR_VERSION == 5
		UTexture2DArray* Tex2DArray = Cast< UTexture2DArray>( Binding->UnrealTexture );
		if( Tex2DArray )
		{
			TileX = 1;
			TileY = Tex2DArray->SourceTextures.Num();
			Shape = UTS_2DArray;
		}
		#endif
		UVolumeTexture* VolTex = Cast<UVolumeTexture>( Binding->UnrealTexture );
		if( VolTex )
		{
			Shape = UTS_3D;
			
			UTexture2D* SourceTex = nullptr;
			#if ENGINE_MAJOR_VERSION == 4
				SourceTex = VolTex->Source2DTexture;
			#else
				SourceTex = VolTex->Source2DTexture.Get();
			#endif
			if( SourceTex )
			{
				TileX = SourceTex->Source.GetSizeX() / VolTex->Source2DTileSizeX;
				TileY = SourceTex->Source.GetSizeY() / VolTex->Source2DTileSizeY;
			}
		}
		
		std::string TexMetaContents = ExporterInterface->GenerateTextureMeta( Binding->UnityTex, Binding->IsNormalMap, sRGB, Shape, TileX, TileY );
		if ( ExporterInterface->Settings->ExportTextures )
			::SaveFile( TexMetaFile.c_str(), (uint32*)TexMetaContents.c_str(), TexMetaContents.length() );
	}
	//Wait for all textures to get exported
	if( DoAsyncTextureExport )
	{
		FScopedSlowTask TexturesProgress( AllTextures.size() );
		TexturesProgress.MakeDialog();
		int Finished = 0;

		float LastPercent = 0.0f;

		auto ShowProgressForDelta = [&]( float TargetPercent, const FText& Message = FText::GetEmpty() )
		{
			TargetPercent = FMath::Clamp( TargetPercent, 0.0f, TexturesProgress.TotalAmountOfWork );
			float Delta = TargetPercent - LastPercent;

			if (Delta > 0.0f)
			{
				// Advance progress by the delta and optionally change the message
				TexturesProgress.EnterProgressFrame( Delta, Message );
				LastPercent = TargetPercent;
			}
		};

		//DispatchAll
		for (int i = 0; i < AllTextures.size(); i++)
		{
			TextureBinding* Binding = AllTextures[i];
			Binding->Task->Unlock();
		}

		while ( Finished < AllTextures.size() )
		{
			Finished = 0;
			for( int i = 0; i < AllTextures.size(); i++ )
			{
				TextureBinding* Binding = AllTextures[ i ];
				if (Binding->TaskHandle->IsComplete())
					Finished++;
			}

			FString Text = FString::Printf( TEXT( "Exporting Textures Asynchronously %d/%d" ), Finished, AllTextures.size() );
			FText StrTxt = FText::FromString( Text );
			
			ShowProgressForDelta( (float)Finished, StrTxt );

			FPlatformProcess::Sleep( 0.1f );
		}
	}
	
	for( int i = 0; i < AllMaterials.size(); i++ )
	{
		MaterialBinding* Binding = AllMaterials[ i ];

		FString MatFile;

		if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
		{
			MatFile += GetAssetPathFolder( Binding->MaterialInterface );
		}
		else
		{
			MatFile += TEXT("/Materials/" );
		}
		
		FString NameWide = ToWideString( Binding->UnityMat->Name ).c_str();
		MatFile += NameWide;
		MatFile += GetMaterialExtension();
		Binding->UnityMat->ResourcePath = TCHAR_TO_ANSI( *MatFile );
		MatFile = ExporterInterface->GetAssetsFolder() + MatFile;
		CreateAllDirectories( MatFile );
		Binding->UnityMat->File = TCHAR_TO_ANSI( *MatFile );
		
		//Remap sanitized parameters
		if( UnitySettings && UnitySettings->WithMaterialParameters() && Binding->MaterialInstance )
		{
			MaterialBinding* BaseBinding = GetMaterialBinding( Binding->BaseMaterial );
			if( BaseBinding )
			{
				RemapSanitizedParameters( Binding, BaseBinding );
			}
		}

		std::string MatFileContents = ExporterInterface->GenerateMaterial( Binding->UnityMat );
		std::string MatFileStr = TCHAR_TO_ANSI( *MatFile );
		FixUnicodeCharacters( MatFileStr );
		::SaveFile( MatFileStr.c_str(), (uint32*)MatFileContents.c_str(), MatFileContents.length() );

		if( ExporterInterface->Type == EngineType::UNITY )
		{
			FString MatMetaFile = MatFile;
			MatMetaFile += GetMetaExtension();
			Binding->UnityMat->GenerateGUID();
			std::string MaterialMetaContents = ExporterInterface->GenerateMaterialMeta( Binding->UnityMat->GUID );
			std::string MatMetaFileStr = TCHAR_TO_ANSI( *MatMetaFile );
			FixUnicodeCharacters( MatMetaFileStr );
			::SaveFile( MatMetaFileStr.c_str(), (uint32*)MaterialMetaContents.c_str(), MaterialMetaContents.length() );
		}
	}
	
	{
		FScopedSlowTask MeshesProgress( MeshList.size() );
		MeshesProgress.MakeDialog();

		TArray<UAnimSequence*> AnimationSequences;
		auto GetAllAnimSequencesLambda = [&]( FAssetData& Asset )
		{
			bool Modified = false;
			UAnimSequence* Anim = Cast<UAnimSequence>( Asset.GetAsset() );
			if( Anim )
			{
				AnimationSequences.Add( Anim );
			}

			return Modified;
		};
		
		if( UnitySettings && UnitySettings->ExportAllAnimations )
		{
			IterateOverAllAssetsOfType( FName( "AnimSequence" ), GetAllAnimSequencesLambda, false );
		}
		for( int i = 0; i < MeshList.size(); i++ )
		{
			MeshBinding* MB = MeshList[ i ];
			UnityMesh* M = MB->TheUnityMesh;

			FString MeshFile = M->File.c_str();

			UE_LOG( LogTemp, Log, TEXT( "Writing Mesh %s (%d/%d)" ), *MeshFile, i + 1, MeshList.size() );

			FString Filename = MeshFile;
			CreateAllDirectories( Filename );
			
			if( ExporterInterface->Type == EngineType::GODOT )
			{
				bool ShowOptions = ( i == 0 );
				bool Status = ExporterInterface->DoMeshExport( MB, ShowOptions );
				//User may have cancelled
				if( !Status )
					break;
			}
			#ifdef ENABLE_UNITY_EXPORT
			else
				ExportFBXForActor( MB->OwnerActor, Filename );
			#endif
			
			if( UnitySettings && UnitySettings->ExportAllAnimations && MB->UnrealSkeletalMesh )
			{
				for( int a = 0; a < AnimationSequences.Num(); a++ )
				{
					UAnimSequence* Anim = AnimationSequences[ a ];
					
					if( GetSkeleton( MB->UnrealSkeletalMesh ) == Anim->GetSkeleton() )
					{
						AnimationEntry NewAnimationEntry;

						NewAnimationEntry.SkeletalMesh = MB->UnrealSkeletalMesh;
						NewAnimationEntry.AnimSeq = Anim;
						NewAnimationEntry.MeshName = MB->UnrealSkeletalMesh->GetName();
						
						NewAnimationEntry.GenerateGUID();
							
						AnimationsToExport.Add( NewAnimationEntry );
					}					
				}
			}

			if( MB->UnrealSkeletalMesh && AnimationsToExport.Num() > 0 )
			{
				#ifdef ENABLE_UNITY_EXPORT
					ExportAllAnimations( MeshesProgress, MB );
				#endif
			}
			else
			{
				FString Text = FString::Printf( TEXT( "Exporting Meshes %d/%d" ), i + 1, MeshList.size() );
				FText StrTxt = FText::FromString( Text );
				MeshesProgress.EnterProgressFrame( 1.0f, StrTxt );
			}

			FString MeshMetaFile = MeshFile;
			MeshMetaFile += GetMetaExtension();

			std::string MetaData = ExporterInterface->GenerateMeshMeta( M, ( CVarFBXPerActor.GetValueOnAnyThread() == 1 ), MB->TotalLods );
			int MetaDataLen = MetaData.length();
			std::string MeshMetaFileStr = ToANSIString( *MeshMetaFile );
			::SaveFile( MeshMetaFileStr.c_str(), (uint32*)MetaData.c_str(), MetaDataLen );
		}
	}

	FString ProxySceneFile = GetResourceDir();
	ProxySceneFile += TEXT( "Proxy.unity" );

	if( GExporterScene->UsePrefabs )
	{
		FString PrefabFolder = ExporterInterface->GetAssetsFolder();
		PrefabFolder += TEXT("Prefabs/" );
		VerifyOrCreateDirectory( PrefabFolder );
		GExporterScene->FixDuplicatePrefabNames();
		std::string PrefabFolderStr = ToANSIString( *PrefabFolder );
		ExporterInterface->WritePrefabs( GExporterScene, PrefabFolderStr.c_str() );
	}

	if( GameObjectForTrees )
	{
		GExporterScene->Add( GameObjectForTrees );
	}
	ExporterInterface->WriteScene( GExporterScene, ToANSIString(*SceneFile).c_str(), ToANSIString(*ProxySceneFile).c_str() );
	
#ifdef ENABLE_UNITY_EXPORT
	//Reset these to give a chance of changing them on subsequent exports
	extern UFbxExportOption* GlobalFBXExportOptions;
	GlobalFBXExportOptions = nullptr;
#endif

	if ( ExporterInterface->Settings->ExportAllSounds )
		ExportSounds();

	if( ExporterInterface->Type == EngineType::UNITY )
	{
		FString FinishText = FString::Printf( TEXT(
			"Export Complete !\n\n"
			"For URP/HDRP you need Unity 2021.2 minimum\n"
			"Don't mix Builtin/URP/HDRP exports into the same folder\n"
			"Don't save the current scene because the export process modifies some assets in memory\n" ) );
		FText Txt = FText::FromString( FinishText );
		FMessageDialog::Open( EAppMsgType::Ok, Txt );
	}
	else
	{
		ExporterInterface->OnExportFinished();
	}
	#endif
}
