# -*- coding: utf-8 -*-
"""手写 Unity YAML 序列化。

对标 UnrealToUnity：在 Maya 侧直接生成 Unity 的文本资产文件
（.meta / .mat / .prefab / .unity）。文件格式以 Unity 2021.3 Built-in 为准，
fileID 采用固定约定（见 constants.py），GUID 用确定性 md5。

关键机制：
  - FBX 的 .meta 通过 fileIDToRecycleName 把内部 mesh 的 fileID 固定为 4300000，
    于是 .prefab 里就能用 {fileID: 4300000, guid: <fbx guid>, type: 3} 引用它。
"""
from __future__ import print_function

from constants import (
    FILEID_ROOT_GAMEOBJECT, FILEID_ROOT_TRANSFORM, FILEID_MESH_RENDERER,
    FILEID_MESH_FILTER, FILEID_MESH, FILEID_MATERIAL, FILEID_TEXTURE,
    FILEID_PREFAB_SOURCE, STANDARD_SHADER_GUID,
)


def _f(v):
    """格式化数值：整数的 float 输出为整数形式，其余用 repr。"""
    if isinstance(v, float):
        if v == int(v):
            return str(int(v))
        return repr(v)
    return str(v)


def _vec4(x, y, z, w):
    return "{x: %s, y: %s, z: %s, w: %s}" % (_f(x), _f(y), _f(z), _f(w))


def _vec3(x, y, z):
    return "{x: %s, y: %s, z: %s}" % (_f(x), _f(y), _f(z))


# =====================================================================
# .meta 文件
# =====================================================================

def fbx_meta(guid, mesh_name):
    """FBX 的 .meta（ModelImporter）。fileIDToRecycleName 固定内部 fileID。"""
    return (
        "fileFormatVersion: 2\n"
        "guid: %s\n"
        "ModelImporter:\n"
        "  serializedVersion: 23\n"
        "  fileIDToRecycleName:\n"
        "    100000: //RootNode\n"
        "    400000: //RootNode\n"
        "    2300000: //RootNode\n"
        "    3300000: //RootNode\n"
        "    4300000: %s\n"
        "    4300002: %s\n"
        "  externalObjects: {}\n"
        "  materials:\n"
        "    importMaterials: 0\n"
        "    materialName: 0\n"
        "    materialSearch: 1\n"
        "    materialLocation: 1\n"
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
        "    indexFormat: 0\n"
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
        "    hasTranslationDoF: 0\n"
        "    hasExtraRoot: 0\n"
        "    skeletonHasParents: 1\n"
        "  lastHumanDescriptionAvatarSource: {instanceID: 0}\n"
        "  animationType: 0\n"
        "  humanoidOversampling: 1\n"
        "  additionalBone: 0\n"
        "  userData: \n"
        "  assetBundleName: \n"
        "  assetBundleVariant: \n"
    ) % (guid, mesh_name, mesh_name)


def texture_meta(guid, srgb=True, normal=False):
    """贴图的 .meta（TextureImporter）。

    srgb=False 用于金属/粗糙度等线性数据贴图。
    normal=True 时标记为 Normal map（textureType: 1）。
    """
    if normal:
        srgb = False
    srgb_flag = 1 if srgb else 0
    linear_flag = 0 if srgb else 1
    texture_type = 1 if normal else 0
    return (
        "fileFormatVersion: 2\n"
        "guid: %s\n"
        "TextureImporter:\n"
        "  fileIDToRecycleName: {}\n"
        "  serializedVersion: 4\n"
        "  mipmaps:\n"
        "    mipMapMode: 0\n"
        "    enableMipMap: 1\n"
        "    sRGBTexture: %d\n"
        "    linearTexture: %d\n"
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
        "  isReadable: 1\n"
        "  grayScaleToAlpha: 0\n"
        "  generateCubemap: 6\n"
        "  cubemapConvolution: 0\n"
        "  seamlessCubemap: 0\n"
        "  textureFormat: 1\n"
        "  maxTextureSize: 2048\n"
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
        "  textureShape: 1\n"
        "  singleChannelComponent: 0\n"
        "  flipbookRows: 1\n"
        "  flipbookColumns: 1\n"
        "  maxTextureSizeSet: 0\n"
        "  compressionQualitySet: 0\n"
        "  textureFormatSet: 0\n"
        "  platformSettings:\n"
        "  - buildTarget: DefaultTexturePlatform\n"
        "    maxTextureSize: 2048\n"
        "    textureFormat: -1\n"
        "    textureCompression: 1\n"
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
        "  assetBundleVariant: \n"
    ) % (guid, srgb_flag, linear_flag, texture_type)


