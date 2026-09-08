# Static model import boundary

## Ownership and rendering

`AssimpModelImporter -> ModelAsset (CPU) -> Model (CPU revision/snapshot) -> ModelGpuCache -> GpuModel -> rendering passes`.

- `hpRenderer_import` links Assimp/GLM, not OpenGL, GLFW or ImGui. Imports own their vertex/index arrays, materials, image bytes and node hierarchy after the Assimp importer is destroyed.
- Model owns CPU data only. GpuModel owns private GPU meshes with `meshCount()` / `const Mesh& mesh(index)`. The renderer-owned ModelGpuCache shares one upload per Model revision. Each source mesh is uploaded once. Each node reference produces a draw using `objectWorld * nodeWorld`; nodeWorld is accumulated `parentWorld * local`.
- All geometry paths share DrawHelpers, including alpha-tested directional/point shadows. Mirrored transforms temporarily reverse front-face winding; singular transforms are skipped. Normals use inverse-transpose, tangents use the linear model transform.
- Source material factors and texture channels are kept with the asset. Per-object MaterialInstance retains only AO/roughness/metallic biases and the normal-map switch. Source material values are not editor-editable. glTF roughness is G, metallic B; AO has an independent source using R. Separate legacy maps use R. The narrowly scoped old `_rough_` to `_arm_` missing-file compatibility alias is retained for existing project assets, with diagnostics.
- Encoded and raw RGBA embedded images are owned CPU bytes and uploaded without pretending to be disk paths. Embedded texture reuse is local to one Model revision and includes semantic/color policy; file textures still use AssetManager's path/semantic/color-space cache.
- Import/reload failures keep the old CPU/GPU model. Missing individual images log diagnostics and use material defaults instead of invalid texture sampling.
- OPAQUE and MASK enter the forward or GBuffer pass. BLEND draws are collected across objects, sorted by view-space mesh-center depth, then composited forward after opaque geometry and skybox with depth writes disabled. MASK affects shadows; BLEND does not cast a solid shadow. With postprocessing enabled (or deferred rendering), blending occurs on linear HDR lighting before tone mapping. The legacy no-post forward path still blends display-mapped colors.
- PMX/OBJ without explicit alpha modes infer texture transparency from decoded diffuse/opacity images; fully opaque images still use the depth-writing path.

## Deliberate limits

No animation evaluation, skinning, inverse-bind/weight retention, morph evaluation, IK, physics, Toon, outline or sphere-map rendering. The node hierarchy is retained as a useful boundary for future animation work, not a complete skeleton implementation. Use Blender to bake a desired pose into static vertex positions.

This is a basic UV0 metallic/roughness path, not a full glTF conformance implementation. Emission/unlit, vertex colors, secondary UVs, texture transforms/sampler overrides, normal scale/AO strength and advanced material/texture extensions are not currently consumed. Transparent meshes are not triangle-sorted and may exhibit intersecting-surface artifacts. Model reload refreshes embedded and referenced external images on the next GPU preparation; a failed external refresh keeps any existing valid cached texture. Material-slot editing is intentionally disabled; object biases survive model reload.

The old public Model instancing flags and shader-independent `draw()` were removed: they could bypass node transforms and material filtering, and had no call sites outside the old helper. Mesh-level instanced drawing remains available; future model-level batching should instance complete draw submissions with explicit transforms.

## Small vendored Assimp patch

The bundled MMD importer omitted `AI_MATKEY_TWOSIDED`. `third_party/assimp/code/AssetLib/MMD/MMDImporter.cpp` now exports PMX material flag bit 0 to that standard property. This keeps format-specific parsing out of the renderer and avoids incorrectly making every PMX material double-sided. Recheck this patch when upgrading Assimp. The PMX bit mapping can also be seen in the [MMD Tools PMX reader](https://github.com/MMD-Blender/blender_mmd_tools/blob/main/mmd_tools/core/pmx/__init__.py).

## Verification

The file dialog uses `USE_STD_FILESYSTEM` so Windows directory enumeration and
selection preserve UTF-8 through native wide paths. Its metadata lookup also uses
the same conversion (a small vendored ImGuiFileDialog fix to retain on upgrades).
Assimp's default Windows IO already uses `_wfopen` with UTF-8 conversion; Texture
uses `std::filesystem::u8path`. Do not add an ANSI/code-page fallback between them.
The model filter accepts case-insensitive FBX alongside glTF/GLB/OBJ/PMX. FBX
uses the existing static CPU/GPU import pipeline; animation support is unchanged.
Regression tests cover Chinese directories/filenames, external glTF binary files,
texture upload, dialog listing/metadata, and ASCII/Binary FBX import.
On Windows the editor loads an installed Microsoft YaHei/SimSun font through a
native filesystem path, letting ImGui 1.92 populate Chinese glyphs on demand.
No system font is copied into the repository; missing fonts fall back to the
default font with a diagnostic and do not prevent Unicode file access.

`model_import` is a CPU-only target: hierarchical/repeated meshes, matrix conversion, generated normals/tangents, material factors, alpha/double-sided values, independent AO vs packed MR channels, embedded glTF/GLB, transactional failures, .blend guidance and a generated minimal PMX fixture. It does not redistribute a downloaded character model.

`model_import_gpu` checks actual pixels/depth for node instances, mirrored culling, material fallback/factors, alpha mask, double-sided rendering, forward/deferred transparent overlay, state restoration, failed reload and embedded/raw image upload. Existing renderer, editor, IBL, framebuffer and asset-cache tests remain part of CTest.

Run from the project root:

```powershell
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
```

GPU tests require a suitable driver and `-DHPRENDERER_GPU_TESTS=ON` at configure time. This does not certify all third-party PMX files or replace a visual test with the user's chosen model.

For conversion steps, see [Blender beginner manual](blender_model_import_zh.md).
For the reorganized directories, build boundaries, six-image skybox and material editing, see [current architecture](project_architecture_zh.md).
