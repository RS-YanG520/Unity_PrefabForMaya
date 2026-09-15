// Copyright Ciprian Stanciu 2024
#pragma once

#include <string>
#include <vector>
#include "CoreMinimal.h"
#include "Engine/Scene.h"
#include "BaseScene.generated.h"

class ULightComponent;
class UAnimSequence;

enum ComponentType
{
	CT_UNKNOWN = 0,
	CT_TRANSFORM,
	CT_MESHRENDERER,
	CT_SKINNEDMESHRENDERER,
	CT_MESHFILTER,	
	CT_LIGHT,
	CT_LODGROUP,
	CT_MESHCOLLIDER,
	CT_BOXCOLLIDER,
	CT_SPHERECOLLIDER,
	CT_CAPSULECOLLIDER,
	CT_REFLECTIONPROBE,
	CT_DECAL,
	CT_ANIMATION,
	CT_ADDITIONAL_LIGHT_DATA,
	CT_ADDITIONAL_CAMERA_DATA,
	CT_TREEINSTANCES
};
class UnityComponent
{
public :
	UnityComponent();
	virtual ~UnityComponent() {};
	uint64 ID = 0;
	ComponentType Type = CT_UNKNOWN;
	std::string Flag;
	bool Enabled = true;
	std::string Name;
	class GameObject *Owner = nullptr;

	static uint64 Counter;
	virtual std::string GetSceneData();
	static uint64 GetUniqueID();
};

class TransformComponent : public UnityComponent
{
public:
	TransformComponent();
	void Reset();
	void Set( FVector Position, FQuat Quat, FVector Scale );
	void Copy( TransformComponent& Other );

	float LocalPosition[ 3 ];
	float LocalRotation[ 4 ];//Quat ?
	float LocalScale[ 3 ];

	std::string GetChildrenText();
	virtual std::string GetSceneData();
};
class UnityMaterial;
class Renderer : public UnityComponent
{
public:
	void AddMaterial( UnityMaterial* NewMat, bool HasDuplicateMaterials );
	std::vector<class UnityMaterial*> Materials;
	bool CastShadows = true;
};
class MeshRenderer : public Renderer
{
public:
	MeshRenderer()
	{
		Type = CT_MESHRENDERER;
	}
	
	virtual std::string GetSceneData();
};

enum TextureSemantic
{
	TS_Unknown,
	TS_BaseMap,
	TS_BumpMap,
	TS_DetailAlbedoMap,
	TS_DetailMask,	
	TS_DetailNormalMap,	
	TS_EmissionMap,	
	TS_MainTex,
	TS_MetallicGlossMap,
	TS_OcclusionMap,
	TS_ParallaxMap,
	TS_SpecGlossMap,

	//GodotOnly
	TS_OpacityMap,

	TS_MaxSemantics
};
enum UnityTextureShape
{
	UTS_2D,
	UTS_Cube,
	UTS_2DArray,
	UTS_3D,
};
class UnityTexture
{
public:
	UnityTexture( UTexture* Source );
	void GenerateGUID();

	std::string File;
	std::string GUID;
	UnityTextureShape Shape = UTS_2D;
	//int SpecificIndex = -1;
	TextureSemantic Semantic = TS_Unknown;
	bool IsVirtual = false;
	bool IsReadable = false;
	int Compression = 1;
	int MaxTextureSize = 0;

	//Godot
	std::string ResourcePath;
	std::string LocalGUID;

	//Unreal
	UTexture* SourceTexture = nullptr;
};
enum UShaderType
{
	UST_UNKNOWN,

	UST_VERTEX,
	UST_FRAGMENT
};
enum ShaderParameterType
{
	SPT_FLOAT,
	SPT_VECTOR,
	SPT_TEXTURE,
	SPT_BOOL,
};
class ShaderParameter
{
public:
	std::string Name;
	ShaderParameterType Type;

	FVector4 DefaultVector;
	float DefaultFloat;
	UnityTexture* DefaultTexture = nullptr;
	bool DefaultBool = false;

