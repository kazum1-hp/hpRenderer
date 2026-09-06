# Renderer Refactor Baseline

This document defines the behavior that must remain stable while the renderer
and editor are separated. Run the automated test and complete the manual smoke
matrix after every refactor stage.

## Automated baseline

Configure and build with tests enabled:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The `scene_behavior` test does not create a window or OpenGL context. It checks:

- object add/remove behavior;
- preservation of an object's transform and material association;
- enforcement of `RenderLimits::MaxPointLights`;
- cleanup of scene-owned objects, lights, and environment selection.

## Framebuffer checks (stage 2)

`framebuffer_descriptors` validates attachment configurations without a GL context.
To include real OpenGL checks on a machine with an OpenGL 3.3-capable driver:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON -DHPRENDERER_GPU_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The opt-in `framebuffer_gpu` test creates a hidden GLFW context and checks actual
attachment formats/filtering, framebuffer completeness, MRT readback, depth blit,
viewport resizing, fixed shadow allocations, move ownership and resource release.
All GPU objects are destroyed before its context is closed. These checks do not
replace the full-scene visual/interaction matrix below.

The stage 2 target definitions preserve the previous allocations:

| Target | Color attachments | Depth attachment |
| --- | --- | --- |
| HDR / deferred lighting | Two RGBA16F, linear filtering | Depth24Stencil8 renderbuffer |
| G-buffer | Five RGBA16F, nearest filtering | Depth24 texture |
| Final output | One RGB8, linear filtering | Depth24Stencil8 renderbuffer |
| Bloom ping-pong (each) | One RGBA16F, linear filtering | Depth24 texture (preserved legacy allocation) |
| Directional shadow | None | Depth texture, clamp-to-border, white border |
| Point shadow (each) | None | Layered depth cubemap, clamp-to-edge |

Viewport targets follow the viewport size; shadows retain their 1024 x 1024 size.
The descriptor also permits genuinely color-only targets by omitting `depth`.
Its optional MSAA path is limited to one RGB8 color attachment at 4 samples with
an optional depth/stencil renderbuffer; this is a wrapper restriction, not an
OpenGL restriction. The current pipeline continues to allocate single-sample targets.
Invalid descriptions or incomplete FBOs throw an error containing the target name.
Viewport resize allocates the entire new set before replacing the old one; failure
keeps the old set and is logged. This briefly requires memory for both allocations.

## Editor separation (stage 3)

The application now composes two build targets: `hpRenderer_core` (no ImGui or
file-dialog dependency) and `hpRenderer_editor`. `Application` drives
`EditorLayer`, which owns the ImGui context/backends, docking state and six panels:
SceneViewport, Inspector, Lighting, RenderSettings, Asset and Console.
Inspector and RenderSettings still share the existing "Renderer Settings" window;
splitting classes does not require changing the user's window layout.

- Transform and Light expose data access/reset methods, not UI drawing methods.
- InputManager accepts plain `InputCaptureState`; it never queries ImGui.
- The viewport consumes `RenderOutput` and requests an extent. Application applies
  it before the next render, so the displayed texture has been rendered before use.
- AssetPanel owns paths, selection, dialogs and reload messages. After successful
  shader reloads, Application's callback asks Renderer to restore sampler bindings.
  Initialization and reload now use the same binding setup.
- Debug-label parsing belongs to SceneViewportPanel, not Renderer.
- ConsoleCapture restores stdout/stderr stream buffers when its owning instance
  is destroyed. Capture starts with EditorLayer construction, including asset-load
  diagnostics; it is main-thread-only, not an asynchronous logging service.

Additional automated checks:

- `runtime_boundary`: includes runtime headers without ImGui linkage, rejects a
  transitive ImGui include, and checks transform/light editing, mouse/scroll capture,
  movement-speed access, console clearing and nested stream restoration.
- `editor_panels`: builds real ImGui frames without a GL context; checks preserved
  window names, empty/deleted objects, viewport size requests, and activation of
  Reset, shadow, light-enabled and model-delete controls.
