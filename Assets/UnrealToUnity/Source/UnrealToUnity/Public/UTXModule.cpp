// Copyright Ciprian Stanciu 2024

#include "UTXModule.h"
#include "ExportToUnity.h"
#ifdef ENABLE_GODOT_EXPORT
	#include "GodotScene.h"
#endif
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "LevelEditor.h"
#include "ExportActor.h"

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	#include "AssetRegistryModule.h"
#else
	#include "AssetRegistry/AssetRegistryModule.h"
#endif
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include <functional>

#include "Editor/MaterialEditor/Public/MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSampleParameter.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionCustom.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "Runtime/Engine/Classes/Exporters/Exporter.h"
#include "VT/VirtualTextureBuilder.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "Misc/MessageDialog.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialFunction.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "UObject/MetaData.h"

#if ENGINE_MAJOR_VERSION >= 5
	#include "Materials/MaterialExpressionComposite.h"
	#include "StaticMeshCompiler.h"
	#include "ShaderCompiler.h"
	#include "TextureCompiler.h"
	#include "DynamicMesh/DynamicMesh3.h"
	#include "DynamicMeshToMeshDescription.h"
	#include "Components/DynamicMeshComponent.h"
	#include "AssetCompilingManager.h"
#endif
#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION >= 1 && ENGINE_MINOR_VERSION <= 3 )
	#include "Materials/MaterialExpressionStrata.h"
#endif
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
	#include "StaticMeshComponentLODInfo.h"
	#include "MaterialDomain.h"
	#include "Engine/SkinnedAssetCommon.h"
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	#include "Materials/MaterialExpressionSubstrate.h"
#endif

#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "LandscapeGrassType.h"
#include "InstancedFoliageActor.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/DecalActor.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DecalComponent.h"
#include "Components/BrushComponent.h"
#include "Rendering/SkeletalMeshModel.h"

#include "ExportLandscape.h"

#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "RawMesh.h"
#include "SkeletalRenderPublic.h"
#include "Engine/RendererSettings.h"
#include "Editor/UnrealEd/Public/FileHelpers.h"
#include "Editor/UnrealEd/Classes/Analytics/AnalyticsPrivacySettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "Runtime/Engine/Public/Rendering/SkeletalMeshRenderData.h"

//Function params
#include "IStructureDetailsView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "PropertyEditorModule.h"
//BakeTextures
#include "Developer/MaterialBaking/Public/MaterialOptions.h"
#include "Developer/MaterialBaking/Public/IMaterialBakingModule.h"
#include "Developer/MeshMergeUtilities/Public/StaticMeshComponentAdapter.h"
#include "Developer/MeshMergeUtilities/Public/MeshMergeModule.h"

TAutoConsoleVariable<int> CVarMaxFoliageActors(
	TEXT( "utu.MaxFoliageActors" ),
	100000,
	TEXT( "Max foliage instance threshold after which you get a warning and ability to skip foliage" ),
	ECVF_Cheat );

extern TAutoConsoleVariable<int> CVarRenderPipeline;
extern FAssetToolsModule* AssetToolsModule;

#define LOCTEXT_NAMESPACE "FUnrealToXModule"

void FUnrealToXModule::StartupModule()
{
	//return;
	FUnrealToUnity_Commands::Register();
	BindEditorCommands();
	
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable( new FExtender );
	ToolbarExtender->AddToolBarExtension(
	#if ENGINE_MAJOR_VERSION == 4
		"Game",
	#else
		"File",
	#endif
		EExtensionHook::After,
		NULL,
		FToolBarExtensionDelegate::CreateRaw( this, &FUnrealToXModule::AddCustomMenu )
	);

	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender( ToolbarExtender );
}


bool PreparedForExport = false;
void FUnrealToXModule::DoExport( )
{
	UEditorLoadingSavingSettings* EditorSettings = GetMutableDefault< UEditorLoadingSavingSettings >();
	//Disable autosave so modified assets don't get saved
	if( EditorSettings )
	{
		EditorSettings->bAutoSaveEnable = 0;
	}

	UAnalyticsPrivacySettings* AnalyticsPrivacySettings = GetMutableDefault<UAnalyticsPrivacySettings>();
	//Disable analytics as sometimes I got to 40MB/s of analytics written which may impact export performance
	if( AnalyticsPrivacySettings && AnalyticsPrivacySettings->bSendUsageData )
	{
		AnalyticsPrivacySettings->bSendUsageData = false;
	}

	DoPrepareForExport();	

	AExportActor::DoExport( );
	PreparedForExport = false;
}
UBaseSettingsClass* UTXBaseSettings = nullptr;
class SFunctionParamDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SFunctionParamDialog) {}

	/** Text to display on the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonText)

	/** Tooltip text for the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonTooltipText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow)
	{
		bOKPressed = false;

		// Initialize details view
		FDetailsViewArgs DetailsViewArgs;
		{
			DetailsViewArgs.bAllowSearch = false;
			DetailsViewArgs.bHideSelectionTip = true;
			DetailsViewArgs.bLockable = false;
			DetailsViewArgs.bSearchInitialKeyFocus = true;
			DetailsViewArgs.bUpdatesFromSelection = false;
			DetailsViewArgs.bShowOptions = false;
			DetailsViewArgs.bShowModifiedPropertiesOption = false;
			#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
				DetailsViewArgs.bShowObjectLabel = false;
			#else
				DetailsViewArgs.bShowActorLabel = false;
			#endif
			DetailsViewArgs.bForceHiddenPropertyVisibility = false;
			DetailsViewArgs.bShowScrollBar = false;
			#ifdef ENABLE_GODOT_EXPORT
				DetailsViewArgs.ColumnWidth = 0.6f;
			#else
				DetailsViewArgs.ColumnWidth = 0.5f;
			#endif
		}
	
		FStructureDetailsViewArgs StructureViewArgs;
		{
			StructureViewArgs.bShowObjects = true;
			StructureViewArgs.bShowAssets = true;
			StructureViewArgs.bShowClasses = true;
			StructureViewArgs.bShowInterfaces = true;
		}

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		
		StructureDetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );

		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					StructureDetailsView.ToSharedRef()
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(6, 2))
						.OnClicked_Lambda([this, InParentWindow, InArgs]()
						{
							if(InParentWindow.IsValid())
							{
								InParentWindow.Pin()->RequestDestroyWindow();
							}
							bOKPressed = true;
							return FReply::Handled(); 
						})
						.ToolTipText(InArgs._OkButtonTooltipText)
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text(InArgs._OkButtonText)
						]
					]
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(6, 2))
						.OnClicked_Lambda([InParentWindow]()
						{ 
							if(InParentWindow.IsValid())
							{
								InParentWindow.Pin()->RequestDestroyWindow();
							}
							return FReply::Handled(); 
						})
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text(LOCTEXT("Cancel", "Cancel"))
						]
					]
				]
			]
		];

		//StructureDetailsView->SetObject( TargetObject );
	}

	bool bOKPressed;
public:
	TSharedPtr<class IDetailsView> StructureDetailsView;
};
FString GetDefaultOutputFolder()
{
	FString BaseFolderName = TEXT( "" );
	if ( ExporterInterface )
		BaseFolderName = ExporterInterface->GetDefaultBaseOutputFolder();

	#if PLATFORM_WINDOWS
		FString StrOutputFolder = FString::Printf( TEXT( "C:\\%s" ), *BaseFolderName );
	#elif PLATFORM_LINUX
		FString StrOutputFolder = FString::Printf( TEXT( "%s%s" ), FGenericPlatformProcess::UserHomeDir(), *BaseFolderName );
	#else
		FString StrOutputFolder = FString::Printf( TEXT( "/Users/Shared/%s" ), *BaseFolderName );
	#endif

	return StrOutputFolder;
}
void UBaseSettingsClass::Initialize()
{
	OutputDirectory = GetDefaultOutputFolder();	
	SkinnedMeshes = SkinnedMeshOptionsEnum::SMO_EXPORT_AS_SKINNED;
	NaniteOptions = NaniteOptionsEnum::NANITE_COARSE_MESH;
	ExportVertexShader = true;
	ExportBrushes = false;
	ExportPhysics = true;
	ExportTextures = true;
	ExportAllSounds = false;
}
UBaseSettingsClass::UBaseSettingsClass()
{
	Initialize();
}
UUnitySettingsClass::UUnitySettingsClass()
{
	//Engine = EngineType::UNITY;
	ScriptableRenderPipeline = RenderPipeline::RP_URP;
	Shaders = UnityShadersType::ST_CUSTOM_SHADERS;
	ExposeMaterialParameters = true;
	ShowFBXExportDialog = false;
	ExportVertexShader = true;
	ExportLODs = true;
	ExportVertexColors = true;
	TangentToWorldLocation = TangentToWorldLocationType::TTWL_BEFORE_NORMAL_CALCULATION;
	ExportAllAnimations = false;
	FoliageExport = FoliageExportType::FET_CREATE_GAMEOBJECTS;
	ExportAllSounds = false;
	UsePrefabs = true;
}
bool UUnitySettingsClass::WithMaterialParameters()
{
	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
		return ExposeMaterialParameters;
	#else
		return false;
	#endif
}
UBakeTextureSettingsClass::UBakeTextureSettingsClass()
{
#ifdef ENABLE_UNITY_EXPORT
	ScriptableRenderPipeline = RenderPipeline::RP_URP;
#endif
	UVChannel = 0;
	TargetResolution = 2048;
	LOD = 0;
	UseMeshData = false;
	OneTextureSetPerMaterial = true;
}

UBakeTextureSettingsClass* BakeTextureSettings = nullptr;

void InstantiateExporter( EngineType Engine = EngineType::DEFAULT )
{
	if( ExporterInterface && ExporterInterface->Type == Engine )
		return;
	if( ExporterInterface )
	{
		delete ExporterInterface;
		ExporterInterface = nullptr;
	}

#ifdef ENABLE_GODOT_EXPORT
	if( Engine == EngineType::GODOT || Engine == EngineType::DEFAULT )
	{
		ExporterInterface = new GodotExporter;
	}
#endif
#ifdef ENABLE_UNITY_EXPORT
	if( Engine == EngineType::UNITY || Engine == EngineType::DEFAULT )
	{
		ExporterInterface = new UnityExporter;
	}
#endif
}
void EnsureSettingsExists()
{
	if( ExporterInterface->Settings == nullptr )
	{
		UBaseSettingsClass* Settings = ExporterInterface->NewSettings();
		Settings->Initialize();
		Settings->AddToRoot();
	}
}
IUnrealExporterInterface* ExporterInterface = nullptr;


void FUnrealToXModule::ExportToUnity()
{
	InstantiateExporter( EngineType::UNITY );

	EnsureSettingsExists();
	
	FString TitleStr = ExporterInterface->GetPluginName();
	TitleStr += TEXT( " " );
	TitleStr += ExporterInterface->GetPluginVersion();

	UE_LOG( LogTemp, Warning, TEXT( "%s" ), *TitleStr );
	
	FText Title = FText::FromString( TitleStr );

	// pop up a dialog to input params to the function
	TSharedRef<SWindow> Window = SNew( SWindow )
		.Title( Title )
		.ClientSize( FVector2D( 550, 550 ) )
		.SupportsMinimize( false )
		.SupportsMaximize( false );

	TSharedPtr<SFunctionParamDialog> Dialog;
	
	Window->SetContent(
		SAssignNew( Dialog, SFunctionParamDialog, Window )
		.OkButtonText( LOCTEXT( "OKButton", "OK" )));
	Dialog->StructureDetailsView->SetObject( ExporterInterface->Settings );

	GEditor->EditorAddModalWindow( Window );

	if( Dialog->bOKPressed )
	{
		if ( ExporterInterface->Settings->OutputDirectory[ ExporterInterface->Settings->OutputDirectory.Len() - 1] != '/' )
			ExporterInterface->Settings->OutputDirectory += TEXT( "/" );
		
		DoExport( );
	}
}
void FUnrealToXModule::ExportToGodot()
{
	InstantiateExporter( EngineType::GODOT );
	EnsureSettingsExists();

	
	FString TitleStr = ExporterInterface->GetPluginName();
	TitleStr += TEXT( " " );
	TitleStr += ExporterInterface->GetPluginVersion();

	UE_LOG( LogTemp, Warning, TEXT( "%s" ), *TitleStr );

	FText Title = FText::FromString( TitleStr );

	// pop up a dialog to input params to the function
	TSharedRef<SWindow> Window = SNew( SWindow )
		.Title( Title )
		.ClientSize( FVector2D( 550, 470 ) )
		.SupportsMinimize( false )
		.SupportsMaximize( false );

	TSharedPtr<SFunctionParamDialog> Dialog;
	
	Window->SetContent(
		SAssignNew( Dialog, SFunctionParamDialog, Window )
		.OkButtonText( LOCTEXT( "OKButton", "OK" ) ) );
	Dialog->StructureDetailsView->SetObject( ExporterInterface->Settings );

	GEditor->EditorAddModalWindow( Window );

	if( Dialog->bOKPressed )
	{
		if( ExporterInterface->Settings->OutputDirectory[ ExporterInterface->Settings->OutputDirectory.Len() - 1 ] != '/' )
			ExporterInterface->Settings->OutputDirectory += TEXT( "/" );

		DoExport( );
	}
}
void FUnrealToXModule::PrepareForExport()
{
	DoPrepareForExport();
}

void FUnrealToXModule::DoPrepareForExport()
{
	EnsureSettingsExists();

	if( !PreparedForExport )
	{
		if( ExporterInterface->Type == EngineType::GODOT )
		{
			ReorderMaterials();
			FixSkeletalMeshBoneInfluences();
			#if ENGINE_MAJOR_VERSION == 5
				FAssetCompilingManager::Get().FinishAllCompilation();
			#endif
		}

		//If virtual textures are disable, don't remove RVTs in shaders because they won't exist anyway
		bool VirtualTexturesSupported = GetDefault<URendererSettings>()->bVirtualTextures;

		bool UseShadersWithVirtualTextureSupport = true;		
		if (!UseShadersWithVirtualTextureSupport )
			RemoveVirtualTextures();
		if( ExporterInterface->Settings->NaniteOptions == NaniteOptionsEnum::NANITE_ORIGINAL_MESH )
			DoRemoveNanite();
		bool DoReplaceRVTs = false;// true;
		if( DoReplaceRVTs )
			ExportRVTTextures();
		else if ( VirtualTexturesSupported && ExporterInterface->Type == EngineType::UNITY )
			RemoveRVTs();
		RemoveVertexInterpolators();
		//if ( ExporterInterface->Type == EngineType::UNITY )
			RemoveUnsupportedMaterialNodes();//RemoveParallaxMapping
		MoveFoliageToActors();
		FixVolumeMaterials();
		#if ENGINE_MAJOR_VERSION == 5
			GShaderCompilingManager->FinishAllCompilation();
		#endif
		ConvertGeometryCachesToStaticMeshes();
		ConvertISMToActors();
		DetachMultipleComponentActors();
		//ConvertLandscapesToStaticMeshes();
		ConvertSplineMeshesAndDynamicMeshesToStaticActors();
		if ( ExporterInterface->Settings->SkinnedMeshes == SkinnedMeshOptionsEnum::SMO_EXPORT_AS_STATIC)
			ConvertSkeletalMeshesToStaticActors();
		if ( ExporterInterface->Settings->ExportBrushes )
			ConvertBrushesToStaticActors();		

		PreparedForExport = true;
	}
}
extern TAutoConsoleVariable<int> CVarRenderPipeline;

void FUnrealToXModule::AddCustomMenu( FToolBarBuilder& ToolbarBuilder )
{
	InstantiateExporter();

	FString ButtonName = ExporterInterface->GetPluginName();
	#ifdef ENABLE_GODOT_EXPORT
		ButtonName = TEXT( "UnrealToGodot" );
	#endif

	ToolbarBuilder.BeginSection( FName( ButtonName ) );
#if ENGINE_MAJOR_VERSION == 5
	ToolbarBuilder.SetLabelVisibility( EVisibility::Visible );
#endif
	TAttribute<FText> InLabelOverride = LOCTEXT( "UnrealToUnityLabel", "UnrealToUnity" );
	TAttribute<FText> InToolTipOverride = LOCTEXT( "UnrealToUnityTooltip", "UnrealToUnity" );

#ifdef ENABLE_GODOT_EXPORT
	InLabelOverride = LOCTEXT( "UnrealToGodotLabel", "UnrealToGodot" );
	InToolTipOverride = LOCTEXT( "UnrealToGodotTooltip", "UnrealToGodot" );
#endif
	{
		ToolbarBuilder.AddComboButton(
			FUIAction()
			, FOnGetContent::CreateRaw( this, &FUnrealToXModule::CreateMenuContent )
			, InLabelOverride
			, InToolTipOverride
			, FSlateIcon( FEditorStyle::GetStyleSetName(), "LevelEditor.GameSettings" )
		);
	}
	ToolbarBuilder.EndSection();
}


#define MapActionHelper(x) EditorCommands->MapAction( Commands.x, FExecuteAction::CreateRaw(this, &FUnrealToXModule::x), FCanExecuteAction(), FIsActionChecked())

void FUnrealToXModule::BindEditorCommands()
{
	if( !EditorCommands.IsValid() )
	{
		EditorCommands = MakeShareable( new FUICommandList() );
	}

	const FUnrealToUnity_Commands& Commands = FUnrealToUnity_Commands::Get();

#ifdef ENABLE_UNITY_EXPORT
	MapActionHelper( ExportToUnity );
#endif
#ifdef ENABLE_GODOT_EXPORT
	MapActionHelper( ExportToGodot );
#endif
	MapActionHelper( PlaceSelectedAssetsInScene );
	MapActionHelper( ConvertLandscapesToStaticMeshes );
	MapActionHelper( PrepareForExport );
	MapActionHelper( SpawnAllMaterialNodes );	
	MapActionHelper( BakeTextures );
	//MapActionHelper( GetTerrainData );
	MapActionHelper( PlaceCubesWithSelectedMaterials );
	MapActionHelper( RevertSubstrateMaterials );
	MapActionHelper( ExportRVTTexture );
	MapActionHelper( PrepareForRelease );
}

TSharedRef<SWidget> FUnrealToXModule::CreateMenuContent()
{
	FMenuBuilder MenuBuilder( true, EditorCommands );
	
#ifdef ENABLE_UNITY_EXPORT
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().ExportToUnity );
#endif
#ifdef ENABLE_GODOT_EXPORT
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().ExportToGodot );
#endif	
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().PlaceSelectedAssetsInScene );
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().ConvertLandscapesToStaticMeshes );
	//MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().PrepareForExport );
	//MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().SpawnAllMaterialNodes );
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().BakeTextures );
	//MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().GetTerrainData );
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().PlaceCubesWithSelectedMaterials );
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().RevertSubstrateMaterials );
	//MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().ExportRVTTexture );
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().PrepareForRelease );

	return MenuBuilder.MakeWidget();
}

void FUnrealToUnity_Commands::RegisterCommands()
{
	UI_COMMAND( ExportToUnity, "ExportToUnity...", "Export current scene to a unity project", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ExportToGodot, "ExportToGodot...", "Export current scene to a Godot project", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( PlaceSelectedAssetsInScene, "PlaceSelectedAssetsInScene", "Place selected assets in the project in a grid", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ConvertLandscapesToStaticMeshes, "ConvertLandscapesToStaticMeshes", "Converts Landscapes To StaticMeshes. Also modifies their materials to work with Static Meshes", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( PrepareForExport, "PrepareForExport", "Prepares materials for export. Wait for all shaders to recompile before using Export", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( SpawnAllMaterialNodes, "SpawnAllMaterialNodes", "SpawnAllMaterialNodes", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( BakeTextures, "BakeTextures", "BakeTextures", EUserInterfaceActionType::Button, FInputChord() );
	//UI_COMMAND( GetTerrainData, "GetTerrainData", "GetTerrainData", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( PlaceCubesWithSelectedMaterials, "PlaceCubesWithSelectedMaterials", "PlaceCubesWithSelectedMaterials", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( RevertSubstrateMaterials, "RevertSubstrateMaterials", "RevertSubstrateMaterials", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ExportRVTTexture, "ExportRVTTexture", "ExportRVTTexture", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( PrepareForRelease, "PrepareForRelease", "PrepareForRelease", EUserInterfaceActionType::Button, FInputChord());
}

void FUnrealToXModule::ShutdownModule()
{
}

void GetOutputExpression( FExpressionInput* MaterialInput, UMaterialExpression* Source, int OutputIndex, TArray< FExpressionInput*>& Inputs )
{
	if( !MaterialInput )
		return;
	if( MaterialInput->Expression == Source && MaterialInput->OutputIndex == OutputIndex )
		Inputs.Add( MaterialInput );
}
TArray<FExpressionInput*> GetInputs( UMaterialExpression* Exp )
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	//In 5.5 Some nodes don't initialize CachedInputs at all...
	TArray<FExpressionInput*> Inputs;
	int NumInputs = Exp->CountInputs();
	for (int i = 0; i < NumInputs; i++)
	{
		FExpressionInput* NewInput = Exp->GetInput( i );
		if (NewInput)
		{
			Inputs.Add( NewInput );
		}
	}
	
	return Inputs;
#else	
	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		return TArray<FExpressionInput*>{ Exp->GetInputsView() };
	#else
		return Exp->GetInputs();
	#endif
#endif
}
void GetOutputExpression( UMaterial* Material, UMaterialExpression* Source, int OutputIndex, TArray< FExpressionInput*>& Inputs )
{
	TArray<UMaterialExpression*> AllExpressions = GetExpressions( Material );

	for( int i = 0; i < AllExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = AllExpressions[ i ];

		TArray<FExpressionInput*> ExpInputs = GetInputs( Exp );
		for( int u = 0; u < ExpInputs.Num(); u++ )
		{
			UMaterialExpression* A = ExpInputs[ u ]->Expression;
			if( A == Source && ExpInputs[ u ]->OutputIndex == OutputIndex )
			{
				Inputs.Add( ExpInputs[ u ] );
			}
		}
	}

	GetOutputExpression( GetBaseColor(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetMetallic(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetSpecular(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetRoughness(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetAnisotropy(Material), Source, OutputIndex, Inputs);
	GetOutputExpression( GetNormal(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetTangent(Material), Source, OutputIndex, Inputs);
	GetOutputExpression( GetEmissiveColor(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetOpacity(Material), Source, OutputIndex, Inputs);
	GetOutputExpression( GetOpacityMask( Material ), Source, OutputIndex, Inputs);

	GetOutputExpression( GetWorldPositionOffset(Material), Source, OutputIndex, Inputs );
	#if ENGINE_MAJOR_VERSION == 4
	GetOutputExpression( &Material->WorldDisplacement, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->TessellationMultiplier, Source, OutputIndex, Inputs );
	#endif
	GetOutputExpression( GetSubsurfaceColor(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetClearCoat(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetClearCoatRoughness(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetAmbientOcclusion(Material), Source, OutputIndex, Inputs );
	GetOutputExpression( GetRefraction( Material ), Source, OutputIndex, Inputs);

	GetOutputExpression( GetMaterialAttributes( Material), Source, OutputIndex, Inputs);
	
	for( int i = 0; i < 8; i++ )
		GetOutputExpression( GetCustomizedUVs(Material, i ), Source, OutputIndex, Inputs );

	GetOutputExpression( GetPixelDepthOffset( Material ), Source, OutputIndex, Inputs);
	GetOutputExpression( GetShadingModelFromMaterialExpression( Material ), Source, OutputIndex, Inputs);
}
void GetOutputExpression( UMaterialFunction* MaterialFunction, UMaterialExpression* Source, int OutputIndex, TArray< FExpressionInput*>& Inputs )
{
	const TArray<UMaterialExpression*> AllExpressions = GetFunctionExpressions( MaterialFunction );

	for( int i = 0; i < AllExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = AllExpressions[ i ];

		TArray<FExpressionInput*> ExpInputs = GetInputs( Exp );
		for( int u = 0; u < ExpInputs.Num(); u++ )
		{
			UMaterialExpression* A = ExpInputs[ u ]->Expression;
			if( A == Source && ExpInputs[ u ]->OutputIndex == OutputIndex )
			{
				Inputs.Add( ExpInputs[ u ] );
			}
		}
	}
}
void GetOutputExpression( UMaterial* Material, UMaterialFunction* MaterialFunction, UMaterialExpression* Source, int OutputIndex, TArray< FExpressionInput*>& Inputs )
{
	if( Material )
	{
		GetOutputExpression( Material, Source, OutputIndex, Inputs );
	}
	else if( MaterialFunction )
	{
		GetOutputExpression( MaterialFunction, Source, OutputIndex, Inputs );
	}
}
UMaterialExpression* CreateRVTReplacementExpression( FExpressionOutput& OE, UMaterial* Material, UMaterialFunction* MaterialFunction, FVector2D& Location )
{
	UMaterialExpression* NewExpression = nullptr;

	if( OE.OutputName.ToString().Compare( "BaseColor" ) == 0 ||
		OE.OutputName.ToString().Compare( "Normal" ) == 0 )
	{
		//1, 1, 1, 0
		NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, MaterialFunction, UMaterialExpressionConstant3Vector::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionConstant3Vector* NewExpC = Cast < UMaterialExpressionConstant3Vector>( NewExpression );

		if( OE.OutputName.ToString().Compare( "BaseColor" ) == 0 )
		{
			NewExpC->Constant.R = NewExpC->Constant.G = NewExpC->Constant.B = 1;
			NewExpC->Constant.A = 0;
		}
		else
		{
			NewExpC->Constant.R = 0;
			NewExpC->Constant.G = 0;
			NewExpC->Constant.B = 1;
			NewExpC->Constant.A = 0;
		}

		Location.Y += 140;
	}
	else if( OE.OutputName.ToString().Compare( "Specular" ) == 0 ||
			 OE.OutputName.ToString().Compare( "Roughness" ) == 0 ||			 
			 OE.OutputName.ToString().Compare( "Mask" ) == 0 )
	{
		NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, MaterialFunction, UMaterialExpressionConstant::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionConstant* NewExpC = Cast < UMaterialExpressionConstant>( NewExpression );

		if( OE.OutputName.ToString().Compare( "Roughness" ) == 0 )
			NewExpC->R = 1;
		else
			NewExpC->R = 0;

		Location.Y += 60;
	}
	else if( OE.OutputName.ToString().Compare( "WorldHeight" ) == 0 )
	{
		UMaterialExpression* NewExp1 = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, MaterialFunction, UMaterialExpressionWorldPosition::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionWorldPosition* WPExp = Cast < UMaterialExpressionWorldPosition>( NewExp1 );
		WPExp->Desc = TEXT( "Replacement for RVT->WorldHeight" );

		Location.X += 60;
		Location.Y += 60;

		UMaterialExpression* NewExp2 = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, MaterialFunction, UMaterialExpressionComponentMask::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionComponentMask* CMExp = Cast < UMaterialExpressionComponentMask>( NewExp2 );

		CMExp->R = 0;
		CMExp->G = 0;
		CMExp->B = 1;//Z
		CMExp->A = 0;

		CMExp->Input.Expression = WPExp;

		Location.Y += 60;

		NewExpression = CMExp;
	}

	return NewExpression;
}
void IterateOverAllAssetsOfType( FName TypeName, std::function<bool( FAssetData& )> lambda, bool SaveAndUnload )
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );

	TArray<FAssetData> Assets;
	#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
		FTopLevelAssetPath TopLevelAssetPath( TEXT( "/Script/Engine" ), TypeName );
		AssetRegistryModule.Get().GetAssetsByClass( TopLevelAssetPath, Assets );
	#else
		AssetRegistryModule.Get().GetAssetsByClass( TypeName, Assets );
	#endif

	int current = 0;
	int Offset = 0;
	int threshold = 300;
	TArray< UPackage*> Packages;
	TArray< UPackage*> SavePackages;
	bool bOutAnyPackagesUnloaded;
	FText OutErrorMessage;
	for( int i = 0; i < Assets.Num(); i++)
	{
		FAssetData& Asset = Assets[ i ];
		if( Offset <= current )
		{
			if( Asset.PackageName.ToString().Contains( "/Game/Developers/" ) ||
				!Asset.PackageName.ToString().StartsWith( "/Game" ) )
			{
				continue;
			}

			UObject* Object = Asset.GetAsset();
			if( !Object )
				continue;
			UPackage* Package = Object->GetPackage();
			Packages.Add( Package );
			bool Modified = lambda( Asset );
			if( Modified )
			{
				SavePackages.Add( Package );
			}
			#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
				//Need to verify if this works similar to <5.6
				Package->ClearFlags( RF_Standalone );
			#else
				Package->GetMetaData()->ClearFlags( RF_Standalone );
			#endif

			if( SaveAndUnload && Packages.Num() % threshold == 0 )
			{
				if( SavePackages.Num() > 0 )
				{
					UEditorLoadingAndSavingUtils::SavePackages( SavePackages, true );
					SavePackages.Reset();
				}
			
				UEditorLoadingAndSavingUtils::UnloadPackages( Packages, bOutAnyPackagesUnloaded, OutErrorMessage );
				Packages.Reset();

				FlushRenderingCommands();
			}
		}
		current++;
	}

	if( SaveAndUnload )
	{
		UEditorLoadingAndSavingUtils::SavePackages( SavePackages, true );
		SavePackages.Reset();

		//UEditorLoadingAndSavingUtils::UnloadPackages( Packages, bOutAnyPackagesUnloaded, OutErrorMessage );

		FlushRenderingCommands();
	}
}
void IterateOverSelection( std::function<void( FAssetData& )> lambda )
{
	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>( "ContentBrowser" ).Get();

	TArray<FAssetData> SelectedAssets;
	ContentBrowser.GetSelectedAssets( SelectedAssets );

	int current = 0;
	for( FAssetData& Asset : SelectedAssets )
	{
		lambda( Asset );
		current++;
	}
}
EMaterialSamplerType GetNonVirtualSampler( EMaterialSamplerType Type)
{
	switch( Type )
	{
		default:
		case SAMPLERTYPE_VirtualColor: return SAMPLERTYPE_Color;
		case SAMPLERTYPE_VirtualGrayscale: return SAMPLERTYPE_Grayscale;
		case SAMPLERTYPE_VirtualAlpha: return SAMPLERTYPE_Alpha;
		case SAMPLERTYPE_VirtualNormal: return SAMPLERTYPE_Normal;
		case SAMPLERTYPE_VirtualMasks: return SAMPLERTYPE_Masks;

		case SAMPLERTYPE_VirtualLinearColor: return SAMPLERTYPE_LinearColor;
		case SAMPLERTYPE_VirtualLinearGrayscale: return SAMPLERTYPE_LinearGrayscale;
	}
}

class UTexture2DPublic : public UTexture2D
{
public:
	void CancelCachePlatformData()
	{
		#if ENGINE_MAJOR_VERSION >= 5
			while( !UTexture2D::TryCancelCachePlatformData() )
			{
			
				FPlatformProcess::Sleep( 0.01f );
			};
		#else
			UTexture2D::FinishCachePlatformData();
		#endif
	}
};

void FUnrealToXModule::RemoveVirtualTextures()
{
	if( !GetDefault<URendererSettings>()->bVirtualTextures )
		return;
	
	TArray<FString> Messages;
	auto lambdaTextures = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UTexture2D* Tex = Cast<UTexture2D>( Asset.GetAsset() );
		if( Tex )
		{
			if( Tex->VirtualTextureStreaming )
			{
				((UTexture2DPublic*)Tex)->CancelCachePlatformData();
				Tex->PreEditChange( nullptr );
				Tex->VirtualTextureStreaming = false;
				Modified = true;
			}
			
			if( Modified )
			{
				Tex->PostEditChange();
				Tex->Modify();
			}

			Tex->ClearFlags( RF_Standalone );
			Tex->AssetImportData = nullptr;
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Texture2D" ), lambdaTextures );
	//IterateOverSelection( lambdaTextures );

#if ENGINE_MAJOR_VERSION == 5
	FTextureCompilingManager::Get().FinishAllCompilation();
#endif

	auto lambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			for( int i = 0; i < GetExpressions(Material).Num(); i++ )
			{
				UMaterialExpression* Exp = GetExpressions(Material)[ i ];
				UMaterialExpressionTextureBase* TexBase = Cast< UMaterialExpressionTextureBase>( Exp );
				if( TexBase && IsVirtualSamplerType( TexBase->SamplerType ) )
				{
					TexBase->SamplerType = GetNonVirtualSampler( TexBase->SamplerType );
					Exp->PostEditChange();
					Modified = true;
				}
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), lambda );
	//IterateOverSelection( lambda );

	auto MaterialFunctionLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>( Asset.GetAsset() );
		if( MaterialFunction )
		{
			TArray<UMaterialExpression*> TempExpArray = GetFunctionExpressions( MaterialFunction );
			const TArray<UMaterialExpression*>* OutExpressions = &TempExpArray;			
			for( int i = 0; i < OutExpressions->Num(); i++ )
			{
				UMaterialExpression* Exp = ( *OutExpressions )[ i ];
				UMaterialExpressionTextureBase* TexBase = Cast< UMaterialExpressionTextureBase>( Exp );
				if( TexBase && IsVirtualSamplerType( TexBase->SamplerType ) )
				{
					TexBase->SamplerType = GetNonVirtualSampler( TexBase->SamplerType );
					Exp->PostEditChange();
					Modified = true;
				}
				
			}

			if( Modified )
			{
				MaterialFunction->PostEditChange();
				MaterialFunction->MarkPackageDirty();

				UMaterialEditingLibrary::UpdateMaterialFunction( MaterialFunction, nullptr );				
			}						
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "MaterialFunction" ), MaterialFunctionLambda );
	//IterateOverSelection( MaterialFunctionLambda );
}

void FUnrealToXModule::DoRemoveNanite()
{
#if ENGINE_MAJOR_VERSION == 5
	TArray<UStaticMesh*> MeshesToBuild;
	auto lambda = [&]( FAssetData& Asset )
	{		
		UStaticMesh* StaticMesh = Cast<UStaticMesh>( Asset.GetAsset() );
		if( StaticMesh )
		{
			if( StaticMesh->NaniteSettings.bEnabled )
			{
				StaticMesh->NaniteSettings.bEnabled = false;
				StaticMesh->AddToRoot();
				MeshesToBuild.Add( StaticMesh );				
			}		
		}

		return false;
	};

	IterateOverAllAssetsOfType( FName( "StaticMesh" ), lambda );
	//IterateOverSelection( lambda );

	FStaticMeshCompilingManager::Get().FinishCompilation( MeshesToBuild );
	UStaticMesh::BatchBuild( MeshesToBuild, true );
	FStaticMeshCompilingManager::Get().FinishCompilation( MeshesToBuild );

	for( int i = 0; i < MeshesToBuild.Num(); i++ )
	{
		UStaticMesh* StaticMesh = MeshesToBuild[ i ];

		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
		StaticMesh->ClearFlags( RF_Standalone );
		StaticMesh->RemoveFromRoot();
	}
#endif
}
void FUnrealToXModule::RemoveRVTs()
{
	int ModifiedMaterials = 0;
	int ModifiedMaterialFunctions = 0;
	TArray<FString> Messages;
	auto lambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			TArray<UMaterialExpression*> OutExpressions = GetExpressions( Material );
			for( int i = 0; i < OutExpressions.Num(); i++ )
			{
				UMaterialExpression* Exp = OutExpressions[ i ];
				UMaterialExpressionRuntimeVirtualTextureSample* RVTExp = Cast< UMaterialExpressionRuntimeVirtualTextureSample>( Exp );
				UMaterialExpressionRuntimeVirtualTextureSampleParameter* RVTExp2 = Cast< UMaterialExpressionRuntimeVirtualTextureSampleParameter>( Exp );
				if( RVTExp || RVTExp2 )
				{
					TArray<FExpressionOutput>& Outputs = Exp->GetOutputs();

					FVector2D Location( Exp->MaterialExpressionEditorX + 250, Exp->MaterialExpressionEditorY );

					for( int o = 0; o < Outputs.Num(); o++ )
					{
						FExpressionOutput& OE = Outputs[ o ];
						TArray< FExpressionInput*> Inputs;
						GetOutputExpression( Material, Exp, o, Inputs );
						if( Inputs.Num() > 0 )
						{
							auto NewExp = CreateRVTReplacementExpression( OE, Material, nullptr, Location );

							for( int j = 0; j < Inputs.Num(); j++ )
							{
								auto ExpInput = Inputs[ j ];
								ExpInput->Expression = NewExp;
								ExpInput->OutputIndex = 0;//usually the RGB value of the node
							}
							Modified = true;
						}
					}
				}
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
				ModifiedMaterials++;
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), lambda );
	
	auto MaterialFunctionLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>( Asset.GetAsset() );
		if( MaterialFunction )
		{
			TArray<UMaterialExpression*> TempExpArray = GetFunctionExpressions( MaterialFunction );
			const TArray<UMaterialExpression*>* OutExpressions = &TempExpArray;
			for( int i = 0; i < OutExpressions->Num(); i++ )
			{
				UMaterialExpression* Exp = ( *OutExpressions )[ i ];

				UMaterialExpressionRuntimeVirtualTextureSample* RVTExp = Cast< UMaterialExpressionRuntimeVirtualTextureSample>( Exp );
				UMaterialExpressionRuntimeVirtualTextureSampleParameter* RVTExp2 = Cast< UMaterialExpressionRuntimeVirtualTextureSampleParameter>( Exp );
				if( RVTExp || RVTExp2 )
				{
					TArray<FExpressionOutput>& Outputs = Exp->GetOutputs();

					FVector2D Location( Exp->MaterialExpressionEditorX + 250, Exp->MaterialExpressionEditorY );

					for( int o = 0; o < Outputs.Num(); o++ )
					{
						FExpressionOutput& OE = Outputs[ o ];
						TArray< FExpressionInput*> Inputs;
						GetOutputExpression( MaterialFunction, Exp, o, Inputs );
						if( Inputs.Num() > 0 )
						{
							auto NewExp = CreateRVTReplacementExpression( OE, nullptr, MaterialFunction, Location );

							for( int j = 0; j < Inputs.Num(); j++ )
							{
								auto ExpInput = Inputs[ j ];
								ExpInput->Expression = NewExp;
								ExpInput->OutputIndex = 0;//usually the RGB value of the node
							}
							Modified = true;
						}
					}
				}
			}

			if( Modified )
			{
				MaterialFunction->PostEditChange();
				MaterialFunction->MarkPackageDirty();

				UMaterialEditingLibrary::UpdateMaterialFunction( MaterialFunction, nullptr );
				ModifiedMaterialFunctions++;
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "MaterialFunction" ), MaterialFunctionLambda );
}
UExporter* GetExporterForAsset( UObject* Asset, FString* RequiredName );
bool UseUEExporter( UExporter* ExporterToUse, UObject* ObjectToExport, FString Filename );
FString GenerateAssetPath( UObject* Asset, FString Extension );
int CreateAllDirectories( FString FileName );
void FUnrealToXModule::ExportRVTTexture()
{
	ExportRVTTextures();
	/*auto Lambda = [&](FAssetData& Asset)
	{
		URuntimeVirtualTexture* RVT = Cast<URuntimeVirtualTexture>( Asset.GetAsset() );
		if( RVT )
		{
			
		}
	};

	//IterateOverAllAssetsOfType( FName( "StaticMesh" ), MaterialLambda );
	IterateOverSelection( Lambda );*/
}
void FUnrealToXModule::ExportRVTTextures()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();

	for( TActorIterator<AActor> ActorIterator( World ); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		if( Actor->IsHiddenEd() )
			continue;

		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );

		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			URuntimeVirtualTextureComponent* RVTComp = Cast<URuntimeVirtualTextureComponent>( AC );
			if( RVTComp )
			{
				UVirtualTextureBuilder* StreamingTexture = RVTComp->GetStreamingTexture();
				if( StreamingTexture )
				{
					UExporter* Exporter = GetExporterForAsset( StreamingTexture, nullptr );
					FString Extension = TEXT( ".png" );
					FString Filename = GenerateAssetPath( StreamingTexture, Extension );
					if( !FPlatformFileManager::Get().GetPlatformFile().FileExists( *Filename ) )
					{
						CreateAllDirectories( Filename );
						UseUEExporter( Exporter, StreamingTexture, Filename, false );
					}
				}
			}
		}
	}
}
void GetExpressionConnections( UMaterialExpression* Exp, UMaterial* Material, UMaterialFunction* MaterialFunction, TArray<UMaterialExpression*>& Connections,
							   TArray<FExpressionInput*>& OutputConnections );
