// Copyright Ciprian Stanciu 2024

#pragma once

#include "PluginConfig.h"

#include "Runtime/Launch/Resources/Version.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureDefines.h"
#include "UTXModule.h"
#include "BaseScene.h"
#include <string>
#ifdef ENABLE_GODOT_EXPORT
	#include "Builders/GLTFContainerBuilder.h"
#endif

#include "Misc/ScopedSlowTask.h"
#include "Interfaces/IPluginManager.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	#include "Runtime/Engine/Public/StaticMeshComponentLODInfo.h"

	#include "MaterialDomain.h"
	#include "Materials/MaterialRenderProxy.h"
	#include "Engine/SkinnedAssetCommon.h"
	#include "Animation/BuiltInAttributeTypes.h"
#endif

class UMaterial;
class UMaterialInstance;
class FMaterialResource;
class FMaterialRenderProxy;
class USkeletalMesh;
class UPrimitiveComponent;
class UAnimSequence;
class UMaterialExpression;
class USkeletalMeshComponent;
struct FExpressionInput;
struct FScalarMaterialInput;
struct FColorMaterialInput;
class UMaterialFunctionInterface;
class ULightComponent;
class LightComponent;
class AdditionalLightData;
class USkyLightComponent;
class USkeleton;
class UMorphTarget;

void DoExportScene( UWorld *World );
int64_t LoadFile( const char *File, uint8** data );
bool SaveFile( const char *File, void *data, int64_t size );
bool VerifyOrCreateDirectory( const wchar_t * TestDir );
bool VerifyOrCreateDirectory( const FString& TestDir );
void ExportFBXForActor( AActor* Actor, FString CurrentFilename );
int StringReplacement( std::string& OriginalString, const char* What, const char* Replace, int StartPos = 0 );
std::string GetLine( std::string String, int position );
void SanitizeParamName( FString& OriginalName );
FString GenerateAnimationFilePath( const UAnimSequence* Anim );
FString GetAssetPathFolder( const UObject* Obj );
FString GetMetaExtension();
bool HasOnlyShadingModel( UMaterial* Mat, EMaterialShadingModel ShadingModel );
UBodySetup* GetBodySetup( const UStaticMesh* StaticMesh );
UTexture2D* GetHeightmap( class ULandscapeComponent* LandscapeComponent );
#if ENGINE_MAJOR_VERSION == 4
void PNGCompressImageArray( int32 ImageWidth, int32 ImageHeight, const TArray<FColor>& SrcData, TArray<uint8>& DstData );
#else
void PNGCompressImageArray( int32 ImageWidth, int32 ImageHeight, const TArray<FColor>& SrcData, TArray64<uint8>& DstData );
#endif
void ShowWarningIfFileIsMissing( FString SourceFile );

#if PLATFORM_WINDOWS
	std::string ToANSIString( std::wstring w );
	std::wstring ToWideString( std::string str );
#else
	std::string ToANSIString( std::u16string w );
	std::u16string ToWideString( std::string str );
	#define stricmp strcasecmp 
#endif

void ReplaceHLSLSampleCall( std::string& Exp, const char* strToFind, const char* ReplacementFunc );



struct MaterialBinding
{
	MaterialBinding();
	FString GenerateName();

	class UMaterialInterface *MaterialInterface = nullptr;
	class UnityMaterial *UnityMat = nullptr;
	//FString ShaderSource;

	//IntermediaryData
	UMaterial*			BaseMaterial = nullptr;
	UMaterialInstance*	MaterialInstance = nullptr;
	//FMaterialResource*	MaterialResource = nullptr;
	FMaterialRenderProxy*	RenderProxy = nullptr;
	int ID = 0;
	TArray<FString> TextureParamNames;
	TMap<FString, FString> SanitizedParamNames;
	FString GetSanitizedParamName( FString ParamName );
};

MaterialBinding* GetMaterialBinding( UMaterialInterface* M );
float GetSequenceLength( const UAnimSequence* AnimSeq );
int32 GetNumberOfFrames( const UAnimSequence* AnimSeq );
TArray<UMorphTarget*> GetMorphTargets( const USkeletalMesh* SkeletalMesh );
const TArray<FSkeletalMaterial>& GetMaterials( const USkeletalMesh* SkeletalMesh );
USkeletalMesh* GetSkeletalMesh( USkinnedMeshComponent* SMC );
std::string FixUnicodeCharacters( std::string& In );
FString FixUnicodeCharacters( FString& In );

class UnityMesh;
class UnityTexture;

class MeshBinding
{
public:
	UnityMesh* TheUnityMesh = nullptr;
	UStaticMesh* UnrealStaticMesh = nullptr;
	USkeletalMesh* UnrealSkeletalMesh = nullptr;
	UPrimitiveComponent* MeshComp = nullptr;
	AActor* OwnerActor = nullptr;
	int LOD = 0;
	int TotalLods = -1;
	TArray<UAnimSequence*> AnimSequences;
};

