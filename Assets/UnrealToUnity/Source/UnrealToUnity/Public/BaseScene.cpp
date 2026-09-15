// Copyright Ciprian Stanciu 2024

#include "BaseScene.h"
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
#include <string.h>

#if PLATFORM_WINDOWS
#else
//#define snprintf sprintf
#define stricmp strcasecmp
#endif
#if WITH_EDITOR

extern TAutoConsoleVariable<int> CVarRenderPipeline;
extern std::string NullGUID;

UnityComponent::UnityComponent()
{
	ID = GetUniqueID();
}
std::string UnityComponent::GetSceneData()
{
	return "";
}
uint64 UnityComponent::GetUniqueID()
{
	uint64 ID = 1030971880 + ( Counter++);
	return ID;
}

uint64 UnityComponent::Counter = 0;

TransformComponent::TransformComponent()
{
	Type = CT_TRANSFORM;

	Reset();
}
void TransformComponent::Reset()
{
	memset( LocalPosition, 0, sizeof( LocalPosition ) );
	memset( LocalRotation, 0, sizeof( LocalRotation ) );
	memset( LocalScale, 0, sizeof( LocalScale ) );

	LocalRotation[ 3 ] = 1.0f;
	for( int i = 0; i < 3; i++ )
		LocalScale[ i ] = 1.0f;
}
void TransformComponent::Set( FVector Position, FQuat Quat, FVector Scale )
{
	LocalPosition[ 0 ] = Position.X;
	LocalPosition[ 1 ] = Position.Y;
	LocalPosition[ 2 ] = Position.Z;
	LocalRotation[ 0 ] = Quat.X;
	LocalRotation[ 1 ] = Quat.Y;
	LocalRotation[ 2 ] = Quat.Z;
	LocalRotation[ 3 ] = Quat.W;
	LocalScale[ 0 ] = Scale.X;
	LocalScale[ 1 ] = Scale.Y;
	LocalScale[ 2 ] = Scale.Z;
}
void TransformComponent::Copy( TransformComponent& Other )
{
	for( int i = 0; i < 4; i++ )
	{
		LocalRotation[ i ] = Other.LocalRotation[ i ];
		if( i < 3 )
		{
			LocalPosition[ i ] = Other.LocalPosition[ i ];
			LocalScale[ i ] = Other.LocalScale[ i ];
		}
	}
}
std::string TransformComponent::GetChildrenText()
{	
	if( Owner->Children.size() == 0 )
		return " []\n";
	else
	{
		std::string Ret = "\n";
		for( int i = 0; i < Owner->Children.size(); i++ )
		{
			auto Child = Owner->Children[ i ];
			TransformComponent* TC = ( TransformComponent*)Child->GetComponent( ComponentType::CT_TRANSFORM );
			char Line[ 256 ];
			//uint64_t ID = 0;
			snprintf( Line, sizeof( Line ), "  - {fileID: %lld}\n", TC->ID );
			Ret += Line;
		}

		return Ret;
	}
}
std::string TransformComponent::GetSceneData()
{
	return ExporterInterface->TransformComponent_GetSceneData( this );
}