def material_meta(guid):
    """材质 .mat 的 .meta（NativeFormatImporter）。"""
    return (
        "fileFormatVersion: 2\n"
        "guid: %s\n"
        "NativeFormatImporter:\n"
        "  mainObjectFileID: 2100000\n"
        "  userData: \n"
        "  assetBundleName: \n"
        "  assetBundleVariant: \n"
    ) % guid


def prefab_meta(guid):
    """Prefab 的 .meta（PrefabImporter）。"""
    return (
        "fileFormatVersion: 2\n"
        "guid: %s\n"
        "PrefabImporter:\n"
        "  externalObjects: {}\n"
        "  userData: \n"
        "  assetBundleName: \n"
        "  assetBundleVariant: \n"
    ) % guid


# =====================================================================
# 材质 .mat（Built-in Standard shader）
# =====================================================================

def material_file(material):
    """生成 Unity 材质 .mat 内容（!u!21 Material）。"""
    def _tex_ref(tex):
        if tex:
            return "        m_Texture: {fileID: %d, guid: %s, type: 3}\n" % (
                FILEID_TEXTURE, tex.guid
            )
        return "        m_Texture: {fileID: 0}\n"

    main_tex = _tex_ref(material.albedo_texture)
    bump_tex = _tex_ref(getattr(material, "normal_texture", None))
    metal_tex = _tex_ref(getattr(material, "metallic_texture", None))

    keywords = []
    if getattr(material, "normal_texture", None):
        keywords.append("_NORMALMAP")
    if getattr(material, "metallic_texture", None):
        keywords.append("_METALLICGLOSSMAP")
    if keywords:
        kw_block = "  m_ValidKeywords:\n" + "".join("  - %s\n" % k for k in keywords)
    else:
        kw_block = "  m_ValidKeywords: []\n"

    r, g, b, a = material.color
    if material.albedo_texture:
        # 有贴图时，_Color 只作 tint（与 _MainTex 相乘），须为白色，否则会把贴图乘成黑色
        r, g, b, a = (1.0, 1.0, 1.0, 1.0)
    return (
        "%%YAML 1.1\n"
        "%%TAG !u! tag:unity3d.com,2011:\n"
        "--- !u!21 &%d\n"
        "Material:\n"
        "  serializedVersion: 8\n"
        "  m_ObjectHideFlags: 0\n"
        "  m_CorrespondingSourceObject: {fileID: 0}\n"
        "  m_PrefabInstance: {fileID: 0}\n"
        "  m_PrefabAsset: {fileID: 0}\n"
        "  m_Name: %s\n"
        "  m_Shader: {fileID: 46, guid: %s, type: 0}\n"
        "%s"
        "  m_InvalidKeywords: []\n"
        "  m_LightmapFlags: 4\n"
        "  m_EnableInstancingVariants: 0\n"
        "  m_DoubleSidedGI: 0\n"
        "  m_CustomRenderQueue: -1\n"
        "  stringTagMap: {}\n"
        "  disabledShaderPasses: []\n"
        "  m_SavedProperties:\n"
        "    serializedVersion: 3\n"
        "    m_TexEnvs:\n"
        "    - _MainTex:\n"
        "%s"
        "        m_Scale: {x: 1, y: 1}\n"
        "        m_Offset: {x: 0, y: 0}\n"
        "    - _BumpMap:\n"
        "%s"
        "        m_Scale: {x: 1, y: 1}\n"
        "        m_Offset: {x: 0, y: 0}\n"
        "    - _MetallicGlossMap:\n"
        "%s"
        "        m_Scale: {x: 1, y: 1}\n"
        "        m_Offset: {x: 0, y: 0}\n"
        "    m_Ints: []\n"
        "    m_Floats:\n"
        "    - _Glossiness: %s\n"
        "    - _Metallic: %s\n"
        "    - _BumpScale: 1\n"
        "    - _Parallax: 0.02\n"
        "    - _OcclusionStrength: 1\n"
        "    - _UVSec: 0\n"
        "    - _Mode: 0\n"
        "    - _SrcBlend: 1\n"
        "    - _DstBlend: 0\n"
        "    - _ZWrite: 1\n"
        "    - _Cutoff: 0.5\n"
        "    m_Colors:\n"
        "    - _Color: {r: %s, g: %s, b: %s, a: %s}\n"
        "    - _EmissionColor: {r: 0, g: 0, b: 0, a: 1}\n"
        "  m_BuildTextureStacks: []\n"
    ) % (
        FILEID_MATERIAL, material.name, STANDARD_SHADER_GUID, kw_block,
        main_tex, bump_tex, metal_tex,
        _f(material.smoothness), _f(material.metallic),
        _f(r), _f(g), _f(b), _f(a),
    )