	UShaderType ShaderType = UST_UNKNOWN;
};
class UnityShader
{
public:
	std::string Name;
	std::string FileName;
	std::string GUID;
	std::string Contents;
	FString UnrealShaderSource;
	std::vector< ShaderParameter > Parameters;
	void GenerateGUID();	

	//Godot
	std::string ResourcePath;
	std::string LocalGUID;
};
class UnityMaterial
{
public:
	UnityMaterial();

	void GenerateGUID();

	std::string Name;
	std::string File;
	std::string GUID;
	bool IsDecalMaterial = false;

	UnityShader* Shader = nullptr;
	std::vector< UnityTexture* > Textures;
	std::vector< ShaderParameter > Parameters;
	int DecalDrawOrder = 0;
	float MetallicValue = 0.0f;
	float SmoothnessMultiplier = 0.5f;
	int HasOpacityMask = 0;
	FLinearColor BaseColorMultiplier = FLinearColor(1,1,1,1);
	FLinearColor EmissionColor = FLinearColor( 0, 0, 0, 1 );
	bool EnableEmission = false;
	float CullMode = 0;	
	
	UnityTexture* GetTexture( TextureSemantic Semantic );

	//Godot
	std::string ResourcePath;
};

struct MeshSection
{
	//uint32 *Indices = nullptr;
	uint32 IndexOffset = 0;
	uint32 NumIndices = 0;
	uint32 MaterialIndex = 0;
};

class UnityMesh
{
public:
	void GenerateGUID();

	std::string Name;
	std::string File;
	std::string GUID;

	FVector*		Vertices = nullptr;
	FVector*		Normals = nullptr;
	FVector4*	Tangents = nullptr;
	FVector2D*	Texcoords = nullptr;
	FVector2D*	Texcoords1 = nullptr;
	FVector2D*	Texcoords2 = nullptr;
	FVector2D*	Texcoords3 = nullptr;
	FColor*		Colors = nullptr;
	uint32* AllIndices = nullptr;
	std::vector<MeshSection*> Sections;

	int NumVertices;
	int NumIndices;

	std::vector<UnityMaterial*> Materials;	
	bool HasConvexHulls = false;
	int ConvexHullMeshID = 0;

	//SkinnedMesh
	std::vector<std::string> BoneNames;

	//Godot
	std::string ResourcePath;
};

class MeshFilter : public UnityComponent
{
public:
	MeshFilter()
	{
		Type = CT_MESHFILTER;
	}
	UnityMesh *Mesh = nullptr;
	
	uint64_t SubMeshID = 4300000;
	virtual std::string GetSceneData();
};
class SkinnedMeshRenderer : public Renderer
{
public:
	SkinnedMeshRenderer()
	{
		Type = CT_SKINNEDMESHRENDERER;
	}

	virtual std::string GetSceneData();
	std::string GenerateBoneDesc();
	void FindBoneGameObjects();
	uint64 GetRootTransformID();
	UnityMesh* Mesh = nullptr;

	uint64_t SubMeshID = 4300000;
	std::vector<uint64> BoneGUIDs;
	FVector Center;
	FVector Extent;
	int LOD = 0;
	int TotalLods = 0;
};
class AnimationComponent : public UnityComponent
{
public:
	AnimationComponent()
	{
		Type = CT_ANIMATION;
	}

	virtual std::string GetSceneData();
	std::string AnimationGUID;
	UAnimSequence* AnimSequence = nullptr;
};

class LOD
{
public:
	float screenRelativeTransitionHeight = 0;
	float fadeTransitionWidth = 0;

	std::vector<Renderer*> renderers;
};
class LodGroup : public UnityComponent
{
public:
	LodGroup()
	{
		Type = CT_LODGROUP;
		LocalReferencePoint = FVector( 0, 0, 0 );
	}
	
	virtual std::string GetSceneData();
	std::string GetLODsString();

	std::vector<LOD*> Lods;
	float Size = 1.0f;
	FVector LocalReferencePoint;
};
class MeshCollider : public UnityComponent
{
public:
	MeshCollider()
	{
		Type = CT_MESHCOLLIDER;
	}