- `editor_gpu` (under `HPRENDERER_GPU_TESTS`): draws a real texture through the editor
  backends in a hidden window, checks GL errors, repeats initialization/shutdown and
  verifies that the editor does not own/delete the renderer's output texture.

Intentional small behavior corrections in this stage:

- Text entry and active UI controls suppress camera movement/reset and light hotkeys.
  Otherwise the hovered Scene viewport allows these keys even when ImGui keyboard
  navigation requests capture. Outside Scene, ImGui keyboard capture is respected.
  Escape keeps its existing application-close behavior.
- Non-positive/collapsed viewport sizes do not trigger framebuffer allocation.
- The dialog dock node is retained between frames, and docking hints are issued only
  for open dialogs so they cannot affect an unrelated window.
- Move Speed remains editable even with no model objects.

Debug build and all six locally enabled CTest checks passed. These tests do not
verify full-scene visual parity or interactive model/HDR loading, failed shader
compilation, camera controls, detached viewport interactions and every settings
combination. Those affected smoke-matrix rows still need manual acceptance.
Stage 3 left environment GPU ownership for stage 4 below. Renderer's external
references and internal render passes remain for subsequent stages.

## Environment ownership and IBL (stage 4)

Scene now stores a shared, read-only `EnvironmentAsset` selection containing only
an HDR path and revision. No environment texture names or generation flags live in
Scene. This specifically decouples environment ownership; Scene's models and other
existing GPU-backed data are not made backend-independent by this stage.

- ResourceManager owns loaded HDR textures. `LoadEnvironment` shares metadata for
  an exact path; `ReloadEnvironment` replaces the source and advances its revision
  only on successful loading. A failed reload leaves the selection, source and
  revision unchanged. This is a lightweight path-based asset reference, not a new
  generic handle registry or path-alias canonicalization system.
- Renderer owns `IBLCache`. Entries use weak asset identities and track revisions,
  source texture object identity and baking shader program IDs. An unchanged input
  reuses its baked maps; scenes sharing a selection share a cache entry. Distinct
  Renderer instances still have distinct caches.
- `IBLPrecompute` owns the four baking shaders and generates the cubemap, irradiance
  and prefiltered environment. Defaults remain 1024 / 32 / 128, with five prefilter
  mip levels. The 512 x 512 BRDF LUT is shared across environments in one cache and
  regenerated when its shader program changes.
- Replacement is transactional: bake a complete resource set before releasing the
  previous set. Each framebuffer attachment is checked. Failure retains that
  asset's previous maps and is retried after an input/program change. A failed
  first bake returns an empty view, never another scene's environment.
- Temporary capture framebuffer/renderbuffer objects and partially baked textures
  use RAII. Baking restores the GL state it changes, including framebuffer/program/
  VAO bindings, texture unit zero, viewport, write masks and render enable flags.
- Discarded selections are collected on the next cache prepare call; shared maps
  remain while another selection references the asset. Renderer shutdown clears
  all cached GPU resources while its GL context is still current. CPU environment
  metadata may outlive that context. ResourceManager retains its existing source
  texture caching policy until Clear; this stage does not add texture-cache eviction.

Application and AssetPanel use the environment-loading API instead of assigning
GLuints or setting dirty flags. The renderer consumes a non-owning GPU view for the
current frame; scene clearing no longer needs renderer-side deletion of Scene fields.
The previous Scene shortcut-capture fix remains unchanged.

Additional acceptance checks:

- `environment_asset` compiles and runs using only the CPU metadata header, with no
  OpenGL/ImGui includes or libraries.
- `ibl_gpu` (opt-in under `HPRENDERER_GPU_TESTS`) uses small temporary HDR fixtures
  and the real baking shaders. It reads back all environment faces, irradiance,
  prefilter mips and BRDF pixels; checks GL state restoration, cache/LUT reuse,
  source reload, failed reload/bake fallback, recovery, shader-driven LUT refresh,
  unused-entry collection and shutdown. Temporary fixtures, not project assets or
  shaders, are modified during testing.
- Runtime tests also protect the original production IBL resolutions.

These automated checks use reduced baking dimensions for speed; full-resolution
visual parity, editor-driven HDR switching and the full-scene smoke matrix below
still require manual acceptance.

