# obs-streamelements-core

Fork of `obs-streamelements-core`, the base StreamElements plugin for OBS.

## English

### About this repository

The main plugin functionality was not developed in this repository. The
original work belongs to the upstream StreamElements project:

- Upstream: <https://github.com/StreamElements/obs-streamelements-core>

This fork exists to concentrate the Linux compatibility, build, and
installation work, together with the adjustments needed to test it more
realistically in that environment.

### Purpose of this fork

The focus of this repository is:

- maintain a useful Linux-oriented branch;
- simplify local plugin builds;
- document installation and testing workflows;
- support validation on both Wayland and X11 sessions.

If you are looking for the original StreamElements functionality inside OBS,
the upstream project remains the main reference. If you need to build, install,
and validate this Linux-oriented variant, this fork and its documentation are
the right place to start.

### Main documentation

The operational guide for this fork is:

- [README-linux-build.md](README-linux-build.md)
- [README-linux-release.md](README-linux-release.md)

That document covers:

- build dependencies;
- usage of the `_scripts/build-linux.sh` script;
- packaging of Linux prebuilt release artifacts;
- integration with the custom `obs-browser` checkout;
- OBS installation paths;
- runtime recommendations;
- the suggested testing and validation flow.

### Quick summary

This repository includes a script to simplify Linux builds:

```bash
./_scripts/build-linux.sh --help
```

For a clean build and installation into the user's OBS plugin directory:

```bash
./_scripts/build-linux.sh --clean --install-user
```

Important:

- this branch depends on custom changes in `obs-browser`;
- the testing workflow and requirements are documented in detail in
  [README-linux-build.md](README-linux-build.md);
- it is not safe to assume upstream Linux behavior matches this fork.

### Credits

- Base project and main functionality: StreamElements.
- Linux build, integration, and validation work in this fork: maintained in
  this repository.

---

## Espanol

### Sobre este repositorio

La funcionalidad principal del plugin no fue desarrollada en este repositorio.
El trabajo original pertenece al proyecto upstream de StreamElements:

- Upstream: <https://github.com/StreamElements/obs-streamelements-core>

Este fork existe para concentrar el trabajo de compatibilidad, compilacion e
instalacion en Linux, junto con los ajustes necesarios para poder probarlo en
ese entorno de forma mas realista.

### Objetivo de este fork

El foco de este repositorio es:

- mantener una rama util para Linux;
- facilitar el build local del plugin;
- documentar el flujo de instalacion y prueba;
- apoyar pruebas en sesiones Wayland y X11.

Si lo que buscas es entender la funcionalidad original de StreamElements dentro
de OBS, la referencia principal sigue siendo el proyecto upstream. Si lo que
necesitas es compilar, instalar y validar esta variante en Linux, este fork y
su documentacion son el punto correcto.

### Documentacion principal

La guia operativa de este fork esta en:

- [README-linux-build.md](README-linux-build.md)
- [README-linux-release.md](README-linux-release.md)

Ese documento concentra:

- dependencias de compilacion;
- uso del script `_scripts/build-linux.sh`;
- empaquetado de artefactos precompilados para Linux;
- integracion con el checkout personalizado de `obs-browser`;
- rutas de instalacion en OBS;
- recomendaciones de runtime;
- flujo sugerido de pruebas y validacion.

### Resumen rapido

Este repo incluye un script para simplificar el build en Linux:

```bash
./_scripts/build-linux.sh --help
```

Para una compilacion limpia e instalacion en el directorio de plugins de OBS
del usuario:

```bash
./_scripts/build-linux.sh --clean --install-user
```

Importante:

- esta rama depende de cambios personalizados en `obs-browser`;
- la forma de probarla y los requisitos estan explicados en detalle en
  [README-linux-build.md](README-linux-build.md);
- no conviene asumir que el comportamiento Linux de upstream sea equivalente al
  de este fork.

### Creditos

- Proyecto base y funcionalidad principal: StreamElements.
- Trabajo de build, integracion y pruebas para Linux en este fork: mantenido en
  este repositorio.

---

## Upstream Project Notes

Core SE.Live OBS plugin.

### Building locally

