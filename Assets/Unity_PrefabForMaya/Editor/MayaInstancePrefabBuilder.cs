using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEngine;

namespace MayaPrefabBuilder
{
    /// <summary>一个实例组：几何体完全相同的一组对象。</summary>
    public class InstanceGroup
    {
        public string PrefabName;
        public List<Transform> Members = new List<Transform>();

        /// <summary>按名字匹配到的已有单模型 Prefab（可能为 null，表示未匹配）。</summary>
        public GameObject MatchedPrefab;
    }

    /// <summary>单个 FBX 的分析结果。</summary>
    public class FbxAnalysis
    {
        public string FbxPath;
        public string FbxName;
        public readonly List<InstanceGroup> Groups = new List<InstanceGroup>();
        public readonly List<Transform> Singletons = new List<Transform>();
    }

    /// <summary>
    /// 识别 FBX 中的 Maya 实例，生成基础 Prefab，并按原始位置摆入当前场景。
    ///
    /// 用法：Tools > Maya Prefab Builder > Build from Selected FBX（快捷命令），
    /// 或打开可视化窗口 Tools > Maya Prefab Builder > Open Window。
    /// </summary>
    public static class MayaInstancePrefabBuilder
    {
        private const string MenuRoot = "Tools/Maya Prefab Builder/";
        private const string OutputFolder = "Assets/Unity_PrefabForMaya/GeneratedPrefabs";

        // 单次运行内用于避免 Prefab 命名冲突
        private static readonly HashSet<string> s_usedNames = new HashSet<string>();

        private enum OverwriteAction
        {
            Overwrite,
            Skip,
            Rename
        }

        [MenuItem(MenuRoot + "Build from Selected FBX")]
        public static void BuildFromSelected()
        {
            var fbxPaths = Selection.assetGUIDs
                .Select(AssetDatabase.GUIDToAssetPath)
                .Where(p => !string.IsNullOrEmpty(p) && p.EndsWith(".fbx", System.StringComparison.OrdinalIgnoreCase))
                .ToArray();

            if (fbxPaths.Length == 0)
            {
                Debug.LogWarning("Maya Prefab Builder: 请在 Project 窗口选中至少一个 FBX 文件。");
                return;
            }

            foreach (var path in fbxPaths)
            {
                Build(Analyze(path));
            }

            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
        }

        [MenuItem(MenuRoot + "Convert Selected FBX to Prefab")]
        public static void ConvertSelectedToPrefab()
        {
            var fbxPaths = Selection.assetGUIDs
                .Select(AssetDatabase.GUIDToAssetPath)
                .Where(p => !string.IsNullOrEmpty(p) && p.EndsWith(".fbx", System.StringComparison.OrdinalIgnoreCase))
                .ToArray();

            if (fbxPaths.Length == 0)
            {
                Debug.LogWarning("Maya Prefab Builder: 请在 Project 窗口选中至少一个 FBX 文件。");
                return;
            }

            foreach (var path in fbxPaths)
            {
                ConvertFbxToPrefab(path);
            }

            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
        }

        /// <summary>
        /// 将单个模型 FBX 转换为独立 Prefab（命名 = FBX 文件名），供整体 FBX 的实例替换使用。
        /// </summary>
        public static GameObject ConvertFbxToPrefab(string fbxPath)
        {
            var fbxRoot = AssetDatabase.LoadAssetAtPath<GameObject>(fbxPath);
            if (fbxRoot == null)
            {
                Debug.LogError($"Maya Prefab Builder: 无法加载模型 {fbxPath}");
                return null;
            }

            EnsureFolder(OutputFolder);

            var prefabName = Path.GetFileNameWithoutExtension(fbxPath);
            var copy = Object.Instantiate(fbxRoot);
            copy.name = prefabName;

            // 根变换归零，保留子物体相对变换
            copy.transform.position = Vector3.zero;
            copy.transform.rotation = Quaternion.identity;
            copy.transform.localScale = Vector3.one;

            var prefabPath = OutputFolder + "/" + prefabName + ".prefab";
            var action = AskOverwrite(ref prefabPath);
            if (action == OverwriteAction.Skip)
            {
                Object.DestroyImmediate(copy);
                Debug.Log($"Maya Prefab Builder: 已跳过 {prefabPath}（已存在同名 Prefab）。");
                return null;
            }

            // Overwrite 或 Rename：prefabPath 已是最终目标路径
            var prefab = PrefabUtility.SaveAsPrefabAsset(copy, prefabPath);
            Object.DestroyImmediate(copy);

            Debug.Log(action == OverwriteAction.Rename
                ? $"Maya Prefab Builder: 已重命名生成单模型 Prefab {prefabPath}"
                : $"Maya Prefab Builder: 已生成单模型 Prefab {prefabPath}");
            return prefab;
        }

