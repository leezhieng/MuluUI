# MuluUI

MuluUI is a cross-platform C++ GUI SDK that wraps native GUI toolkits:

| Platform | Native toolkit | Status |
|----------|----------------|--------|
| Windows  | WinUI3 (Windows App SDK) | ✅ First target (scaffolded) |
| Linux    | GTK (GNOME) / Qt-based (KDE) | ⏳ Planned |
| macOS    | SwiftUI / AppKit | ⏳ Planned |

Your application code talks only to the platform-neutral `M*` API. A small
backend layer translates that API onto each native toolkit, so a single code
base renders as a true native app everywhere.

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
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌───────────────┐        ┌───────────────┐        ┌───────────────┐
│ WinUI3 backend│        │  Linux backend│        │  macOS backend│
│ (src/platform/│        │   (planned)   │        │   (planned)   │
│   windows)    │        │               │        │               │
│ Application   │        │ GTK/GNOME     │        │ SwiftUI       │
│ Window        │        │ KDE           │        │ AppKit        │
│ MddBootstrap  │        │               │        │               │
└───────────────┘        └───────────────┘        └───────────────┘
```

### Layers

1. **Core (platform-neutral)** — `src/core/`, headers in `include/Mulu/`
   - [`MObject`](include/Mulu/MObject.h) — root of the object tree; parent/child
     ownership, object names, `findChild()` (the future home of a meta-object
     system / signals).
   - [`MApplication`](include/Mulu/MApplication.h) — owns the backend, drives the
     native event loop (`exec()` / `quit()`).
   - [`MWindow`](include/Mulu/MWindow.h) — top-level window; title, size, central
     widget, close callback, native handle.
   - [`MWidget`](include/Mulu/MWidget.h) — base for all UI elements; geometry,
     visibility, parent-chain lookup.
   - Basic controls: [`MLabel`](include/Mulu/MLabel.h), [`MButton`](include/Mulu/MButton.h),
     [`MStackLayout`](include/Mulu/MStackLayout.h).
   - [`MUiLoader`](include/Mulu/MUiLoader.h) — loads Qt Designer-style `.ui`
     files (XML) and builds the M-widget tree (the `QUiLoader`/`uic`
     equivalent for MuluUI).
   - Value types: [`MString`](include/Mulu/MString.h) (UTF-8 ↔ UTF-16),
     [`MPoint`](include/Mulu/MPoint.h), [`MSize`](include/Mulu/MSize.h),
     [`MRect`](include/Mulu/MRect.h), [`MColor`](include/Mulu/MColor.h).

2. **Platform abstraction** — [`MPlatform.h`](include/Mulu/MPlatform.h)
   - [`MPlatformBackend`](include/Mulu/MPlatform.h) — initialize, run event loop,
     create/destroy windows, app name.
   - [`MPlatformWindow`](include/Mulu/MPlatform.h) — show/hide/close, title/size,
     native handle, `syncWidgetTree()`.
   - [`createPlatformBackend()`](src/core/MPlatform.cpp) — factory selecting the
     backend for the current platform.

3. **WinUI3 backend (Windows)** — `src/platform/windows/`
   - [`MWindowsPlatform`](src/platform/windows/MWindowsPlatform.h) — bootstraps the
     Windows App Runtime (`MddBootstrapInitialize`) and starts the XAML event
     loop via `Application::Start`.
   - [`MWinUIApp`](src/platform/windows/MWinUIApp.h) — the WinUI3 `Application`
     object; `OnLaunched` notifies the platform that native windows can now be
     created.
   - [`MWindowsWindow`](src/platform/windows/MWindowsWindow.h) — wraps a
     `Microsoft.UI.Xaml.Window` and maps the `M*` widget tree onto native
     controls (`MButton → Button`, `MLabel → TextBlock`,
     `MStackLayout → StackPanel`, ...).

### Design notes

- **Deferred native window creation.** XAML windows can only be created after
  the WinUI3 event loop is running. `MWindow::show()` before `exec()` therefore
  only records intent; `MWindowsPlatform::onXamlApplicationStarted()` materializes
  and activates the windows once `Application::Start` is live.
- **Lifetime safety.** `MWindow` tears down its `MPlatformWindow` before the
  `MObject` base deletes the widget tree, so native controls (and their event
  handlers, which capture `M*` pointers) never outlive the widgets they wrap.
- **Unpackaged deployment.** The Windows backend initializes the Windows App
  Runtime with `MddBootstrapInitialize`, so no MSIX packaging is required to
  run WinUI3 apps.

---

## Project layout

```
.
├── CMakeLists.txt              # Build + install/export of MuluUI
├── CMakePresets.json           # VS2022 + vcpkg preset
├── vcpkg.json                  # Dependency manifest (Windows App SDK)
├── build_sdk.py                # Cross-platform CMake build driver
├── build_examples.py           # Build all examples (reuses build_sdk.py)
├── include/Mulu/               # Public API (platform-neutral headers)
├── src/
│   ├── core/                   # Platform-neutral implementation
│   └── platform/windows/       # WinUI3 backend
└── examples/hello/             # "Hello MuluUI" sample app
```

## Building

Prerequisites:

- **Python 3.8+** and **CMake ≥ 3.22**
- **Windows 10/11**: **Visual Studio 2022** with the *Desktop development with
  C++* workload
- **vcpkg** — install it and set the `VCPKG_ROOT` environment variable
  (required on Windows so the Windows App SDK can be provisioned):

  ```powershell
  git clone https://github.com/microsoft/vcpkg
  .\vcpkg\bootstrap-vcpkg.bat
  [Environment]::SetEnvironmentVariable("VCPKG_ROOT", "$PWD\vcpkg", "User")
  ```

### Cross-platform driver: `build_sdk.py`

The SDK ships a single Python build driver that detects the running platform
and drives CMake accordingly:

```bash
# Windows -> Visual Studio generator; Linux/macOS -> Ninja (or Unix Makefiles)
python build_sdk.py                       # Release, auto-detected generator
python build_sdk.py -c Debug -j 8         # config + parallel jobs
python build_sdk.py -g Ninja --clean      # explicit generator, clean build
python build_sdk.py --target hello        # build one target only
python build_sdk.py --preset windows      # delegate to a CMakePresets.json preset
python build_sdk.py --no-vcpkg            # build core without vcpkg deps
```

It automatically:
- selects a generator for the current platform (Visual Studio 17 2022 →
  Ninja on Windows; Ninja → Unix Makefiles on Linux/macOS),
- locates the vcpkg toolchain from `VCPKG_ROOT` (or common checkouts) and
  passes it to CMake, picking a sensible default triplet per platform
  (`x64-windows`, `x64-linux`, `arm64-osx`, ...),
- applies the correct build-type handling for single- vs. multi-config
  generators.

#### `build_sdk.py` options

| Option | Description | Default |
|--------|-------------|---------|
| `-c, --config <NAME>` | Build configuration: `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` | `Release` |
| `-g, --generator <NAME>` | CMake generator (e.g. `Ninja`, `Visual Studio 17 2022`) | auto-detected per platform |
| `-j, --jobs <N>` | Number of parallel build jobs | CMake default |
| `-b, --build-dir <PATH>` | CMake build directory | `<project>/build` |
| `--clean` | Wipe the build directory before configuring | off |
| `--target <NAME>` | Build only this CMake target (e.g. `hello`, `MuluUI`) | all targets |
| `--preset <NAME>` | Configure/build through a named `CMakePresets.json` preset | none |
| `--toolchain <PATH>` | Path to a CMake toolchain file | auto-detected vcpkg |
| `--no-vcpkg` | Do not use the vcpkg toolchain (builds the core only) | off |
| `--triplet <NAME>` | vcpkg target triplet | per-platform (`x64-windows`, `x64-linux`, `arm64-osx`, ...) |
| `-D, --define <VAR=VAL>` | Extra CMake cache variable (repeatable) | none |
| `-v, --verbose` | Verbose build output | off |
| `-h, --help` | Show the full usage/help text | — |

Run `python build_sdk.py --help` for the up-to-date list on any machine.

### Alternative: explicit CMake

```powershell
cmake --preset windows
cmake --build build --config Release
```

### Building the examples: `build_examples.py`

[`build_examples.py`](build_examples.py) discovers every example under
`examples/` (a sub-directory whose `CMakeLists.txt` declares one or more
`add_executable` targets), configures the project once, then builds each
example target — reporting a per-target summary and continuing past
individual failures:

```bash
python build_examples.py                  # build every example (Release)
python build_examples.py -c Debug -j 8    # config + parallel jobs
python build_examples.py --only hello     # build just the 'hello' example
python build_examples.py --only hello --only another   # multiple examples
python build_examples.py --no-vcpkg       # build core without vcpkg deps
```

It reuses the platform detection, generator selection, and vcpkg toolchain
logic from [`build_sdk.py`](build_sdk.py), so it accepts the same
`-c/--config`, `-g/--generator`, `-b/--build-dir`, `--clean`, `--toolchain`,
`--triplet`, `-D/--define`, and `-v/--verbose` options, plus `--only` to
select specific examples.

### Run the sample

```powershell
.\build\examples\hello\Release\hello.exe     # Windows / MSVC
./build/examples/hello/hello                 # Linux / macOS
```

> Notes:
> - building the WinUI3 backend requires the Windows App SDK, which is
>   fetched automatically by vcpkg;
> - the core library depends on **tinyxml2** (used by
>   [`MUiLoader`](include/Mulu/MUiLoader.h)); vcpkg installs it, or it can be
>   provided by the system (`libtinyxml2-dev` on Debian/Ubuntu);
> - the core library itself (`MObject`, `MWidget`, `MApplication`, value
>   types) is platform-neutral and compiles on any C++20 toolchain; on
>   non-Windows platforms the build currently produces the core with no
>   backend (the factory returns `nullptr` and `exec()` returns `1`).

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

    auto* root  = new MStackLayout();
    auto* label = new MLabel("Hello, MuluUI!");
    auto* button = new MButton("Click me");

    root->addWidget(label);
    root->addWidget(button);

    button->setOnClicked([label]() { label->setText("Button clicked!"); });

    window.setCentralWidget(root);
    window.show();

    return app.exec();
}
```

