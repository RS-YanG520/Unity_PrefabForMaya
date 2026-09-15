# Maya Prefab Builder

一个 Unity 编辑器插件：识别 FBX 中由 **Maya 实例**展开的相同网格，把它们还原成 Unity 的 **Prefab + 实例** 形式，并按 Maya 里的原始位置摆入当前场景。

在 Maya 中通过实例复制（Duplicate Special / instance）得到的多个实例，导出为 FBX 后会各自展开成「几何体完全相同、但相互独立」的网格。Unity 默认会当成 N 份独立网格处理，造成几何体重复。本插件把它们识别为同一组，只保留一份 Prefab，其余用 Prefab 实例引用，符合 Unity 的最佳实践。

---

## 功能特性

- **单模型 → Prefab**：把单个 FBX 转成独立 Prefab（命名 = FBX 文件名）。
- **整体 FBX → 实例替换**：分析一个整体 FBX，识别其中的实例组，生成 Prefab 并按原始位置摆入场景。
- **几何签名识别**：无需 Maya 侧脚本，靠网格的几何特征（顶点数 / 包围盒 / 子网格 / 索引数，可读时再加顶点哈希）判断「同一实例」。
- **重名处理**：输出目录已存在同名 Prefab 时，弹窗让用户选择「覆盖 / 跳过 / 重命名」。
- **引用 FBX 内资产**：Prefab 直接引用 FBX 子资产（网格、材质），不重复导出 `.mesh` / `.mat`。

---

## 目录结构

```
Assets/Unity_PrefabForMaya/
├── Editor/           插件脚本
│   ├── MeshSignature.cs               几何签名工具
│   ├── MayaInstancePrefabBuilder.cs   核心编排 + 菜单命令
│   └── MayaInstancePrefabWindow.cs    可视化窗口
├── GeneratedPrefabs/ 生成的 Prefab 输出目录（自动创建）
├── Model/            示例模型
│   ├── exp.fbx       含多个 Maya 实例的整体 FBX（演示「实例替换」）
│   └── pCube2.fbx    单个立方体 FBX（演示「单模型 → Prefab」）
└── Scenes/           示例场景
    └── SampleScene.unity
```

---

## 使用方式

所有入口都在菜单 **Tools > Maya Prefab Builder** 下：

| 菜单项 | 说明 |
|---|---|
| **Open Window** | 打开可视化窗口（推荐），支持拖拽 FBX、查看识别结果 |
| **Build from Selected FBX** | 快捷命令：对 Project 窗口中选中的 FBX 直接「分析 + 生成 + 摆入场景」 |
| **Convert Selected FBX to Prefab** | 快捷命令：把选中的 FBX 直接转成独立 Prefab |

### 可视化窗口流程

1. 打开窗口：`Tools > Maya Prefab Builder > Open Window`。
2. **单模型 → Prefab**：拖入单个 FBX（如 `pCube2.fbx`），点「生成 Prefab 文件」，输出到 `GeneratedPrefabs/`。
3. **整体 FBX → 实例替换**：拖入整体 FBX（如 `exp.fbx`），点「分析实例」查看识别出的实例组，再点「生成 Prefab 并摆入场景」。
4. 场景中会出现 `{FBX名}_Instances` 根节点，其下是按 Maya 原始位置摆放的 Prefab 实例。

---

## 工作原理

1. 用 `AssetDatabase.LoadAssetAtPath<GameObject>` 读取 FBX 导入后的根对象，取其**直接子物体**作为实例候选（对应 Maya 的 Model 节点）。
2. 对每个候选递归收集 `MeshFilter.sharedMesh`，计算**几何签名**：
   - 始终可用：`vertexCount`、`subMeshCount`、`bounds`、每个 submesh 的 `GetIndexCount`；
   - 网格 `isReadable` 时：额外用 FNV-1a 64 位哈希折叠顶点 / 法线 / UV / 三角形，消除假阳性。
3. 按签名分组，**成员数 ≥ 2** 的组即为一个实例组。
4. 每组取第一个成员为 base，生成基础 Prefab（根变换归零，保留子物体相对变换），再对组内每个成员用 `PrefabUtility.InstantiatePrefab` 实例化，并设置为其在 FBX 中的世界位置。
5. 签名唯一（成员数 = 1）的网格不是实例，跳过并在 Console 打印提示。

> Maya 实例在 FBX 里被展开成各自独立的 Mesh，指针不同，所以不能像 UnrealToUnity 那样直接比 Mesh 指针，必须算几何签名。

---

## 重名处理

当 `GeneratedPrefabs/` 里已存在同名 Prefab 时，会弹出对话框，三个选项：

- **覆盖**：用新 Prefab 覆盖旧的。
- **跳过**：跳过本次生成，保留旧文件。
- **重命名**：用 `AssetDatabase.GenerateUniqueAssetPath` 自动加数字后缀，另存一个新 Prefab。

---

## 相关参考

- [../UnrealToUnity](../UnrealToUnity)：一个 Unreal → Unity 的 C++ 导出插件，同样实现了「识别相同组件 → Prefab 实例化」的思路，但其实例判定靠「组件类型 + Mesh/材质指针」，与本插件靠「几何签名」形成对照。详见其目录下的 `UnrealToUnity_插件功能分析.md`。
