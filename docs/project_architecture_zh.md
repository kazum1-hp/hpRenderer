# 当前项目结构与使用说明

## 目录与依赖

```text
src/
├── app/                 Application 与程序入口
├── core/                Window、InputManager、输入捕获策略
├── assets/              AssetManager、AssimpModelImporter、Model、ImageData、SkyboxAsset
├── scene/               场景、变换、相机、灯光、每对象材质 bias
├── renderer/
│   ├── passes/          阴影、Forward、GBuffer、光照、Skybox、Bloom、ToneMapping
│   ├── ibl/             HDR 烘焙、IBL 缓存、cubemap 矩阵
│   └── opengl/          Texture、Mesh、GpuModel、FrameBuffer、Shader、Skybox
└── editor/
    ├── layers/          EditorLayer
    ├── panels/          视口、资源、检查器、灯光、设置、控制台
    └── console/         输出捕获
include/hpr/             对应上述模块；统一使用 hpr/... 头文件路径
tests/                   CPU 与隐藏窗口 GPU 回归测试
```

只有头文件的类型在 `include/hpr/` 中，不为凑目录创建空 `.cpp`。
CMake 显式列出源文件，添加实现文件时需同步更新 CMake。

构建依赖：`hpRenderer_import → hpRenderer_opengl → hpRenderer_core → hpRenderer_editor → hpRenderer`。
这里箭头表示后者依赖前者；不是所有对象之间的调用顺序。

- `hpRenderer_import`：Assimp/GLM/stb 解码，不链接 GLAD、GLFW、ImGui。
- `hpRenderer_opengl`：具体 OpenGL 资源，不设计跨 API RHI。
- `hpRenderer_core`：窗口输入、资产管理、场景提取、渲染编排及 passes，不依赖编辑器。
- `hpRenderer_editor`：独占 ImGui 与文件对话框依赖。

## CPU / GPU 边界

`AssimpModelImporter → ModelAsset → Model（CPU 版本容器）→ ModelGpuCache → GpuModel → Mesh/Texture`。

`Model` 可以在无 OpenGL 上下文时加载/重载。导入结果拥有顶点、索引、材质、节点和内嵌图片数据；
`snapshot()` 返回共享的只读数据，重载成功发布新版本，失败保留旧版本。
`Scene` 保存 CPU Model、变换、材质 bias 以及环境资产，不持有 OpenGL 对象。

渲染管线的 `ModelGpuCache` 在提交时按 Model 身份/版本准备 GPU 数据。同一模型被多个对象使用时共享 GPU 网格。
旧提交仍持有对应 CPU 快照及 GPU 数据，不会被新的 CPU 重载破坏。模型重载也触发所引用外部贴图刷新；
刷新失败时优先保留缓存中的有效贴图，没有旧贴图时使用材质默认值。

`ImageData` 负责文件读取和像素解码；`Texture(ImageData, ...)` 与 `Skybox::load(SkyboxAsset)` 负责上传。
文件路径/编码图片构造函数仍是便利入口，内部复用同一 CPU 解码层。上传只应在当前 OpenGL 上下文有效时执行。

`AssetManager` 暂时仍管理共享 Shader/Texture 缓存，属于 Application 生命周期内的资源服务，
并不是完全纯 CPU 的资产数据库。清空这些缓存、Renderer 和 Editor 的 GPU 资源必须早于销毁窗口/上下文。
这一轮把 CPU 导入和 GPU 上传实际分离，但没有引入异步上传、全局 AssetHandle 系统或独立显存预算管理器。

## 环境：默认 IBL，也可使用六张天空盒图片

资源面板的 `Environment` 有三种模式：

1. `HDR / IBL`：默认模式，使用原有 HDR、irradiance、prefilter 和 BRDF LUT。
2. `Six Images (no IBL)`：保留 Skybox 类，加载六图 cubemap，只绘制天空背景，不贡献 IBL 环境光/反射。
3. `Disabled`：关闭天空背景和 IBL，直接灯光仍可工作。