void ConnectExpressionOutputToOtherExpression( UMaterial* BaseMaterial, UMaterialFunction* MaterialFunction, UMaterialExpression* TargetExpression,
											   UMaterialExpression* OtherExpression, int OutputIndex, int Mask, int MaskR, int MaskG, int MaskB, int MaskA  )
{
	TArray<UMaterialExpression*> Connections;
	TArray<FExpressionInput*> OutputConnections;
	GetExpressionConnections( TargetExpression, BaseMaterial, MaterialFunction, Connections, OutputConnections );
	for( int u = 0; u < Connections.Num(); u++ )
	{
		UMaterialExpression* Connection = Connections[ u ];
		auto Inputs = GetInputs( Connection );
		for( int m = 0; m < Inputs.Num(); m++ )
		{
			if( Inputs[ m ]->Expression == TargetExpression )
			{
				Inputs[ m ]->Expression = OtherExpression;
				Inputs[ m ]->OutputIndex = OutputIndex;
				Inputs[ m ]->Mask = Mask;
				Inputs[ m ]->MaskR = MaskR;
				Inputs[ m ]->MaskG = MaskG;
				Inputs[ m ]->MaskB = MaskB;
				Inputs[ m ]->MaskA = MaskA;
			}
		}
	}
	for( int i = 0; i < OutputConnections.Num(); i++ )
	{
		OutputConnections[ i ]->Expression = OtherExpression;
		OutputConnections[ i ]->OutputIndex = OutputIndex;
		OutputConnections[ i ]->Mask = Mask;
		OutputConnections[ i ]->MaskR = MaskR;
		OutputConnections[ i ]->MaskG = MaskG;
		OutputConnections[ i ]->MaskB = MaskB;
		OutputConnections[ i ]->MaskA = MaskA;
	}
}
const TArray<UMaterialExpression*> GetFunctionExpressions( UMaterialFunctionInterface* MaterialFunction );
void GetExpressionConnections( UMaterialExpression* Exp, UMaterial* Material, UMaterialFunction* MaterialFunction, TArray<UMaterialExpression*>& Connections,
							   TArray<FExpressionInput*>& OutputConnections )
{
	const TArray<UMaterialExpression*>* ExpressionsPointer = nullptr;
	TArray<UMaterialExpression*> ExpressionsArray;
	TArray<UMaterialExpression*> TempExpArray;
	if( Material )
	{
		ExpressionsArray = GetExpressions( Material );
		ExpressionsPointer = &ExpressionsArray;

		TArray<FExpressionInput*> MaterialOutputs;

		MaterialOutputs.Add( GetBaseColor(Material) );
		MaterialOutputs.Add( GetMetallic(Material) );
		MaterialOutputs.Add( GetSpecular(Material) );
		MaterialOutputs.Add( GetRoughness(Material) );
		MaterialOutputs.Add( GetAnisotropy(Material) );
		MaterialOutputs.Add( GetNormal(Material) );
		MaterialOutputs.Add( GetTangent(Material) );
		MaterialOutputs.Add( GetEmissiveColor(Material) );
		MaterialOutputs.Add( GetOpacity(Material) );
		MaterialOutputs.Add( GetOpacityMask(Material) );

		MaterialOutputs.Add( GetWorldPositionOffset(Material) );
		#if ENGINE_MAJOR_VERSION == 4
		MaterialOutputs.Add( &Material->WorldDisplacement );
		MaterialOutputs.Add( &Material->TessellationMultiplier );
		#endif
		MaterialOutputs.Add( GetSubsurfaceColor(Material) );
		MaterialOutputs.Add( GetClearCoat(Material) );
		MaterialOutputs.Add( GetClearCoatRoughness(Material) );
		MaterialOutputs.Add( GetAmbientOcclusion(Material) );
		MaterialOutputs.Add( GetRefraction(Material) );

		for( int i = 0; i < 8; i++ )
			MaterialOutputs.Add( GetCustomizedUVs(Material, i ) );

		MaterialOutputs.Add( GetPixelDepthOffset(Material) );
		MaterialOutputs.Add( GetShadingModelFromMaterialExpression(Material) );

		for( int i = 0; i < MaterialOutputs.Num(); i++ )
		{
			if( MaterialOutputs[ i ]->Expression == Exp )
			{
				OutputConnections.Add( MaterialOutputs[ i ] );
			}
		}
	}
	else if ( MaterialFunction )
	{
		TempExpArray = GetFunctionExpressions( MaterialFunction );
		ExpressionsPointer = &TempExpArray;
	}
	for( int i = 0; i < ExpressionsPointer->Num(); i++ )
	{
		UMaterialExpression* MatExp = ( *ExpressionsPointer)[ i ];
		if( MatExp)
		{
			TArray<FExpressionInput*> Inputs = GetInputs( MatExp );
			for( int u = 0; u < Inputs.Num(); u++ )
			{
				if( Inputs[ u ]->Expression == Exp )
				{
					Connections.Add( MatExp );
					break;
				}				
			}
		}
	}

	
}
struct ExpressionReplacementResult
{
	UMaterialExpression* Expression = nullptr;
	int OutputIndex = 0;
};
void DisconnectNodes( std::function<bool ( UMaterialExpression* )> IsNodeEligible,
	std::function<ExpressionReplacementResult( UMaterialExpression*, UMaterial*, UMaterialFunction* )> ReplaceLambda, int OutputIndex = 0 )
{
	TArray<FString> Messages;
	auto MaterialLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			TArray<UMaterialExpression*> OutExpressions = GetExpressions( Material );
			
			for( int i = 0; i < OutExpressions.Num(); i++ )
			{
				UMaterialExpression* TargetExp = OutExpressions[ i ];
				if( IsNodeEligible( TargetExp ))
				{
					TArray<UMaterialExpression*> Connections;
					TArray<FExpressionInput*> OutputConnections;
					GetExpressionConnections( TargetExp, Material, nullptr, Connections, OutputConnections );
					for( int c = 0; c < Connections.Num(); c++ )
					{
						auto Inputs = GetInputs( Connections[ c ] );
						for( int u = 0; u < Inputs.Num(); u++ )
						{
							if( Inputs[ u ]->Expression == TargetExp && Inputs[ u ]->OutputIndex == OutputIndex )
							{
								ExpressionReplacementResult Result = ReplaceLambda( TargetExp, Material, nullptr );
								Inputs[ u ]->Expression = Result.Expression;
								Inputs[ u ]->OutputIndex = Result.OutputIndex;
								Modified = true;
							}
						}
					}

					TArray< FExpressionInput*> Inputs;
					GetOutputExpression( Material, TargetExp, 0, Inputs );
					for( int u = 0; u < Inputs.Num(); u++ )
					{
						if( Inputs[ u ]->Expression == TargetExp )
						{
							ExpressionReplacementResult Result = ReplaceLambda( TargetExp, Material, nullptr );
							Inputs[ u ]->Expression = Result.Expression;
							Inputs[ u ]->OutputIndex = Result.OutputIndex;
							Modified = true;
						}
					}
				}
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), MaterialLambda );

	auto MaterialFunctionLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>( Asset.GetAsset() );
		if( MaterialFunction )
		{
			TArray<UMaterialExpression*> TempExpArray = GetFunctionExpressions( MaterialFunction );
			const TArray<UMaterialExpression*>* OutExpressions = &TempExpArray;
			for( int i = 0; i < OutExpressions->Num(); i++ )
			{
				UMaterialExpression* TargetExp = ( *OutExpressions )[ i ];
				if( IsNodeEligible( TargetExp ) )
				{
					TArray<UMaterialExpression*> Connections;
					TArray<FExpressionInput*> OutputConnections;
					GetExpressionConnections( TargetExp, nullptr, MaterialFunction, Connections, OutputConnections );
					for( int c = 0; c < Connections.Num(); c++ )
					{
						auto Inputs = GetInputs( Connections[ c ] );
						for( int u = 0; u < Inputs.Num(); u++ )
						{
							if( Inputs[ u ]->Expression == TargetExp )
							{
								ExpressionReplacementResult Result = ReplaceLambda( TargetExp, nullptr, MaterialFunction );
								Inputs[ u ]->Expression = Result.Expression;
								Inputs[ u ]->OutputIndex = Result.OutputIndex;
							}
						}
					}

					Modified = true;
				}
			}

			if( Modified )
			{
				MaterialFunction->PostEditChange();
				MaterialFunction->MarkPackageDirty();

				UMaterialEditingLibrary::UpdateMaterialFunction( MaterialFunction, nullptr );
			}			
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "MaterialFunction" ), MaterialFunctionLambda );
}
void FUnrealToXModule::RemoveVertexInterpolators()
{
	auto IsEligibleVertexInterpolator = [&]( UMaterialExpression* Exp )
	{
		UMaterialExpressionVertexInterpolator* E = Cast<UMaterialExpressionVertexInterpolator>( Exp );
		if( E )
			return true;
		else
			return false;
	};
	auto ReplaceVertexInterpolator = [&]( UMaterialExpression* TargetExp, UMaterial* OwnerMaterial, UMaterialFunction* OwnerFunction )
	{
		ExpressionReplacementResult Result;
		UMaterialExpressionVertexInterpolator* ExpressionVertexInterpolator = Cast<UMaterialExpressionVertexInterpolator>( TargetExp );
		Result.Expression = ExpressionVertexInterpolator->Input.Expression;
		return Result;
	};

	DisconnectNodes( IsEligibleVertexInterpolator, ReplaceVertexInterpolator );
}
void FUnrealToXModule::RemoveUnsupportedMaterialNodes()
{
	//No longer needed, it looks ok without it ( StonePineForest fails to recompile material anyway )
	//RemoveDitherTemporalAA();
	RemoveParallaxMapping();
}
void FUnrealToXModule::RemoveDitherTemporalAA()
{
	auto IsEligibleDitherTemporalAA = [&]( UMaterialExpression* Exp ) -> bool
	{
		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( Exp );
		if( ExpMFC && ExpMFC->MaterialFunction )
		{
			if ( ExpMFC->MaterialFunction->GetName().Compare( TEXT("DitherTemporalAA")) == 0 )
				return true;
		}
		return false;
	};
	auto ReplaceDitherTemporalAA = [&]( UMaterialExpression* TargetExp, UMaterial* OwnerMaterial, UMaterialFunction* OwnerFunction ) -> ExpressionReplacementResult
	{
		ExpressionReplacementResult Result;

		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( TargetExp );
		FExpressionInput* ExpInput = ExpMFC->GetInput( 0 );
		if( ExpInput )
		{
			Result.Expression = ExpInput->Expression;
			Result.OutputIndex = ExpInput->OutputIndex;
		}
		else
		{
			auto NewConstantExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( TargetExp->Material, TargetExp->Function, UMaterialExpressionConstant::StaticClass(), nullptr, TargetExp->MaterialExpressionEditorX, TargetExp->MaterialExpressionEditorY + 100 );
			UMaterialExpressionConstant* NewExpC = Cast < UMaterialExpressionConstant>( NewConstantExp );

			NewExpC->R = 0.5;
			Result.Expression = NewExpC;
		}
		return Result;
	};
	
	DisconnectNodes( IsEligibleDitherTemporalAA, ReplaceDitherTemporalAA );
}

