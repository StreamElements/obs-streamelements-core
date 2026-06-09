# Linux Build & Runtime Notes (`obs-streamelements-core`)

This document is bilingual:
1. English
2. Español

---

## English

### 1) Current scope

Target of this branch:
- Linux with native Wayland and X11 behavior.
- The custom widget bridge selects the platform-specific path at runtime.

Validated environment:
- Fedora 43
- OBS 32.1.x
- Qt 6

### 2) Required `obs-browser`

This branch depends on custom `obs-browser` changes.

Use:
- `clairerb6/obs-browser`

Do not assume upstream `obs-browser` has the same behavior required here.

`build-linux.sh` supports:
- auto-detection from sibling folder: `../obs-browser`
- explicit path: `--obs-browser-dir <path>`

### 3) Build dependencies (Fedora)

Recommended minimum:

```bash
sudo dnf install \
  git cmake ninja-build gcc gcc-c++ make pkgconf-pkg-config \
  obs-studio-devel \
  qt6-qtbase-devel qt6-qttools-devel \
  curl-devel swig vlc-devel \
  libavutil-free-devel libswresample-free-devel libavcodec-free-devel \
  libavformat-free-devel libswscale-free-devel libpostproc-free-devel \
  libavfilter-free-devel
```

Note:
- Keep FFmpeg packages consistent (avoid mixing Fedora `*-free` and RPMFusion stacks blindly).

### 4) Build script

Script:
- `_scripts/build-linux.sh`

Help:

```bash
./_scripts/build-linux.sh --help
```

Common commands:

```bash
# configure + build
./_scripts/build-linux.sh

# clean build
./_scripts/build-linux.sh --clean

# clean build + install into user OBS plugins dir
./_scripts/build-linux.sh --clean --install-user

# clean build + install both obs-browser and obs-streamelements-core
./_scripts/build-linux.sh --clean --install-user \
  --obs-browser-dir ~/Projects/Others/obs-browser

# clean build with a specific Qt major version
./_scripts/build-linux.sh --qt-version 5 --clean
QT_VERSION=6 ./_scripts/build-linux.sh --clean

# explicit obs-browser checkout
./_scripts/build-linux.sh --clean --install-user \
  --obs-browser-dir ~/Projects/Others/obs-browser
```

Main options:
- `--build-dir <path>`
- `--build-type <type>`
- `--qt-version <5|6>` or `QT_VERSION=<5|6>` environment variable
- `--generator <name>`
- `--target <name>`
- `--jobs <n>`
- `--clean`
- `--no-configure`
- `--configure-only`
- `--cmake-arg <arg>` (repeatable)
- `--install-user`
- `--user-plugin-dir <path>`
- `--obs-browser-dir <path>`
- `--obs-browser-build-dir <path>`
- `--obs-browser-cmake-arg <arg>` (repeatable)

### 5) Install paths (`--install-user`)

With user install enabled, files are copied to:
- `~/.config/obs-studio/plugins/obs-streamelements-core/bin/64bit/`
- `~/.config/obs-studio/plugins/obs-streamelements-core/data/obs-plugins/obs-streamelements-core/`

When `obs-browser` is available through `--obs-browser-dir` or the sibling
`../obs-browser` checkout, the script also builds and installs:
- `~/.config/obs-studio/plugins/obs-browser/bin/64bit/`
- `~/.config/obs-studio/plugins/obs-browser/data/obs-plugins/obs-browser/`

### 6) Runtime policy for this branch

- Validate in native Wayland and native X11 sessions.
- Let OBS/Qt choose the platform normally; avoid forcing `QT_QPA_PLATFORM`
  unless testing a specific platform path.
- Do not load two browser plugins at once.

Important:
- Keep only one active `obs-browser` variant in OBS (system OR custom), not both.

### 7) Status snapshot (2026-04-27)

What improved:
- Significant stability progress in panel rendering and interaction.
- Startup/interaction behavior is much better than initial migration state.

Known issues still open:
- Some docked web panels may still need undock/redock to render correctly in specific layouts.
- Exit path is not fully stable yet:
  - Closing OBS from window button can be clean.
  - `File -> Exit` can still crash in some runs.

### 8) Suggested quick validation loop

After each local build/install:
1. Open OBS.
2. Verify docked panel rendering.
3. Verify typing/input inside web panels.
4. Exit using window close button.
5. Exit using `File -> Exit`.
6. If crash occurs, save OBS log and coredump id for analysis.

---

## Español

### 1) Alcance actual

Objetivo de esta rama:
- Linux con comportamiento nativo para Wayland y X11.
- El bridge de widgets personalizado selecciona la ruta especifica de
  plataforma en tiempo de ejecucion.