## Designing forms with `.ui` files

[`MUiLoader`](include/Mulu/MUiLoader.h) reads Qt Designer-style `.ui` XML and
builds the equivalent M-widget tree at runtime — the `QUiLoader`/`uic`
equivalent for MuluUI. Forms authored in **Qt Designer** (or any tool that
emits the Qt `.ui` format) can therefore be consumed directly.

```cpp
#include <Mulu/Mulu.h>
#include <Mulu/MUiLoader.h>

int main(int argc, char** argv)
{
    MApplication app(argc, argv);

    MUiLoader loader;
    // A QMainWindow root yields an MWindow; a QWidget root yields an MWidget.
    MObject* form = loader.load("myform.ui");
    if (!form) {
        std::cerr << "Failed to load UI: " << loader.errorString() << "\n";
        return 1;
    }

    auto* window = dynamic_cast<MWindow*>(form);
    if (!window) {                    // plain-widget root: wrap it
        window = new MWindow();
        window->setCentralWidget(static_cast<MWidget*>(form));
    }

    // Look up widgets by their objectName from the .ui file.
    if (auto* button = dynamic_cast<MButton*>(window->findChild("pushButton"))) {
        button->setOnClicked([]() { /* ... */ });
    }

    window->show();
    return app.exec();
}
```

