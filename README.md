# MuluUI

MuluUI is a cross-platform C++ GUI SDK that renders entirely on the GPU using
custom shaders — **no native toolkit dependencies** beyond a windowing library
and an OpenGL driver.

| Platform | Backend | Approach |
|----------|---------|----------|
| Windows, Linux, macOS | SDL3 + OpenGL 3.3 | Custom GLSL shaders (Fluent Design style) |
| Future | Vulkan / DirectX 12 / OpenGL ES / MoltenVK | Multi-backend GPU abstraction |

Your application code talks only to the platform-neutral `M*` API. The rendering
is done by a retained-mode draw list converted from the logical widget tree,
executed each frame via batch-based GPU draw calls — pixel-perfect and
identical across every platform.

> **Current backend:** SDL3 for windowing + input, OpenGL 3.3 Core for
> rendering. Text is rendered via `stb_truetype` SDF glyph atlases. Rounded
> rectangles use signed-distance-field (SDF) fragment shaders for anti-aliased
> corners at any scale.

---

## Architecture

```
┌───────────────────────────────────────────────────────────────┐
│                        Application code                      │
│            (uses only the public MuluUI headers)             │
└───────────────────────────────┬───────────────────────────────┘
                                │
┌───────────────────────────────▼───────────────────────────────┐
│  Public API (include/Mulu)                                    │
│   MApplication  MWindow  MWidget  MButton  MLabel             │
│   MStackLayout  MObject  MString  MPoint/MSize/MRect/MColor   │
└───────────────────────────────┬───────────────────────────────┘
                                │
┌───────────────────────────────▼───────────────────────────────┐
│  Platform abstraction (include/Mulu/MPlatform.h)              │
│   MPlatformBackend   ·  MPlatformWindow   ·  factory          │
└───────────────────────────────┬───────────────────────────────┘
                                │
                                ▼
┌───────────────────────────────────────────────────────────────┐
│  SDL3 + OpenGL 3.3 backend (src/platform/sdl)                 │
│                                                               │
│   MOpenGLPlatform    Event loop (SDL_PollEvent)               │
│   MOpenGLWindow      SDL_Window + GL context + draw list      │
│   GLRenderer         Batch VAO/VBO/IBO + embedded GLSL        │
│   TextRenderer       stb_truetype SDF glyph atlas             │
│   shaders/           rect.frag (SDF corners), text.frag (SDF) │
└───────────────────────────────────────────────────────────────┘
```

### Layers

1. **Core (platform-neutral)** — `src/core/`, headers in `include/Mulu/`
   - [`MObject`](include/Mulu/MObject.h) — root of the object tree; parent/child
     ownership, object names, `findChild()` (the future home of a meta-object
     system / signals).
   - [`MApplication`](include/Mulu/MApplication.h) — owns the backend, drives the
     event loop (`exec()` / `quit()`).
   - [`MWindow`](include/Mulu/MWindow.h) — top-level window; title, size, central
     widget, close callback, native handle.
   - [`MWidget`](include/Mulu/MWidget.h) — base for all UI elements; geometry,
     visibility, parent-chain lookup.
   - Basic controls: [`MLabel`](include/Mulu/MLabel.h), [`MButton`](include/Mulu/MButton.h),
     [`MStackLayout`](include/Mulu/MStackLayout.h).
   - [`MUiLoader`](include/Mulu/MUiLoader.h) — loads Qt Designer-style `.ui`
     files (XML) and builds the M-widget tree.
   - Value types: [`MString`](include/Mulu/MString.h) (UTF-8 ↔ UTF-16),
     [`MPoint`](include/Mulu/MPoint.h), [`MSize`](include/Mulu/MSize.h),
     [`MRect`](include/Mulu/MRect.h), [`MColor`](include/Mulu/MColor.h).

2. **Platform abstraction** — [`MPlatform.h`](include/Mulu/MPlatform.h)
   - [`MPlatformBackend`](include/Mulu/MPlatform.h:45) — initialize, run event loop,
     create/destroy windows.
   - [`MPlatformWindow`](include/Mulu/MPlatform.h:17) — show/hide/close, title/size,
     native handle, `syncWidgetTree()`.

3. **SDL3 + OpenGL 3.3 backend** — `src/platform/sdl/`
   - [`MOpenGLPlatform`](src/platform/sdl/MOpenGLPlatform.h) — SDL3 video init,
     event loop (`SDL_PollEvent`), per-frame rendering dispatch.
   - [`MOpenGLWindow`](src/platform/sdl/MOpenGLWindow.h) — `SDL_Window` with
     OpenGL 3.3 Core context. Converts the `MWidget` tree into a flat
     `DrawNode` list. Hit-tests mouse events and dispatches to `MButton::click()`.
   - [`GLRenderer`](src/platform/sdl/GLRenderer.h) — batch-based 2D GPU renderer.
     Dynamic VBO/IBO/VAO. Two shader programs: rounded-rect (SDF anti-aliased
     corners) and textured-quad (SDF glyph rendering). Shader sources embedded
     as string literals.
   - [`TextRenderer`](src/platform/sdl/TextRenderer.h) — `stb_truetype.h` SDF
     glyph atlas. UTF-8 decoding. On-demand glyph rasterisation with row-packing.
   - [`GLFuncs`](src/platform/sdl/GLFuncs.h) — minimal OpenGL 3.3 Core function
     loader via `SDL_GL_GetProcAddress`. No external GL loader needed.
   - [`shaders/`](src/platform/sdl/shaders/) — canonical GLSL reference files.