切到六图模式后，通过六个路径输入框或 Browse 选择图片，再按 `Load / Reload Six Images`。
顺序固定为 **+X 右、-X 左、+Y 上、-Y 下、+Z 前、-Z 后**（OpenGL cubemap 面顺序）。
六张图片都必须是同尺寸正方形。不会自动旋转/翻转图片；若接缝/朝向不正确，需要调整素材的面方向。
图片按 sRGB 颜色读取，支持中文路径。任何一张解码失败不会替换已成功载入的 CPU 天空盒；GPU 上传也先准备替代资源再交换。
切换模式保留 HDR 与六图选择，可以直接切回。六图模式下物体失去 IBL 补光属于预期，需要保留或调整直接灯光。

## 材质：使用导入值，仅开放 bias

不使用旧 Phong 材质。基础色、贴图、透明模式、双面等取自导入模型，不再提供逐材质槽编辑。
Inspector 只保留每对象的 aoBias、roughnessBias、metallicBias 和 useNormal。
bias 默认为 0，导入法线贴图默认开启；这些参数跟随对象，不会修改共享模型。
移除了此前的逐槽覆盖字典及对应渲染分支。模型重载更新导入材质，保留对象 bias。

## 后续补充顺序

1. 场景序列化：保存模型路径、Transform、环境模式/路径、对象 bias；先定义格式版本。
2. 材质贴图槽编辑与独立材质资产；配合稳定资产标识解决重载后的匹配问题。
3. 将 AssetManager 中共享的 Shader/Texture GPU 缓存进一步独立；按实际加载卡顿引入后台解码和主线程上传队列。
4. 增加选择、复制和撤销/重做需要时，再考虑 Entity ID/组件化。当前无需为了目标目录引入完整 ECS。
5. 真正需要动画时补骨骼、权重、动画片段和姿态计算；目前是静态模型导入。

保留 OpenGL 封装即可；没有第二个后端需求前不实现 Vulkan/D3D12 风格 RHI。

## 验证

```powershell
cmake -S . -B build -DBUILD_TESTING=ON -DHPRENDERER_GPU_TESTS=ON
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
```

新增 CPU 边界测试无需显卡上下文即可导入模型、解码中文路径天空盒图片，并验证快照和失败重载。
GPU 回归检查六面顺序、sRGB 存储与上传状态恢复、模型缓存共享与更新、外部贴图失败回退、
材质 bias 的实际数值，以及前向/延迟两种管线的 IBL → 六图 → 关闭 → 六图 → IBL 切换。
测试使用临时生成的素材，不修改用户下载的模型。第三方模型的视觉效果仍需用实际素材验收。

### 六面方向排查

六图解码显式使用 top-down 行顺序（stb flip=false），HDR 的翻转策略不会污染后续六图。
不要同时翻转图片、反转 shader 的 Y 和对调 py/ny；应先确认输入约定。
GPU 测试新增非对称渐变图的六方向像素检查，防止纯色测试漏掉面内旋转/镜像错误。
也可只读诊断自己的素材（目录内命名 px/nx/py/ny/pz/nz.png）：

```powershell
.\build\Debug\hpRenderer_renderer_gpu_tests.exe D:\cpp\openGL\Assets\genshin
```

本次该素材的前向/延迟六方向采样检查通过；原方向 py 边缘与四个侧面匹配。
后续截图确认实际 UI 把 px/nx、pz/nz 两组互换，py/ny 保持原位：四个侧面绕 Y 轴转了 180°，
顶面没有跟着转，因此出现顶部云层断开。保持图片和 shader 不变，恢复文件名与正负轴一一对应即可。
资源面板新增 `Match px/nx/py/ny/pz/nz`，显式按文件名整理六个路径；随后点击 Load / Reload 应用。
任意文件名仍可手动指定，不会在加载时偷偷重新排序。该修复不改动原始图片。
