# Blender 模型转换手册：把 .blend / PMX 导入 hpRenderer

适用范围：本次静态模型导入实现。面向第一次使用 Blender 的用户，菜单以英文名为准，附中文解释。整理日期：2026-09-08。

## 1. 先判断走哪条路线

| 拿到的文件 | 推荐操作 | 需要注意 |
| --- | --- | --- |
| `.glb` | 在 hpRenderer 中直接导入 | 推荐的交付格式；普通 PNG/JPEG 内嵌贴图可直接读取 |
| `.gltf` | 直接导入 | `.bin` 和贴图目录也要保留，不能只复制 glTF 文件 |
| `.obj` | 直接导入 | 一并保留 `.mtl` 和贴图；传统材质只能近似映射到 PBR |
| `.fbx` / `.FBX` | 直接导入静态模型 | 支持 ASCII/Binary FBX；保留外部贴图目录，骨骼动画仍不播放 |
| `.pmx` | 先尝试直接导入；有材质或姿势问题再用本手册转换 | 静态网格、基础颜色/贴图和透明；不是 MMD 播放器 |
| `.blend` | Blender 打开 → 整理 → 导出 `.glb` | 不能通过修改扩展名转换；本项目会提示先使用 Blender |
| `.pmd` | MMD Tools 导入 Blender → 导出 `.glb` | 当前文件选择器没有开放 PMD 原生导入 |
| `.vmd` / `.vpd` | 不是独立模型文件 | 分别是动作/姿势数据；先有模型，且当前渲染器不播放它们 |
| `.zip` / `.7z` / `.rar` | 完整解压后再找上述模型文件 | 压缩包本身不是模型 |

最快路线：先试直接导入 PMX；只有需要修贴图、整理材质或固定姿势时才进 Blender。不要一上来学习绑定、动画和物理系统。

当前导入器会保留节点层级和静态变换，但不计算骨骼蒙皮、动画、Morph、IK、布料或刚体。带骨骼文件显示的是源网格的静态形状，不保证等于 Blender 当前画面中的姿势。需要某个姿势时，按第 6 节制作静态副本。

## 2. 安装和准备

### 2.1 Blender 版本