# =====================================================================
# Prefab .prefab
# =====================================================================

def prefab_file(prefab_name, fbx_guid, material_guid):
    """生成 Prefab .prefab 内容（GameObject + Transform + MeshFilter + MeshRenderer）。

    mesh 引用 FBX 内固定 fileID 4300000；材质引用独立 .mat（type: 2）。
    """
    go = FILEID_ROOT_GAMEOBJECT
    tr = FILEID_ROOT_TRANSFORM
    mf = FILEID_MESH_FILTER
    mr = FILEID_MESH_RENDERER

    if material_guid:
        material_ref = "  - {fileID: %d, guid: %s, type: 2}\n" % (FILEID_MATERIAL, material_guid)
    else:
        # 无材质时引用内置默认材质
        material_ref = "  - {fileID: 10303, guid: 0000000000000000f000000000000000, type: 0}\n"

    return (
        "%%YAML 1.1\n"
        "%%TAG !u! tag:unity3d.com,2011:\n"
        "--- !u!1 &%d\n"
        "GameObject:\n"
        "  m_ObjectHideFlags: 0\n"
        "  m_CorrespondingSourceObject: {fileID: 0}\n"
        "  m_PrefabInstance: {fileID: 0}\n"
        "  m_PrefabAsset: {fileID: 0}\n"
        "  serializedVersion: 6\n"
        "  m_Component:\n"
        "  - component: {fileID: %d}\n"
        "  - component: {fileID: %d}\n"
        "  - component: {fileID: %d}\n"
        "  m_Layer: 0\n"
        "  m_Name: %s\n"
        "  m_TagString: Untagged\n"
        "  m_Icon: {fileID: 0}\n"
        "  m_NavMeshLayer: 0\n"
        "  m_StaticEditorFlags: 0\n"
        "  m_IsActive: 1\n"
        "--- !u!4 &%d\n"
        "Transform:\n"
        "  m_ObjectHideFlags: 0\n"
        "  m_CorrespondingSourceObject: {fileID: 0}\n"
        "  m_PrefabInstance: {fileID: 0}\n"
        "  m_PrefabAsset: {fileID: 0}\n"
        "  m_GameObject: {fileID: %d}\n"
        "  m_LocalRotation: {x: 0, y: 0, z: 0, w: 1}\n"
        "  m_LocalPosition: {x: 0, y: 0, z: 0}\n"
        "  m_LocalScale: {x: 1, y: 1, z: 1}\n"
        "  m_ConstrainProportionsScale: 0\n"
        "  m_Children: []\n"
        "  m_Father: {fileID: 0}\n"
        "  m_RootOrder: 0\n"
        "  m_LocalEulerAnglesHint: {x: 0, y: 0, z: 0}\n"
        "--- !u!33 &%d\n"
        "MeshFilter:\n"
        "  m_ObjectHideFlags: 0\n"
        "  m_CorrespondingSourceObject: {fileID: 0}\n"
        "  m_PrefabInstance: {fileID: 0}\n"
        "  m_PrefabAsset: {fileID: 0}\n"
        "  m_GameObject: {fileID: %d}\n"
        "  m_Mesh: {fileID: %d, guid: %s, type: 3}\n"
        "--- !u!23 &%d\n"
        "MeshRenderer:\n"
        "  m_ObjectHideFlags: 0\n"
        "  m_CorrespondingSourceObject: {fileID: 0}\n"
        "  m_PrefabInstance: {fileID: 0}\n"
        "  m_PrefabAsset: {fileID: 0}\n"
        "  m_GameObject: {fileID: %d}\n"
        "  m_Enabled: 1\n"
        "  m_CastShadows: 1\n"
        "  m_ReceiveShadows: 1\n"
        "  m_DynamicOccludee: 1\n"
        "  m_StaticShadowCaster: 0\n"
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
        "  m_PreserveUVs: 0\n"
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
    ) % (
        go, tr, mf, mr, prefab_name, tr, go,
        mf, go, FILEID_MESH, fbx_guid, mr, go, material_ref,
    )


