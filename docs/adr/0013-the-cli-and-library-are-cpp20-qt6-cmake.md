# 0013 — The CLI and library are C++20, Qt 6 Core and CMake

## Status

Accepted.

## Context

The CLI must be callable from the QML widget. The library must be usable by
C++/Qt apps such as JS8, which already builds with qmake and links Qt 6 Core.
The candidates were C++20 + Qt 6 Core, dependency-light C++ without Qt, and a
non-C++ CLI with a C++ library.

Qt Core supplies `QJsonDocument`, `QFileSystemWatcher`, `QProcess`,
`QStandardPaths`, `QSaveFile`-style file handling and the event loop that the
watcher needs. JS8 and the widget already depend on Qt 6, so a Qt Core dependency
adds nothing to either consumer. A non-C++ CLI would split the writer and the
library into two implementations; a dependency-light C++ library would mean
owning JSON, watch and process plumbing that Qt already provides.

CMake is the build system JS8 does not use today (it uses qmake), but a library
can be consumed from qmake through `pkg-config` or the installed CMake package.
CMake is also the practical way to publish both the library and a package
config.

## Decision

- C++20.
- Qt 6 Core only; no Qt GUI, Widgets, Multimedia or DBus.
- CMake for the library, CLI and tests.
- A static library target `omarchy_ham`; the CLI target is `omarchy-ham`.
- Public headers under `include/omarchy_ham/`.
- Install a CMake package (`find_package(omarchy_ham CONFIG)`) and a
  `pkg-config` file (`omarchy-ham`), so both CMake and qmake consumers can link.
- CTest for the unit tests; a Python integration test drives the CLI with a
  temporary `XDG_CONFIG_HOME`.

## Consequences

- JS8 can link `omarchy_ham::omarchy_ham` or use `pkg-config omarchy-ham`.
- The QML widget uses the CLI, not the library, avoiding a QML-to-C++ bridge.
- The library has no daemon and no GUI dependency.
- A static library avoids a C++ ABI commitment for now; a shared library can be
  added later with a SONAME once the API stabilises.
- CMake becomes a build dependency in addition to Qt 6 Core.

## Rejected

- **A Rust or Go CLI plus a C++ library.** Two implementations of the writer,
  watcher and validation, and two release paths.
- **Dependency-light C++ without Qt.** More code for JSON, watching and process
  handling, and a second event-loop style next to JS8's Qt code.
- **Qt GUI or Widgets.** The CLI has no UI, and the widget already has QML.
- **Meson or qmake.** CMake gives the package config and test integration with
  the least extra work.
