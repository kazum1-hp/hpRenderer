# hpRenderer — Windows runtime package

Extract the entire ZIP, then open `bin/hpRenderer.exe`. Keep `bin/`, `assets/`,
`shaders/`, `docs/`, and `licenses/` together. No model download, Git LFS, or
development tools are required to run this package. An OpenGL 3.3-capable Windows
graphics driver is required; bundled DLLs do not replace the graphics driver.

The compact package includes the marble bust, its textures, the Newport Loft HDR,
and the brick-plane textures. Full-demo packages contain the original three-model
scene and the repository's additional demo resources.

Move inside the Scene viewport with W/A/S/D and Q/E; mouse look controls rotation.
Hold Left Alt to use the editor cursor. Space resets the camera. 1/2 toggle lights.
Escape exits. Text editing captures keyboard input.

Project source, build instructions, screenshots, and future releases:
[hpRenderer on GitHub](https://github.com/kazum1-hp/hpRenderer).
See `docs/architecture.md` for the implementation and `docs/repository.md` for
packaging/dependency policy. Project code and third-party resources have separate
licenses; see `LICENSE`, `licenses/`, and the asset notes in `docs/repository.md`.