# =====================================================================
# 场景 .unity
# =====================================================================

# 场景基础对象（OcclusionCullingSettings / RenderSettings / LightmapSettings /
# NavMeshSettings）+ 平行光 + 主相机。fileID 固定：1~4 基础对象，
# 100~102 平行光，200~203 主相机。PrefabInstance 从 1000 起分配。
SCENE_HEADER = """%YAML 1.1
%TAG !u! tag:unity3d.com,2011:
--- !u!29 &1
OcclusionCullingSettings:
  m_ObjectHideFlags: 0
  serializedVersion: 2
  m_OcclusionBakeSettings:
    smallestOccluder: 5
    smallestHole: 0.25
    backfaceThreshold: 100
  m_SceneGUID: 00000000000000000000000000000000
  m_OcclusionCullingData: {fileID: 0}
--- !u!104 &2
RenderSettings:
  m_ObjectHideFlags: 0
  serializedVersion: 9
  m_Fog: 0
  m_FogColor: {r: 0.5, g: 0.5, b: 0.5, a: 1}
  m_FogMode: 3
  m_FogDensity: 0.01
  m_LinearFogStart: 0
  m_LinearFogEnd: 300
  m_AmbientSkyColor: {r: 0.212, g: 0.227, b: 0.259, a: 1}
  m_AmbientEquatorColor: {r: 0.114, g: 0.125, b: 0.133, a: 1}
  m_AmbientGroundColor: {r: 0.047, g: 0.043, b: 0.035, a: 1}
  m_AmbientIntensity: 1
  m_AmbientMode: 0
  m_SubtractiveShadowColor: {r: 0.42, g: 0.478, b: 0.627, a: 1}
  m_SkyboxMaterial: {fileID: 10304, guid: 0000000000000000f000000000000000, type: 0}
  m_HaloStrength: 0.5
  m_FlareStrength: 1
  m_FlareFadeSpeed: 3
  m_HaloTexture: {fileID: 0}
  m_SpotCookie: {fileID: 10001, guid: 0000000000000000e000000000000000, type: 0}
  m_DefaultReflectionMode: 0
  m_DefaultReflectionResolution: 128
  m_ReflectionBounces: 1
  m_ReflectionIntensity: 1
  m_CustomReflection: {fileID: 0}
  m_Sun: {fileID: 0}
  m_IndirectSpecularColor: {r: 0.18028378, g: 0.22571412, b: 0.30692285, a: 1}
  m_UseRadianceAmbientProbe: 0
--- !u!157 &3
LightmapSettings:
  m_ObjectHideFlags: 0
  serializedVersion: 12
  m_GIWorkflowMode: 1
  m_GISettings:
    serializedVersion: 2
    m_BounceScale: 1
    m_IndirectOutputScale: 1
    m_AlbedoBoost: 1
    m_EnvironmentLightingMode: 0
    m_EnableBakedLightmaps: 0
    m_EnableRealtimeLightmaps: 0
  m_LightmapEditorSettings:
    serializedVersion: 12
    m_Resolution: 2
    m_BakeResolution: 40
    m_AtlasSize: 1024
    m_AO: 0
    m_AOMaxDistance: 1
    m_CompAOExponent: 1
    m_CompAOExponentDirect: 0
    m_ExtractAmbientOcclusion: 0
    m_Padding: 2
    m_LightmapParameters: {fileID: 0}
    m_LightmapsBakeMode: 1
    m_TextureCompression: 1
    m_FinalGather: 0
    m_FinalGatherFiltering: 1
    m_FinalGatherRayCount: 256
    m_ReflectionCompression: 2
    m_MixedBakeMode: 2
    m_BakeBackend: 1
    m_PVRSampling: 1
    m_PVRDirectSampleCount: 32
    m_PVRSampleCount: 500
    m_PVRBounces: 2
    m_PVREnvironmentSampleCount: 500
    m_PVREnvironmentReferencePointCount: 2048
    m_PVRFilteringMode: 1
    m_PVRDenoiserTypeDirect: 1
    m_PVRDenoiserTypeIndirect: 1
    m_PVRDenoiserTypeAO: 1
    m_PVRFilterTypeDirect: 0
    m_PVRFilterTypeIndirect: 0
    m_PVRFilterTypeAO: 0
    m_PVREnvironmentMIS: 1
    m_PVRCulling: 1
    m_PVRFilteringGaussRadiusDirect: 1
    m_PVRFilteringGaussRadiusIndirect: 5
    m_PVRFilteringGaussRadiusAO: 2
    m_PVRFilteringAtrousPositionSigmaDirect: 0.5
    m_PVRFilteringAtrousPositionSigmaIndirect: 2
    m_PVRFilteringAtrousPositionSigmaAO: 1
    m_ExportTrainingData: 0
    m_TrainingDataDestination: TrainingData
    m_LightProbeSampleCountMultiplier: 4
  m_LightingDataAsset: {fileID: 0}
  m_LightingSettings: {fileID: 0}
--- !u!196 &4
NavMeshSettings:
  serializedVersion: 2
  m_ObjectHideFlags: 0
  m_BuildSettings:
    serializedVersion: 2
    agentTypeID: 0
    agentRadius: 0.5
    agentHeight: 2
    agentSlope: 45
    agentClimb: 0.4
    ledgeDropHeight: 0
    maxJumpAcrossDistance: 0
    minRegionArea: 2
    manualCellSize: 0
    cellSize: 0.16666667
    manualTileSize: 0
    tileSize: 256
    accuratePlacement: 0
    maxJobWorkers: 0
    preserveTilesOutsideBounds: 0
    debug:
      m_Flags: 0
  m_NavMeshData: {fileID: 0}
--- !u!1 &100
GameObject:
  m_ObjectHideFlags: 0
  m_CorrespondingSourceObject: {fileID: 0}
  m_PrefabInstance: {fileID: 0}
  m_PrefabAsset: {fileID: 0}
  serializedVersion: 6
  m_Component:
  - component: {fileID: 102}
  - component: {fileID: 101}
  m_Layer: 0
  m_Name: Directional Light
  m_TagString: Untagged
  m_Icon: {fileID: 0}
  m_NavMeshLayer: 0
  m_StaticEditorFlags: 0
  m_IsActive: 1
--- !u!108 &101
Light:
  m_ObjectHideFlags: 0
  m_CorrespondingSourceObject: {fileID: 0}
  m_PrefabInstance: {fileID: 0}
  m_PrefabAsset: {fileID: 0}
  m_GameObject: {fileID: 100}
  m_Enabled: 1
  serializedVersion: 10
  m_Type: 1
  m_Shape: 0
  m_Color: {r: 1, g: 0.95686275, b: 0.8392157, a: 1}
  m_Intensity: 1
  m_Range: 10
  m_SpotAngle: 30
  m_InnerSpotAngle: 21.80208
  m_CookieSize: 10
  m_Shadows:
    m_Type: 2
    m_Resolution: -1
    m_CustomResolution: -1
    m_Strength: 1
    m_Bias: 0.05
    m_NormalBias: 0.4
    m_NearPlane: 0.2
    m_CullingMatrixOverride:
      e00: 1
      e01: 0
      e02: 0
      e03: 0
      e10: 0
      e11: 1
      e12: 0
      e13: 0
      e20: 0
      e21: 0
      e22: 1
      e23: 0
      e30: 0
      e31: 0
      e32: 0
      e33: 1
    m_UseCullingMatrixOverride: 0
  m_Cookie: {fileID: 0}
  m_DrawHalo: 0
  m_Flare: {fileID: 0}
  m_RenderMode: 0
  m_CullingMask:
    serializedVersion: 2
    m_Bits: 4294967295
  m_RenderingLayerMask: 1
  m_Lightmapping: 1
  m_LightShadowCasterMode: 0
  m_AreaSize: {x: 1, y: 1}
  m_BounceIntensity: 1
  m_ColorTemperature: 6570
  m_UseColorTemperature: 0
  m_BoundingSphereOverride: {x: 0, y: 0, z: 0, w: 0}
  m_UseBoundingSphereOverride: 0
  m_UseViewFrustumForShadowCasterCull: 1
  m_ShadowRadius: 0
  m_ShadowAngle: 0
--- !u!4 &102
Transform:
  m_ObjectHideFlags: 0
  m_CorrespondingSourceObject: {fileID: 0}
  m_PrefabInstance: {fileID: 0}
  m_PrefabAsset: {fileID: 0}
  m_GameObject: {fileID: 100}
  m_LocalRotation: {x: 0.40821788, y: -0.23456968, z: 0.10938163, w: 0.8754261}
  m_LocalPosition: {x: 0, y: 3, z: 0}
  m_LocalScale: {x: 1, y: 1, z: 1}
  m_ConstrainProportionsScale: 0
  m_Children: []
  m_Father: {fileID: 0}
  m_RootOrder: 1
  m_LocalEulerAnglesHint: {x: 50, y: -30, z: 0}
--- !u!1 &200
GameObject:
  m_ObjectHideFlags: 0
  m_CorrespondingSourceObject: {fileID: 0}
  m_PrefabInstance: {fileID: 0}
  m_PrefabAsset: {fileID: 0}
  serializedVersion: 6
  m_Component:
  - component: {fileID: 203}
  - component: {fileID: 202}
  - component: {fileID: 201}
  m_Layer: 0
  m_Name: Main Camera
  m_TagString: MainCamera
  m_Icon: {fileID: 0}
  m_NavMeshLayer: 0
  m_StaticEditorFlags: 0
  m_IsActive: 1
--- !u!81 &201
AudioListener:
  m_ObjectHideFlags: 0
  m_CorrespondingSourceObject: {fileID: 0}
  m_PrefabInstance: {fileID: 0}
  m_PrefabAsset: {fileID: 0}
  m_GameObject: {fileID: 200}
  m_Enabled: 1
--- !u!20 &202
Camera:
  m_ObjectHideFlags: 0
  m_CorrespondingSourceObject: {fileID: 0}
  m_PrefabInstance: {fileID: 0}
  m_PrefabAsset: {fileID: 0}
  m_GameObject: {fileID: 200}
  m_Enabled: 1
  serializedVersion: 2
  m_ClearFlags: 1
  m_BackGroundColor: {r: 0.19215687, g: 0.3019608, b: 0.4745098, a: 0}
  m_projectionMatrixMode: 1
  m_GateFitMode: 2
  m_FOVAxisMode: 0
  m_SensorSize: {x: 36, y: 24}
  m_LensShift: {x: 0, y: 0}
  m_FocalLength: 50
  m_NormalizedViewPortRect:
    serializedVersion: 2
    x: 0
    y: 0
    width: 1
    height: 1
  near clip plane: 0.3
  far clip plane: 1000
  field of view: 60
  orthographic: 0
  orthographic size: 5
  m_Depth: -1
  m_CullingMask:
    serializedVersion: 2
    m_Bits: 4294967295
  m_RenderingPath: -1
  m_TargetTexture: {fileID: 0}
  m_TargetDisplay: 0
  m_TargetEye: 3
  m_HDR: 1
  m_AllowMSAA: 1
  m_AllowDynamicResolution: 0
  m_ForceIntoRT: 0
  m_OcclusionCulling: 1
  m_StereoConvergence: 10
  m_StereoSeparation: 0.022
--- !u!4 &203
Transform:
  m_ObjectHideFlags: 0
  m_CorrespondingSourceObject: {fileID: 0}
  m_PrefabInstance: {fileID: 0}
  m_PrefabAsset: {fileID: 0}
  m_GameObject: {fileID: 200}
  m_LocalRotation: {x: 0, y: 0, z: 0, w: 1}
  m_LocalPosition: {x: 0, y: 1, z: -10}
  m_LocalScale: {x: 1, y: 1, z: 1}
  m_ConstrainProportionsScale: 0
  m_Children: []
  m_Father: {fileID: 0}
  m_RootOrder: 0
  m_LocalEulerAnglesHint: {x: 0, y: 0, z: 0}
"""


