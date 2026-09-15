// Copyright Ciprian Stanciu 2024
#pragma once

#include "ExportToUnity.h"

#include "BaseScene.h"
#include "UTXModule.h"

#include "Runtime/Core/Public/GenericPlatform/GenericPlatformFile.h"
#include "Runtime/Core/Public/HAL/PlatformFileManager.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "ImageUtils.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"

#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"

#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialParameterCollection.h"

#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/Paths.h"

#include "Materials/MaterialInstanceDynamic.h"

//Landscape
#include "Runtime/Landscape/Classes/Landscape.h"
#include "Runtime/Landscape/Classes/LandscapeStreamingProxy.h"
#include "LandscapeDataAccess.h"
#include "ProceduralMeshComponent.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMeshActor.h"
#include "MeshDescription.h"
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION == 5
	#include "ProceduralMeshConversion.h"
#endif

class LandscapeMeshData
{
public:
	LandscapeMeshData()
	{
		MeshCenter = FVector( 0, 0, 0 );
	};
	~LandscapeMeshData()
	{
		if( MeshDescription )
			delete MeshDescription;
		ProceduralMesh = nullptr;
		StaticMesh = nullptr;
	}	
	
	ALandscapeProxy* Landscape = nullptr;
	FVector MeshCenter;
	FString MeshAssetName;
	UPROPERTY()
	UProceduralMeshComponent* ProceduralMesh = nullptr;
	UPROPERTY()
	UStaticMesh* StaticMesh = nullptr;
	FMeshDescription* MeshDescription = nullptr;
	TMap<UMaterialInterface*, FPolygonGroupID> UniqueMaterials;	
};

class FLandscapeTools
{
public:
	FLandscapeTools()
	{
		MeshesGenerated = 0;
	}
	void Process( UWorld* world );
	void GetDataFromlandscape( ALandscapeProxy* Landscape, bool Winding );	
	UProceduralMeshComponent* CreateNewProceduralComponent( UObject* Outer );
	static UStaticMesh* CreateStaticMeshFromProceduralMesh( FString UserPackageName, LandscapeMeshData* MeshData );
	
	void FinalizeMesh( LandscapeMeshData* MeshData );	

	UMaterialInterface* LandscapeMaterial = nullptr;
	ALandscapeProxy* Landscape = nullptr;
	bool Winding;
	
	FTransform LandscapeTransform;

	TArray<FVector> InitialVertices;
	TArray<FVector> normals;
	TArray<FVector2D> UV0;
	TArray<FVector2D> UV1;
	TArray<FProcMeshTangent> tangents;
	
	
	UWorld* world = nullptr;
	int MeshesGenerated;
	
	TArray< LandscapeMeshData*> Patches;
};

extern FLandscapeTools LandscapeTools;
UTexture2D* CreateTexture( int X, int Y, EPixelFormat PixelFormat, ETextureSourceFormat SourceFormat, void* PixelData, FName Name, UObject* Outer );
FName ToFName( FString Str );