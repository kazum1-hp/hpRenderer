# 仓库与分发策略

## 首要目标：评审者拿到项目就能运行

求职作品首先需要可复现、可浏览、可运行，而不是单纯追求最小仓库。
当前普通 Git clone / 源码 ZIP 包含第三方源码、shaders 和默认场景全部资源。
安装好 Visual Studio 2022 C++ 工具链、Windows SDK、CMake 3.22+ 后，配置构建
不需要下载依赖。运行需要支持 OpenGL 3.3 的显卡驱动。

默认演示保留胸像及配套纹理、Newport Loft HDR 和可选地面的砖纹理，9 个文件
约 **9.32 MiB**。渲染功能没有裁剪。其他旧模型和 HDR **仍在仓库中**，可通过
编辑器载入；`HPRENDERER_FULL_DEMO=ON` 恢复原来的三个模型启动场景，并将
`cmake/DemoAssets.cmake` 明确列出的额外演示资源加入构建目录和分发包。

## 为什么没有直接删除资源、迁移 LFS 或 FetchContent

整理前，本地 HEAD 跟踪的文件总量约为：assets **417.66 MiB**、third_party
**251.23 MiB**、docs **15.99 MiB**。本地 `git count-objects -vH` 的 loose objects
约 **1017.84 MiB**；这不是 GitHub 的压缩下载体积，也不是最终运行包大小。

删除当前文件不会自动删除 Git 历史中的大对象。迁移已有文件到 LFS 涉及历史、
LFS 客户端/下载和托管额度；改写公开历史还会影响旧 clone、分支和链接。因此本次
不删除素材、不改写历史、不执行 LFS migration、不强推。浅克隆可以减少历史下载：

```powershell
git clone --depth 1 https://github.com/kazum1-hp/hpRenderer.git
```

浅克隆仍包含当前版本全部文件，并不会免去当前的大素材。以后确需缩减源码下载时，
先核对资产来源和使用情况，制作并验证完整备份/资源包，再单独决定是否移除可选素材
以及是否迁移历史；默认演示和测试必需文件应继续自包含。

第三方依赖暂时保留 vendored 方式：Assimp 和 ImGuiFileDialog 有本地修复，模型
回归测试依赖 Assimp 的 FBX/glTF 测试素材。盲目替换成上游下载会丢失这些内容。
将来可以逐库迁移，固定 commit/hash、保留补丁和许可证、测试通过，并继续提供
完整源代码快照。参见 [依赖版本与补丁记录](../third_party/README.md)。

参考：[Git LFS 迁移说明](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-migrate.adoc)、
[CMake FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)。

## 构建、测试和打包

在仓库根目录打开 PowerShell：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
.\build\Release\hpRenderer.exe
```

完整启动场景：重新配置时增加 `-DHPRENDERER_FULL_DEMO=ON`，然后重新构建。
切回轻量模式使用 `OFF`。旧 build 中的多余素材不会被自动删除，以免误删用户文件；
正式发布到一个新的 staging 目录，CPack 按安装白名单生成 ZIP，不遍历旧 build。

```powershell
cmake --install build --config Release --component Runtime --prefix dist/staged
.\build\Release\hpRenderer_startup_asset_tests.exe dist/staged/assets
cmake -DBUNDLE_DIR=dist/staged -P cmake/ValidateRuntimeBundle.cmake
cpack --config build/CPackConfig.cmake -C Release -B dist
```

ZIP 位于 `dist/hpRenderer-windows-x64.zip`。其中包含 `bin/hpRenderer.exe`、MSVC
运行库、assets、shaders、文档和第三方声明。应完整解压，不能单独拷贝 exe。
标准构建将项目依赖静态链接；系统显卡驱动仍由运行机器提供。

`startup_assets` 测试实际导入模型、解析外部纹理引用并解码纹理/HDR，同时验证从
不同工作目录恢复到程序目录。它也可针对安装后的 assets 执行，避免“源码有资源，
发布包漏文件”。文件验证脚本另检查 shader 和运行库，但不代替实际 GL 渲染测试。

`.github/workflows/windows.yml` 在 Windows 2022 runner 上构建 Debug / Release、
执行 CPU 测试，并上传 Release ZIP artifact。它不会自动发布 GitHub Release。
GPU 测试程序也会编译，但标准托管 runner 不保证 OpenGL 3.3 驱动，所以实际 GPU
测试在本地通过 `-DHPRENDERER_GPU_TESTS=ON` 开启。不能把 CI 通过等同于画面正确。

正式发布前还需要手动检查：首次打开的相机视角、Forward/Deferred、IBL/六图天空盒、
阴影、透明材质、resize、中文路径、输入捕获，以及干净机器/新目录中的 ZIP 启动。
Release 标题应标明对应 tag；测试完成后再手动上传 ZIP，保留源码与资源一致性。

## 代码维护与资产来源

- 项目和 ImGui 的 C++ 源文件使用显式 CMake 列表；IDE 展示用 headers/shaders
  以及许可证清单的 glob 带 `CONFIGURE_DEPENDS`，新增文件会触发重新配置。
- `.clang-format` 定义 C++17、4 空格、120 列等规则。本次不全库格式化，以免产生
  与行为无关的大 diff；后续只格式化修改过的第一方文件，不批量修改第三方源码。
- 新下载的个人模型放在被忽略的 `assets/local/`。演示资源采用明确打包白名单，
  不会因为某人临时下载模型就自动把它放进 Release。
- 项目 MIT 许可证只覆盖项目自身代码，不会自动覆盖第三方库、模型、HDR 或图片。
  原 README 标注部分素材来自 Poly Haven，并参考 LearnOpenGL；已有文件尚缺逐项
  来源记录。正式对外发布前，应补齐下载页面、作者、许可证和必要署名，不能把
  模之屋等第三方角色素材默认为可再分发。当前打包名单没有加入用户外部下载目录。
- [Poly Haven 许可说明](https://polyhaven.com/license)可用于核对确实来自该站的资产；
  它不能作为全部现有文件的来源证明。

优先补充的是资源 provenance 清单和自动启动/画面 smoke test，而不是跨 API RHI。
当前模块依赖、pass 顺序和资源生命周期见 [architecture.md](architecture.md)。