void FUnrealToXModule::RemoveParallaxMapping()
{
	auto IsEligibleNode = [&]( UMaterialExpression* Exp ) -> bool
	{
		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( Exp );
		if( ExpMFC && ExpMFC->MaterialFunction )
		{
			if( ExpMFC->MaterialFunction->GetName().Compare( TEXT( "ParallaxOcclusionMapping" ) ) == 0 )
				return true;
		}
		UMaterialExpressionCustom* ExpCustom = Cast<UMaterialExpressionCustom>( Exp );
		if (ExpCustom &&
			( ExpCustom->Description.Compare( TEXT("Parallax Occlusion Mapping")) == 0 ||
			  ExpCustom->Description.Compare( TEXT("Parallax Only Mapping")) == 0 ))
		{
			return true;
		}
		return false;
	};
	auto ReplaceNode = [&]( UMaterialExpression* TargetExp, UMaterial* OwnerMaterial, UMaterialFunction* OwnerFunction )
	{
		ExpressionReplacementResult Result;
		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( TargetExp );
		if (ExpMFC)
		{
			FExpressionInput* ExpInput = ExpMFC->GetInput( 4 );
			if (ExpInput)
			{
				if (ExpInput->Expression)
					Result.Expression = ExpInput->Expression;
				else
				{
					UMaterialExpression* TexcoordExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( OwnerMaterial, OwnerFunction,
						UMaterialExpressionTextureCoordinate::StaticClass(), nullptr,
						TargetExp->MaterialExpressionEditorX - 150, TargetExp->MaterialExpressionEditorY );
					ExpInput->Expression = TexcoordExp;
					Result.Expression = ExpInput->Expression;
				}
			}
		}
		UMaterialExpressionCustom* ExpCustom = Cast<UMaterialExpressionCustom>( TargetExp );
		if (ExpCustom &&
			(ExpCustom->Description.Compare( TEXT( "Parallax Occlusion Mapping" ) ) == 0 ||
				ExpCustom->Description.Compare( TEXT( "Parallax Only Mapping" ) ) == 0))
		{
			FExpressionInput* ExpInput = ExpCustom->GetInput( 1 );//1 is UV
			if (ExpInput)
			{
				if (ExpInput->Expression)
					Result.Expression = ExpInput->Expression;
			}
		}
		return Result;
	};

	DisconnectNodes( IsEligibleNode, ReplaceNode );
}
void FUnrealToXModule::FixVolumeMaterials()
{
	TArray<FString> Messages;
	auto MaterialLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			if( Material->MaterialDomain == EMaterialDomain::MD_Volume )
			{
				if( GetSubsurfaceColor( Material )->Expression )
				{
					GetOpacity(Material)->Expression = GetSubsurfaceColor( Material)->Expression;
					GetSubsurfaceColor( Material )->Expression = nullptr;
				}
				Material->MaterialDomain = EMaterialDomain::MD_Surface;
				Modified = true;
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}			
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), MaterialLambda );
}

const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& GetFoliageInfos( AInstancedFoliageActor* IFA )
{
	#if ENGINE_MAJOR_VERSION == 4
		auto& Infos = IFA->FoliageInfos;
	#else
		auto& Infos = IFA->GetFoliageInfos();
	#endif

	return Infos;
}
int64 GetTotalFoliageInstances()
{
	int64 Total = 0;
	
	UWorld* World = GEditor->GetEditorWorldContext().World();

	for( TActorIterator<AActor> ActorIterator( World, AActor::StaticClass(), EActorIteratorFlags::OnlyActiveLevels ); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>( Actor );

		if( IFA )
		{
			auto& Infos = GetFoliageInfos( IFA );
			for( auto& Pair : Infos )
			{
				const UFoliageType* FoliageType = Pair.Key;
				const UFoliageType_InstancedStaticMesh* FT_StaticMesh = Cast<UFoliageType_InstancedStaticMesh>( FoliageType );
				if( !FT_StaticMesh )
				{
					continue;
				}
				UStaticMesh* SM = FT_StaticMesh->GetStaticMesh();
				if( !SM )
				{
					continue;
				}

				const FFoliageInfo& Info = *Pair.Value;
				
				Total += Info.Instances.Num();
			}
		}
	}

	return Total;
}
FVector RemoveNans( FVector Value );
bool HasNans( FVector Value );
GameObject* GameObjectForTrees = nullptr;
bool UsesGrass()
{
#if ENGINE_MAJOR_VERSION == 5
	for( TObjectIterator<ULandscapeComponent> It; It; ++It )
	{
		ULandscapeComponent* Component = *It;

		FLandscapeComponentGrassData& GrassData = Component->GrassData.Get();

		for( auto ItPair : GrassData.WeightOffsets )
		{
			const ULandscapeGrassType* GrassType = ItPair.Key;
			return true;
		}
	}
#endif

	return false;
}
void FUnrealToXModule::MoveFoliageToActors()
{
	int64 Total = GetTotalFoliageInstances();
	UE_LOG( LogTemp, Warning, TEXT( "GetTotalFoliageInstances = %lld" ), Total );
	int Threshold = CVarMaxFoliageActors.GetValueOnAnyThread();

	FoliageExportType ExportType = FoliageExportType::FET_CREATE_GAMEOBJECTS;
	UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
	if( UnitySettings )
		ExportType = UnitySettings->FoliageExport;

	if( Total > Threshold && ExportType == FoliageExportType::FET_CREATE_GAMEOBJECTS )
	{
		FString ExtraText = TEXT( "" );
		if ( ExporterInterface->Type == EngineType::UNITY )
			ExtraText = TEXT( "You can also export them as TreeInstances which will be much faster but it requires extra steps in Unity to complete the export.\n" );
		FString Text = FString::Printf( TEXT( "Foliage contains %d Instances. The conversion to static actors may take from 1 minute to hours.\n"
											  "%s"
											  "Do you still want to export all foliage as static mesh actors ?" ), Total, *ExtraText );
		FText Txt = FText::FromString( Text );

		EAppReturnType::Type Result = FMessageDialog::Open( EAppMsgType::YesNo, Txt );
		if( Result == EAppReturnType::Type::No )
			return;
	}
	TArray<FString> Messages;

	
	TreeInstancesComponent* TIC = nullptr;
	if( ExportType == FoliageExportType::FET_USE_TREEINSTANCES || UsesGrass() )
	{
		GameObjectForTrees = new GameObject();
		GameObjectForTrees->Name = "TerrainData";
		TIC = (TreeInstancesComponent*)GameObjectForTrees->AddComponent( CT_TREEINSTANCES );
	}
	else
		GameObjectForTrees = nullptr;

	int UniqueIndex = 0;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	TArray< const UFoliageType* > FoliageTypes;
	for( TActorIterator<AActor> ActorIterator( World, AActor::StaticClass(), EActorIteratorFlags::OnlyActiveLevels ); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>( Actor );

		if( IFA )
		{
			FTransform ActorTransform = IFA->GetActorTransform();
			const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& Infos = GetFoliageInfos( IFA );
			for( auto& Pair : Infos )
			{
				const UFoliageType* FoliageType = Pair.Key;
				const UFoliageType_InstancedStaticMesh* FT_StaticMesh = Cast<UFoliageType_InstancedStaticMesh>( FoliageType );
				if( !FT_StaticMesh )
				{
					continue;
				}
				UStaticMesh* SM = FT_StaticMesh->GetStaticMesh();
				if( !SM )
				{
					continue;
				}

				int FoliageID = FoliageTypes.Find( FoliageType );
				bool CreateInstanceForPrefab = false;
				if( FoliageID == INDEX_NONE )
				{
					FoliageID = FoliageTypes.Num();
					FoliageTypes.Add( FoliageType );
					CreateInstanceForPrefab = true;
				}
				const FFoliageInfo& Info = *Pair.Value;
				int Duplicates = 0;
				TArray<int32> InstancesIndices;
				for( int i = 0; i < Info.Instances.Num(); i++ )
				{
					const FFoliageInstance& Instance = Info.Instances[ i ];

					FTransform InstanceTransform = Instance.GetInstanceWorldTransform();
					FTransform Transform = InstanceTransform * ActorTransform;
					FVector Location = Transform.GetLocation();
					if( HasNans( Location ) )
						continue;

					//Location = RemoveNans( Location );
					Transform.SetTranslation( Location );

					if( ExportType == FoliageExportType::FET_USE_TREEINSTANCES )
					{
						//From Unreal position to Unity
						FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 0, 0 ) );
						Location = Rotator.RotateVector( Location );						
						Location = Location / 100;

						TreeInstancesComponent::TreeInstance* NewTreeInstance = new TreeInstancesComponent::TreeInstance;
						auto Scale = Transform.GetScale3D();
						NewTreeInstance->heightScale = Scale.Z;
						NewTreeInstance->widthScale = (Scale.X + Scale.Y)/2;
						NewTreeInstance->Position = Location;
						NewTreeInstance->rotation = Transform.GetRotation().Euler().Z + 90;
						NewTreeInstance->Prefab = nullptr;
						NewTreeInstance->FoliageID = FoliageID;
						TIC->TreeInstances.push_back( NewTreeInstance );
					}
					
					if( ExportType == FoliageExportType::FET_CREATE_GAMEOBJECTS ||
						( ExportType == FoliageExportType::FET_USE_TREEINSTANCES && CreateInstanceForPrefab && i == 0 ) )
					{
						AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform );
						AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
						NewSMActor->GetStaticMeshComponent()->SetStaticMesh( SM );

						UniqueIndex++;
						FString NewName = FString::Printf( TEXT( "%s_FromFoliage_%d" ), *SM->GetName(), UniqueIndex );
						if ( ExportType == FoliageExportType::FET_USE_TREEINSTANCES && CreateInstanceForPrefab )
							NewName = FString::Printf( TEXT( "UTU_FoliageID %d %s" ), FoliageID , *SM->GetName() );
						NewSMActor->Rename( *NewName );
						NewSMActor->SetActorLabel( NewName );

						if ( ExportType == FoliageExportType::FET_CREATE_GAMEOBJECTS )
							InstancesIndices.Add( i );
					}
				}

				#if ENGINE_MAJOR_VERSION == 4
					( (FFoliageInfo&)Info ).RemoveInstances( IFA, InstancesIndices, true );
				#else
					((FFoliageInfo&)Info).RemoveInstances( InstancesIndices, true );
				#endif
			}
		}
	}
	if( !TIC )
		return;
	int GrassID = FoliageTypes.Num();
	//Spawn grass meshes
	for( TObjectIterator<ULandscapeComponent> It; It; ++It )
	{
		ULandscapeComponent* LandscapeComponent = *It;

		FLandscapeComponentGrassData& GrassData = LandscapeComponent->GrassData.Get();

		#if ENGINE_MAJOR_VERSION == 5
		TArray<uint8>& HeightWeightData = GrassData.HeightWeightData;
		uint8* Data = HeightWeightData.GetData();

		for( auto ItPair : GrassData.WeightOffsets )
		{
			const ULandscapeGrassType* GrassType = ItPair.Key;
			int32 GrassTypeOffset = ItPair.Value;

			TreeInstancesComponent::GrassMesh* NewGrassMesh = new TreeInstancesComponent::GrassMesh;
			FString GrassAssetPath = TEXT( "Grass/" ) + GrassType->GetName() + TEXT( ".png" );
			NewGrassMesh->GrassTexture = ToANSIString( *GrassAssetPath );
			NewGrassMesh->FoliageID = GrassID;
			TIC->GrassMeshes.push_back( NewGrassMesh );

			TArray64<uint8> PNG_Compressed_ImageData;
			int Width = (int)FMath::Sqrt( (float)GrassData.NumElements );
			uint8* ImageData = Data + GrassTypeOffset;
			TArray<FColor> ColorData;
			for( int c = 0; c < GrassData.NumElements; c++ )
			{
				uint8 Amount = ImageData[ c ];
				ColorData.Add( FColor( Amount, Amount, Amount, Amount ) );
			}

			PNGCompressImageArray( Width, Width, ColorData, PNG_Compressed_ImageData );

			VerifyOrCreateDirectory( ExporterInterface->GetAssetsFolder() + TEXT( "/Grass/" ) );

			FString FileName = ExporterInterface->GetAssetsFolder() + TEXT( "/" ) + GrassAssetPath;
			bool Status = FFileHelper::SaveArrayToFile( PNG_Compressed_ImageData, *FileName );

			for( int v = 0; v < GrassType->GrassVarieties.Num(); v++ )
			{
				const FGrassVariety& Variety = GrassType->GrassVarieties[ v ];
				UStaticMesh* GrassMesh = Variety.GrassMesh;
				if (!GrassMesh)
					continue;
				const TArray<TObjectPtr<class UMaterialInterface>>& Materials = Variety.OverrideMaterials;

				FTransform Transform;
				AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform );
				AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
				NewSMActor->GetStaticMeshComponent()->SetStaticMesh( GrassMesh );
				if ( Materials.Num() > 0 )
				{
					for( int m = 0; m < Materials.Num(); m++ )
					{
						NewSMActor->GetStaticMeshComponent()->SetMaterial( m, Materials[ m ] );
					}
				}

				FString NewName = FString::Printf( TEXT( "UTU_FoliageID %d %s" ), GrassID, *GrassMesh->GetName() );
				NewSMActor->Rename( *NewName );
				NewSMActor->SetActorLabel( NewName );

				GrassID++;
			}
		}
		#endif
	}
}
void GetReferencedMeshes( const UGeometryCollection* Col, TArray<UStaticMesh*>& OutMeshes )
{
	for( int i = 0; i < Col->GeometrySource.Num(); i++ )
	{
		const FGeometryCollectionSource& Source = Col->GeometrySource[ i ];
		FSoftObjectPath Path = Source.SourceGeometryObject;
		UObject* LoadedObject = Path.TryLoad();
		if( LoadedObject )
		{
			UGeometryCollection* ChildCollection = Cast<UGeometryCollection>( LoadedObject );
			if( ChildCollection )
			{
				GetReferencedMeshes( ChildCollection, OutMeshes );
			}
			UStaticMesh* Mesh = Cast<UStaticMesh>( LoadedObject );
			if( Mesh )
			{
				OutMeshes.Add( Mesh );
			}
		}
	}
}
const UGeometryCollection* GetRestCollection( UGeometryCollectionComponent* Comp )
{
	#if ENGINE_MAJOR_VERSION == 5
		return Comp->RestCollection.Get();
	#else
		return Comp->RestCollection;
	#endif
}
void FUnrealToXModule::ConvertGeometryCachesToStaticMeshes()
{
	TArray<FString> Messages;

	int UniqueIndex = 0;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	int ActorCount = World->GetActorCount();
	FScopedSlowTask Progress( ActorCount );
	Progress.MakeDialog();

	int Index = 0;
	TArray<AActor*> WorldActors;
	for( TActorIterator<AActor> ActorIterator( World ); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		if( Actor->IsHiddenEd() )
			continue;
		WorldActors.Add( Actor );
	}
	for(int a=0; a<WorldActors.Num(); a++ )
	{
		Index++;
		FString Text = FString::Printf( TEXT( "ConvertGeometryCachesToStaticMeshes (Searching actors %d/%d)" ), Index, ActorCount );
		FText StrTxt = FText::FromString( Text );
		Progress.EnterProgressFrame( 1.0f, StrTxt );
		AActor* Actor = WorldActors[ a ];
		
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );

		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			UGeometryCollectionComponent* GeomComp = Cast<UGeometryCollectionComponent>( AC );//UHierarchicalInstancedStaticMeshComponent
			if( GeomComp && GeomComp->IsVisible() )
			{
				if( GetRestCollection(GeomComp) )
				{
					const UGeometryCollection* GeomColl = GetRestCollection( GeomComp );
					TArray<UStaticMesh*> OutMeshes;
					GetReferencedMeshes( GeomColl, OutMeshes );
					for( int m = 0; m < OutMeshes.Num(); m++ )
					{
						FActorSpawnParameters Params;
						FString NewActorName = FString::Printf( TEXT( "%s_GeomCollection_%d_%d" ), *Actor->GetActorLabel(), i, m );
						Params.Name = FName( NewActorName );
						FTransform Transform = GeomComp->GetComponentTransform();
						AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform, Params );
						NewActor->SetActorLabel( NewActorName );
						AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
						NewSMActor->GetStaticMeshComponent()->SetStaticMesh( OutMeshes[m] );
					}
				}
			}
		}
	}
}
void FUnrealToXModule::ConvertISMToActors()
{
	TArray<FString> Messages;

	int UniqueIndex = 0;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	TArray<AActor*> ActorArray;
	for( TActorIterator<AActor> ActorIterator( World ); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		ActorArray.Add( Actor );
	}

	FScopedSlowTask Progress( ActorArray.Num() );
	Progress.MakeDialog();

	int Index = 0;
	for( int a = 0; a < ActorArray.Num(); a++ )
	{
		Index++;
		FString Text = FString::Printf( TEXT( "ConvertInstancedStaticMeshToActors (Searching actors %d/%d)" ), Index, ActorArray.Num() );
		FText StrTxt = FText::FromString( Text );
		Progress.EnterProgressFrame( 1.0f, StrTxt );

		AActor* Actor = ActorArray[ a ];
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>( Actor );
		if( IFA )
			continue;
		if( Actor->IsHiddenEd() )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );
		
		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>( AC );//UHierarchicalInstancedStaticMeshComponent
			if( ISM && ISM->IsVisible() )
			{
				for( int u = 0; u < ISM->GetInstanceCount(); u++ )
				{
					FTransform Transform;
					ISM->GetInstanceTransform( u, Transform, true );

					FActorSpawnParameters Params;
					FString NewActorName = FString::Printf( TEXT( "%s_ISM%d_Instance%d_%d" ), *Actor->GetActorLabel(), i, u, UniqueIndex++ );
					Params.Name = FName(NewActorName);
					AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform, Params );
					NewActor->SetActorLabel( NewActorName );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( ISM->GetStaticMesh() );
					//Set the materials from the ISM, base mesh materials might be different
					auto Materials = ISM->GetMaterials();
					for( int m = 0; m < Materials.Num(); m++ )
					{
						NewSMActor->GetStaticMeshComponent()->SetMaterial( m, Materials[m] );
					}
				}
				for( int u = 0; u < ISM->GetInstanceCount(); u++ )
				{
					ISM->RemoveInstance( u );
					u--;
				}
				//Actor->RemoveOwnedComponent( ISM );
			}
		}
	}
}
void FUnrealToXModule::PlaceSelectedAssetsInScene()
{
	TArray<UStaticMesh*> StaticMeshes;
	TArray<USkeletalMesh*> SkeletalMeshes;
	TArray<FString> Messages;
	float MaxSphere = 0.0f;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	auto MaterialLambda = [&]( FAssetData& Asset )
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>( Asset.GetAsset() );
		if( StaticMesh )
		{
			FBoxSphereBounds Bounds = StaticMesh->GetBounds();
			MaxSphere = FMath::Max( MaxSphere, Bounds.SphereRadius );
			StaticMeshes.Add( StaticMesh );
		}
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>( Asset.GetAsset() );
		if( SkeletalMesh )
		{
			FBoxSphereBounds Bounds = SkeletalMesh->GetBounds();
			MaxSphere = FMath::Max( MaxSphere, Bounds.SphereRadius );
			SkeletalMeshes.Add( SkeletalMesh );
		}
	};

	//IterateOverAllAssetsOfType( FName( "StaticMesh" ), MaterialLambda );
	IterateOverSelection( MaterialLambda );

	int GridSize = (int)FMath::Sqrt( (float)StaticMeshes.Num() ) + 1;
	
	auto DoSpawn = [&]( FVector Position, int StaticMeshIndex, int SkeletalMeshIndex )
	{
		FTransform Transform;
		Transform.SetLocation( Position );

		if( StaticMeshIndex != -1 )
		{
			AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform );
			AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
			NewSMActor->GetStaticMeshComponent()->SetStaticMesh( StaticMeshes[ StaticMeshIndex ] );
			NewSMActor->SetActorLabel( StaticMeshes[ StaticMeshIndex ]->GetName() );
		}
		else if ( SkeletalMeshIndex != -1 )
		{
			AActor* NewActor = World->SpawnActor( ASkeletalMeshActor::StaticClass(), &Transform );
			ASkeletalMeshActor* NewSMActor = Cast<ASkeletalMeshActor>( NewActor );
			NewSMActor->GetSkeletalMeshComponent()->SetSkeletalMesh( SkeletalMeshes[ SkeletalMeshIndex ] );
			NewSMActor->SetActorLabel( SkeletalMeshes[ SkeletalMeshIndex ]->GetName() );
		}		
	};
	bool SpawnInLine = true;

	if( SpawnInLine )
	{
		FVector Offset = FVector(0,0,0);
		for(int u=0; u<StaticMeshes.Num(); u++ )
		{
			DoSpawn( Offset, u, -1 );
			Offset.X += StaticMeshes[ u ]->GetBounds().BoxExtent.X;
			if ( u < StaticMeshes.Num() - 1 )
				Offset.X += StaticMeshes[ u + 1 ]->GetBounds().BoxExtent.X;
		}
		for( int u = 0; u < SkeletalMeshes.Num(); u++ )
		{
			DoSpawn( Offset, -1, u );
			Offset.X += SkeletalMeshes[ u ]->GetBounds().BoxExtent.X;
			if( u < SkeletalMeshes.Num() - 1 )
				Offset.X += SkeletalMeshes[ u + 1 ]->GetBounds().BoxExtent.X;
		}
	}
	else
	{
		for( int y = 0; y < GridSize; y++ )
		{
			for( int x = 0; x < GridSize; x++ )
			{				
				int Index = y * GridSize + x;
				FVector Position( x * MaxSphere, y * MaxSphere, 0 );
				if ( Index < StaticMeshes.Num() )
					DoSpawn( Position, Index, -1 );
			}
		}
	}
}
void FUnrealToXModule::PlaceCubesWithSelectedMaterials()
{
	TArray<UMaterialInterface*> Materials;
	
	TArray<FString> Messages;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	auto MaterialLambda = [&]( FAssetData& Asset )
	{
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			Materials.Add( Material );
		}
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( Asset.GetAsset() );
		if( MaterialInstance )
		{
			Materials.Add( MaterialInstance );
		}
	};
	
	IterateOverSelection( MaterialLambda );

	UObject* LoadedObject = StaticLoadObject( UStaticMesh::StaticClass(), nullptr, TEXT( "/Engine/BasicShapes/Cube" ) );
	if( !LoadedObject )
		return;
	UStaticMesh* DefaultCube = Cast<UStaticMesh>( LoadedObject );

	int GridSize = (int)FMath::Sqrt( (float)Materials.Num() ) + 1;

	APostProcessVolume* GlobalPostProcessVolume = nullptr;
	auto DoSpawn = [&]( FVector Position, int Index )
	{
		FTransform Transform;
		Transform.SetLocation( Position );

		if( Index != -1 && Index < Materials .Num() )
		{
			UMaterialInterface* Mat = Materials[Index];
			//Could also do something for post processing materials
			if (Mat && Mat->GetBaseMaterial())
			{
				if (Mat->GetBaseMaterial()->MaterialDomain == EMaterialDomain::MD_DeferredDecal)
				{
					AActor* NewActor = World->SpawnActor( ADecalActor::StaticClass(), &Transform );
					ADecalActor* NewDecalActor = Cast<ADecalActor>( NewActor );
					NewDecalActor->SetActorLabel( Materials[Index]->GetName() );

					NewDecalActor->GetDecal()->DecalSize = FVector( 50, 50, 50 );
					NewDecalActor->GetDecal()->SetDecalMaterial( Mat );
				}
				else if ( Mat->GetBaseMaterial()->MaterialDomain == EMaterialDomain::MD_PostProcess )
				{
					if (!GlobalPostProcessVolume )
					{
						AActor* NewActor = World->SpawnActor( APostProcessVolume::StaticClass(), &Transform );
						GlobalPostProcessVolume = Cast<APostProcessVolume>( NewActor );
					}

					GlobalPostProcessVolume->AddOrUpdateBlendable( Mat, 1.0f );
				}
				else
				{
					AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( DefaultCube );
					NewSMActor->GetStaticMeshComponent()->SetMaterial( 0, Materials[Index] );
					NewSMActor->SetActorLabel( Materials[Index]->GetName() );
				}
			}			
		}
	};
	
	auto CubeBounds = DefaultCube->GetBounds();
	auto CubeSize = CubeBounds.BoxExtent.X * 2;

	for( int y = 0; y < GridSize; y++ )
	{
		for( int x = 0; x < GridSize; x++ )
		{
			FVector Position( x * CubeSize, y * CubeSize, 0 );
			int Index = y * GridSize + x;
			DoSpawn( Position, Index );
		}
	}
}
void FUnrealToXModule::FixLandscapeMaterials()
{
	TArray<FString> Messages;
	auto lambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			TArray<UMaterialExpression*> AllExpressions = GetExpressions( Material );
			
			for( int e = 0; e < AllExpressions.Num(); e++ )
			{
				UMaterialExpression* BaseExp = AllExpressions[ e ];
				UMaterialExpressionLandscapeLayerBlend* LayerBlendExp = Cast< UMaterialExpressionLandscapeLayerBlend>( BaseExp );
				if( LayerBlendExp )
				{
					bool HasMaterialAttributes = false;
					for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
					{
						auto Layer = LayerBlendExp->Layers[ i ];
						if( Layer.LayerInput.Expression &&
							Layer.LayerInput.Expression->IsResultMaterialAttributes( Layer.LayerInput.OutputIndex ) )
						{
							HasMaterialAttributes = true;
						}
					}
					if( HasMaterialAttributes )
					{
						TArray< UMaterialExpressionBreakMaterialAttributes*> BreakNodes;
						for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
						{
							auto Layer = LayerBlendExp->Layers[ i ];
							auto SourceExp = Layer.LayerInput.Expression;
							if( SourceExp )
							{
								FVector2D Location( SourceExp->MaterialExpressionEditorX + 200, SourceExp->MaterialExpressionEditorY );
								auto NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionBreakMaterialAttributes::StaticClass(), nullptr, Location.X, Location.Y );
								UMaterialExpressionBreakMaterialAttributes* NewExpC = Cast < UMaterialExpressionBreakMaterialAttributes>( NewExpression );
								*NewExpC->GetInput( 0 ) = Layer.LayerInput;
								BreakNodes.Add( NewExpC );
							}
						}

						const int NumIndicesUsed = 6;
															//BaseColor,Metallic,Specular,Roughness,Normal, AmbientOcclusion
						int InputIndices[ NumIndicesUsed ] = { 0, 1, 2, 3, 8, 14 };
						TArray< UMaterialExpressionLandscapeLayerBlend*> NewLayerBlendNodes;
						for( int u = 0; u < NumIndicesUsed; u++ )
						{
							FVector2D Location( LayerBlendExp->MaterialExpressionEditorX - 200, LayerBlendExp->MaterialExpressionEditorY + InputIndices[ u ] * 150 );
							auto NewLayerBlendExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionLandscapeLayerBlend::StaticClass(), nullptr, Location.X, Location.Y );
							UMaterialExpressionLandscapeLayerBlend* NewLayerBlendExpC = Cast < UMaterialExpressionLandscapeLayerBlend>( NewLayerBlendExp );

							for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
							{
								NewLayerBlendExpC->Layers.Add( LayerBlendExp->Layers[ i ] );
								if ( BreakNodes.Num() > i )
									NewLayerBlendExpC->Layers[ i ].LayerInput.Expression = BreakNodes[ i ];
								NewLayerBlendExpC->Layers[ i ].LayerInput.OutputIndex = InputIndices[ u ];
							}

							NewLayerBlendNodes.Add( NewLayerBlendExpC );
						}

						FVector2D Location( LayerBlendExp->MaterialExpressionEditorX + 200, LayerBlendExp->MaterialExpressionEditorY );
						
						auto NewMakeAttribsExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionMakeMaterialAttributes::StaticClass(), nullptr, Location.X, Location.Y );
						UMaterialExpressionMakeMaterialAttributes* NewMakeAttribsExpC = Cast < UMaterialExpressionMakeMaterialAttributes>( NewMakeAttribsExp );

						for( int u = 0; u < NumIndicesUsed; u++ )
						{
							NewMakeAttribsExp->GetInput( InputIndices[ u ] )->Expression = NewLayerBlendNodes[u];
						}

						TArray<FExpressionInput*> ConnectedExpressions;
						GetOutputExpression( Material, LayerBlendExp, 0, ConnectedExpressions );
						for( int i = 0; i < ConnectedExpressions.Num(); i++ )
						{
							ConnectedExpressions[ i ]->Expression = NewMakeAttribsExpC;
							ConnectedExpressions[ i ]->OutputIndex = 0;
						}
					}
				}
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), lambda );
	//IterateOverSelection( lambda );

}
void FUnrealToXModule::ConvertLandscapesToStaticMeshes()
{
	#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
		//StaticMeshDescription module is missing...
	#else
		UEditorLoadingSavingSettings* EditorSettings = GetMutableDefault< UEditorLoadingSavingSettings >();
		//Disable autosave so modified materials don't get saved
		if( EditorSettings )
		{
			EditorSettings->bAutoSaveEnable = 0;
		}
		FixLandscapeMaterials();

		UWorld* World = GEditor->GetEditorWorldContext().World();
	
		LandscapeTools.Process( World );
	#endif
}