### Design notes

- **Eager window creation.** SDL windows and GL contexts are created immediately
  on first `show()`. No deferred activation — the GL context is ready the moment
  the window appears.

- **Retained draw list.** `syncWidgetTree()` walks the logical widget tree and
  produces a flat `DrawNode` list. This list is re-rendered each frame via the
  batch renderer. Widget state changes trigger a re-sync.

- **Lifetime safety.** `MWindow` tears down its `MPlatformWindow` before the
  `MObject` base deletes the widget tree, so GPU resources and event handlers
  never outlive the widgets they reference.

- **Zero-dependency GL loading.** All 44 OpenGL 3.3 Core entry points are loaded
  through `SDL_GL_GetProcAddress` — no GLEW, GLAD, or other loader.

- **Conditional text rendering.** If `stb_truetype.h` is not present in
  `dependencies/install/include/`, CMake defines `MULU_NO_TEXT_RENDERER` and
  the backend renders controls without text labels. Run `download_dep.py` to
  fetch it, then rebuild to enable full text support.

---

## Project layout

```
.
├── CMakeLists.txt              # Build + install/export of MuluUI
├── CMakePresets.json           # CMake preset (MSVC + vcpkg)
├── vcpkg.json                  # Dependency manifest (sdl3, tinyxml2)
├── download_dep.py             # Dependency downloader & builder
├── build_sdk.py                # Cross-platform CMake build driver
├── build_examples.py           # Build all examples
├── dependencies/               # (generated by download_dep.py, gitignored)
│   ├── install/                #   Built libraries + headers
│   └── .mulu_dep_config.json   #   Saved linkage choice
├── include/Mulu/               # Public API (platform-neutral headers)
├── src/
│   ├── core/                   # Platform-neutral implementation
│   └── platform/sdl/           # SDL3 + OpenGL 3.3 backend
│       └── shaders/            # Canonical GLSL shader sources
└── examples/
    ├── hello/                  # Minimal "Hello MuluUI" sample
    ├── sdlhello/               # SDL3+OpenGL demo
    └── uiloader/               # .ui form loader sample
```

## Building

Prerequisites:

- **Python 3.8+** and **CMake ≥ 3.22**
- **A C++20 compiler** (MSVC 2022+, GCC 12+, Clang 16+)
- **Git** (for downloading dependency sources)

No system package manager is required — all dependencies are downloaded and
built locally by [`download_dep.py`](download_dep.py).

### Step 1: Download & build dependencies

```bash
python download_dep.py              # interactive: asks static vs shared
python download_dep.py --static     # build static libraries (.lib / .a)
python download_dep.py --shared     # build shared libraries (.dll / .so)
python download_dep.py --clean      # wipe dependencies/ and rebuild fresh
```

This downloads SDL3, tinyxml2, and stb_truetype.h, then builds them into
`dependencies/install/`. The linkage choice is saved to
`dependencies/.mulu_dep_config.json` so later build steps pick it up
automatically.

| Dependency | Version | Source |
|---|---|---|
| SDL3 | 3.2.10 | GitHub release (source build via CMake) |
| tinyxml2 | 10.1.0 | GitHub release (source build via CMake) |
| stb_truetype.h | latest | Single header from nothings/stb (download only) |

> **Note:** `stb_truetype.h` is downloaded to `dependencies/install/include/`
> by [`download_dep.py`](download_dep.py). If the file is missing, CMake
> disables text rendering with a warning — controls still render (buttons,
> layouts) but without text labels.

### Step 2: Build MuluUI

```bash
python build_sdk.py                 # reads linkage config automatically
python build_sdk.py --shared        # override to shared linkage
python build_sdk.py --static        # override to static linkage
python build_sdk.py -c Debug -j 8   # debug build, 8 parallel jobs
python build_sdk.py --target hello  # build only the hello example
python build_sdk.py -g Ninja        # explicit CMake generator
python build_sdk.py --clean         # wipe build/ before configuring
```

[`build_sdk.py`](build_sdk.py) automatically:
- Detects the best CMake generator for your platform (Visual Studio on Windows,
  Ninja on Linux/macOS)
- Finds dependencies in `dependencies/install/` (built by `download_dep.py`)
- Reads the saved linkage config and passes matching `BUILD_SHARED_LIBS` to CMake
- Falls back to vcpkg if no local dependencies are found
- Falls back to system packages (apt/brew) if neither is available

### Step 3: Run

```powershell
.\build\examples\hello\Release\hello.exe         # Windows
./build/examples/hello/hello                     # Linux / macOS
```

### Building all examples

```bash
python build_examples.py                         # build every example
python build_examples.py --only sdlhello         # build just the SDL example
python build_examples.py --only hello --only uiloader
python build_examples.py -c Debug -j 8 --shared
```