	UnityMesh* Mesh = nullptr;
	bool IsConvex = false;
	uint64_t SubMeshID = 0;
	virtual std::string GetSceneData();
};
class BoxCollider : public UnityComponent
{
public:
	BoxCollider()
	{
		Type = CT_BOXCOLLIDER;
		Center.Set( 0, 0, 0 );
	}

	FVector Center;
	FVector Size;

	virtual std::string GetSceneData();
};
class SphereCollider : public UnityComponent
{
public:
	SphereCollider()
	{
		Type = CT_SPHERECOLLIDER;
		Center.Set( 0, 0, 0 );
	}

	FVector Center;
	float Radius;

	virtual std::string GetSceneData();
};
class CapsuleCollider : public UnityComponent
{
public:
	CapsuleCollider()
	{
		Type = CT_CAPSULECOLLIDER;
		Center.Set( 0, 0, 0 );
	}

	FVector Center;
	float Radius;
	float Height;
	int Direction = 1;

	virtual std::string GetSceneData();
};

enum UnityLightType
{
	LT_SPOT,
	LT_DIRECTIONAL,
	LT_POINT,
	LT_AREA
};
enum UnityLightIntensityUnity
{
	LIU_LUMENS = 0,
	LIU_CANDELAS = 1,
	LIU_LUX = 2,
};

class LightComponent : public UnityComponent
{
public:
	LightComponent()
	{
		UnityComponent::Type = CT_LIGHT;
	}
	virtual std::string GetSceneData();
	
	UnityLightType Type;
	float Range;
	FColor Color;
	float Intensity;
	float SpotAngle = 30.0f;
	bool Shadows = true;
	float Width = 1;
	float Height = 1;

	//Godot
	float Temperature = 0.0f;
};

enum UnityReflectionProbeType
{
	RPB_REALTIME,
	RPB_CUSTOM,
};

class ReflectionProbeComponent : public UnityComponent
{
public:
	ReflectionProbeComponent()
	{
		UnityComponent::Type = CT_REFLECTIONPROBE;
	}
	virtual std::string GetSceneData();
	UnityReflectionProbeType Type = RPB_REALTIME;
	FVector Size;
	float Intensity = 1.0f;
};
class DecalComponent : public UnityComponent
{
public:
	DecalComponent()
	{
		UnityComponent::Type = CT_DECAL;
	}
	virtual std::string GetSceneData();
	UnityMaterial* Material = nullptr;
	FVector Size;
	int DrawOrder = 0;
};

class AdditionalLightData : public UnityComponent
{
public:
	AdditionalLightData()
	{
		UnityComponent::Type = CT_ADDITIONAL_LIGHT_DATA;
	}
	virtual std::string GetSceneData();

	LightComponent* Light = nullptr;
	UnityLightIntensityUnity Unit = LIU_LUX;
};
class AdditionalCameraData : public UnityComponent
{
public:
	AdditionalCameraData()
	{
		UnityComponent::Type = CT_ADDITIONAL_CAMERA_DATA;
	}
	virtual std::string GetSceneData();

	UnityComponent* Camera = nullptr;
};

class TreeInstancesComponent : public UnityComponent
{
public:
	TreeInstancesComponent()
	{
		UnityComponent::Type = CT_TREEINSTANCES;
	}
	virtual std::string GetSceneData();
	
	class TreeInstance
	{
	public:
		FVector Position;
		float widthScale;
		float heightScale;
		float rotation;
		GameObject* Prefab = nullptr;
		int FoliageID = -1;
	};
	class GrassMesh
	{
	public:
		GameObject* Prefab = nullptr;
		std::string GrassTexture;
		int FoliageID = -1;
	};
	std::vector< TreeInstance* > TreeInstances;
	std::vector< GrassMesh* > GrassMeshes;

	std::string HeightMapTexture;
	float TerrainHeight = 0;
};
class ExporterScene;
class GameObject
{
public:
	GameObject( );

	UnityComponent* GetComponent( ComponentType Type );
	UnityComponent* GetComponentInChildren( ComponentType Type );
	void		GetAllComponentsInChildren( std::vector< UnityComponent* > & AllComponents );
	UnityComponent* AddComponent( ComponentType Type );
	void AddChild( GameObject* GO );
	GameObject* Find( const char* pName );