本手册建议使用 **Blender 4.5 LTS**，方便跟随相对稳定的界面。它不是最新版；若作者要求更高版本来打开 `.blend`，按作者要求使用相应版本，不要强行用旧版本保存覆盖。下载 Windows 安装包或便携版即可，首次启动保留默认 Blender 快捷键和左键选择。[Blender 4.5 LTS 官方页面](https://www.blender.org/releases/4-5/)

### 2.2 留好原件

1. 阅读模型作者随附的说明，确认转换、修改、展示、再分发等使用范围；转换格式不会改变原有授权。
2. 完整解压到单独目录，不要拆散 `textures` 等文件夹。
3. 保留一份未修改的原始包；转换结果另存到工作目录。
4. 示例文件夹结构：`原始模型/`、`工作副本/character_work.blend`、`导出结果/character_static.glb`。
5. 贴图路径可以使用中文，但第三方插件对特殊字符的处理可能不同。排查问题时可用短英文工作目录；不要单独重命名贴图而不更新材质引用。
6. 打开来路不明的 `.blend` 时，不要为了消除提示就开启自动运行 Python 脚本。某些复杂绑定依赖脚本，是否启用应先确认来源；本手册不要求开启它。

## 3. 只学这几个 Blender 操作就够了

快捷键作用于鼠标所在的区域。操作模型时，把鼠标放在中央的 **3D Viewport（3D 视图）**，不要停在右侧列表或底部时间轴上。

| 目的 | 操作 |
| --- | --- |
| 选择模型 | 左键点击；`Shift` + 左键多选 |
| 围绕模型查看 | 按住鼠标中键拖动 |
| 平移视图 | `Shift` + 鼠标中键拖动 |
| 拉近/拉远 | 滚轮 |
| 找到选中的模型 | 数字小键盘 `.`；没有小键盘时用 `View → Frame Selected` |
| 看见场景全部对象 | `Home`，或 `View → Frame All` |
| 撤销误操作 | `Ctrl + Z` |
| 搜索操作 | `F3`，输入英文操作名称 |

右上角的 **Outliner（大纲视图）**是对象列表。角色可能分成身体、头发、衣服、眼睛等多个网格，不要以为选中身体就选中了整个角色。左上角模式下拉框应为 **Object Mode（物体模式）**；本手册大部分操作都在此模式完成。[Blender 视图导航说明](https://docs.blender.org/manual/en/3.3/editors/3dview/navigate/navigation.html)

## 4. 路线 A：处理 .blend 文件

### 4.1 打开与另存

1. `File → Open`，选择下载模型中的 `.blend`。不是 `File → Import`。
2. 打开后马上用 `File → Save As`，另存为 `character_work.blend`。
3. 在 Outliner 选择角色网格，用 `Frame Selected` 定位。
4. 点击 3D 视图右上角的 **Material Preview（材质预览）**小球，等待贴图加载。Solid（实体）模式通常只显示灰色，不能凭它判断贴图丢失。

### 4.2 如果出现亮粉色/紫红色

这通常是贴图文件没有找到。

1. `File → External Data → Find Missing Files`。
2. 选模型解压后的根目录，让 Blender 在其中查找贴图。
3. 等待材质刷新；必要时保存工作副本并重新打开。
4. 若仍缺图，检查原压缩包是否完整、有无另一份贴图压缩包，或在第 7 节的 Image Texture 节点上重新选正确图片。

查找操作只能重新连接实际存在的文件，不能生成缺失的贴图。[Blender 外部资源说明](https://docs.blender.org/manual/en/4.2/interface/window_system/topbar.html)

### 4.3 决定是否要固定当前姿势

如果只是桌子、雕像等普通静态模型，直接检查材质后去第 8 节导出。如果有骨架、修改器、表情，或者希望导出“现在看到的样子”，先完成第 6 节。

## 5. 路线 B：通过 MMD Tools 处理 PMX / PMD

### 5.1 安装 MMD Tools

Blender 默认不带 PMX 导入菜单，需要安装扩展。

1. 打开 `Edit → Preferences`。
2. 在 `System → Network` 中允许联网（`Allow Online Access`），用于获取扩展。
3. 打开 `Get Extensions`，搜索 **MMD Tools**，安装并确保启用。
4. 当前维护版支持 Blender 4.2–5.2；不要照旧教程安装已经停止维护的 Blender 3.6 分支。
5. 如果无法在线安装，去官方扩展页面获取与 Blender 版本匹配的安装包，再在 Preferences 的扩展菜单使用 `Install from Disk`；不要随意下载安装来源不明的脚本包。

[维护者安装与版本说明](https://github.com/MMD-Blender/blender_mmd_tools)、[Blender 官方扩展页面](https://extensions.blender.org/add-ons/mmd-tools/)。安装位置可能随 Blender 小版本变动，以维护者说明为准。

### 5.2 导入角色

1. `File → New → General` 建立空白工作文件；确认已经保存其他正在编辑的文件。
2. 在新文件的 3D 视图中按 `A`，再按 `X`，确认删除默认立方体、灯光和相机。此操作只针对刚新建的默认场景。
3. 使用 `File → Import` 下 MMD Tools 提供的 MikuMikuDance 模型入口，选择 `.pmx` 或 `.pmd`。也可以在 3D 视图的 MMD 侧栏中使用 Import Model；若菜单没有出现，先检查扩展是否启用。
4. 第一次导入保留默认缩放；不要再同时手工交换坐标轴和旋转 90°。
5. 在 Outliner 找到角色的网格对象，用 `Frame Selected` 定位并切换 Material Preview。
6. 另存为 `character_work.blend`，不覆盖原始 PMX。

现在不需要点击 Physics 的 Build/模拟，也不需要导入 VMD。骨架线条、刚体盒子等辅助显示不是衣服或身体，最终不要把它们当作网格一起导出。

### 5.3 为什么 Blender 中看起来正确，导出却可能变白？

MMD Tools 常使用自定义节点组表达 Toon、球形贴图等效果。glTF 导出器不能把任意节点网络都翻译成标准 PBR 材质。**安装 MMD Tools 只解决读取 PMX，不代表材质自动适合本渲染器。** 请按第 7 节逐个检查主要材质槽；先做好身体、脸、头发，再处理眼睛和睫毛。

## 6. 把当前姿势/修改器结果固定成静态网格

这是“以后还想做骨骼动画”时最需要注意的步骤：**只转换工作副本，保留带骨架的原件。** 当前导出的静态 GLB 不会保留可恢复的绑定系统。

1. 在 `character_work.blend` 中，把时间轴停在希望保留的帧；调整到想要的姿势。不要继续播放动画。
2. 再 `Save As` 为 `character_static_work.blend`，以下操作只在这个副本中进行。
3. 切回 Object Mode。在 Outliner 中多选所有需要导出的网格：身体、衣服、头发、眼睛等；不要只选骨架或最上层 Empty。
4. 在 3D 视图用 `Object → Convert → Mesh`；也可用 `F3` 搜索 `Convert Mesh`。这一步用于把可转换对象/修改器的当前可见几何结果落到网格上。曲线、文字等也要转换成网格才适合这条导入链路。[Blender Convert 说明](https://docs.blender.org/manual/en/4.4/scene_layout/object/editing/convert.html)
5. 转换后逐一检查网格的扳手图标（Modifiers）：不应再依赖 Armature 修改器来维持当前形状。切换时间轴，角色网格应保持同一姿势。若仍会变形或转换报错，先撤销，不要直接删除骨架。
6. 对确认已经固定的网格，用 `Alt + P → Clear Parent and Keep Transformation` 去掉父级依赖，同时保留世界位置。与普通 Clear Parent 不同，必须选择保留变换的选项。
7. 核对外观无误后，在该静态副本中排除原骨架、刚体、辅助 Empty 和未转换的重复对象。**先确认副本不依赖它们，再删除或不导出它们。**

常见陷阱：只在导出器中关闭 Animation，并不等于把蒙皮后的姿势烘焙进顶点；仅勾选 Apply Modifiers 也不要当成适用于所有绑定的保证。重新导入导出的 GLB，确认姿势，才算完成。

Geometry Nodes 的实例、毛发、复杂约束、链接库角色可能需要额外实现实例或烘焙。若上述转换后形状变化明显，保留工作文件，不要连续尝试破坏性操作；这类文件需要按实际节点/绑定单独处理。

## 7. 最小可用材质：先把颜色和透明做对

### 7.1 一个材质槽一个材质槽地处理

1. 选中网格，切到顶部 **Shading** 工作区。
2. 在下方 Shader Editor 中找到材质选择框。一个网格可能有多个材质槽，身体和睫毛未必是同一个材质。
3. 若材质使用复杂 MMD 节点，先保存工作副本；通过 `Shift + A → Shader → Principled BSDF` 添加标准节点。
4. 让 `Principled BSDF` 的 BSDF 输出连接到 `Material Output` 的 Surface 输入。材质输出一次只能接一条主 Surface；连接新的输出会替换旧的连接。
5. 找到正确的 `Image Texture` 节点，或通过 `Shift + A → Texture → Image Texture` 新增并 Open 对应图片。不要把所有材质都指定成身体贴图。
6. 图片的 Color 输出连接 Principled 的 **Base Color**。身体、脸、衣服颜色贴图使用 **sRGB**。没有颜色贴图时直接调整 Base Color。
7. 对皮肤、布料等先设 **Metallic = 0**、**Roughness = 0.5–0.8**，这是本项目的起始调试建议，不是所有模型的物理测量值。

标准 glTF 材质支持基础颜色、金属度/粗糙度等字段，但不支持原样搬运任意 Blender 着色节点。这里的目标是稳定显示静态模型，而不是复刻作者的 Cycles/EEVEE/MMD 最终渲染。[Blender glTF 材质说明](https://docs.blender.org/manual/en/4.3/addons/import_export/scene_gltf2.html)

### 7.2 睫毛、头发卡片、半透明衣物

1. 确认图片真的含有 Alpha；黑背景不一定代表透明，JPEG 本身没有 Alpha。
2. 图片的 **Alpha** 输出连接 Principled 的 **Alpha** 输入。
3. 在 Material Properties 的设置中选择适当的透明渲染方式。Blender 4.2+ 常见名称为 **Render Method**；旧教程里的 **Blend Mode / Alpha Clip** 不一定在同一位置，不能照名称硬找。
4. 优先用普通 Alpha Blended 导出半透明材质。纯镂空材质可使用与导出器兼容的阈值方案导出为 glTF `MASK`，随后检查导出结果的 `alphaMode` 和 `alphaCutoff`；不要认为 Blender 的 Dithered 必然等于 glTF MASK。
5. 对需要正反面都看见的薄片，关闭材质的背面剔除（Backface Culling），导出后检查是否得到 `doubleSided: true`。

本项目 `MASK` 会按阈值裁剪颜色和阴影；`BLEND` 会在不透明物体之后由前向路径合成，关闭深度写入。BLEND 按网格中心排序，不是逐三角形排序，因此交叉头发或多层透明衣物仍可能有排序瑕疵。BLEND 暂不投射半透明阴影；需要稳定镂空阴影时优先使用 MASK。建议开启后处理，以在线性光照结果上混合，再统一映射到屏幕颜色。

### 7.3 法线与 PBR 贴图（可后做）

颜色正确后再加细节。法线图设为 **Non-Color**，经 **Normal Map** 节点接 Principled 的 Normal；不能直接接 Base Color。本项目使用 UV0，导出时把需要的 UV 作为第一套 UV。hpRenderer 默认启用导入的法线贴图；可用对应对象的 `useNormal` 开关关闭。

粗糙度、金属度、AO 属于数据贴图，应为 Non-Color。glTF 的 Metallic-Roughness 图使用 **G = Roughness、B = Metallic**；AO 使用其指定贴图的 R，不要求它与 MR 是同一张图片。本次已分别处理，不再把任意单通道贴图误当 ARM。

本次未实现 Emission、Unlit、Transmission、Clearcoat、复杂纹理变换、UV1、顶点色、Toon、描边和球形贴图；这些材质可能只保留基础 PBR 部分。需要还原复杂节点外观时可另做烘焙工作，本手册不把“直接导出”描述成无损转换。

## 8. 导出为 GLB

1. 保存静态工作副本。
2. 在 Object Mode 中选中所有准备导出的网格。多部件角色要一起选；不要误选地板、相机、灯光、刚体和第二份身体。
3. `File → Export → glTF 2.0 (.glb/.gltf)`。
4. File Format 选择 **glTF Binary (.glb)**。它便于把几何与普通贴图一起交付，避免漏带文件。[Blender glTF 格式说明](https://docs.blender.org/manual/en/4.0/addons/import_export/scene_gltf2.html)
5. Include 中勾选 **Selected Objects**；不导出 Cameras / Punctual Lights。
6. 保留 **+Y Up** 默认转换；不要再额外手动旋转一次来“适配 OpenGL”。
7. Mesh 数据保留 **UVs、Normals**；有法线图时导出 Tangents。普通静态模型可启用 Apply Modifiers；角色应先完成第 6 节，并检查形状。
8. 关闭 **Animations、Skinning、Shape Keys / Morph Targets** 等本次不使用的数据。部分选项仅在相关对象存在时显示。
9. 导出 Materials。贴图用 **PNG/JPEG**，含透明的颜色图用 PNG。先不要开启 Draco、Meshopt、KTX2/BasisU/WebP 等压缩或扩展选项。
10. 保存为 `character_static.glb`。

导出窗口的设置组位置在不同版本中可能变化，以上按功能识别，不要求每个小版本完全一致。若要排查贴图或 alphaMode，可暂选 **glTF Separate** 输出 `.gltf + .bin + 贴图`，用文本编辑器查看 glTF；不要只把其中的 `.gltf` 移走。

### 必做：回读检查

新开一个 Blender 文件，用 `File → Import → glTF 2.0` 重新导入刚导出的 GLB，检查：身体、衣服、头发、眼睛是否齐全，姿势是否固定，颜色和透明是否正常。

如果回读到 Blender 就已经错误，先修转换/导出；如果回读正确而 hpRenderer 不正确，再排查渲染器目前支持范围。这一步能把问题缩小到一边。

## 9. 在 hpRenderer 中加载

1. 把导出结果放在自己的资产目录，例如项目的 `assets/models/角色名/` 下；避免提交作者不允许再分发的模型。
2. 运行程序，找到 **Reload Assets** 窗口。
3. 第一次建议点 **Add Model...**，保留原场景方便对比；要替换现有对象才点 **Replace Model...**。
4. 点 **Browse Model...**，选择 GLB、glTF、OBJ、PMX 或 FBX 并确认。扩展名筛选不区分大小写，中文目录和文件名按 UTF-8 处理。当前对话框确认后会执行添加/替换；单独填写 Model Path 不会自动执行。
5. 在 **Renderer Settings → Select Object** 选中刚添加的对象，检查 Transform 的 Position、Rotation、Scale。新模型尺度可能与原场景完全不同；先确认位置在视野前，再统一缩放三个轴，不要直接设为 0。
6. 若模型发黑，先检查灯光/HDR 是否启用，再看 Console 有没有 `Missing texture`、`Texture unavailable` 等提示；缺贴图时现在会使用基础材质值，不再无条件采样空纹理。
7. `useNormal` 默认开启。对象上的 ao/roughness/metallic Bias 是在导入材质值之上做调整，初次检查保持 0。基础色、透明模式、双面和贴图由模型导入材质决定，编辑器不再提供逐材质槽编辑。
8. **Reload Selected Model** 会重读网格、节点、材质及 GLB 内嵌图，并在下次 GPU 上传时刷新引用的外部贴图。失败保留旧模型；外部贴图刷新失败时保留已有有效缓存。成功重载更新模型导入材质，保留对象的 bias 调节。

## 10. 排错速查

| 现象 | 优先检查 |
| --- | --- |
| 找不到 .blend 导入入口 | 正常：先在 Blender 打开并导出 GLB |
| PMX 菜单没出现 | MMD Tools 是否安装、启用，是否与 Blender 版本匹配 |
| 导入后什么都没有 | 是否完整解压；Console 是否报错；模型是否太大/太小或在相机背后 |
| 模型拆开、零件错位 | 先检查导出回读结果；本次渲染已累乘节点父子变换，不需要手动合并所有零件 |
| 颜色全白/灰 | 当前是不是 Solid 预览；Image Texture 是否连接到 Principled Base Color |
| 粉色 | 外部贴图丢失，先 Find Missing Files |
| 睫毛变成黑片 | 图片是否含 Alpha；是否连接 Alpha；导出是否为 OPAQUE |
| 衣服或头发从背面消失 | 材质是否允许双面；网格法线是否反向 |
| 姿势退回展开手臂 | 没有烘焙成静态网格；关闭 Animation 不等于固定姿势 |
| 人物太亮/像金属 | 检查 Metallic、贴图颜色空间、灯光和 HDR；不要用 Bias 掩盖贴图接错 |
| 头发前后关系偶尔不对 | 可能是 BLEND 网格排序限制；镂空材质优先 MASK，或拆分交叉透明网格 |
| 与模之屋预览图不一样 | 作者可能用了 Toon、描边、特殊灯光、后期；静态 PBR 导入不等于复刻预览渲染 |

## 11. 完成标准和保留文件

- [ ] 原始下载包、说明和带骨架源文件仍然保留。
- [ ] 导出 GLB 回读 Blender 后，零件、姿势、贴图和透明基本正确。
- [ ] hpRenderer 前向/延迟模式均能看到模型，没有持续 GL/导入错误。
- [ ] 模型比例和方向合理，正反面符合材质设置。
- [ ] 当前只验收静态显示，没有把动作、表情、Toon 或物理效果算入完成范围。

未来做骨骼动画时，应回到保留的带绑定源文件重新导出，而不是尝试从静态 GLB 反推出原骨架和权重。

说明：手册依据官方文档、维护者说明和本项目实际入口编写；没有代替你在某个具体下载角色上逐步操作 Blender。遇到不同版本的按钮名称或特殊绑定，请保留原件并以对应版本界面为准。