[`build_examples.py`](build_examples.py) discovers every example under
`examples/`, configures the project once, then builds each target, reporting
a per-target summary. It accepts the same `--shared`/`--static`, `--clean`,
`-c`/`-g`/`-j` flags as `build_sdk.py`.

### Option reference

#### `download_dep.py`

| Flag | Description |
|---|---|
| `--shared` | Build shared libraries (.dll / .so) |
| `--static` | Build static libraries (.lib / .a) |
| `--skip-build` | Download sources only; do not compile |
| `--clean` | Remove `dependencies/` and start fresh |

#### `build_sdk.py` / `build_examples.py`

| Flag | Description |
|---|---|
| `--shared` | Force shared linkage (overrides saved config) |
| `--static` | Force static linkage (overrides saved config) |
| `-c, --config <NAME>` | Build configuration: `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `-g, --generator <NAME>` | CMake generator (auto-detected by default) |
| `-j, --jobs <N>` | Parallel build jobs |
| `-b, --build-dir <PATH>` | CMake build directory (default: `build/`) |
| `--clean` | Wipe build directory before configuring |
| `--target <NAME>` | Build only this CMake target |
| `--toolchain <PATH>` | Explicit CMake toolchain file (overrides auto-detection) |
| `--no-deps` | Skip dependency detection; build core library only |
| `--triplet <NAME>` | vcpkg target triplet (only used with vcpkg fallback) |
| `-D, --define <VAR=VAL>` | Extra CMake cache variable (repeatable) |
| `-v, --verbose` | Verbose build output |

---

## Using MuluUI

```cpp
#include <Mulu/Mulu.h>

using namespace mulu;

int main(int argc, char** argv)
{
    MApplication app(argc, argv);

    MWindow window;
    window.setTitle("Hello MuluUI");
    window.setSize(MSize(640, 480));

    auto* root   = new MStackLayout();
    auto* label  = new MLabel("Hello, MuluUI!");
    auto* button = new MButton("Click me");

    root->addWidget(label);
    root->addWidget(button);

    button->setOnClicked([label]() { label->setText("Button clicked!"); });

    window.setCentralWidget(root);
    window.show();

    return app.exec();
}
```

---

## Designing forms with `.ui` files

[`MUiLoader`](include/Mulu/MUiLoader.h) reads Qt Designer-style `.ui` XML and
builds the equivalent M-widget tree at runtime:

```cpp
#include <Mulu/Mulu.h>
#include <Mulu/MUiLoader.h>

int main(int argc, char** argv)
{
    MApplication app(argc, argv);

    MUiLoader loader;
    MObject* form = loader.load("myform.ui");
    if (!form) {
        std::cerr << "Failed to load UI: " << loader.errorString() << "\n";
        return 1;
    }

    auto* window = dynamic_cast<MWindow*>(form);
    if (!window) {
        window = new MWindow();
        window->setCentralWidget(static_cast<MWidget*>(form));
    }

    if (auto* btn = dynamic_cast<MButton*>(window->findChild("pushButton"))) {
        btn->setOnClicked([]() { /* ... */ });
    }

    window->show();
    return app.exec();
}
```

### Supported markup

| Qt Designer element | MuluUI mapping |
|---------------------|----------------|
| `<widget class="QMainWindow">` / `QDialog` / `QWindow` | `MWindow` |
| `<widget class="QWidget">` | `MWidget` |
| `<widget class="QLabel">` | `MLabel` |
| `<widget class="QPushButton">` | `MButton` |
| `<layout class="QVBoxLayout">` | `MStackLayout` (vertical) |
| `<layout class="QHBoxLayout">` | `MStackLayout` (horizontal) |
| `objectName` attribute | `MObject::setObjectName` |
| `geometry` / `text` / `visible` | geometry / text / visibility |

---

## Roadmap

- [x] Public API core (`MObject`, `MApplication`, `MWindow`, `MWidget`, basic controls)
- [x] SDL3 + OpenGL 3.3 backend (batch GPU renderer, SDF rects, SDF text)
- [x] MUi `.ui` form loader (`MUiLoader` + `uiloader` example)
- [ ] Fluent Design shader polish: Acrylic/Mica blur, Reveal highlight, elevation shadows
- [ ] Text rendering polish: proper SDF generation, kerning, CJK shaping (HarfBuzz)
- [ ] Layout engine (auto-size/position widgets based on stack/grid constraints)
- [ ] Meta-object system (properties + signals/slots) on `MObject`
- [ ] More widgets: text input, checkbox, list, scroll area, tabs
- [ ] `.ui` property coverage: font, palette, stylesheet, alignment, tooltips
- [ ] Vulkan backend (via SDL3 GPU API or direct Vulkan)
- [ ] DirectX 12 backend (Windows)
- [ ] OpenGL ES 3.0 backend (mobile / embedded)
- [ ] Metal backend via MoltenVK (macOS / iOS)
- [ ] Package manager integration (vcpkg port, Conan) and CI

## License

[MIT](LICENSE)