USkeletalMesh* GetSkeletalMesh( USkinnedMeshComponent* SMC );

void FUnrealToXModule::DetachMultipleComponentActors()
{
	//Append a unique ID in case there's 2 actors with the same name
	int UniqueID = 0;
	int UniqueDecalID = 0;
	UWorld* world = GEditor->GetEditorWorldContext().World();
	for( TActorIterator<AActor> iterator( world ); iterator; ++iterator )
	{
		AActor* Actor = *iterator;
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>( Actor );
		if( Actor->IsHiddenEd() || IFA )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );
		int DetachableComponents = 0;
		int LocalDecalID = 0;
		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( AC );
			USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( AC );
			UDecalComponent* DecalComp = Cast<UDecalComponent>( AC );
			UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>( AC );
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>( AC );
			UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>( AC );
			if( (StaticMeshComp && !SplineMeshComponent && !ISM ) || DecalComp || SkeletalMeshComponent || ChildActorComponent )
			{
				DetachableComponents++;
			}
		}
		if( DetachableComponents > 1 )
		{
			for( int i = 0; i < comps.Num(); i++ )
			{
				UActorComponent* AC = comps[ i ];
				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( AC );
				USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( AC );
				UDecalComponent* DecalComp = Cast<UDecalComponent>( AC );
				USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>( AC );
				UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>( AC );
				if( StaticMeshComp && StaticMeshComp->GetStaticMesh() && !SplineMeshComponent && StaticMeshComp->IsVisible())
				{
					FTransform Transform = StaticMeshComp->GetComponentToWorld();
					FActorSpawnParameters Params;
					FString NewActorName = FString::Printf( TEXT( "%s_%s_%d_%d" ), *Actor->GetName(), *StaticMeshComp->GetStaticMesh()->GetName(), i, UniqueID++ );
					Params.Name = FName( NewActorName );
					AActor* NewActor = world->SpawnActor( AStaticMeshActor::StaticClass(), &Transform, Params );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->SetActorLabel( NewActorName );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( StaticMeshComp->GetStaticMesh() );
					NewSMActor->GetStaticMeshComponent()->OverrideMaterials = StaticMeshComp->OverrideMaterials;

					StaticMeshComp->DestroyComponent();
				}

				if( SkeletalMeshComponent && GetSkeletalMesh( SkeletalMeshComponent ) && SkeletalMeshComponent->IsVisible() )
				{
					USkeletalMesh* SkeletalMesh = GetSkeletalMesh( SkeletalMeshComponent );
					FTransform Transform = SkeletalMeshComponent->GetComponentToWorld();
					FActorSpawnParameters Params;
					FString NewActorName = FString::Printf( TEXT( "%s_%s_%d_%d" ), *Actor->GetName(), *SkeletalMesh->GetName(), i, UniqueID++ );
					Params.Name = FName( NewActorName );
					AActor* NewActor = world->SpawnActor( ASkeletalMeshActor::StaticClass(), &Transform, Params );
					ASkeletalMeshActor* NewSMActor = Cast<ASkeletalMeshActor>( NewActor );
					NewSMActor->SetActorLabel( NewActorName );
					NewSMActor->GetSkeletalMeshComponent()->SetSkeletalMesh( SkeletalMesh );
					NewSMActor->GetSkeletalMeshComponent()->OverrideMaterials = SkeletalMeshComponent->OverrideMaterials;
					NewSMActor->GetSkeletalMeshComponent()->SetAnimationMode( SkeletalMeshComponent->GetAnimationMode() );
					UAnimSequence* AnimationToSet = GetAnimationSequence( SkeletalMeshComponent );
					NewSMActor->GetSkeletalMeshComponent()->SetAnimation( AnimationToSet );
					NewSMActor->GetSkeletalMeshComponent()->AnimationData.AnimToPlay = AnimationToSet;					

					SkeletalMeshComponent->DestroyComponent();
				}
				if( DecalComp && DecalComp->IsVisible() )
				{
					FDetachmentTransformRules DetachRules( EDetachmentRule::KeepWorld, true );
					FAttachmentTransformRules AttachRules( EAttachmentRule::KeepWorld, false );
					
					FTransform Transform = DecalComp->GetComponentTransform();// *Actor->GetTransform();
					FActorSpawnParameters Params;
					FString NewActorName = FString::Printf( TEXT( "%s_%s_%d_%d" ), *Actor->GetName(), *DecalComp->GetName(), LocalDecalID++, UniqueDecalID++ );
					Params.Name = FName( NewActorName );
					
					AActor* NewActor = world->SpawnActor( ADecalActor::StaticClass(), &FTransform::Identity, Params);
					ADecalActor* NewDecalActor = Cast<ADecalActor>( NewActor );
					NewActor->SetActorLabel( NewActorName );

					NewDecalActor->GetDecal()->SetRelativeTransform( Transform );
					NewDecalActor->GetDecal()->UpdateComponentToWorld();

					NewDecalActor->GetDecal()->DecalSize = DecalComp->DecalSize;
					NewDecalActor->GetDecal()->SetDecalMaterial( DecalComp->GetDecalMaterial() );

					DecalComp->DestroyComponent();
				}
				if( ChildActorComponent )
				{
					AActor* ChildActor = ChildActorComponent->GetChildActor();					
					if(ChildActor)
					{						
						FTransform Transform = ChildActorComponent->GetComponentToWorld();
						FActorSpawnParameters Params;
						FString NewActorName = FString::Printf( TEXT( "%s_%d_%d" ), *ChildActor->GetName(), i, UniqueID++ );
						AStaticMeshActor* ChildStaticMeshActor = Cast< AStaticMeshActor>( ChildActor );
						UStaticMeshComponent* ChildActorStaticMeshComponent = nullptr;
						if (ChildStaticMeshActor)
						{
							ChildActorStaticMeshComponent = ChildStaticMeshActor->GetStaticMeshComponent();
							if (ChildActorStaticMeshComponent->GetStaticMesh())
							{
								NewActorName = FString::Printf( TEXT( "%s_%s_%d_%d" ), *ChildActor->GetName(), *ChildActorStaticMeshComponent->GetStaticMesh()->GetName(), i, UniqueID++ );
							}
						}
						Params.Name = FName( NewActorName );
						AActor* NewActor = world->SpawnActor( ChildActor->GetClass(), &Transform, Params);
						AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
						if (NewSMActor && ChildActorStaticMeshComponent)
						{
							NewSMActor->SetActorLabel( NewActorName );
							NewSMActor->GetStaticMeshComponent()->SetStaticMesh( ChildActorStaticMeshComponent->GetStaticMesh() );
							NewSMActor->GetStaticMeshComponent()->OverrideMaterials = ChildActorStaticMeshComponent->OverrideMaterials;
						}

						ChildActorComponent->DestroyComponent();						
					}
				}
			}
		}
	}
}

void FUnrealToXModule::ConvertSplineMeshesAndDynamicMeshesToStaticActors()
{
	UWorld* world = GEditor->GetEditorWorldContext().World();
	for( TActorIterator<AActor> iterator( world ); iterator; ++iterator )
	{
		AActor* Actor = *iterator;
		if( Actor->IsHiddenEd() )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );
		int SplineComponents = 0;
		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			USplineMeshComponent* SplineMeshComponent = dynamic_cast<USplineMeshComponent*>( AC );
			if( SplineMeshComponent )
			{
				SplineComponents++;
			}
			#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
			UDynamicMeshComponent* DynamicMeshComponent = dynamic_cast<UDynamicMeshComponent*>( AC );
			if( DynamicMeshComponent )
			{
				UStaticMesh* OutStaticMesh = nullptr;
				static int DynamicMeshID = 0;
				FString NewAssetPath = FString::Printf( TEXT( "/Game/DynamicMesh_%d" ), DynamicMeshID );
				bool Result = ConvertDynamicMeshToStaticMesh( NewAssetPath, DynamicMeshComponent, OutStaticMesh );
				if( Result && OutStaticMesh )
				{
					FTransform ActorTransform = DynamicMeshComponent->GetComponentTransform();
					UWorld* World = GEditor->GetEditorWorldContext().World();
					AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &ActorTransform );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( OutStaticMesh );

					FString NewName = FString::Printf( TEXT( "%s_DynamicMesh_%d" ), *Actor->GetName(), DynamicMeshID );
					NewSMActor->Rename( *NewName );
					NewSMActor->SetActorLabel( NewName );
					DynamicMeshID++;
				}
			}
			#endif
			UProceduralMeshComponent* ProceduralMeshComponent = Cast<UProceduralMeshComponent>( AC );
			if( ProceduralMeshComponent )
			{
				UStaticMesh* OutStaticMesh = nullptr;
				static int ProceduralMeshID = 0;
				FString NewAssetPath = FString::Printf( TEXT( "/Game/ProceduralMesh_%d" ), ProceduralMeshID );
				bool Result = ConvertProceduralMeshToStaticMesh( NewAssetPath, ProceduralMeshComponent, OutStaticMesh );
				if( Result && OutStaticMesh )
				{
					FTransform ActorTransform = ProceduralMeshComponent->GetComponentTransform();
					UWorld* World = GEditor->GetEditorWorldContext().World();
					AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &ActorTransform );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( OutStaticMesh );

					FString NewName = FString::Printf( TEXT( "%s_ProceduralMesh_%d" ), *Actor->GetName(), ProceduralMeshID );
					NewSMActor->Rename( *NewName );
					NewSMActor->SetActorLabel( NewName );
					ProceduralMeshID++;
				}
			}
		}
		if( SplineComponents > 0 )
		{
			ConvertSplineMeshesToStaticActor( Actor );
		}
	}
}
void ExportStaticMeshLOD( const FStaticMeshLODResources& StaticMeshLOD, FMeshDescription& OutRawMesh, const TArray<FStaticMaterial>& Materials )
{
	const int32 NumWedges = StaticMeshLOD.IndexBuffer.GetNumIndices();
	const int32 NumVertexPositions = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	const int32 NumFaces = NumWedges / 3;

	OutRawMesh.Empty();

	if( NumVertexPositions <= 0 || StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() <= 0 )
	{
		return;
	}

	//TEdgeAttributesRef<bool> EdgeHardnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );
	//TEdgeAttributesRef<float> EdgeCreaseSharpnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<float>( MeshAttribute::Edge::CreaseSharpness );	
	#if ENGINE_MAJOR_VERSION == 4
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = OutRawMesh.PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );
	#else
	FStaticMeshAttributes Attributes( OutRawMesh );
	Attributes.Register();
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	#endif
	

	OutRawMesh.ReserveNewVertices( NumVertexPositions );
	OutRawMesh.ReserveNewVertexInstances( NumWedges );
	OutRawMesh.ReserveNewPolygons( NumFaces );
	OutRawMesh.ReserveNewEdges( NumWedges );

	const int32 NumTexCoords = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	VertexInstanceUVs.SetNumIndices( NumTexCoords );

	for( int32 SectionIndex = 0; SectionIndex < StaticMeshLOD.Sections.Num(); ++SectionIndex )
	{
		const FStaticMeshSection& Section = StaticMeshLOD.Sections[ SectionIndex ];
		FPolygonGroupID CurrentPolygonGroupID = OutRawMesh.CreatePolygonGroup();
		check( CurrentPolygonGroupID.GetValue() == SectionIndex );
		if( Materials.IsValidIndex( Section.MaterialIndex ) )
		{
			PolygonGroupImportedMaterialSlotNames[ CurrentPolygonGroupID ] = Materials[ Section.MaterialIndex ].ImportedMaterialSlotName;
		}
		else
		{
			PolygonGroupImportedMaterialSlotNames[ CurrentPolygonGroupID ] = FName( *( TEXT( "MeshMergeMaterial_" ) + FString::FromInt( SectionIndex ) ) );
		}
	}

	//Create the vertex
	for( int32 VertexIndex = 0; VertexIndex < NumVertexPositions; ++VertexIndex )
	{
		FVertexID VertexID = OutRawMesh.CreateVertex();
		VertexPositions[ VertexID ] = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition( VertexIndex );
	}

	//Create the vertex instances
	for( int32 TriangleIndex = 0; TriangleIndex < NumFaces; ++TriangleIndex )
	{
		#if ENGINE_MAJOR_VERSION == 4
			FPolygonGroupID CurrentPolygonGroupID( FPolygonGroupID::Invalid );
		#else
			FPolygonGroupID CurrentPolygonGroupID( INDEX_NONE );
		#endif
		for( int32 SectionIndex = 0; SectionIndex < StaticMeshLOD.Sections.Num(); ++SectionIndex )
		{
			const FStaticMeshSection& Section = StaticMeshLOD.Sections[ SectionIndex ];
			uint32 BeginTriangle = Section.FirstIndex / 3;
			uint32 EndTriangle = BeginTriangle + Section.NumTriangles;
			if( (uint32)TriangleIndex >= BeginTriangle && (uint32)TriangleIndex < EndTriangle )
			{
				CurrentPolygonGroupID = FPolygonGroupID( SectionIndex );
				break;
			}
		}

		#if ENGINE_MAJOR_VERSION == 4
			check( CurrentPolygonGroupID != FPolygonGroupID::Invalid );
		#else
			check( CurrentPolygonGroupID != INDEX_NONE );
		#endif

		FVertexID VertexIDs[ 3 ];
		TArray<FVertexInstanceID> VertexInstanceIDs;
		VertexInstanceIDs.SetNum( 3 );

		for( int32 Corner = 0; Corner < 3; ++Corner )
		{
			int32 WedgeIndex = StaticMeshLOD.IndexBuffer.GetIndex( TriangleIndex * 3 + Corner );
			FVertexID VertexID( WedgeIndex );
			FVertexInstanceID VertexInstanceID = OutRawMesh.CreateVertexInstance( VertexID );
			VertexIDs[ Corner ] = VertexID;
			VertexInstanceIDs[ Corner ] = VertexInstanceID;

			//NTBs
			#if ENGINE_MAJOR_VERSION == 4
				FVector TangentX = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX( WedgeIndex );
				FVector TangentY = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY( WedgeIndex );
				FVector TangentZ = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( WedgeIndex );
				VertexInstanceTangents[ VertexInstanceID ] = TangentX;				
				VertexInstanceBinormalSigns[ VertexInstanceID ] = GetBasisDeterminantSign( TangentX, TangentY, TangentZ );
				VertexInstanceNormals[ VertexInstanceID ] = TangentZ;
			#else
				FVector TangentX = FVector4( StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX( WedgeIndex ) );
				FVector TangentY = FVector( StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY( WedgeIndex ) );
				FVector TangentZ = FVector4( StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( WedgeIndex ) );
				VertexInstanceTangents[ VertexInstanceID ] = (FVector3f)TangentX;
				VertexInstanceBinormalSigns[ VertexInstanceID ] = GetBasisDeterminantSign( TangentX, TangentY, TangentZ );
				VertexInstanceNormals[ VertexInstanceID ] = (FVector3f)TangentZ;
			#endif
			

			// Vertex colors
			if( StaticMeshLOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0 )
			{
				VertexInstanceColors[ VertexInstanceID ] = FLinearColor( StaticMeshLOD.VertexBuffers.ColorVertexBuffer.VertexColor( WedgeIndex ) );
			}
			else
			{
				VertexInstanceColors[ VertexInstanceID ] = FLinearColor::White;
			}

			//Tex coord
			for( int32 TexCoodIdx = 0; TexCoodIdx < NumTexCoords; ++TexCoodIdx )
			{
				VertexInstanceUVs.Set( VertexInstanceID, TexCoodIdx, StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( WedgeIndex, TexCoodIdx ) );
			}
		}
		//Create a polygon from this triangle
		const FPolygonID NewPolygonID = OutRawMesh.CreatePolygon( CurrentPolygonGroupID, VertexInstanceIDs );
	}
}
#if ENGINE_MAJOR_VERSION == 5
FVector3f ToFVector3f( FVector3d VD )
{
	return FVector3f( VD.X, VD.Y, VD.Z );
}
FVector3d ToFVector3d( FVector3f V )
{
	return FVector3d( V.X, V.Y, V.Z );
}
#else
FVector ToFVector3f( FVector VD )
{
	return FVector( VD.X, VD.Y, VD.Z );
}
FVector ToFVector3d( FVector V )
{
	return FVector( V.X, V.Y, V.Z );
}
#endif

template<class VectorType>
float& USplineMeshComponent_GetAxisValue( VectorType& InVector, ESplineMeshAxis::Type InAxis )
{
	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		float& AxisValue = USplineMeshComponent::GetAxisValueRef( InVector, InAxis );
	#else
		float& AxisValue = USplineMeshComponent::GetAxisValue( InVector, InAxis );
	#endif
	return AxisValue;
}
void FMeshMergeHelpers_PropagateSplineDeformationToRawMesh( const USplineMeshComponent* InSplineMeshComponent, FMeshDescription& OutRawMesh )
{
#if ENGINE_MAJOR_VERSION == 4
	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );
#else
	FStaticMeshAttributes Attributes( OutRawMesh );
	
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
#endif

	// Apply spline deformation for each vertex's tangents
	int32 WedgeIndex = 0;