struct AnimationEntry
{
	const USkeletalMesh* SkeletalMesh;
	UAnimSequence* AnimSeq;
	FString MeshName;
	TArray<UMaterialInterface*> OverrideMaterials;
	std::string GUID;
	void GenerateGUID( );
};

void ExportAllAnimations( FScopedSlowTask& MeshesProgress, MeshBinding* MB );
MeshBinding* GetMeshBinding( UnityMesh* Mesh );

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26

struct FPreshaderDataContext
{
	explicit FPreshaderDataContext( const FMaterialPreshaderData& InData )
		: Ptr( InData.Data.GetData() )
		, EndPtr( Ptr + InData.Data.Num() )
		, Names( InData.Names.GetData() )
		, NumNames( InData.Names.Num() )
	{
	}

	explicit FPreshaderDataContext( const FPreshaderDataContext& InContext, const FMaterialUniformPreshaderHeader& InHeader )
		: Ptr( InContext.Ptr + InHeader.OpcodeOffset )
		, EndPtr( Ptr + InHeader.OpcodeSize )
		, Names( InContext.Names )
		, NumNames( InContext.NumNames )
	{
	}

	const uint8* RESTRICT Ptr;
	const uint8* RESTRICT EndPtr;
	const FScriptName* RESTRICT Names;
	int32 NumNames;
};

#endif
#if 0//ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3

#include "Shader/ShaderTypes.h"
#include "Shader/PreshaderTypes.h"
#include "Runtime/Engine/Private/Shader/PreshaderEvaluate.h"
//Causes a weird template error in 5.4 !!!
using namespace UE::Shader;

struct FPreshaderDataContext2
{
	FPreshaderDataContext2( const FPreshaderData& InData )
		: Ptr( InData.Data.GetData() )
		, EndPtr( Ptr + InData.Data.Num() )
		, Names( InData.Names )
		, StructTypes( InData.StructTypes )
		, StructComponentTypes( InData.StructComponentTypes )
	{
	}

	FPreshaderDataContext2( const FPreshaderDataContext2& InContext, uint32 InOffset, uint32 InSize )
		: Ptr( InContext.Ptr + InOffset )
		, EndPtr( Ptr + InSize )
		, Names( InContext.Names )
		, StructTypes( InContext.StructTypes )
		, StructComponentTypes( InContext.StructComponentTypes )
	{
	}
	const uint8* RESTRICT Ptr;
	const uint8* RESTRICT EndPtr;
	TArrayView<const FScriptName> Names;
	TArrayView<const FPreshaderStructType> StructTypes;
	TArrayView<const EValueComponentType> StructComponentTypes;
};
#endif

TArray<UMaterialExpression*> GetExpressions( UMaterial* Material );
const TArray<FStaticMaterial>& GetStaticMaterials( const UStaticMesh* StaticMesh );
class UExporter;
bool UseUEExporter( UExporter* ExporterToUse, UObject* ObjectToExport, FString Filename, bool ShowOptions = true, UObject* OverrideOptions = nullptr );
UExporter* GetExporterForAsset( UObject* Asset, FString* RequiredName = nullptr );

class TextureBinding;

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	#define TArray64 TArray
#endif

class UETextureExporter
{
public:
	static void ConvertToColors( TArray64<uint8>& SourceData, ETextureSourceFormat Format, int32 Width, int32 Height, int32 Depth, TArray<FColor>& InputImageData, FString TextureFileName );
	static bool DoExport( TextureBinding* Binding );
	static bool CompressAndSaveTexture( FString TextureFileName, TArray<FColor>& InputImageData, int Width, int Height );
	static TextureBinding* Find( UTexture* Tex );
	static TextureBinding* Export( UTexture* Tex );
};

class FExportTextureTask
{

public:
	FExportTextureTask( TextureBinding* Binding );
	static const TCHAR* GetTaskName();
	FORCEINLINE TStatId GetStatId() const;

	ENamedThreads::Type GetDesiredThread();

	static ESubsequentsMode::Type GetSubsequentsMode();
	void DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent );

	TextureBinding* Binding = nullptr;
};

class TextureBinding
{
public:
	UnityTexture* UnityTex = nullptr;
	UTexture* UnrealTexture = nullptr;
	bool IsNormalMap = false;
	TGraphTask<FExportTextureTask>* Task = nullptr;
	FGraphEventRef TaskHandle = nullptr;
	TArray<FColor> RenderTargetData;
};