def scene_header():
    return SCENE_HEADER


def empty_gameobject_block(
    go_id, tr_id, name, parent_tr_id, position, rotation, scale,
    children_tr_ids, root_order,
):
    """生成场景里的空 GameObject + Transform（用于 Maya 组）。"""
    px, py, pz = position
    qx, qy, qz, qw = rotation
    sx, sy, sz = scale

    if children_tr_ids:
        children_lines = "\n".join(
            "  - {fileID: %d}" % cid for cid in children_tr_ids
        )
        children_block = "  m_Children:\n%s\n" % children_lines
    else:
        children_block = "  m_Children: []\n"

    return (
        "--- !u!1 &%d\n"
        "GameObject:\n"
        "  m_ObjectHideFlags: 0\n"
        "  m_CorrespondingSourceObject: {fileID: 0}\n"
        "  m_PrefabInstance: {fileID: 0}\n"
        "  m_PrefabAsset: {fileID: 0}\n"
        "  serializedVersion: 6\n"
        "  m_Component:\n"
        "  - component: {fileID: %d}\n"
        "  m_Layer: 0\n"
        "  m_Name: %s\n"
        "  m_TagString: Untagged\n"
        "  m_Icon: {fileID: 0}\n"
        "  m_NavMeshLayer: 0\n"
        "  m_StaticEditorFlags: 0\n"
        "  m_IsActive: 1\n"
        "--- !u!4 &%d\n"
        "Transform:\n"
        "  m_ObjectHideFlags: 0\n"
        "  m_CorrespondingSourceObject: {fileID: 0}\n"
        "  m_PrefabInstance: {fileID: 0}\n"
        "  m_PrefabAsset: {fileID: 0}\n"
        "  m_GameObject: {fileID: %d}\n"
        "  m_LocalRotation: {x: %s, y: %s, z: %s, w: %s}\n"
        "  m_LocalPosition: {x: %s, y: %s, z: %s}\n"
        "  m_LocalScale: {x: %s, y: %s, z: %s}\n"
        "  m_ConstrainProportionsScale: 0\n"
        "%s"
        "  m_Father: {fileID: %d}\n"
        "  m_RootOrder: %d\n"
        "  m_LocalEulerAnglesHint: {x: 0, y: 0, z: 0}\n"
    ) % (
        go_id, tr_id, name, tr_id, go_id,
        _f(qx), _f(qy), _f(qz), _f(qw),
        _f(px), _f(py), _f(pz),
        _f(sx), _f(sy), _f(sz),
        children_block,
        parent_tr_id, root_order,
    )