std::string MeshRenderer::GetSceneData()
{
	return ExporterInterface->MeshRenderer_GetSceneData( this );
}
std::string SkinnedMeshRenderer::GenerateBoneDesc()
{
	std::string Ret = "";

	if( BoneGUIDs.size() > 0 )
	{
		Ret += "\n";
		for( int i = 0; i < BoneGUIDs.size(); i++ )
		{
			char Text[ 128 ] = "";
			snprintf( Text, sizeof( Text ), "  - {fileID: %lld}\n", BoneGUIDs[ i ] );
			Ret += Text;
		}
	}
	else
	{
		Ret += "[]\n";
	}

	return Ret;
}
void SkinnedMeshRenderer::FindBoneGameObjects()
{
				//Means we already found the IDs
	if( !Mesh || BoneGUIDs.size() > 0)
		return;
	
	for( int i = 0; i < Mesh->BoneNames.size(); i++ )
	{
		GameObject* GO = Owner->Parent->Find( Mesh->BoneNames[i].c_str());
		if( GO )
		{
			TransformComponent* TC = ( TransformComponent*)GO->GetComponent( CT_TRANSFORM );
			BoneGUIDs.push_back( TC->ID );
		}
		else
			BoneGUIDs.push_back( 0 );
	}
}
uint64 SkinnedMeshRenderer::GetRootTransformID()
{
	if( Mesh->BoneNames.size() == 0 )
		return 0;

	GameObject* GO = Owner->Parent->Find( Mesh->BoneNames[ 0 ].c_str() );
	if( GO )
	{
		TransformComponent* TC = (TransformComponent*)GO->GetComponent( CT_TRANSFORM );
		return TC->ID;
	}

	return 0;
}
std::string SkinnedMeshRenderer::GetSceneData()
{
	return ExporterInterface->SkinnedMeshRenderer_GetSceneData( this );
}
std::string AnimationComponent::GetSceneData()
{
	return ExporterInterface->AnimationComponent_GetSceneData( this );
}
void Renderer::AddMaterial( UnityMaterial* NewMat, bool HasDuplicateMaterials )
{
	//Only needs this when base mesh has duplicate materials that will get merged by unity
	//if( HasDuplicateMaterials )
	//{
	//	for( int i = 0; i < Materials.size(); i++ )
	//	{
	//		if( Materials[ i ] == NewMat )
	//		{
	//			//If it's a duplicate, don't add it
	//			return;
	//		}
	//	}
	//}
	Materials.push_back( NewMat );
}
std::string MeshFilter::GetSceneData()
{
	return ExporterInterface->MeshFilter_GetSceneData( this );
}
std::string LodGroup::GetLODsString()
{
	std::string RetStr;
	for( int i = 0; i < Lods.size(); i++ )
	{
		LOD* lod = Lods[ i ];

		std::string RenderersText;
		for( int l = 0; l < lod->renderers.size(); l++ )
		{
			char Local[ 1024 ];
			snprintf( Local, sizeof( Local ), "    - renderer: {fileID: %lld}\n", lod->renderers[l]->ID );
			RenderersText += Local;
		}

		char Text[ 1024 ];
		snprintf( Text, sizeof( Text ),
				   "  - screenRelativeHeight: %0.2f\n"
				   "    fadeTransitionWidth: %0.1f\n"
				   "    renderers:\n", lod->screenRelativeTransitionHeight, lod->fadeTransitionWidth );
		RetStr += Text;
		RetStr += RenderersText;
	}

	return RetStr;
}
std::string LodGroup::GetSceneData()
{
	return ExporterInterface->LodGroup_GetSceneData( this );
}
std::string MeshCollider::GetSceneData()
{
	return ExporterInterface->MeshCollider_GetSceneData( this );
}
std::string BoxCollider::GetSceneData()
{
	return ExporterInterface->BoxCollider_GetSceneData( this );
}
std::string SphereCollider::GetSceneData()
{
	return ExporterInterface->SphereCollider_GetSceneData( this );
}
std::string CapsuleCollider::GetSceneData()
{
	return ExporterInterface->CapsuleCollider_GetSceneData( this );
}
float ToFloat( uint8 v )
{
	float r = float(v) / 255.0f;
	return r;
}
std::string LightComponent::GetSceneData()
{
	return ExporterInterface->LightComponent_GetSceneData( this );
}
std::string ReflectionProbeComponent::GetSceneData()
{
	return ExporterInterface->ReflectionProbeComponent_GetSceneData( this );
}
std::string DecalComponent::GetSceneData()
{
	return ExporterInterface->DecalComponent_GetSceneData( this );
}
std::string AdditionalCameraData::GetSceneData()
{
	return ExporterInterface->AdditionalCameraData_GetSceneData( this );
}
std::string AdditionalLightData::GetSceneData()
{
	return ExporterInterface->AdditionalLightData_GetSceneData( this );
}