        /// <summary>
        /// 分析单个 FBX：识别实例组与唯一网格，不产生任何修改。
        /// </summary>
        public static FbxAnalysis Analyze(string fbxPath)
        {
            var fbxRoot = AssetDatabase.LoadAssetAtPath<GameObject>(fbxPath);
            if (fbxRoot == null)
            {
                Debug.LogError($"Maya Prefab Builder: 无法加载模型 {fbxPath}");
                return null;
            }

            var result = new FbxAnalysis
            {
                FbxPath = fbxPath,
                FbxName = Path.GetFileNameWithoutExtension(fbxPath)
            };

            // 收集顶层实例候选（对应 Maya 的 Model 节点）
            var candidates = new List<Transform>();
            foreach (Transform child in fbxRoot.transform)
            {
                candidates.Add(child);
            }

            // 按几何签名分组，保留层级顺序
            var groups = new Dictionary<string, List<Transform>>();
            var order = new List<string>();
            bool warnedUnreadable = false;
            foreach (var c in candidates)
            {
                var sig = MeshSignature.ComputeSignature(c.gameObject);
                if (!groups.TryGetValue(sig, out var list))
                {
                    list = new List<Transform>();
                    groups[sig] = list;
                    order.Add(sig);
                }
                list.Add(c);

                if (!warnedUnreadable)
                {
                    var f = c.GetComponentInChildren<MeshFilter>();
                    if (f != null && f.sharedMesh != null && !f.sharedMesh.isReadable)
                    {
                        Debug.LogWarning("Maya Prefab Builder: 网格不可读，使用轻量几何签名（如需更精确，可在模型导入设置中开启 Read/Write Enabled）。");
                        warnedUnreadable = true;
                    }
                }
            }

            foreach (var sig in order)
            {
                var members = groups[sig];
                if (members.Count >= 2)
                {
                    var baseName = members[0].name;
                    var prefabName = StripTrailingDigits(baseName);
                    // 优先精确同名匹配（pCube2 → pCube2.prefab），其次去数字匹配（pCube2 → pCube.prefab）
                    var matched = FindPrefabByName(baseName) ?? FindPrefabByName(prefabName);
                    result.Groups.Add(new InstanceGroup
                    {
                        PrefabName = prefabName,
                        Members = members,
                        MatchedPrefab = matched
                    });
                }
                else
                {
                    result.Singletons.Add(members[0]);
                }
            }

            return result;
        }

        /// <summary>
        /// 根据分析结果生成 Prefab 并摆入当前场景。
        /// </summary>
        public static void Build(FbxAnalysis analysis)
        {
            if (analysis == null || analysis.Groups.Count == 0)
            {
                Debug.LogWarning($"Maya Prefab Builder: {(analysis == null ? "无分析结果" : analysis.FbxName + " 未识别到实例组")}，跳过。");
                return;
            }

            EnsureFolder(OutputFolder);
            s_usedNames.Clear();

            var sceneRoot = new GameObject(analysis.FbxName + "_Instances");
            Undo.SetCurrentGroupName("Maya Prefab Builder");
            Undo.RegisterCreatedObjectUndo(sceneRoot, "Maya Prefab Builder");

            int prefabCount = 0;
            int instanceCount = 0;

            foreach (var group in analysis.Groups)
            {
                // 优先使用匹配到的已有单模型 Prefab，否则回退从 FBX 提取生成
                GameObject prefab;
                if (group.MatchedPrefab != null)
                {
                    prefab = group.MatchedPrefab;
                }
                else
                {
                    prefab = CreatePrefab(group.Members[0].gameObject, group.PrefabName, analysis.FbxName);
                    if (prefab == null)
                    {
                        // 用户跳过这一组：不实例化，继续处理下一组
                        continue;
                    }
                }

                foreach (var member in group.Members)
                {
                    var inst = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
                    inst.transform.SetParent(sceneRoot.transform, true);
                    inst.name = member.name;
                    inst.transform.position = member.position;
                    inst.transform.rotation = member.rotation;
                    inst.transform.localScale = member.lossyScale;
                    Undo.RegisterCreatedObjectUndo(inst, "Maya Prefab Builder");
                    instanceCount++;
                }

                prefabCount++;
            }

            Selection.activeGameObject = sceneRoot;
            EditorGUIUtility.PingObject(sceneRoot);
            Debug.Log($"Maya Prefab Builder: {analysis.FbxName} 完成 —— 生成 {prefabCount} 个 Prefab、{instanceCount} 个实例，跳过 {analysis.Singletons.Count} 个唯一网格。");
        }