### Supported markup

| Qt Designer element | MuluUI mapping |
|---------------------|----------------|
| `<widget class="QMainWindow">` / `QDialog` / `QWindow` | `MWindow` (form root) |
| `<widget class="QWidget">` | `MWidget` |
| `<widget class="QLabel">` | `MLabel` |
| `<widget class="QPushButton">` | `MButton` |
| `<layout class="QVBoxLayout">` | `MStackLayout` (vertical) |
| `<layout class="QHBoxLayout">` | `MStackLayout` (horizontal) |
| `objectName` attribute | `MObject::setObjectName` |
| `geometry` / `text` / `visible` properties | geometry / text / visibility on the mapped widget |
| `windowTitle` / `minimumSize` / `maximumSize` (window root) | `MWindow` title / size limits |

Unknown widget/layout classes fall back to a generic `MWidget` (or a vertical
stack) and are reported through `warnings()`. Unsupported properties
(`font`, `palette`, `styleSheet`, ...) are ignored for now. Two entry points
are available: `load()` returns the form (window or widget), and `loadInto()`
loads a form directly into an existing `MWindow` as its central widget.

See the [`uiloader`](examples/uiloader/main.cpp) example
([`form.ui`](examples/uiloader/form.ui)) for a complete, runnable sample.

## Adding a new backend

1. Implement [`MPlatformBackend`](include/Mulu/MPlatform.h) and
   [`MPlatformWindow`](include/Mulu/MPlatform.h) for the target toolkit
   (e.g. GTK3/GTK4 on Linux, SwiftUI via a bridging layer on macOS).
2. Add the sources under `src/platform/<name>/` and wire them into
   [`CMakeLists.txt`](CMakeLists.txt) behind a platform guard.
3. Register the backend in [`createPlatformBackend()`](src/core/MPlatform.cpp).
4. Map the `M*` widget classes onto native controls in the platform window's
   `syncWidgetTree()`.

## Roadmap

- [x] Public API core (`MObject`, `MApplication`, `MWindow`, `MWidget`, basic controls)
- [x] Windows / WinUI3 backend scaffold (bootstrap, event loop, widget mapping)
- [x] Qt Designer `.ui` form loader (`MUiLoader` + `uiloader` example)
- [ ] Windows polish: hit-testing, DPI, input, theme, menus, dialogs
- [ ] Meta-object system (properties + signals/slots) on `MObject`
- [ ] More widgets: text input, checkbox, list, scroll area, tabs
- [ ] Layout engine (stack, grid, dock) independent of the backend
- [ ] `.ui` property coverage: font, palette, stylesheet, alignment, tooltips
- [ ] Painting/rendering abstraction (`MPainter`)
- [ ] Linux backend (GTK for GNOME, Qt for KDE)
- [ ] macOS backend (SwiftUI/AppKit)
- [ ] Package manager integration (vcpkg port, Conan) and CI

## License

[MIT](LICENSE)