def prefab_instance_block(
    instance_id, transform_id, prefab_guid, name, position, rotation, scale,
    parent_transform_id=0, root_order=0,
):
    """生成场景里的一个 PrefabInstance（!u!1001）+ 其 stripped Transform（!u!4）。

    parent_transform_id：父 Transform 的 fileID，0 表示场景根。
    """
    px, py, pz = position
    qx, qy, qz, qw = rotation
    sx, sy, sz = scale

    mods = []
    mods.append(
        "    - target: {fileID: %d, guid: %s, type: 3}\n"
        "      propertyPath: m_Name\n"
        "      value: %s\n"
        "      objectReference: {fileID: 0}\n" % (FILEID_ROOT_GAMEOBJECT, prefab_guid, name)
    )
    for prop, val in (
        ("m_LocalPosition.x", px), ("m_LocalPosition.y", py), ("m_LocalPosition.z", pz),
        ("m_LocalRotation.x", qx), ("m_LocalRotation.y", qy), ("m_LocalRotation.z", qz),
        ("m_LocalRotation.w", qw),
        ("m_LocalScale.x", sx), ("m_LocalScale.y", sy), ("m_LocalScale.z", sz),
        ("m_RootOrder", root_order),
    ):
        mods.append(
            "    - target: {fileID: %d, guid: %s, type: 3}\n"
            "      propertyPath: %s\n"
            "      value: %s\n"
            "      objectReference: {fileID: 0}\n" % (FILEID_ROOT_TRANSFORM, prefab_guid, prop, _f(val))
        )

    return (
        "--- !u!1001 &%d\n"
        "PrefabInstance:\n"
        "  m_ObjectHideFlags: 0\n"
        "  serializedVersion: 2\n"
        "  m_Modification:\n"
        "    m_TransformParent: {fileID: %d}\n"
        "    m_Modifications:\n"
        "%s"
        "    m_RemovedComponents: []\n"
        "  m_SourcePrefab: {fileID: %d, guid: %s, type: 3}\n"
        "--- !u!4 &%d stripped\n"
        "Transform:\n"
        "  m_CorrespondingSourceObject: {fileID: %d, guid: %s, type: 3}\n"
        "  m_PrefabInstance: {fileID: %d}\n"
        "  m_PrefabAsset: {fileID: 0}\n"
    ) % (
        instance_id, parent_transform_id, "".join(mods),
        FILEID_PREFAB_SOURCE, prefab_guid,
        transform_id, FILEID_ROOT_TRANSFORM, prefab_guid, instance_id,
    )


def scene_file(body_blocks):
    """组装完整场景文件：场景模板 + 空组 / PrefabInstance 等块。"""
    return SCENE_HEADER + body_blocks
