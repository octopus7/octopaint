[KO](README_KO.md) | [JA](README_JA.md)

# OctoPaint

OctoPaint is a native Windows image editor built with C++23. Its first frontend uses WinUI 3, but the editor core, application API, rendering contracts, and file-format boundaries are designed to remain independent of any UI framework.

The product name, executable name, and window title are all `OctoPaint`.

## Planned capabilities

- Multi-document editing
- Layers, groups, blend modes, raster masks, and clipping relationships
- Composite, RGB, alpha, and additional named channels
- Selection operations and saved selections
- Brightness/contrast, hue/saturation, curves, and desaturation adjustments
- Image filters, including Gaussian blur
- Cropping, nine-anchor canvas resizing, and image resampling by percentage or pixel dimensions
- Native layered `.ocp` documents
- PNG and JPEG import/export
- Layer-aware PSD import/export with explicit compatibility reporting

## Architecture

```text
OctoPaint.WinUI (replaceable WinUI 3 frontend)
        |
        v
OctoPaint.Application (UI-neutral commands and snapshots)
        |
        v
OctoPaint.Core (platform-independent document domain)
```

`OctoPaint.Core` uses only the C++23 standard library. WinUI, WinRT, Win32, Direct3D, and other frontend-specific types stay outside its public interface. A future frontend can consume `OctoPaint.Application` without rewriting document or editor behavior.

## Repository structure

- `src/OctoPaint.Core`: platform-independent document model
- `src/OctoPaint.Application`: UI-neutral commands and immutable snapshots
- `src/OctoPaint.WinUI`: replaceable WinUI 3 adapter and `OctoPaint` executable
- `tests/OctoPaint.Core.Tests`: headless application and core verification
- `docs`: architecture, product, editor, and file-format design documents

## Design documents

- [Architecture and frontend replacement rules](docs/ARCHITECTURE.md)
- [Product requirements](docs/PRODUCT_REQUIREMENTS.md)
- [Editor architecture](docs/EDITOR_ARCHITECTURE.md)
- [File formats and interoperability](docs/FILE_FORMATS.md)

## Build

Open `OctoPaint.sln` in Visual Studio 2022 and select the `x64` platform, or run:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" .\OctoPaint.sln /restore /m /p:Configuration=Debug /p:Platform=x64
```

Run the headless checks with:

```powershell
.\out\bin\x64\Debug\OctoPaint.Core.Tests\OctoPaint.Core.Tests.exe
```

## Project status

The repository currently contains the initial C++23 solution scaffold and the first frontend-independent workspace flow. The broader editing and interoperability capabilities above are design targets and will be implemented incrementally.
