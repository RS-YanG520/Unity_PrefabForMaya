# Unity MayaToUnity（Maya 2018 插件）

对标 **UnrealToUnity**：在 Maya 侧直接**手写 Unity 的文本资产**（FBX、`.mat`、`.prefab`、`.unity` 及对应 `.meta`），导出一个可直接放入 Unity 项目的目录，无需在 Unity 里做二次处理。

## 功能

- **识别 Maya 实例**：Maya 里用 Duplicate Special（instance）创建的实例会共享同一个 mesh shape。插件把「共享同一 shape 的多个 transform」识别为一个实例组。
- **实例 → Prefab**：每组只导出一个网格（FBX），生成一个 Prefab，其余实例在场景里以该 Prefab 的实例形式、按各自世界变换摆放。
- **唯一网格**：不参与实例化的唯一网格也各导出为一个 Prefab。
- **材质 / 贴图**：读取 Maya 主颜色（baseColor / color）、主贴图（file 节点）、Arnold metalness / specularRoughness，生成 Built-in Standard 材质的 `.mat` 与贴图副本。
- **材质转 PBR**：菜单可将非 `standardSurface` / Arnold 材质一键换成 PBR（保留原名，源材质备份为 `*_mtu_src`），并可一键还原。
- **导出范围可选**：选中对象时只导出选中；不选时导出整个场景。

## 环境

- Maya **2018**（内置 **Python 2.7**）
- Unity **2021.3**（**Built-in 渲染管线**）

## 安装

把 `Unity_MayaToUnity` 目录整体复制到 Maya 脚本目录：

```
D:\My Documents\maya\2018\zh_CN\scripts\Unity_MayaToUnity\
```

目录结构：

```
Unity_MayaToUnity/
  __init__.py           # 包入口 + 菜单注册
  constants.py          # GUID / fileID 约定
  coords.py             # Maya→Unity 坐标转换
  instance_detector.py  # 实例识别
  material_mapper.py    # 材质/贴图映射
  exporter.py           # 导出主流程
  yaml_writer.py        # 手写 Unity YAML
```

## 使用

### 方式一：菜单

在 Maya 脚本编辑器（Script Editor）里运行一次：

```python
import Unity_MayaToUnity
Unity_MayaToUnity.install_menu()
```

主菜单栏会出现 **Unity MayaToUnity → 导出到 Unity 项目...**，点击后弹出目录选择框，选 Unity 项目里的目标目录（例如 `Assets\GeneratedPrefabs`）即可。

### 方式二：直接调用

```python
import Unity_MayaToUnity
Unity_MayaToUnity.export(r"D:\UnityProject\Assets\GeneratedPrefabs")
```

可选参数：

```python
# 只导出当前选中对象；自定义场景名
Unity_MayaToUnity.export(
    output_dir=r"D:\UnityProject\Assets\GeneratedPrefabs",
    selection=cmds.ls(selection=True),
    scene_name="MyScene",
)
```

## 调试

- **诊断场景（dry-run）**：菜单「诊断当前场景...」，或脚本里 `Unity_MayaToUnity.diagnose()`。不导出，打印实例组 / 唯一网格 / 材质 / 贴图及坐标预览。
- **开启调试日志**：菜单勾选「开启调试日志」，或 `Unity_MayaToUnity.set_debug(True)`。导出过程实时打印关键信息（实例组、mesh 名、GUID、材质映射等）。
- **诊断报告文件**：每次导出都会在输出目录写一份 `export_report.log`，记录完整过程。排查「mesh 丢失 / 材质紫色」时优先看它——尤其核对 `FBX mesh 名` 是否与 `.meta` 里 `fileIDToRecycleName` 的 key 一致。

## 输出结构

```
<输出目录>/
  <名字>.fbx          # 每个 Prefab 一份网格
  <名字>.fbx.meta     # 固定 fileID（4300000 等），供引用
  <名字>.prefab       # 引用 FBX 内网格 + 材质
  <名字>.prefab.meta
  Materials/
    <材质>.mat
    <材质>.mat.meta
  Textures/
    <贴图>.<ext>
    <贴图>.<ext>.meta
  MayaToUnity.unity   # 场景：Prefab 实例按 Maya 世界变换摆放
  export_report.log   # 诊断日志（每次导出始终写入）
```

## 原理要点

- **GUID + fileID 引用**：Unity 资源引用靠「GUID + fileID」两要素。插件用名字的 MD5 生成确定性 GUID，fileID 采用固定约定（mesh=4300000、材质=2100000、贴图=2800000、Prefab 源=100100000 等）。
- **fileIDToRecycleName**：FBX 的 `.meta` 通过该表把 FBX 内 mesh 的 fileID 固定为 4300000，于是 `.prefab` / `.unity` 里能用 `{fileID: 4300000, guid: <fbx guid>, type: 3}` 直接引用，无需复刻 Unity 的名字哈希算法。
- **坐标转换**（已实测）：位置 `(-x/100, y/100, z/100)`；旋转经 X 镜像 + 四元数共轭；缩放取世界矩阵**行**长度。详见 `可行性分析.md` 阶段进度。

## 当前限制（第一版）

- 只处理静态 mesh，不含骨骼 / 蒙皮 / 动画。
- 材质只映射主颜色 + 主贴图 + metallic / roughness；复杂 shading network（程序纹理、混合节点）需要先烘焙。
- 贴图按引用复制，不处理 UV 平铺/偏移与颜色校正节点。
- 场景生成的是基础摄像机 + 平行光 + Prefab 实例；Maya 里的灯光 / 相机不导出。
- 场景保留组嵌套（空组→空 GameObject）；Prefab 资产本身仍是单网格，不含内部层级。

## 阶段进度（摘要）

第一版主路径已打通：实例识别 → 对象空间 FBX → Prefab / 材质 / 带层级的场景 YAML，坐标已对齐。后续重点为材质增强，见 `可行性分析.md` §6。
