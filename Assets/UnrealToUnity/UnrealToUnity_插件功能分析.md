# UnrealToUnity 插件功能分析

> 分析对象：`Assets/UnrealToUnity/`（Unreal 引擎 C++ 编辑器插件）
> 作者：Ciprian Stanciu ｜ 版本：`UTU Version = 1.54`（见 `Source/UnrealToUnity/Public/PluginConfig.h`）
> 分析日期：2026-09-15

---

## 一、一句话定位

一个 Unreal 引擎的 **C++ 编辑器插件**，在 Unreal Editor 里一键把当前关卡导出成一套 Unity 可直接打开的项目资产，输出到 `C:\UnrealToUnity\`，然后用户用 Unity 打开那个目录。

它不是 Unity 侧插件，而是**在 Unreal 里直接生成 Unity 的序列化文本文件**（`.unity` / `.prefab` / `.mat` / `.shader` / `.fbx` / `.png` / `.meta`）。

---

## 二、整体架构：两半

### 1. Unreal 侧（`Source/UnrealToUnity/`，C++，约 2.4 万行）

| 文件 | 行数 | 职责 |
|---|---|---|
| `Public/UTXModule.cpp` | 5112 | 模块入口、工具栏按钮/菜单、材质图 → HLSL 翻译 |
| `Public/ExportToUnity.cpp` | 7282 | 导出主流程：遍历 Actor、导出 mesh/FBX/贴图/材质/动画/灯光/碰撞 |
| `Public/UnityScene.cpp` | 6059 | 把中间数据序列化成 Unity YAML 文本 |
| `Public/BaseScene.cpp` | 928 | 中间对象模型（核心数据结构） |
| `Public/ExportLandscape.cpp` | 1452 | 地形导出 |
| `Public/MaterialUniformExpressions2.cpp` + `Public/Preshader.cpp` | ~1500 | 材质参数 / Preshader 求值 |

依赖模块（`UnrealToUnity.Build.cs`）：`MaterialEditor`、`MeshDescription`、`RawMesh`、`Landscape`、`Foliage`、`MeshMergeUtilities`、`MaterialBaking`、`StaticMeshDescription`、`Chaos` 等，以及 FBX 第三方库。

### 2. Unity 侧（`Resources/`，随导出结果一起复制到 Unity 项目）

- `Resources/InstancedMesh.cs`：运行时用 `Graphics.DrawMeshInstanced` 批量绘制实例（存一组 TRS 数组）。
- `Resources/TreeInstanceComponent.cs`：存 Foliage 实例数据（位置/缩放/旋转/Prefab），之后可转移到 Terrain 组件。
- `Resources/Proxy.shader`、`UnrealCommon.cginc`、`LargeWorldCoordinates.hlsl`、`LWCOperations.hlsl` 等：还原 Unreal 材质的 shader 基础设施。
- `Resources/URP/`、`Resources/HDRP/`：预设的渲染管线设置资源。

---

## 三、使用流程

1. 新建一个 **C++ 项目**（不能是 Blueprint 项目），把插件文件夹复制到 `/YourProject/Plugins/UnrealToUnity/`。
2. 打开一个场景，或用菜单 `PlaceAllAssetsInScene` 把资源自动铺到一个空场景。**只有当前打开场景里的资产会被导出**。
3. 点工具栏 `UnityToUnreal` 按钮 → `Export...`，弹出属性面板设置选项。
4. 导出到 `C:\UnrealToUnity\`。
5. 用 Unity 打开该目录；根目录 `Assets/` 下有一个 FBX 或 `Scene.unity`，拖入新场景即可（建议删掉默认的 `MainCamera` 和 `DirectionalLight`，会与 Unreal 自带的相机/灯光冲突）。

其它菜单项：
- `PlaceAllAssetsInScene`：把整个项目的静态网格铺进空场景。
- `ConvertLandscapesToStaticMeshes`：把地形转成静态网格（导出前用）。
- `BakeTextures`：按物体 UV 烘焙贴图，配合 Standard Shader 填充 Unity 贴图槽。

Console 变量：
- `utu.UsePrefabs`：设为 0 可关闭 Prefab 实例化（遇到缺网格/缺材质问题时调试用）。

---

## 四、核心机制

### 1. 中间场景模型（`BaseScene.cpp`）

在内存里先建一套「Unity 概念的对象模型」，Unreal 对象翻译成这套结构，再由 UnityScene 序列化：

- 组件：`GameObject`、`TransformComponent`、`MeshRenderer`、`SkinnedMeshRenderer`、`MeshFilter`、`LightComponent`、`LodGroup`、`MeshCollider`/`BoxCollider`/`SphereCollider`/`CapsuleCollider`、`ReflectionProbe`、`Decal`、`AnimationComponent`、`TreeInstancesComponent`。
- 资产：`UnityMesh`、`UnityMaterial`、`UnityTexture`、`UnityShader`。

`IUnrealExporterInterface`（`ExportToUnity.h:290`）抽象了目标引擎差异：`UnityExporter` 是唯一启用实现，Godot 实现被注释掉（`PluginConfig.h` 里 `#define ENABLE_UNITY_EXPORT`）。