        /// <summary>
        /// 以 baseObj 为原型生成基础 Prefab，引用 FBX 内的网格与材质子资产。
        /// </summary>
        private static GameObject CreatePrefab(GameObject baseObj, string prefabName, string fbxName)
        {
            var copy = Object.Instantiate(baseObj);
            copy.name = prefabName;

            // 根变换归零，保留子物体相对变换，使 Prefab 表示“原点处的模型”
            copy.transform.position = Vector3.zero;
            copy.transform.rotation = Quaternion.identity;
            copy.transform.localScale = Vector3.one;

            var uniqueName = MakeUniqueName(prefabName);
            var prefabPath = OutputFolder + "/" + fbxName + "_" + uniqueName + ".prefab";

            var action = AskOverwrite(ref prefabPath);
            if (action == OverwriteAction.Skip)
            {
                Object.DestroyImmediate(copy);
                Debug.Log($"Maya Prefab Builder: 已跳过 {prefabPath}（已存在同名 Prefab）。");
                return null;
            }

            var prefab = PrefabUtility.SaveAsPrefabAsset(copy, prefabPath);
            Object.DestroyImmediate(copy);
            if (action == OverwriteAction.Rename)
                Debug.Log($"Maya Prefab Builder: 已重命名生成 Prefab {prefabPath}");
            return prefab;
        }

        private static string MakeUniqueName(string name)
        {
            if (s_usedNames.Add(name))
                return name;

            int i = 2;
            while (!s_usedNames.Add(name + "_" + i))
            {
                i++;
            }
            return name + "_" + i;
        }

        /// <summary>
        /// 检查目标 Prefab 路径是否已存在；若存在则弹窗让用户选择「覆盖 / 跳过 / 重命名」。
        /// 选「重命名」时，用 GenerateUniqueAssetPath 为 prefabPath 自动加上数字后缀。
        /// </summary>
        private static OverwriteAction AskOverwrite(ref string prefabPath)
        {
            var existing = AssetDatabase.LoadAssetAtPath<Object>(prefabPath);
            if (existing == null)
                return OverwriteAction.Overwrite;

            var choice = EditorUtility.DisplayDialogComplex(
                "同名 Prefab 已存在",
                "输出目录里已存在同名 Prefab：\n" + prefabPath + "\n\n如何处理？",
                "覆盖",
                "跳过",
                "重命名");

            switch (choice)
            {
                case 0:
                    return OverwriteAction.Overwrite;
                case 1:
                    return OverwriteAction.Skip;
                default:
                    prefabPath = AssetDatabase.GenerateUniqueAssetPath(prefabPath);
                    return OverwriteAction.Rename;
            }
        }

        private static string StripTrailingDigits(string name)
        {
            var match = Regex.Match(name, @"^(.*?)\d+$");
            var baseName = match.Success && match.Groups[1].Value.Length > 0 ? match.Groups[1].Value : name;
            return baseName.TrimEnd('_', ' ', '.', '-');
        }

        /// <summary>
        /// 按名字在 Assets 下查找已存在的单模型 Prefab（不区分大小写）。
        /// </summary>
        private static GameObject FindPrefabByName(string prefabName)
        {
            var guids = AssetDatabase.FindAssets("t:Prefab", new[] { "Assets" });
            foreach (var guid in guids)
            {
                var path = AssetDatabase.GUIDToAssetPath(guid);
                if (string.Equals(Path.GetFileNameWithoutExtension(path), prefabName, System.StringComparison.OrdinalIgnoreCase))
                {
                    return AssetDatabase.LoadAssetAtPath<GameObject>(path);
                }
            }
            return null;
        }

        private static void EnsureFolder(string folder)
        {
            if (AssetDatabase.IsValidFolder(folder))
                return;

            var parent = Path.GetDirectoryName(folder).Replace('\\', '/');
            var leaf = Path.GetFileName(folder);
            if (string.IsNullOrEmpty(parent) || parent == folder)
                return;

            EnsureFolder(parent);
            AssetDatabase.CreateFolder(parent, leaf);
        }
    }
}