## Explicit render submissions (stage 5)

Renderer has one read-only drawing entry point:

```cpp
RenderOutput render(const RenderScene& scene, const CameraData& camera,
    const RenderSettings& settings, const RenderFrameData& frame);
```

It no longer stores Scene, Camera, Window or InputManager references, owns editor
settings, reads GLFW time/keys, installs window callbacks or changes camera aspect.
Construction performs no GL allocation. The host explicitly initializes it with a
ResourceManager and target extent while a GL context is current. Drawing, resizing
and resource destruction still require that context; there is no backend switch in
this stage.

- `BuildRenderScene` copies object transforms/material overrides and lighting data
  into a submission. Model geometry, base materials and environment metadata remain
  shared assets. This is a synchronous value snapshot of per-scene state, not an
  ECS, a deep copy of GPU assets or a thread-safe hot-reload snapshot.
- The source Scene may be edited or destroyed without changing the copied values.
  A held snapshot keeps its shared assets alive; release GPU-backed snapshots before
  destroying their GL context. Renderer does not retain the submitted snapshot.
- `BuildCameraData` computes projection from the actual target extent without
  modifying Camera. Two submissions may use different cameras or aspect ratios.
- Application owns RenderSettings and supplies current time plus directional/point
  light switches through RenderFrameData. The Scene shortcut-capture policy stays
  unchanged. Passes receive a stack-local FrameContext rather than retained inputs.
- Deferred rendering still uses the post-processing path. Application preserves
  the editor's existing sticky `usePost` toggle when deferred is active, rather than
  letting Renderer silently modify caller settings.
- `resize` returns the actual allocation size. A non-positive request is ignored;
  allocation failure retains/reports the previous size, so camera projection remains
  consistent with the surviving targets.
- RenderOutput is a borrowed texture reference: resize/shutdown invalidates it,
  and later renders can overwrite its pixels. Rendering two scenes sequentially is
  supported; retaining two independent preview images requires separate target sets
  (or separate Renderer instances), not just keeping two RenderOutput structs.

New checks:

- `render_submission`: public renderer headers do not transitively include GLFW or
  ImGui; Scene destruction preserves copied data; camera extraction respects extent
  without mutation; constructing/shutting down an uninitialized Renderer requires
  no GL context, and drawing before initialization is rejected.
- `renderer_gpu` (opt-in): a hidden context and temporary triangle model exercise
  forward/deferred rendering, shadows, bloom, camera/light uniform propagation,
  consecutive submissions of different scenes/settings, resize and failed-resize
  fallback, model ownership release, shutdown and reinitialization. It does not
  create the application's Window or InputManager.

Debug build and all ten locally enabled CTest checks passed. Full-scene visual
parity, all editor interactions and production-size HDR/asset combinations still
need the manual matrix. Individual RenderPass extraction remains for stage 6.

## Concrete render pipeline and passes (stage 6)

Renderer is now a lifecycle/submission facade over RenderPipeline and IBLCache.
Its public stage-5 API and borrowed RenderOutput lifetime are unchanged.
RenderPipeline owns RenderTargets, shared drawing primitives, and the concrete
passes in include/Rendering and src/Rendering. Each pass holds only its own shader
references and receives borrowed frame inputs plus explicit input/output targets.
There is no virtual pass framework, render graph, backend abstraction or retained
Scene snapshot.

The execution order is explicit:

1. Optional directional/point ShadowPass.
2. ForwardPass, or GBufferPass followed by DeferredLightingPass and its depth copy.
3. Optional LightMarkerPass, then SkyboxPass (shared by both rendering paths).
4. Optional deferred GBufferDebugPass.
5. Optional BloomPass, then ToneMappingPass when post processing is active.
   Deferred rendering still always uses post processing.

BloomPass returns the last ping-pong texture explicitly. ToneMappingPass consumes
that texture and the scene color without knowing the ping-pong implementation.
DrawHelpers centralizes existing mesh texture binding and model drawing.
Passes establish their framebuffer, viewport and depth-test enable state; they
still run inside the existing OpenGL renderer state contract, not arbitrary
external GL state. Shader reload binding restoration is delegated to the passes
with persistent sampler/attenuation uniforms; other uniforms are set per draw.
The unused debug shader is no longer a required pipeline dependency.

