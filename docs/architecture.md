# Current architecture

This describes the current implementation, not a proposed Vulkan-style RHI.
The [stage-by-stage record](refactor_baseline.md) contains historical names.

## Module dependencies

Arrows below mean **depends on**. CMake enforces these target boundaries.

```mermaid
flowchart TD
    App[Application / executable] --> Editor[hpRenderer_editor]
    Editor --> Core[hpRenderer_core]
    Editor --> UI[ImGui / ImGuiFileDialog]
    Core --> GL[hpRenderer_opengl]
    Core --> GLFW[GLFW]
    GL --> Import[hpRenderer_import]
    GL --> GLAD[GLAD / OpenGL]
    Import --> Assimp[Assimp / GLM / stb_image]
```

- `app/`: creates services, chooses the initial scene, drives update/render/editor,
  and defines shutdown order. The executable resolves its own directory before
  opening relative shader/asset paths.
- `assets/`: `AssimpModelImporter` produces owned CPU `ModelAsset` data;
  `ImageData` decodes pixels without GL. `Model` retains data and a revision.
  Node hierarchy/transforms, mesh instancing, material factors and image references
  survive import. Animation playback is not implemented.
- `scene/`: objects, transforms, lights and CPU asset references. There are no
  ImGui widgets or raw cubemap ownership in scene data.
- `renderer/`: extracts `RenderScene`, uploads/caches models, manages IBL,
  render targets and concrete passes. `Renderer::render` receives scene/camera/
  settings/frame inputs and returns a texture view; it stores no Scene or Window.
- `renderer/opengl/`: concrete GL mesh/texture/shader/framebuffer/skybox wrappers.
- `editor/`: panels, file dialogs, console capture and ImGui lifecycle. Editor
  capture state is explicitly passed to input; the Scene viewport allows camera
  and light shortcuts while text fields suppress them.

`AssetManager` is Application-owned, not a singleton. It still caches GL textures
and shaders as well as CPU models; **it is not yet a wholly CPU-only asset service**.
`hpRenderer_import` is the genuinely GL-free boundary. Texture keys include the
normalized filesystem path, semantic and color space. Shaders use `ShaderId`.

## Per-frame pipeline

```mermaid
flowchart TD
    Scene[Scene + camera + settings] --> Extract[Render extraction]
    Extract --> Prepare[Model GPU cache + environment preparation]
    Prepare --> Shadow[Optional directional / point shadows]
    Shadow --> Mode{Rendering path}
    Mode -->|Forward| Forward[Opaque / masked forward shading]
    Mode -->|Deferred| GBuffer[G-buffer: opaque / masked]
    GBuffer --> Lighting[Deferred lighting + depth copy]
    Forward --> Markers[Optional light markers]
    Lighting --> Markers
    Markers --> Sky[IBL skybox / six-face background / none]
    Sky --> Alpha[Sorted forward transparency]
    Alpha --> Debug[Optional deferred G-buffer overlay]
    Debug --> Bloom[Optional bloom blur when post enabled]
    Bloom --> Tone[Display output: tone mapping / gamma + optional effects]
    Tone --> View[Editor Scene viewport]
```

The order follows `RenderPipeline::render`. Transparent materials are composited
after the skybox in **both** paths, using opaque depth and no transparent depth
writes. Scene shading, skyboxes, light markers and transparent blending all remain
in linear HDR. Both paths always finish with display conversion. With post disabled,
the output uses Reinhard tone mapping and gamma 2.2 at exposure 1; bloom, image
effects and saved custom settings are ignored. With post enabled, the configured
tone mapper, exposure, bloom and effects apply (`useHdr=false` skips tone mapping,
but still applies gamma). Deferred rendering does not force the post switch on.
G-buffer diagnostic overlays retain their position before display conversion;
their displayed values are therefore affected by that conversion and enabled effects.
Final output and bloom targets have no depth/stencil attachments; scene targets
retain depth for geometry, skyboxes and transparency. The returned
`RenderOutput` is a non-owning view, not a transferable texture allocation.

### Transparency limitations