TArray<UMaterialExpression*> GetMaterialExpressionsIncludingFunctions( UMaterialInterface* Mat );
const TArray<UMaterialExpression*> GetFunctionExpressions( UMaterialFunctionInterface* MaterialFunction );
TArray<UMaterialExpression*> GetExpressions( UMaterial* Material );
FExpressionInput* GetMaterialAttributes( UMaterial* Material );
FScalarMaterialInput* GetOpacity( UMaterial* Material );
FScalarMaterialInput* GetOpacityMask( UMaterial* Material );
FColorMaterialInput* GetBaseColor( UMaterial* Material );
FColorMaterialInput* GetEmissiveColor( UMaterial* Material );
FExpressionInput* GetNormal( UMaterial* Material );
FExpressionInput* GetMetallic( UMaterial* Material );
FExpressionInput* GetRoughness( UMaterial* Material );
FExpressionInput* GetSpecular( UMaterial* Material );
FExpressionInput* GetAnisotropy( UMaterial* Material );
FExpressionInput* GetTangent( UMaterial* Material );
FExpressionInput* GetWorldPositionOffset( UMaterial* Material );
FExpressionInput* GetSubsurfaceColor( UMaterial* Material );
FExpressionInput* GetClearCoat( UMaterial* Material );
FExpressionInput* GetClearCoatRoughness( UMaterial* Material );
FExpressionInput* GetAmbientOcclusion( UMaterial* Material );
FExpressionInput* GetRefraction( UMaterial* Material );
FExpressionInput* GetPixelDepthOffset( UMaterial* Material );
FExpressionInput* GetShadingModelFromMaterialExpression( UMaterial* Material );
FExpressionInput* GetCustomizedUVs( UMaterial* Material, int i );
float ConvertSpotAngle( float OuterConeAngle );
UAnimSequence* GetAnimationSequence( USkeletalMeshComponent* SkeletalMeshComp );
void CopyToOutput( FString SourceFile, FString OutFilePath, bool Overwrite = true );
const FStaticMeshRenderData* GetRenderData( const UStaticMesh* StaticMesh );
const USkeleton* GetSkeleton( const USkeletalMesh* SkeletalMesh );
UPackage* CreatePackageVersioned( FString PackageName );
#if ENGINE_MAJOR_VERSION == 4
	TArray<UMaterialInterface*> GetArrayOfMaterials( TArray<UMaterialInterface*>& ArrayType );
#else
	TArray<UMaterialInterface*> GetArrayOfMaterials( TArray<TObjectPtr<class UMaterialInterface>>& NewArrayType );
#endif
AnimationEntry* AddAnimationEntry( const USkeletalMesh* SkeletalMesh, UAnimSequence* AnimSeq, const TCHAR* MeshName, const TArray<UMaterialInterface*>* OverrideMaterials );

class IUnrealExporterInterface
{
public:
	EngineType Type;
	UBaseSettingsClass* Settings = nullptr;
	USkyLightComponent* SkylightComponent = nullptr;

	virtual ~IUnrealExporterInterface() {};

	virtual void GenerateShader( MaterialBinding* Mat, FString ShaderFileName ) {};
	virtual void TranslateLightData( ULightComponent* LightComp, LightComponent* NewLightComponent, AdditionalLightData* LightData ) {};
	virtual void CopyGlobalFiles() {};
	virtual bool DoMeshExport( MeshBinding* MB, bool ShowOptions ) { return false; }
	virtual void OnExportBegin() {};
	virtual void OnExportFinished() {};

	//Landscapes
	virtual FString GetLandscapeWeightString() { return TEXT( "UNIMPLEMENTED" ); };
	virtual void TransformLandscapeTexcoordScale( FLinearColor& Value) {};