### 2. 手写 Unity YAML 序列化（`UnityScene.cpp`）

不依赖 Unity 运行时，直接 `fopen` 写 Unity 的序列化格式：

- `!u!1 GameObject`、`!u!4 Transform`、`!u!1001 PrefabInstance`、`!u!21 Material` 等。
- `WriteScene`（`UnityScene.cpp:5122`）用 `Resources/Proxy.unity` 做模板头，把生成的 GameObject 文本拼接到模板后面写出。

### 3. GUID / meta / fileID 编码

每个资产生成一个 GUID 写进 `.meta`；场景内用 `{fileID: xxx, guid: xxx, type: 3}` 引用。fileID 按类型用**固定区间编码**：

| 资产类型 | fileID |
|---|---|
| Texture2D | 2800000 |
| TextureCube | 8900000 |
| Texture2DArray | 18700000 |
| VolumeTexture | 11700000 |
| Material mainObject | 2100000 |
| 动画 | 7400000 |
| Prefab source | 100100000 |
| URP shader | 4800000 |
| Standard shader | 46 |

### 4. Prefab 实例化机制（最核心，`BaseScene.cpp`）

- **`EvaluatePrefab`**（`BaseScene.cpp:739`）判断一个对象该不该变成 Prefab 实例：
  1. Foliage（名字带 `UTU_FoliageID`）→ 强制 Prefab；
  2. 否则和已注册的 Prefab 逐个比较「**组件类型列表 + Mesh 指针 + 材质指针列表**」是否完全相同（`BaseScene.cpp:768-829`），相同 → 标记为已有 Prefab 的实例；
  3. 都不是 → 把自己注册成新 Prefab，`PrefabName = Mesh 名字`。
- **`GeneratePrefabInstance`**（`BaseScene.cpp:513`）：写 `!u!1001 PrefabInstance`，用 `m_Modifications` 覆盖实例差异（`m_Name`、`m_LocalPosition`、`m_LocalRotation`、`m_LocalScale`），`m_SourcePrefab` 指向源。
- **`WritePrefabs`**（`UnityScene.cpp:5625`）：每个 Prefab 写 `.prefab` + `.prefab.meta`；根 Transform 在 `ToSceneData(true)` 里 `Reset()` 归零。
- **`FixDuplicatePrefabNames`**（`BaseScene.cpp:717`）：重名加 `_1`/`_2` 后缀。

### 5. 材质转换（`UTXModule.cpp`）

遍历 Unreal 材质图，翻译成 HLSL（自定义 shader，精度高）或映射到 Standard/URP Lit/HDRP Lit 的参数槽（`TextureSemantic` 枚举 = `BaseMap`/`BumpMap`/`MetallicGlossMap`/`OcclusionMap`…，对应 Unity 标准 shader 的贴图槽）。`Preshader.cpp` / `MaterialUniformExpressions2.cpp` 负责把材质参数烘焙成常量/表达式。

