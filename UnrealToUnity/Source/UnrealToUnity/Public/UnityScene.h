// Copyright Ciprian Stanciu 2024
#pragma once

#include <string>
#include <vector>
#include "CoreMinimal.h"
#include "Engine/Scene.h"
#include "BaseScene.h"

std::string GenerateMeshMetaForUnity( UnityMesh* M, bool IsFBX, int TotalLods );
std::string GenerateMeshMeta( UnityMesh* M, bool IsFBX, int TotalLods );
int64_t LoadFile( const char *File, uint8** data );
std::string GenerateMaterialMeta( std::string GUID );
std::string GenerateShaderMeta( std::string GUID );
std::string GenerateTextureMeta( UnityTexture* Tex, bool IsNormalMap, int sRGB, UnityTextureShape Shape, int TileX, int TileY, int TextureCompression );
std::string  GeneratePrefabMeta( std::string GUID );
UnityLightIntensityUnity GetUnityLightIntensity( ELightUnits Unit );