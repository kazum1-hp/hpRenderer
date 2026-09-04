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