Blended draws are stably sorted back-to-front by their transformed mesh center in
camera space, across objects and node instances. They depth-test against opaque
geometry but do not write depth. This is an approximation: intersecting meshes,
intersecting triangles within one mesh, cyclic overlap and large meshes whose
centers do not represent their visible surfaces can blend incorrectly. There is
no per-triangle sorting or order-independent transparency (OIT). Masked materials
use alpha cutoff and participate in opaque depth/shadow rendering instead.
Consider mesh splitting or OIT only when a scene requires better transparency.

IBL precompute runs on demand, not as an unconditional per-frame pass. It generates
the environment cubemap, irradiance map, prefiltered map and BRDF LUT. Selecting
six images supplies a background cubemap only and disables IBL illumination; it
does not secretly bake six-image lighting. Faces use `+X -X +Y -Y +Z -Z`, decoded
without vertical flipping. Named-face matching is explicit in the asset panel.

## Ownership and lifetime

| Owner | Resources | Validity / invalidation |
| --- | --- | --- |
| Application / Window | Current GLFW GL context | Must outlive every GPU owner |
| Scene / Model | CPU objects, material bias, imported data, environment selection | Independent of ImGui and GL handles |
| AssetManager | CPU model cache, GL texture/shader caches | Clear after render/editor borrowers |
| ModelGpuCache (pipeline) | Uploaded models/meshes | Weak CPU model identity + model revision; expired entries removed |
| IBLCache (renderer) | Baked environment resources | Source identity/revision and bake shader programs; expired entries removed |
| RenderTargets (pipeline) | HDR, G-buffer, lighting, final, bloom and shadow targets | Viewport resize reallocates viewport targets; shadows have separate sizes |
| EditorLayer | ImGui context/backends, panel state | Shutdown before GL context destruction |

Normal shutdown is explicit: **Editor → Renderer → Scene.Clear → AssetManager.Clear
→ Window/context**. Member declaration order also keeps the Window alive during
constructor unwinding. GPU wrappers release their handles while a context exists.
Failed asset reloads preserve previous usable data; framebuffer resize failure
preserves the old valid extent. Do not retain `RenderOutput` across resize/shutdown.

Imported PBR factors/textures remain the default material. Per-object controls are
AO, roughness and metallic **bias**, plus normal-map enable/disable; there is no
Phong material system or editable material-slot override layer.

### Asset cache policy

The current demo retains successfully loaded models, textures and shaders through
strong references in `AssetManager` until explicit `Clear()` (normally shutdown),
or replacement of a cache entry during reload. Removing a Scene object only drops
that object's reference: it does **not** promise CPU memory or GPU memory reclamation.
`ModelGpuCache` removes entries whose CPU model identity has expired during its
next `prepare()`; AssetManager-owned models normally cannot expire while cached.
Loading many distinct large assets can therefore accumulate memory. Environment
asset identities use weak references, but cached HDR textures still follow the
texture retention policy.

This is intentional for the small demo: repeated imports reuse resources. There
is currently no automatic eviction, memory budget or unload-on-object-delete.
`Clear()` drops cache references, not references held by other consumers, and is
not a scene-unload protocol. Release GPU owners only with the GL context current.

When scene switching or repeated large-model browsing requires reclamation, add
an explicit unused-resource sweep: drop unused CPU model cache entries, prune
expired GPU models, then remove unused cached textures after GPU material holders
release them. Render snapshots and other borrowers must be accounted for. Add
capacity budgets/LRU only when measured asset workloads justify them.

## Remaining boundaries worth improving

1. Separate CPU asset catalog and GL texture/shader cache when another consumer
   actually needs it; retain the small `TextureLoader` boundary meanwhile.
2. Replace the simple executable-relative working directory convention with an
   explicit asset root if command-line scene paths or multiple projects are added.
3. Add scene serialization, stable entity IDs and asset provenance metadata before
   a large editor expansion; keep save/load independent of GPU state.
4. Profile, then add visibility culling and measured optimizations. A render graph,
   cross-API RHI, skeletal animation and asynchronous upload are not implemented.

See [repository/build policy](repository.md) and the existing
[manual smoke matrix](refactor_baseline.md) for verification.