### 6. 坐标转换（`ExportActor.cpp`）

`RotateActorsForExport`（`ExportActor.cpp:71`）：导出前把所有 Actor 绕 Z 轴旋转 90°（UE 的 Z-up → Unity 的 Y-up），导出后再转回来。

---

## 五、导出内容覆盖范围

- 静态网格（FBX，用 Unreal 的 FBX 导出器 `ExportFBXForActor`）
- 骨骼网格 + 动画（AnimSequence → 动画 FBX + meta）
- 贴图（PNG / 压缩）、材质、Shader
- 灯光（点 / 聚光 / 平行 / 面积）、天空光
- 碰撞（Box / Sphere / Capsule / Mesh）
- LOD（`LodGroup`）
- 反射探针、贴花
- Landscape（可转静态网格）
- Foliage（实例化，可转 `TreeInstances` 组件）
- 声音（OGG）

---

## 六、可配置项（Export 属性面板，`UUnitySettingsClass`）

| 选项 | 说明 |
|---|---|
| RenderPipeline | Builtin / URP / HDRP |
| Shaders | CustomShaders（精度高）/ Standard Shaders（映射标准槽位） |
| ShowFBXExportDialog | 弹 FBX 导出对话框 |
| ExportLODs / ExportVertexColors | 是否导出 LOD / 顶点色 |
| ExportVertexShader | 导出 WorldPositionOffset（新选项，可能有 shader 错误） |
| ExportBrushes | 导出 brush actor（UE5 下可能崩溃） |
| SkinnedMeshes | Export as Skinned / Static Mesh |
| NaniteOptions | Export Coarse Mesh / Original Mesh |
| TangentToWorldLocation | Keep Unity Vectors / Flip YZ Before-After Normals |
| ExportAllAnimations | 导出骨骼网格的所有兼容动画 |
| FoliageExport | Create GameObject Per Instance / Tree Instances |
| UsePrefabs | 是否生成 Prefab 实例 |

---

## 七、与 Maya→Prefab 插件的对照

两者本质相同：**识别「组件相同」的一组对象 → 提取一个做 Prefab → 其余用 PrefabInstance 引用 + 位置覆盖**。但有一个关键差异：

| | UnrealToUnity | Maya Prefab Builder（本工程） |
|---|---|---|
| 实例判定 | 直接比 `Mesh 指针 ==` 和材质指针（Unreal 里同一 Mesh 资产天然指针相同） | 必须算**几何签名**（vertexCount/bounds/顶点哈希），因为 Maya 实例在 FBX 里被展开成各自独立的 Mesh，指针不同 |
| 位置还原 | 手写 PrefabInstance 的 `m_Modifications` 覆盖 LocalPosition/Rotation/Scale | 运行时 `PrefabUtility.InstantiatePrefab` + 设置 world transform |
| Prefab 根归零 | `ToSceneData(true)` 里 Transform `Reset()` | `position/rotation=0`、`localScale=1` |
| 重名处理 | `_1`/`_2` 后缀 | `GenerateUniqueAssetPath`（空格+数字） |

---

## 八、可借鉴点

1. **「组件签名」比纯几何签名更完整**：它的 `EvaluatePrefab` 除了比 Mesh，还比组件类型和材质——可以区分「几何相同但材质不同」的对象。当前 Maya 插件只按几何签名分组，若要细分材质可参考此思路。
2. **Prefab 命名去重是标准需求**：它专门写了 `FixDuplicatePrefabNames`，印证「重名」是这类工具的通用痛点。
3. **fileID/GUID 固定编码**：若将来想在 Unity 编辑器外直接生成资产文本，这套编码约定（哪个 fileID 区间代表哪类资产）是现成参考。

> 注：在 Unity 编辑器内开发时，优先用官方 `PrefabUtility` API（`SaveAsPrefabAsset` / `InstantiatePrefab`），Unity 会自动维护 GUID、fileID、连接关系和 `.meta` 文件，不需要像本插件那样手写 YAML——后者只是因为它跑在 Unreal 里用不了 `PrefabUtility`。
