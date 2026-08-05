# OctoPaint architecture

OctoPaint uses a ports-and-adapters structure so that the desktop UI can be replaced without rewriting the editor.

```text
OctoPaint (replaceable WinUI 3 adapter executable)
            |
            v
OctoPaint.Application (frontend-facing API)
            |
            v
OctoPaint.Core (platform-independent domain)
```

## Dependency rules

- `OctoPaint.Core` uses only the C++23 standard library. It must not include WinUI, WinRT, Win32, Direct3D, or frontend headers.
- `OctoPaint.Application` exposes commands and immutable snapshots using standard C++ value types. It owns orchestration but no widgets or windows.
- `OctoPaint.WinUI` is an adapter. XAML, `winrt::*`, HWND values, and Windows App SDK types stay inside this project.
- A future frontend links `OctoPaint.Application` and translates its snapshots into its own controls.
- Rendering will be introduced behind a separate renderer boundary. A frontend may host a native surface, but document and tool behavior must not depend on that surface type.
- Calls across boundaries should describe user intent, such as `NewDocument`, rather than control events, such as `NewButtonClicked`.

## Replacement procedure

To add another frontend, create a new executable project, reference `OctoPaint.Application`, and implement presentation code against `Workspace` commands and `WorkspaceSnapshot`. No changes to `OctoPaint.Core` are required.