	std::string GetGameObjectString( bool WritePrefab = false );
	std::string ToSceneData( bool WritePrefab = false );
	std::string GeneratePrefabInstance();

	std::vector< UnityComponent* > Components;
	std::vector< GameObject* > Children;
	GameObject *Parent = nullptr;
	std::string Name;
	uint64 ID;
	ExporterScene* OwnerScene = nullptr;

	bool CanBePrefab = false;
	std::string PrefabGUID;
	std::string PrefabName;
	GameObject* PrefabTemplate = nullptr;

	//Godot
	std::string PrefabResourcePath;
	std::string PrefabLocalID;
};

class ExporterScene
{
public:
	std::vector< GameObject* > GameObjects;
	std::vector< UnityMaterial* > Materials;
	std::vector< GameObject* > Prefabs;

	void Add( GameObject* GO );
	void Replace( GameObject* Target, GameObject* Replacement );
	GameObject* Find( const char* Name );
		
	void EvaluatePrefab( GameObject* GO );
	void FixDuplicatePrefabNames();
	std::string GetPrefabName( GameObject* GO );
	void SetupFoliagePrefab( GameObject* FoliagePrefab, int FoliageID );
	bool UsePrefabs = true;
};


UENUM()
enum class RenderPipeline : uint8
{
	RP_BUILTIN UMETA( DisplayName = "Built In Render Pipeline" ),
	RP_URP UMETA( DisplayName = "Universal Render Pipeline" ),
	RP_HDRP UMETA( DisplayName = "HD Render Pipeline" ),
};
UENUM()
enum class SkinnedMeshOptionsEnum : uint8
{
	SMO_EXPORT_AS_STATIC UMETA( DisplayName = "Export As Static Mesh" ),
	SMO_EXPORT_AS_SKINNED UMETA( DisplayName = "Export As Skinned Mesh" ),
};
UENUM()
enum class NaniteOptionsEnum : uint8
{
	NANITE_COARSE_MESH UMETA( DisplayName = "Export Coarse Mesh" ),
	NANITE_ORIGINAL_MESH UMETA( DisplayName = "Export Original Mesh" ),
};
UENUM()
enum class TangentToWorldLocationType : uint8
{
	TTWL_KEEP_UNITY_VECTORS UMETA( DisplayName = "Keep Unity Vectors" ),
	TTWL_KEEP_UNITY_VECTORS_NO_WORLDNORMALCOPY UMETA( DisplayName = "Keep Unity Vectors + No WorldNormalCopy" ),
	TTWL_BEFORE_NORMAL_CALCULATION UMETA( DisplayName = "Flip YZ Before Normals" ),
	TTWL_AFTER_NORMAL_CALCULATION UMETA( DisplayName = "Flip YZ After Normals" )
};
UENUM()
enum class FoliageExportType : uint8
{
	FET_CREATE_GAMEOBJECTS UMETA( DisplayName = "Create GameObject Per Instance" ),
	FET_USE_TREEINSTANCES UMETA( DisplayName = "Tree Instances(requires extra step in Unity)" )
};
UENUM()
enum class EngineType : uint8
{
	DEFAULT UMETA( Hidden ),
	UNITY UMETA( DisplayName = "Unity" ),
	GODOT UMETA( DisplayName = "Godot" )
};
UENUM()
enum class UnityShadersType : uint8
{
	ST_CUSTOM_SHADERS UMETA( DisplayName = "CustomShaders(designed for accuracy)" ),
	ST_STANDARD_SHADERS UMETA( DisplayName = "Standard Shaders" )
};

UCLASS( config = EditorPerProjectUserSettings, MinimalAPI, BlueprintType )
class UBaseSettingsClass : public UObject
{
	GENERATED_BODY()
public:
	UBaseSettingsClass();
	void Initialize();

	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	FString OutputDirectory;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	SkinnedMeshOptionsEnum SkinnedMeshes;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	NaniteOptionsEnum NaniteOptions;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ExportVertexShader;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ExportBrushes;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ExportPhysics;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ExportTextures;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, config, Category = Parameters )
	bool ExportAllSounds;
};

float ToFloat( uint8 v );
std::string GenerateGUIDFromString( std::string str );