	virtual std::string TransformComponent_GetSceneData( TransformComponent* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string MeshRenderer_GetSceneData( MeshRenderer* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string SkinnedMeshRenderer_GetSceneData( SkinnedMeshRenderer* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string AnimationComponent_GetSceneData( AnimationComponent* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string MeshFilter_GetSceneData( MeshFilter* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string LodGroup_GetSceneData( LodGroup* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string MeshCollider_GetSceneData( MeshCollider* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string BoxCollider_GetSceneData( BoxCollider* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string SphereCollider_GetSceneData( SphereCollider* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string CapsuleCollider_GetSceneData( CapsuleCollider* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string LightComponent_GetSceneData( LightComponent* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string ReflectionProbeComponent_GetSceneData( ReflectionProbeComponent* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string DecalComponent_GetSceneData( DecalComponent* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string AdditionalCameraData_GetSceneData( AdditionalCameraData* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string AdditionalLightData_GetSceneData( AdditionalLightData* THIS ) { return "UNIMPLEMENTED"; };
	virtual std::string TreeInstancesComponent_GetSceneData( TreeInstancesComponent* THIS ) { return "UNIMPLEMENTED"; };

	virtual std::string GenerateTextureMeta( UnityTexture* Tex, bool IsNormalMap, int sRGB, UnityTextureShape Shape, int TileX, int TileY ) { return "UNIMPLEMENTED"; };
	virtual std::string GenerateMaterialMeta( std::string GUID ) { return "UNIMPLEMENTED"; };
	virtual std::string GenerateMeshMeta( UnityMesh* Mesh, bool IsFBX, int TotalLods ) { return "UNIMPLEMENTED"; };
	virtual std::string GenerateShaderMeta( std::string GUID ) { return "UNIMPLEMENTED"; };
	virtual std::string GenerateAnimationFBXMeta( std::string GUID, UAnimSequence* Anim ) { return "UNIMPLEMENTED"; };

	virtual void WriteScene( ExporterScene* Scene, const char* File, const char* ProxyScene ) {};
	virtual void WritePrefabs( ExporterScene* Scene, const char* PrefabFolder ) {};
	virtual std::string GenerateMaterial( UnityMaterial* Material ) { return "UNIMPLEMENTED"; };
	virtual std::string GetGUIDFromSizeT( size_t Input ) { return "UNIMPLEMENTED"; };
	virtual FString GetDefaultBaseOutputFolder() { return "UNIMPLEMENTED"; };
	virtual FString GetPluginName() { return "UNIMPLEMENTED"; };
	virtual FString GetPluginVersion();
	FString GetAssetsFolder()
	{
		EnsureSettingsExists();

		FString Folder = Settings->OutputDirectory;
		if( Type == EngineType::UNITY )
			Folder += TEXT( "/Assets/" );

		return Folder;
	}

	virtual UBaseSettingsClass* NewSettings() { return NewObject<UBaseSettingsClass>();};
	FPluginDescriptor GetPluginDescriptor();
};

#ifdef ENABLE_UNITY_EXPORT
class UnityExporter : public IUnrealExporterInterface
{
public:
	UUnitySettingsClass* UnitySettings = nullptr;

	UnityExporter();
	virtual ~UnityExporter() {};
	
	virtual void TranslateLightData( ULightComponent* LightComp, LightComponent* NewLightComponent, AdditionalLightData* LightData );
	virtual void CopyGlobalFiles();
	virtual FString GetLandscapeWeightString();
	virtual void OnExportBegin();

	virtual std::string TransformComponent_GetSceneData( TransformComponent* THIS );
	virtual std::string MeshRenderer_GetSceneData( MeshRenderer* THIS );
	virtual std::string SkinnedMeshRenderer_GetSceneData( SkinnedMeshRenderer* THIS );
	virtual std::string AnimationComponent_GetSceneData( AnimationComponent* THIS );
	virtual std::string MeshFilter_GetSceneData( MeshFilter* THIS );
	virtual std::string LodGroup_GetSceneData( LodGroup* THIS );
	virtual std::string MeshCollider_GetSceneData( MeshCollider* THIS );
	virtual std::string BoxCollider_GetSceneData( BoxCollider* THIS );
	virtual std::string SphereCollider_GetSceneData( SphereCollider* THIS );
	virtual std::string CapsuleCollider_GetSceneData( CapsuleCollider* THIS );
	virtual std::string LightComponent_GetSceneData( LightComponent* THIS );
	virtual std::string ReflectionProbeComponent_GetSceneData( ReflectionProbeComponent* THIS );
	virtual std::string DecalComponent_GetSceneData( DecalComponent* THIS );
	virtual std::string AdditionalCameraData_GetSceneData( AdditionalCameraData* THIS );
	virtual std::string AdditionalLightData_GetSceneData( AdditionalLightData* THIS );
	virtual std::string TreeInstancesComponent_GetSceneData( TreeInstancesComponent* THIS ) ;

	virtual std::string  GenerateTextureMeta( UnityTexture* Tex, bool IsNormalMap, int sRGB, UnityTextureShape Shape, int TileX, int TileY );
	virtual std::string GenerateMaterialMeta( std::string GUID );
	virtual std::string GenerateMeshMeta( UnityMesh* Mesh, bool IsFBX, int TotalLods );
	virtual std::string GenerateShaderMeta( std::string GUID );
	virtual std::string GenerateAnimationFBXMeta( std::string GUID, UAnimSequence* Anim );

	virtual void WriteScene( ExporterScene* Scene, const char* File, const char* ProxyScene );
	virtual void WritePrefabs( ExporterScene* Scene, const char* PrefabFolder );
	virtual std::string GenerateMaterial( UnityMaterial* Material );
	virtual std::string GetGUIDFromSizeT( size_t Input );
	virtual FString GetDefaultBaseOutputFolder();
	virtual FString GetPluginName();

	virtual UBaseSettingsClass* NewSettings();
};
#endif

IUnrealExporterInterface* NewUnrealExporter();
extern IUnrealExporterInterface* ExporterInterface;