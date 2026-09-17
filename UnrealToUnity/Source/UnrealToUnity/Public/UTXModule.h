// Copyright Ciprian Stanciu 2024

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "Modules/ModuleManager.h"
#include <functional>
#include "EditorStyleSet.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/Commands.h"
#include "Containers/Array.h"
#include "BaseScene.h"
#include "PluginConfig.h"
#include "UTXModule.generated.h"

class UStaticMesh;
class UMaterial;
class UMaterialFunction;
class UMaterialExpression;
struct FExpressionInput;


UCLASS( config = EditorPerProjectUserSettings, MinimalAPI, BlueprintType )
class UUnitySettingsClass : public UBaseSettingsClass
{
	GENERATED_BODY()
public:
	
	UUnitySettingsClass();

	bool WithMaterialParameters();
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	RenderPipeline ScriptableRenderPipeline;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	UnityShadersType Shaders;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters, DisplayName = "ExposeParameters(experimental,requires UE5.4+)" )
	bool ExposeMaterialParameters;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ShowFBXExportDialog;	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ExportLODs;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ExportVertexColors;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	TangentToWorldLocationType TangentToWorldLocation;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ExportAllAnimations;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	FoliageExportType FoliageExport;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool UsePrefabs;
};

UCLASS( config = EditorPerProjectUserSettings, MinimalAPI, BlueprintType )
class UBakeTextureSettingsClass : public UObject
{
	GENERATED_BODY()
public:
	UBakeTextureSettingsClass();
	void Initialize();

	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	RenderPipeline ScriptableRenderPipeline;

	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	int UVChannel;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	int TargetResolution;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	int LOD;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool UseMeshData;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool OneTextureSetPerMaterial;
};

extern UBakeTextureSettingsClass* BakeTextureSettings;

class UDynamicMeshComponent;
class UProceduralMeshComponent;

class FUnrealToXModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void BindEditorCommands();
	void AddCustomMenu( FToolBarBuilder& ToolbarBuilder );
	TSharedRef<SWidget> CreateMenuContent();
	void ExportToUnity();
	void ExportToGodot();
	static void DoExport( );
	static void RemoveVirtualTextures();
	static void DoRemoveNanite();
	static void RemoveRVTs();
	static void ExportRVTTextures();
	static void RemoveVertexInterpolators();
	static void RemoveUnsupportedMaterialNodes();
		static void RemoveDitherTemporalAA();
		static void RemoveParallaxMapping();
	static void MoveFoliageToActors();
	static void FixVolumeMaterials();
	static void ConvertISMToActors();
	static void ConvertGeometryCachesToStaticMeshes();
	void PlaceSelectedAssetsInScene();
	void PlaceCubesWithSelectedMaterials();
	void ConvertLandscapesToStaticMeshes();
	static void DetachMultipleComponentActors();
	static void ConvertSplineMeshesToStaticActor( AActor* Actor );
	static void ConvertSplineMeshesAndDynamicMeshesToStaticActors();
	static void ConvertSkeletalMeshesToStaticActors();
	static void ConvertBrushesToStaticActors();
	static bool ConvertDynamicMeshToStaticMesh( FString NewAssetPath, UDynamicMeshComponent* DynamicMeshComponent, UStaticMesh*& OutStaticMesh );
	static bool ConvertProceduralMeshToStaticMesh( FString NewAssetPath, UProceduralMeshComponent* DynamicMeshComponent, UStaticMesh*& OutStaticMesh );
	void FixLandscapeMaterials();
	void PrepareForExport();
	static void DoPrepareForExport();
	void SpawnAllMaterialNodes();
	void RemoveSkinChunking();
	void BakeTextures();
	//void GetTerrainData();
	static void ReorderMaterials();
	static void FixSkeletalMeshBoneInfluences();
	void RevertSubstrateMaterials();
	void ExportRVTTexture();
	void PrepareForRelease();

	TSharedPtr<FUICommandList> EditorCommands;
};


class FUnrealToUnity_Commands : public TCommands<FUnrealToUnity_Commands>
{
public:
	FUnrealToUnity_Commands()
		: TCommands<FUnrealToUnity_Commands>( "UnrealToUnityEditor", NSLOCTEXT( "Contexts", "UnrealToUnityEditor", "UnrealToUnityEditor" ), NAME_None, 
		#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
			FAppStyle::GetAppStyleSetName()
		#else
			FEditorStyle::GetStyleSetName()
		#endif
		)
	{
	}

	TSharedPtr<FUICommandInfo> ExportToUnity;
	TSharedPtr<FUICommandInfo> ExportToGodot;
	TSharedPtr<FUICommandInfo> PlaceSelectedAssetsInScene;
	TSharedPtr<FUICommandInfo> ConvertLandscapesToStaticMeshes;
	TSharedPtr<FUICommandInfo> PrepareForExport;
	TSharedPtr<FUICommandInfo> SpawnAllMaterialNodes;
	TSharedPtr<FUICommandInfo> BakeTextures;
	//TSharedPtr<FUICommandInfo> GetTerrainData;
	TSharedPtr<FUICommandInfo> PlaceCubesWithSelectedMaterials;
	TSharedPtr<FUICommandInfo> RevertSubstrateMaterials;
	TSharedPtr<FUICommandInfo> ExportRVTTexture;
	TSharedPtr<FUICommandInfo> PrepareForRelease;
	
	virtual void RegisterCommands() override;
};

void IterateOverAllAssetsOfType( FName TypeName, std::function<bool( FAssetData& )> lambda, bool SaveAndUnload = true );
void EnsureSettingsExists();
void GetOutputExpression( UMaterial* Material, UMaterialExpression* Source, int OutputIndex, TArray< FExpressionInput*>& Inputs );
void GetOutputExpression( UMaterial* Material, UMaterialFunction* MaterialFunction, UMaterialExpression* Source, int OutputIndex, TArray< FExpressionInput*>& Inputs );
int64_t LoadFile( const char* File, uint8** data );
bool SaveFile( const char* File, void* data, int64_t size );