#if ENGINE_MAJOR_VERSION == 4	
	for( const FPolygonID PolygonID : OutRawMesh.Polygons().GetElementIDs() )
	{
		for( const FTriangleID& TriangleID : OutRawMesh.GetPolygonTriangleIDs( PolygonID ) )
#else
	{
		for( const FTriangleID TriangleID : OutRawMesh.Triangles().GetElementIDs() )
#endif
		{
			for( int32 Corner = 0; Corner < 3; ++Corner, ++WedgeIndex )
			{
				const FVertexInstanceID VertexInstanceID = OutRawMesh.GetTriangleVertexInstance( TriangleID, Corner );
				const FVertexID VertexID = OutRawMesh.GetVertexInstanceVertex( VertexInstanceID );
				const float& AxisValue = USplineMeshComponent_GetAxisValue( VertexPositions[ VertexID ], InSplineMeshComponent->ForwardAxis );
				FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform( AxisValue );
				FVector TangentY = FVector::CrossProduct( (FVector)VertexInstanceNormals[ VertexInstanceID ], (FVector)VertexInstanceTangents[ VertexInstanceID ] ).GetSafeNormal() * VertexInstanceBinormalSigns[ VertexInstanceID ];
				VertexInstanceTangents[ VertexInstanceID ] = ToFVector3f( SliceTransform.TransformVector( ToFVector3d(VertexInstanceTangents[ VertexInstanceID ] ) ));
				TangentY = SliceTransform.TransformVector( TangentY );
				VertexInstanceNormals[ VertexInstanceID ] = ToFVector3f( SliceTransform.TransformVector( ToFVector3d(VertexInstanceNormals[ VertexInstanceID ] ) ));
				VertexInstanceBinormalSigns[ VertexInstanceID ] = GetBasisDeterminantSign( (FVector)VertexInstanceTangents[ VertexInstanceID ], TangentY, (FVector)VertexInstanceNormals[ VertexInstanceID ] );
			}
		}
	}

	// Apply spline deformation for each vertex position
	for( const FVertexID VertexID : OutRawMesh.Vertices().GetElementIDs() )
	{
		auto& AxisValue = USplineMeshComponent_GetAxisValue( VertexPositions[ VertexID ], InSplineMeshComponent->ForwardAxis );
		
		FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform( AxisValue );
		
		AxisValue = 0.0f;
		VertexPositions[ VertexID ] = ToFVector3f( SliceTransform.TransformPosition( ToFVector3d( VertexPositions[ VertexID ] ) ) );
	}
}
const TArray<FStaticMaterial>& GetStaticMaterials( const UStaticMesh* StaticMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION == 5
		const TArray<FStaticMaterial>& Materials = StaticMesh->GetStaticMaterials();
	#else
		const TArray<FStaticMaterial>& Materials = StaticMesh->StaticMaterials;
	#endif

	return Materials;
}
UPackage* CreatePackageVersioned( FString PackageName );
UStaticMesh* CreateStaticMeshFromMeshDescription( FString UserPackageName, FMeshDescription* MeshDescription, UStaticMesh* OriginalMesh, TArray<FStaticMaterial> *MergeMaterials)
{
	FString NewNameSuggestion = FString( TEXT( "ProcMesh" ) );
	FString PackageName = FString( TEXT( "/Game/Meshes/" ) ) + NewNameSuggestion;
	FString Name;

	//FAssetToolsModule* 
		AssetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
	AssetToolsModule->Get().CreateUniqueAssetName( PackageName, TEXT( "" ), PackageName, Name );

	{
		// Get the full name of where we want to create the physics asset.
		//FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
		FName MeshName( *FPackageName::GetLongPackageAssetName( UserPackageName ) );

		// Check if the user inputed a valid asset name, if they did not, give it the generated default name
		if( MeshName == NAME_None )
		{
			// Use the defaults that were already generated.
			UserPackageName = PackageName;
			MeshName = *Name;
		}

		// If we got some valid data.
		if( MeshDescription->Polygons().Num() > 0 )
		{
			// Then find/create it.
			UPackage* Package = CreatePackageVersioned( UserPackageName );
			check( Package );

			// Create StaticMesh object
			UStaticMesh* StaticMesh = NewObject<UStaticMesh>( Package, MeshName, RF_Public | RF_Standalone );
			StaticMesh->InitResources();

			StaticMesh->SetLightingGuid();

			// Add source to new StaticMesh
			FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
			SrcModel.BuildSettings.bRecomputeNormals = false;
			SrcModel.BuildSettings.bRecomputeTangents = false;
			SrcModel.BuildSettings.bRemoveDegenerates = false;
			SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
			SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
			SrcModel.BuildSettings.bGenerateLightmapUVs = true;
			SrcModel.BuildSettings.SrcLightmapIndex = 0;
			SrcModel.BuildSettings.DstLightmapIndex = 1;
			SrcModel.BuildSettings.DistanceFieldResolutionScale = 0.0f;
			SrcModel.BuildSettings.bUseFullPrecisionUVs = true;
			FMeshDescription* OriginalMeshDescription = StaticMesh->GetMeshDescription( 0 );
			if( OriginalMeshDescription == nullptr )
			{
				OriginalMeshDescription = StaticMesh->CreateMeshDescription( 0 );
			}
			*OriginalMeshDescription = *MeshDescription;
			StaticMesh->CommitMeshDescription( 0 );

			const TArray<FStaticMeshSourceModel>& SourceModels = OriginalMesh->GetSourceModels();
			for( int i = 1; i < SourceModels.Num(); i++ )
			{
				FStaticMeshSourceModel& SrcModelLOD = StaticMesh->AddSourceModel();
				SrcModelLOD.BuildSettings = OriginalMesh->GetSourceModel( i ).BuildSettings;
				SrcModelLOD.ReductionSettings = OriginalMesh->GetSourceModel( i ).ReductionSettings;
			}

			
			TArray<FStaticMaterial> LocalMaterials;
			if( MergeMaterials )
			{
				LocalMaterials = *MergeMaterials;
			}
			else
				LocalMaterials = GetStaticMaterials( OriginalMesh );

			#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 27) || ENGINE_MAJOR_VERSION == 5
				StaticMesh->SetStaticMaterials( LocalMaterials );
			#else
				StaticMesh->StaticMaterials = LocalMaterials;
			#endif

			StaticMesh->GetSectionInfoMap().CopyFrom( OriginalMesh->GetSectionInfoMap() );

			//Set the Imported version before calling the build
			StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

			// Build mesh from source
			StaticMesh->Build( false );
		#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 27) || ENGINE_MAJOR_VERSION == 5
			UBodySetup* BodySetup = StaticMesh->GetBodySetup();
		#else
			UBodySetup* BodySetup = StaticMesh->BodySetup;
		#endif
			if( BodySetup )
				BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
			StaticMesh->PostEditChange();

			Package->Modify();			

			return StaticMesh;
		}
	}

	return nullptr;
}
const FStaticMeshRenderData* GetRenderData( const UStaticMesh* StaticMesh );
bool FMeshMergeHelpers_PropagatePaintedColorsToRawMesh( const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FMeshDescription& RawMesh )
{
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

	if( StaticMesh->IsSourceModelValid( LODIndex ) &&
		StaticMeshComponent->LODData.IsValidIndex( LODIndex ) &&
		StaticMeshComponent->LODData[ LODIndex ].OverrideVertexColors != nullptr )
	{
		FColorVertexBuffer& ColorVertexBuffer = *StaticMeshComponent->LODData[ LODIndex ].OverrideVertexColors;
		const FStaticMeshLODResources& RenderModel = GetRenderData( StaticMesh )->LODResources[ LODIndex ];

		if( ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices() )
		{
			const int32 NumWedges = RawMesh.VertexInstances().Num();
			const int32 NumRenderWedges = RenderModel.IndexBuffer.GetNumIndices();
			const bool bUseRenderWedges = NumWedges == NumRenderWedges;

		#if ENGINE_MAJOR_VERSION == 4
			TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = RawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );
		#else
			TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = FStaticMeshAttributes( RawMesh ).GetVertexInstanceColors();
		#endif

			if( bUseRenderWedges )
			{
				//Create a map index
				TMap<int32, FVertexInstanceID> IndexToVertexInstanceID;
				IndexToVertexInstanceID.Reserve( NumWedges );
				int32 CurrentWedgeIndex = 0;
				for( const FPolygonID PolygonID : RawMesh.Polygons().GetElementIDs() )
				{
					const TArray<FTriangleID>& TriangleIDs = RawMesh.GetPolygonTriangleIDs( PolygonID );
					for( const FTriangleID& TriangleID : TriangleIDs )
					{
						for( int32 Corner = 0; Corner < 3; ++Corner, ++CurrentWedgeIndex )
						{
							IndexToVertexInstanceID.Add( CurrentWedgeIndex, RawMesh.GetTriangleVertexInstance( TriangleID, Corner ) );
						}
					}
				}

				const FIndexArrayView ArrayView = RenderModel.IndexBuffer.GetArrayView();
				for( int32 WedgeIndex = 0; WedgeIndex < NumRenderWedges; WedgeIndex++ )
				{
					const int32 Index = ArrayView[ WedgeIndex ];
					FColor WedgeColor = FColor::White;
					if( Index != INDEX_NONE )
					{
						WedgeColor = ColorVertexBuffer.VertexColor( Index );
					}
					VertexInstanceColors[ IndexToVertexInstanceID[ WedgeIndex ] ] = FLinearColor( WedgeColor );
				}

				return true;
			}
			// No wedge map (this can happen when we poly reduce the LOD for example)
			// Use index buffer directly. Not sure this will happen with FMeshDescription
			else
			{
				if( RawMesh.Vertices().Num() == ColorVertexBuffer.GetNumVertices() )
				{
					//Create a map index
					TMap<FVertexID, int32> VertexIDToVertexIndex;
					VertexIDToVertexIndex.Reserve( RawMesh.Vertices().Num() );
					int32 CurrentVertexIndex = 0;
					for( const FVertexID VertexID : RawMesh.Vertices().GetElementIDs() )
					{
						VertexIDToVertexIndex.Add( VertexID, CurrentVertexIndex++ );
					}

					for( const FVertexID VertexID : RawMesh.Vertices().GetElementIDs() )
					{
						FColor WedgeColor = FColor::White;
						uint32 VertIndex = VertexIDToVertexIndex[ VertexID ];

						if( VertIndex < ColorVertexBuffer.GetNumVertices() )
						{
							WedgeColor = ColorVertexBuffer.VertexColor( VertIndex );
						}
						const TArray<FVertexInstanceID>& VertexInstances = RawMesh.GetVertexVertexInstances( VertexID );
						for( const FVertexInstanceID& VertexInstanceID : VertexInstances )
						{
							VertexInstanceColors[ VertexInstanceID ] = FLinearColor( WedgeColor );
						}
					}
					return true;
				}
			}
		}
	}

	return false;
}
void FMeshMergeHelpers_TransformRawMeshVertexData( const FTransform& Transform, FMeshDescription& OutRawMesh )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FMeshMergeHelpers::TransformRawMeshVertexData )

	//TEdgeAttributesRef<bool> EdgeHardnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );
	//TEdgeAttributesRef<float> EdgeCreaseSharpnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<float>( MeshAttribute::Edge::CreaseSharpness );
	//TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = OutRawMesh.PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
#if ENGINE_MAJOR_VERSION == 4
	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );
#else
	FStaticMeshAttributes Attributes( OutRawMesh );
	Attributes.Register();
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
#endif

	for( const FVertexID VertexID : OutRawMesh.Vertices().GetElementIDs() )
	{
		VertexPositions[ VertexID ] = ToFVector3f( Transform.TransformPosition( ToFVector3d( VertexPositions[ VertexID ] ) ));
	}

	FMatrix Matrix = Transform.ToMatrixWithScale();
	FMatrix AdjointT = Matrix.TransposeAdjoint();
	AdjointT.RemoveScaling();

	const float MulBy = Matrix.Determinant() < 0.f ? -1.f : 1.f;
	auto TransformNormal =
		[&AdjointT, MulBy]( FVector& Normal )
	{
		Normal = AdjointT.TransformVector( Normal ) * MulBy;
	};

	for( const FVertexInstanceID VertexInstanceID : OutRawMesh.VertexInstances().GetElementIDs() )
	{
	#if ENGINE_MAJOR_VERSION == 4
		FVector TangentY = FVector::CrossProduct( VertexInstanceNormals[ VertexInstanceID ], VertexInstanceTangents[ VertexInstanceID ] ).GetSafeNormal() * VertexInstanceBinormalSigns[ VertexInstanceID ];
		TransformNormal( VertexInstanceTangents[ VertexInstanceID ] );
		TransformNormal( TangentY );
		TransformNormal( VertexInstanceNormals[ VertexInstanceID ] );
		VertexInstanceBinormalSigns[ VertexInstanceID ] = GetBasisDeterminantSign( VertexInstanceTangents[ VertexInstanceID ], TangentY, VertexInstanceNormals[ VertexInstanceID ] );
	#else
		FVector3f Tangent = VertexInstanceTangents[ VertexInstanceID ];
		FVector3f Normal = VertexInstanceNormals[ VertexInstanceID ];
		float BinormalSign = VertexInstanceBinormalSigns[ VertexInstanceID ];

		VertexInstanceTangents[ VertexInstanceID ] = (FVector3f)FVector( AdjointT.TransformVector( (FVector)Tangent ) * MulBy );
		VertexInstanceBinormalSigns[ VertexInstanceID ] = BinormalSign * MulBy;
		VertexInstanceNormals[ VertexInstanceID ] = (FVector3f)FVector( AdjointT.TransformVector( (FVector)Normal ) * MulBy );
	#endif
	}

	const bool bIsMirrored = Transform.GetDeterminant() < 0.f;
	if( bIsMirrored )
	{
		//Reverse the vertex instance
		OutRawMesh.ReverseAllPolygonFacing();
	}
}
FMeshDescription* FMeshMergeHelpers_RetrieveMesh( FMeshDescription* RawMesh, const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, bool bPropagateVertexColours )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FMeshMergeHelpers::RetrieveMesh )

	const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if( !StaticMesh )
		return nullptr;
	const FStaticMeshSourceModel& StaticMeshModel = StaticMesh->GetSourceModel( LODIndex );

	const bool bIsSplineMeshComponent = StaticMeshComponent->IsA<USplineMeshComponent>();

	// Imported meshes will have a valid mesh description
	const bool bImportedMesh = StaticMesh->IsMeshDescriptionValid( LODIndex );

	// Export the raw mesh data using static mesh render data
	ExportStaticMeshLOD( GetRenderData(StaticMesh)->LODResources[ LODIndex ], *RawMesh, GetStaticMaterials( StaticMesh ) );

	// Make sure the raw mesh is not irreparably malformed.
	if( RawMesh->VertexInstances().Num() <= 0 )
	{
		return nullptr;
	}

	// Use build settings from base mesh for LOD entries that was generated inside Editor.
	const FMeshBuildSettings& BuildSettings = bImportedMesh ? StaticMeshModel.BuildSettings : StaticMesh->GetSourceModel( 0 ).BuildSettings;

	// Transform raw mesh to world space
	FTransform ComponentToWorldTransform = StaticMeshComponent->GetComponentTransform();

	// Handle spline mesh deformation
	if( bIsSplineMeshComponent )
	{
		const USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( StaticMeshComponent );
		// Deform raw mesh data according to the Spline Mesh Component's data
		FMeshMergeHelpers_PropagateSplineDeformationToRawMesh( SplineMeshComponent, *RawMesh );
	}

	if( bPropagateVertexColours )
	{
		FMeshMergeHelpers_PropagatePaintedColorsToRawMesh( StaticMeshComponent, LODIndex, *RawMesh );
	}

	// Transform raw mesh vertex data by the Static Mesh Component's component to world transformation	
	FMeshMergeHelpers_TransformRawMeshVertexData( ComponentToWorldTransform, *RawMesh );

	return RawMesh;	
}
void FUnrealToXModule::ConvertSplineMeshesToStaticActor( AActor* Actor )
{
	TArray<USplineMeshComponent*> ComponentsToMerge;
	TArray<UActorComponent*> comps;
	Actor->GetComponents( comps );
	int SplineComponents = 0;
	for( int i = 0; i < comps.Num(); i++ )
	{
		UActorComponent* AC = comps[ i ];
		USplineMeshComponent* SplineMeshComponent = dynamic_cast<USplineMeshComponent*>( AC );
		if( SplineMeshComponent )
		{
			ComponentsToMerge.Add( SplineMeshComponent );
		}
	}

	if( ComponentsToMerge.Num() > 0 && ComponentsToMerge[ 0 ]->GetStaticMesh() )
	{
		TArray< FMeshDescription*> RawMeshArray;
		FMeshDescription* MeshDescription = new FMeshDescription();
		FStaticMeshAttributes( *MeshDescription ).Register();
				
		TArray<FStaticMaterial> Materials;
		for( int i = 0; i < ComponentsToMerge.Num(); i++ )
		{
			FMeshDescription* RawMesh = new FMeshDescription();
			FStaticMeshAttributes( *RawMesh ).Register();
			RawMeshArray.Add( RawMesh );

			FMeshMergeHelpers_RetrieveMesh( RawMesh, ComponentsToMerge[ i ], 0, true );
			UStaticMesh* OriginalMesh = ComponentsToMerge[ i ]->GetStaticMesh();
			FStaticMeshOperations::FAppendSettings AppendSettings;
			TArray<FStaticMaterial> MeshMaterials = GetStaticMaterials( OriginalMesh );
			for( int m = 0; m < MeshMaterials.Num(); m++ )
			{
				if( !Materials.FindByPredicate( [&]( const FStaticMaterial& Elem )
				{
					return Elem.MaterialInterface == MeshMaterials[ m ].MaterialInterface;
				} ))
				{
					Materials.Add( MeshMaterials[ m ] );
				}
			}			
			
			FStaticMeshOperations::AppendMeshDescription( *RawMesh, *MeshDescription, AppendSettings );
		}

		static int ID = 0;		
		FString MeshName = ComponentsToMerge[ 0 ]->GetStaticMesh()->GetName();
		FString PackageName = FString::Printf( TEXT( "/Game/%s_Splines_%d" ), *MeshName, ID );

		UStaticMesh* NewStaticMesh = CreateStaticMeshFromMeshDescription( PackageName, MeshDescription, ComponentsToMerge[ 0 ]->GetStaticMesh(), &Materials );

		FTransform ActorTransform;// = Actor->GetActorTransform();
		UWorld* World = GEditor->GetEditorWorldContext().World();
		AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &ActorTransform );
		AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
		NewSMActor->GetStaticMeshComponent()->SetStaticMesh( NewStaticMesh );

		FString NewName = FString::Printf( TEXT( "%s_Splines_%d" ), *MeshName, ID );
		NewSMActor->Rename( *NewName );
		NewSMActor->SetActorLabel( NewName );
		//Destroy original so if I export twice I don't get duplicates
		Actor->Destroy();
		ID++;
	}
}
static bool IsValidSkinnedMeshComponent( USkinnedMeshComponent* InComponent )
{
	return InComponent && InComponent->MeshObject && InComponent->IsVisible();
}
static bool IsValidStaticMeshComponent( UStaticMeshComponent* InComponent )
{
	return InComponent && InComponent->GetStaticMesh() && GetRenderData( InComponent->GetStaticMesh()) && InComponent->IsVisible();
}

/** Helper struct for tracking validity of optional buffers */
struct FRawMeshTracker
{
	FRawMeshTracker()
		: bValidColors( false )
	{
		FMemory::Memset( bValidTexCoords, 0 );
	}

