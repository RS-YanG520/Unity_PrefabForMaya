using System;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

namespace MayaPrefabBuilder
{
    /// <summary>
    /// Maya 实例 → Prefab 构建器的可视化窗口。
    /// 用法：Tools > Maya Prefab Builder > Open Window。
    /// </summary>
    public class MayaInstancePrefabWindow : EditorWindow
    {
        private readonly List<string> fbxPaths = new List<string>();
        private readonly List<string> singleFbxPaths = new List<string>();
        private readonly List<FbxAnalysis> analyses = new List<FbxAnalysis>();
        private readonly List<string> log = new List<string>();
        private Vector2 scroll;

        [MenuItem("Tools/Maya Prefab Builder/Open Window")]
        public static void Open()
        {
            GetWindow<MayaInstancePrefabWindow>("Maya Prefab Builder").Show();
        }

        private void OnGUI()
        {
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Maya 实例 → Prefab 构建器", EditorStyles.boldLabel);
            EditorGUILayout.HelpBox("识别 FBX 中由 Maya 实例展开的相同网格，生成基础 Prefab 并按原始位置摆入场景。", MessageType.Info);

            // ── 单模型 → Prefab ──
            EditorGUILayout.LabelField("单模型 → Prefab", EditorStyles.boldLabel);
            var singleDropRect = GUILayoutUtility.GetRect(0, 40, GUILayout.ExpandWidth(true));
            DrawDropBox(singleDropRect, "拖入单个模型 FBX 到这里");
            HandleSingleDragDrop(singleDropRect);

            for (int i = singleFbxPaths.Count - 1; i >= 0; i--)
            {
                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField(singleFbxPaths[i], EditorStyles.miniLabel);
                if (GUILayout.Button("移除", GUILayout.Width(40)))
                    singleFbxPaths.RemoveAt(i);
                EditorGUILayout.EndHorizontal();
            }

            EditorGUILayout.BeginHorizontal();
            if (GUILayout.Button("添加当前选中"))
                AddSelectedSingles();
            if (GUILayout.Button("清空列表"))
                singleFbxPaths.Clear();
            EditorGUILayout.EndHorizontal();

            GUI.enabled = singleFbxPaths.Count > 0;
            if (GUILayout.Button("生成 Prefab 文件", GUILayout.Height(26)))
                ConvertSinglesToPrefabs();
            GUI.enabled = true;

            EditorGUILayout.Space();

            // ── 整体 FBX → 实例替换 ──
            EditorGUILayout.LabelField("整体 FBX → 实例替换", EditorStyles.boldLabel);

            var listDropRect = GUILayoutUtility.GetRect(0, 40, GUILayout.ExpandWidth(true));
            DrawDropBox(listDropRect, "拖入整体 FBX 到这里，添加到下方列表");
            HandleListDragDrop(listDropRect);

            EditorGUILayout.Space();

            // 目标模型列表
            EditorGUILayout.LabelField("目标模型 (" + fbxPaths.Count + ")", EditorStyles.boldLabel);
            for (int i = fbxPaths.Count - 1; i >= 0; i--)
            {
                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField(fbxPaths[i], EditorStyles.miniLabel);
                if (GUILayout.Button("移除", GUILayout.Width(40)))
                    fbxPaths.RemoveAt(i);
                EditorGUILayout.EndHorizontal();
            }

            EditorGUILayout.BeginHorizontal();
            if (GUILayout.Button("添加当前选中"))
                AddSelected();
            if (GUILayout.Button("清空列表"))
                fbxPaths.Clear();
            EditorGUILayout.EndHorizontal();

            EditorGUILayout.Space();

            GUI.enabled = fbxPaths.Count > 0;
            if (GUILayout.Button("分析实例", GUILayout.Height(26)))
                AnalyzeAll();
            GUI.enabled = true;

            // 识别结果
            if (analyses.Count > 0)
            {
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("识别结果", EditorStyles.boldLabel);
                scroll = EditorGUILayout.BeginScrollView(scroll);
                foreach (var a in analyses)
                {
                    EditorGUILayout.LabelField(a.FbxName, EditorStyles.boldLabel);
                    foreach (var g in a.Groups)
                    {
                        var status = g.MatchedPrefab != null ? "✓ 匹配已有 Prefab" : "· 将提取生成";
                        EditorGUILayout.LabelField($"  Prefab: {g.PrefabName}（{g.Members.Count} 个实例）{status}", EditorStyles.miniLabel);
                        foreach (var m in g.Members)
                            EditorGUILayout.LabelField("      - " + m.name, EditorStyles.miniLabel);
                    }
                    foreach (var s in a.Singletons)
                        EditorGUILayout.LabelField("  跳过唯一网格: " + s.name, EditorStyles.miniLabel);
                }
                EditorGUILayout.EndScrollView();

                EditorGUILayout.Space();
                if (GUILayout.Button("生成 Prefab 并摆入场景", GUILayout.Height(26)))
                    BuildAll();
            }

            // 日志
            if (log.Count > 0)
            {
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("日志", EditorStyles.boldLabel);
                foreach (var line in log)
                    EditorGUILayout.LabelField(line, EditorStyles.wordWrappedMiniLabel);
            }
        }