Entorno validado:
- Fedora 43
- OBS 32.1.x
- Qt 6

### 2) `obs-browser` requerido

Esta rama depende de cambios personalizados en `obs-browser`.

Usar:
- `clairerb6/obs-browser`

No asumir que el `obs-browser` oficial/upstream tenga el mismo comportamiento requerido aquí.

`build-linux.sh` soporta:
- autodetección desde carpeta hermana: `../obs-browser`
- ruta explícita: `--obs-browser-dir <path>`

### 3) Dependencias de build (Fedora)

Mínimo recomendado:

```bash
sudo dnf install \
  git cmake ninja-build gcc gcc-c++ make pkgconf-pkg-config \
  obs-studio-devel \
  qt6-qtbase-devel qt6-qttools-devel \
  curl-devel swig vlc-devel \
  libavutil-free-devel libswresample-free-devel libavcodec-free-devel \
  libavformat-free-devel libswscale-free-devel libpostproc-free-devel \
  libavfilter-free-devel
```

Nota:
- Mantener consistente la familia FFmpeg (evitar mezclar sin control `*-free` de Fedora con paquetes de RPMFusion).

### 4) Script de build

Script:
- `_scripts/build-linux.sh`

Ayuda:

```bash
./_scripts/build-linux.sh --help
```

Comandos comunes:

```bash
# configurar + compilar
./_scripts/build-linux.sh

# build limpia
./_scripts/build-linux.sh --clean

# build limpia + instalación en plugins de usuario OBS
./_scripts/build-linux.sh --clean --install-user

# build limpia + instalacion de obs-browser y obs-streamelements-core
./_scripts/build-linux.sh --clean --install-user \
  --obs-browser-dir ~/Projects/Others/obs-browser

# build limpia con una version mayor especifica de Qt
./_scripts/build-linux.sh --qt-version 5 --clean
QT_VERSION=6 ./_scripts/build-linux.sh --clean

# checkout de obs-browser explícito
./_scripts/build-linux.sh --clean --install-user \
  --obs-browser-dir ~/Projects/Others/obs-browser
```

Opciones principales:
- `--build-dir <path>`
- `--build-type <type>`
- `--qt-version <5|6>` o variable de entorno `QT_VERSION=<5|6>`
- `--generator <name>`
- `--target <name>`
- `--jobs <n>`
- `--clean`
- `--no-configure`
- `--configure-only`
- `--cmake-arg <arg>` (repetible)
- `--install-user`
- `--user-plugin-dir <path>`
- `--obs-browser-dir <path>`
- `--obs-browser-build-dir <path>`
- `--obs-browser-cmake-arg <arg>` (repetible)

### 5) Rutas de instalación (`--install-user`)

Con instalación de usuario activa, se copia a:
- `~/.config/obs-studio/plugins/obs-streamelements-core/bin/64bit/`
- `~/.config/obs-studio/plugins/obs-streamelements-core/data/obs-plugins/obs-streamelements-core/`

Cuando `obs-browser` esta disponible por `--obs-browser-dir` o por el checkout
hermano `../obs-browser`, el script tambien compila e instala:
- `~/.config/obs-studio/plugins/obs-browser/bin/64bit/`
- `~/.config/obs-studio/plugins/obs-browser/data/obs-plugins/obs-browser/`

### 6) Política runtime de esta rama

- Validar en sesiones nativas Wayland y X11.
- Dejar que OBS/Qt elija la plataforma normalmente; evitar forzar
  `QT_QPA_PLATFORM` salvo para probar una ruta especifica.
- No cargar dos variantes de plugin browser al mismo tiempo.

Importante:
- Mantener solo una variante activa de `obs-browser` en OBS (sistema O personalizada), no ambas.

### 7) Estado actual (2026-04-27)

Mejoras:
- Avance fuerte en estabilidad de render e interacción de paneles.
- El comportamiento de inicio/interacción es mucho mejor que en el estado inicial de la migración.

Problemas conocidos pendientes:
- En ciertos layouts, algunos paneles web acoplados todavía pueden requerir desacoplar/acoplar para renderizar correctamente.
- La salida aún no está completamente estable:
  - Cerrar OBS desde el botón de la ventana puede salir limpio.
  - `Archivo -> Salir` todavía puede crashear en algunas ejecuciones.

### 8) Ciclo rápido de validación recomendado

Después de cada build/install local:
1. Abrir OBS.
2. Verificar render de paneles acoplados.
3. Verificar input de texto dentro de paneles web.
4. Cerrar desde botón de ventana.
5. Cerrar desde `Archivo -> Salir`.
6. Si hay crash, guardar log de OBS y el id de coredump para análisis.