std::string TreeInstancesComponent::GetSceneData()
{
	return ExporterInterface->TreeInstancesComponent_GetSceneData( this );
}
UnityTexture::UnityTexture( UTexture* Source )
{
	SourceTexture = Source;
}
void UnityTexture::GenerateGUID()
{
	std::size_t hash = std::hash<std::string>{}( File );
	GUID = ExporterInterface->GetGUIDFromSizeT( hash );
}
void UnityMesh::GenerateGUID()
{
	std::size_t hash = std::hash<std::string>{}( File );
	GUID = ExporterInterface->GetGUIDFromSizeT( hash );
}
UnityMaterial::UnityMaterial()
{
	Shader = new UnityShader();
}
void UnityShader::GenerateGUID()
{
	std::size_t hash = std::hash<std::string>{}( FileName );
	GUID = ExporterInterface->GetGUIDFromSizeT( hash );
}
void UnityMaterial::GenerateGUID()
{
	std::string Input = File + Shader->GUID;
	std::size_t hash = std::hash<std::string>{}( Input );
	GUID = ExporterInterface->GetGUIDFromSizeT( hash );
}
std::string GenerateGUIDFromString( std::string str )
{
	std::size_t hash = std::hash<std::string>{}( str );
	std::string Ret = ExporterInterface->GetGUIDFromSizeT( hash );
	return Ret;
}
GameObject::GameObject( )
{
	ID = UnityComponent::GetUniqueID();

	TransformComponent *Trans = new TransformComponent;
	Trans->Owner = this;
	Components.push_back( Trans );
}
UnityComponent* GameObject::AddComponent( ComponentType Type )
{
	if( Type == CT_MESHFILTER )
	{
		MeshFilter *MF = new MeshFilter;
		MF->Owner = this;
		Components.push_back( MF );
		return MF;
	}
	else if( Type == CT_MESHRENDERER )
	{
		MeshRenderer *MR = new MeshRenderer;
		MR->Owner = this;
		Components.push_back( MR );
		return MR;
	}
	else if( Type == CT_SKINNEDMESHRENDERER )
	{
		SkinnedMeshRenderer* SMR = new SkinnedMeshRenderer;
		SMR->Owner = this;
		Components.push_back( SMR );
		return SMR;
	}	
	else if( Type == CT_LIGHT )
	{
		LightComponent* NewLight = new LightComponent;
		NewLight->Owner = this;
		Components.push_back( NewLight );
		return NewLight;
	}
	else if( Type == CT_LODGROUP )
	{
		LodGroup* NewLodGroup = new LodGroup;
		NewLodGroup->Owner = this;
		Components.push_back( NewLodGroup );
		return NewLodGroup;
	}
	else if( Type == CT_MESHCOLLIDER )
	{
		MeshCollider* NewMeshCollider = new MeshCollider;
		NewMeshCollider->Owner = this;
		Components.push_back( NewMeshCollider );
		return NewMeshCollider;
	}
	else if( Type == CT_BOXCOLLIDER )
	{
		BoxCollider* NewBoxCollider = new BoxCollider;
		NewBoxCollider->Owner = this;
		Components.push_back( NewBoxCollider );
		return NewBoxCollider;
	}
	else if( Type == CT_SPHERECOLLIDER )
	{
		SphereCollider* NewSphereCollider = new SphereCollider;
		NewSphereCollider->Owner = this;
		Components.push_back( NewSphereCollider );
		return NewSphereCollider;
	}
	else if( Type == CT_CAPSULECOLLIDER )
	{
		CapsuleCollider* NewCapsuleCollider = new CapsuleCollider;
		NewCapsuleCollider->Owner = this;
		Components.push_back( NewCapsuleCollider );
		return NewCapsuleCollider;
	}
	else if( Type == CT_REFLECTIONPROBE )
	{
		ReflectionProbeComponent* NewReflectionProbeComponent = new ReflectionProbeComponent;
		NewReflectionProbeComponent->Owner = this;
		Components.push_back( NewReflectionProbeComponent );
		return NewReflectionProbeComponent;
	}
	else if( Type == CT_DECAL )
	{
		DecalComponent* NewDecalComponent = new DecalComponent;
		NewDecalComponent->Owner = this;
		Components.push_back( NewDecalComponent );
		return NewDecalComponent;
	}
	else if( Type == CT_ANIMATION )
	{
		AnimationComponent* NewAnimationComponent = new AnimationComponent;
		NewAnimationComponent->Owner = this;
		Components.push_back( NewAnimationComponent );
		return NewAnimationComponent;
	}
	else if( Type == CT_ADDITIONAL_LIGHT_DATA )
	{
		AdditionalLightData* NewAdditionalLightData = new AdditionalLightData;
		NewAdditionalLightData->Owner = this;
		Components.push_back( NewAdditionalLightData );
		return NewAdditionalLightData;
	}
	else if( Type == CT_ADDITIONAL_CAMERA_DATA )
	{
		AdditionalCameraData* NewAdditionalCameraData = new AdditionalCameraData;
		NewAdditionalCameraData->Owner = this;
		Components.push_back( NewAdditionalCameraData );
		return NewAdditionalCameraData;
	}
	else if( Type == CT_TREEINSTANCES )
	{
		TreeInstancesComponent* NewTreeInstancesComponent = new TreeInstancesComponent;
		NewTreeInstancesComponent->Owner = this;
		Components.push_back( NewTreeInstancesComponent );
		return NewTreeInstancesComponent;
	}

	return nullptr;
}
void GameObject::AddChild( GameObject* GO )
{
	Children.push_back( GO );
	GO->Parent = this;
}
UnityComponent* GameObject::GetComponent( ComponentType Type )
{
	for ( int i = 0; i < Components.size(); i++ )
	{
		if ( Components[ i ]->Type == Type )
			return Components[ i ];
	}

	return nullptr;
}
UnityComponent* GameObject::GetComponentInChildren( ComponentType Type )
{
	for( int i = 0; i < Components.size(); i++ )
	{
		if( Components[ i ]->Type == Type )
			return Components[ i ];
	}

	for( int i = 0; i < Children.size(); i++ )
	{
		UnityComponent* Comp = Children[ i ]->GetComponentInChildren( Type );
		if( Comp )
			return Comp;
	}

	return nullptr;
}
GameObject* GameObject::Find( const char* pName )
{
	if( this->Name.compare( pName ) == 0 )
		return this;

	for( int i = 0; i < Children.size(); i++ )
	{
		GameObject* GO = Children[ i ]->Find( pName );
		if ( GO )
			return GO;
	}

	return nullptr;
}
void GameObject::GetAllComponentsInChildren( std::vector< UnityComponent* >& AllComponents )
{
	for( int i = 0; i < Components.size(); i++ )
	{
		AllComponents.push_back( Components[ i ] );
	}

	for( int i = 0; i < Children.size(); i++ )
	{
		Children[ i ]->GetAllComponentsInChildren( AllComponents );
	}
}
std::string GameObject::ToSceneData( bool WritePrefab )
{
	std::string RetStr;

	std::string MainStr = GetGameObjectString( WritePrefab );
	RetStr += MainStr;

	for( int i = 0; i < Components.size(); i++ )
	{
		auto Comp = Components[ i ];
		if( WritePrefab && Comp->Type == ComponentType::CT_TRANSFORM )
		{
			TransformComponent* Trans = (TransformComponent*)Comp;
			TransformComponent OriginalTransform;
			OriginalTransform = *Trans;
			Trans->Reset();
			RetStr += Comp->GetSceneData();
			*Trans = OriginalTransform;
		}
		else
			RetStr += Comp->GetSceneData();
	}

	for( int i = 0; i < Children.size(); i++ )
	{
		auto Child = Children[ i ];
		std::string ChildStr = Child->ToSceneData();
		RetStr += ChildStr;
	}

	return RetStr;
}
std::string GameObject::GeneratePrefabInstance()
{
	if( !PrefabTemplate )
		return "";
	
	TransformComponent* TemplateTransform = ( TransformComponent*)PrefabTemplate->GetComponent( CT_TRANSFORM );
	TransformComponent* InstanceTransform = ( TransformComponent*)GetComponent( CT_TRANSFORM );
	uint64 PrefabTransformID = 0;
	if( TemplateTransform )
		PrefabTransformID = TemplateTransform->ID;

	std::string PrefabID = PrefabTemplate->PrefabGUID;
	char Text[ 4096 ];
	snprintf( Text, sizeof( Text ),
		"--- !u!1001 &%lld\n"
		"PrefabInstance:\n"
		"  m_ObjectHideFlags: 0\n"
		"  serializedVersion: 2\n"
		"  m_Modification:\n"
		"    m_TransformParent: {fileID: 0}\n"
		"    m_Modifications:\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_Name\n"
		"      value: %s\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalPosition.x\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalPosition.y\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalPosition.z\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalRotation.x\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalRotation.y\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalRotation.z\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalRotation.w\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		//"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		//"      propertyPath: m_LocalEulerAnglesHint.x\n"
		//"      value: %f\n"
		//"      objectReference: {fileID: 0}\n"
		//"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		//"      propertyPath: m_LocalEulerAnglesHint.y\n"
		//"      value:%f\n"
		//"      objectReference: {fileID: 0}\n"
		//"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		//"      propertyPath: m_LocalEulerAnglesHint.z\n"
		//"      value: %f\n"
		//"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalScale.x\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalScale.y\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalScale.z\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    m_RemovedComponents: []\n"
		"  m_SourcePrefab: {fileID: 100100000, guid: %s, type: 3}\n", ID, 
			   PrefabTemplate->ID, PrefabID.c_str(), this->Name.c_str(),

			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalPosition[ 0 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalPosition[ 1 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalPosition[ 2 ],

			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalRotation[ 0 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalRotation[ 1 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalRotation[ 2 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalRotation[ 3 ],

			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalScale[ 0 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalScale[ 1 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalScale[ 2 ],

			   PrefabID.c_str()
	);

	return Text;
}
std::string GameObject::GetGameObjectString( bool WritePrefab )
{
	int serializedVersion = 6;
	
	char Text[ 1024 ];
	snprintf( Text, sizeof( Text ),
		"--- !u!1 &%lld\n"
		"GameObject:\n"
		"  m_ObjectHideFlags: 0\n"
		"  m_CorrespondingSourceObject: {fileID: 0}\n"
		"  m_PrefabInstance: {fileID: 0}\n"
		"  m_PrefabAsset: {fileID: 0}\n"
		"  serializedVersion: %d\n"
		"  m_Component:\n",
			 this->ID, serializedVersion );

	std::string Str = Text;

	for ( int i = 0; i < Components.size(); i++ )
	{
		auto Comp = Components[ i ];
		char Line[ 128 ];
		if ( serializedVersion >= 5 )
		{
			snprintf( Line, sizeof( Line ), "  - component: {fileID: %lld}\n", Comp->ID );
		}		
		
		Str += Line;
	}
	
	const char* TargetName = Name.c_str();
	if( WritePrefab )
		TargetName = PrefabName.c_str();
	snprintf( Text, sizeof( Text ),
	"  m_Layer: 0\n"
	"  m_Name: %s\n"
	"  m_TagString: Untagged\n"
	"  m_Icon: {fileID: 0}\n"
	"  m_NavMeshLayer: 0\n"
	"  m_StaticEditorFlags: 0\n"
	"  m_IsActive: 1\n",
			   TargetName );

	Str += Text;

	return Str;
}
void ExporterScene::Add( GameObject* GO )
{
	for( int i = 0; i < GameObjects.size(); i++ )
	{
		if( GameObjects[ i ] == GO )
			return;
	}

	GameObjects.push_back( GO );
	GO->OwnerScene = this;
}
void ExporterScene::Replace( GameObject* Target, GameObject* Replacement )
{
	for( int i = 0; i < GameObjects.size(); i++ )
	{
		GameObject* GO = GameObjects[ i ];
		if( GO == Target )
		{
			GameObjects[ i ] = Replacement;
			break;
		}
	}
}
GameObject* ExporterScene::Find( const char* Name )
{
	for( int i = 0; i < GameObjects.size(); i++ )
	{
		GameObject* GO = GameObjects[ i ];
		if( !GO )
			continue;

		if( GO->Name.compare( Name ) == 0 )
			return GO;
	}

	return nullptr;
}
std::string ExporterScene::GetPrefabName( GameObject* GO )
{
	std::vector< UnityComponent* > AllComponents;
	GO->GetAllComponentsInChildren( AllComponents);

	for( int c = 0; c < AllComponents.size(); c++ )
	{
		if( AllComponents[ c ]->Type == CT_MESHFILTER )
		{
			MeshFilter* MF = (MeshFilter*)AllComponents[ c ];
			return MF->Mesh->Name;
		}
		else if( AllComponents[ c ]->Type == CT_SKINNEDMESHRENDERER )
		{
			SkinnedMeshRenderer* SMR = (SkinnedMeshRenderer*)AllComponents[ c ];
			return SMR->Mesh->Name;
		}
	}

	return "";
}
void ExporterScene::FixDuplicatePrefabNames()
{
	if( Prefabs.size() == 0 )
		return;
	for( int i = 0; i < Prefabs.size() - 1; i++ )
	{
		auto PrefabA = Prefabs[ i ];
		int Instances = 1;
		for( int u = i + 1; u < Prefabs.size(); u++ )
		{
			GameObject* PrefabB = Prefabs[ u ];
			//compare disregarding case
			if( stricmp( PrefabA->PrefabName.c_str(), PrefabB->PrefabName.c_str()) == 0)
			{
				char Text[ 32 ];
				snprintf( Text, sizeof( Text ), "_%d", Instances );
				PrefabB->PrefabName += Text;
				Instances++;
			}
		}
	}
}
void ExporterScene::EvaluatePrefab( GameObject* GO )
{
	if( !UsePrefabs )
		return;
	bool MustBePrefab = false;
	int FoliageID = -1;
	if( GO->Name.find( "UTU_FoliageID" ) != -1 )
	{
		sscanf( GO->Name.c_str(), "UTU_FoliageID %d", &FoliageID );
		if( FoliageID != -1 )
			MustBePrefab = true;
	}

	bool IsPrefabInstance = false;
	if( !MustBePrefab )
	{
		std::vector< UnityComponent* >* AllComponentsA = new std::vector< UnityComponent* >;
		std::vector< UnityComponent* >* AllComponentsB = new std::vector< UnityComponent* >;
		for( int i = 0; i < Prefabs.size(); i++ )
		{
			AllComponentsA->clear();
			AllComponentsB->clear();
			GameObject* Prefab = Prefabs[ i ];
			Prefab->GetAllComponentsInChildren( *AllComponentsA );
			GO->GetAllComponentsInChildren( *AllComponentsB );

			if( AllComponentsA->size() != AllComponentsB->size() )
				continue;

			bool Identical = true;
			for( int c = 0; c < AllComponentsA->size(); c++ )
			{
				UnityComponent* CompA = ( *AllComponentsA )[ c ];
				UnityComponent* CompB = ( *AllComponentsB )[ c ];
				if( CompA->Type != CompB->Type )
				{
					Identical = false;
					break;
				}
				if( CompA->Type == CT_MESHFILTER )
				{
					MeshFilter* MF_A = (MeshFilter*)CompA;
					MeshFilter* MF_B = (MeshFilter*)CompB;
					if( MF_A->Mesh != MF_B->Mesh )
					{
						Identical = false;
						break;
					}
				}
				if( CompA->Type == CT_MESHRENDERER )
				{
					MeshRenderer* MR_A = (MeshRenderer*)CompA;
					MeshRenderer* MR_B = (MeshRenderer*)CompB;
					if( MR_A->Materials.size() != MR_B->Materials.size() )
					{
						Identical = false;
						break;
					}
					for( int m = 0; m < MR_A->Materials.size(); m++ )
					{
						if( MR_A->Materials[ m ] != MR_B->Materials[ m ] )
						{
							Identical = false;
							break;
						}
					}
				}
				if( CompA->Type == CT_SKINNEDMESHRENDERER )
				{
					SkinnedMeshRenderer* SMR_A = (SkinnedMeshRenderer*)CompA;
					SkinnedMeshRenderer* SMR_B = (SkinnedMeshRenderer*)CompB;
					if( SMR_A->Mesh != SMR_B->Mesh )
					{
						Identical = false;
						break;
					}
					if( SMR_A->Materials.size() != SMR_B->Materials.size() )
					{
						Identical = false;
						break;
					}
					for( int m = 0; m < SMR_A->Materials.size(); m++ )
					{
						if( SMR_A->Materials[ m ] != SMR_B->Materials[ m ] )
						{
							Identical = false;
							break;
						}
					}
				}
			}

			if( !Identical )
				continue;

			IsPrefabInstance = true;
			GO->PrefabTemplate = Prefab;
		}
		delete AllComponentsA;
		delete AllComponentsB;
	}

	if( !IsPrefabInstance )
	{
		if( GO->CanBePrefab )
		{
			//Give the prefab the name of the mesh
			GO->PrefabName = GetPrefabName( GO );
			GameObject* PrefabInstance = nullptr;
			
			if( FoliageID == -1 )
			{
				PrefabInstance = new GameObject();
				PrefabInstance->PrefabTemplate = GO;
				PrefabInstance->Name = GO->Name;
			}
			else
			{
				SetupFoliagePrefab( GO, FoliageID );
			}
			TransformComponent* TemplateTransform = (TransformComponent*)GO->GetComponent( CT_TRANSFORM );
			if( FoliageID == -1 )
			{
				TransformComponent* InstanceTransform = (TransformComponent*)PrefabInstance->GetComponent( CT_TRANSFORM );
				InstanceTransform->Copy( *TemplateTransform );
				Replace( GO, PrefabInstance );
			}
			else//remove FoliagePrefabs as instances
				Replace( GO, nullptr );

			Prefabs.push_back( GO );
		}
	}
}
extern GameObject* GameObjectForTrees;
void ExporterScene::SetupFoliagePrefab( GameObject* FoliagePrefab, int FoliageID )
{
	if( !GameObjectForTrees )
		return;

	TreeInstancesComponent* TIC = ( TreeInstancesComponent*)GameObjectForTrees->GetComponent( CT_TREEINSTANCES );

	for( int i = 0; i < TIC->TreeInstances.size(); i++ )
	{
		TreeInstancesComponent::TreeInstance* Instance = TIC->TreeInstances[ i ];
		if( Instance->FoliageID == FoliageID )
		{
			Instance->Prefab = FoliagePrefab;
		}
	}
	for( int i = 0; i < TIC->GrassMeshes.size(); i++ )
	{
		TreeInstancesComponent::GrassMesh* Mesh = TIC->GrassMeshes[ i ];
		if( Mesh->FoliageID == FoliageID )
		{
			Mesh->Prefab = FoliagePrefab;
		}
	}
}

UnityTexture* UnityMaterial::GetTexture( TextureSemantic Semantic )
{
	for( int i = 0; i < Textures.size(); i++ )
	{
		if( Textures[ i ]->Semantic == Semantic )
		{
			return Textures[ i ];
		}
	}

	return nullptr;
}
FPluginDescriptor IUnrealExporterInterface::GetPluginDescriptor()
{
	FString PluginName = GetPluginName();

	TSharedPtr<IPlugin> ThisPlugin = IPluginManager::Get().FindPlugin( PluginName );
	if (ThisPlugin.Get())
	{
		const FPluginDescriptor& Descriptor = ThisPlugin->GetDescriptor();
		return Descriptor;
	}
	FPluginDescriptor Blank;
	return Blank;
}
FString IUnrealExporterInterface::GetPluginVersion()
{
	const FPluginDescriptor& Descriptor = GetPluginDescriptor();
	return Descriptor.VersionName;
}
#endif