This stage keeps shader algorithms, texture units, five bloom blur iterations,
1024-pixel shadow maps, existing ground-plane shadow behavior and G-buffer debug
layout unchanged. Editor input capture, IBL baking/cache policy and asset loading
are not part of this extraction.

The renderer_gpu check additionally exercises:

- Constant-image bloom and Reinhard/gamma output via pixel readback, with bloom
  enabled and disabled; the final ping-pong target is checked explicitly.
- A red light marker surviving subsequent skybox drawing and tone mapping in
  both forward and deferred paths.
- G-buffer geometry depth copied to the deferred lighting target.
- All forward/deferred, post-process and bloom combinations after shader reload,
  including deliberately stale viewport/depth-test state between submissions.
- Real shader replacement using private temporary copies, followed by sampler
  binding checks. No source shaders or user assets are modified by these tests.

Debug build and all ten locally enabled CTest checks passed after this extraction.
Full-scene visual parity and editor interaction acceptance still require the
manual smoke matrix below; synthetic pixel tests do not replace that acceptance.

## Manual rendering smoke matrix

Use a Debug build and the default assets. For each row, verify that the Scene
viewport continues to render, no framebuffer error is printed, and application
shutdown does not report an OpenGL resource error.

| Area | Action | Expected behavior |
| --- | --- | --- |
| Startup | Launch with the default scene | Three models and the HDR environment load; the editor remains responsive |
| Viewport | Resize the Scene panel, including very small sizes | The image follows the available panel size without stretching stale content or crashing |
| Camera | Hover the Scene panel and use mouse plus Q/W/E/A/S/D | Camera movement affects only the scene view |
| UI capture | Move the pointer over editor controls and drag a value | Camera look does not consume UI interaction |
| Lighting | Press `1`, then `2` | Directional and point lighting toggle independently |
| Light editing | Change directional and point-light color, position/direction, intensity, and enabled state | Forward and deferred output respond to the edited values |
| Forward | Disable `useDeferred` | Scene, environment, object materials, optional plane, light markers, and enabled shadows render correctly |
| Deferred | Enable `useDeferred` | The visible result remains consistent with forward rendering within known path differences |
| G-buffer debug | Enable deferred rendering and `drawDebug` | Normal, roughness, metallic, and depth previews appear with labels |
| Directional shadow | Enable shadows and directional lighting | Directional shadows appear and toggle off cleanly |
| Point shadow | Enable shadows and point lighting | Enabled point lights cast point shadows without exceeding the configured light limit |
| Post process | Toggle `usePost`, HDR, each tone-mapping mode, and exposure | Final output changes without a black frame or stale texture |
| Effects | Select inversion, grayscale, sharpen, and blur; adjust their exposed values | Each effect updates the final output immediately |
| Bloom | Enable light markers, post processing, and bloom; adjust sampler distance | Bright areas blur and combine with the scene without ping-pong artifacts |
| Material overrides | Change AO, roughness, metallic, and normal-map controls for each object | Only the selected object changes |
| Object lifecycle | Add, replace, reload, and delete a model | Selection remains valid and surviving objects retain their own settings |
| HDR lifecycle | Load another HDR, then reload the same path | IBL maps regenerate once per resource change and the new environment is visible |
| Shader reload | Reload all shaders after a valid edit and after an intentional compile error | Success and failure are reported without losing the last usable application state |
| Console | Produce output, clear it, and continue using the app | Output appears once, Clear empties the panel, and logging continues |
| Shutdown | Close with Escape and with the window close button | The process exits normally after releasing renderer resources while the context is valid |

## Stage acceptance rule

A refactor stage is complete only when:

1. the project builds in Debug;
2. `ctest` passes;
3. the smoke-matrix rows affected by the stage pass;
4. no unrelated rendering or editor behavior changes;
5. the stage can be reviewed as a focused change without depending on a later stage.

Any intentional behavior change must be recorded separately instead of silently
updating this baseline during an architectural refactor.