        private void AddSelectedSingles()
        {
            foreach (var guid in Selection.assetGUIDs)
            {
                var p = AssetDatabase.GUIDToAssetPath(guid);
                if (!string.IsNullOrEmpty(p) && p.EndsWith(".fbx", StringComparison.OrdinalIgnoreCase) && !singleFbxPaths.Contains(p))
                    singleFbxPaths.Add(p);
            }
        }

        private void ConvertSinglesToPrefabs()
        {
            if (singleFbxPaths.Count == 0)
            {
                log.Add("请先拖入或添加至少一个单模型 FBX。");
                return;
            }

            int generated = 0;
            foreach (var p in singleFbxPaths)
            {
                if (MayaInstancePrefabBuilder.ConvertFbxToPrefab(p) != null)
                    generated++;
            }

            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
            log.Add($"已生成 {generated} 个单模型 Prefab（输出到 Assets/Unity_PrefabForMaya/GeneratedPrefabs/）。");
            Repaint();
        }

        private void AddSelected()
        {
            foreach (var guid in Selection.assetGUIDs)
            {
                var p = AssetDatabase.GUIDToAssetPath(guid);
                if (!string.IsNullOrEmpty(p) && p.EndsWith(".fbx", StringComparison.OrdinalIgnoreCase) && !fbxPaths.Contains(p))
                    fbxPaths.Add(p);
            }
        }

        private void HandleSingleDragDrop(Rect rect)
        {
            var e = Event.current;
            if (!rect.Contains(e.mousePosition))
                return;
            if (e.type != EventType.DragUpdated && e.type != EventType.DragPerform)
                return;

            var fbx = GetDraggedFbx();
            if (fbx.Length == 0)
                return;

            DragAndDrop.visualMode = DragAndDropVisualMode.Copy;
            if (e.type == EventType.DragPerform)
            {
                DragAndDrop.AcceptDrag();
                foreach (var p in fbx)
                {
                    if (!singleFbxPaths.Contains(p))
                        singleFbxPaths.Add(p);
                }
            }
            e.Use();
        }

        private void HandleListDragDrop(Rect rect)
        {
            var e = Event.current;
            if (!rect.Contains(e.mousePosition))
                return;
            if (e.type != EventType.DragUpdated && e.type != EventType.DragPerform)
                return;

            var fbx = GetDraggedFbx();
            if (fbx.Length == 0)
                return;

            DragAndDrop.visualMode = DragAndDropVisualMode.Copy;
            if (e.type == EventType.DragPerform)
            {
                DragAndDrop.AcceptDrag();
                foreach (var p in fbx)
                {
                    if (!fbxPaths.Contains(p))
                        fbxPaths.Add(p);
                }
            }
            e.Use();
        }

        private string[] GetDraggedFbx()
        {
            return DragAndDrop.objectReferences
                .Select(AssetDatabase.GetAssetPath)
                .Where(p => !string.IsNullOrEmpty(p) && p.EndsWith(".fbx", StringComparison.OrdinalIgnoreCase))
                .ToArray();
        }

        private void DrawDropBox(Rect rect, string label)
        {
            var oldColor = GUI.color;
            if (IsDraggingFbxOver(rect))
                GUI.color = new Color(0.45f, 0.75f, 1f, 0.6f); // 拖拽悬停高亮（浅蓝）
            GUI.Box(rect, label);
            GUI.color = oldColor;
        }

        private bool IsDraggingFbxOver(Rect rect)
        {
            var e = Event.current;
            if (e.type != EventType.DragUpdated && e.type != EventType.DragPerform)
                return false;
            if (!rect.Contains(e.mousePosition))
                return false;
            return GetDraggedFbx().Length > 0;
        }

        private void AnalyzeAll()
        {
            analyses.Clear();
            log.Clear();
            foreach (var p in fbxPaths)
            {
                var a = MayaInstancePrefabBuilder.Analyze(p);
                if (a == null)
                {
                    log.Add("无法加载: " + p);
                    continue;
                }
                analyses.Add(a);
                log.Add($"{a.FbxName}: {a.Groups.Count} 个实例组, {a.Singletons.Count} 个唯一网格");
            }
            Repaint();
        }

        private void BuildAll()
        {
            foreach (var a in analyses)
                MayaInstancePrefabBuilder.Build(a);

            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
            log.Add("完成：已生成 Prefab 并摆入场景。");
            Repaint();
        }
    }
}