	bool bValidTexCoords[ MAX_MESH_TEXTURE_COORDS ];
	bool bValidColors;
};
static void AddOrDuplicateMaterial( UMaterialInterface* InMaterialInterface, const FString& InPackageName, TArray<UMaterialInterface*>& OutMaterials )
{
	if( InMaterialInterface && !InMaterialInterface->GetOuter()->IsA<UPackage>() )
	{
		// Convert runtime material instances to new concrete material instances
		// Create new package
		FString OriginalMaterialName = InMaterialInterface->GetName();
		FString MaterialPath = FPackageName::GetLongPackagePath( InPackageName ) / OriginalMaterialName;
		FString MaterialName;
		//FAssetToolsModule& 
			AssetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
		AssetToolsModule->Get().CreateUniqueAssetName( MaterialPath, TEXT( "" ), MaterialPath, MaterialName );
		UPackage* MaterialPackage = CreatePackage( *MaterialPath );

		// Duplicate the object into the new package
		UMaterialInterface* NewMaterialInterface = DuplicateObject<UMaterialInterface>( InMaterialInterface, MaterialPackage, *MaterialName );
		NewMaterialInterface->SetFlags( RF_Public | RF_Standalone );

		if( UMaterialInstanceDynamic* MaterialInstanceDynamic = Cast<UMaterialInstanceDynamic>( NewMaterialInterface ) )
		{
			UMaterialInstanceDynamic* OldMaterialInstanceDynamic = CastChecked<UMaterialInstanceDynamic>( InMaterialInterface );
			MaterialInstanceDynamic->K2_CopyMaterialInstanceParameters( OldMaterialInstanceDynamic );
		}

		NewMaterialInterface->MarkPackageDirty();

		FAssetRegistryModule::AssetCreated( NewMaterialInterface );

		InMaterialInterface = NewMaterialInterface;
	}

	OutMaterials.Add( InMaterialInterface );
}
template <typename ComponentType>
static void ProcessMaterials( ComponentType* InComponent, const FString& InPackageName, TArray<UMaterialInterface*>& OutMaterials )
{
	const int32 NumMaterials = InComponent->GetNumMaterials();
	for( int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++ )
	{
		UMaterialInterface* MaterialInterface = InComponent->GetMaterial( MaterialIndex );
		AddOrDuplicateMaterial( MaterialInterface, InPackageName, OutMaterials );
	}
}
const TArray<FSkeletalMaterial>& GetMaterials( const USkeletalMesh* SkeletalMesh );
static void SkinnedMeshToRawMeshes( USkinnedMeshComponent* InSkinnedMeshComponent, int32 InOverallMaxLODs, const FMatrix& InComponentToWorld, const FString& InPackageName, TArray<FRawMeshTracker>& OutRawMeshTrackers, TArray<FRawMesh>& OutRawMeshes, TArray<UMaterialInterface*>& OutMaterials )
{
	const int32 BaseMaterialIndex = OutMaterials.Num();

	// Export all LODs to raw meshes
	const int32 NumLODs = InSkinnedMeshComponent->GetNumLODs();

	for( int32 OverallLODIndex = 0; OverallLODIndex < InOverallMaxLODs; OverallLODIndex++ )
	{
		int32 LODIndexRead = FMath::Min( OverallLODIndex, NumLODs - 1 );

		FRawMesh& RawMesh = OutRawMeshes[ OverallLODIndex ];
		FRawMeshTracker& RawMeshTracker = OutRawMeshTrackers[ OverallLODIndex ];
		const int32 BaseVertexIndex = RawMesh.VertexPositions.Num();

		FSkeletalMeshLODInfo& SrcLODInfo = *( GetSkeletalMesh( InSkinnedMeshComponent )->GetLODInfo( LODIndexRead ) );

		// Get the CPU skinned verts for this LOD
		TArray<FFinalSkinVertex> FinalVertices;
		InSkinnedMeshComponent->GetCPUSkinnedVertices( FinalVertices, LODIndexRead );

		FSkeletalMeshRenderData& SkeletalMeshRenderData = InSkinnedMeshComponent->MeshObject->GetSkeletalMeshRenderData();
		FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData.LODRenderData[ LODIndexRead ];

		// Copy skinned vertex positions
		for( int32 VertIndex = 0; VertIndex < FinalVertices.Num(); ++VertIndex )
		{
			auto NewPos = InComponentToWorld.TransformPosition( ToFVector3d( FinalVertices[ VertIndex ].Position ) );
			RawMesh.VertexPositions.Add( ToFVector3f( NewPos) );												
		}

		const uint32 NumTexCoords = FMath::Min( LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(), (uint32)MAX_MESH_TEXTURE_COORDS );
		const int32 NumSections = LODData.RenderSections.Num();
		FRawStaticIndexBuffer16or32Interface& IndexBuffer = *LODData.MultiSizeIndexContainer.GetIndexBuffer();

		for( int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++ )
		{
			const FSkelMeshRenderSection& SkelMeshSection = LODData.RenderSections[ SectionIndex ];
			if( InSkinnedMeshComponent->IsMaterialSectionShown( SkelMeshSection.MaterialIndex, LODIndexRead ) )
			{
				// Build 'wedge' info
				const int32 NumWedges = SkelMeshSection.NumTriangles * 3;
				for( int32 WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++ )
				{
					const int32 VertexIndexForWedge = IndexBuffer.Get( SkelMeshSection.BaseIndex + WedgeIndex );
					//Fix for rare crash
					if( VertexIndexForWedge >= FinalVertices.Num() )
						continue;

					RawMesh.WedgeIndices.Add( BaseVertexIndex + VertexIndexForWedge );

					const FFinalSkinVertex& SkinnedVertex = FinalVertices[ VertexIndexForWedge ];
					const FVector TangentX = InComponentToWorld.TransformVector( SkinnedVertex.TangentX.ToFVector() );
					const FVector TangentZ = InComponentToWorld.TransformVector( SkinnedVertex.TangentZ.ToFVector() );
					const FVector4 UnpackedTangentZ = SkinnedVertex.TangentZ.ToFVector4();
					const FVector TangentY = ( TangentZ ^ TangentX ).GetSafeNormal() * UnpackedTangentZ.W;

					RawMesh.WedgeTangentX.Add( ToFVector3f( TangentX ) );
					RawMesh.WedgeTangentY.Add( ToFVector3f( TangentY ) );
					RawMesh.WedgeTangentZ.Add( ToFVector3f( TangentZ ) );

					for( uint32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; TexCoordIndex++ )
					{
						if( TexCoordIndex >= NumTexCoords )
						{
							RawMesh.WedgeTexCoords[ TexCoordIndex ].AddDefaulted();
						}
						else
						{
							RawMesh.WedgeTexCoords[ TexCoordIndex ].Add( LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV( VertexIndexForWedge, TexCoordIndex ) );
							RawMeshTracker.bValidTexCoords[ TexCoordIndex ] = true;
						}
					}

					if( LODData.StaticVertexBuffers.ColorVertexBuffer.IsInitialized() )
					{
						RawMesh.WedgeColors.Add( LODData.StaticVertexBuffers.ColorVertexBuffer.VertexColor( VertexIndexForWedge ) );
						RawMeshTracker.bValidColors = true;
					}
					else
					{
						RawMesh.WedgeColors.Add( FColor::White );
					}
				}

				int32 MaterialIndex = SkelMeshSection.MaterialIndex;
				// use the remapping of material indices if there is a valid value
				if( SrcLODInfo.LODMaterialMap.IsValidIndex( SectionIndex ) && SrcLODInfo.LODMaterialMap[ SectionIndex ] != INDEX_NONE )
				{
					MaterialIndex = FMath::Clamp<int32>( SrcLODInfo.LODMaterialMap[ SectionIndex ], 0, GetMaterials( GetSkeletalMesh( InSkinnedMeshComponent )).Num() );
				}

				// copy face info
				for( uint32 TriIndex = 0; TriIndex < SkelMeshSection.NumTriangles; TriIndex++ )
				{
					RawMesh.FaceMaterialIndices.Add( BaseMaterialIndex + MaterialIndex );
					RawMesh.FaceSmoothingMasks.Add( 0 ); // Assume this is ignored as bRecomputeNormals is false
				}
			}
		}
	}

	ProcessMaterials<USkinnedMeshComponent>( InSkinnedMeshComponent, InPackageName, OutMaterials );
}
UStaticMesh* FMeshUtilities_ConvertMeshesToStaticMesh( const TArray<UMeshComponent*>& InMeshComponents, const FTransform& InRootTransform, const FString& InPackageName )
{
	UStaticMesh* StaticMesh = nullptr;

	// Build a package name to use
	FString MeshName;
	FString PackageName;
	if( InPackageName.IsEmpty() )
	{
		FString NewNameSuggestion = FString( TEXT( "StaticMesh" ) );
		FString PackageNameSuggestion = FString( TEXT( "/Game/Meshes/" ) ) + NewNameSuggestion;
		FString Name;
		//FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
						   AssetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
		AssetToolsModule->Get().CreateUniqueAssetName( PackageNameSuggestion, TEXT( "" ), PackageNameSuggestion, Name );

		//TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
		//	SNew( SDlgPickAssetPath )
		//	.Title( LOCTEXT( "ConvertToStaticMeshPickName", "Choose New StaticMesh Location" ) )
		//	.DefaultAssetPath( FText::FromString( PackageNameSuggestion ) );

		UMeshComponent* MC = InMeshComponents[ 0 ];
		USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>( MC );
		FString FullAssetPath = FString::Printf( TEXT( "/Game/Meshes/%s_AsStaticMesh" ), *GetSkeletalMesh(SMC)->GetName() );
		//if( PickAssetPathWidget->ShowModal() == EAppReturnType::Ok )
		{
			// Get the full name of where we want to create the mesh asset.
			PackageName = FullAssetPath;// PickAssetPathWidget->GetFullAssetPath().ToString();
			MeshName = FPackageName::GetLongPackageAssetName( PackageName );

			// Check if the user inputed a valid asset name, if they did not, give it the generated default name
			if( MeshName.IsEmpty() )
			{
				// Use the defaults that were already generated.
				PackageName = PackageNameSuggestion;
				MeshName = *Name;
			}
		}
	}
	else
	{
		PackageName = InPackageName;
		MeshName = *FPackageName::GetLongPackageAssetName( PackageName );
	}

	if( !PackageName.IsEmpty() && !MeshName.IsEmpty() )
	{
		TArray<FRawMesh> RawMeshes;
		TArray<UMaterialInterface*> Materials;

		TArray<FRawMeshTracker> RawMeshTrackers;

		FMatrix WorldToRoot = InRootTransform.ToMatrixWithScale().Inverse();

		// first do a pass to determine the max LOD level we will be combining meshes into
		int32 OverallMaxLODs = 0;
		for( UMeshComponent* MeshComponent : InMeshComponents )
		{
			USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>( MeshComponent );
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( MeshComponent );

			if( IsValidSkinnedMeshComponent( SkinnedMeshComponent ) )
			{
				OverallMaxLODs = FMath::Max( SkinnedMeshComponent->MeshObject->GetSkeletalMeshRenderData().LODRenderData.Num(), OverallMaxLODs );
			}
			else if( IsValidStaticMeshComponent( StaticMeshComponent ) )
			{
				OverallMaxLODs = FMath::Max( GetRenderData( StaticMeshComponent->GetStaticMesh() )->LODResources.Num(), OverallMaxLODs );
			}
		}

		// Resize raw meshes to accommodate the number of LODs we will need
		RawMeshes.SetNum( OverallMaxLODs );
		RawMeshTrackers.SetNum( OverallMaxLODs );

		// Export all visible components
		for( UMeshComponent* MeshComponent : InMeshComponents )
		{
			FMatrix ComponentToWorld = MeshComponent->GetComponentTransform().ToMatrixWithScale() * WorldToRoot;

			USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>( MeshComponent );
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( MeshComponent );

			if( IsValidSkinnedMeshComponent( SkinnedMeshComponent ) )
			{
				SkinnedMeshToRawMeshes( SkinnedMeshComponent, OverallMaxLODs, ComponentToWorld, PackageName, RawMeshTrackers, RawMeshes, Materials );
			}
			else if( IsValidStaticMeshComponent( StaticMeshComponent ) )
			{
				//StaticMeshToRawMeshes( StaticMeshComponent, OverallMaxLODs, ComponentToWorld, PackageName, RawMeshTrackers, RawMeshes, Materials );
			}
		}

		uint32 MaxInUseTextureCoordinate = 0;

		// scrub invalid vert color & tex coord data
		check( RawMeshes.Num() == RawMeshTrackers.Num() );
		for( int32 RawMeshIndex = 0; RawMeshIndex < RawMeshes.Num(); RawMeshIndex++ )
		{
			if( !RawMeshTrackers[ RawMeshIndex ].bValidColors )
			{
				RawMeshes[ RawMeshIndex ].WedgeColors.Empty();
			}

			for( uint32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; TexCoordIndex++ )
			{
				if( !RawMeshTrackers[ RawMeshIndex ].bValidTexCoords[ TexCoordIndex ] )
				{
					RawMeshes[ RawMeshIndex ].WedgeTexCoords[ TexCoordIndex ].Empty();
				}
				else
				{
					// Store first texture coordinate index not in use
					MaxInUseTextureCoordinate = FMath::Max( MaxInUseTextureCoordinate, TexCoordIndex );
				}
			}
		}

		// Check if we got some valid data.
		bool bValidData = false;
		for( FRawMesh& RawMesh : RawMeshes )
		{
			if( RawMesh.IsValidOrFixable() )
			{
				bValidData = true;
				break;
			}
		}

		if( bValidData )
		{
			// Then find/create it.
			UPackage* Package = CreatePackage( *PackageName );
			check( Package );

			// Create StaticMesh object
			StaticMesh = NewObject<UStaticMesh>( Package, *MeshName, RF_Public | RF_Standalone );
			StaticMesh->InitResources();

			StaticMesh->SetLightingGuid();

			// Determine which texture coordinate map should be used for storing/generating the lightmap UVs
			const uint32 LightMapIndex = FMath::Min( MaxInUseTextureCoordinate + 1, (uint32)MAX_MESH_TEXTURE_COORDS - 1 );

			// Add source to new StaticMesh
			for( FRawMesh& RawMesh : RawMeshes )
			{
				if( RawMesh.IsValidOrFixable() )
				{
					FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
					SrcModel.BuildSettings.bRecomputeNormals = false;
					SrcModel.BuildSettings.bRecomputeTangents = false;
					SrcModel.BuildSettings.bRemoveDegenerates = true;
					SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
					SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
					SrcModel.BuildSettings.bGenerateLightmapUVs = true;
					SrcModel.BuildSettings.SrcLightmapIndex = 0;
					SrcModel.BuildSettings.DstLightmapIndex = LightMapIndex;
					SrcModel.SaveRawMesh( RawMesh );
				}
			}

			// Copy materials to new mesh 
			for( UMaterialInterface* Material : Materials )
			{
				const TArray<FStaticMaterial>& StaticMaterials = GetStaticMaterials( StaticMesh );
				TArray<FStaticMaterial>& StaticMaterialsMutable = ( TArray<FStaticMaterial>&)StaticMaterials;
				StaticMaterialsMutable.Add( FStaticMaterial( Material ) );
			}

			//Set the Imported version before calling the build
			StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		#if ENGINE_MINOR_VERSION >= 27
			// Set light map coordinate index to match DstLightmapIndex
			StaticMesh->SetLightMapCoordinateIndex( LightMapIndex );
		#endif
			// setup section info map
			for( int32 RawMeshLODIndex = 0; RawMeshLODIndex < RawMeshes.Num(); RawMeshLODIndex++ )
			{
				const FRawMesh& RawMesh = RawMeshes[ RawMeshLODIndex ];
				TArray<int32> UniqueMaterialIndices;
				for( int32 MaterialIndex : RawMesh.FaceMaterialIndices )
				{
					UniqueMaterialIndices.AddUnique( MaterialIndex );
				}

				int32 SectionIndex = 0;
				for( int32 UniqueMaterialIndex : UniqueMaterialIndices )
				{
					StaticMesh->GetSectionInfoMap().Set( RawMeshLODIndex, SectionIndex, FMeshSectionInfo( UniqueMaterialIndex ) );
					SectionIndex++;
				}
			}
			StaticMesh->GetOriginalSectionInfoMap().CopyFrom( StaticMesh->GetSectionInfoMap() );

			// Build mesh from source
			StaticMesh->Build( false );
			StaticMesh->PostEditChange();

			StaticMesh->MarkPackageDirty();

			// Notify asset registry of new asset
			FAssetRegistryModule::AssetCreated( StaticMesh );

			// Display notification so users can quickly access the mesh
			//if( GIsEditor )
			//{
			//	FNotificationInfo Info( FText::Format( LOCTEXT( "SkeletalMeshConverted", "Successfully Converted Mesh" ), FText::FromString( StaticMesh->GetName() ) ) );
			//	Info.ExpireDuration = 8.0f;
			//	Info.bUseLargeFont = false;
			//	Info.Hyperlink = FSimpleDelegate::CreateLambda( [=]() { GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets( TArray<UObject*>( { StaticMesh } ) ); } );
			//	Info.HyperlinkText = FText::Format( LOCTEXT( "OpenNewAnimationHyperlink", "Open {0}" ), FText::FromString( StaticMesh->GetName() ) );
			//	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( Info );
			//	if( Notification.IsValid() )
			//	{
			//		Notification->SetCompletionState( SNotificationItem::CS_Success );
			//	}
			//}
		}
	}

	return StaticMesh;
}
void FUnrealToXModule::ConvertSkeletalMeshesToStaticActors()
{
	TArray<USkeletalMesh*> SkeletalMeshSources;
	TArray<UStaticMesh*> GeneratedMeshes;
	static int GeneratedSkeletalID = 0;
	UWorld* world = GEditor->GetEditorWorldContext().World();
	for( TActorIterator<AActor> iterator( world ); iterator; ++iterator )
	{
		AActor* Actor = *iterator;
		if( Actor->IsHiddenEd() )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );		
		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			USkeletalMeshComponent* SkeletalMeshComponent = dynamic_cast<USkeletalMeshComponent*>( AC );
			if( SkeletalMeshComponent && GetSkeletalMesh( SkeletalMeshComponent ) )
			{
				UStaticMesh* GeneratedStaticMesh = nullptr;
				int Index = SkeletalMeshSources.Find( GetSkeletalMesh( SkeletalMeshComponent ) );
				FTransform ComponentToWorld = SkeletalMeshComponent->GetComponentToWorld();
				if( Index == INDEX_NONE )
				{
					TArray<UMeshComponent*> InMeshComponents;
					InMeshComponents.Add( SkeletalMeshComponent );
					FString InPackageName;
					FTransform DefaultTransform;					
					SkeletalMeshComponent->SetComponentToWorld( DefaultTransform );
					GeneratedStaticMesh = FMeshUtilities_ConvertMeshesToStaticMesh( InMeshComponents, DefaultTransform, InPackageName );

					GeneratedMeshes.Add( GeneratedStaticMesh );
					SkeletalMeshSources.Add( SkeletalMeshComponent->SkeletalMesh );
				}
				else
					GeneratedStaticMesh = GeneratedMeshes[ Index ];

				if( GeneratedStaticMesh )
				{
					UWorld* World = GEditor->GetEditorWorldContext().World();
					AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &ComponentToWorld );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( GeneratedStaticMesh );

					FString NewName = FString::Printf( TEXT( "%s_%d" ), *GeneratedStaticMesh->GetName(), GeneratedSkeletalID++ );
					NewSMActor->Rename( *NewName );
					NewSMActor->SetActorLabel( NewName );

					SkeletalMeshComponent->DestroyComponent();
				}
				else
				{
					UE_LOG( LogTemp, Warning, TEXT( "ERROR! FMeshUtilities_ConvertMeshesToStaticMesh returned nullptr!" ) );
				}
			}
		}
	}
}
void FUnrealToXModule::ConvertBrushesToStaticActors()
{
	UWorld* world = GEditor->GetEditorWorldContext().World();
	int BrushID = 0;
	TArray<ABrush*> BrushesList;
	for( TActorIterator<ABrush> iterator( world ); iterator; ++iterator )
	{
		ABrush* BrushActor = *iterator;
		if( BrushActor->IsHiddenEd() )
			continue;

		auto Comp = BrushActor->GetBrushComponent();
		if( !Comp->GetVisibleFlag() )
			continue;

		if ( IsValid( BrushActor ) )
			BrushesList.Add( BrushActor );
	}

	bool MergeAllBrushes = false;
	
	if( MergeAllBrushes )
	{
		TArray<AActor*> ValidSelectedBrushes;
		for( int i = 0; i < BrushesList.Num(); i++ )
		{
			ValidSelectedBrushes.Add( BrushesList[ i ] );
		}

		if( ValidSelectedBrushes.Num() > 0 )
		{
			FString PackageName = FString::Printf( TEXT( "/Game/AllBrushes_AsStaticMesh_%d" ), BrushID );
			GEditor->DoConvertActors( ValidSelectedBrushes, AStaticMeshActor::StaticClass(), TSet<FString>(), true, PackageName );
			BrushID++;
		}
	}
	else
	{
		for( int i = 0; i < BrushesList.Num(); i++ )
		{
			TArray<AActor*> ValidSelectedBrushes;
			ValidSelectedBrushes.Add( BrushesList[ i ] );
			FString PackageName = FString::Printf( TEXT( "/Game/Brush_%d_AsStaticMesh" ), BrushID );
			GEditor->DoConvertActors( ValidSelectedBrushes, AStaticMeshActor::StaticClass(), TSet<FString>(), true, PackageName );
			BrushID++;
		}
	}
}
void FUnrealToXModule::SpawnAllMaterialNodes()
{
	TArray<UClass*> Classes;
	for( TObjectIterator<UClass> It; It; ++It )
	{
		UClass* Class = *It;
		FString ClassName = Class->GetName();
		if( Class->IsChildOf( UMaterialExpression::StaticClass() ) && !Class->HasAnyClassFlags( CLASS_Abstract ) )
		{
			if ( !ClassName.Contains(TEXT("Parameter")))
				Classes.Add( Class );
		}
	}

	FVector2D Location( Classes.Num() * -200, 0 );

	auto MaterialLambda = [&]( FAssetData& Asset )
	{
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			UMaterialExpressionMultiply* LastMultiply = nullptr;
			for( int i = 0; i < Classes.Num(); i++ )
			{
				UClass* Class = Classes[ i ];
				UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, Class, nullptr, Location.X, Location.Y );

				FVector2D MultiplyLocation = Location + FVector2D( 100, -200 );
				UMaterialExpressionMultiply* ThisMultiply = Cast<UMaterialExpressionMultiply>( UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionMultiply::StaticClass(), nullptr, MultiplyLocation.X, MultiplyLocation.Y) );
				ThisMultiply->A.Expression = LastMultiply;
				ThisMultiply->B.Expression = NewExpression;
				
				Location.X += 200;

				//auto Inputs = NewExpression->GetInputs();
				//for( int k = 0; k < Inputs.Num(); k++ )
				//{
				//	Inputs[ k ]->Expression = LastMultiply;
				//	Inputs[ k ]->OutputIndex = 0;
				//}

				LastMultiply = ThisMultiply;
			}

			Material->PostEditChange();
			Material->MarkPackageDirty();

			UMaterialEditingLibrary::RecompileMaterial( Material );
		}
	};

	//IterateOverAllAssetsOfType( FName( "StaticMesh" ), MaterialLambda );
	IterateOverSelection( MaterialLambda );
}
void FUnrealToXModule::RemoveSkinChunking()
{
	auto lambda = [&]( FAssetData& Asset )
	{
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>( Asset.GetAsset() );
		if( SkeletalMesh )
		{
			bool Modified = false;
			FSkeletalMeshModel* SkelMeshModel = SkeletalMesh->GetImportedModel();
			for( int i = 0; i < SkelMeshModel->LODModels.Num(); i++ )
			{
				auto& LOD = SkelMeshModel->LODModels[ i ];
				for( int u = 0; u < LOD.Sections.Num(); u++ )
				{
					auto& Section = LOD.Sections[ u ];
					if( Section.ChunkedParentSectionIndex != INDEX_NONE )
					{
						Section.ChunkedParentSectionIndex = INDEX_NONE;
						Modified = true;
					}
				}
			}			

			if( Modified )
			{
				SkeletalMesh->PostEditChange();
				SkeletalMesh->MarkPackageDirty();
			}
		}
	};

	IterateOverSelection( lambda );
}

void GetTextureColors( UTexture2D* Tex, TArray<FColor>& Colors )
{
	TArray64<uint8> MipDataA;
	Tex->Source.GetMipData( MipDataA, 0 );
	int32 Width = Tex->Source.GetSizeX();
	int32 Height = Tex->Source.GetSizeY();
	int32 Depth = Tex->Source.GetNumSlices();
	ETextureSourceFormat Format = Tex->Source.GetFormat();

	UETextureExporter::ConvertToColors( MipDataA, Format, Width, Height, Depth, Colors, Tex->GetName() );
}
enum CombineMode
{
	CM_INVERSE_ROUGHNESS_TO_METALLIC,
	CM_ROUGHNESS_TO_METALLIC,
	CM_OPACITYMASK_TO_BASECOLOR_ALPHA,
	CM_OCCLUSION_TO_GREEN,
};
UTexture2D* CombineTextures( UTexture2D* TexA, UTexture2D* TexB, CombineMode Mode, UObject* Outer )
{
	TArray<FColor> ColorsA;
	TArray<FColor> ColorsB;
	int32 Width = TexA->Source.GetSizeX();
	int32 Height = TexA->Source.GetSizeY();
	GetTextureColors( TexA, ColorsA );
	GetTextureColors( TexB, ColorsB );

	if ( ColorsA.Num() != ColorsB.Num() )
		return nullptr;

	for( int i = 0; i < ColorsA.Num(); i++ )
	{
		if( Mode == CM_INVERSE_ROUGHNESS_TO_METALLIC )
		{
			//Put inverted roughness (smoothness) in metallic alpha
			ColorsA[ i ].A = 255 - ColorsB[ i ].R;
		}
		if( Mode == CM_ROUGHNESS_TO_METALLIC )
		{
			ColorsA[ i ].A = ColorsB[ i ].R;
		}
		else if( Mode == CM_OPACITYMASK_TO_BASECOLOR_ALPHA )
		{
			ColorsA[ i ].A = ColorsB[ i ].R;
		}
		else if( Mode == CM_OCCLUSION_TO_GREEN )
		{
			ColorsA[ i ].G = ColorsB[ i ].R;
		}
	}

	EPixelFormat Format = EPixelFormat::PF_B8G8R8A8;
	ETextureSourceFormat SourceFormat = TSF_BGRA8;
	FString CombinedName = FString::Printf( TEXT( "%s+%s" ), *TexA->GetName(), *TexB->GetName() );
	UTexture2D* CombinedTexture = CreateTexture( Width, Height, Format, SourceFormat, &ColorsA[ 0 ], FName( CombinedName ), Outer );
	return CombinedTexture;
}
bool HasTexture( UMaterialInstance* MatInstance, FString TexName )
{
	FHashedMaterialParameterInfo ParamInfo( *TexName );
	UTexture* Tex = nullptr;
	bool Ret = MatInstance->GetTextureParameterValue( ParamInfo, Tex );
	return Ret;
}
float ConvertToOriginalValue( float InVal );
//Need this for backcompat with 4.26
void SetStaticSwitchParameter( UMaterialInstance* MatInstance, FName PropertyName, bool Value )
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	FMaterialParameterInfo ParamInfo( *PropertyName.ToString() );
	MatInstance->SetStaticSwitchParameterValueEditorOnly( ParamInfo, Value );
#else
	FStaticParameterSet StaticParametersSet;
	MatInstance->GetStaticParameterValues( StaticParametersSet );

	for( int u = 0; u < StaticParametersSet.StaticSwitchParameters.Num(); u++ )
	{
		FStaticSwitchParameter& SwitchParameter = StaticParametersSet.StaticSwitchParameters[ u ];
		if( SwitchParameter.ParameterInfo.Name.IsEqual( PropertyName ) )
		{
			SwitchParameter.bOverride = true;
			SwitchParameter.Value = Value;

			break;
		}
	}

	MatInstance->UpdateStaticPermutation( StaticParametersSet );
