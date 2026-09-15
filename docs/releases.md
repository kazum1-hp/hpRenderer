# Release milestones

## v1.1 — 架构重构

This release marks the completed architecture refactor and includes all work on
master through the directional-shadow coverage update.

- Separated the editor, scene extraction, CPU assets, OpenGL resources and render passes.
- Improved model import, transparency, skyboxes and resource lifetime handling.
- Added frame statistics, asynchronous GPU pass profiling and configurable ambient light.
- Made directional-shadow coverage follow the camera, include off-camera casters,
  and expose a configurable shadow distance with stable texel alignment.
- Added CPU/GPU regression coverage and a standalone Windows runtime package.

The `v1.1` and `架构重构` tags identify the same commit. Extract the entire Windows
ZIP and run `bin/hpRenderer.exe`; keep the bundled assets, shaders and DLLs.
The compact package includes the default bust demo. Additional tracked demo assets
remain available in the source repository.

## v1.0 — Original architecture

The `pre-refactor` tag identifies the same commit as the existing `v1.0` release
tag. It preserves the released original architecture, rather than the later
pre-refactor test-baseline commit.

## Development after v1.1

Start substantial new features from master on separate branches, for example
`feature/pcss` and `feature/frustum-culling`. Merge each feature into master after
validation. Keep milestone tags fixed so each release remains reproducible.
