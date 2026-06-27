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