#endif
}
void CombineTextures( FString TexAName, FString TexBName, FColor ConstantA, FColor ConstantB, UMaterialInstance* MatInstance, CombineMode Mode, UObject* Outer )
{
	FHashedMaterialParameterInfo ParamInfo( *TexAName );
	UTexture* TexA = nullptr;
	MatInstance->GetTextureParameterValue( ParamInfo, TexA, true );

	FHashedMaterialParameterInfo ParamInfo2( *TexBName );
	UTexture* TexB = nullptr;
	MatInstance->GetTextureParameterValue( ParamInfo2, TexB, true );
	if( !TexA && TexB )
	{
		TArray<FColor> Colors;
		int32 Width = TexB->Source.GetSizeX();
		int32 Height = TexB->Source.GetSizeY();
		int Pixels = Width * Height;

		for( int i = 0; i < Pixels; i++)
		{
			Colors.Add( ConstantA );
		}
		EPixelFormat Format = EPixelFormat::PF_B8G8R8A8;
		ETextureSourceFormat SourceFormat = TSF_BGRA8;
		FString NewTexName = FString::Printf( TEXT( "Constant_Mode_%d_For_%s" ), Mode, *TexB->GetName() );
		TexA = CreateTexture( Width, Height, Format, SourceFormat, &Colors[ 0 ], FName( NewTexName ), Outer );

		if( Mode == CM_INVERSE_ROUGHNESS_TO_METALLIC || Mode == CM_ROUGHNESS_TO_METALLIC )
		{
			SetStaticSwitchParameter( MatInstance, TEXT( "UseMetallic" ), true );
		}
		else if ( Mode == CM_OPACITYMASK_TO_BASECOLOR_ALPHA )
		{
			SetStaticSwitchParameter( MatInstance, TEXT( "UseBaseColor" ), true );
		}
	}
	//Do this only for Masks in HDRP, no idea if this breaks anything else
	if( Mode == CM_OCCLUSION_TO_GREEN )
	{
		if( TexA && !TexB )
		{
			TArray<FColor> Colors;
			int32 Width = TexA->Source.GetSizeX();
			int32 Height = TexA->Source.GetSizeY();
			int Pixels = Width * Height;

			for( int i = 0; i < Pixels; i++ )
			{
				Colors.Add( ConstantB );
			}
			EPixelFormat Format = EPixelFormat::PF_B8G8R8A8;
			ETextureSourceFormat SourceFormat = TSF_BGRA8;
			FString NewTexName = FString::Printf( TEXT( "Constant_Mode_%d_For_%s" ), Mode, *TexA->GetName() );
			TexB = CreateTexture( Width, Height, Format, SourceFormat, &Colors[ 0 ], FName( NewTexName ), Outer );
		}
	}

	if( TexA && TexB )
	{
		UTexture2D* TexA2D = Cast<UTexture2D>( TexA );
		UTexture2D* TexB2D = Cast<UTexture2D>( TexB );
		UTexture2D* CombinedTexture = CombineTextures( TexA2D, TexB2D, Mode, Outer );
		if( CombinedTexture )
		{
			bool Found = false;
			for( int p = 0; p < MatInstance->TextureParameterValues.Num(); p++ )
			{
				FTextureParameterValue& ParamVal = MatInstance->TextureParameterValues[ p ];
				if( ParamVal.ParameterInfo.Name.ToString().Contains( TexAName ))
				{
					ParamVal.ParameterValue = CombinedTexture;
					
					#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
					FMaterialInstanceParameterUpdateContext UpdateContext( MatInstance );
					FMaterialParameterMetadata ParameterMeta;
					if( MatInstance->GetParameterOverrideValue( EMaterialParameterType::Texture, ParamVal.ParameterInfo.Name, ParameterMeta ) )
					{
						UpdateContext.SetParameterValueEditorOnly( ParamVal.ParameterInfo, ParameterMeta );
					}
					#endif
					Found = true;
					break;
				}
			}
			if( !Found )
			{
				FTextureParameterValue TextureParameterValue;
				TextureParameterValue.ParameterInfo.Name = *TexAName;
				TextureParameterValue.ParameterValue = CombinedTexture;
				MatInstance->TextureParameterValues.Add( TextureParameterValue );
			}
		}
	}	
}
bool RenameAsset(UObject* Obj, FString ActualObjName, FString NewName, bool RenamePackage = true )
{
	FString AssetPath = Obj->GetPackage()->GetPathName();
	//AssetPath = AssetPath.Replace( TEXT( "/Game/" ), TEXT( "" ) );
	bool Removed = AssetPath.RemoveFromEnd( ActualObjName );

	FString NewPackagePath = AssetPath + NewName;
	bool Success = false;
	if( RenamePackage )
	{
		UObject* ExistingObject = StaticFindObject(/*Class=*/ nullptr, Obj->GetPackage()->GetOuter(), *NewPackagePath, true );
		if( ExistingObject )
		{
			//Rather odd situation but it can sometimes happen
			static int ExtraIndex = 0;
			NewPackagePath = FString::Printf( TEXT( "%s_%d" ), *NewPackagePath, ExtraIndex );
			ExtraIndex++;
		}

		Success = Obj->GetPackage()->Rename( *NewPackagePath );
	}
	Success = Obj->Rename( *NewName );
	return Success;
}
class BakeEntry
{
public:
	BakeEntry( UStaticMesh* Mesh = nullptr, UMaterialInterface* OriginalMaterial = nullptr, int Section = -1 )
	{
		this->Mesh = Mesh;
		this->OriginalMaterial = OriginalMaterial;
		this->Section = Section;
	}
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* OriginalMaterial = nullptr;
	UMaterialInterface* BakedMaterial = nullptr;
	int Section = -1;
};
BakeEntry* GetBakeEntry( TArray<BakeEntry*>& BakeEntries, UStaticMesh* Mesh, UMaterialInterface* OriginalMaterial, int Section )
{
	for( int i = 0; i < BakeEntries.Num(); i++ )
	{
		BakeEntry* Entry = BakeEntries[ i ];
		if( Mesh )
		{
			if( Entry->Mesh == Mesh && Entry->OriginalMaterial == OriginalMaterial && Entry->Section == Section )
				return Entry;
		}
		else
		{
			if( Entry->OriginalMaterial == OriginalMaterial && Entry->Section == Section )
				return Entry;
		}
	}

	return nullptr;
}
bool CheckOOM()
{
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	double UsedMemoryPercentage = (double)Stats.AvailablePhysical / (double)Stats.TotalPhysical;
	double RemainingMemoryGB = (double)( Stats.AvailablePhysical ) / 1000000000.0;
	if( UsedMemoryPercentage < 0.1f )
	{
		static bool AlreadyDismissed = false;
		if( AlreadyDismissed )
		{
			return true;
		}
		FString FinishText = FString::Printf( TEXT(
			"You're almost out of memory ! After this point the export process may crash unreal !\n\n"
			"RemainingPhysicalMemory %0.2fGB\n"
			"Are you sure you want to continue ?\n"
		), RemainingMemoryGB );
		FText Txt = FText::FromString( FinishText );
		EAppReturnType::Type Result = FMessageDialog::Open( EAppMsgType::YesNo, Txt );
		if( Result == EAppReturnType::Type::Yes )
		{
			AlreadyDismissed = true;
			return true;
		}
		else
			return false;
	}
	else
		return true;	
}
void FUnrealToXModule::BakeTextures()
{
	if( !BakeTextureSettings )
	{
		BakeTextureSettings = NewObject<UBakeTextureSettingsClass>();
		BakeTextureSettings->AddToRoot();
	}

	FText Title = FText::FromString( TEXT( "Bake Textures" ) );

	TSharedRef<SWindow> Window = SNew( SWindow )
		.Title( Title )
		.ClientSize( FVector2D( 550, 250 ) )
		.SupportsMinimize( false )
		.SupportsMaximize( false );

	TSharedPtr<SFunctionParamDialog> Dialog;
	
	Window->SetContent(
		SAssignNew( Dialog, SFunctionParamDialog, Window )
		.OkButtonText( LOCTEXT( "OKButton", "OK" ) ) );
	Dialog->StructureDetailsView->SetObject( BakeTextureSettings );

	GEditor->EditorAddModalWindow( Window );

	if( !Dialog->bOKPressed )
		return;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	//Destroy previously baked textures if any
	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	TArray<TArray<UMaterialInterface*>> OriginalMaterialSets;
	//TArray<TArray<UMaterialInterface*>> BakedMaterials;

	for( TActorIterator<AActor> ActorIterator( World ); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		if( Actor->IsHiddenEd() )
			continue;

		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );

		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( AC );
			if( StaticMeshComponent && StaticMeshComponent->GetStaticMesh() )
			{
				StaticMeshComponents.Add( StaticMeshComponent );
				OriginalMaterialSets.Add( StaticMeshComponent->GetMaterials() );
			}
		}
	}

	TArray<BakeEntry*> BakeEntries;
	TArray<UStaticMeshComponent*> UniqueStaticMeshComponents;
	TArray<int> UniqueStaticMeshIndices;

	for( int i = 0; i < StaticMeshComponents.Num(); i++ )
	{
		UStaticMeshComponent* StaticMeshComponent = StaticMeshComponents[ i ];

		TArray<UMaterialInterface*> OriginalMaterials = StaticMeshComponent->GetMaterials();

		for( int m = 0; m < OriginalMaterials.Num(); m++ )
		{
			UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
			if( BakeTextureSettings->OneTextureSetPerMaterial )
				Mesh = nullptr;
			BakeEntry* Entry = GetBakeEntry( BakeEntries, Mesh, OriginalMaterials[ m ], m );
			if( !Entry )
			{
				Entry = new BakeEntry( StaticMeshComponent->GetStaticMesh(), OriginalMaterials[ m ], m );
				BakeEntries.Add( Entry );

				int Index = UniqueStaticMeshComponents.Find( StaticMeshComponent );
				if ( Index == INDEX_NONE )
				{
					UniqueStaticMeshComponents.Add( StaticMeshComponent );
					int SMCIndex = StaticMeshComponents.Find( StaticMeshComponent );
					UniqueStaticMeshIndices.Add( SMCIndex );
				}
			}
		}
	}

	FScopedSlowTask BakeProgress( UniqueStaticMeshComponents.Num() );
	BakeProgress.MakeDialog();

	TArray<UMaterialInterface*> FixedMaterials;
	double StartTime = FPlatformTime::Seconds();
	for( int i = 0; i < UniqueStaticMeshComponents.Num(); i++ )
	{
		if( !CheckOOM() )
			return;
		auto StaticMeshComponent = UniqueStaticMeshComponents[ i ];
		double NowTime = FPlatformTime::Seconds();
		float ElapsedTime = NowTime - StartTime;
		float EstimatedTime = ElapsedTime / (float)i * (float)UniqueStaticMeshComponents.Num();
		float LeftTime = EstimatedTime - ElapsedTime;
		if( i == 0 )
			EstimatedTime = 0.0f;
		FString Text = FString::Printf( TEXT( "BakeTextures %d/%d Elapsed %0.2f seconds Remaining %0.2f seconds" ), i, UniqueStaticMeshComponents.Num(), ElapsedTime, LeftTime );
		FText StrTxt = FText::FromString( Text );
		BakeProgress.EnterProgressFrame( 1.0f, StrTxt );		

		UMaterialOptions* MaterialOptions = DuplicateObject( GetMutableDefault<UMaterialOptions>(), GetTransientPackage() );
		UAssetBakeOptions* AssetOptions = GetMutableDefault<UAssetBakeOptions>();
		UMaterialMergeOptions* MergeOptions = GetMutableDefault<UMaterialMergeOptions>();
		TArray<TWeakObjectPtr<UObject>> Objects{ MergeOptions, AssetOptions, MaterialOptions };
				
		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		const int32 NumLODs = StaticMesh->GetNumLODs();
		IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>( "MaterialBaking" );
		static bool ShowOptions = false;
		if( ShowOptions )
		{
			if( !Module.SetupMaterialBakeSettings( Objects, NumLODs ) )
			{
				return;
			}
		}
		else
		{
			for(int lod=1; lod < NumLODs; lod++ )
				MaterialOptions->LODIndices.Add( lod );
					
			MaterialOptions->Properties.Add( MP_Metallic );
			MaterialOptions->Properties.Add( MP_Roughness );
			MaterialOptions->Properties.Add( MP_Normal );
			MaterialOptions->Properties.Add( MP_AmbientOcclusion );
			MaterialOptions->Properties.Add( MP_Opacity );
			MaterialOptions->Properties.Add( MP_OpacityMask );
			MaterialOptions->Properties.Add( MP_EmissiveColor );

			MaterialOptions->TextureSize = FIntPoint( BakeTextureSettings->TargetResolution, BakeTextureSettings->TargetResolution );
			MaterialOptions->bUseSpecificUVIndex = true;
			MaterialOptions->TextureCoordinateIndex = BakeTextureSettings->UVChannel;
			MaterialOptions->LODIndices[0] = BakeTextureSettings->LOD;
			MaterialOptions->bUseMeshData = BakeTextureSettings->UseMeshData;
		}
				
		// Bake out materials for static mesh component
		FStaticMeshComponentAdapter Adapter( StaticMeshComponent );
		const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>( "MeshMergeUtilities" ).GetUtilities();
		MeshMergeUtilities.BakeMaterialsForComponent( Objects, &Adapter );

		TArray<UMaterialInterface*> OriginalMaterials = OriginalMaterialSets[ UniqueStaticMeshIndices[i] ];
		TArray<UMaterialInterface*> BakedMaterials = StaticMeshComponent->GetMaterials();
		for( int m = 0; m < BakedMaterials.Num(); m++ )
		{
			UMaterialInterface* Mat = BakedMaterials[ m ];
			UMaterialInstance* MatInstance = Cast< UMaterialInstance>( Mat );
			if( !MatInstance )
				continue;

			//Don't fix twice due to renaming issues
			if( FixedMaterials.Find( Mat ) != INDEX_NONE )
			{
				BakeEntry* Entry = GetBakeEntry( BakeEntries, StaticMeshComponent->GetStaticMesh(), OriginalMaterials[ m ], m );
				if( Entry )
				{
					if( Entry->BakedMaterial == nullptr )
						Entry->BakedMaterial = Mat;
				}
				continue;
			}
			FixedMaterials.Add( Mat );

			float MetallicVal = 0.0f;
			MatInstance->GetScalarParameterValue( TEXT( "MetallicConst" ), MetallicVal, true );
			float OriginalMetallic = ConvertToOriginalValue( MetallicVal );
			FColor MetallicColor( (uint8)( OriginalMetallic * 255.0f ), (uint8)( OriginalMetallic * 255.0f ), (uint8)( OriginalMetallic * 255.0f ) );
			FLinearColor BaseColor;
			MatInstance->GetVectorParameterValue( TEXT( "BaseColorConst" ), BaseColor, true );
			float RoughnessConst = 0.5;
			MatInstance->GetScalarParameterValue( TEXT( "RoughnessConst" ), RoughnessConst, true );
			float AmbientOcclusionConst = 1.0;
			MatInstance->GetScalarParameterValue( TEXT( "AmbientOcclusionConst" ), AmbientOcclusionConst, true );
			FLinearColor RoughnessConstColor( RoughnessConst, RoughnessConst, RoughnessConst, RoughnessConst );
			FLinearColor AmbientOcclusionConstColor( AmbientOcclusionConst, AmbientOcclusionConst, AmbientOcclusionConst, AmbientOcclusionConst );

			FHashedMaterialParameterInfo ParamInfoNormal( TEXT( "NormalTexture" ) );
			UTexture* NormalTexture = nullptr;
			if( MatInstance->GetTextureParameterValue( ParamInfoNormal, NormalTexture, true ) && NormalTexture )
			{
				NormalTexture->Modify();
				NormalTexture->bFlipGreenChannel = 1;
				NormalTexture->PostEditChange();
			}

			CombineMode MetallicRoughnessCombine = CM_INVERSE_ROUGHNESS_TO_METALLIC;
			if( ExporterInterface && ExporterInterface->Type == EngineType::GODOT )
			{
				MetallicRoughnessCombine = CM_ROUGHNESS_TO_METALLIC;
			}
			CombineTextures( TEXT( "MetallicTexture" ), TEXT( "RoughnessTexture" ), MetallicColor, RoughnessConstColor.ToFColor( true ), MatInstance, MetallicRoughnessCombine, MatInstance->GetPackage());
			//Blend either OpacityMask (for alpha test) or Opacity for translucency
			FString TargetAlphaTexture = TEXT( "OpacityMaskTexture" );
			FHashedMaterialParameterInfo ParamInfoOpacityTex( *TargetAlphaTexture );
			UTexture* TexOpacity = nullptr;
			if (!MatInstance->GetTextureParameterValue( ParamInfoOpacityTex, TexOpacity, true))
			{
				TargetAlphaTexture = TEXT( "OpacityTexture" );
			}
			CombineTextures( TEXT( "BaseColorTexture" ), TargetAlphaTexture, BaseColor.ToFColor( true ), FColor(0,0,0,0), MatInstance, CM_OPACITYMASK_TO_BASECOLOR_ALPHA, MatInstance->GetPackage() );
			//HDRP uses a combined Metallic,AO,Detail,Smoothness texture
			//UUnitySettingsClass* UnitySettings = Cast< UUnitySettingsClass>( ExporterInterface->Settings );
			//if( UnitySettings && UnitySettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
			if ( BakeTextureSettings && BakeTextureSettings->ScriptableRenderPipeline == RenderPipeline::RP_HDRP )
			{
				CombineTextures( TEXT( "MetallicTexture" ), TEXT( "AmbientOcclusionTexture" ), MetallicColor, AmbientOcclusionConstColor.ToFColor( true ), MatInstance, CM_OCCLUSION_TO_GREEN, MatInstance->GetPackage() );
			}

			static bool DoRename = true;
			BakeEntry* Entry = GetBakeEntry( BakeEntries, StaticMeshComponent->GetStaticMesh(), OriginalMaterials[ m ], m );
			if( Entry )
			{
				if( Entry->BakedMaterial == nullptr )
				{
					Entry->BakedMaterial = Mat;
					DoRename = true;
				}
				else
					DoRename = false;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>( OriginalMaterials[ m ] );
			if( DoRename && !MID )
			{
				FString Str = FString::Printf( TEXT( "%s_%s_%d" ), *StaticMesh->GetName(), *OriginalMaterials[ m ]->GetName(), m );
				FString ActualName = MatInstance->GetName();
				bool Removed = ActualName.RemoveFromStart( TEXT( "M_" ) );
				bool RenameSuccess = RenameAsset( MatInstance, ActualName, *Str );

				static bool DoRenameTextures = true;
				if( DoRenameTextures )
					for( int t = 0; t < MatInstance->TextureParameterValues.Num(); t++ )
					{
						auto Param = MatInstance->TextureParameterValues[ t ];
						UTexture* Texture = Param.ParameterValue;
						if( Texture )
						{
							Str = FString::Printf( TEXT( "_%s" ), *Param.ParameterInfo.Name.ToString() );
							ActualName = Texture->GetName();
							Removed = ActualName.RemoveFromStart( TEXT( "T_" ) );
							RenameSuccess = RenameAsset( Texture, ActualName, *Str, false );
						}
					}
			}
		}

		StaticMeshComponent->MarkRenderStateDirty();
		//BakedMaterials.Add( StaticMeshComponent->GetMaterials() );
	}

	//Assign the baked materials
	for( int i = 0; i < StaticMeshComponents.Num(); i++ )
	{
		auto StaticMeshComponent = StaticMeshComponents[ i ];
		TArray<UMaterialInterface*> OriginalMaterialsArray = OriginalMaterialSets[ i ];
		for( int m = 0; m < OriginalMaterialsArray.Num(); m++ )
		{
			UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
			if( BakeTextureSettings->OneTextureSetPerMaterial )
				Mesh = nullptr;
			BakeEntry* Entry = GetBakeEntry( BakeEntries, Mesh, OriginalMaterialsArray[ m ], m );
			if( Entry && Entry->BakedMaterial )
			{
				StaticMeshComponent->SetMaterial( m, Entry->BakedMaterial );
			}
		}
	}
}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
class FDynamicMesh3Derived : public FDynamicMesh3
{
public:
	int TriangleCountDerived()
	{
		return (int)TriangleRefCounts.GetCount();
	}
};
#endif
bool FUnrealToXModule::ConvertDynamicMeshToStaticMesh( FString NewAssetPath, UDynamicMeshComponent* DynamicMeshComponent, UStaticMesh*& OutStaticMesh )
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
	UE::Geometry::FDynamicMesh3* DynamicMesh = DynamicMeshComponent->GetMesh();
	int TriangleCount = ( (FDynamicMesh3Derived*)DynamicMesh )->TriangleCountDerived();
	if (!DynamicMesh || TriangleCount == 0 )
		return false;
	auto BaseMaterials = DynamicMeshComponent->BaseMaterials;
	FString NewObjectName = FPackageName::GetLongPackageAssetName( NewAssetPath );

	UPackage* UsePackage;
	//if (Options.UsePackage != nullptr)
	//{
	//	UsePackage = Options.UsePackage;
	//}
	//else
	//{
		UsePackage = CreatePackage(*NewAssetPath);
	//}
	if (ensure(UsePackage != nullptr) == false)
	{
		return false;
	}

	// create new UStaticMesh object
	EObjectFlags UseFlags = EObjectFlags::RF_Public;// | EObjectFlags::RF_Standalone;
	UStaticMesh* NewStaticMesh = NewObject<UStaticMesh>(UsePackage, FName(*NewObjectName), UseFlags);
	if (ensure(NewStaticMesh != nullptr) == false)
	{
		return false;
	}

	// initialize the MeshDescription SourceModel LODs
	int NumSourceModels = 1;
	int32 UseNumSourceModels = FMath::Max(1, NumSourceModels);
	NewStaticMesh->SetNumSourceModels(UseNumSourceModels);
	for (int32 k = 0; k < UseNumSourceModels; ++k)
	{
		FMeshBuildSettings& BuildSettings = NewStaticMesh->GetSourceModel(k).BuildSettings;

		BuildSettings.bRecomputeNormals = 0;// Options.bEnableRecomputeNormals;
		BuildSettings.bRecomputeTangents = 0;// Options.bEnableRecomputeTangents;
		BuildSettings.bGenerateLightmapUVs = 0;// Options.bGenerateLightmapUVs;

		if (false)//!Options.bAllowDistanceField)
		{
			BuildSettings.DistanceFieldResolutionScale = 0.0f;
		}

		NewStaticMesh->CreateMeshDescription(k);
	}

	// create physics body and configure appropriately
	bool bCreatePhysicsBody = true;
	if (bCreatePhysicsBody)
	{
		NewStaticMesh->CreateBodySetup();
		ECollisionTraceFlag CollisionType = ECollisionTraceFlag::CTF_UseDefault;
		NewStaticMesh->GetBodySetup()->CollisionTraceFlag = CollisionType;
	}

	int NumMaterialSlots = 1;

	TArray<FStaticMaterial> StaticMaterials;
	for (int32 MatIdx = 0; MatIdx < BaseMaterials.Num(); ++MatIdx)
	{
		FStaticMaterial NewMaterial;
		// fallback to default material if no material is found
		if (MatIdx < BaseMaterials.Num() && BaseMaterials[MatIdx] != nullptr)
		{
			NewMaterial.MaterialInterface = BaseMaterials[MatIdx];
			NewMaterial.MaterialSlotName = FName( FString::Printf( TEXT( "Mat%d_%s" ), MatIdx, *NewMaterial.MaterialInterface->GetName() ) );
		}
		else
		{
			NewMaterial.MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
			NewMaterial.MaterialSlotName = FName( FString::Printf( TEXT( "NoMat%d" ), MatIdx ) );
		}
		NewMaterial.ImportedMaterialSlotName = NewMaterial.MaterialSlotName;
		NewMaterial.UVChannelData = FMeshUVChannelInfo(1.f);		// this avoids an ensure in  UStaticMesh::GetUVChannelData
		StaticMaterials.Add(NewMaterial);
	}
	NewStaticMesh->SetStaticMaterials(StaticMaterials);


	// determine maximum number of sections across all mesh LODs
	int32 MaxNumSections = 0;

	// if options included SourceModel meshes, copy them over
	//if (Options.SourceMeshes.MoveMeshDescriptions.Num() > 0)
	//{
	//	if (ensure(Options.SourceMeshes.MoveMeshDescriptions.Num() == UseNumSourceModels))
	//	{
	//		for (int32 k = 0; k < UseNumSourceModels; ++k)
	//		{
	//			FMeshDescription* Mesh = NewStaticMesh->GetMeshDescription(k);
	//			*Mesh = MoveTemp(*Options.SourceMeshes.MoveMeshDescriptions[k]);
	//			MaxNumSections = FMath::Max(MaxNumSections, Mesh->PolygonGroups().Num());
	//			NewStaticMesh->CommitMeshDescription(k);
	//		}
	//	}
	//}
	//else if (Options.SourceMeshes.MeshDescriptions.Num() > 0)
	//{
	//	if (ensure(Options.SourceMeshes.MeshDescriptions.Num() == UseNumSourceModels))
	//	{
	//		for (int32 k = 0; k < UseNumSourceModels; ++k)
	//		{
	//			FMeshDescription* Mesh = NewStaticMesh->GetMeshDescription(k);
	//			*Mesh = *Options.SourceMeshes.MeshDescriptions[k];
	//			MaxNumSections = FMath::Max(MaxNumSections, Mesh->PolygonGroups().Num());
	//			NewStaticMesh->CommitMeshDescription(k);
	//		}
	//	}
	//}
	//else if (Options.SourceMeshes.DynamicMeshes.Num() > 0)
	{
		//if (ensure(Options.SourceMeshes.DynamicMeshes.Num() == UseNumSourceModels))
		{
			for (int32 k = 0; k < UseNumSourceModels; ++k)
			{
				FMeshDescription* Mesh = NewStaticMesh->GetMeshDescription(k);
				FDynamicMeshToMeshDescription DynamicMeshToMeshDescriptionConverter;
				DynamicMeshToMeshDescriptionConverter.Convert( DynamicMesh, *Mesh, !0);
				MaxNumSections = FMath::Max(MaxNumSections, Mesh->PolygonGroups().Num());
				NewStaticMesh->CommitMeshDescription(k);
			}
		}
	}

	int32 CurNumValidSections = BaseMaterials.Num();
	while (CurNumValidSections < MaxNumSections)
	{
		NewStaticMesh->GetStaticMaterials().Add(FStaticMaterial());
		CurNumValidSections++;
	}

	// Nanite options
	NewStaticMesh->NaniteSettings.bEnabled = false;
	//if (Options.bGenerateNaniteEnabledMesh)
	//{
	//	NewStaticMesh->NaniteSettings = Options.NaniteSettings;
	//}

	// Ray tracing
	NewStaticMesh->bSupportRayTracing = true;// Options.bSupportRayTracing;

	// Distance field
	NewStaticMesh->bGenerateMeshDistanceField = true;// Options.bAllowDistanceField;

	// Set the initial overall static mesh lightmap settings to match the first lightmap build settings
	//if (Options.bGenerateLightmapUVs)
	//{
	//	FMeshBuildSettings& BuildSettings = NewStaticMesh->GetSourceModel(0).BuildSettings;
	//	NewStaticMesh->SetLightMapCoordinateIndex(BuildSettings.DstLightmapIndex);
	//	NewStaticMesh->SetLightMapResolution(BuildSettings.MinLightmapResolution);
	//}

	[[maybe_unused]] bool MarkedDirty = NewStaticMesh->MarkPackageDirty();
	//if (Options.bDeferPostEditChange == false)
	{
		NewStaticMesh->PostEditChange();
	}

	OutStaticMesh = NewStaticMesh;
	return true;
#else
	return false;