This plugin does not build on its own. It is built as part of an obs-studio
tree, and it has a hard dependency on the `obs-browser` plugin being present
as a sibling. `StreamElementsBrowserWidget.cpp` includes
`../../obs-browser/panel/browser-panel.hpp` by relative path.

### Layout

```
obs-studio/
|-- CMakeUserPresets.json          <- copy of this repo's CMakeUserPresets.json (Windows)
`-- plugins/
    |-- CMakeLists.txt             <- must contain: add_obs_plugin(obs-streamelements-core)
    |-- obs-browser/               <- required
    `-- obs-streamelements-core/   <- this repo
```

Clone obs-studio with `--recursive` so `obs-browser` comes along. CI pins
obs-studio to tag `31.1.2`; newer trees also work.

### Windows

CMake reads `CMakeUserPresets.json` only from the top-level source directory,
so the preset has to be copied up to the obs-studio root before configuring:

```cmd
copy plugins\obs-streamelements-core\CMakeUserPresets.json CMakeUserPresets.json
cmake --preset=windows-x64-streamelements
msbuild build_x64\obs-studio.sln -p:Configuration=RelWithDebInfo
```

The `windows-x64-streamelements` preset just inherits obs-studio's own
`windows-x64` preset, so configuring with `--preset=windows-x64` works too.

### macOS

`deps/BugSplat.xcframework.zip` must be unpacked first or CMake stops with a
fatal error:

```bash
(cd plugins/obs-streamelements-core/deps && unzip -o BugSplat.xcframework.zip)
cmake --preset=macos -DCMAKE_OSX_ARCHITECTURES=arm64 -B build_macos_arm64
cd build_macos_arm64 && xcodebuild -configuration RelWithDebInfo \
    -scheme obs-streamelements-core \
    -destination "generic/platform=macOS,name=Any Mac"
```

### Notes

- `streamelements/Version.generated.hpp` must define
  `STREAMELEMENTS_PLUGIN_VERSION`. A placeholder is committed so a fresh
  checkout compiles; CI overwrites it with the real build number. Don't edit
  it by hand.
- Don't copy the `-DENABLE_*=OFF` flags out of `.github/workflows/build.yml`.
  CI turns off nearly every OBS feature, including `-DENABLE_UI=OFF`,
  because it only needs the plugin binary as an artifact. For local work you
  want the OBS UI built so there is something to run the plugin in.
- There are no tests. Compiling is the only automated check, alongside
  `CI/check-format.sh`.
- Format only what you changed:
  `clang-format -i -style=file -fallback-style=none <file>`. Do not run
  `formatcode.sh`; it has no exclusions and rewrites all of `deps/` and
  `streamelements/deps/`.

## Release channels status

|Platform 	|Environment 	|Version 	|
|---	|---	|---	    |
| Windows | `signed` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/signed/obs-streamelements.version.svg" /> |
| Windows | `qa` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/qa/obs-streamelements.version.svg" /> |
| Windows | `beta` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/beta/obs-streamelements.version.svg" /> |
| Windows | `latest` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/latest/obs-streamelements.version.svg" /> |
| Windows | `stable` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/stable/obs-streamelements.version.svg" /> |
| MacOS | `signed` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/signed/obs-streamelements.version.svg" /> |
| MacOS | `qa` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/qa/obs-streamelements.version.svg" /> |
| MacOS | `beta` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/beta/obs-streamelements.version.svg" /> |
| MacOS | `latest` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/latest/obs-streamelements.version.svg" /> |
| MacOS | `stable` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/stable/obs-streamelements.version.svg" /> |

<a href="https://cdn.streamelements.com/obs/qa/status.html" target="_blank">Extended Status Page</a>

## Deployment

1. Make sure RELEASE_NOTES.md reflects the release content of the plug-in since previous release to the public (`latest` release group).

2. Once this repository is built, run the <a href="https://github.com/StreamElements/obs-streamelements-core/actions/workflows/release.yml" target="_blank">Release Signed Binaries</a> action in github.

## Proper deployment order

| From Environment | To Environment | Predicate |
|--- |--- |---
| `signed` | `qa` | none |
| `qa` | `beta` | passes internal and closed beta testing |
| `beta` | `latest` | 2 weeks without serious issues on `beta` |
| `latest` | `stable` | 2 weeks without stability issues on `latest` |