#endif
}
bool FUnrealToXModule::ConvertProceduralMeshToStaticMesh( FString NewAssetPath, UProceduralMeshComponent* ProceduralMeshComponent, UStaticMesh*& OutStaticMesh )
{
	LandscapeMeshData MeshData;
	MeshData.MeshAssetName = NewAssetPath;
	FMeshDescription MeshDescription = BuildMeshDescription( ProceduralMeshComponent );
	MeshData.ProceduralMesh = ProceduralMeshComponent;
	MeshData.MeshDescription = &MeshDescription;

	OutStaticMesh = FLandscapeTools::CreateStaticMeshFromProceduralMesh( MeshData.MeshAssetName, &MeshData );
	MeshData.MeshDescription = nullptr;
	if( OutStaticMesh )
		return true;

	return false;
}
UTexture2D* GetHeightmap( ULandscapeComponent* LandscapeComponent )
{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	return LandscapeComponent->HeightmapTexture;
#else
	return LandscapeComponent->GetHeightmap();
#endif
}
void GetLandscapeData( ALandscapeProxy* Landscape )
{
	if( Landscape )
	{
		TArray< UTexture2D*> AllHeightmaps;

		for( int32 ComponentIndex = 0; ComponentIndex < Landscape->LandscapeComponents.Num(); ComponentIndex++ )
		{
			ULandscapeComponent* Component = Landscape->LandscapeComponents[ ComponentIndex ];

			UTexture2D* HeightMap = GetHeightmap( Component );
			if ( AllHeightmaps.Find( HeightMap ) == INDEX_NONE )
				AllHeightmaps.Add( HeightMap );
			FLandscapeComponentGrassData& GrassData = Component->GrassData.Get();

			#if ENGINE_MAJOR_VERSION == 5
			TArray<uint8>& HeightWeightData = GrassData.HeightWeightData;
			uint8* Data = HeightWeightData.GetData();

			for( auto ItPair : GrassData.WeightOffsets )
			{
				const ULandscapeGrassType* GrassType = ItPair.Key;
				int32 GrassTypeOffset = ItPair.Value;

				TArray64<uint8> PNG_Compressed_ImageData;
				int Width = (int)FMath::Sqrt( (float)GrassData.NumElements );
				uint8* ImageData = Data + GrassTypeOffset;
				TArray<FColor> ColorData;
				for( int i = 0; i < GrassData.NumElements; i++ )
				{
					uint8 Amount = ImageData[i];
					ColorData.Add( FColor( Amount, Amount, Amount, Amount ) );
				}

				PNGCompressImageArray( Width, Width, ColorData, PNG_Compressed_ImageData );

				FString FileName = ExporterInterface->GetAssetsFolder();
				FileName += GrassType->GetName();
				FileName += TEXT( ".png" );
				bool Status = FFileHelper::SaveArrayToFile( PNG_Compressed_ImageData, *FileName );
			}

			TextureBinding Binding;
			Binding.UnrealTexture = HeightMap;
			bool Result = UETextureExporter::DoExport( &Binding );
			#endif
		}

		FTransform LandscapeTransform = Landscape->GetActorTransform();
		auto LandscapePosition = LandscapeTransform.GetLocation();
		auto LandscapeScale = LandscapeTransform.GetScale3D();

		int NumComponentsPerAxis = (int)FMath::Sqrt( (float)Landscape->LandscapeComponents.Num() );
		auto LandscapeSize = FVector( Landscape->ComponentSizeQuads * NumComponentsPerAxis, Landscape->ComponentSizeQuads * NumComponentsPerAxis, 0 );
	}
}
//void FUnrealToXModule::GetTerrainData()
//{
//	TArray< ALandscapeProxy*> AllProxies;;
//	UWorld* World = GEditor->GetEditorWorldContext().World();
//	for( TActorIterator<AActor> iterator( World ); iterator; ++iterator )
//	{
//		AActor* Actor = *iterator;
//		ALandscapeProxy* Proxy = Cast< ALandscapeProxy>( Actor );
//		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor>( Actor );
//
//		if( Proxy )
//		{
//			AllProxies.Add( Proxy );
//		}
//	}
//
//	for( int i = 0; i < AllProxies.Num(); i++ )
//	{
//		GetLandscapeData( AllProxies[ i ] );
//	}
//}
void ReorderMaterialsForGodot( UStaticMesh* StaticMesh, TArray<int>& MaterialIndices )
{
	if( StaticMesh->GetRenderData()->LODResources.Num() == 0 )
		return;

	bool Modified = false;
	TArray<FStaticMaterial> OldMaterials = StaticMesh->GetStaticMaterials();
	TArray<FStaticMaterial> NewMaterials;

	FMeshSectionInfoMap& SectionInfoMap = StaticMesh->GetSectionInfoMap();

	int LOD = 0;
	int NumSections = SectionInfoMap.GetSectionNumber( LOD );
	for( int i = 0; i < NumSections; i++ )
	{
		FMeshSectionInfo SectionInfo = SectionInfoMap.Get( LOD, i );

		MaterialIndices.Add( SectionInfo.MaterialIndex );

		if( SectionInfo.MaterialIndex >= 0 && SectionInfo.MaterialIndex < OldMaterials.Num() &&
			SectionInfo.MaterialIndex != i )
		{			
			NewMaterials.Add( OldMaterials[ SectionInfo.MaterialIndex ] );
			SectionInfo.MaterialIndex = i;
			SectionInfoMap.Set( LOD, i, SectionInfo );
			Modified = true;
		}
		else if( SectionInfo.MaterialIndex == i )
		{
			NewMaterials.Add( OldMaterials[ SectionInfo.MaterialIndex ] );
		}
	}

	if( Modified )
	{
		StaticMesh->SetStaticMaterials( NewMaterials );

		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
	}
}
void FixStaticMeshesWithUnusedMaterialSlots( UStaticMesh* StaticMesh )
{
	if( StaticMesh->GetRenderData()->LODResources.Num() == 0 )
		return;

	int Removals = 0;
	bool RemovedOne = false;
	do
	{
		RemovedOne = false;
		TArray<FStaticMaterial>& Materials = StaticMesh->GetStaticMaterials();

		TArray<int> UsedMaterials;
		for( int i = 0; i < Materials.Num(); i++ )
		{
			UsedMaterials.Add( 0 );
		}

		FMeshSectionInfoMap& SectionInfoMap = StaticMesh->GetSectionInfoMap();
		int LOD = 0;
		int NumSections = SectionInfoMap.GetSectionNumber( LOD );
		//This happens for TerrainMesh for some reason
		if( NumSections == 0 )
			return;

		for( int i = 0; i < NumSections; i++ )
		{
			FMeshSectionInfo SectionInfo = SectionInfoMap.Get( LOD, i );
			if( SectionInfo.MaterialIndex >= 0 && SectionInfo.MaterialIndex < UsedMaterials.Num() )
			{
				UsedMaterials[ SectionInfo.MaterialIndex ]++;
			}
		}

		for( int m = 0; m < UsedMaterials.Num(); m++ )
		{
			if( UsedMaterials[ m ] == 0 )
			{
				RemovedOne = true;
				Removals++;
				Materials.RemoveAt( m );
				StaticMesh->SetStaticMaterials( Materials );

				for( int i = 0; i < NumSections; i++ )
				{
					FMeshSectionInfo SectionInfo = SectionInfoMap.Get( LOD, i );
					if( SectionInfo.MaterialIndex > m )
					{
						SectionInfo.MaterialIndex--;
						SectionInfoMap.Set( LOD, i, SectionInfo );
					}
				}

				break;
			}
		}
	} while( RemovedOne );

	if( Removals > 0 )
	{
		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
	}
}
void FixNullMaterials( UStaticMesh* StaticMesh )
{
	TArray<FStaticMaterial>& Materials = StaticMesh->GetStaticMaterials();

	UObject* LoadedObject = StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, TEXT( "/Engine/EngineMaterials/DefaultMaterial" ) );
	UMaterialInterface* DefaultMaterial = Cast<UMaterialInterface>( LoadedObject );
	bool Modified = false;
	for( int i = 0; i < Materials.Num(); i++ )
	{
		if( !Materials[ i ].MaterialInterface )
		{
			Modified = true;
			Materials[ i ].MaterialInterface = DefaultMaterial;
		};
	}

	if( Modified )
	{
		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
	}
}
template<class TYPE>
void IterateOverAllComponentsInScene( TYPE lambda )
{
	UWorld* world = GEditor->GetEditorWorldContext().World();
	for( TActorIterator<AActor> iterator( world ); iterator; ++iterator )
	{
		AActor* Actor = *iterator;
		if( !Actor )
			continue;
		if( Actor->IsHiddenEd() )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );
		int SplineComponents = 0;
		for( int i = 0; i < comps.Num(); i++ )
		{
			if ( comps[ i ] )
				lambda( comps[ i ]);
		}
	}
}
class MaterialRemapping
{
public:
	UStaticMesh* StaticMesh = nullptr;
	TArray<int> MaterialIndices;
	~MaterialRemapping()
	{
		MaterialIndices.Reset( 0 );
	}
};
void FUnrealToXModule::ReorderMaterials()
{
	TArray<MaterialRemapping*> Remappings;

	auto lambda = [&]( UActorComponent* Component )
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( Component );
		if( StaticMeshComponent )
		{
			UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
			if( !Mesh )
				return;

			MaterialRemapping* FoundMaterial = nullptr;
			MaterialRemapping** Found = Remappings.FindByPredicate( [&]( const MaterialRemapping* Item ) { return Item->StaticMesh == Mesh; } );
			if( Found )
			{
				FoundMaterial = *Found;
			}
			else
			{
				MaterialRemapping* NewMaterialRemapping = new MaterialRemapping;
				Remappings.Add( NewMaterialRemapping );
				NewMaterialRemapping->StaticMesh = Mesh;

				FixStaticMeshesWithUnusedMaterialSlots( Mesh );
				FixNullMaterials( Mesh );
				ReorderMaterialsForGodot( Mesh, NewMaterialRemapping->MaterialIndices );

				FoundMaterial = NewMaterialRemapping;
			}

			if( StaticMeshComponent->OverrideMaterials.Num() > 0 )
			{
				TArray<UMaterialInterface*> ReorderedOverrideMaterials;
				for( int i = 0; i < FoundMaterial->MaterialIndices.Num(); i++ )
				{
					int ReorderedIndex = FoundMaterial->MaterialIndices[ i ];
					if( ReorderedIndex < StaticMeshComponent->OverrideMaterials.Num() )
						ReorderedOverrideMaterials.Add( StaticMeshComponent->OverrideMaterials[ ReorderedIndex ] );
					else
					{
						ReorderedOverrideMaterials.Add( nullptr );
					}
				}

				StaticMeshComponent->OverrideMaterials = ReorderedOverrideMaterials;
			}
		}
	};

	IterateOverAllComponentsInScene( lambda );
}
void FUnrealToXModule::FixSkeletalMeshBoneInfluences()
{
	std::function<void( UActorComponent* )> lambda = [&]( UActorComponent* Component )
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>( Component );
		if( SkeletalMeshComponent )
		{
			USkeletalMesh* SkeletalMesh = GetSkeletalMesh( SkeletalMeshComponent );
			if( !SkeletalMesh )
				return;

			bool Modified = false;
			TArray<FSkeletalMeshLODInfo>& LODInfoArray = SkeletalMesh->GetLODInfoArray();
			for( int i = 0; i < LODInfoArray.Num(); i++ )
			{
				FSkeletalMeshLODInfo& LODInfo = LODInfoArray[ i ];
				//Limit to 4 or else the export will look broken
				#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
				if( LODInfo.BuildSettings.BoneInfluenceLimit == 0 || LODInfo.BuildSettings.BoneInfluenceLimit > 4 )
				{
					LODInfo.BuildSettings.BoneInfluenceLimit = 4;
					Modified = true;
				}
				#endif
			}
			if( Modified )
			{
				SkeletalMesh->PostEditChange();
				SkeletalMesh->MarkPackageDirty();
			}
		}
	};

	IterateOverAllComponentsInScene( lambda );
}
void FUnrealToXModule::RevertSubstrateMaterials()
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	auto lambda = [&]( FAssetData& Asset )
	{
		UObject* AssetObject = Asset.GetAsset();
		UMaterial* Material = Cast< UMaterial >( AssetObject );
		if( Material )
		{
			TArray<UMaterialExpression*> Expressions = GetExpressions( Material );
			for( int i = 0; i < Expressions.Num(); i++ )
			{
				UMaterialExpression* Exp = Expressions[ i ];

				UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
				bool Modified = false;

			#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
				UMaterialExpressionSubstrateShadingModels* ShadingModels = Cast< UMaterialExpressionSubstrateShadingModels >( Exp );
				UMaterialExpressionSubstrateSlabBSDF* SlabBSDF = Cast< UMaterialExpressionSubstrateSlabBSDF >( Exp );
				UMaterialExpressionSubstrateUnlitBSDF* UnlitBSDF = Cast< UMaterialExpressionSubstrateUnlitBSDF >( Exp );
				UMaterialExpressionSubstrateSingleLayerWaterBSDF* SingleLayerWaterBSDF = Cast< UMaterialExpressionSubstrateSingleLayerWaterBSDF >( Exp );
			#else
				UMaterialExpressionStrataLegacyConversion* ShadingModels = Cast< UMaterialExpressionStrataLegacyConversion>( Exp );
				UMaterialExpressionStrataSlabBSDF* SlabBSDF = Cast< UMaterialExpressionStrataSlabBSDF>( Exp );
				UMaterialExpressionStrataUnlitBSDF* UnlitBSDF = Cast< UMaterialExpressionStrataUnlitBSDF >(Exp);
				UMaterialExpressionStrataSingleLayerWaterBSDF* SingleLayerWaterBSDF = Cast< UMaterialExpressionStrataSingleLayerWaterBSDF >(Exp);
			#endif
				
				if( ShadingModels )
				{
					if( !EditorOnlyData->Opacity.Expression )
						EditorOnlyData->Opacity.Expression = ShadingModels->Opacity.Expression;
					if( !EditorOnlyData->EmissiveColor.Expression )
						EditorOnlyData->EmissiveColor.Expression = ShadingModels->EmissiveColor.Expression;
					if( !EditorOnlyData->BaseColor.Expression )
						EditorOnlyData->BaseColor.Expression = ShadingModels->BaseColor.Expression;
					if( !EditorOnlyData->Normal.Expression )
						EditorOnlyData->Normal.Expression = ShadingModels->Normal.Expression;
					if( !EditorOnlyData->SubsurfaceColor.Expression )
						EditorOnlyData->SubsurfaceColor.Expression = ShadingModels->SubSurfaceColor.Expression;
					if( !EditorOnlyData->ClearCoat.Expression )
						EditorOnlyData->ClearCoat.Expression = ShadingModels->ClearCoat.Expression;
					if( !EditorOnlyData->Metallic.Expression )
						EditorOnlyData->Metallic.Expression = ShadingModels->Metallic.Expression;
					if( !EditorOnlyData->Specular.Expression )
						EditorOnlyData->Specular.Expression = ShadingModels->Specular.Expression;
					if( !EditorOnlyData->Roughness.Expression )
						EditorOnlyData->Roughness.Expression = ShadingModels->Roughness.Expression;

					Modified = true;
				}
				if( SlabBSDF )
				{
					if( !EditorOnlyData->EmissiveColor.Expression )
						EditorOnlyData->EmissiveColor.Expression = SlabBSDF->EmissiveColor.Expression;
					if( !EditorOnlyData->BaseColor.Expression )
						EditorOnlyData->BaseColor.Expression = SlabBSDF->DiffuseAlbedo.Expression;
					if( !EditorOnlyData->Normal.Expression )
						EditorOnlyData->Normal.Expression = SlabBSDF->Normal.Expression;
					if( !EditorOnlyData->Roughness.Expression )
						EditorOnlyData->Roughness.Expression = SlabBSDF->Roughness.Expression;

					Modified = true;
				}
				if( UnlitBSDF )
				{
					if( !EditorOnlyData->EmissiveColor.Expression )
						EditorOnlyData->EmissiveColor.Expression = UnlitBSDF->EmissiveColor.Expression;
					#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
					if( !EditorOnlyData->Normal.Expression )
						EditorOnlyData->Normal.Expression = UnlitBSDF->Normal.Expression;
					#endif

					Modified = true;
				}
				if( SingleLayerWaterBSDF )
				{
					if( !EditorOnlyData->EmissiveColor.Expression )
						EditorOnlyData->EmissiveColor.Expression = SingleLayerWaterBSDF->EmissiveColor.Expression;
					if( !EditorOnlyData->BaseColor.Expression )
						EditorOnlyData->BaseColor.Expression = SingleLayerWaterBSDF->BaseColor.Expression;
					if( !EditorOnlyData->Normal.Expression )
						EditorOnlyData->Normal.Expression = SingleLayerWaterBSDF->Normal.Expression;
					if( !EditorOnlyData->Metallic.Expression )
						EditorOnlyData->Metallic.Expression = SingleLayerWaterBSDF->Metallic.Expression;
					if( !EditorOnlyData->Specular.Expression )
						EditorOnlyData->Specular.Expression = SingleLayerWaterBSDF->Specular.Expression;
					if( !EditorOnlyData->Roughness.Expression )
						EditorOnlyData->Roughness.Expression = SingleLayerWaterBSDF->Roughness.Expression;

					Modified = true;
				}
				if( Modified )
				{
					Material->PostEditChange();
					Material->MarkPackageDirty();
				}
			}
		}
	};

	IterateOverSelection( lambda );
#endif
}
bool CopyToEngineVersion( const TCHAR* DestinationDirectory, const TCHAR* Source, const char* Version )
{
	bool Result = true;

	FString PluginFile = Source;
	FString PluginName = ExporterInterface->GetPluginName();

	PluginFile += FString::Printf( TEXT( "/%s.uplugin" ), *PluginName );
	std::string PluginFileStr = ToANSIString( *PluginFile );
	uint8* Data = nullptr;
	int64_t Size = LoadFile( PluginFileStr.c_str(), &Data );
	if (Size > 0)
	{
		std::string FileAsString = (char*)Data;
		const char* Marker = "\"EngineVersion\": \"";
		int pos = FileAsString.find( Marker );
		if (pos != -1)
		{
			int InsertPos = pos + strlen( Marker );
			FileAsString.insert( InsertPos, Version );
			Result = SaveFile( PluginFileStr.c_str(), (void*)FileAsString.c_str(), FileAsString.length() );
		}
		else
			return false;
	}
	else
		return false;

	Result = FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively( DestinationDirectory ) && Result;
	Result = FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree( DestinationDirectory, Source, true ) && Result;

	if ( Data )
	{
		//Write back unmodified uplugin so I can repeat the operation
		Result = SaveFile( PluginFileStr.c_str(), Data, Size );
	}
	return Result;
}
void ExecuteCommand( const TCHAR* URL, const TCHAR* Parms )
{
	FProcHandle Handle = FPlatformProcess::CreateProc( URL, Parms, false, false, false, nullptr, 0, nullptr, nullptr );
	FPlatformProcess::WaitForProc( Handle );
}
void DeletePluginDirectories( FString PluginName, int FromVersion, int ToVersion )
{
	for (int i = FromVersion; i <= ToVersion; i++)
	{
		FString Path = FString::Printf( TEXT( "C:/UnrealProjects/Test5_%d/Plugins/%s" ), i, *PluginName );
		FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively( *Path );
	}
}
bool CopyToMultipleEngineVersions( FString PluginName, int FromVersion, int ToVersion )
{
	bool Result = true;
	for (int i = FromVersion; i <= ToVersion; i++)
	{
		char Text[255];
		snprintf( Text, sizeof( Text ), "%d.%d", ENGINE_MAJOR_VERSION, i );
		//FString Version = FString::Printf( TEXT( "%d.%d" ), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION );
		Result = CopyToEngineVersion( 
			*FString::Printf( TEXT( "C:/UnrealProjects/Test%d_%d/Plugins/%s" ), ENGINE_MAJOR_VERSION, i, *PluginName ),
			*FString::Printf( TEXT( "C:/MyProjects/Unreal/%s" ), *PluginName ), Text ) && Result;
	}
	return Result;
}
void FindPluginPDBFilesRecursively( TArray<FString>& PDBFiles, FString PluginName, int FromVersion, int ToVersion )
{
	for (int i = FromVersion; i <= ToVersion; i++)
	{
		FPlatformFileManager::Get().GetPlatformFile().FindFilesRecursively( PDBFiles, 
			*FString::Printf( TEXT( "C:/UnrealProjects/Test%d_%d/Plugins/%s/" ), ENGINE_MAJOR_VERSION, i, *PluginName ), TEXT( ".pdb" ) );
	}
}
void FUnrealToXModule::PrepareForRelease()
{
	FString LatestUnrealVersion = FString::Printf( TEXT( "%d.%d.%d" ), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION );
#ifdef ENABLE_UNITY_EXPORT
	
	FString Text = TEXT( "DoInitialCopy ?" );
	FText Txt = FText::FromString( Text );
	EAppReturnType::Type MessageDialogResult = FMessageDialog::Open( EAppMsgType::YesNo, Txt );
	if(MessageDialogResult == EAppReturnType::Type::Yes )
	{
		DeletePluginDirectories( TEXT( "UnrealToGodot" ), 1, ENGINE_MINOR_VERSION );

		bool Result;
		CopyToMultipleEngineVersions( TEXT( "UnrealToUnity" ), 0, ENGINE_MINOR_VERSION );
		Result = CopyToEngineVersion( TEXT( "C:/UnrealProjects/Test4_27/Plugins/UnrealToUnity" ), TEXT( "C:/MyProjects/Unreal/UnrealToUnity" ), "4.27" );
	}
	Text = TEXT( "GatherBinaries ?" );
	Txt = FText::FromString( Text );
	MessageDialogResult = FMessageDialog::Open( EAppMsgType::YesNo, Txt );
	if (MessageDialogResult == EAppReturnType::Type::Yes)
	{
		//Delete huge ass PDB files
		TArray<FString> PDBFiles;
		FindPluginPDBFilesRecursively( PDBFiles, TEXT( "UnrealToUnity" ), 0, ENGINE_MINOR_VERSION );
		FPlatformFileManager::Get().GetPlatformFile().FindFilesRecursively( PDBFiles, TEXT( "C:/UnrealProjects/Test4_27/Plugins/UnrealToUnity/" ), TEXT( ".pdb" ) );
		for( int i = 0; i < PDBFiles.Num(); i++ )
		{
			FPlatformFileManager::Get().GetPlatformFile().DeleteFile( *PDBFiles[ i ] );
		}

		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			*FString::Printf( TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_%s.zip C:/UnrealProjects/Test5_7/Plugins/UnrealToUnity/*.*" ), *LatestUnrealVersion ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_5.6.1.zip C:/UnrealProjects/Test5_6/Plugins/UnrealToUnity/*.*" ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_5.5.4.zip C:/UnrealProjects/Test5_5/Plugins/UnrealToUnity/*.*" ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_5.4.4.zip C:/UnrealProjects/Test5_4/Plugins/UnrealToUnity/*.*" ));
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_5.3.2.zip C:/UnrealProjects/Test5_3/Plugins/UnrealToUnity/*.*" ));
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_5.2.1.zip C:/UnrealProjects/Test5_2/Plugins/UnrealToUnity/*.*" ));
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_5.1.1.zip C:/UnrealProjects/Test5_1/Plugins/UnrealToUnity/*.*" ));
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_5.0.3.zip C:/UnrealProjects/Test5_0/Plugins/UnrealToUnity/*.*" ));
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_4.27.2.zip C:/UnrealProjects/Test4_27/Plugins/UnrealToUnity/*.*" ));

		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTU_Release/UnrealToUnity_With_Binaries.zip C:/MyProjects/Unreal/UTU_Release/*.*" ));
	}
#endif
#ifdef ENABLE_GODOT_EXPORT
	FString Text = TEXT( "DoInitialCopy ?" );
	FText Txt = FText::FromString( Text );
	EAppReturnType::Type MessageDialogResult = FMessageDialog::Open( EAppMsgType::YesNo, Txt );
	if (MessageDialogResult == EAppReturnType::Type::Yes)
	{
		DeletePluginDirectories( TEXT( "UnrealToUnity" ), 1, ENGINE_MINOR_VERSION );

		bool Result;
		CopyToEngineVersion( TEXT( "UnrealToGodot" ), 1, ENGINE_MINOR_VERSION );
	}
	Text = TEXT( "GatherBinaries ?" );
	Txt = FText::FromString( Text );
	MessageDialogResult = FMessageDialog::Open( EAppMsgType::YesNo, Txt );
	if (MessageDialogResult == EAppReturnType::Type::Yes)
	{
		//Delete huge ass PDB files
		TArray<FString> PDBFiles;
		FindPluginPDBFilesRecursively( PDBFiles, TEXT( "UnrealToGodot" ), 1, ENGINE_MINOR_VERSION );
		
		for( int i = 0; i < PDBFiles.Num(); i++ )
		{
			FPlatformFileManager::Get().GetPlatformFile().DeleteFile( *PDBFiles[ i ] );
		}
		
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ), 
			*FString::Printf( TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTG_Release/UnrealToGodot_%s.zip C:/UnrealProjects/Test5_7/Plugins/UnrealToGodot/*.*" ), *LatestUnrealVersion ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTG_Release/UnrealToGodot_5.6.1.zip C:/UnrealProjects/Test5_6/Plugins/UnrealToGodot/*.*" ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTG_Release/UnrealToGodot_5.5.4.zip C:/UnrealProjects/Test5_5/Plugins/UnrealToGodot/*.*" ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTG_Release/UnrealToGodot_5.4.4.zip C:/UnrealProjects/Test5_4/Plugins/UnrealToGodot/*.*" ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTG_Release/UnrealToGodot_5.3.2.zip C:/UnrealProjects/Test5_3/Plugins/UnrealToGodot/*.*" ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTG_Release/UnrealToGodot_5.2.1.zip C:/UnrealProjects/Test5_2/Plugins/UnrealToGodot/*.*" ) );
		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTG_Release/UnrealToGodot_5.1.1.zip C:/UnrealProjects/Test5_1/Plugins/UnrealToGodot/*.*" ) );

		ExecuteCommand( TEXT( "C:/Program Files/WinRAR/WinRAR.exe" ),
			TEXT( "a -r -ep1 C:/MyProjects/Unreal/UTG_Release/UnrealToGodot_With_Binaries.zip C:/MyProjects/Unreal/UTG_Release/*.*" ) );
	}
#endif	
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE( FUnrealToXModule, UnrealToUnity )